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

#ifndef FILTER_TLB_H_
#define FILTER_TLB_H_

#include "bithacks.h"
#include "tlb.h"
#include "tlb_arrays.h"
#include "galloc.h"
#include "zsim.h"

/* Extends TLB with an L0 direct-mapped cache, optimized to hell for hits
 *
 * L1 lookups are dominated by several kinds of overhead (grab the cache locks,
 * several virtual functions for the replacement policy, etc.). This
 * specialization of Cache solves these issues by having a filter array that
 * holds the most recently used line in each set. Accesses check the filter array,
 * and then go through the normal access path. Because there is one line per set,
 * it is fine to do this without grabbing a lock.
 */

class FilterTLB : public TLB {
    private:
        struct FilterEntry {
            volatile Address rdAddr;
            volatile uint64_t availCycle;

            void clear() {rdAddr = 0; availCycle = 0;}
        };

        //Replicates the most accessed line of each set in the tlb
        FilterEntry* filterArray;
        Address setMask;
        uint32_t numSets;
        uint32_t srcId; //should match the core
        uint32_t reqFlags;

        lock_t filterLock;
        uint64_t fLKUPHit;

    public:
        FilterTLB(uint32_t _numSets, uint32_t _numEntries, TLBArray* _array,
                TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, g_string& _name)
            : TLB(_numEntries, _array, _rp, _accLat, _invLat, _name)
        {
            numSets = _numSets;
            setMask = numSets - 1;
            filterArray = gm_memalign<FilterEntry>(CACHE_LINE_BYTES, numSets);
            for (uint32_t i = 0; i < numSets; i++) filterArray[i].clear();
            futex_init(&filterLock);
            fLKUPHit = 0;
            srcId = -1;
            reqFlags = 0;
        }

        FilterTLB(uint32_t _numSets, uint32_t _numEntries, TransObject* _parent, TLBArray* _array,
                TLBReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, g_string& _name)
            : TLB(_numEntries, _parent, _array, _rp, _accLat, _invLat, _name)
        {
            numSets = _numSets;
            setMask = numSets - 1;
            filterArray = gm_memalign<FilterEntry>(CACHE_LINE_BYTES, numSets);
            for (uint32_t i = 0; i < numSets; i++) filterArray[i].clear();
            futex_init(&filterLock);
            fLKUPHit = 0;
            srcId = -1;
            reqFlags = 0;
        }

        void setSourceId(uint32_t id) {
            srcId = id;
        }

        void setFlags(uint32_t flags) {
            reqFlags = flags;
        }

        void initStats(AggregateStat* parentStat) {
            AggregateStat* tlbStat = new AggregateStat();
            tlbStat->init(name.c_str(), "Filter tlb stats");

            ProxyStat* flkupStat = new ProxyStat();
            flkupStat->init("fhLKUP", "Filtered Lookup hits", &fLKUPHit);
            tlbStat->append(flkupStat);

            initTLBStats(tlbStat);
            parentStat->append(tlbStat);
        }

        inline uint64_t lookup(Address vAddr, uint64_t curCycle) {
            //info("filter_tlb %s lookup Addr = %lx", name.c_str(), vAddr);
            Address vPageAddr = vAddr >> pageBits;
            uint32_t idx = vPageAddr & setMask;
            uint64_t availCycle = filterArray[idx].availCycle; //read before, careful with ordering to avoid timing races
            if (vPageAddr == filterArray[idx].rdAddr) {
                fLKUPHit++;
                return MAX(curCycle, availCycle);
            } else {
                return replace(vPageAddr, idx, curCycle);
            }
        }

        uint64_t replace(Address vPageAddr, uint32_t idx, uint64_t curCycle) {
            Address vPageAddrWithPID = procMask | vPageAddr;
            futex_lock(&filterLock);
            TransReq req = {vPageAddrWithPID, 0, curCycle, &filterLock, srcId, reqFlags};
            uint64_t respCycle  = access(req);

            //Careful with this order
            Address oldAddr = filterArray[idx].rdAddr;
            filterArray[idx].rdAddr = vPageAddr;

            if (oldAddr != vPageAddr) filterArray[idx].availCycle = respCycle;
            futex_unlock(&filterLock);
            return respCycle;
        }

        uint64_t invalidate(const TransInvReq& req) {
            futex_lock(&filterLock);
            uint32_t idx = req.pageAddr & setMask; //works because of how virtual<->physical is done...
            if ((filterArray[idx].rdAddr | procMask) == req.pageAddr) { //FIXME: If another process calls invalidate(), procMask will not match even though we may be doing a capacity-induced invalidation!
                filterArray[idx].rdAddr = -1L;
            }
            uint64_t respCycle = TLB::invalidate(req); // releases cache's downLock
            futex_unlock(&filterLock);
            return respCycle;
        }

        void contextSwitch() {
            futex_lock(&filterLock);
            for (uint32_t i = 0; i < numSets; i++) filterArray[i].clear();
            futex_unlock(&filterLock);
        }
};

#endif  // FILTER_TLB_H_
