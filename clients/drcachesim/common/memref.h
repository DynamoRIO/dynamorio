/* **********************************************************
 * Copyright (c) 2015-2025 Google, Inc.  All rights reserved.
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

/* This is the data format that the simulator and analysis tools take as input. */

#ifndef _MEMREF_H_
#define _MEMREF_H_ 1

#include <stdint.h>
#include <stddef.h> // for size_t
#include "trace_entry.h"

/**
 * @file drmemtrace/memref.h
 * @brief DrMemtrace trace entry structures.
 */

namespace dynamorio {  /**< General DynamoRIO namespace. */
namespace drmemtrace { /**< DrMemtrace tracing + simulation infrastructure namespace. */

// On some platforms, like MacOS, a thread id is 64 bits.
// We just make both 64 bits to cover all our bases.
/**
 * Process id type.  When multiple workloads are combined in one trace, a workload ordinal
 * is added to the top (64-MEMREF_ID_WORKLOAD_SHIFT) bits; the workload_from_memref_pid()
 * and pid_from_memref_pid() helpers can be used to separate the values if desired.
 */
typedef int64_t memref_pid_t;
/**
 * Thread id type.  When multiple workloads are combined in one trace, a workload ordinal
 * is added to the top (64-MEMREF_ID_WORKLOAD_SHIFT) bits; the workload_from_memref_tid()
 * and tid_from_memref_tid() helpers can be used to separate the values if desired.
 */
typedef int64_t memref_tid_t;

/** Constants related to tid and pid fields. */
enum {
    /**
     * When multiple workloads are combined in one trace, a workload ordinal is added to
     * the top (64-MEMREF_ID_WORKLOAD_SHIFT) bits of the pid and tid fields of #memref_t.
     * We use 48 to leave some room for >32-bit identifiers (Mac has a 64-bit tid type)
     * while still leaving plenty of room for the workload ordinal.
     */
    MEMREF_ID_WORKLOAD_SHIFT = 48,
};

/**
 * When multiple workloads are combined in one trace, a workload ordinal is added to the
 * top (64-MEMREF_ID_WORKLOAD_SHIFT) bits of the #memref_t pid field.  This function
 * extracts the workload ordinal.
 */
static inline int
workload_from_memref_pid(memref_tid_t pid)
{
    return pid >> MEMREF_ID_WORKLOAD_SHIFT;
}

/**
 * When multiple workloads are combined in one trace, a workload ordinal is added to the
 * top (64-MEMREF_ID_WORKLOAD_SHIFT) bits of the #memref_t tid field.  This function
 * extracts the workload ordinal.
 */
static inline int
workload_from_memref_tid(memref_tid_t tid)
{
    return tid >> MEMREF_ID_WORKLOAD_SHIFT;
}

/**
 * When multiple workloads are combined in one trace, a workload ordinal is added to the
 * top (64-MEMREF_ID_WORKLOAD_SHIFT) bits of the #memref_t pid field.  This function
 * extracts just the pid.
 */
static inline memref_pid_t
pid_from_memref_pid(memref_pid_t pid)
{
    return pid & ((1ULL << MEMREF_ID_WORKLOAD_SHIFT) - 1);
}

/**
 * When multiple workloads are combined in one trace, a workload ordinal is added to the
 * top (64-MEMREF_ID_WORKLOAD_SHIFT) bits of the #memref_t tid field.  This function
 * extracts just the tid.
 */
static inline memref_tid_t
tid_from_memref_tid(memref_tid_t tid)
{
    return tid & ((1ULL << MEMREF_ID_WORKLOAD_SHIFT) - 1);
}

/** A trace entry representing a data load, store, or prefetch. */
struct _memref_data_t {
    trace_type_t type; /**< #TRACE_TYPE_READ, #TRACE_TYPE_WRITE, or type_is_prefetch(). */
    memref_pid_t pid;  /**< Process id. */
    memref_tid_t tid;  /**< Thread id. */
    addr_t addr;       /**< Address of data being loaded or stored. */
    size_t size;       /**< Size of data being loaded or stored. */
    addr_t pc;         /**< Program counter of instruction performing load or store. */
};

/** A trace entry representing an instruction fetch. */
struct _memref_instr_t {
    trace_type_t type; /**< Matches type_is_instr() or #TRACE_TYPE_INSTR_NO_FETCH. */
    memref_pid_t pid;  /**< Process id. */
    memref_tid_t tid;  /**< Thread id. */
    addr_t addr;       /**< The address of the instruction (i.e., program counter). */
    size_t size;       /**< The length of the instruction. */
    /**
     * The instruction's raw encoding.  This field is only valid when the file type
     * (see #TRACE_MARKER_TYPE_FILETYPE) has #OFFLINE_FILE_TYPE_ENCODINGS set.
     * DynamoRIO's decode_from_copy() (or any other decoding library) can be used to
     * decode into a higher-level instruction representation.
     */
    unsigned char encoding[MAX_ENCODING_LENGTH];
    /**
     * Indicates whether the encoding field is the first instance of its kind for this
     * address.  This can be used to determine when to invalidate cached decoding
     * information.  This field may be set to true on internal file divisions and
     * not only when application code actually changed.
     */
    bool encoding_is_new;
    /**
     * Valid only for an indirect branch instruction (types
     * #TRACE_TYPE_INSTR_INDIRECT_JUMP, #TRACE_TYPE_INSTR_INDIRECT_CALL, and
     * #TRACE_TYPE_INSTR_RETURN).  Holds the actual target of that branch.  This is only
     * present in trace version #TRACE_ENTRY_VERSION_BRANCH_INFO and higher.
     */
    addr_t indirect_branch_target;
};

/** A trace entry representing a software-requested explicit cache flush. */
struct _memref_flush_t {
    trace_type_t type; /**< #TRACE_TYPE_INSTR_FLUSH or #TRACE_TYPE_DATA_FLUSH. */
    memref_pid_t pid;  /**< Process id. */
    memref_tid_t tid;  /**< Thread id. */
    addr_t addr;       /**< The start address of the region being flushed. */
    size_t size;       /**< The size of the region being flushed. */
    addr_t pc;         /**< Program counter of the instruction requesting the flush. */
};

/** A trace entry representing a thread exit. */
struct _memref_thread_exit_t {
    trace_type_t type; /**< #TRACE_TYPE_THREAD_EXIT. */
    memref_pid_t pid;  /**< Process id. */
    memref_tid_t tid;  /**< Thread id. */
};

/**
 * A trace entry containing metadata identifying some event that occurred at this
 * point in the trace.  Common markers include timestamp and cpu information for
 * certain points in the trace.  Another marker type represents a kernel-mediated
 * control flow change such as a signal delivery, entry into an APC, callback, or
 * exception dispatcher on Windows, or a system call that changes the context such as
 * a signal return.
 */
struct _memref_marker_t {
    trace_type_t type;               /**< #TRACE_TYPE_MARKER. */
    memref_pid_t pid;                /**< Process id. */
    memref_tid_t tid;                /**< Thread id. */
    trace_marker_type_t marker_type; /**< Identifies the type of marker. */
    uintptr_t marker_value; /**< A value whose meaning depends on the marker type. */
};

/**
 * To enable #memref_t to be default initialized reliably, a byte array is defined
 * with the same length as the largest member of the union.  A subsequent
 * static_assert makes sure the chosen size is truly the largest.
 */
constexpr int MEMREF_T_SIZE_BYTES = sizeof(_memref_instr_t);

/**
 * Each trace entry is one of the structures in this union.
 * Each entry identifies the originating process and thread.
 * Although the pc of each data reference is provided, the trace also guarantees that
 * an instruction entry immediately precedes the data references that it is
 * responsible for, with no intervening trace entries (unless it is a trace filtered
 * with an online first-level cache).
 * Offline traces further guarantee that an instruction entry for a branch
 * instruction is always followed by an instruction entry for the branch's
 * target (with any memory references for the branch in between of course)
 * without a thread switch intervening, to make it simpler to identify branch
 * targets (again, unless the trace is filtered by an online first-level cache).
 * Online traces do not currently guarantee this.
 *
 * Note that #memref_t is **not** initialized by default.  The _raw_bytes array
 * is added to the union as its first member to make sure a #memref_t object
 * can be fully initialized if desired, for example `memref_t memref = {};`.
 */
typedef union _memref_t {
    // The C standard allows us to reference the type field of any of these, and the
    // addr and size fields of data, instr, or flush generically if known to be one
    // of those types, due to the shared fields in our union of structs.
    // The _raw_bytes entry is for initialization purposes and must be first in
    // this list.  A byte array is used for initialization rather than an existing struct
    // to avoid incomplete initialization due to padding or alignment constraints within a
    // struct.  This array is not intended to be used for memref_t access.
    uint8_t _raw_bytes[MEMREF_T_SIZE_BYTES]; /**< Do not use: for init only. */
    struct _memref_data_t data;              /**< A data load or store. */
    struct _memref_instr_t instr;            /**< An instruction fetch. */
    struct _memref_flush_t flush;            /**< A software-initiated cache flush. */
    struct _memref_thread_exit_t exit;       /**< A thread exit. */
    struct _memref_marker_t marker;          /**< A marker holding metadata. */
} memref_t;

static inline bool
memref_has_pc(const memref_t &memref, uint64_t &pc)
{
    if (type_is_instr(memref.instr.type)) {
        pc = memref.instr.addr;
        return true;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) {
        pc = memref.marker.marker_value;
        return true;
    }
    return false;
}

static_assert(sizeof(memref_t) == MEMREF_T_SIZE_BYTES,
              "Update MEMREF_T_SIZE_BYTES to match sizeof(memref_t).  Did the largest "
              "union member change?");

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _MEMREF_H_ */
