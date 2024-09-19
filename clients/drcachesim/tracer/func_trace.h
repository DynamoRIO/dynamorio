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

// func_trace.h: header of module for recording function traces

#ifndef _FUNC_TRACE_
#define _FUNC_TRACE_ 1

#include <stddef.h>
#include <sys/types.h>

#include <cstdint>

#include "dr_api.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#define MAX_FUNC_TRACE_ENTRY_VEC_CAP 16

struct func_trace_entry_t {
    func_trace_entry_t()
    {
    }
    func_trace_entry_t(trace_marker_type_t type, uintptr_t value)
        : marker_type(type)
        , marker_value(value)
    {
    }
    trace_marker_type_t marker_type;
    uintptr_t marker_value;
};

// XXX: replace it with a drvector_t. But note that we need to care about the overhead
// when doing so, since the existence of this vector is to reduce the overhead to
// under a threshold for some large application.
struct func_trace_entry_vector_t {
    int size;
    func_trace_entry_t entries[MAX_FUNC_TRACE_ENTRY_VEC_CAP];
};

typedef void (*func_trace_append_entry_vec_t)(void *, func_trace_entry_vector_t *);

// Initializes the func_trace module. Each call must be paired with a
// corresponding call to func_trace_exit().
bool
func_trace_init(func_trace_append_entry_vec_t append_entry_vec_,
                ssize_t (*write_file)(file_t file, const void *data, size_t count),
                file_t funclist_file);

// Cleans up the func_trace module
void
func_trace_exit();

// Needed for DRWRAP_INVERT_CONTROL.
dr_emit_flags_t
func_trace_enabled_instrument_event(void *drcontext, void *tag, instrlist_t *bb,
                                    instr_t *instr, instr_t *where, bool for_trace,
                                    bool translating, void *user_data);
dr_emit_flags_t
func_trace_disabled_instrument_event(void *drcontext, void *tag, instrlist_t *bb,
                                     instr_t *instr, instr_t *where, bool for_trace,
                                     bool translating, void *user_data);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _FUNC_TRACE_ */
