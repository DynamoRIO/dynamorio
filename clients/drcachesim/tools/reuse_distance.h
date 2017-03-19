/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* reuse-distance: a memory trace reuse distance analysis tool.
 */

#ifndef _REUSE_DISTANCE_H_
#define _REUSE_DISTANCE_H_ 1

#include <map>
#include <string>
#include "../analysis_tool.h"
#include "../common/memref.h"

/* A doubly linked list node for the cache line reference info */
struct line_ref_t
{
    struct line_ref_t *prev;  // the prev line_ref in the list
    struct line_ref_t *next;  // the next line_ref in the list
    uint64_t time_stamp;      // the most recent reference time stamp on this line
    uint64_t total_refs;      // the total number of references on this line
    uint64_t distant_refs;    // the total number of distant references on this line
    addr_t tag;
    line_ref_t(addr_t val) :
        prev(NULL), next(NULL), total_refs(1), distant_refs(0), tag(val)
    {
    }
};

// We use a doubly linked list to keep track of the cache line reuse distance.
// The head of the list is the most recently accessed cache line.
// The earlier a cache line was accessed last time, the deeper that cache line
// is in the list.
// If a cache line is accessed, its time stamp is set as current, and it is
// added/moved to the front of the list.  The cache line reference
// reuse distance is the cache line position in the list before moving.
// We also keep a pointer (gate) pointing to the the earliest cache
// line referenced within the threshold.  Thus, we can quickly check
// whether a cache line is recently accessed by comparing the time
// stamp of the referenced cache line and the gate cache line.
struct line_ref_list_t
{
    line_ref_t *head;       // the most recently accessed cache line
    line_ref_t *gate;       // the earliest cache line refs within the threshold
    uint64_t cur_time;      // current time stamp
    uint64_t unique_lines;  // the total number of unique cache lines accessed
    uint64_t threshold;     // the reuse distance threshold

    line_ref_list_t(uint64_t reuse_threshold) :
        head(NULL), gate(NULL), cur_time(0), unique_lines(0), threshold(reuse_threshold)
    {
    }

    virtual
    ~line_ref_list_t()
    {
        line_ref_t *ref;
        line_ref_t *next;
        if (head == NULL)
            return;
        for (ref = head; ref != NULL; ref = next) {
            next = ref->next;
            delete ref;
        }
    }

    bool
    ref_is_distant(line_ref_t *ref)
    {
        if (gate == NULL || ref->time_stamp >= gate->time_stamp)
            return false;
        return true;
    }

    // Add a new cache line to the front of the list.
    // We may need to move gate forward if there are more cache lines
    // than the threshold so that the gate points to the earliest
    // referenced cache line within the threshold.
    void
    add_to_front(line_ref_t *ref)
    {
        // update head
        ref->next = head;
        if (head != NULL)
            head->prev = ref;
        head = ref;
        if (gate == NULL)
            gate = head;
        // move gate forward if necessary
        if (unique_lines > threshold)
            gate = gate->prev;
        unique_lines++;
        head->time_stamp = cur_time++;
    }

    // Move a referenced cache line to the front of the list.
    // We need to move the gate pointer forward if the referenced cache
    // line is the gate cache line or any cache line after.
    void
    move_to_front(line_ref_t *ref)
    {
        line_ref_t *prev;
        line_ref_t *next;

        ref->total_refs++;
        if (ref == head)
            return;
        if (ref_is_distant(ref)) {
            ref->distant_refs++;
            gate = gate->prev;
        } else if (ref == gate) {
            // move gate if ref is the gate.
            gate = gate->prev;
        }

        // remove ref from the list
        prev = ref->prev;
        next = ref->next;
        prev->next = next;
        // ref could be the last
        if (next != NULL)
            next->prev = prev;
        // move ref to the front
        ref->prev = NULL;
        ref->next = head;
        head->prev = ref;
        head = ref;
        head->time_stamp = cur_time++;
    }
};

class reuse_distance_t : public analysis_tool_t
{
 public:
    reuse_distance_t();
    virtual ~reuse_distance_t();
    virtual bool process_memref(const memref_t &memref);
    virtual bool print_results();

 protected:
    /* XXX i#2020: use unsorted_map (C++11) for faster lookup */
    std::map<addr_t, line_ref_t*> cache_map;
    line_ref_list_t *ref_list;

    uint64_t time_stamp;
    size_t line_size;
    size_t line_size_bits;
    int_least64_t total_refs;
    size_t report_top;  /* most accessed lines */
    static const std::string TOOL_NAME;
};

#endif /* _REUSE_DISTANCE_H_ */
