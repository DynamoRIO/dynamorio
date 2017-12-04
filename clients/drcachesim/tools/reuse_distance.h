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

#include <unordered_map>
#include <string>
#include <assert.h>
#include <iostream>
#include "../analysis_tool.h"
#include "../common/memref.h"

// We see noticeable overhead in release build with an if() that directly
// checks knob_verbose, so for debug-only uses we turn it into something the
// compiler can remove for better performance without going so far as ifdef-ing
// big code chunks and impairing readability.
#ifdef DEBUG
# define DEBUG_VERBOSE(level) (reuse_distance_t::knob_verbose >= (level))
#else
# define DEBUG_VERBOSE(level) (false)
#endif

struct line_ref_t;
struct line_ref_list_t;

class reuse_distance_t : public analysis_tool_t
{
 public:
    reuse_distance_t(unsigned int line_size,
                     bool report_histogram,
                     unsigned int distance_threshold,
                     unsigned int report_top,
                     unsigned int skip_list_distance,
                     bool verify_skip,
                     unsigned int verbose);
    virtual ~reuse_distance_t();
    virtual bool process_memref(const memref_t &memref);
    virtual bool print_results();

    // Global value for use in non-member code.
    static unsigned int knob_verbose;

 protected:
    /* XXX i#2020: use unsorted_map (C++11) for faster lookup */
    std::unordered_map<addr_t, line_ref_t*> cache_map;
    // This is our reuse distance histogram.
    std::unordered_map<int_least64_t, int_least64_t> dist_map;
    line_ref_list_t *ref_list;

    unsigned int knob_line_size;
    bool knob_report_histogram;
    unsigned int knob_report_top; /* most accessed lines */

    uint64_t time_stamp;
    size_t line_size_bits;
    int_least64_t total_refs;
    static const std::string TOOL_NAME;
};

/* A doubly linked list node for the cache line reference info */
struct line_ref_t
{
    struct line_ref_t *prev;  // the prev line_ref in the list
    struct line_ref_t *next;  // the next line_ref in the list
    uint64_t time_stamp;      // the most recent reference time stamp on this line
    uint64_t total_refs;      // the total number of references on this line
    uint64_t distant_refs;    // the total number of distant references on this line
    addr_t tag;

    // We have a one-layer skip list for more efficient depth computation.
    // We inline the fields in every node for simplicity and to reduce allocs.
    struct line_ref_t *prev_skip;  // the prev line_ref in the skip list
    struct line_ref_t *next_skip;  // the next line_ref in the skip list
    int_least64_t depth; // only valid for skip list nodes; -1 for others

    line_ref_t(addr_t val) :
        prev(NULL), next(NULL), total_refs(1), distant_refs(0), tag(val),
        prev_skip(NULL), next_skip(NULL), depth(-1)
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
//
// We have a second doubly-linked list, a one-layer skip list, for
// more efficient computation of the depth.  Each node in the skip
// list stores its depth from the front.
struct line_ref_list_t
{
    line_ref_t *head;       // the most recently accessed cache line
    line_ref_t *gate;       // the earliest cache line refs within the threshold
    uint64_t cur_time;      // current time stamp
    uint64_t unique_lines;  // the total number of unique cache lines accessed
    uint64_t threshold;     // the reuse distance threshold
    uint64_t skip_distance; // distance between skip list nodes
    bool verify_skip;       // check results using brute-force walks

    line_ref_list_t(uint64_t reuse_threshold, uint64_t skip_dist, bool verify) :
        head(NULL), gate(NULL), cur_time(0), unique_lines(0),
        threshold(reuse_threshold), skip_distance(skip_dist), verify_skip(verify)
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

    void
    print_list()
    {
        std::cerr << "Reuse tag list:\n";
        for (line_ref_t *node = head; node != NULL; node = node->next) {
            std::cerr << "\tTag 0x" << std::hex << node->tag;
            if (node->depth != -1) {
                std::cerr << " depth=" << std::dec << node->depth
                          << " prev=" << std::hex
                          << (node->prev_skip == NULL ? 0 : node->prev_skip->tag)
                          << " next=" << std::hex
                          << (node->next_skip == NULL ? 0 : node->next_skip->tag);
                assert(node->next_skip == NULL || node->next_skip->prev_skip == node);
            } else
                assert(node->next_skip == NULL && node->prev_skip == NULL);
            std::cerr << "\n";
        }
    }

