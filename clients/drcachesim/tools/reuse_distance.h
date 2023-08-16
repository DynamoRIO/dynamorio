/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "reuse_distance_create.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// We see noticeable overhead in release build with an if() that directly
// checks knob_verbose, so for non-debug uses we eliminate it entirely.
// Example usage:
//   IF_DEBUG_VERBOSE(1, std::cerr << "This code was executed.\n")
#ifdef DEBUG
#    define IF_DEBUG_VERBOSE(level, action)                  \
        do {                                                 \
            if (reuse_distance_t::knob_verbose >= (level)) { \
                action;                                      \
            }                                                \
        } while (0)
#else
#    define IF_DEBUG_VERBOSE(level, action)
#endif

struct line_ref_list_t;
struct line_ref_t;

class reuse_distance_t : public analysis_tool_t {
public:
    explicit reuse_distance_t(const reuse_distance_knobs_t &knobs);
    ~reuse_distance_t() override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

    // Global value for use in non-member code.
    // XXX: Change to an instance variable so multiple instances can have
    // different verbosities.
    static unsigned int knob_verbose;

    using distance_histogram_t = std::unordered_map<int64_t, int64_t>;
    using distance_map_pair_t = std::pair<int64_t, int64_t>;

protected:
    // We assume that the shard unit is the unit over which we should measure
    // distance.  By default this is a traced thread.  For serial operation we look
    // at the tid values and enforce it to be a thread, but for parallel we just use
    // the shards we're given.  This is for simplicity and to give the user a method
    // for computing over different units if for some reason that was desired.
    struct shard_data_t {
        shard_data_t(uint64_t reuse_threshold, uint64_t skip_dist,
                     unsigned int distance_limit, bool verify);
        std::unordered_map<addr_t, line_ref_t *> cache_map;
        std::unordered_set<addr_t> pruned_addresses;
        // These are our reuse distance histograms: one for all accesses and one
        // only for data references.  An instruction histogram can be computed by
        // subtracting data references from the full histogram.  The final histogram
        // statistics need a full histogram to sort, and in most traces the majority of
        // accesss are instruction references, so the histograms are split this way to
        // provide the full histogram we need with the smallest secondary histogram.
        // Furthermore, during analysis each reference is added to only one histogram
        // to minimize the performance impact of dual histogram collection, with the data
        // references added into the primary histogram during final result aggregation.
        // This means dist_map is effectively instruction-only until aggregation.
        distance_histogram_t dist_map;
        distance_histogram_t dist_map_data;
        bool dist_map_is_instr_only = true;
        std::unique_ptr<line_ref_list_t> ref_list;
        int64_t total_refs = 0;
        int64_t data_refs = 0; // Non-instruction reference count.
        // Ideally the shard index would be the tid when shard==thread but that's
        // not the case today so we store the tid.
        memref_tid_t tid;
        std::string error;
        // Keep a per-shard copy of distance_limit for parallel operation.
        unsigned int distance_limit = 0;
        // Track the number of insertions (pruned_address_count) and deletions
        // (pruned_address_hits) from the pruned_addresses set.
        uint64_t pruned_address_count = 0;
        uint64_t pruned_address_hits = 0;
    };

    void
    print_histogram(std::ostream &out, int64_t total_count,
                    const std::vector<distance_map_pair_t> &sorted,
                    const distance_histogram_t &dist_map_data);

    void
    print_shard_results(const shard_data_t *shard);

    // Return a pointer to aggregate results, building them if needed.
    virtual const shard_data_t *
    get_aggregated_results();
    std::unique_ptr<shard_data_t> aggregated_results_;

    const reuse_distance_knobs_t knobs_;
    const size_t line_size_bits_;
    static const std::string TOOL_NAME;
    // In parallel operation the keys are "shard indices": just ints.
    std::unordered_map<memref_tid_t, shard_data_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
};

/* A doubly linked list node for the cache line reference info */
struct line_ref_t {
    struct line_ref_t *prev; // the prev line_ref in the list
    struct line_ref_t *next; // the next line_ref in the list
    uint64_t time_stamp;     // the most recent reference time stamp on this line
    uint64_t total_refs;     // the total number of references on this line
    uint64_t distant_refs;   // the total number of distant references on this line
    addr_t tag;

    // We have a one-layer skip list for more efficient depth computation.
    // We inline the fields in every node for simplicity and to reduce allocs.
    struct line_ref_t *prev_skip; // the prev line_ref in the skip list
    struct line_ref_t *next_skip; // the next line_ref in the skip list
    int64_t depth;                // only valid for skip list nodes; -1 for others

    line_ref_t(addr_t val)
        : prev(NULL)
        , next(NULL)
        , total_refs(1)
        , distant_refs(0)
        , tag(val)
        , prev_skip(NULL)
        , next_skip(NULL)
        , depth(-1)
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
// We also keep a pointer (gate) pointing to the earliest cache
// line referenced within the threshold.  Thus, we can quickly check
// whether a cache line is recently accessed by comparing the time
// stamp of the referenced cache line and the gate cache line.
//
// We have a second doubly-linked list, a one-layer skip list, for
// more efficient computation of the depth.  Each node in the skip
// list stores its depth from the front.
struct line_ref_list_t {
    line_ref_t *head_;       // the most recently accessed cache line
    line_ref_t *gate_;       // the earliest cache line refs within the threshold
    line_ref_t *tail_;       // the least recently accessed cache line
    uint64_t cur_time_;      // current time stamp
    uint64_t unique_lines_;  // the total number of unique cache lines accessed
    uint64_t threshold_;     // the reuse distance threshold
    uint64_t skip_distance_; // distance between skip list nodes
    bool verify_skip_;       // check results using brute-force walks

