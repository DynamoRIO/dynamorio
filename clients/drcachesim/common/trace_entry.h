/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

/* This is the binary data format for what we send through IPC between the
 * memory tracing clients running inside the application(s) and the simulator
 * process.
 * We aren't bothering to pack it as it won't be over the network or persisted.
 * It's already arranged to minimize padding.
 * We do save space using heterogenous data via the type field to send
 * thread id data only periodically rather than paying for the cost of a
 * thread id field in every entry.
 */

#ifndef _TRACE_ENTRY_H_
#define _TRACE_ENTRY_H_ 1

#include <stdint.h>
#include "utils.h"

typedef uintptr_t addr_t;

#define TRACE_ENTRY_VERSION 1

// The type identifier for trace entries in the raw trace_entry_t passed to
// reader_t and the exposed memref_t passed to analysis tools.
// XXX: if we want to rely on a recent C++ standard we could try to get
// this enum to be just 2 bytes instead of an int and give it qualified names.
// N.B.: when adding new values, be sure to update trace_type_names[].
typedef enum {
    // XXX: if we want to include OP_ opcode values, we should have them occupy
    // the first 1,000 or so values and use OP_AFTER_LAST for the first of the
    // values below.  That would require including dr_api.h though.  For now we
    // assume we don't need the opcode.

    // These entries describe a memory reference as data:
    TRACE_TYPE_READ,
    TRACE_TYPE_WRITE,

    // General prefetch to L1 data cache
    TRACE_TYPE_PREFETCH,
    // X86 specific prefetch
    TRACE_TYPE_PREFETCHT0,
    TRACE_TYPE_PREFETCHT1,
    TRACE_TYPE_PREFETCHT2,
    TRACE_TYPE_PREFETCHNTA,
    // ARM specific prefetch
    TRACE_TYPE_PREFETCH_READ,
    TRACE_TYPE_PREFETCH_WRITE,
    TRACE_TYPE_PREFETCH_INSTR,

    // These entries describe an instruction fetch memory reference.
    // The trace_entry_t stream always has the instr fetch prior to data refs,
    // which the reader can use to obtain the PC for data references.
    // For memref_t, the instruction address is in the addr field.
    // An instruction *not* of the types below:
    TRACE_TYPE_INSTR,
    // Particular categories of instructions:
    TRACE_TYPE_INSTR_DIRECT_JUMP,
    TRACE_TYPE_INSTR_INDIRECT_JUMP,
    TRACE_TYPE_INSTR_CONDITIONAL_JUMP,
    TRACE_TYPE_INSTR_DIRECT_CALL,
    TRACE_TYPE_INSTR_INDIRECT_CALL,
    TRACE_TYPE_INSTR_RETURN,
    // These entries describe a bundle of consecutive instruction fetch
    // memory references.  The trace stream always has a single instr fetch
    // prior to instr bundles which the reader can use to obtain the starting PC.
    // This entry type is hidden by reader_t and expanded into a series of
    // TRACE_TYPE_INSTR* entries for memref_t.
    TRACE_TYPE_INSTR_BUNDLE,

    // A cache flush:
    // On ARM, a flush is requested via a SYS_cacheflush system call,
    // and the flush size could be larger than USHRT_MAX.
    // If the size is smaller than USHRT_MAX, we use one entry with non-zero size.
    // Otherwise, we use two entries, one entry has type TRACE_TYPE_*_FLUSH for
    // the start address of flush, and one entry has type TRACE_TYPE_*_FLUSH_END
    // for the end address (exclusive) of flush.
    // The size field of both entries should be 0.
    // The _END entries are hidden by reader_t as memref_t has space for the size.
    TRACE_TYPE_INSTR_FLUSH,
    TRACE_TYPE_INSTR_FLUSH_END,
    TRACE_TYPE_DATA_FLUSH,
    TRACE_TYPE_DATA_FLUSH_END,

    // These entries indicate that all subsequent memory references (until the
    // next entry of this type) came from the thread whose id is in the addr
    // field.
    // These entries are hidden by reader_t and turned into memref_t.tid.
    TRACE_TYPE_THREAD,

    // This entry indicates that the thread whose id is in the addr field exited:
    TRACE_TYPE_THREAD_EXIT,

    // These entries indicate which process the current thread belongs to.
    // The process id is in the addr field.
    // These entries are hidden by reader_t and turned into memref_t.pid.
    TRACE_TYPE_PID,

    // The initial entry in an offline file.  It stores the version (should
    // match TRACE_ENTRY_VERSION) in the addr field.  Unused for pipes.
    TRACE_TYPE_HEADER,

    // The final entry in an offline file or a pipe.
    TRACE_TYPE_FOOTER,
} trace_type_t;

