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

#ifndef TRANSLATION_HIERARCHY_H_
#define TRANSLATION_HIERARCHY_H_

/* Type and interface definitions of translation objects */

#include <stdint.h>
#include "g_std/g_vector.h"
#include "galloc.h"
#include "locks.h"
#include "memory_hierarchy.h"

/** TYPES **/

/* Addresses are plain 64-bit uints. This should be kept compatible with PIN addrints */
typedef uint64_t Address;

/* Types of Invalidation. An Invalidation is a request issued from upper to lower
 * levels of the hierarchy.
 */
typedef enum {
    INVPTE,  // invalidate a pte
    INVALL,  // invalidate all
} TransInvType;

//Convenience methods for clearer debug traces
const char* TransInvTypeName(TransInvType t);

/* Memory request */
struct TransReq {
    Address pageAddr;
    uint32_t childId;
    uint64_t cycle; //cycle where request arrives at component

    //Used for race detection/sync
    lock_t* childLock;

    //Requester id --- used for contention simulation
    uint32_t srcId;

    //Flags propagate across levels, though not to evictions
    //Some other things that can be indicated here: Demand vs prefetch accesses, TLB accesses, etc.
    enum Flag {
        PREFETCH      = (1<<1), //Prefetch
    };
    uint32_t flags;

    inline void set(Flag f) {flags |= f;}
    inline bool is (Flag f) const {return flags & f;}
};

/* Invalidation/downgrade request */
struct TransInvReq {
    Address pageAddr;
    TransInvType type;
    uint64_t cycle;
    uint32_t level;
};

/** INTERFACES **/

//class AggregateStat;

/* Base class for all memory objects (caches and memories) */
class TransObject : public GlobAlloc {
    public:
        //Returns response cycle
        virtual uint64_t access(const TransReq& req) = 0;
        virtual uint64_t invalidate(const TransInvReq& req) = 0;

        virtual void initStats(AggregateStat* parentStat) {}
        virtual const char* getName() = 0;
};

/* Base class for all tlb objects */
class BaseTLB : public TransObject {
    public:
        virtual void setParent(TransObject* parent) = 0;
};

/* Base class for all ptw objects */
class BasePTW : public TransObject {
    public:
        virtual void setParentMem(MemObject* parentMem) = 0;
};
#endif  // TRANSLATION_HIERARCHY_H_
