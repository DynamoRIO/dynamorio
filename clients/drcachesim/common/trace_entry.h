/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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

/**
 * @file drmemtrace/trace_entry.h
 * @brief DrMemtrace trace entry enum types and definitions.
 */

typedef uintptr_t addr_t; /**< The type of a memory address. */

#define TRACE_ENTRY_VERSION 2 /**< The version of the trace format. */

/** The type of a trace entry in a #memref_t structure. */
// The type identifier for trace entries in the raw trace_entry_t passed to
// reader_t and the exposed #memref_t passed to analysis tools.
// XXX: if we want to rely on a recent C++ standard we could try to get
// this enum to be just 2 bytes instead of an int and give it qualified names.
// N.B.: when adding new values, be sure to update trace_type_names[].
typedef enum {
    // XXX: if we want to include OP_ opcode values, we should have them occupy
    // the first 1,000 or so values and use OP_AFTER_LAST for the first of the
    // values below.  That would require including dr_api.h though.  For now we
    // assume we don't need the opcode.

    // These entries describe a memory reference as data:
    TRACE_TYPE_READ,  /**< A data load. */
    TRACE_TYPE_WRITE, /**< A data store. */

    TRACE_TYPE_PREFETCH, /**< A general prefetch. */

    // X86 specific prefetch
    TRACE_TYPE_PREFETCHT0, /**< An x86 prefetch to all levels of the cache. */
    TRACE_TYPE_PREFETCH_READ_L1 =
        TRACE_TYPE_PREFETCHT0, /**< Load prefetch to L1 cache. */
    TRACE_TYPE_PREFETCHT1,     /**< An x86 prefetch to level 2 cache and higher. */
    TRACE_TYPE_PREFETCH_READ_L2 =
        TRACE_TYPE_PREFETCHT1, /**< Load prefetch to L2 cache. */
    TRACE_TYPE_PREFETCHT2,     /**< An x86 prefetch to level 3 cache and higher. */
    TRACE_TYPE_PREFETCH_READ_L3 =
        TRACE_TYPE_PREFETCHT2, /**< Load prefetch to L3 cache. */
    // This prefetches data into a non-temporal cache structure and into a location
    // close to the processor, minimizing cache pollution.
    TRACE_TYPE_PREFETCHNTA, /**< An x86 non-temporal prefetch. */

    // ARM specific prefetch
    TRACE_TYPE_PREFETCH_READ,  /**< An ARM load prefetch. */
    TRACE_TYPE_PREFETCH_WRITE, /**< An ARM store prefetch. */
    TRACE_TYPE_PREFETCH_INSTR, /**< An ARM insruction prefetch. */

    // These entries describe an instruction fetch memory reference.
    // The trace_entry_t stream always has the instr fetch prior to data refs,
    // which the reader can use to obtain the PC for data references.
    // For memref_t, the instruction address is in the addr field.
    // An instruction *not* of the types below:
    TRACE_TYPE_INSTR, /**< A non-branch instruction. */
    // Particular categories of instructions:
    TRACE_TYPE_INSTR_DIRECT_JUMP,      /**< A direct unconditional jump instruction. */
    TRACE_TYPE_INSTR_INDIRECT_JUMP,    /**< An indirect jump instruction. */
    TRACE_TYPE_INSTR_CONDITIONAL_JUMP, /**< A conditional jump instruction. */
    TRACE_TYPE_INSTR_DIRECT_CALL,      /**< A direct call instruction. */
    TRACE_TYPE_INSTR_INDIRECT_CALL,    /**< An indirect call instruction. */
    TRACE_TYPE_INSTR_RETURN,           /**< A return instruction. */
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
    TRACE_TYPE_INSTR_FLUSH, /**< An instruction cache flush. */
    TRACE_TYPE_INSTR_FLUSH_END,
    TRACE_TYPE_DATA_FLUSH, /**< A data cache flush. */
    TRACE_TYPE_DATA_FLUSH_END,

    // These entries indicate that all subsequent memory references (until the
    // next entry of this type) came from the thread whose id is in the addr
    // field.
    // These entries are hidden by reader_t and turned into memref_t.tid.
    TRACE_TYPE_THREAD,

    // This entry indicates that the thread whose id is in the addr field exited:
    TRACE_TYPE_THREAD_EXIT, /**< A thread exit. */

    // These entries indicate which process the current thread belongs to.
    // The process id is in the addr field.
    // These entries are hidden by reader_t and turned into memref_t.pid.
    TRACE_TYPE_PID,

    // The initial entry in an offline file.  It stores the version (should
    // match TRACE_ENTRY_VERSION) in the addr field.  Unused for pipes.
    TRACE_TYPE_HEADER,

    // The final entry in an offline file or a pipe.
    TRACE_TYPE_FOOTER,

    /** A hardware-issued prefetch (generated after tracing by a cache simulator). */
    TRACE_TYPE_HARDWARE_PREFETCH,

    /**
     * A marker containing metadata about this point in the trace.
     * It includes a marker sub-type #trace_marker_type_t and a value.
     */
    TRACE_TYPE_MARKER,

    /**
     * For core simulators, a trace includes instructions that do not incur
     * instruction cache fetches, such as on each subsequent iteration of a
     * rep string loop on x86.
     */
    TRACE_TYPE_INSTR_NO_FETCH,
    // An internal value used for online traces and turned by reader_t into
    // either TRACE_TYPE_INSTR or TRACE_TYPE_INSTR_NO_FETCH.
    TRACE_TYPE_INSTR_MAYBE_FETCH,

    /**
     * We separate out the x86 sysenter instruction as it has a hardcoded
     * return point that shows up as a discontinuity in the user mode program
     * counter execution sequence.
     */
    TRACE_TYPE_INSTR_SYSENTER,

    // Architecture-agnostic trace entry types for prefetch instructions.
    TRACE_TYPE_PREFETCH_READ_L1_NT, /**< Non-temporal load prefetch to L1 cache. */
    TRACE_TYPE_PREFETCH_READ_L2_NT, /**< Non-temporal load prefetch to L2 cache. */
    TRACE_TYPE_PREFETCH_READ_L3_NT, /**< Non-temporal load prefetch to L3 cache. */

    TRACE_TYPE_PREFETCH_INSTR_L1,    /**< Instr prefetch to L1 cache. */
    TRACE_TYPE_PREFETCH_INSTR_L1_NT, /**< Non-temporal instr prefetch to L1 cache. */
    TRACE_TYPE_PREFETCH_INSTR_L2,    /**< Instr prefetch to L2 cache. */
    TRACE_TYPE_PREFETCH_INSTR_L2_NT, /**< Non-temporal instr prefetch to L2 cache. */
    TRACE_TYPE_PREFETCH_INSTR_L3,    /**< Instr prefetch to L3 cache. */
    TRACE_TYPE_PREFETCH_INSTR_L3_NT, /**< Non-temporal instr prefetch to L3 cache. */

    TRACE_TYPE_PREFETCH_WRITE_L1,    /**< Store prefetch to L1 cache. */
    TRACE_TYPE_PREFETCH_WRITE_L1_NT, /**< Non-temporal store prefetch to L1 cache. */
    TRACE_TYPE_PREFETCH_WRITE_L2,    /**< Store prefetch to L2 cache. */
    TRACE_TYPE_PREFETCH_WRITE_L2_NT, /**< Non-temporal store prefetch to L2 cache. */
    TRACE_TYPE_PREFETCH_WRITE_L3,    /**< Store prefetch to L3 cache. */
    TRACE_TYPE_PREFETCH_WRITE_L3_NT, /**< Non-temporal store prefetch to L3 cache. */

    // Update trace_type_names[] when adding here.
} trace_type_t;

