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

#include "tlb_arrays.h"
#include "hash.h"
#include "repl_policies.h"

/* Set-associative array implementation */

//SetAssocTLBArray::SetAssocTLBArray(uint32_t _numEntries, uint32_t _assoc, TLBReplPolicy* _rp, HashFamily* _hf) : rp(_rp), hf(_hf), numEntries(_numEntries), assoc(_assoc)  {
//    array = gm_calloc<Address>(numEntries);
//    numSets = numEntries/assoc;
//    setMask = numSets - 1;
//    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
//}

int32_t SetAssocTLBArray::lookup(const Address pageAddr, const TransReq* req, bool updateReplacement) {
    uint32_t set = hf->hash(0, pageAddr) & setMask;
    uint32_t first = set*assoc;
    for (uint32_t id = first; id < first + assoc; id++) {
        if (array[id] ==  pageAddr) {
            if (updateReplacement) rp->update(id, req);
            return id;
        }
    }
    return -1;
}

uint32_t SetAssocTLBArray::insert(const Address pageAddr, const TransReq* req, Address* victimPageAddr) { //TODO: Give out valid bit of wb cand?
    uint32_t set = hf->hash(0, pageAddr) & setMask;
    uint32_t first = set*assoc;

    uint32_t candidate = rp->rankCands(req, SetAssocCands(first, first+assoc));

    *victimPageAddr = array[candidate];

    rp->replaced(candidate);
    array[candidate] = pageAddr;
    rp->update(candidate, req);

    return candidate;
}

uint32_t SetAssocTLBArray::invalidPTE(const Address pageAddr) {
    uint32_t set = hf->hash(0, pageAddr) & setMask;
    uint32_t first = set*assoc;
    for (uint32_t id = first; id < first + assoc; id++) {
        if (array[id] ==  pageAddr) {
            rp->replaced(id);
            return id;
        }
    }
    return -1;
}

void SetAssocTLBArray::invalidAll(const Address pageAddr) {
    for (uint32_t id = 0; id < numEntries; id++) {
        rp->replaced(id);
    }
}
