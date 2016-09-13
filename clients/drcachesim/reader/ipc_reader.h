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

/* ipc_reader: obtains memory streams from DR clients running in
 * application processes and presents them via an interator interface
 * to the cache simulator.
 */

#ifndef _IPC_READER_H_
#define _IPC_READER_H_ 1

#include <list>
#include <string>
#include <map>
#include "reader.h"
#include "../common/memref.h"
#include "../common/named_pipe.h"
#include "../common/trace_entry.h"

class ipc_reader_t : public reader_t
{
 public:
    ipc_reader_t();
    explicit ipc_reader_t(const char *ipc_name);
    virtual ~ipc_reader_t();
    bool init();
    virtual const memref_t& operator*();
    virtual bool operator==(const ipc_reader_t& rhs);
    virtual bool operator!=(const ipc_reader_t& rhs);
    virtual reader_t operator++(int);
    virtual reader_t& operator++();

    void stream_server();

 private:
    bool at_eof;
    named_pipe_t pipe;
    memref_t cur_ref;
    memref_tid_t cur_tid;
    memref_pid_t cur_pid;
    addr_t cur_pc;
    addr_t next_pc;
    int bundle_idx;
    std::map<memref_tid_t, memref_pid_t> tid2pid;

    // For efficiency we want to read large chunks at a time.
    // The atomic write size for a pipe on Linux is 4096 bytes but
    // we want to go ahead and read as much data as we can at one
    // time.
    static const int BUF_SIZE = 16*1024;
    trace_entry_t buf[BUF_SIZE];
    trace_entry_t *cur_buf;
    trace_entry_t *end_buf;
};

#endif /* _IPC_READER_H_ */
