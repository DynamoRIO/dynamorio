/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

// On some platforms, like MacOS, a thread id is 64 bits.
// We just make both 64 bits to cover all our bases.
typedef int_least64_t memref_pid_t; /**< Process id type. */
typedef int_least64_t memref_tid_t; /**< Thread id type. */

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
 */
typedef union _memref_t {
    // The C standard allows us to reference the type field of any of these, and the
    // addr and size fields of data, instr, or flush generically if known to be one
    // of those types, due to the shared fields in our union of structs.
    struct _memref_data_t data;        /**< A data load or store. */
    struct _memref_instr_t instr;      /**< An insruction fetch. */
    struct _memref_flush_t flush;      /**< A software-initiated cache flush. */
    struct _memref_thread_exit_t exit; /**< A thread exit. */
    struct _memref_marker_t marker;    /**< A marker holding metadata. */
} memref_t;

#endif /* _MEMREF_H_ */
