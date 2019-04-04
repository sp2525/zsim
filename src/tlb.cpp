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

#include "tlb.h"
#include "hash.h"

#include "event_recorder.h"
#include "timing_event.h"
#include "zsim.h"

TLB::TLB(uint32_t _numEntries, TransObject* _parent, TLBArray* _array, TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : parent(_parent), array(_array), rp(_rp), numEntries(_numEntries), accLat(_accLat), invLat(_invLat), name(_name) {
        futex_init(&tlbLock);
    }

TLB::TLB(uint32_t _numEntries, TLBArray* _array, TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : array(_array), rp(_rp), numEntries(_numEntries), accLat(_accLat), invLat(_invLat), name(_name) {
        futex_init(&tlbLock);
    }

const char* TLB::getName() {
    return name.c_str();
}

void TLB::setParent(TransObject* _parent) {
    parent = _parent;
}

void TLB::initStats(AggregateStat* parentStat) {
    AggregateStat* tlbStat = new AggregateStat();
    tlbStat->init(name.c_str(), "tlb stats");
    initTLBStats(tlbStat);
    profLKUPHit.init("hLKUP", "LKUP hits");
    profLKUPMiss.init("mLKUP", "LKUP misses");
    profINV.init("INV", "Invalidates");
    profLKUPNextLevelLat.init("latLKUPnl", "LKUP latency on next level");

    parentStat->append(&profLKUPHit);
    parentStat->append(&profLKUPMiss);
    parentStat->append(&profINV);
    parentStat->append(&profLKUPNextLevelLat);
    parentStat->append(tlbStat);
}

void TLB::initTLBStats(AggregateStat* tlbStat) {
    array->initStats(tlbStat);
    rp->initStats(tlbStat);
}

uint64_t TLB::access(const TransReq& req) {
    uint64_t respCycle = req.cycle;
    bool updateReplacement = true;

    lock();
    int32_t wayIdx = array->lookup(req.pageAddr, &req, updateReplacement);
    respCycle += accLat;

    if (wayIdx == -1) {
        if (parent != nullptr) {
          respCycle = parent->access(req);
        }
        Address victimPageAddr;
        wayIdx = array->insert(req.pageAddr, &req, &victimPageAddr);
        //trace(TLB, "[%s] Evicting 0x%lx", name.c_str(), victimPageAddr);
    }
    unlock();

    assert_msg(respCycle >= req.cycle, "[%s] resp < req? 0x%lx, respCycle %ld reqCycle %ld",
            name.c_str(), req.pageAddr, respCycle, req.cycle);
    return respCycle;
}

uint64_t TLB::invalidate(const TransInvReq& req) {
    uint64_t respCycle = req.cycle;
    respCycle += invLat;

    lock();
    if (req.type == INVPTE) {
      array->invalidPTE(req.pageAddr);
    }
    else if(req.type == INVALL) {
      array->invalidAll();
    } 

    if (req.level > level) {
      respCycle = parent->invalidate(req);
    }
    unlock();

    return respCycle;
}