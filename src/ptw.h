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

#ifndef PTW_H_
#define PTW_H_

#include "g_std/g_string.h"
#include "g_std/g_vector.h"
#include "translation_hierarchy.h"
#include "repl_policies.h"
#include "stats.h"

class PTWCache : public GlobAlloc {
};

class SetAssocPTWCache : public PTWCache {
  protected:
      uint32_t numEntries;
      uint32_t numSets;
      uint32_t assoc;
      uint32_t setMask;
  public:
      SetAssocPTWCache(uint32_t _numEntries, uint32_t _assoc) : 
        numEntries(_numEntries), assoc(_assoc) {
          numSets = numEntries/assoc;
          setMask = numSets - 1;
          assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
        }
};

class PTW : public BasePTW {
    protected:
        MemObject* parentMem;
        PTWCache* ptwCache;

        bool realMemAccess;
        //Latencies
        uint32_t accLat; //latency of a normal request
        uint32_t invLat; //latency of an invalidation

        //Profiling counters
        Counter profREQ;
        Counter profINVPTE;
        Counter profINVALL;
        Counter profMemAccess;
        Counter profMemAccessLat;

        //bool inclusive;

        PAD();
        lock_t ptwLock;
        PAD();

        g_string name;

    public:
        PTW(MemObject* _parentMem, PTWCache* _ptwCache, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name);
        PTW(PTWCache* _ptwCache, bool _realMemAccess, uint32_t _accLat, uint32_t _invLat, const g_string& _name);

        const char* getName();
        void setParentMem(MemObject* _parentMem);
        void initStats(AggregateStat* parentStat);

        uint64_t access(const TransReq& req);

        uint64_t invalidate(const TransInvReq& req);

        inline void lock() {
            futex_lock(&ptwLock);
        }

        inline void unlock() {
            futex_unlock(&ptwLock);
        }

    protected:
        void initPTWStats(AggregateStat* ptwStat);
};

#endif  // PTW_H_
