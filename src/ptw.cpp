/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ptw.h"
#include "hash.h"

#include "event_recorder.h"
#include "timing_event.h"
#include "zsim.h"

#define BASE_ADDR ((uint64_t)0xAAAA) << 32
#define WALK_LAT  1
#define L2_LAT  7

PTW::PTW(MemObject* _parentMem, PTWCache* _ptwCache, uint32_t _pageSize, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : parentMem(_parentMem), ptwCache(_ptwCache), pageSize(_pageSize), realMemAccess(_realMemAccess), accLat(_accLat), invLat(_invLat), name(_name) {
      init();
    }

PTW::PTW(PTWCache* _ptwCache, uint32_t _pageSize, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : ptwCache(_ptwCache), pageSize(_pageSize), realMemAccess(_realMemAccess), accLat(_accLat), invLat(_invLat), name(_name) {
      init();
    }

void PTW::init(void) {
        futex_init(&ptwLock);
        BaseAddr = BASE_ADDR;
        transLevel = (pageSize == 4096) ? 4 :
                     (pageSize == 2097152) ? 3 : 0;
        assert(transLevel >= 3);
        if (realMemAccess) {
          hf = new SHA1HashFamily(1);
        }
}

const char* PTW::getName() {
    return name.c_str();
}

void PTW::setParentMem(MemObject* _parentMem){
    parentMem = _parentMem;
}

void PTW::initStats(AggregateStat* parentStat) {
    AggregateStat* ptwStat = new AggregateStat();
    ptwStat->init(name.c_str(), "ptw stats");
    initPTWStats(ptwStat);
    parentStat->append(ptwStat);
}

void PTW::initPTWStats(AggregateStat* ptwStat) {
    profREQ.init("REQ", "Requests");
    profINVPTE.init("INVPTE", "Invalidate pte");
    profINVALL.init("INVALL", "Invalidate all");
    profMemAccess.init("MemAccess", "Memory Access");
    profMemAccessLat.init("MemAccessLat", "Memory access latency");
    profWalkLat.init("WalkLat", "PTW latency including memory access");

    ptwStat->append(&profREQ);
    ptwStat->append(&profINVPTE);
    ptwStat->append(&profINVALL);
    ptwStat->append(&profMemAccess);
    ptwStat->append(&profMemAccessLat);
    ptwStat->append(&profWalkLat);
    //ptwCache->initStats(ptwStat);
}

uint64_t PTW::access(const TransReq& req) {
    uint64_t respCycle = req.cycle;
    bool updateReplacement = true;


    lock();
    profREQ.inc();

    if (!realMemAccess) {
        uint32_t memAccess = 3;
        assert(accLat > 10);
        uint32_t memAccessLat = accLat - 10;
        profMemAccess.inc(memAccess);
        profMemAccessLat.inc(memAccessLat);
        respCycle += accLat;
    }
    else {
      //info("ptw %s access Addr = %lx", name.c_str(), req.pageAddr);
      MESIState dummyState = MESIState::I;
      MemReq memReq = {0x0, GETS, 0, &dummyState, respCycle, &ptwLock, dummyState, req.srcId, MemReq::PTWFETCH};
      uint64_t tableAddr = BaseAddr;

      for(uint32_t l = 1; l <= transLevel; l++) {
          uint32_t idx = (req.pageAddr >> (9 * (transLevel - l))) & 0x1ff;
          uint64_t pteAddr = tableAddr + (idx * 8);
          memReq.lineAddr = procMask | (pteAddr >> lineBits);
          memReq.cycle = respCycle;
          uint64_t memAccessLat = parentMem->access(memReq) - respCycle;
          //info("level %d mem access addr: 0x%lx, memAccessLat = %d, idx = 0x%x, pteAddr = 0x%lx, pageAddr = 0x%lx", l, memReq.lineAddr, memAccessLat, idx, pteAddr, req.pageAddr);

          //If you want to use timing models with a weave phase(DDR mem, Timing cache), change code for handling events
          //if(zinfo->eventRecorders[req.srcId])
          //  zinfo->eventRecorders[req.srcId]->popRecord();

          // PTW can read unified cache (L2)
          // parentMem of PTW is L1D in current configuration because of race condition.
          // if L1D hit, let's assume it's L2 hit and use L2_LAT
          // if L1D miss, use the latency
          if (memAccessLat == 0)
            memAccessLat = L2_LAT;
          profMemAccess.inc();
          profMemAccessLat.inc(memAccessLat);
          profWalkLat.inc(memAccessLat + WALK_LAT);
          respCycle += memAccessLat;
          respCycle += WALK_LAT;
          if (l != transLevel)
            tableAddr = hf->hash(0, pteAddr);
      }
      
    }
    unlock();

    assert_msg(respCycle >= req.cycle, "[%s] resp < req? 0x%lx, respCycle %ld reqCycle %ld",
            name.c_str(), req.pageAddr, respCycle, req.cycle);
    return respCycle;
}

uint64_t PTW::invalidate(const TransInvReq& req) {
    uint64_t respCycle = req.cycle;
    respCycle += invLat;

    lock();
    if (req.type == INVPTE) {
      profINVPTE.inc();
      //ptwCache->invalidPTE(req.pageAddr);
    }
    else if(req.type == INVALL) {
      profINVALL.inc();
      //ptwCache->invalidAll();
    } 
    unlock();

    return respCycle;
}