    line_ref_list_t(uint64_t reuse_threshold_, uint64_t skip_dist, bool verify)
        : head_(NULL)
        , gate_(NULL)
        , tail_(NULL)
        , cur_time_(0)
        , unique_lines_(0)
        , threshold_(reuse_threshold_)
        , skip_distance_(skip_dist)
        , verify_skip_(verify)
    {
    }

    virtual ~line_ref_list_t()
    {
        line_ref_t *ref;
        line_ref_t *next;
        if (head_ == NULL)
            return;
        for (ref = head_; ref != NULL; ref = next) {
            next = ref->next;
            delete ref;
        }
    }

    bool
    ref_is_distant(line_ref_t *ref)
    {
        if (gate_ == NULL || ref->time_stamp >= gate_->time_stamp)
            return false;
        return true;
    }

    void
    print_list()
    {
        std::cerr << "Reuse tag list:\n";
        for (line_ref_t *node = head_; node != NULL; node = node->next) {
            std::cerr << "\tTag 0x" << std::hex << node->tag;
            if (node->depth != -1) {
                std::cerr << " depth=" << std::dec << node->depth << " prev=" << std::hex
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
    // We may need to move gate_ forward if there are more cache lines
    // than the threshold so that the gate points to the earliest
    // referenced cache line within the threshold.
    void
    add_to_front(line_ref_t *ref)
    {
        IF_DEBUG_VERBOSE(3, std::cerr << "Add tag 0x" << std::hex << ref->tag << "\n");
        // update head_
        ref->next = head_;
        if (head_ != NULL)
            head_->prev = ref;
        head_ = ref;
        if (gate_ == NULL)
            gate_ = head_;
        // move gate_ forward if necessary
        if (unique_lines_ > threshold_)
            gate_ = gate_->prev;
        if (tail_ == NULL)
            tail_ = ref;
        unique_lines_++;
        head_->time_stamp = cur_time_++;

        // Add a new skip node if necessary.
        // We don't bother keeping one right at the front: too much overhead_.
        uint64_t count = 0;
        line_ref_t *node, *skip = NULL;
        for (node = head_; node != NULL && node->depth == -1; node = node->next) {
            ++count;
            if (count == skip_distance_)
                skip = node;
        }
        if (count >= 2 * skip_distance_ - 1) {
            assert(skip != NULL);
            IF_DEBUG_VERBOSE(3,
                             std::cerr << "New skip node for tag 0x" << std::hex
                                       << skip->tag << "\n");
            skip->depth = skip_distance_ - 1;
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
        IF_DEBUG_VERBOSE(3, print_list());
    }

    // Remove the last entry from the distance list.
    void
    prune_tail()
    {
        // Make sure the tail pointers are legal.
        assert(tail_ != NULL);
        assert(tail_ != head_);
        assert(tail_->next == NULL);
        assert(tail_->prev != NULL);

        IF_DEBUG_VERBOSE(3,
                         std::cerr << "Prune tag 0x" << std::hex << tail_->tag << "\n");

        line_ref_t *new_tail = tail_->prev;
        new_tail->next = NULL;

        // If there's a prior skip, remove its ptr to tail.
        if (tail_->depth != -1 && tail_->prev_skip != NULL) {
            tail_->prev_skip->next_skip = NULL;
        }

        if (tail_ == gate_) {
            // move gate_ if tail_ was the gate_.
            gate_ = gate_->prev;
        }

        // And finally, update tail_.
        tail_ = new_tail;
    }

    // Move a referenced cache line to the front of the list.
    // We need to move the gate_ pointer forward if the referenced cache
    // line is the gate_ cache line or any cache line after.
    // Returns the reuse distance of ref.
    int64_t
    move_to_front(line_ref_t *ref)
    {
        IF_DEBUG_VERBOSE(
            3, std::cerr << "Move tag 0x" << std::hex << ref->tag << " to front\n");
        line_ref_t *prev;
        line_ref_t *next;

        ref->total_refs++;
        if (ref == head_)
            return 0;
        if (ref_is_distant(ref)) {
            ref->distant_refs++;
            gate_ = gate_->prev;
        } else if (ref == gate_) {
            // move gate_ if ref is the gate_.
            gate_ = gate_->prev;
        }
        if (ref == tail_) {
            tail_ = tail_->prev;
        }

        // Compute reuse distance.
        int64_t dist = 0;
        line_ref_t *skip;
        for (skip = ref; skip != NULL && skip->depth == -1; skip = skip->prev)
            ++dist;
        if (skip != NULL)
            dist += skip->depth;
        else
            --dist; // Don't count self.

        IF_DEBUG_VERBOSE(
            0, if (verify_skip_) {
                // Compute reuse distance with a full list walk as a sanity check.
                // This is a debug-only option, so we guard with IF_DEBUG_VERBOSE(0).
                // Yes, the option check branch shows noticeable overhead without it.
                int64_t brute_dist = 0;
                for (prev = head_; prev != ref; prev = prev->next)
                    ++brute_dist;
                if (brute_dist != dist) {
                    std::cerr << "Mismatch!  Brute=" << std::dec << brute_dist
                              << " vs skip=" << dist << "\n";
                    print_list();
                    assert(false);
                }
            });

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
        ref->next = head_;
        head_->prev = ref;
        head_ = ref;
        head_->time_stamp = cur_time_++;

        IF_DEBUG_VERBOSE(3, print_list());
        // XXX: we should keep a running mean of the distance, and adjust
        // knob_reuse_skip_dist to stay close to the mean, for best performance.
        return dist;
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _REUSE_DISTANCE_H_ */
