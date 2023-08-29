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

#ifndef _TRACER_
#define _TRACER_ 1

#include <stddef.h>

#include <atomic>
#include <cstdint>

#include "dr_api.h"
#include "drmemtrace.h"
#include "instru.h"
#include "named_pipe.h"
#include "options.h"
#include "physaddr.h"
#ifdef HAS_SNAPPY
#    include <snappy.h>

#    include "snappy_file_writer.h"
#endif
#ifdef HAS_ZLIB
#    include <zlib.h>
#endif
#ifdef HAS_LZ4
#    include <lz4frame.h>
#endif
#ifdef BUILD_PT_TRACER
#    include "syscall_pt_trace.h"
#endif

namespace dynamorio {
namespace drmemtrace {

extern named_pipe_t ipc_pipe;
// A clean exit via dr_exit_process() is not supported from init code, but
// we do want to at least close the pipe file.
#define FATAL(...)                       \
    do {                                 \
        dr_fprintf(STDERR, __VA_ARGS__); \
        if (!op_offline.get_value())     \
            ipc_pipe.close();            \
        dr_abort();                      \
    } while (0)

/* Thread private data.  This is all set to 0 at thread init. */
typedef struct {
    byte *seg_base;
    byte *buf_base;
    uint64 num_refs;
    uint64 num_writeouts; /* Buffer writeout instances. */
    uint64 bytes_written;
    uint64 cur_window_instr_count;
    /* For offline traces */
    file_t file;
    size_t init_header_size;
    /* For file_ops_func.handoff_buf */
    uint num_buffers;
    byte *reserve_buf;
    /* For level 0 filters */
    byte *l0_dcache;
    byte *l0_icache;
    /* For passing data from app2app to other stages, b/c drbbdup doesn't provide
     * the same user_data path that drmgr does.
     */
    bool repstr;
    bool scatter_gather;
#ifdef HAS_SNAPPY
    snappy_file_writer_t *snappy_writer;
#endif
#ifdef HAS_ZLIB
    z_stream zstream;
    byte *buf_compressed;
#endif
#ifdef HAS_LZ4
    LZ4F_compressionContext_t lzcxt;
    size_t buf_lz4_size;
    byte *buf_lz4;
#endif
    bool has_thread_header;
    // The physaddr_t class is designed to be per-thread.
    physaddr_t physaddr;
    uint64 num_phys_markers;
    byte *v2p_buf;
    uint64 num_v2p_writeouts; /* v2p_buf writeout instances. */
#ifdef BUILD_PT_TRACER
    /* For syscall kernel trace. */
    syscall_pt_trace_t syscall_pt_trace;
#endif
} per_thread_t;

/* Allocated TLS slot offsets */
enum {
    MEMTRACE_TLS_OFFS_BUF_PTR,
    /* XXX: we could make these dynamic to save slots when there's no
     *-L0I_filter or -L0D_filter.
     */
    MEMTRACE_TLS_OFFS_DCACHE,
    MEMTRACE_TLS_OFFS_ICACHE,
    /* The instruction count for -L0I_filter. */
    MEMTRACE_TLS_OFFS_ICOUNT,
    /* The decrementing instruction count for -trace_after_instrs.
     * We could share with MEMTRACE_TLS_OFFS_ICOUNT if we cleared in each thread
     * on the transition.
     */
    MEMTRACE_TLS_OFFS_ICOUNTDOWN,
    // For has_tracing_windows(), this is the ordinal of the tracing window at
    // the start of the current trace buffer.  It is -1 if windows are not enabled.
    MEMTRACE_TLS_OFFS_WINDOW,
    // For -L0_filter_until_instrs, this is used for triggering mode switch in other
    // threads when a thread changes tracing_mode.
    MEMTRACE_TLS_OFFS_MODE,
    MEMTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

extern reg_id_t tls_seg;
extern uint tls_offs;
extern int tls_idx;
#define TLS_SLOT(tls_base, enum_val) \
    (((void **)((byte *)(tls_base) + tls_offs)) + (enum_val))
#define BUF_PTR(tls_base) *(byte **)TLS_SLOT(tls_base, MEMTRACE_TLS_OFFS_BUF_PTR)

extern instru_t *instru;

/* Lock for instrumentation phase transitions and global counters. */
extern void *mutex;

bool
is_first_nonlabel(void *drcontext, instr_t *instr);

extern std::atomic<ptr_int_t> tracing_mode;
extern std::atomic<ptr_int_t> tracing_window;
extern bool attached_midway;
extern std::atomic<uint64> attached_timestamp;

/* We have multiple modes.  While just 2 results in a more efficient dispatch,
 * the power of extra modes justifies the extra overhead.
 *
 * TODO i#3995: Use the new BBDUP_MODE_NOP to implement -max_trace_size with drbbdup
 * cases (with thread-private encodings), to support nudges enabling tracing, and to
 * have a single -trace_for_instrs with no -retrace_every_instrs transition to something
 * lower-cost than counting.
 */
enum {
    BBDUP_MODE_TRACE = 0,     /* Full address tracing. */
    BBDUP_MODE_COUNT = 1,     /* Instr counting for delayed tracing or trace windows. */
    BBDUP_MODE_FUNC_ONLY = 2, /* Function tracing during no-full-trace periods. */
    BBDUP_MODE_NOP = 3,       /* No tracing or counting for pre-attach or post-detach. */
    BBDUP_MODE_L0_FILTER = 4, /* Address tracing with L0_filter. */
};

#if defined(X86_64) || defined(AARCH64)
#    define DELAYED_CHECK_INLINED 1
#else
/* XXX we don't have the inlining implemented yet for 32-bit architectures. */
#endif

#define INSTR_COUNT_LOCAL_UNIT 10000

/***************************************************************************
 * Buffer writing to disk.
 */

/* file operations functions */
struct file_ops_func_t {
    file_ops_func_t()
        : open_file(dr_open_file)
        , open_file_ex(nullptr)
        , read_file(dr_read_file)
        , write_file(dr_write_file)
        , close_file(dr_close_file)
        , create_dir(dr_create_dir)
        , handoff_buf(nullptr)
        , exit_cb(nullptr)
        , exit_arg(nullptr)
    {
    }
    file_t
    call_open_file(const char *fname, uint mode_flags, thread_id_t thread_id,
                   int64 window_id)
    {
        if (open_file_ex != nullptr)
            return open_file_ex(fname, mode_flags, thread_id, window_id);
        return open_file(fname, mode_flags);
    }
    // For process-wide files.
    file_t
    open_process_file(const char *fname, uint mode_flags)
    {
        return call_open_file(fname, mode_flags, 0, -1);
    }
    drmemtrace_open_file_func_t open_file;
    drmemtrace_open_file_ex_func_t open_file_ex;
    drmemtrace_read_file_func_t read_file;
    drmemtrace_write_file_func_t write_file;
    drmemtrace_close_file_func_t close_file;
    drmemtrace_create_dir_func_t create_dir;
    drmemtrace_handoff_func_t handoff_buf;
    drmemtrace_exit_func_t exit_cb;
    void *exit_arg;
};
extern struct file_ops_func_t file_ops_func;

extern uint64 num_refs_racy; /* racy global memory reference count */
extern uint64
    num_filter_refs_racy; /* racy global memory reference count in warmup mode */
extern char logsubdir[MAXIMUM_PATH];
extern char subdir_prefix[MAXIMUM_PATH]; /* Holds op_subdir_prefix. */
extern size_t trace_buf_size;
extern size_t redzone_size;
extern size_t max_buf_size;
extern size_t buf_hdr_slots_size;

#define MAX_NUM_DELAY_INSTRS 32
// Really sizeof(trace_entry_t.length)
#define MAX_NUM_DELAY_ENTRIES (MAX_NUM_DELAY_INSTRS / sizeof(addr_t))

static inline bool
is_num_refs_beyond_global_max(void)
{
    return op_max_global_trace_refs.get_value() > 0 &&
        (num_refs_racy - num_filter_refs_racy) > op_max_global_trace_refs.get_value();
}

static inline bool
is_bytes_written_beyond_trace_max(per_thread_t *data)
{
    return op_max_trace_size.get_value() > 0 &&
        data->bytes_written > op_max_trace_size.get_value();
}

static inline bool
has_tracing_windows()
{
    // We return true for a single-window -trace_for_instrs (without -retrace) setup
    // since we rely on having window numbers for the end-of-block buffer output check
    // used for a single-window transition away from tracing.
    return op_trace_for_instrs.get_value() > 0 || op_retrace_every_instrs.get_value() > 0;
}

static inline bool
align_attach_detach_endpoints()
{
    return attached_midway && op_align_endpoints.get_value();
}

static inline bool
is_in_tracing_mode(uintptr_t mode)
{
    return (mode == BBDUP_MODE_TRACE || mode == BBDUP_MODE_L0_FILTER);
}

void
get_L0_filters_enabled(uintptr_t mode, OUT bool *l0i_enabled, OUT bool *l0d_enabled);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _TRACER_ */
