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

       virtual uint32_t invalidPTE(const Address pageAddr) = 0;
        virtual void invalidAll(void) = 0;
        virtual void initStats(AggregateStat* parent) {}
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
        SetAssocTLBArray(uint32_t _numEntries, uint32_t _assoc, TLBReplPolicy* _rp, HashFamily* _hf);

        int32_t lookup(const Address pageAddr, const TransReq* req, bool updateReplacement);
        uint32_t insert(const Address pageAddr, const TransReq* req, Address* victimPageAddr);
        uint32_t invalidPTE(const Address pageAddr);
        void invalidAll(void);

};

struct SetAssocTLBCands {
    struct iterator {
        uint32_t x;
        explicit inline iterator(uint32_t _x) : x(_x) {}
        inline void inc() {x++;} //overloading prefix/postfix too messy
        inline uint32_t operator*() const { return x; }
        inline bool operator==(const iterator& it) const { return it.x == x; }
        inline bool operator!=(const iterator& it) const { return it.x != x; }
    };

    uint32_t b, e;
    inline SetAssocTLBCands(uint32_t _b, uint32_t _e) : b(_b), e(_e) {}
    inline iterator begin() const {return iterator(b);}
    inline iterator end() const {return iterator(e);}
    inline uint32_t numCands() const { return e-b; }
};

#endif  // TLB_ARRAYS_H_