    void
    move_skip_fields(line_ref_t *src, line_ref_t *dst)
    {
        dst->prev_skip = src->prev_skip;
        dst->next_skip = src->next_skip;
        dst->depth = src->depth;
        if (src->prev_skip != NULL)
            src->prev_skip->next_skip = dst;
        if (src->next_skip != NULL)
            src->next_skip->prev_skip = dst;
        src->prev_skip = NULL;
        src->next_skip = NULL;
        src->depth = -1;
    }

    // Add a new cache line to the front of the list.
    // We may need to move gate forward if there are more cache lines
    // than the threshold so that the gate points to the earliest
    // referenced cache line within the threshold.
    void
    add_to_front(line_ref_t *ref)
    {
        if (DEBUG_VERBOSE(3))
            std::cerr << "Add tag 0x" << std::hex << ref->tag << "\n";
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

        // Add a new skip node if necessary.
        // We don't bother keeping one right at the front: too much overhead.
        uint64_t count = 0;
        line_ref_t *node, *skip = NULL;
        for (node = head; node != NULL && node->depth == -1; node = node->next) {
            ++count;
            if (count == skip_distance)
                skip = node;
        }
        if (count >= 2*skip_distance-1) {
            assert(skip != NULL);
            if (DEBUG_VERBOSE(3))
                std::cerr << "New skip node for tag 0x" << std::hex << skip->tag << "\n";
            skip->depth = skip_distance - 1;
            if (node != NULL) {
                assert(node->prev_skip == NULL);
                node->prev_skip = skip;
            }
            skip->next_skip = node;
            assert(skip->prev_skip == NULL);
        }
        // Update skip list depths.
        for (; node != NULL; node = node->next_skip)
            ++node->depth;
        if (DEBUG_VERBOSE(3))
            print_list();
    }

    // Move a referenced cache line to the front of the list.
    // We need to move the gate pointer forward if the referenced cache
    // line is the gate cache line or any cache line after.
    // Returns the reuse distance of ref.
    int_least64_t
    move_to_front(line_ref_t *ref)
    {
        if (DEBUG_VERBOSE(3))
            std::cerr << "Move tag 0x" << std::hex << ref->tag << " to front\n";
        line_ref_t *prev;
        line_ref_t *next;

        ref->total_refs++;
        if (ref == head)
            return 0;
        if (ref_is_distant(ref)) {
            ref->distant_refs++;
            gate = gate->prev;
        } else if (ref == gate) {
            // move gate if ref is the gate.
            gate = gate->prev;
        }

        // Compute reuse distance.
        int_least64_t dist = 0;
        line_ref_t *skip;
        for (skip = ref; skip != NULL && skip->depth == -1; skip = skip->prev)
            ++dist;
        if (skip != NULL)
            dist += skip->depth;
        else
            --dist; // Don't count self.

        if (DEBUG_VERBOSE(0) && verify_skip) {
            // Compute reuse distance with a full list walk as a sanity check.
            // This is a debug-only option, so we guard with DEBUG_VERBOSE(0).
            // Yes, the option check branch shows noticeable overhead without it.
            int_least64_t brute_dist = 0;
            for (prev = head; prev != ref; prev = prev->next)
                ++brute_dist;
            if (brute_dist != dist) {
                std::cerr << "Mismatch!  Brute=" << brute_dist
                          << " vs skip=" << dist << "\n";
                print_list();
                assert(false);
            }
        }

        // Shift skip nodes between where ref was and head one earlier to
        // maintain spacing.  This means their depths remain the same.
        if (skip != NULL) {
            for (; skip != NULL; skip = next) {
                next = skip->prev_skip;
                assert(skip->prev != NULL);
                move_skip_fields(skip, skip->prev);
            }
        } else
            assert(ref->depth == -1);

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

        if (DEBUG_VERBOSE(3))
            print_list();
        // XXX: we should keep a running mean of the distance, and adjust
        // knob_reuse_skip_dist to stay close to the mean, for best performance.
        return dist;
    }
};

#endif /* _REUSE_DISTANCE_H_ */
