/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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
#define _REUSE_DISTANCE_H_

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

struct line_ref_node_t;
struct line_ref_splay_t;

class reuse_distance_t : public analysis_tool_t {
public:
    explicit reuse_distance_t(const reuse_distance_knobs_t &knobs);
    ~reuse_distance_t() override;
    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    std::string
    initialize_shard_type(shard_type_t shard_type) override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *stream) override;
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
        shard_data_t(uint64_t reuse_threshold, unsigned int distance_limit);
        std::unordered_map<addr_t, line_ref_node_t *> cache_map;
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
        std::unique_ptr<line_ref_splay_t> ref_list;
        int64_t total_refs = 0;
        int64_t data_refs = 0; // Non-instruction reference count.
        memref_tid_t tid = 0;  // For SHARD_BY_THREAD.
        int64_t core = 0;      // For SHARD_BY_CORE.
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
    std::unordered_map<int, shard_data_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    shard_type_t shard_type_ = SHARD_BY_THREAD;
    memtrace_stream_t *serial_stream_ = nullptr;
};

/* A splay tree node for the cache line reference info */
struct line_ref_node_t {
    struct line_ref_node_t *left;   // the left child of the node
    struct line_ref_node_t *right;  // the right child of the node
    struct line_ref_node_t *parent; // the parent of the node
    uint64_t size;                  // Size of the subtree
    uint64_t time_stamp;            // the most recent reference time stamp on this line
    uint64_t total_refs;            // the total number of references on this line
    uint64_t distant_refs;          // the total number of distant references on this line
    addr_t tag;

    line_ref_node_t(addr_t val)
        : left(nullptr)
        , right(nullptr)
        , parent(nullptr)
        , size(1)
        , total_refs(1)
        , distant_refs(0)
        , tag(val)
    {
    }
};

// We use a splay tree to keep track of the cache line reuse distance.
// The leftmost node of the splay tree is the most recently accessed cache line.
// The earlier the cache line was last accessed, the more to the right this cache line
// is in the splay tree.
// If a cache line is accessed, its time stamp is set as current, and it is
// added/moved to the left of the splay tree.  The cache line reference
// reuse distance is the cache line position in the splay tree before moving.
// We also keep a pointer (gate) pointing to the earliest cache
// line referenced within the threshold.  Thus, we can quickly check
// whether a cache line is recently accessed by comparing the time
// stamp of the referenced cache line and the gate cache line.
// The splay tree is a binary search tree that uses the splay operation for balancing,
// which allows deleting and inserting an element at any position in O(log n) amortized
// time.
struct line_ref_splay_t {
    line_ref_node_t *root_; // root of the splay
    line_ref_node_t *gate_; // the earliest cache line refs within the threshold
    line_ref_node_t *head_; // the most recently accessed cache line
    line_ref_node_t *tail_; // the least recently accessed cache line
    uint64_t cur_time_;     // current time stamp
    uint64_t unique_lines_; // the total number of unique cache lines accessed
    uint64_t threshold_;    // the reuse distance threshold

    line_ref_splay_t(uint64_t reuse_threshold_)
        : root_(nullptr)
        , gate_(nullptr)
        , head_(nullptr)
        , tail_(nullptr)
        , cur_time_(0)
        , unique_lines_(0)
        , threshold_(reuse_threshold_)
    {
    }

    virtual ~line_ref_splay_t()
    {
        line_ref_node_t *current = root_;
        while (current != nullptr) {
            if (current->left != nullptr) {
                current = current->left;
            } else if (current->right != nullptr) {
                current = current->right;
            } else {
                line_ref_node_t *parent = current->parent;
                if (parent != nullptr) {
                    if (parent->left == current) {
                        parent->left = nullptr;
                    } else {
                        parent->right = nullptr;
                    }
                }
                delete current;
                current = parent;
            }
        }
    }

    bool
    ref_is_distant(line_ref_node_t *ref)
    {
        if (gate_ == NULL || ref->time_stamp >= gate_->time_stamp)
            return false;
        return true;
    }

    void
    print_node(line_ref_node_t *node)
    {
        assert(node != nullptr);
        std::cerr << "\tTag 0x" << std::hex << node->tag << " size=" << std::dec
                  << node->size << " parent=" << std::hex
                  << (node->parent == nullptr ? 0 : node->parent->tag)
                  << " left=" << std::hex << (node->left == nullptr ? 0 : node->left->tag)
                  << " right=" << std::hex
                  << (node->right == nullptr ? 0 : node->right->tag);
    }

