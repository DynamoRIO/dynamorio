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

/* simulator: represent a simulator of a set of caching devices.
 */

#ifndef _SIMULATOR_H_
#define _SIMULATOR_H_ 1

#include <map>
#include "../analyzer.h"
#include "caching_device_stats.h"
#include "caching_device.h"
#include "../reader/ipc_reader.h"

class simulator_t : public analyzer_t
{
 public:
    simulator_t() {}
    virtual bool init() = 0;
    virtual ~simulator_t() = 0;
    virtual bool run() = 0;
    virtual bool print_stats() = 0;

 protected:
    virtual int core_for_thread(memref_tid_t tid);
    virtual void handle_thread_exit(memref_tid_t tid);

    int num_cores;

    ipc_reader_t ipc_end;
    ipc_reader_t ipc_iter;

    // For thread mapping to cores:
    std::map<memref_tid_t, int> thread2core;
    unsigned int *thread_counts;
    unsigned int *thread_ever_counts;
};

#endif /* _SIMULATOR_H_ */
