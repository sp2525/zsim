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

PTW::PTW(MemObject* _parentMem, PTWCache* _ptwCache, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : parentMem(_parentMem), ptwCache(_ptwCache), realMemAccess(_realMemAccess), accLat(_accLat), invLat(_invLat), name(_name) {
        futex_init(&ptwLock);
    }

PTW::PTW(PTWCache* _ptwCache, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : ptwCache(_ptwCache), realMemAccess(_realMemAccess), accLat(_accLat), invLat(_invLat), name(_name) {
        futex_init(&ptwLock);
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

    ptwStat->append(&profREQ);
    ptwStat->append(&profINVPTE);
    ptwStat->append(&profINVALL);
    ptwStat->append(&profMemAccess);
    ptwStat->append(&profMemAccessLat);
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
//    else {
//    }
/*
    int32_t wayIdx = ptwCache->lookup(req.pageAddr, &req, updateReplacement);
    respCycle += accLat;

    if (wayIdx == -1) {
        profLKUPMiss.inc();
        if (parent != nullptr) {
          uint32_t nextLevelLat = parent->access(req) - respCycle;
          profLKUPNextLevelLat.inc(nextLevelLat);
          respCycle += nextLevelLat;
        }
        Address victimPageAddr;
        wayIdx = array->insert(req.pageAddr, &req, &victimPageAddr);
        //trace(TLB, "[%s] Evicting 0x%lx", name.c_str(), victimPageAddr);
    }
    else {
        profLKUPHit.inc();
    }
*/
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