    // Print splay tree in the order of traversal.
    void
    print_list()
    {
        std::cerr << "Reuse tag list:\n";
        line_ref_node_t *node = root_;
        // the last visited node
        line_ref_node_t *last = nullptr;

        while (node != nullptr) {
            assert(node->parent != nullptr || node == root_);
            assert(get_size(node->left) + get_size(node->right) + 1 == node->size);

            if (node->left != nullptr && last != nullptr) {
                node = node->left;
                last = nullptr;
            } else if (node->right != nullptr && last == node->left) {
                print_node(node);
                node = node->right;
                last = nullptr;
            } else {
                if (node->right == nullptr) {
                    print_node(node);
                }
                last = node;
                node = node->parent;
            }
        }
    }

    // Add a new cache line to the front of the splay tree.
    // We may need to move gate_ forward if there are more cache lines
    // than the threshold so that the gate points to the earliest
    // referenced cache line within the threshold.
    void
    add_to_front(line_ref_node_t *ref)
    {
        push_front(ref);
        if (gate_ == nullptr) {
            gate_ = ref;
        }
        if (tail_ == nullptr) {
            tail_ = ref;
        }
        // move gate_ forward if necessary
        if (unique_lines_ > threshold_) {
            gate_ = get_prev(gate_);
        }

        unique_lines_++;
        ref->time_stamp = cur_time_++;
        IF_DEBUG_VERBOSE(3, print_list());
    }

    // Remove the last entry from the distance tree.
    void
    prune_tail()
    {
        // Make sure the tail pointers are legal.
        assert(tail_ != nullptr);
        assert(tail_ != head_);

        // Get new tail.
        line_ref_node_t *new_tail = get_prev(tail_);

        // Link the child of tail with the parent
        if (tail_->parent != nullptr) {
            tail_->parent->right = tail_->left;
            if (tail_->left != nullptr) {
                tail_->left->parent = tail_->parent;
            }
        }

        // Update sizes of all parents
        line_ref_node_t *parent = tail_->parent;
        while (parent != nullptr) {
            parent->size--;
            parent = parent->parent;
        }
        if (tail_ == gate_) {
            // move gate_ if tail_ was the gate_.
            gate_ = new_tail;
        }
        // And finally, update tail_.
        tail_ = new_tail;
    }

    // Move a referenced cache line to the front of the splay tree.
    // We need to move the gate_ pointer forward if the referenced cache
    // line is the gate_ cache line or any cache line after.
    // Returns the reuse distance of ref.
    int64_t
    move_to_front(line_ref_node_t *ref)
    {
        ref->total_refs++;
        if (ref == head_)
            return 0;
        splay(ref);
        // Get the reuse distance of ref.
        int64_t dist = get_size(ref->left);

        if (ref_is_distant(ref)) {
            ref->distant_refs++;
            gate_ = get_prev(gate_);
        } else if (ref == gate_) {
            // move gate_ if ref is the gate_.
            gate_ = get_prev(gate_);
        }
        if (ref == tail_) {
            tail_ = get_prev(tail_);
        }
        remove(ref);
        push_front(ref);
        ref->time_stamp = cur_time_++;
        IF_DEBUG_VERBOSE(3, print_list());
        return dist;
    }

    // Push node to the front of the splay tree.
    void
    push_front(line_ref_node_t *ref)
    {
        // Link ref to the front of the head.
        if (head_)
            head_->left = ref;
        else
            root_ = ref;
        ref->parent = head_;
        line_ref_node_t *parent = ref->parent;
        // Update sizes of parents
        while (parent != nullptr) {
            parent->size++;
            parent = parent->parent;
        }

        // Update head
        head_ = ref;
        splay(ref);
    }
    // Remove the node ref from the splay tree
    void
    remove(line_ref_node_t *ref)
    {
        splay(ref);
        // Replace ref with left child and link right child to the tail of left child
        if (ref->left != nullptr) {
            ref->left->parent = ref->parent;
            if (ref->right != nullptr) {
                line_ref_node_t *left_tail = get_tail(ref->left);
                ref->right->parent = left_tail;
                left_tail->right = ref->right;
                // Update sizes of the left child's subtrees
                do {
                    left_tail->size += ref->right->size;
                    left_tail = left_tail->parent;
                } while (left_tail != ref->left->parent);
            }
            if (root_ == ref) {
                root_ = ref->left;
            }
        } else {
            if (root_ == ref)
                root_ = ref->right;
            ref->right->parent = ref->parent;
        }
        // Update sizes of the ref's parents
        line_ref_node_t *parent = ref->parent;
        while (parent != nullptr) {
            parent->size--;
            parent = parent->parent;
        }
        // Clear ref
        ref->parent = ref->right = ref->left = nullptr;
        ref->size = 1;
    }

