/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* cache: represents a single hardware cache.
 */

#ifndef _CACHE_H_
#define _CACHE_H_ 1

#include "cache_line.h"
#include "cache_stats.h"
#include "memref.h"

// Statistics collection is abstracted out into the cache_stats_t class.

// Different replacement policies are expected to be implemented by
// subclassing cache_t.

// We assume we're only invoked from a single thread of control and do
// not need to synchronize data access.

class cache_t
{
 public:
    cache_t();
    virtual bool init(int associativity, int line_size, int num_lines,
                      cache_t *parent, cache_stats_t *stats);
    virtual ~cache_t();
    virtual void request(const memref_t &memref);

 private:
    virtual void replace_update(int line_idx, int way);
    virtual int replace_which_way(int line_idx);

    int associativity;
    int line_size;
    int num_lines;
    cache_t *parent;
    cache_line_t *lines;
    int lines_per_set;
    cache_stats_t *stats;
};

#endif /* _CACHE_H_ */
