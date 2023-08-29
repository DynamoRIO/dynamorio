/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
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

#ifndef _OUTPUT_
#define _OUTPUT_ 1

#include "dr_api.h"
#include "tracer.h"

namespace dynamorio {
namespace drmemtrace {

void
open_new_window_dir(ptr_int_t window_num);

int
append_unit_header(void *drcontext, byte *buf_ptr, thread_id_t tid, ptr_int_t window);

void
process_and_output_buffer(void *drcontext, bool skip_size_cap);

void
init_thread_io(void *drcontext);

void
exit_thread_io(void *drcontext);

void
init_buffers(per_thread_t *data);

void
init_io();

void
exit_io();

// Returns true for an empty new (non-initial) buffer for a tracing window
// with no instructions traced yet in the window.
inline bool
is_new_window_buffer_empty(per_thread_t *data)
{
    // Since it's non-initial we do not add init_header_size.
    return BUF_PTR(data->seg_base) == data->buf_base + buf_hdr_slots_size &&
        data->cur_window_instr_count == 0;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _OUTPUT_ */