extern const char * const trace_type_names[];

static inline bool
type_is_instr(const trace_type_t type)
{
    return (type >= TRACE_TYPE_INSTR && type <= TRACE_TYPE_INSTR_BUNDLE);
}

static inline bool
type_is_prefetch(const trace_type_t type)
{
    return (type >= TRACE_TYPE_PREFETCH && type <= TRACE_TYPE_PREFETCH_INSTR);
}

// This is the data format generated by the online tracer.
// The reader_t class transforms this into memref_t before handing to analysis tools.
// Each trace entry is a <type, size, addr> tuple representing:
// - a memory reference
// - an instr fetch
// - a bundle of instrs
// - a flush request
// - a prefetch request
// - a thread/process
START_PACKED_STRUCTURE
struct _trace_entry_t {
    unsigned short type; // 2 bytes: trace_type_t
    // 2 bytes: mem ref size, instr length, or num of instrs for instr bundle
    unsigned short size;
    union {
        addr_t addr;     // 4/8 bytes: mem ref addr, instr pc, tid, pid
        // The length of each instr in the instr bundle
        unsigned char length[sizeof(addr_t)];
    };
} END_PACKED_STRUCTURE;
typedef struct _trace_entry_t trace_entry_t;

///////////////////////////////////////////////////////////////////////////
//
// Offline trace format

// For offline traces, the tracing overhead is no longer overshadowed by online
// simulation.  Consequently, we aggressively shrink the tracer's trace entries,
// reconstructing the trace_entry_t format that the readers expect via a
// post-processing step before feeding it to analysis tools.

// We target 64-bit addresses and do not bother to shrink the module or timestamp
// entries for 32-bit apps.
// We assume that a 64-bit address has far fewer real bits, typically
// 48 bits, and that the top bits 48..63 are always identical.  Thus we can store
// a type field in those top bits.
// For the most common, a memref, we have both all 0's and all 1's be its
// type to reduce instrumentation overhead.
// The type simply identifies which union alternative:
typedef enum {
    OFFLINE_TYPE_MEMREF, // We rely on this being 0.
    OFFLINE_TYPE_PC,
    OFFLINE_TYPE_THREAD,
    OFFLINE_TYPE_PID,
    OFFLINE_TYPE_TIMESTAMP,
    // An ARM SYS_cacheflush: always has two addr entries for [start, end).
    OFFLINE_TYPE_IFLUSH,
    // The ext field identifies this further.
    OFFLINE_TYPE_EXTENDED,
    OFFLINE_TYPE_MEMREF_HIGH = 7,
} offline_type_t;

// Sub-type when the primary type is OFFLINE_TYPE_EXTENDED.
// These differ in what they store in offline_entry_t.extended.value.
typedef enum {
    // The initial entry in the file.  The value field holds the version.
    OFFLINE_EXT_TYPE_HEADER,
    // The final entry in the file.  The value field is 0.
    OFFLINE_EXT_TYPE_FOOTER,
} offline_ext_type_t;

#define OFFLINE_FILE_VERSION 1

START_PACKED_STRUCTURE
struct _offline_entry_t {
    union {
        // Unfortunately the compiler won't combine bitfields across the union border
        // so we have to duplicate the type field in each alternative.
        struct {
            uint64_t addr:61;
            uint64_t type:3;
        } addr;
        struct {
            // This describes the entire basic block.
            uint64_t modoffs:33;
            uint64_t modidx:12;
            uint64_t instr_count:16;
            uint64_t type:3;
        } pc;
        struct {
            uint64_t tid:61;
            uint64_t type:3;
        } tid;
        struct {
            uint64_t pid:61;
            uint64_t type:3;
        } pid;
        struct {
            uint64_t usec:61; // Microseconds since Jan 1, 1601.
            uint64_t type:3;
        } timestamp;
        struct {
            uint64_t value:56; // Meaning is specific to ext type.
            uint64_t ext:5;    // Holds an offline_ext_type_t value.
            uint64_t type:3;
        } extended;
        uint64_t combined_value;
        // XXX: add a CPU id entry for more faithful thread scheduling.
    };
} END_PACKED_STRUCTURE;
typedef struct _offline_entry_t offline_entry_t;

#endif /* _TRACE_ENTRY_H_ */
