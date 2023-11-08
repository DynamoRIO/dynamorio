/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* A priority queue with constant-time search and removal from the middle. */

#ifndef _DRMEMTRACE_FLEXIBLE_QUEUE_H_
#define _DRMEMTRACE_FLEXIBLE_QUEUE_H_ 1

/**
 * @file drmemtrace/flexible_queue.h
 * @brief DrMemtrace flexible priority queue.
 */

#define NOMINMAX // Avoid windows.h messing up std::max.
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

namespace dynamorio {  /**< General DynamoRIO namespace. */
namespace drmemtrace { /**< DrMemtrace tracing + simulation infrastructure namespace. */

/**
 * A priority queue with constant-time search and removal from the middle.
 * The type T must support the << operator.
 * We follow std::priority_queue convention where comparator_t(a,b) returning true
 * means that a is lower priority (worse) than b.
 */
template <typename T, class comparator_t = std::less<T>, class hash_t = std::hash<T>>
class flexible_queue_t {
public:
    typedef typename std::vector<T>::size_type index_t;
    // Wrap max in parens to work around Visual Studio compiler issues with the
    // max macro (even despite NOMINMAX defined above).
    static constexpr index_t INVALID_INDEX = (std::numeric_limits<index_t>::max)();

    flexible_queue_t() = default;
    explicit flexible_queue_t(int verbose)
        : verbose_(verbose)
    {
    }
    bool
    push(T entry)
    {
        if (entry2index_.find(entry) != entry2index_.end())
            return false; // Duplicates not allowed.
        entries_.push_back(entry);
        index_t node = entries_.size() - 1;
        entry2index_[entry] = node;
        percolate_up(node);
        vprint(1, "after push");
        return true;
    }

    void
    pop()
    {
        erase(top());
    }

    T
    top() const
    {
        return entries_[0]; // Undefined if empty.
    }

    bool
    empty() const
    {
        return entries_.empty();
    }

    size_t
    size() const
    {
        return entries_.size();
    }

    bool
    find(T entry)
    {
        auto it = entry2index_.find(entry);
        if (it == entry2index_.end())
            return false;
        return true;
    }

    bool
    erase(T entry)
    {
        auto it = entry2index_.find(entry);
        if (it == entry2index_.end())
            return false;
        index_t node = it->second;
        if (node == entries_.size() - 1) {
            entry2index_.erase(entry);
            entries_.pop_back();
            return true;
        }
        swap(node, entries_.size() - 1);
        entry2index_.erase(entry);
        entries_.pop_back();
        percolate_down(node);
        percolate_up(node);
        vprint(1, "after erase");
        return true;
    }

private:
    void
    vprint(int verbose_threshold, const std::string &message)
    {
        if (verbose_ < verbose_threshold)
            return;
        std::cout << message << "\n";
        print();
    }

    void
    print()
    {
        for (index_t i = 0; i < entries_.size(); ++i) {
            std::cout << i << ": " << entries_[i] << " @ " << entry2index_[entries_[i]]
                      << "\n";
        }
    }

    inline index_t
    parent_node(index_t node)
    {
        if (node <= 0)
            return INVALID_INDEX;
        return node / 2;
    }
    inline index_t
    left_child(index_t node)
    {
        index_t child = node * 2;
        if (child >= entries_.size())
            return INVALID_INDEX;
        return child;
    }
    inline index_t
    right_child(index_t node)
    {
        index_t child = node * 2 + 1;
        if (child >= entries_.size())
            return INVALID_INDEX;
        return child;
    }
    void
    swap(index_t a, index_t b)
    {
        T temp = entries_[a];
        entries_[a] = entries_[b];
        entries_[b] = temp;
        entry2index_[temp] = b;
        entry2index_[entries_[a]] = a;
    }
    void
    percolate_down(index_t node)
    {
        index_t should_be_parent = node;
        index_t left = left_child(node);
        index_t right = right_child(node);
        if (left != INVALID_INDEX && !compare_(entries_[left], entries_[node]))
            should_be_parent = left;
        if (right != INVALID_INDEX && !compare_(entries_[right], entries_[node])) {
            if (should_be_parent != left || !compare_(entries_[right], entries_[left]))
                should_be_parent = right;
        }
        if (should_be_parent == node)
            return;
        swap(node, should_be_parent);
        percolate_down(should_be_parent);
    }

    void
    percolate_up(index_t node)
    {
        index_t parent = parent_node(node);
        if (parent == INVALID_INDEX || !compare_(entries_[parent], entries_[node]))
            return;
        swap(node, parent);
        percolate_up(parent);
    }

    std::vector<T> entries_;
    // We follow std::priority_queue convention where compare_(a,b) returning true
    // means that a is lower priority (worse) than b.
    comparator_t compare_;
    std::unordered_map<T, index_t, hash_t> entry2index_;
    int verbose_ = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_FLEXIBLE_QUEUE_H_ */