    // Find the tail of the splay tree.
    // Returns a pointer to the tail.
    line_ref_node_t *
    get_tail(line_ref_node_t *root)
    {
        if (root == nullptr) {
            return nullptr;
        }
        // Tail is the far right node in the tree
        while (root->right != nullptr) {
            root = root->right;
        }
        return root;
    }

    // Find previos element of ref in the splay tree.
    // Returns a pointer to the previos node.
    line_ref_node_t *
    get_prev(line_ref_node_t *ref)
    {
        if (ref == nullptr)
            return nullptr;
        // the last visited node
        line_ref_node_t *last = nullptr;
        // Walk up by tree wihle did not found a node to the left of the current
        while ((ref->left == nullptr || ref->left == last) && ref->parent != nullptr &&
               ref->parent->left == ref) {
            last = ref;
            ref = ref->parent;
        }

        // previos node is the far right node in left subtree
        if (ref->left != nullptr && ref->left != last) {
            return get_tail(ref->left);
        }

        // if the node is right child of another, the parent is the previos node
        if (ref->parent != nullptr && ref->parent->right == ref)
            return ref->parent;
        return nullptr;
    }

    // Get size of node
    // Returns size of node if node exist and 0 else
    uint64_t
    get_size(line_ref_node_t *node)
    {
        if (node != nullptr)
            return node->size;
        return 0;
    }

    // Recalculate size of node.
    void
    recalc_size(line_ref_node_t *node)
    {
        node->size = get_size(node->left) + get_size(node->right) + 1;
    }

    // Make left rotate of the subtree.
    void
    left_rotate(line_ref_node_t *node)
    {
        line_ref_node_t *new_parent = node->right;
        if (new_parent != nullptr) {
            node->right = new_parent->left;
            if (new_parent->left != nullptr)
                new_parent->left->parent = node;
            new_parent->parent = node->parent;
        }

        if (node->parent == nullptr)
            root_ = new_parent;
        else if (node == node->parent->left)
            node->parent->left = new_parent;
        else
            node->parent->right = new_parent;
        if (new_parent != nullptr)
            new_parent->left = node;
        node->parent = new_parent;
        recalc_size(node);
        recalc_size(new_parent);
    }

    // Make right rotate of the subtree.
    void
    right_rotate(line_ref_node_t *node)
    {
        line_ref_node_t *new_parent = node->left;
        if (new_parent != nullptr) {
            node->left = new_parent->right;
            if (new_parent->right != nullptr)
                new_parent->right->parent = node;
            new_parent->parent = node->parent;
        }
        if (node->parent == nullptr)
            root_ = new_parent;
        else if (node == node->parent->left)
            node->parent->left = new_parent;
        else
            node->parent->right = new_parent;
        if (new_parent != nullptr)
            new_parent->right = node;
        node->parent = new_parent;
        recalc_size(node);
        recalc_size(new_parent);
    }

    // Move the node to the root using rotate operations.
    void
    splay(line_ref_node_t *node)
    {
        while (node->parent != nullptr) {
            if (node->parent->parent == nullptr) {
                if (node->parent->left == node)
                    right_rotate(node->parent);
                else
                    left_rotate(node->parent);
            } else if (node->parent->left == node &&
                       node->parent->parent->left == node->parent) {
                right_rotate(node->parent->parent);
                right_rotate(node->parent);
            } else if (node->parent->right == node &&
                       node->parent->parent->right == node->parent) {
                left_rotate(node->parent->parent);
                left_rotate(node->parent);
            } else if (node->parent->left == node &&
                       node->parent->parent->right == node->parent) {
                right_rotate(node->parent);
                left_rotate(node->parent);
            } else {
                left_rotate(node->parent);
                right_rotate(node->parent);
            }
        }
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _REUSE_DISTANCE_H_ */
