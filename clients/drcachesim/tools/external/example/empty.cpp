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

/* External analysis tool example. */

#include "dr_api.h"
#include "empty.h"

const std::string empty_t::TOOL_NAME = "Empty tool";

analysis_tool_t *
empty_tool_create(unsigned int verbose)
{
    return new empty_t(verbose);
}

empty_t::empty_t(unsigned int verbose)
{
    fprintf(stderr, "Empty tool created\n");
}

std::string
empty_t::initialize()
{
    return std::string("");
}

empty_t::~empty_t()
{
}

bool
empty_t::parallel_shard_supported()
{
    return true;
}

void *
empty_t::parallel_worker_init(int worker_index)
{
    return NULL;
}

std::string
empty_t::parallel_worker_exit(void *worker_data)
{
    return std::string("");
}

void *
empty_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return NULL;
}

bool
empty_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

bool
empty_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    return true;
}

std::string
empty_t::parallel_shard_error(void *shard_data)
{
    return std::string("");
}

bool
empty_t::process_memref(const memref_t &memref)
{
    return true;
}

bool
empty_t::print_results()
{
    fprintf(stderr, "Empty tool results:\n");
    return true;
}