/** The sub-type for TRACE_TYPE_MARKER. */
typedef enum {
    /**
     * The subsequent instruction is the start of a handler for a kernel-initiated
     * event: a signal handler or restartable sequence abort handler on UNIX, or an
     * APC, exception, or callback dispatcher on Windows.
     * The value holds the module offset of the interruption point PC,
     * which is used in post-processing.  The value is 0 for some types, namely
     * Windows callbacks and Linux rseq aborts, but these can be assumed to target
     * the start of a block and so there is no loss of accuracy when post-processing.
     */
    TRACE_MARKER_TYPE_KERNEL_EVENT,
    /**
     * The subsequent instruction is the target of a system call that changes the
     * context: a signal return on UNIX, or a callback return or NtContinue or
     * NtSetContextThread on Windows.
     */
    TRACE_MARKER_TYPE_KERNEL_XFER,
    /**
     * The marker value contains a timestamp for this point in the trace, in units
     * of microseconds since Jan 1, 1601 (the UTC time).  For 32-bit, the value
     * is truncated to 32 bits.
     */
    TRACE_MARKER_TYPE_TIMESTAMP,
    /**
     * The marker value contains the cpu identifier of the cpu this thread was running
     * on at this point in the trace.  A value of (uintptr_t)-1 indicates that the
     * cpu could not be determined.
     */
    TRACE_MARKER_TYPE_CPU_ID,

    /**
     * The marker value contains the function id defined by the user in the
     * -record_function (and -record_heap_value if -record_heap is specified)
     * option.
     */
    TRACE_MARKER_TYPE_FUNC_ID,

    // XXX i#3048: replace return address with callstack information.
    /**
     * The marker value contains the return address of the just-entered
     * function, whose id is specified by the closest previous
     * #TRACE_MARKER_TYPE_FUNC_ID marker entry.
     */
    TRACE_MARKER_TYPE_FUNC_RETADDR,

    /**
     * The marker value contains one argument value of the just-entered
     * function, whose id is specified by the closest previous
     * #TRACE_MARKER_TYPE_FUNC_ID marker entry. The number of such entries
     * for one function invocation is equal to the specified argument in
     * -record_function (or pre-defined functions in -record_heap_value if
     * -record_heap is specified).
     */
    TRACE_MARKER_TYPE_FUNC_ARG,

    /**
     * The marker value contains the return value of the just-entered function,
     * whose id is specified by the closest previous #TRACE_MARKER_TYPE_FUNC_ID
     * marker entry
     */
    TRACE_MARKER_TYPE_FUNC_RETVAL,

    /* This is a non-public type only present in an offline raw trace. To support a
     * full 64-bit marker value in an offline trace where
     * offline_entry_t.extended.valueA contains <64 bits, we use two consecutive
     * entries.  We rely on these being adjacent in the trace.  This entry must come
     * first, and its valueA is left-shited 32 and then OR-ed with the subsequent
     * entry's valueA to produce the final marker value.
     */
    TRACE_MARKER_TYPE_SPLIT_VALUE,

    /**
     * The marker value contains the OFFLINE_FILE_TYPE_* bitfields of type
     * #offline_file_type_t identifying the architecture and other key high-level
     * attributes of the trace.
     */
    TRACE_MARKER_TYPE_FILETYPE,

    /**
     * The marker value contains the traced processor's cache line size in
     * bytes.
     */
    TRACE_MARKER_TYPE_CACHE_LINE_SIZE,

    // ...
    // These values are reserved for future built-in marker types.
    // ...
    TRACE_MARKER_TYPE_RESERVED_END = 100,
    // Values below here are available for users to use for custom markers.
} trace_marker_type_t;

