/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

/* kernel_tracker.h: Utility to track kernel state based on trace records.
 */

#ifndef _KERNEL_TRACKER_H_
#define _KERNEL_TRACKER_H_

#include "trace_entry.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Utility class to track kernel state based on trace records.
 * Handles nesting of hardware events and separate tracking of syscalls
 * and context switches.
 *
 * XXX: Add handling to properly recognize kernel regions if we land in the
 * middle of one after a chunk skip, or if the trace itself starts in the middle of one.
 *
 * RecordType is the type of trace record to process (e.g., #memref_t or #trace_entry_t).
 */
template <typename RecordType> class kernel_tracker_tmpl_t {
public:
    /**
     * Returns whether the current point in the trace is inside a kernel region.
     * Kernel regions include system calls, context switches, and hardware events
     * such an interrupts.
     */
    bool
    in_kernel_trace() const
    {
        return in_syscall_ || in_context_switch_ || hardware_event_depth_ > 0 ||
            last_update_was_end_marker_;
    }

    /**
     * Updates the kernel tracking state based on the provided trace \p record.
     */
    void
    update(const RecordType &record)
    {
        update_internal(record);
    }

private:
    void
    update_for_marker(trace_marker_type_t marker_type)
    {
        switch (marker_type) {
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_START: in_syscall_ = true; break;
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START: in_context_switch_ = true; break;
        case TRACE_MARKER_TYPE_HARDWARE_EVENT: ++hardware_event_depth_; break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
            in_syscall_ = false;
            last_update_was_end_marker_ = true;
            break;
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
            in_context_switch_ = false;
            last_update_was_end_marker_ = true;
            break;
        case TRACE_MARKER_TYPE_HARDWARE_CONTEXT_RETURN:
            // OFFLINE_FILE_TYPE_WHOLE_SYSTEM traces are expected to have matching pairs
            // of hardware_event and hardware_context_return markers.
            --hardware_event_depth_;
            if (hardware_event_depth_ == 0)
                last_update_was_end_marker_ = true;
            break;
        default: break;
        }
    }

    void
    update_internal(const memref_t &record)
    {
        last_update_was_end_marker_ = false;
        if (record.marker.type == TRACE_TYPE_MARKER) {
            update_for_marker(
                static_cast<trace_marker_type_t>(record.marker.marker_type));
        }
    }

    void
    update_internal(const trace_entry_t &record)
    {
        last_update_was_end_marker_ = false;
        if (record.type == TRACE_TYPE_MARKER) {
            update_for_marker(static_cast<trace_marker_type_t>(record.size));
        }
    }

    bool in_syscall_ = false;
    bool in_context_switch_ = false;
    int hardware_event_depth_ = 0;
    bool last_update_was_end_marker_ = false;
};

/**
 * Instantiation of #kernel_tracker_tmpl_t for #memref_t records.
 */
using kernel_tracker_t = kernel_tracker_tmpl_t<memref_t>;

/**
 * Instantiation of #kernel_tracker_tmpl_t for #trace_entry_t records.
 */
using kernel_record_tracker_t = kernel_tracker_tmpl_t<trace_entry_t>;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _KERNEL_TRACKER_H_ */
