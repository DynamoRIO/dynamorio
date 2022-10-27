/* **********************************************************
 * Copyright (c) 2022 RIVOS, Inc. All rights reserved.
 * **********************************************************/

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef TRIE_H
#define TRIE_H 1

#include <stdint.h>

/* Trie for matching bit patterns.
 *
 * This file contains structures and functions for operating on an array-based prefix tree
 * used for decoding instruction words into indices into a target array (i.e. of
 * instr_info_t structures).
 *
 * 1. Trie array construction
 *
 * The trie used by trie_lookup() is implemented as a Patricia tree (see [1]) in form of
 * an array with nodes laid out in a Breadth First Search manner - all nodes of a single
 * level are located before nodes of a next level. Moreover all children of a particular
 * node are laid out continuously before children of a next node:
 *
 * | N0 | N1 | ... | Nn | C0(N0) | ... | Cn(N0) | C0(N1) | ... |
 *
 * Each non-leaf node (mask != 0) represents a range of bits shared among instructions
 * belonging to that tree branch.
 *
 * Each leaf node (mask == 0) contains an index into the target array or TRIE_NODE_EMPTY
 * if it is a no-match node.
 *
 * 2. Lookup procedure
 *
 * Each node specifies a right shift amount and a mask to apply to the looked-up
 * instruction word in order to create an offset into the list of child nodes:
 *
 *   offset = (inst >> node.shift) & node.mask
 *
 * node->index is an index into the trie array where child nodes of this node are located.
 * The child node is determined by adding offset to node->index.
 *
 * This procedure is repeated until a leaf node (mask == 0) is reached.
 *
 * 3. Trie array creation
 *
 * The procedure for creating a trie array is implemented codec.py in
 * IslGenerator::construct_trie() but is specific to RISC-V ISL files.
 *
 * [1] https://en.wikipedia.org/wiki/Trie#Patricia_trees
 */

/* A prefix-tree node. */
typedef struct {
    byte mask;      /* The mask to apply to an instruction after applying shift. */
    byte shift;     /* The shift to apply to an instruction before applying mask. */
    uint16_t index; /* The index into the trie table. If mask == 0, index is the index
                       into instr_infos. */
} trie_node_t;

#define TRIE_NODE_EMPTY (uint16_t)(-1)
#define TRIE_LEAF_NODE(n) ((n)->mask == 0)
#define TRIE_CHILD_OFFSET(n, i) (((i) >> (n)->shift) & (n)->mask)

/* Lookup the instruction word in the trie.
 *
 * The lookup starts at the index `start` in the trie. This is so that some
 * parts of the lookup may be skipped.
 *
 * @param[in] trie The trie array to lookup in.
 * @param[in] word The instruction word to look up.
 * @param[in] start The trie array index to start the lookup at.
 *
 * @return trie_node_t::index value or TRIE_NODE_EMPTY on no match.
 */
static inline size_t
trie_lookup(trie_node_t trie[], uint32_t word, size_t start)
{
    trie_node_t *node;
    size_t index = start;

    while (index < TRIE_NODE_EMPTY) {
        node = &trie[index];
        if (TRIE_LEAF_NODE(node))
            return node->index;
        index = node->index + TRIE_CHILD_OFFSET(node, word);
    }
    return TRIE_NODE_EMPTY;
}

#endif /* TRIE_H */