extern const char *const trace_type_names[];

/**
 * Returns whether the type represents an instruction fetch.
 * Deliberately excludes TRACE_TYPE_INSTR_NO_FETCH and TRACE_TYPE_INSTR_BUNDLE.
 */
static inline bool
type_is_instr(const trace_type_t type)
{
    return (type >= TRACE_TYPE_INSTR && type <= TRACE_TYPE_INSTR_RETURN) ||
        type == TRACE_TYPE_INSTR_SYSENTER;
}

/** Returns whether the type represents the fetch of a branch instruction. */
static inline bool
type_is_instr_branch(const trace_type_t type)
{
    return (type >= TRACE_TYPE_INSTR_DIRECT_JUMP && type <= TRACE_TYPE_INSTR_RETURN);
}

/** Returns whether the type represents a prefetch request. */
static inline bool
type_is_prefetch(const trace_type_t type)
{
    return (type >= TRACE_TYPE_PREFETCH && type <= TRACE_TYPE_PREFETCH_INSTR) ||
        (type >= TRACE_TYPE_PREFETCH_READ_L1_NT &&
         type <= TRACE_TYPE_PREFETCH_WRITE_L3_NT) ||
        type == TRACE_TYPE_HARDWARE_PREFETCH;
}

// This is the data format generated by the online tracer and produced after
// post-processing of raw offline traces.
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
    // 2 bytes: mem ref size, instr length, or num of instrs for instr bundle,
    // or marker sub-type.
    unsigned short size;
    union {
        addr_t addr; // 4/8 bytes: mem ref addr, instr pc, tid, pid, marker val
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
    // The initial entry in the file.  The valueA field holds the version
    // (OFFLINE_FILE_VERSION*) while valueB holds the type
    // (OFFLINE_FILE_TYPE*).
    OFFLINE_EXT_TYPE_HEADER,
    // The final entry in the file.  The value fields are 0.
    OFFLINE_EXT_TYPE_FOOTER,
    // A marker type.  The valueB field holds the sub-type and valueA the value.
    OFFLINE_EXT_TYPE_MARKER,
    // Stores the type of access in valueB and the size in valueA.
    // Used for filters on multi-memref instrs where post-processing can't tell
    // which memref passed the filter.
    OFFLINE_EXT_TYPE_MEMINFO,
} offline_ext_type_t;

