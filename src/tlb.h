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

#ifndef TLB_H_
#define TLB_H_

#include "tlb_arrays.h"
#include "g_std/g_string.h"
#include "g_std/g_vector.h"
#include "translation_hierarchy.h"
#include "repl_policies.h"
#include "stats.h"

/* General coherent modular cache. The replacement policy and cache array are
 * pretty much mix and match. The coherence controller interfaces are general
 * too, but to avoid virtual function call overheads we work with MESI
 * controllers, since for now we only have MESI controllers
 */
class TLB : public BaseTLB {
    protected:
        TransObject* parent;
        TLBArray* array;
        TLBReplPolicy* rp;

        uint32_t numEntries;

        //Latencies
        uint32_t accLat; //latency of a normal access (could split in get/put, probably not needed)
        uint32_t invLat; //latency of an invalidation

        //Profiling counters
        Counter profLKUPHit, profLKUPMiss;
        Counter profINV;
        Counter profLKUPNextLevelLat;

        //bool inclusive;

        PAD();
        lock_t tlbLock;
        PAD();

        uint32_t level;
        g_string name;

    public:
        TLB(uint32_t _numEntries, TransObject* _parent, TLBArray* _array, TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, const g_string& _name);
        TLB(uint32_t _numEntries, TLBArray* _array, TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, const g_string& _name);

        const char* getName();
        void setParent(TransObject* parent);
        void initStats(AggregateStat* parentStat);

        uint64_t access(const TransReq& req);

        uint64_t invalidate(const TransInvReq& req);

        inline void lock() {
            futex_lock(&tlbLock);
        }

        inline void unlock() {
            futex_unlock(&tlbLock);
        }

    protected:
        void initTLBStats(AggregateStat* TLBStat);
};

#endif  // TLB_H_
