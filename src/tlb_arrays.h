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

#ifndef TLB_ARRAYS_H_
#define TLB_ARRAYS_H_

#include "translation_hierarchy.h"
#include "stats.h"
#include "hash.h"
#include "repl_policies.h"

/* General interface of a cache array. The array is a fixed-size associative container that
 * translates addresses to line IDs. A line ID represents the position of the tag. The other
 * cache components store tag data in non-associative arrays indexed by line ID.
 */
class TLBArray : public GlobAlloc {
    public:
        /* Returns tag's ID if present, -1 otherwise. If updateReplacement is set, call the replacement policy's update() on the line accessed*/
        virtual int32_t lookup(const Address pageAddr, const TransReq* req, bool updateReplacement) = 0;

        /* Runs replacement scheme, returns tag ID of new pos and address of victim pte*/
        virtual uint32_t insert(const Address pageAddr, const TransReq* req, Address* victimPageAddr) = 0;

        virtual void initStats(AggregateStat* parent) {}
        virtual uint32_t invalidPTE(const Address pageAddr);
        virtual void invalidAll(const Address pageAddr);
};

class TLBReplPolicy;
class HashFamily;

/* Set-associative cache array */
class SetAssocTLBArray : public TLBArray {
    protected:
        Address* array;
        TLBReplPolicy* rp;
        HashFamily* hf;
        uint32_t numEntries;
        uint32_t numSets;
        uint32_t assoc;
        uint32_t setMask;

    public:
//        SetAssocTLBArray(uint32_t _numEntries, uint32_t _assoc, TLBReplPolicy* _rp, HashFamily* _hf);
        SetAssocTLBArray(uint32_t _numEntries, uint32_t _assoc, TLBReplPolicy* _rp, HashFamily* _hf) : rp(_rp), hf(_hf), numEntries(_numEntries), assoc(_assoc)  {
            array = gm_calloc<Address>(numEntries);
            numSets = numEntries/assoc;
            setMask = numSets - 1;
            assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
        }

        int32_t lookup(const Address pageAddr, const TransReq* req, bool updateReplacement);
        uint32_t insert(const Address pageAddr, const TransReq* req, Address* victimPageAddr);
        uint32_t invalidPTE(const Address pageAddr);
        void invalidAll(const Address pageAddr);
};

#endif  // TLB_ARRAYS_H_