#define EXT_VALUE_A_BITS 48
#define EXT_VALUE_B_BITS 8

#define PC_MODOFFS_BITS 33
#define PC_MODIDX_BITS 16
#define PC_INSTR_COUNT_BITS 12
#define PC_TYPE_BITS 3

#define OFFLINE_FILE_VERSION_NO_ELISION 2
#define OFFLINE_FILE_VERSION_OLDEST_SUPPORTED OFFLINE_FILE_VERSION_NO_ELISION
#define OFFLINE_FILE_VERSION_ELIDE_UNMOD_BASE 3
#define OFFLINE_FILE_VERSION OFFLINE_FILE_VERSION_ELIDE_UNMOD_BASE

/**
 * Bitfields used to describe the high-level characteristics of both an
 * offline final trace and a raw not-yet-postprocessed trace.
 * In a final trace these are stored in a marker of type #TRACE_MARKER_TYPE_FILETYPE.
 */
typedef enum {
    OFFLINE_FILE_TYPE_DEFAULT = 0x00,
    OFFLINE_FILE_TYPE_FILTERED = 0x01, /**< Addresses filtered online. */
    OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS = 0x02,
    OFFLINE_FILE_TYPE_INSTRUCTION_ONLY = 0x04, /**< Trace has no data references. */
    OFFLINE_FILE_TYPE_ARCH_AARCH64 = 0x08,     /**< Recorded on AArch64. */
    OFFLINE_FILE_TYPE_ARCH_ARM32 = 0x10,       /**< Recorded on ARM (32-bit). */
    OFFLINE_FILE_TYPE_ARCH_X86_32 = 0x20,      /**< Recorded on x86 (32-bit). */
    OFFLINE_FILE_TYPE_ARCH_X86_64 = 0x40,      /**< Recorded on x86 (64-bit). */
    OFFLINE_FILE_TYPE_ARCH_ALL = OFFLINE_FILE_TYPE_ARCH_AARCH64 |
        OFFLINE_FILE_TYPE_ARCH_ARM32 | OFFLINE_FILE_TYPE_ARCH_X86_32 |
        OFFLINE_FILE_TYPE_ARCH_X86_64, /**< All possible architecture types. */
    // For raw files, this is currently stored in an 8-bit field.
    // If we run out of flags we should swap the version to be in valueB and
    // the flags in valueA, leaving the bottom few bits of valueA for compatibility
    // with old versions.
} offline_file_type_t;

static inline const char *
trace_arch_string(offline_file_type_t type)
{
    return TESTANY(OFFLINE_FILE_TYPE_ARCH_AARCH64, type)
        ? "aarch64"
        : (TESTANY(OFFLINE_FILE_TYPE_ARCH_ARM32, type)
               ? "arm"
               : (TESTANY(OFFLINE_FILE_TYPE_ARCH_X86_32, type)
                      ? "i386"
                      : (TESTANY(OFFLINE_FILE_TYPE_ARCH_X86_64, type) ? "x86_64"
                                                                      : "unspecified")));
}

/* We have non-client targets including this header that do not include API
 * headers defining IF_X86_ELSE, etc.  Those don't need this function so we
 * simply exclude them.
 */
#ifdef IF_X86_ELSE
static inline offline_file_type_t
build_target_arch_type()
{
    return IF_X86_ELSE(
        IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_X86_64, OFFLINE_FILE_TYPE_ARCH_X86_32),
        IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_AARCH64, OFFLINE_FILE_TYPE_ARCH_ARM32));
}
#endif

START_PACKED_STRUCTURE
struct _offline_entry_t {
    union {
        // Unfortunately the compiler won't combine bitfields across the union border
        // so we have to duplicate the type field in each alternative.
        struct {
            uint64_t addr : 61;
            uint64_t type : 3;
        } addr;
        struct {
            // This describes the entire basic block.
            uint64_t modoffs : PC_MODOFFS_BITS;
            uint64_t modidx : PC_MODIDX_BITS;
            uint64_t instr_count : PC_INSTR_COUNT_BITS;
            uint64_t type : PC_TYPE_BITS;
        } pc;
        struct {
            uint64_t tid : 61;
            uint64_t type : 3;
        } tid;
        struct {
            uint64_t pid : 61;
            uint64_t type : 3;
        } pid;
        struct {
            uint64_t usec : 61; // Microseconds since Jan 1, 1601.
            uint64_t type : 3;
        } timestamp;
        struct {
            uint64_t valueA : EXT_VALUE_A_BITS; // Meaning is specific to ext type.
            uint64_t valueB : EXT_VALUE_B_BITS; // Meaning is specific to ext type.
            uint64_t ext : 5;                   // Holds an offline_ext_type_t value.
            uint64_t type : 3;
        } extended;
        uint64_t combined_value;
        // XXX: add a CPU id entry for more faithful thread scheduling.
    };
} END_PACKED_STRUCTURE;
typedef struct _offline_entry_t offline_entry_t;

/**
 * The name of the file in -offline mode where module data is written.
 * Its creation can be customized using drmemtrace_custom_module_data()
 * and then modified before passing to raw2trace via
 * drmodtrack_add_custom_data() and drmodtrack_offline_write().
 * Use drmemtrace_get_modlist_path() to obtain the full path.
 */
#define DRMEMTRACE_MODULE_LIST_FILENAME "modules.log"

/**
 * The name of the file in -offline mode where function tracing names
 * are written.  Use drmemtrace_get_funclist_path() to obtain the full path.
 */
#define DRMEMTRACE_FUNCTION_LIST_FILENAME "funclist.log"

#endif /* _TRACE_ENTRY_H_ */
