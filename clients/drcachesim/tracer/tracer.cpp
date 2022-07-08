/* ******************************************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * ******************************************************************************/

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

/* tracer.cpp: tracing client for feeding data to cache simulator.
 *
 * Originally built from the memtrace_opt.c sample.
 * XXX i#1703, i#2001: add in more optimizations to improve performance.
 * XXX i#1703: perhaps refactor and split up to make it more
 * modular.
 */

#include <limits.h>
#include <string.h>
#include <atomic>
#include <string>
#include "dr_api.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drmemtrace.h"
#include "drreg.h"
#include "drutil.h"
#include "drx.h"
#include "drstatecmp.h"
#include "droption.h"
#include "drbbdup.h"
#include "instru.h"
#include "raw2trace.h"
#include "physaddr.h"
#include "func_trace.h"
#include "../common/trace_entry.h"
#include "../common/named_pipe.h"
#include "../common/options.h"
#include "../common/utils.h"
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

#ifdef ARM
#    include "../../../core/unix/include/syscall_linux_arm.h" // for SYS_cacheflush
#endif

/* Make sure we export function name as the symbol name without mangling. */
#ifdef __cplusplus
extern "C" {
DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[]);
}
#endif

/* Request debug-build checks on use of malloc mid-run which will break statically
 * linking this client into an app.
 */
DR_DISALLOW_UNSAFE_STATIC

// A clean exit via dr_exit_process() is not supported from init code, but
// we do want to at least close the pipe file.
#define FATAL(...)                       \
    do {                                 \
        dr_fprintf(STDERR, __VA_ARGS__); \
        if (!op_offline.get_value())     \
            ipc_pipe.close();            \
        dr_abort();                      \
    } while (0)

static char logsubdir[MAXIMUM_PATH];
static char subdir_prefix[MAXIMUM_PATH]; /* Holds op_subdir_prefix. */
static file_t module_file;
static file_t funclist_file = INVALID_FILE;
static int notify_beyond_global_max_once;

/* Max number of entries a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
// XXX i#1703: use an option instead.
#define MAX_NUM_ENTRIES 4096
/* The buffer size for holding trace entries. */
static size_t trace_buf_size;
/* The redzone is allocated right after the trace buffer.
 * We fill the redzone with sentinel value to detect when the redzone
 * is reached, i.e., when the trace buffer is full.
 */
static size_t redzone_size;
static size_t max_buf_size;

static drvector_t scratch_reserve_vec;

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
} per_thread_t;

#define MAX_NUM_DELAY_INSTRS 32
// Really sizeof(trace_entry_t.length)
#define MAX_NUM_DELAY_ENTRIES (MAX_NUM_DELAY_INSTRS / sizeof(addr_t))
/* per bb user data during instrumentation */
typedef struct {
    app_pc last_app_pc;
    instr_t *strex;
    int num_delay_instrs;
    instr_t *delay_instrs[MAX_NUM_DELAY_INSTRS];
    bool repstr;
    bool scatter_gather;
    void *instru_field;        /* For use by instru_t. */
    bool recorded_instr;       /* For offline single-PC-per-block. */
    int bb_instr_count;        /* For filtered traces. */
    bool recorded_instr_count; /* For filtered traces. */
} user_data_t;

/* For online simulation, we write to a single global pipe */
static named_pipe_t ipc_pipe;

#define MAX_INSTRU_SIZE 128 /* the max obj size of instr_t or its children */
static instru_t *instru;

static client_id_t client_id;
static void *mutex;          /* for multithread support */
static uint64 num_refs;      /* keep a global memory reference count */
static uint64 num_refs_racy; /* racy global memory reference count */
static uint64 num_writeouts;
static uint64 num_v2p_writeouts;
static uint64 num_phys_markers;
static volatile bool exited_process;

static drmgr_priority_t pri_pre_bbdup = { sizeof(drmgr_priority_t),
                                          DRMGR_PRIORITY_NAME_MEMTRACE, NULL, NULL,
                                          DRMGR_PRIORITY_APP2APP_DRBBDUP - 1 };

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
    MEMTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};
static reg_id_t tls_seg;
static uint tls_offs;
static int tls_idx;
#define TLS_SLOT(tls_base, enum_val) \
    (((void **)((byte *)(tls_base) + tls_offs)) + (enum_val))
#define BUF_PTR(tls_base) *(byte **)TLS_SLOT(tls_base, MEMTRACE_TLS_OFFS_BUF_PTR)
/* We leave slot(s) at the start so we can easily insert a header entry */
static size_t buf_hdr_slots_size;

static bool (*should_trace_thread_cb)(thread_id_t tid, void *user_data);
static void *trace_thread_cb_user_data;
static bool thread_filtering_enabled;

/* Similarly to -trace_after_instrs, we use thread-local counters to avoid
 * synchronization costs and only add to the global every N counts.
 */
#define INSTR_COUNT_LOCAL_UNIT 10000
static std::atomic<uint64> cur_window_instr_count;

static bool
count_traced_instrs(void *drcontext, int toadd);

static void
reached_traced_instrs_threshold(void *drcontext);

static offline_file_type_t
get_file_type();

static uint64
local_instr_count_threshold()
{
    uint64 limit = op_trace_for_instrs.get_value();
    if (limit > INSTR_COUNT_LOCAL_UNIT * 10)
        return INSTR_COUNT_LOCAL_UNIT;
    else {
        /* For small windows, use a smaller add-to-global trigger. */
        return limit / 10;
    }
}

static bool
has_tracing_windows()
{
    // We return true for a single-window -trace_for_instrs (without -retrace) setup
    // since we rely on having window numbers for the end-of-block buffer output check
    // used for a single-window transition away from tracing.
    return op_trace_for_instrs.get_value() > 0 || op_retrace_every_instrs.get_value() > 0;
}

static bool
bbdup_duplication_enabled()
{
    return op_trace_after_instrs.get_value() > 0 || op_trace_for_instrs.get_value() > 0 ||
        op_retrace_every_instrs.get_value() > 0;
}

static ptr_int_t
get_local_window(per_thread_t *data)
{
    return *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_WINDOW);
}

static std::atomic<ptr_int_t> tracing_window;

#ifdef HAS_SNAPPY
static inline bool
snappy_enabled()
{
    return op_raw_compress.get_value() == "snappy" ||
        op_raw_compress.get_value() == "snappy_nocrc";
}
#endif

#ifdef HAS_ZLIB
static void *
redirect_malloc(void *drcontext, uint items, uint per_size)
{
    void *mem;
    size_t size = items * per_size; /* XXX: ignoring overflow */
    size += sizeof(size_t);
    mem = dr_custom_alloc(nullptr, static_cast<dr_alloc_flags_t>(0), size,
                          DR_MEMPROT_READ | DR_MEMPROT_WRITE, nullptr);
    if (mem == NULL)
        return Z_NULL;
    *((size_t *)mem) = size;
    return (byte *)mem + sizeof(size_t);
}

static void
redirect_free(void *drcontext, void *ptr)
{
    if (ptr != NULL) {
        byte *mem = (byte *)ptr;
        mem -= sizeof(size_t);
        dr_custom_free(nullptr, static_cast<dr_alloc_flags_t>(0), mem, *((size_t *)mem));
    }
}
#endif

/***************************************************************************
 * Buffer writing to disk.
 */

#ifdef HAS_LZ4
static const LZ4F_preferences_t lz4_ops = {
    { LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
      /*unknown contentSize=*/0, /*dictID=*/0, LZ4F_noBlockChecksum },
    // We may want to expose this knob as a parameter.  The fastest for my
    // SSD is -4096, but on another machine 0 is fastest; plus, we may want
    // to raise it to 3 for cases with higher i/o overhead, where it is
    // slower but still outperforms zlib/gzip.
    /*fastest compressionLevel=*/0,
    /*autoFlush=*/0,
    /*do not favorDecSpeed=*/0,
    /*reserved=*/ { 0, 0, 0 },
};
#endif

/* file operations functions */
struct file_ops_func_t {
    file_ops_func_t()
        : open_file(dr_open_file)
        , read_file(dr_read_file)
        , write_file(dr_write_file)
        , close_file(dr_close_file)
        , create_dir(dr_create_dir)
        , handoff_buf(nullptr)
        , exit_cb(nullptr)
        , exit_arg(nullptr)
    {
    }
    drmemtrace_open_file_func_t open_file;
    drmemtrace_read_file_func_t read_file;
    drmemtrace_write_file_func_t write_file;
    drmemtrace_close_file_func_t close_file;
    drmemtrace_create_dir_func_t create_dir;
    drmemtrace_handoff_func_t handoff_buf;
    drmemtrace_exit_func_t exit_cb;
    void *exit_arg;
};
static struct file_ops_func_t file_ops_func;

drmemtrace_status_t
drmemtrace_replace_file_ops(drmemtrace_open_file_func_t open_file_func,
                            drmemtrace_read_file_func_t read_file_func,
                            drmemtrace_write_file_func_t write_file_func,
                            drmemtrace_close_file_func_t close_file_func,
                            drmemtrace_create_dir_func_t create_dir_func)
{
    /* We don't check op_offline b/c option parsing may not have happened yet. */
    if (open_file_func != NULL)
        file_ops_func.open_file = open_file_func;
    if (read_file_func != NULL)
        file_ops_func.read_file = read_file_func;
    if (write_file_func != NULL)
        file_ops_func.write_file = write_file_func;
    if (close_file_func != NULL)
        file_ops_func.close_file = close_file_func;
    if (create_dir_func != NULL)
        file_ops_func.create_dir = create_dir_func;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_buffer_handoff(drmemtrace_handoff_func_t handoff_func,
                          drmemtrace_exit_func_t exit_func, void *exit_func_arg)
{
    /* We don't check op_offline b/c option parsing may not have happened yet. */
    file_ops_func.handoff_buf = handoff_func;
    file_ops_func.exit_cb = exit_func;
    file_ops_func.exit_arg = exit_func_arg;
    return DRMEMTRACE_SUCCESS;
}

static char modlist_path[MAXIMUM_PATH];
static char funclist_path[MAXIMUM_PATH];

drmemtrace_status_t
drmemtrace_get_output_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = logsubdir;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_modlist_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = modlist_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_get_funclist_path(OUT const char **path)
{
    if (path == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    *path = funclist_path;
    return DRMEMTRACE_SUCCESS;
}

drmemtrace_status_t
drmemtrace_custom_module_data(void *(*load_cb)(module_data_t *module, int seg_idx),
                              int (*print_cb)(void *data, char *dst, size_t max_len),
                              void (*free_cb)(void *data))
{
    /* We want to support this being called prior to initializing us, so we use
     * a static routine and do not check -offline.
     */
    if (offline_instru_t::custom_module_data(load_cb, print_cb, free_cb))
        return DRMEMTRACE_SUCCESS;
    else
        return DRMEMTRACE_ERROR;
}

drmemtrace_status_t
drmemtrace_filter_threads(bool (*should_trace_thread)(thread_id_t tid, void *user_data),
                          void *user_value)
{
    if (should_trace_thread == NULL)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    /* We document that this should be called once at init time: i.e., we do not
     * handle races here.
     * XXX: to filter out the calling thread (the initial application thread) we suggest
     * that this be called prior to DR initialization, but that's only feasible for
     * the start/stop API.  We should consider remembering the main thread's
     * per_thread_t and freeing it here if the filter routine says to skip it.
     */
    should_trace_thread_cb = should_trace_thread;
    trace_thread_cb_user_data = user_value;
    thread_filtering_enabled = true;
    return DRMEMTRACE_SUCCESS;
}

static int
append_unit_header(void *drcontext, byte *buf_ptr, thread_id_t tid, ptr_int_t window)
{
    int size_added = instru->append_unit_header(buf_ptr, tid, window);
    if (op_L0I_filter.get_value()) {
        // Include the instruction count.
        // It might be useful to include the count with each miss as well, but
        // in experiments that adds non-trivial space and time overheads (as
        // a separate marker; squished into the instr_count field might be
        // better but at complexity costs, plus we may need that field for
        // offset-within-block info to adjust the per-block count) and
        // would likely need to be under an off-by-default option and have
        // a mandated use case to justify adding it.
        // Per-buffer should be sufficient as markers to align filtered traces
        // with unfiltered traces, and is much lower overhead.
        uintptr_t icount = 0;
        if (drcontext != NULL) { // Handle process-init header.
            per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
            icount = *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNT);
        }
        size_added += instru->append_marker(buf_ptr + size_added,
                                            TRACE_MARKER_TYPE_INSTRUCTION_COUNT, icount);
    }
    return size_added;
}

static void
open_new_window_dir(ptr_int_t window_num)
{
    if (!op_split_windows.get_value())
        return;
    DR_ASSERT(op_offline.get_value());
    char windir[MAXIMUM_PATH];
    dr_snprintf(windir, BUFFER_SIZE_ELEMENTS(windir), "%s%s" WINDOW_SUBDIR_FORMAT,
                logsubdir, DIRSEP, window_num);
    NULL_TERMINATE_BUFFER(windir);
    if (!file_ops_func.create_dir(windir))
        FATAL("Failed to create window subdir %s\n", windir);
    NOTIFY(2, "Created new window dir %s\n", windir);
}

static void
close_thread_file(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
#ifdef HAS_SNAPPY
    if (op_offline.get_value() && snappy_enabled()) {
        data->snappy_writer->~snappy_file_writer_t();
        dr_custom_free(nullptr, static_cast<dr_alloc_flags_t>(0), data->snappy_writer,
                       sizeof(*data->snappy_writer));
        data->snappy_writer = nullptr;
    }
#endif
#ifdef HAS_ZLIB
    if (op_offline.get_value() &&
        (op_raw_compress.get_value() == "zlib" ||
         op_raw_compress.get_value() == "gzip")) {
        // Flush remaining data.
        data->zstream.next_in = (Bytef *)BUF_PTR(data->seg_base);
        data->zstream.avail_in = 0;
        int res, iters = 0;
        const int MAX_ITERS = 32; // Sanity limit to avoid hang.
        do {
            data->zstream.next_out = (Bytef *)data->buf_compressed;
            data->zstream.avail_out = max_buf_size;
            res = deflate(&data->zstream, Z_FINISH);
            NOTIFY(3, "final deflate => %d in=%d out=%d => in=%d, out=%d, wrote=%d\n",
                   res, 0, max_buf_size, data->zstream.avail_in, data->zstream.avail_out,
                   max_buf_size - data->zstream.avail_out);
            file_ops_func.write_file(data->file, data->buf_compressed,
                                     max_buf_size - data->zstream.avail_out);
        } while ((res == Z_OK || res == Z_BUF_ERROR) && ++iters < MAX_ITERS);
        DR_ASSERT(res == Z_STREAM_END);
        deflateEnd(&data->zstream);
    }
#endif
#ifdef HAS_LZ4
    if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
        // Flush remaining data.
        size_t res =
            LZ4F_compressEnd(data->lzcxt, data->buf_lz4, data->buf_lz4_size, nullptr);
        DR_ASSERT(!LZ4F_isError(res));
        file_ops_func.write_file(data->file, data->buf_lz4, res);
        res = LZ4F_freeCompressionContext(data->lzcxt);
        DR_ASSERT(!LZ4F_isError(res));
    }
#endif
    file_ops_func.close_file(data->file);
    data->file = INVALID_FILE;
}

// Returns whether a new file was opened (it won't be for -no_split_windows).
static bool
open_new_thread_file(void *drcontext, ptr_int_t window_num)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    bool opened_new_file = false;
    DR_ASSERT(op_offline.get_value());
    const char *dir = logsubdir;
    char windir[MAXIMUM_PATH];
    if (has_tracing_windows()) {
        if (op_split_windows.get_value()) {
            dr_snprintf(windir, BUFFER_SIZE_ELEMENTS(windir), "%s%s" WINDOW_SUBDIR_FORMAT,
                        logsubdir, DIRSEP, window_num);
            NULL_TERMINATE_BUFFER(windir);
            dir = windir;
        } else if (data->file != INVALID_FILE)
            return false;
    }
    /* We do not need to call drx_init before using drx_open_unique_appid_file.
     * Since we're now in a subdir we could make the name simpler but this
     * seems nice and complete.
     */
    char buf[MAXIMUM_PATH];
    int i;
    const int NUM_OF_TRIES = 10000;
    uint flags =
        IF_UNIX(DR_FILE_CLOSE_ON_FORK |) DR_FILE_ALLOW_LARGE | DR_FILE_WRITE_REQUIRE_NEW;
    /* We use drx_open_unique_appid_file with DRX_FILE_SKIP_OPEN to get a
     * file name for creation.  Retry if the same name file already exists.
     * Abort if we fail too many times.
     */
    const char *suffix = OUTFILE_SUFFIX;
#ifdef HAS_SNAPPY
    if (snappy_enabled())
        suffix = OUTFILE_SUFFIX_SZ;
#endif
#ifdef HAS_ZLIB
    if (op_raw_compress.get_value() == "zlib")
        suffix = OUTFILE_SUFFIX_ZLIB;
    else if (op_raw_compress.get_value() == "gzip")
        suffix = OUTFILE_SUFFIX_GZ;
#endif
#ifdef HAS_LZ4
    if (op_raw_compress.get_value() == "lz4")
        suffix = OUTFILE_SUFFIX_LZ4;
#endif
    for (i = 0; i < NUM_OF_TRIES; i++) {
        drx_open_unique_appid_file(dir, dr_get_thread_id(drcontext), subdir_prefix,
                                   suffix, DRX_FILE_SKIP_OPEN, buf,
                                   BUFFER_SIZE_ELEMENTS(buf));
        NULL_TERMINATE_BUFFER(buf);
        file_t new_file = file_ops_func.open_file(buf, flags);
        if (new_file == INVALID_FILE)
            continue;
        if (new_file == data->file)
            FATAL("Failed to create new thread file for window %s\n", buf);
        NOTIFY(2, "Created thread trace file %s\n", buf);
        opened_new_file = true;
        if (data->file != INVALID_FILE)
            close_thread_file(drcontext);
        data->file = new_file;
#ifdef HAS_SNAPPY
        if (snappy_enabled()) {
            // We use placement new for better isolation.
            void *placement = dr_custom_alloc(
                nullptr, static_cast<dr_alloc_flags_t>(0), sizeof(*data->snappy_writer),
                DR_MEMPROT_READ | DR_MEMPROT_WRITE, nullptr);
            data->snappy_writer = new (placement)
                snappy_file_writer_t(data->file, file_ops_func.write_file,
                                     op_raw_compress.get_value() != "snappy_nocrc");
            data->snappy_writer->write_file_header();
        }
#endif
#ifdef HAS_ZLIB
        if (op_offline.get_value() && op_raw_compress.get_value() == "zlib") {
            memset(&data->zstream, 0, sizeof(data->zstream));
            data->zstream.zalloc = redirect_malloc;
            data->zstream.zfree = redirect_free;
            data->zstream.opaque = drcontext;
            int res = deflateInit(&data->zstream, Z_BEST_SPEED);
            DR_ASSERT(res == Z_OK);
        } else if (op_offline.get_value() && op_raw_compress.get_value() == "gzip") {
            memset(&data->zstream, 0, sizeof(data->zstream));
            data->zstream.zalloc = redirect_malloc;
            data->zstream.zfree = redirect_free;
            data->zstream.opaque = drcontext;
            const int ZLIB_WINDOW_SIZE = 15;
            const int ZLIB_REQUEST_GZIP = 16; /* Added to size to trigger gz headers. */
            const int ZLIB_MAX_MEM = 9;       /* For optimal speed. */
            int res = deflateInit2(&data->zstream, Z_BEST_SPEED, Z_DEFLATED,
                                   ZLIB_WINDOW_SIZE + ZLIB_REQUEST_GZIP, ZLIB_MAX_MEM,
                                   Z_DEFAULT_STRATEGY);
            DR_ASSERT(res == Z_OK);
            /* We use the default gzip header and don't call deflateSetHeader. */
        }
#endif
#ifdef HAS_LZ4
        if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
            size_t res = LZ4F_createCompressionContext(&data->lzcxt, LZ4F_VERSION);
            DR_ASSERT(!LZ4F_isError(res));
            // Write out the header.
            res = LZ4F_compressBegin(data->lzcxt, data->buf_lz4, data->buf_lz4_size,
                                     &lz4_ops);
            DR_ASSERT(!LZ4F_isError(res));
            ssize_t wrote = file_ops_func.write_file(data->file, data->buf_lz4, res);
            DR_ASSERT(static_cast<size_t>(wrote) == res);
        }
#endif
        break;
    }
    if (i == NUM_OF_TRIES) {
        FATAL("Fatal error: failed to create trace file %s\n", buf);
    }
    return opened_new_file;
}

static size_t
prepend_offline_thread_header(void *drcontext)
{
    DR_ASSERT(op_offline.get_value());
    /* Write initial headers at the top of the first buffer. */
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    size_t size = reinterpret_cast<offline_instru_t *>(instru)->append_thread_header(
        data->buf_base, dr_get_thread_id(drcontext), get_file_type());
    BUF_PTR(data->seg_base) = data->buf_base + size + buf_hdr_slots_size;
    data->has_thread_header = true;
    return size;
}

static inline byte *
atomic_pipe_write(void *drcontext, byte *pipe_start, byte *pipe_end, ptr_int_t window)
{
    ssize_t towrite = pipe_end - pipe_start;
    DR_ASSERT(towrite <= ipc_pipe.get_atomic_write_size() && towrite > 0);
    if (ipc_pipe.write((void *)pipe_start, towrite) < (ssize_t)towrite) {
        FATAL("Fatal error: failed to write to pipe\n");
    }
    // Re-emit buffer unit header to handle split pipe writes.
    if (pipe_end - buf_hdr_slots_size > pipe_start) {
        pipe_start = pipe_end - buf_hdr_slots_size;
        append_unit_header(drcontext, pipe_start, dr_get_thread_id(drcontext), window);
    }
    return pipe_start;
}

static inline byte *
write_trace_data(void *drcontext, byte *towrite_start, byte *towrite_end,
                 ptr_int_t window)
{
    if (op_offline.get_value()) {
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        ssize_t size = towrite_end - towrite_start;
        DR_ASSERT(data->file != INVALID_FILE);
        if (file_ops_func.handoff_buf != NULL) {
            if (!file_ops_func.handoff_buf(data->file, towrite_start, size,
                                           max_buf_size)) {
                FATAL("Fatal error: failed to hand off trace\n");
            }
        } else {
            ssize_t wrote;
#ifdef HAS_SNAPPY
            if (op_offline.get_value() && snappy_enabled())
                wrote = data->snappy_writer->compress_and_write(towrite_start, size);
            else
#endif
#ifdef HAS_ZLIB
                if (op_offline.get_value() &&
                    (op_raw_compress.get_value() == "zlib" ||
                     op_raw_compress.get_value() == "gzip")) {
                data->zstream.next_in = (Bytef *)towrite_start;
                data->zstream.avail_in = size;
                int res;
                do {
                    data->zstream.next_out = (Bytef *)data->buf_compressed;
                    data->zstream.avail_out = max_buf_size;
                    res = deflate(&data->zstream, Z_NO_FLUSH);
                    NOTIFY(3, "deflate => %d in=%d out=%d => in=%d, out=%d, write=%d\n",
                           res, size, size, data->zstream.avail_in,
                           data->zstream.avail_out,
                           max_buf_size - data->zstream.avail_out);
                    DR_ASSERT(res != Z_STREAM_ERROR);
                    wrote =
                        file_ops_func.write_file(data->file, data->buf_compressed,
                                                 max_buf_size - data->zstream.avail_out);
                } while (data->zstream.avail_out == 0);
                DR_ASSERT(data->zstream.avail_in == 0);
                wrote = size;
            } else
#endif
#ifdef HAS_LZ4
                if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
                size_t res =
                    LZ4F_compressUpdate(data->lzcxt, data->buf_lz4, data->buf_lz4_size,
                                        towrite_start, size, nullptr);
                DR_ASSERT(!LZ4F_isError(res));
                wrote = file_ops_func.write_file(data->file, data->buf_lz4, res);
                DR_ASSERT(static_cast<size_t>(wrote) == res);
                wrote = size;
            } else
#endif
                wrote = file_ops_func.write_file(data->file, towrite_start, size);
            if (wrote < size) {
                FATAL("Fatal error: failed to write trace for T%d window %zd: wrote %zd "
                      "of %zd\n",
                      dr_get_thread_id(drcontext), get_local_window(data), wrote, size);
            }
        }
        return towrite_start;
    } else {
#ifdef HAS_SNAPPY
        // XXX i#5427: Use snappy compression for pipe data as well.  We need to
        // create a reader on the other end first.
#endif
        return atomic_pipe_write(drcontext, towrite_start, towrite_end, window);
    }
}

// Should only be called when the trace buffer is empty.
static void
set_local_window(void *drcontext, ptr_int_t value)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    NOTIFY(3, "%s: T%d %zd (old: %zd)\n", __FUNCTION__, dr_get_thread_id(drcontext),
           value, get_local_window(data));
    if (op_offline.get_value()) {
        ptr_int_t old_val = get_local_window(data);
        if (old_val < value || value == 0) {
            // Write out empty thread files for each bypassed window.
            while (++old_val < value && op_split_windows.get_value()) {
                NOTIFY(2, "Writing empty file for T%d window %zd\n",
                       dr_get_thread_id(drcontext), old_val);
                if (!open_new_thread_file(drcontext, old_val)) {
                    // If the replacement open does not want separate files, do not write
                    // new headers.
                    continue;
                }
                byte buf[sizeof(offline_entry_t) * 32]; // Should need <<32.
                byte *entry = buf;
                entry +=
                    reinterpret_cast<offline_instru_t *>(instru)->append_thread_header(
                        entry, dr_get_thread_id(drcontext), get_file_type());
                entry += append_unit_header(drcontext, entry, dr_get_thread_id(drcontext),
                                            old_val);
                // XXX: What about TRACE_MARKER_TYPE_INSTRUCTION_COUNT for filtered
                // like event_thread_exit writes?
                entry += instru->append_thread_exit(entry, dr_get_thread_id(drcontext));
                DR_ASSERT(BUFFER_SIZE_BYTES(buf) >= (size_t)(entry - buf));
                write_trace_data(drcontext, (byte *)buf, entry, old_val);
                close_thread_file(drcontext);
            }
            if ((value > 0 && op_split_windows.get_value()) ||
                data->init_header_size == 0) {
                size_t header_size = prepend_offline_thread_header(drcontext);
                if (data->init_header_size == 0)
                    data->init_header_size = header_size;
                else
                    DR_ASSERT(header_size == data->init_header_size);
            }
            // We delay opening the next window's file to avoid an empty final file.
            // The initial file is opened at thread init.
            if (data->file != INVALID_FILE && value > 0 && op_split_windows.get_value())
                close_thread_file(drcontext);
        }
    }
    *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_WINDOW) = value;
}

static void
create_buffer(per_thread_t *data)
{
    data->buf_base =
        (byte *)dr_raw_mem_alloc(max_buf_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    /* For file_ops_func.handoff_buf we have to handle failure as OOM is not unlikely. */
    if (data->buf_base == NULL) {
        /* Switch to "reserve" buffer. */
        if (data->reserve_buf == NULL) {
            FATAL("Fatal error: out of memory and cannot recover.\n");
        }
        NOTIFY(0, "Out of memory: truncating further tracing.\n");
        data->buf_base = data->reserve_buf;
        /* Avoid future buffer output. */
        op_max_trace_size.set_value(data->bytes_written - 1);
        return;
    }
    /* dr_raw_mem_alloc guarantees to give us zeroed memory, so no need for a memset */
    /* set sentinel (non-zero) value in redzone */
    memset(data->buf_base + trace_buf_size, -1, redzone_size);
    data->num_buffers++;
    if (data->num_buffers == 2) {
        /* Create a "reserve" buffer so we can continue after hitting OOM later.
         * It is much simpler to keep running the same instru that writes to a
         * buffer and just never write it out, similarly to how we handle
         * -max_trace_size.  This costs us some memory (not for idle threads: that's
         * why we wait for the 2nd buffer) but we gain simplicity.
         */
        data->reserve_buf = (byte *)dr_raw_mem_alloc(
            max_buf_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        if (data->reserve_buf != NULL)
            memset(data->reserve_buf + trace_buf_size, -1, redzone_size);
    }
}

static size_t
get_v2p_buffer_size()
{
    // The handoff interface requires we use dr_raw_mem_alloc and thus page alignment.
    // The v2p buffer needs to hold at most enough physical,virtual marker pairs for one
    // regular MAX_NUM_ENTRIES trace buffer; if it's smaller, we'll handle that by
    // emitting multiple times.
    //
    // Currently we use one page which is 256 entries for 4K pages (assuming zero upper
    // bits: so no vsyscall or >48-bit addresses; the upper 16 bits being set would
    // require extra markers for each address) which is enough for a single buffer for
    // most cases.
    //
    // XXX: For many-thread apps, and esp on machines with larger pages, this could
    // use a lot of additional memory we don't need: consider for 64K pages and 10K
    // threads that's 640MB!  We could use a smaller buffer when the handoff interface
    // is not in effect, or change the interface to use a different free function.
    return dr_page_size();
}

static void
create_v2p_buffer(per_thread_t *data)
{
    data->v2p_buf = (byte *)dr_raw_mem_alloc(get_v2p_buffer_size(),
                                             DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    /* For file_ops_func.handoff_buf we have to handle failure as OOM is not unlikely. */
    if (data->v2p_buf == NULL) {
        FATAL("Failed to allocate virtual-to-physical buffer.\n");
    }
}

static bool
is_ok_to_split_before(trace_type_t type)
{
    return type_is_instr(type) || type == TRACE_TYPE_INSTR_MAYBE_FETCH ||
        type == TRACE_TYPE_MARKER || type == TRACE_TYPE_THREAD_EXIT ||
        op_L0I_filter.get_value();
}

static inline bool
is_num_refs_beyond_global_max(void)
{
    return op_max_global_trace_refs.get_value() > 0 &&
        num_refs_racy > op_max_global_trace_refs.get_value();
}

static inline bool
is_bytes_written_beyond_trace_max(per_thread_t *data)
{
    return op_max_trace_size.get_value() > 0 &&
        data->bytes_written > op_max_trace_size.get_value();
}

static size_t
add_buffer_header(void *drcontext, per_thread_t *data, byte *buf_base)
{
    size_t header_size = 0;
    // For online we already wrote the thread header but for offline it is in
    // the first buffer, so skip over it.
    if (data->has_thread_header && op_offline.get_value())
        header_size = data->init_header_size;
    data->has_thread_header = false;
    // The initial slots are left empty for the header, which we add here.
    header_size +=
        append_unit_header(drcontext, buf_base + header_size, dr_get_thread_id(drcontext),
                           get_local_window(data));
    return header_size;
}

static uint
output_buffer(void *drcontext, per_thread_t *data, byte *buf_base, byte *buf_ptr,
              size_t header_size)
{
    byte *pipe_start = buf_base;
    byte *pipe_end = pipe_start;
    if (!op_offline.get_value()) {
        for (byte *mem_ref = buf_base + header_size; mem_ref < buf_ptr;
             mem_ref += instru->sizeof_entry()) {
            // Split up the buffer into multiple writes to ensure atomic pipe writes.
            // We can only split before TRACE_TYPE_INSTR, assuming only a few data
            // entries in between instr entries.
            // XXX i#2638: if we want to support branch target analysis in online
            // traces we'll need to not split after a branch: either split before
            // it or one instr after.
            if (is_ok_to_split_before(instru->get_entry_type(mem_ref))) {
                pipe_end = mem_ref;
                // We check the end of this entry + the max # of delay entries to
                // avoid splitting an instr from its subsequent bundle entry.
                // An alternative is to have the reader use per-thread state.
                if ((mem_ref + (1 + MAX_NUM_DELAY_ENTRIES) * instru->sizeof_entry() -
                     pipe_start) > ipc_pipe.get_atomic_write_size()) {
                    DR_ASSERT(is_ok_to_split_before(
                        instru->get_entry_type(pipe_start + header_size)));
                    pipe_start = atomic_pipe_write(drcontext, pipe_start, pipe_end,
                                                   get_local_window(data));
                }
            }
        }
        // Write the rest to pipe
        // The last few entries (e.g., instr + refs) may exceed the atomic write size,
        // so we may need two writes.
        // XXX i#2638: if we want to support branch target analysis in online
        // traces we'll need to not split after a branch by carrying a write-final
        // branch forward to the next buffer.
        if ((buf_ptr - pipe_start) > ipc_pipe.get_atomic_write_size()) {
            DR_ASSERT(
                is_ok_to_split_before(instru->get_entry_type(pipe_start + header_size)));
            pipe_start = atomic_pipe_write(drcontext, pipe_start, pipe_end,
                                           get_local_window(data));
        }
        if ((buf_ptr - pipe_start) > (ssize_t)buf_hdr_slots_size) {
            DR_ASSERT(
                is_ok_to_split_before(instru->get_entry_type(pipe_start + header_size)));
            atomic_pipe_write(drcontext, pipe_start, buf_ptr, get_local_window(data));
        }
    } else {
        write_trace_data(drcontext, pipe_start, buf_ptr, get_local_window(data));
    }
    auto span = buf_ptr - buf_base; // Include the header.
    DR_ASSERT(span % instru->sizeof_entry() == 0);
    uint current_num_refs = (uint)(span / instru->sizeof_entry());
    data->num_refs += current_num_refs;
    data->bytes_written += buf_ptr - pipe_start;
    bool is_v2p = false;
    if (buf_base >= data->v2p_buf && buf_base < data->v2p_buf + get_v2p_buffer_size())
        is_v2p = true;
    if (is_v2p)
        ++data->num_v2p_writeouts;
    else
        ++data->num_writeouts;

    if (file_ops_func.handoff_buf != NULL) {
        // The owner of the handoff callback now owns the buffer, and we get a new one.
        if (is_v2p)
            create_v2p_buffer(data);
        else
            create_buffer(data);
    }
    return current_num_refs;
}

static byte *
process_entry_for_physaddr(void *drcontext, per_thread_t *data, size_t header_size,
                           byte *v2p_ptr, byte *mem_ref, addr_t virt, trace_type_t type,
                           bool *emitted, size_t *skip)
{
    bool from_cache = false;
    addr_t phys = 0;
    bool success = data->physaddr.virtual2physical(drcontext, virt, &phys, &from_cache);
    ASSERT(emitted != NULL && skip != NULL, "invalid input parameters");
    NOTIFY(4, "%s: type=%2d virt=%p phys=%p\n", __FUNCTION__, type, virt, phys);
    if (!success) {
        // XXX i#1735: Unfortunately this happens; currently we use the virtual
        // address and continue.
        // Cases where xl8 fails include:
        // - vsyscall/kernel page,
        // - wild access (NULL or very large bogus address) by app
        // - page is swapped out (unlikely since we're querying *after* the
        //   the app just accessed, but could happen)
        NOTIFY(1, "virtual2physical translation failure for type=%2d addr=%p\n", type,
               virt);
        phys = virt;
    }
    if (op_offline.get_value()) {
        // For offline we keep the main entries as virtual but add markers showing
        // the corresponding physical.  We assume the mappings are static, allowing
        // us to only emit one marker pair per new page seen (per thread to avoid
        // locks).
        // XXX: Add spot-checks of mapping changes via a separate option from
        // -virt2phys_freq?
        if (from_cache)
            return v2p_ptr;
        // We have something to emit.  Rather than a memmove to insert inside the
        // main buffer, we have a separate buffer, as our pair of markers means we
        // do not need precise placement next to the corresponding regular entry
        // (which also avoids extra work in raw2trace, esp for delayed branches and
        // other cases).
        // The downside is that we might have many buffers with a small number
        // of markers on which we waste buffer output overhead.
        // XXX: We could count them up and do a memmove if the count is small
        // and we have space in the redzone?
        if (!*emitted) {
            // We need to be sure to emit the initial thread header if this is before
            // the first regular buffer and skip it in the regular buffer.
            if (header_size > buf_hdr_slots_size) {
                size_t size =
                    reinterpret_cast<offline_instru_t *>(instru)->append_thread_header(
                        data->v2p_buf, dr_get_thread_id(drcontext), get_file_type());
                ASSERT(size == data->init_header_size, "inconsistent header");
                *skip = data->init_header_size;
                v2p_ptr += size;
                header_size += size;
            }
            v2p_ptr += add_buffer_header(drcontext, data, v2p_ptr);
            *emitted = true;
        }
        if (v2p_ptr + 2 * instru->sizeof_entry() - data->v2p_buf >=
            static_cast<ssize_t>(get_v2p_buffer_size())) {
            NOTIFY(1, "Reached v2p buffer limit: emitting multiple times\n");
            data->num_phys_markers +=
                output_buffer(drcontext, data, data->v2p_buf, v2p_ptr, header_size);
            v2p_ptr = data->v2p_buf;
            v2p_ptr += add_buffer_header(drcontext, data, v2p_ptr);
        }
        if (success) {
            v2p_ptr +=
                instru->append_marker(v2p_ptr, TRACE_MARKER_TYPE_PHYSICAL_ADDRESS, phys);
            v2p_ptr +=
                instru->append_marker(v2p_ptr, TRACE_MARKER_TYPE_VIRTUAL_ADDRESS, virt);
        } else {
            // For translation failure, we insert a distinct marker type, so analyzers
            // know for sure and don't have to infer based on a missing marker.
            v2p_ptr += instru->append_marker(
                v2p_ptr, TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE, virt);
        }
    } else {
        // For online we replace the virtual with physical.
        // XXX i#4014: For consistency we should break compatibility, *not* replace,
        // and insert the markers instead, updating dr$sim to use the markers
        // to compute the physical addresses.  We should then update
        // https://dynamorio.org/sec_drcachesim_phys.html.
        if (success)
            instru->set_entry_addr(mem_ref, phys);
    }
    return v2p_ptr;
}

// Should be called only for -use_physical.
// Returns the byte count to skip in the trace buffer (due to shifting some headers
// to the v2p buffer).
static size_t
process_buffer_for_physaddr(void *drcontext, per_thread_t *data, size_t header_size,
                            byte *buf_ptr)
{
    ASSERT(op_use_physical.get_value(),
           "Caller must check for use_physical being enabled");
    byte *v2p_ptr = data->v2p_buf;
    size_t skip = 0;
    bool emitted = false;
    for (byte *mem_ref = data->buf_base + header_size; mem_ref < buf_ptr;
         mem_ref += instru->sizeof_entry()) {
        trace_type_t type = instru->get_entry_type(mem_ref);
        DR_ASSERT(type != TRACE_TYPE_INSTR_BUNDLE); // Bundles disabled up front.
        if (!type_has_address(type))
            continue;
        addr_t virt = instru->get_entry_addr(drcontext, mem_ref);
        v2p_ptr = process_entry_for_physaddr(drcontext, data, header_size, v2p_ptr,
                                             mem_ref, virt, type, &emitted, &skip);
        // Handle the memory reference crossing onto a second page.
        size_t page_size = dr_page_size();
        addr_t virt_page = ALIGN_BACKWARD(virt, page_size);
        size_t mem_ref_size = instru->get_entry_size(mem_ref);
        if (type_is_instr(type) || type == TRACE_TYPE_INSTR_NO_FETCH ||
            type == TRACE_TYPE_INSTR_MAYBE_FETCH) {
            int instr_count = instru->get_instr_count(mem_ref);
            if (op_offline.get_value()) {
                // We do not have the size so we have to guess.  It is ok to emit an
                // unused translation so we err on the side of caution.  We do not use
                // the maximum possible instruction sizes since for x86 that's 17 * 256
                // (max_bb_instrs) that's >4096.  The average x86 instr length is <4 but
                // we use 8 to be conservative while not as extreme as 17 which will
                // lead to too many unused markers.
                static constexpr size_t PREDICT_INSTR_SIZE_BOUND = IF_X86_ELSE(8, 4);
                mem_ref_size = instr_count * PREDICT_INSTR_SIZE_BOUND;
            } else
                ASSERT(instr_count <= 1, "bundles are disabled");
        } else if (op_offline.get_value()) {
            // For data, we again do not have the size.
            static constexpr size_t PREDICT_DATA_SIZE_BOUND = sizeof(void *);
            mem_ref_size = PREDICT_DATA_SIZE_BOUND;
        }
        if (ALIGN_BACKWARD(virt + mem_ref_size - 1 /*open-ended*/, page_size) !=
            virt_page) {
            NOTIFY(2, "Emitting physaddr for next page %p for type=%2d, addr=%p\n",
                   virt_page + page_size, type, virt);
            v2p_ptr =
                process_entry_for_physaddr(drcontext, data, header_size, v2p_ptr, mem_ref,
                                           virt_page + page_size, type, &emitted, &skip);
        }
    }
    if (emitted) {
        data->num_phys_markers +=
            output_buffer(drcontext, data, data->v2p_buf, v2p_ptr, header_size);
    }
    return skip;
}

// Should be invoked only in the middle of an active tracing window.
static void
memtrace(void *drcontext, bool skip_size_cap)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    byte *mem_ref, *buf_ptr;
    byte *redzone;
    bool do_write = true;
    size_t header_size = 0;
    uint current_num_refs = 0;

    if (op_offline.get_value() && data->file == INVALID_FILE) {
        // We've delayed opening a new window file to avoid an empty final file.
        DR_ASSERT(has_tracing_windows() || op_trace_after_instrs.get_value() > 0);
        open_new_thread_file(drcontext, get_local_window(data));
    }

    buf_ptr = BUF_PTR(data->seg_base);
    // We may get called with nothing to write: e.g., on a syscall for
    // -L0I_filter and -L0D_filter.
    if (buf_ptr == data->buf_base + header_size + buf_hdr_slots_size) {
        if (has_tracing_windows()) {
            // If there is no data to write, we do not emit an empty header here
            // unless this thread is exiting (set_local_window will also write out
            // entries on a window change for offline; for online multiple windows
            // may pass with no output at all for an idle thread).
            set_local_window(drcontext, tracing_window.load(std::memory_order_acquire));
        }
        return;
    }

    header_size = add_buffer_header(drcontext, data, data->buf_base);

    bool window_changed = false;
    if (has_tracing_windows() &&
        get_local_window(data) != tracing_window.load(std::memory_order_acquire)) {
        // This buffer is for a prior window.  Do not add to the current window count;
        // emit under the prior window.
        DR_ASSERT(get_local_window(data) <
                  tracing_window.load(std::memory_order_acquire));
        data->cur_window_instr_count = 0;
        window_changed = true;
        // No need to append TRACE_MARKER_TYPE_WINDOW_ID: the next buffer will have
        // one in its header.
        if (op_offline.get_value() && op_split_windows.get_value())
            buf_ptr += instru->append_thread_exit(buf_ptr, dr_get_thread_id(drcontext));
    }
    if (!skip_size_cap &&
        (is_bytes_written_beyond_trace_max(data) || is_num_refs_beyond_global_max())) {
        /* We don't guarantee to match the limit exactly so we allow one buffer
         * beyond.  We also don't put much effort into reducing overhead once
         * beyond the limit: we still instrument and come here.
         */
        do_write = false;
        if (is_num_refs_beyond_global_max()) {
            /* std::atomic *should* be safe (we can assert std::atomic_is_lock_free())
             * but to avoid any risk we use DR's atomics.
             * Update: we are now using std::atomic for some new variables.
             */
            if (dr_atomic_load32(&notify_beyond_global_max_once) == 0) {
                int count = dr_atomic_add32_return_sum(&notify_beyond_global_max_once, 1);
                if (count == 1) {
                    NOTIFY(0, "Hit -max_global_trace_refs: disabling tracing.\n");
                    // We're not detaching, so the app keeps running and we don't flush
                    // thread buffers or emit thread exits until the app exits.  To avoid
                    // huge time gaps we use the current timestamp for all future
                    // entries.  (An alternative would be a suspsend-the-world now and
                    // flush-and-exit all threads; a better solution for most use cases
                    // is probably i#5022: -detach_after_tracing.)
                    instru->set_frozen_timestamp(instru_t::get_timestamp());
                }
            }
        }
    }

    if (do_write) {
        bool hit_window_end = false;
        if (op_trace_for_instrs.get_value() > 0) {
            for (mem_ref = data->buf_base + header_size; mem_ref < buf_ptr;
                 mem_ref += instru->sizeof_entry()) {
                if (!window_changed && !hit_window_end &&
                    op_trace_for_instrs.get_value() > 0) {
                    hit_window_end =
                        count_traced_instrs(drcontext, instru->get_instr_count(mem_ref));
                    // We have to finish this buffer so we'll go a little beyond the
                    // precise requested window length.
                    // XXX: For small windows this may be significant: we could go
                    // ~5K beyond if we hit the threshold near the start of a full buffer.
                    // Should we discard the rest of the entries in such a case, at
                    // a block boundary, even though we already collected them?
                }
            }
            if (hit_window_end) {
                if (op_offline.get_value() && op_split_windows.get_value()) {
                    size_t add =
                        instru->append_thread_exit(buf_ptr, dr_get_thread_id(drcontext));
                    buf_ptr += add;
                }
                // Update the global window, but not the local so we can place the rest
                // of this buffer into the same local window.
                reached_traced_instrs_threshold(drcontext);
            }
        }
        size_t skip = 0;
        if (op_use_physical.get_value()) {
            skip = process_buffer_for_physaddr(drcontext, data, header_size, buf_ptr);
        }
        current_num_refs +=
            output_buffer(drcontext, data, data->buf_base + skip, buf_ptr, header_size);
    }

    if (file_ops_func.handoff_buf == NULL) {
        // Our instrumentation reads from buffer and skips the clean call if the
        // content is 0, so we need set zero in the trace buffer and set non-zero
        // in redzone.
        memset(data->buf_base, 0, trace_buf_size);
        redzone = data->buf_base + trace_buf_size;
        if (buf_ptr > redzone) {
            // Set sentinel (non-zero) value in redzone
            memset(redzone, -1, buf_ptr - redzone);
        }
    }
    BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
    num_refs_racy += current_num_refs;
    if (op_exit_after_tracing.get_value() > 0 &&
        num_refs_racy > op_exit_after_tracing.get_value()) {
        dr_mutex_lock(mutex);
        if (!exited_process) {
            exited_process = true;
            dr_mutex_unlock(mutex);
            // XXX i#2644: we would prefer detach_after_tracing rather than exiting
            // the process but that requires a client-triggered detach so for now
            // we settle for exiting.
            NOTIFY(0, "Exiting process after ~" UINT64_FORMAT_STRING " references.\n",
                   num_refs_racy);
            dr_exit_process(0);
        }
        dr_mutex_unlock(mutex);
    }
    if (has_tracing_windows()) {
        set_local_window(drcontext, tracing_window.load(std::memory_order_acquire));
    }
}

/* clean_call sends the memory reference info to the simulator */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    memtrace(drcontext, false);
}

/***************************************************************************
 * Alternating tracing-no-tracing feature.
 */

/* We have two modes: count instructions only, and full trace.
 *
 * XXX i#3995: To implement -max_trace_size with drbbdup cases (with thread-private
 * encodings), or support nudges enabling tracing, or have a single -trace_for_instrs
 * transition to something lower-cost than counting, we will likely add a 3rd mode that
 * has zero instrumentation.  We also would use the 3rd mode for just -trace_for_instrs
 * with no -retrace_every_instrs.  For now we have just 2 as the case dispatch is more
 * efficient that way.
 */
enum {
    BBDUP_MODE_TRACE = 0,
    BBDUP_MODE_COUNT = 1,
};

#if defined(X86_64) || defined(AARCH64)
#    define DELAYED_CHECK_INLINED 1
#else
/* XXX we don't have the inlining implemented yet for 32-bit architectures. */
#endif

/* This holds one of the BBDUP_MODE_ enum values, but drbbdup requires that it
 * be pointer-sized.
 */
static std::atomic<ptr_int_t> tracing_disabled;

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data);

static dr_emit_flags_t
event_bb_analysis_cleanup(void *drcontext, void *user_data);

static dr_emit_flags_t
event_inscount_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                           bool translating, void **user_data);

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      instr_t *where, bool for_trace, bool translating,
                      void *orig_analysis_data, void *user_data);
static dr_emit_flags_t
event_inscount_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                               instr_t *instr, instr_t *where, bool for_trace,
                               bool translating, void *orig_analysis_data,
                               void *user_data);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating);

static bool
event_filter_syscall(void *drcontext, int sysnum);

static bool
event_pre_syscall(void *drcontext, int sysnum);

static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info);

// Returns whether we've reached the end of this tracing window.
static bool
count_traced_instrs(void *drcontext, int toadd)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    data->cur_window_instr_count += toadd;
    if (data->cur_window_instr_count >= local_instr_count_threshold()) {
        uint64 newval = cur_window_instr_count.fetch_add(data->cur_window_instr_count,
                                                         std::memory_order_release) +
            // fetch_add returns old value.
            data->cur_window_instr_count;
        data->cur_window_instr_count = 0;
        if (newval >= op_trace_for_instrs.get_value())
            return true;
    }
    return false;
}

// Does not update the local window.
static void
reached_traced_instrs_threshold(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_mutex_lock(mutex);
    if (get_local_window(data) != tracing_window.load(std::memory_order_acquire)) {
        // Another thread already changed the mode.
        dr_mutex_unlock(mutex);
        return;
    }
    // We've reached the end of our window.
    // We do not attempt a proactive synchronous flush of other threads'
    // buffers, relying on our end-of-block check for a mode change.
    // (If -retrace_every_instrs is not set and we're not going to trace
    // again, we still use a counting mode for simplicity of not adding
    // yet another mode.)
    NOTIFY(0, "Hit tracing window #%zd limit: disabling tracing.\n",
           tracing_window.load(std::memory_order_acquire));
    // No need to append TRACE_MARKER_TYPE_WINDOW_ID: the next buffer will have
    // one in its header.
    // If we're counting at exit time, this increment means that the thread
    // exit entries will be the only ones in this new window: but that seems
    // reasonable.
    tracing_window.fetch_add(1, std::memory_order_release);
    // We delay creating a new ouput dir until tracing is enabled again, to avoid
    // an empty final dir.
    DR_ASSERT(tracing_disabled.load(std::memory_order_acquire) == BBDUP_MODE_TRACE);
    tracing_disabled.store(BBDUP_MODE_COUNT, std::memory_order_release);
    cur_window_instr_count.store(0, std::memory_order_release);
    dr_mutex_unlock(mutex);
}

static uintptr_t
event_bb_setup(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    DR_ASSERT(enable_dups != NULL && enable_dynamic_handling != NULL);
    if (bbdup_duplication_enabled()) {
        *enable_dups = true;
        drbbdup_status_t res =
            drbbdup_register_case_encoding(drbbdup_ctx, BBDUP_MODE_COUNT);
        DR_ASSERT(res == DRBBDUP_SUCCESS);
    } else {
        /* Tracing is always on, so we have just one type of instrumentation and
         * do not need block duplication.
         */
        *enable_dups = false;
    }
    *enable_dynamic_handling = false;
    return BBDUP_MODE_TRACE;
}

static void
event_bb_retrieve_mode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                       void *user_data, void *orig_analysis_data)
{
    /* Nothing to do.  We would pass nullptr for this but drbbdup makes it required. */
}

static bool
is_first_nonlabel(void *drcontext, instr_t *instr)
{
    bool is_first_nonlabel = false;
    if (drbbdup_is_first_nonlabel_instr(drcontext, instr, &is_first_nonlabel) !=
        DRBBDUP_SUCCESS)
        DR_ASSERT(false);
    return is_first_nonlabel;
}

static dr_emit_flags_t
event_bb_analyze_case(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, uintptr_t mode, void *user_data,
                      void *orig_analysis_data, void **analysis_data)
{
    if (mode == BBDUP_MODE_TRACE) {
        return event_bb_analysis(drcontext, tag, bb, for_trace, translating,
                                 analysis_data);
    } else if (mode == BBDUP_MODE_COUNT) {
        return event_inscount_bb_analysis(drcontext, tag, bb, for_trace, translating,
                                          analysis_data);
    } else
        DR_ASSERT(false);
    return DR_EMIT_DEFAULT;
}

static void
event_bb_analyze_case_cleanup(void *drcontext, uintptr_t mode, void *user_data,
                              void *orig_analysis_data, void *analysis_data)
{
    if (mode == BBDUP_MODE_TRACE)
        event_bb_analysis_cleanup(drcontext, analysis_data);
    else if (mode == BBDUP_MODE_COUNT)
        ; /* no cleanup needed */
    else
        DR_ASSERT(false);
}

static dr_emit_flags_t
event_app_instruction_case(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                           instr_t *where, bool for_trace, bool translating,
                           uintptr_t mode, void *user_data, void *orig_analysis_data,
                           void *analysis_data)
{
    if (mode == BBDUP_MODE_TRACE) {
        return event_app_instruction(drcontext, tag, bb, instr, where, for_trace,
                                     translating, orig_analysis_data, analysis_data);
    } else if (mode == BBDUP_MODE_COUNT) {
        return event_inscount_app_instruction(drcontext, tag, bb, instr, where, for_trace,
                                              translating, orig_analysis_data,
                                              analysis_data);
    } else
        DR_ASSERT(false);
    return DR_EMIT_DEFAULT;
}

static void
instrumentation_exit()
{
    dr_unregister_filter_syscall_event(event_filter_syscall);
    if (!drmgr_unregister_pre_syscall_event(event_pre_syscall) ||
        !drmgr_unregister_kernel_xfer_event(event_kernel_xfer) ||
        !drmgr_unregister_bb_app2app_event(event_bb_app2app))
        DR_ASSERT(false);
#ifdef DELAYED_CHECK_INLINED
    drx_exit();
#endif
    drbbdup_status_t res = drbbdup_exit();
    DR_ASSERT(res == DRBBDUP_SUCCESS);
}

static void
instrumentation_drbbdup_init()
{
    drbbdup_options_t opts = {
        sizeof(opts),
    };
    opts.set_up_bb_dups = event_bb_setup;
    opts.insert_encode = event_bb_retrieve_mode;
    opts.analyze_case_ex = event_bb_analyze_case;
    opts.destroy_case_analysis = event_bb_analyze_case_cleanup;
    opts.instrument_instr_ex = event_app_instruction_case;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&tracing_disabled, OPSZ_PTR);
    opts.atomic_load_encoding = true;
    if (bbdup_duplication_enabled())
        opts.non_default_case_limit = 1;
    else {
        // Save memory by asking drbbdup to not keep per-block bookkeeping.
        opts.non_default_case_limit = 0;
    }
    // Save per-thread heap for a feature we do not need.
    opts.never_enable_dynamic_handling = true;
    drbbdup_status_t res = drbbdup_init(&opts);
    DR_ASSERT(res == DRBBDUP_SUCCESS);
    /* We just want barriers and atomic ops: no locks b/c they are not safe. */
    DR_ASSERT(tracing_disabled.is_lock_free());
    DR_ASSERT(cur_window_instr_count.is_lock_free());
}

static void
instrumentation_init()
{
    instrumentation_drbbdup_init();
    if (!drmgr_register_pre_syscall_event(event_pre_syscall) ||
        !drmgr_register_kernel_xfer_event(event_kernel_xfer) ||
        !drmgr_register_bb_app2app_event(event_bb_app2app, &pri_pre_bbdup))
        DR_ASSERT(false);
    dr_register_filter_syscall_event(event_filter_syscall);

    if (op_trace_after_instrs.get_value() != 0)
        tracing_disabled.store(BBDUP_MODE_COUNT, std::memory_order_release);

#ifdef DELAYED_CHECK_INLINED
    drx_init();
#endif
}

/***************************************************************************
 * Tracing instrumentation.
 */

static void
append_marker_seg_base(void *drcontext, func_trace_entry_vector_t *vec)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (BUF_PTR(data->seg_base) == NULL)
        return; /* This thread was filtered out. */
    for (int i = 0; i < vec->size; i++) {
        BUF_PTR(data->seg_base) +=
            instru->append_marker(BUF_PTR(data->seg_base), vec->entries[i].marker_type,
                                  vec->entries[i].marker_value);
    }
    /* In a filtered data-only trace, a block with no memrefs today still has
     * a redzone check at the end guarding a clean call to memtrace(), but to
     * be a litte safer in case that changes we also do a redzone check here.
     */
    if (BUF_PTR(data->seg_base) - data->buf_base > static_cast<ssize_t>(trace_buf_size))
        memtrace(drcontext, false);
}

static void
insert_load_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_BUF_PTR,
                           reg_ptr);
}

static void
insert_update_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr, dr_pred_type_t pred, int adjust)
{
    if (adjust == 0)
        return;
    if (!(op_L0I_filter.get_value() ||
          op_L0D_filter.get_value())) // Filter skips over this for !pred.
        instrlist_set_auto_predicate(ilist, pred);
    MINSERT(
        ilist, where,
        XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                            tls_offs + MEMTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
}

static int
instrument_delay_instrs(void *drcontext, void *tag, instrlist_t *ilist, user_data_t *ud,
                        instr_t *where, reg_id_t reg_ptr, int adjust)
{
    // Instrument to add a full instr entry for the first instr.
    adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field, ilist, where,
                                      reg_ptr, adjust, ud->delay_instrs[0]);
    if (op_use_physical.get_value()) {
        // No instr bundle if physical-2-virtual since instr bundle may
        // cross page bundary.
        int i;
        for (i = 1; i < ud->num_delay_instrs; i++) {
            adjust =
                instru->instrument_instr(drcontext, tag, &ud->instru_field, ilist, where,
                                         reg_ptr, adjust, ud->delay_instrs[i]);
        }
    } else {
        adjust =
            instru->instrument_ibundle(drcontext, ilist, where, reg_ptr, adjust,
                                       ud->delay_instrs + 1, ud->num_delay_instrs - 1);
    }
    ud->num_delay_instrs = 0;
    return adjust;
}

/* Inserts a conditional branch that jumps to skip_label if reg_skip_if_zero's
 * value is zero.
 * "*reg_tmp" must start out as DR_REG_NULL. It will hold a temp reg that must be passed
 * to any subsequent call here as well as to insert_conditional_skip_target() at
 * the point where skip_label should be inserted.  Additionally, the
 * app_regs_at_skip set must be empty prior to calling and it must be passed
 * to insert_conditional_skip_target().
 * reg_skip_if_zero must be DR_REG_XCX on x86.
 */
static void
insert_conditional_skip(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t reg_skip_if_zero, reg_id_t *reg_tmp INOUT,
                        instr_t *skip_label, bool short_reaches,
                        reg_id_set_t &app_regs_at_skip)
{
    // Record the registers that will need barriers at the skip target.
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; ++reg) {
        drreg_reserve_info_t info = { sizeof(info) };
        drreg_status_t res = drreg_reservation_info_ex(drcontext, reg, &info);
        DR_ASSERT(res == DRREG_SUCCESS);
        if (info.holds_app_value) {
            app_regs_at_skip.insert(reg);
        }
    }

#ifdef X86
    DR_ASSERT(reg_skip_if_zero == DR_REG_XCX);
    if (short_reaches) {
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(skip_label)));
    } else {
        instr_t *should_skip = INSTR_CREATE_label(drcontext);
        instr_t *no_skip = INSTR_CREATE_label(drcontext);
        MINSERT(ilist, where,
                INSTR_CREATE_jecxz(drcontext, opnd_create_instr(should_skip)));
        MINSERT(ilist, where,
                INSTR_CREATE_jmp_short(drcontext, opnd_create_instr(no_skip)));
        /* XXX i#2825: we need this to not match instr_is_cti_short_rewrite() */
        MINSERT(ilist, where, INSTR_CREATE_nop(drcontext));
        MINSERT(ilist, where, should_skip);
        MINSERT(ilist, where, INSTR_CREATE_jmp(drcontext, opnd_create_instr(skip_label)));
        MINSERT(ilist, where, no_skip);
    }
#elif defined(ARM)
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB) {
        instr_t *noskip = INSTR_CREATE_label(drcontext);
        /* XXX: clean call is too long to use cbz to skip. */
        DR_ASSERT(reg_skip_if_zero <= DR_REG_R7); /* cbnz can't take r8+ */
        MINSERT(ilist, where,
                INSTR_CREATE_cbnz(drcontext, opnd_create_instr(noskip),
                                  opnd_create_reg(reg_skip_if_zero)));
        MINSERT(ilist, where,
                XINST_CREATE_jump(drcontext, opnd_create_instr(skip_label)));
        MINSERT(ilist, where, noskip);
    } else {
        /* There is no jecxz/cbz like instr on ARM-A32 mode, so we have to
         * save aflags to a temp reg before the cmp.
         * XXX optimization: use drreg to avoid aflags save/restore.
         */
        if (*reg_tmp != DR_REG_NULL) {
            /* A prior call has already saved the flags. */
        } else {
            if (drreg_reserve_register(drcontext, ilist, where, &scratch_reserve_vec,
                                       reg_tmp) != DRREG_SUCCESS)
                FATAL("Fatal error: failed to reserve reg.");
            dr_save_arith_flags_to_reg(drcontext, ilist, where, *reg_tmp);
        }
        MINSERT(ilist, where,
                INSTR_CREATE_cmp(drcontext, opnd_create_reg(reg_skip_if_zero),
                                 OPND_CREATE_INT(0)));
        MINSERT(
            ilist, where,
            instr_set_predicate(
                XINST_CREATE_jump(drcontext, opnd_create_instr(skip_label)), DR_PRED_EQ));
    }
#elif defined(AARCH64)
    MINSERT(ilist, where,
            INSTR_CREATE_cbz(drcontext, opnd_create_instr(skip_label),
                             opnd_create_reg(reg_skip_if_zero)));
#endif
}

/* Should be called at the point where skip_label should be inserted.
 * reg_tmp must be the "*reg_tmp" output value from insert_conditional_skip().
 * Inserts a barrier for all app-valued registers at the jump point
 * (stored in app_regs_at_skip), to help avoid problems with different
 * paths having different lazy reg restoring from drreg.
 */
static void
insert_conditional_skip_target(void *drcontext, instrlist_t *ilist, instr_t *where,
                               instr_t *skip_label, reg_id_t reg_tmp,
                               reg_id_set_t &app_regs_at_skip)
{
    for (reg_id_t reg = DR_REG_START_GPR; reg <= DR_REG_STOP_GPR; ++reg) {
        if (app_regs_at_skip.find(reg) != app_regs_at_skip.end() &&
            /* We spilled reg_tmp in insert_conditional_skip() *before* the jump,
             * so we do *not* want to restore it before the target.
             */
            reg != reg_tmp &&
            /* We're not allowed to restore the stolen register this way, but drreg
             * won't hand it out as a scratch register in any case, so we don't need
             * a barrier for it.
             */
            reg != dr_get_stolen_reg()) {
            drreg_status_t res = drreg_get_app_value(drcontext, ilist, where, reg, reg);
            if (res != DRREG_ERROR_NO_APP_VALUE && res != DRREG_SUCCESS)
                FATAL("Fatal error: failed to restore reg.");
        }
    }
    MINSERT(ilist, where, skip_label);
#ifdef ARM
    if (reg_tmp != DR_REG_NULL) {
        dr_restore_arith_flags_from_reg(drcontext, ilist, where, reg_tmp);
        if (drreg_unreserve_register(drcontext, ilist, where, reg_tmp) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to unreserve reg.\n");
    }
#endif
}

/* We insert code to read from trace buffer and check whether the redzone
 * is reached. If redzone is reached, the clean call will be called.
 * Additionally, for tracing windows, we also check for a mode switch and
 * invoke the clean call if our tracing window is over.
 */
static void
instrument_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      reg_id_t reg_ptr)
{
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    bool short_reaches = true;
#ifdef X86
    DR_ASSERT(reg_ptr == DR_REG_XCX);
    /* i#2049: we use DR_CLEANCALL_ALWAYS_OUT_OF_LINE to ensure our jecxz
     * reaches across the clean call (o/w we need 2 jmps to invert the jecxz).
     * Long-term we should try a fault instead (xref drx_buf) or a lean
     * proc to clean call gencode.
     */
    /* i#2147: -prof_pcs adds extra cleancall code that makes jecxz not reach.
     * XXX: it would be nice to have a more robust solution than this explicit check!
     */
    if (dr_is_tracking_where_am_i())
        short_reaches = false;
#elif defined(ARM)
    /* XXX: clean call is too long to use cbz to skip. */
    short_reaches = false;
#endif

    if (has_tracing_windows()) {
        // We need to do the clean call if the mode has changed back to counting.  To
        // detect a double-change we compare the TLS-stored last window to the
        // current tracing_window.  To avoid flags and avoid another branch (jumping
        // over the filter and redzone checks, e.g.), our strategy is to arrange for
        // the redzone load to trigger the call for us if the two window values are
        // not equal by writing their difference onto the next buffer slot.  This
        // requires a store and two scratch regs so perhaps this should be measured
        // against a branch-based scheme, but we assume we're i/o bound and so this will
        // not affect overhead.
        reg_id_t reg_mine = DR_REG_NULL, reg_global = DR_REG_NULL;
        if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_mine) !=
                DRREG_SUCCESS ||
            drreg_reserve_register(drcontext, ilist, where, NULL, &reg_global) !=
                DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve reg.");
#ifdef AARCHXX
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)&tracing_window,
                                         opnd_create_reg(reg_global), ilist, where, NULL,
                                         NULL);
#    ifdef AARCH64
        MINSERT(ilist, where,
                INSTR_CREATE_ldar(drcontext, opnd_create_reg(reg_global),
                                  OPND_CREATE_MEMPTR(reg_global, 0)));
#    else
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                                  OPND_CREATE_MEMPTR(reg_global, 0)));
        MINSERT(ilist, where, INSTR_CREATE_dmb(drcontext, OPND_CREATE_INT(DR_DMB_ISH)));
#    endif
#else
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                                  OPND_CREATE_ABSMEM(&tracing_window, OPSZ_PTR)));
#endif
        dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                               tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_WINDOW,
                               reg_mine);
#ifdef AARCHXX
        MINSERT(ilist, where,
                XINST_CREATE_sub(drcontext, opnd_create_reg(reg_mine),
                                 opnd_create_reg(reg_global)));
#else
        // Our version of a flags-free reg-reg subtraction: 1's complement one reg
        // plus 1 and then add using base+index of LEA.
        MINSERT(ilist, where, INSTR_CREATE_not(drcontext, opnd_create_reg(reg_global)));
        MINSERT(ilist, where,
                INSTR_CREATE_lea(drcontext, opnd_create_reg(reg_global),
                                 OPND_CREATE_MEM_lea(reg_global, DR_REG_NULL, 0, 1)));
        MINSERT(ilist, where,
                INSTR_CREATE_lea(drcontext, opnd_create_reg(reg_mine),
                                 OPND_CREATE_MEM_lea(reg_mine, reg_global, 1, 0)));
#endif
        // To avoid writing a 0 on top of the redzone, we read the buffer value and add
        // that to the local ("mine") window minus the global window.  The redzone is
        // -1, so if we do mine minus global which will always be non-positive, we'll
        // never write 0 for a redzone slot (and thus possibly overflowing).
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(reg_global),
                                  OPND_CREATE_MEMPTR(reg_ptr, 0)));
        MINSERT(ilist, where,
                XINST_CREATE_add(drcontext, opnd_create_reg(reg_mine),
                                 opnd_create_reg(reg_global)));
        MINSERT(ilist, where,
                XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, 0),
                                   opnd_create_reg(reg_mine)));
        if (drreg_unreserve_register(drcontext, ilist, where, reg_global) !=
                DRREG_SUCCESS ||
            drreg_unreserve_register(drcontext, ilist, where, reg_mine) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to unreserve scratch reg.\n");
    }

    reg_id_t reg_tmp = DR_REG_NULL;
    instr_t *skip_thread = INSTR_CREATE_label(drcontext);
    reg_id_set_t app_regs_at_skip_thread;
    if ((op_L0I_filter.get_value() || op_L0D_filter.get_value()) &&
        thread_filtering_enabled) {
        insert_conditional_skip(drcontext, ilist, where, reg_ptr, &reg_tmp, skip_thread,
                                short_reaches, app_regs_at_skip_thread);
    }
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_ptr),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
    reg_id_set_t app_regs_at_skip_call;
    insert_conditional_skip(drcontext, ilist, where, reg_ptr, &reg_tmp, skip_call,
                            short_reaches, app_regs_at_skip_call);

    dr_insert_clean_call_ex(drcontext, ilist, where, (void *)clean_call,
                            DR_CLEANCALL_ALWAYS_OUT_OF_LINE, 0);
    insert_conditional_skip_target(drcontext, ilist, where, skip_call, reg_tmp,
                                   app_regs_at_skip_call);
    insert_conditional_skip_target(drcontext, ilist, where, skip_thread, reg_tmp,
                                   app_regs_at_skip_thread);
}

// Called before writing to the trace buffer.
// reg_ptr is treated as scratch and may be clobbered by this routine.
// Returns DR_REG_NULL to indicate *not* to insert the instrumentation to
// write to the trace buffer.  Otherwise, returns a register that the caller
// must restore *after* the skip target.  The caller must also restore the
// aflags after the skip target.  (This is for parity on all paths per drreg
// limitations.)
static reg_id_t
insert_filter_addr(void *drcontext, instrlist_t *ilist, instr_t *where, user_data_t *ud,
                   reg_id_t reg_ptr, opnd_t ref, instr_t *app, instr_t *skip,
                   dr_pred_type_t pred)
{
    // Our "level 0" inlined direct-mapped cache filter.
    DR_ASSERT(op_L0I_filter.get_value() || op_L0D_filter.get_value());
    reg_id_t reg_idx;
    bool is_icache = opnd_is_null(ref);
    uint64 cache_size = is_icache ? op_L0I_size.get_value() : op_L0D_size.get_value();
    if (cache_size == 0)
        return DR_REG_NULL; // Skip instru.
    ptr_int_t mask = (ptr_int_t)(cache_size / op_line_size.get_value()) - 1;
    int line_bits = compute_log2(op_line_size.get_value());
    uint offs = is_icache ? MEMTRACE_TLS_OFFS_ICACHE : MEMTRACE_TLS_OFFS_DCACHE;
    reg_id_t reg_addr;
    if (is_icache) {
        // For filtering the icache, we disable bundles + delays and call here on
        // every instr.  We skip if we're still on the same cache line.
        if (ud->last_app_pc != NULL) {
            ptr_uint_t prior_line = ((ptr_uint_t)ud->last_app_pc >> line_bits) & mask;
            // FIXME i#2439: we simplify and ignore a 2nd cache line touched by an
            // instr that straddles cache lines.  However, that is not uncommon on
            // x86 and we should check the L0 cache for both lines, do regular instru
            // if either misses, and have some flag telling the regular instru to
            // only do half the instr if only one missed (for offline this flag would
            // have to propagate to raw2trace; for online we could use a mid-instr PC
            // and size).
            ptr_uint_t new_line = ((ptr_uint_t)instr_get_app_pc(app) >> line_bits) & mask;
            if (prior_line == new_line)
                return DR_REG_NULL; // Skip instru.
        }
        ud->last_app_pc = instr_get_app_pc(app);
    }
    if (drreg_reserve_register(drcontext, ilist, where, &scratch_reserve_vec,
                               &reg_addr) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve scratch reg\n");
    if (drreg_reserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve aflags\n");
    // We need a 3rd scratch register.  We can avoid clobbering the app address
    // if we either get a 4th scratch or keep re-computing the tag and the mask
    // but it's better to keep the common path shorter, so we clobber reg_addr
    // with the tag and recompute on a miss.
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_idx) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to reserve 3rd scratch register\n");
#ifdef ARM
    if (instr_predicate_is_cond(pred)) {
        // We can't mark everything as predicated b/c we have a cond branch.
        // Instead we jump over it if the memref won't be executed.
        // We have to do that after spilling the regs for parity on all paths.
        // This means we don't have to restore app flags for later predicate prefixes.
        MINSERT(ilist, where,
                XINST_CREATE_jump_cond(drcontext, instr_invert_predicate(pred),
                                       opnd_create_instr(skip)));
    }
#endif
    if (thread_filtering_enabled) {
        // Similarly to the predication case, we need reg parity, so we incur
        // the cost of spilling the regs before we skip for this thread so the
        // lazy restores are the same on all paths.
        // XXX: do better!
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
        MINSERT(
            ilist, where,
            XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT32(0)));
        MINSERT(ilist, where,
                XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(skip)));
    }
    // First get the cache slot and load what's currently stored there.
    // XXX i#2439: we simplify and ignore a memref that straddles cache lines.
    // That will only happen for unaligned accesses.
    if (is_icache) {
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)instr_get_app_pc(app),
                                         opnd_create_reg(reg_addr), ilist, where, NULL,
                                         NULL);
    } else
        instru->insert_obtain_addr(drcontext, ilist, where, reg_addr, reg_ptr, ref);
    MINSERT(ilist, where,
            XINST_CREATE_slr_s(drcontext, opnd_create_reg(reg_addr),
                               OPND_CREATE_INT8(line_bits)));
    MINSERT(ilist, where,
            XINST_CREATE_move(drcontext, opnd_create_reg(reg_idx),
                              opnd_create_reg(reg_addr)));
#ifndef X86
    /* Unfortunately the mask is likely too big for an immediate (32K cache and
     * 64-byte line => 0x1ff mask, and A32 and T32 have an 8-bit limit).
     */
    MINSERT(ilist, where,
            XINST_CREATE_load_int(drcontext, opnd_create_reg(reg_ptr),
                                  OPND_CREATE_INT32(mask)));
#endif
    MINSERT(ilist, where,
            XINST_CREATE_and_s(
                drcontext, opnd_create_reg(reg_idx),
                IF_X86_ELSE(OPND_CREATE_INT32(mask), opnd_create_reg(reg_ptr))));
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                           tls_offs + sizeof(void *) * offs, reg_ptr);
    // While we can load from a base reg + scaled index reg on x86 and arm, we
    // have to clobber the index reg as the dest, and we need the final address again
    // to store on a miss.  Thus we take a step to compute the final
    // cache addr in a register.
    MINSERT(ilist, where,
            XINST_CREATE_add_sll(drcontext, opnd_create_reg(reg_ptr),
                                 opnd_create_reg(reg_ptr), opnd_create_reg(reg_idx),
                                 compute_log2(sizeof(app_pc))));
    MINSERT(ilist, where,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg_idx),
                              OPND_CREATE_MEMPTR(reg_ptr, 0)));
    // Now see whether it's a hit or a miss.
    MINSERT(
        ilist, where,
        XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_idx), opnd_create_reg(reg_addr)));
    MINSERT(ilist, where,
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(skip)));
    // On a miss, replace the cache entry with the new cache line.
    MINSERT(ilist, where,
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(reg_ptr, 0),
                               opnd_create_reg(reg_addr)));

    // Restore app value b/c the caller will re-compute the app addr.
    // We can avoid clobbering the app address if we either get a 4th scratch or
    // keep re-computing the tag and the mask but it's better to keep the common
    // path shorter, so we clobber reg_addr with the tag and recompute on a miss.
    if (!is_icache && opnd_uses_reg(ref, reg_idx))
        drreg_get_app_value(drcontext, ilist, where, reg_idx, reg_idx);
    if (drreg_unreserve_register(drcontext, ilist, where, reg_addr) != DRREG_SUCCESS)
        FATAL("Fatal error: failed to unreserve scratch reg\n");
    return reg_idx;
}

static int
instrument_memref(void *drcontext, user_data_t *ud, instrlist_t *ilist, instr_t *where,
                  reg_id_t reg_ptr, int adjust, instr_t *app, opnd_t ref, int ref_index,
                  bool write, dr_pred_type_t pred)
{
    if (op_instr_only_trace.get_value()) {
        return adjust;
    }
    instr_t *skip = INSTR_CREATE_label(drcontext);
    reg_id_t reg_third = DR_REG_NULL;
    if (op_L0D_filter.get_value()) {
        reg_third = insert_filter_addr(drcontext, ilist, where, ud, reg_ptr, ref, NULL,
                                       skip, pred);
        if (reg_third == DR_REG_NULL) {
            instr_destroy(drcontext, skip);
            return adjust;
        }
    }
    // XXX: If we're filtering only instrs, not data, then we can possibly
    // avoid loading the buf ptr for each memref. We skip this optimization for
    // now for simplicity.
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value())
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    adjust = instru->instrument_memref(drcontext, ilist, where, reg_ptr, adjust, app, ref,
                                       ref_index, write, pred);
    if ((op_L0I_filter.get_value() || op_L0D_filter.get_value()) && adjust != 0) {
        // When filtering we can't combine buf_ptr adjustments.
        insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, pred, adjust);
        adjust = 0;
    }
    MINSERT(ilist, where, skip);
    if (op_L0D_filter.get_value()) {
        // drreg requires parity on all paths, so we need to restore the scratch regs
        // for the filter *after* the skip target.
        if (reg_third != DR_REG_NULL &&
            drreg_unreserve_register(drcontext, ilist, where, reg_third) != DRREG_SUCCESS)
            DR_ASSERT(false);
        if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
    return adjust;
}

static int
instrument_instr(void *drcontext, void *tag, user_data_t *ud, instrlist_t *ilist,
                 instr_t *where, reg_id_t reg_ptr, int adjust, instr_t *app)
{
    instr_t *skip = INSTR_CREATE_label(drcontext);
    reg_id_t reg_third = DR_REG_NULL;
    if (op_L0I_filter.get_value()) {
        // Count dynamic instructions per thread.
        // It is too expensive to increment per instruction, so we increment once
        // per block by the instruction count for that block.
        if (!ud->recorded_instr_count) {
            ud->recorded_instr_count = true;
            // On x86 we could do this in one instruction if we clobber the flags: but
            // then we'd have to preserve the flags before our same-line skip in
            // insert_filter_addr().
            dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg,
                                   tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNT,
                                   reg_ptr);
            MINSERT(ilist, where,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr),
                                     OPND_CREATE_INT16(ud->bb_instr_count)));
            dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg,
                                    tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNT,
                                    reg_ptr);
        }
        reg_third = insert_filter_addr(drcontext, ilist, where, ud, reg_ptr,
                                       opnd_create_null(), app, skip, DR_PRED_NONE);
        if (reg_third == DR_REG_NULL) {
            instr_destroy(drcontext, skip);
            return adjust;
        }
    }
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value()) // Else already loaded.
        insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    adjust = instru->instrument_instr(drcontext, tag, &ud->instru_field, ilist, where,
                                      reg_ptr, adjust, app);
    if ((op_L0I_filter.get_value() || op_L0D_filter.get_value()) && adjust != 0) {
        // When filtering we can't combine buf_ptr adjustments.
        insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, DR_PRED_NONE, adjust);
        adjust = 0;
    }
    MINSERT(ilist, where, skip);
    if (op_L0I_filter.get_value()) {
        // drreg requires parity on all paths, so we need to restore the scratch regs
        // for the filter *after* the skip target.
        if (reg_third != DR_REG_NULL &&
            drreg_unreserve_register(drcontext, ilist, where, reg_third) != DRREG_SUCCESS)
            DR_ASSERT(false);
        if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
    return adjust;
}

static bool
is_last_instr(void *drcontext, instr_t *instr)
{
    bool is_last = false;
    return drbbdup_is_last_instr(drcontext, instr, &is_last) == DRBBDUP_SUCCESS &&
        is_last;
}

/* For each memory reference app instr, we insert inline code to fill the buffer
 * with an instruction entry and memory reference entries.
 */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      instr_t *where, bool for_trace, bool translating,
                      void *orig_analysis_data, void *user_data)
{
    int i, adjust = 0;
    user_data_t *ud = (user_data_t *)user_data;
    reg_id_t reg_ptr;
    drvector_t rvec;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;

    // We need drwrap's instrumentation to go first so that function trace
    // entries will not be appended to the middle of a BB's PC and Memory Access
    // trace entries. Assumption made here is that every drwrap function pre/post
    // callback always happens at the first instruction of a BB.
    dr_emit_flags_t func_flags = func_trace_enabled_instrument_event(
        drcontext, tag, bb, instr, where, for_trace, translating, NULL);
    flags = static_cast<dr_emit_flags_t>(flags | func_flags);

    drmgr_disable_auto_predication(drcontext, bb);

    if ((op_L0I_filter.get_value() || op_L0D_filter.get_value()) && ud->repstr &&
        is_first_nonlabel(drcontext, instr)) {
        // XXX: the control flow added for repstr ends up jumping over the
        // aflags spill for the memref, yet it hits the lazily-delayed aflags
        // restore.  We don't have a great solution (repstr violates drreg's
        // symmetric-paths requirement) so we work around it by forcing a
        // spill up front before the internal jump.
        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags\n");
        if (drreg_unreserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags\n");
    }

    // Use emulation-aware queries to accurately trace rep string and
    // scatter-gather expansions.  Getting this wrong can result in significantly
    // incorrect ifetch stats (i#2011).
    instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
    instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
    if (instr_fetch == NULL &&
        (instr_operands == NULL ||
         !(instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) &&
        // Ensure we reach the code below for post-strex instru.
        ud->strex == NULL &&
        // Avoid dropping trailing bundled instrs or missing the block-final clean call.
        !is_last_instr(drcontext, instr))
        return flags;

    // i#1698: there are constraints for code between ldrex/strex pairs.
    // Normally DR will have done its -ldstex2cas, but in case that's disabled,
    // we attempt to handle ldrex/strex pairs here as a best-effort attempt.
    // However there is no way to completely avoid the instrumentation in between,
    // so we reduce the instrumentation in between by moving strex instru
    // from before the strex to after the strex.
    instr_t *strex = NULL;
    if (instr_fetch != NULL && instr_is_exclusive_store(instr_fetch))
        strex = instr_fetch;
    else if (instr_operands != NULL && instr_is_exclusive_store(instr_operands))
        strex = instr_operands;
    if (ud->strex == NULL && strex != NULL) {
        opnd_t dst = instr_get_dst(strex, 0);
        DR_ASSERT(opnd_is_base_disp(dst));
        if (!instr_writes_to_reg(strex, opnd_get_base(dst), DR_QUERY_INCLUDE_COND_DSTS)) {
            ud->strex = strex;
            ud->last_app_pc = instr_get_app_pc(strex);
        } else {
            NOTIFY(0,
                   "Exclusive store clobbering base not supported: skipping address\n");
        }
        if (is_last_instr(drcontext, instr)) {
            // We need our block-final call below.
            NOTIFY(0, "Block-final exclusive store: may hang");
            ud->strex = NULL;
        } else
            return flags;
    } else if (strex != NULL) {
        // Assuming there are no consecutive strex instructions, otherwise we
        // will insert instrumentation code at the second strex instruction.
        NOTIFY(0, "Consecutive exclusive stores not supported: may hang.");
    }

    // Optimization: delay the simple instr trace instrumentation if possible.
    // For offline traces we want a single instr entry for the start of the bb.
    if (instr_fetch != NULL && (!op_offline.get_value() || ud->recorded_instr) &&
        (instr_operands == NULL ||
         !(instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) &&
        // Avoid dropping trailing bundled instrs or missing the block-final clean call.
        !is_last_instr(drcontext, instr) &&
        // Avoid bundling instrs whose types we separate.
        (instru_t::instr_to_instr_type(instr_fetch, ud->repstr) == TRACE_TYPE_INSTR ||
         // We avoid overhead of skipped bundling for online unless the user requested
         // instr types.  We could use different types for
         // bundle-ends-in-this-branch-type to avoid this but for now it's not worth it.
         (!op_offline.get_value() && !op_online_instr_types.get_value())) &&
        ud->strex == NULL &&
        // Don't bundle emulated instructions, as they sometimes have internal control
        // flow and other complications that could cause us to skip an instruction.
        !drmgr_in_emulation_region(drcontext, NULL) &&
        // We can't bundle with a filter.
        !(op_L0I_filter.get_value() || op_L0D_filter.get_value()) &&
        // The delay instr buffer is not full.
        ud->num_delay_instrs < MAX_NUM_DELAY_INSTRS) {
        ud->delay_instrs[ud->num_delay_instrs++] = instr_fetch;
        return flags;
    }

    /* We usually need two scratch registers, but not always, so we push the 2nd
     * out into the instru_t routines.
     * reg_ptr must be ECX or RCX for jecxz on x86, and must be <= r7 for cbnz on ARM.
     */
    drreg_init_and_fill_vector(&rvec, false);
#ifdef X86
    drreg_set_vector_entry(&rvec, DR_REG_XCX, true);
#else
    for (reg_ptr = DR_REG_R0; reg_ptr <= DR_REG_R7; reg_ptr++)
        drreg_set_vector_entry(&rvec, reg_ptr, true);
#endif
    if (drreg_reserve_register(drcontext, bb, where, &rvec, &reg_ptr) != DRREG_SUCCESS) {
        // We can't recover.
        FATAL("Fatal error: failed to reserve scratch registers\n");
    }
    drvector_delete(&rvec);

    /* Load buf ptr into reg_ptr, unless we're cache-filtering. */
    instr_t *skip_instru = INSTR_CREATE_label(drcontext);
    reg_id_t reg_skip = DR_REG_NULL;
    reg_id_set_t app_regs_at_skip;
    if (!(op_L0I_filter.get_value() || op_L0D_filter.get_value())) {
        insert_load_buf_ptr(drcontext, bb, where, reg_ptr);
        if (thread_filtering_enabled) {
            bool short_reaches = false;
#ifdef X86
            if (ud->num_delay_instrs == 0 && !is_last_instr(drcontext, instr)) {
                /* jecxz should reach (really we want "smart jecxz" automation here) */
                short_reaches = true;
            }
#endif
            insert_conditional_skip(drcontext, bb, where, reg_ptr, &reg_skip, skip_instru,
                                    short_reaches, app_regs_at_skip);
        }
    }

    if (ud->num_delay_instrs != 0) {
        adjust = instrument_delay_instrs(drcontext, tag, bb, ud, where, reg_ptr, adjust);
    }

    if (ud->strex != NULL) {
        DR_ASSERT(instr_is_exclusive_store(ud->strex));
        adjust =
            instrument_instr(drcontext, tag, ud, bb, where, reg_ptr, adjust, ud->strex);
        adjust = instrument_memref(drcontext, ud, bb, where, reg_ptr, adjust, ud->strex,
                                   instr_get_dst(ud->strex, 0), 0, true,
                                   instr_get_predicate(ud->strex));
        ud->strex = NULL;
    }

    /* Instruction entry for instr fetch trace.  This does double-duty by
     * also providing the PC for subsequent data ref entries.
     */
    if (instr_fetch != NULL && (!op_offline.get_value() || !ud->recorded_instr)) {
        adjust =
            instrument_instr(drcontext, tag, ud, bb, where, reg_ptr, adjust, instr_fetch);
        ud->recorded_instr = true;
        ud->last_app_pc = instr_get_app_pc(instr_fetch);
    }

    /* Data entries. */
    if (instr_operands != NULL &&
        (instr_reads_memory(instr_operands) || instr_writes_memory(instr_operands))) {
        dr_pred_type_t pred = instr_get_predicate(instr_operands);
        if (pred != DR_PRED_NONE && adjust != 0) {
            // Update buffer ptr and reset adjust to 0, because
            // we may not execute the inserted code below.
            insert_update_buf_ptr(drcontext, bb, where, reg_ptr, DR_PRED_NONE, adjust);
            adjust = 0;
        }

        /* insert code to add an entry for each memory reference opnd */
        for (i = 0; i < instr_num_srcs(instr_operands); i++) {
            if (opnd_is_memory_reference(instr_get_src(instr_operands, i))) {
                adjust = instrument_memref(
                    drcontext, ud, bb, where, reg_ptr, adjust, instr_operands,
                    instr_get_src(instr_operands, i), i, false, pred);
            }
        }

        for (i = 0; i < instr_num_dsts(instr_operands); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr_operands, i))) {
                adjust = instrument_memref(
                    drcontext, ud, bb, where, reg_ptr, adjust, instr_operands,
                    instr_get_dst(instr_operands, i), i, true, pred);
            }
        }
        if (adjust != 0)
            insert_update_buf_ptr(drcontext, bb, where, reg_ptr, pred, adjust);
    } else if (adjust != 0)
        insert_update_buf_ptr(drcontext, bb, where, reg_ptr, DR_PRED_NONE, adjust);

    /* Insert code to call clean_call for processing the buffer.
     * We restore the registers after the clean call, which should be ok
     * assuming the clean call does not need the two register values.
     */
    if (is_last_instr(drcontext, instr)) {
        if (op_L0I_filter.get_value() || op_L0D_filter.get_value())
            insert_load_buf_ptr(drcontext, bb, where, reg_ptr);
        instrument_clean_call(drcontext, bb, where, reg_ptr);
    }

    insert_conditional_skip_target(drcontext, bb, where, skip_instru, reg_skip,
                                   app_regs_at_skip);

    /* restore scratch register */
    if (drreg_unreserve_register(drcontext, bb, where, reg_ptr) != DRREG_SUCCESS)
        DR_ASSERT(false);
    return flags;
}

/* We transform string loops into regular loops so we can more easily
 * monitor every memory reference they make.
 */
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    /* drbbdup doesn't pass the user_data from this stage so we use TLS.
     * XXX i#5400: Integrating drbbdup into drmgr would provide user_data here.
     */
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (!drutil_expand_rep_string_ex(drcontext, bb, &pt->repstr, NULL)) {
        DR_ASSERT(false);
        /* in release build, carry on: we'll just miss per-iter refs */
    }
    if (!drx_expand_scatter_gather(drcontext, bb, &pt->scatter_gather)) {
        DR_ASSERT(false);
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    user_data_t *ud = (user_data_t *)dr_thread_alloc(drcontext, sizeof(user_data_t));
    memset(ud, 0, sizeof(*ud));
    ud->repstr = pt->repstr;
    ud->scatter_gather = pt->scatter_gather;
    *user_data = (void *)ud;

    instru->bb_analysis(drcontext, tag, &ud->instru_field, bb, ud->repstr);

    ud->bb_instr_count = instru_t::count_app_instrs(bb);
    // As elsewhere in this code, we want the single-instr original and not
    // the expanded 6 labeled-app instrs for repstr loops (i#2011).
    // The new emulation support should have solved that for us.
    DR_ASSERT((!ud->repstr && !pt->scatter_gather) || ud->bb_instr_count == 1);

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis_cleanup(void *drcontext, void *user_data)
{
    dr_thread_free(drcontext, user_data, sizeof(user_data_t));
    return DR_EMIT_DEFAULT;
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    return true;
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (tracing_disabled.load(std::memory_order_acquire) != BBDUP_MODE_TRACE)
        return true;
    if (BUF_PTR(data->seg_base) == NULL)
        return true; /* This thread was filtered out. */
#ifdef ARM
    // On Linux ARM, cacheflush syscall takes 3 params: start, end, and 0.
    if (sysnum == SYS_cacheflush) {
        addr_t start = (addr_t)dr_syscall_get_param(drcontext, 0);
        addr_t end = (addr_t)dr_syscall_get_param(drcontext, 1);
        if (end > start) {
            BUF_PTR(data->seg_base) +=
                instru->append_iflush(BUF_PTR(data->seg_base), start, end - start);
        }
    }
#endif
    if (file_ops_func.handoff_buf == NULL)
        memtrace(drcontext, false);
    return true;
}

static void
event_kernel_xfer(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    trace_marker_type_t marker_type;
    uintptr_t marker_val = 0;
    if (tracing_disabled.load(std::memory_order_acquire) != BBDUP_MODE_TRACE)
        return;
    if (BUF_PTR(data->seg_base) == NULL)
        return; /* This thread was filtered out. */
    switch (info->type) {
    case DR_XFER_APC_DISPATCHER:
        /* Do not bother with a marker for the thread init routine. */
        if (data->num_refs == 0 &&
            BUF_PTR(data->seg_base) == data->buf_base + data->init_header_size)
            return;
        ANNOTATE_FALLTHROUGH;
    case DR_XFER_SIGNAL_DELIVERY:
    case DR_XFER_EXCEPTION_DISPATCHER:
    case DR_XFER_RAISE_DISPATCHER:
    case DR_XFER_CALLBACK_DISPATCHER:
    case DR_XFER_RSEQ_ABORT: marker_type = TRACE_MARKER_TYPE_KERNEL_EVENT; break;
    case DR_XFER_SIGNAL_RETURN:
    case DR_XFER_CALLBACK_RETURN:
    case DR_XFER_CONTINUE:
    case DR_XFER_SET_CONTEXT_THREAD: marker_type = TRACE_MARKER_TYPE_KERNEL_XFER; break;
    case DR_XFER_CLIENT_REDIRECT: return;
    default: DR_ASSERT(false && "unknown kernel xfer type"); return;
    }
    NOTIFY(2, "%s: type %d, sig %d\n", __FUNCTION__, info->type, info->sig);
    /* TODO i#3937: We need something similar to what raw2trace does with this info
     * for online too, to place signals inside instr bundles.
     */
    /* XXX i#4041: For rseq abort, offline post-processing rolls back the committing
     * store so the abort happens at a reasonable point.  We don't have a solution
     * for online though.
     */
    if (info->source_mcontext != nullptr) {
        /* Enable post-processing to figure out the ordering of this xfer vs
         * non-memref instrs in the bb, and also to give core simulators the
         * interrupted PC -- primarily for a kernel event arriving right
         * after a branch to give a core simulator the branch target.
         */
#ifdef X64 /* For 32-bit we can fit the abs pc but not modix:modoffs. */
        if (op_offline.get_value()) {
            uint modidx;
            /* Just like PC entries, this is the offset from the base, not from
             * the indexed segment.
             */
            uint64_t modoffs = reinterpret_cast<offline_instru_t *>(instru)->get_modoffs(
                drcontext, info->source_mcontext->pc, &modidx);
            /* We save space by using the modidx,modoffs format instead of a raw PC.
             * These 49 bits will always fit into the 48-bit value field unless the
             * module index is very large, when it will take two entries, while using
             * an absolute PC here might always take two entries for some modules.
             * We'll turn this into an absolute PC in the final trace.
             */
            kernel_interrupted_raw_pc_t raw_pc = {};
            raw_pc.pc.modidx = modidx;
            raw_pc.pc.modoffs = modoffs;
            marker_val = raw_pc.combined_value;
        } else
#endif
            marker_val = reinterpret_cast<uintptr_t>(info->source_mcontext->pc);
        NOTIFY(3, "%s: source pc " PFX " => modoffs " PIFX "\n", __FUNCTION__,
               info->source_mcontext->pc, marker_val);
    }
    if (info->type == DR_XFER_RSEQ_ABORT) {
        BUF_PTR(data->seg_base) += instru->append_marker(
            BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_RSEQ_ABORT, marker_val);
    }
    BUF_PTR(data->seg_base) +=
        instru->append_marker(BUF_PTR(data->seg_base), marker_type, marker_val);
    if (file_ops_func.handoff_buf == NULL)
        memtrace(drcontext, false);
}

/***************************************************************************
 * Instruction counting mode where we do not record any trace data.
 */

static uint64 instr_count;
/* For performance, we only increment the global instr_count exactly for
 * small thresholds.  If -trace_after_instrs is larger than this value, we
 * instead use thread-private counters and add to the global every
 * ~DELAY_COUNTDOWN_UNIT instructions.
 */
#define DELAY_EXACT_THRESHOLD (10 * 1024 * 1024)
// We use the same value we use for tracing windows.
#define DELAY_COUNTDOWN_UNIT INSTR_COUNT_LOCAL_UNIT
// For -trace_for_instrs without -retrace_every_instrs we count forever,
// but to avoid the complexity of different instrumentation we need a threshold.
#define DELAY_FOREVER_THRESHOLD (1024 * 1024 * 1024)

std::atomic<bool> reached_trace_after_instrs;

static bool
has_instr_count_threshold_to_enable_tracing()
{
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return true;
    if (op_retrace_every_instrs.get_value() > 0)
        return true;
    return false;
}

static uint64
instr_count_threshold()
{
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        return op_trace_after_instrs.get_value();
    if (op_retrace_every_instrs.get_value() > 0)
        return op_retrace_every_instrs.get_value();
    return DELAY_FOREVER_THRESHOLD;
}

// Enables tracing if we've reached the delay point.
// For tracing windows going in the reverse direction and disabling tracing,
// see reached_traced_instrs_threshold().
static void
hit_instr_count_threshold(app_pc next_pc)
{
    if (!has_instr_count_threshold_to_enable_tracing())
        return;
#ifdef DELAYED_CHECK_INLINED
    /* XXX: We could do the same thread-local counters for non-inlined.
     * We'd then switch to std::atomic or something for 32-bit.
     */
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        void *drcontext = dr_get_current_drcontext();
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        int64 myval = *(int64 *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN);
        uint64 newval = (uint64)dr_atomic_add64_return_sum((volatile int64 *)&instr_count,
                                                           DELAY_COUNTDOWN_UNIT - myval);
        *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN) =
            DELAY_COUNTDOWN_UNIT;
        if (newval < instr_count_threshold())
            return;
    }
#endif
    dr_mutex_lock(mutex);
    if (tracing_disabled.load(std::memory_order_acquire) == BBDUP_MODE_TRACE) {
        // Another thread already changed the mode.
        dr_mutex_unlock(mutex);
        return;
    }
    if (op_trace_after_instrs.get_value() > 0 &&
        !reached_trace_after_instrs.load(std::memory_order_acquire))
        NOTIFY(0, "Hit delay threshold: enabling tracing.\n");
    else {
        NOTIFY(0, "Hit retrace threshold: enabling tracing for window #%zd.\n",
               tracing_window.load(std::memory_order_acquire));
        if (op_offline.get_value())
            open_new_window_dir(tracing_window.load(std::memory_order_acquire));
    }
    if (!reached_trace_after_instrs.load(std::memory_order_acquire)) {
        reached_trace_after_instrs.store(true, std::memory_order_release);
    }
    // Reset for -retrace_every_instrs.
#ifdef X64
    dr_atomic_store64((volatile int64 *)&instr_count, 0);
#else
    // dr_atomic_store64 is not implemented for 32-bit, and it's technically not
    // portably safe to take the address of std::atomic, so we rely on our mutex.
    instr_count = 0;
#endif
    DR_ASSERT(tracing_disabled.load(std::memory_order_acquire) == BBDUP_MODE_COUNT);
    tracing_disabled.store(BBDUP_MODE_TRACE, std::memory_order_release);
    dr_mutex_unlock(mutex);
}

#ifndef DELAYED_CHECK_INLINED
static void
check_instr_count_threshold(uint incby, app_pc next_pc)
{
    if (!has_instr_count_threshold_to_enable_tracing())
        return;
    /* XXX i#5030: This is racy.  We could make std::atomic, or, better, go and
     * implement the inlining and i#5026's thread-private counting.
     */
    instr_count += incby;
    if (instr_count > instr_count_threshold())
        hit_instr_count_threshold(next_pc);
}
#endif

static dr_emit_flags_t
event_inscount_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                           bool translating, void **user_data)
{
    instr_t *instr;
    uint num_instrs;
    for (instr = instrlist_first_app(bb), num_instrs = 0; instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
    }
    *user_data = (void *)(ptr_uint_t)num_instrs;
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_inscount_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                               instr_t *instr, instr_t *where, bool for_trace,
                               bool translating, void *orig_analysis_data,
                               void *user_data)
{
    uint num_instrs;
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;

    // Give drwrap a chance to clean up, even when we're not actively wrapping.
    dr_emit_flags_t func_flags = func_trace_disabled_instrument_event(
        drcontext, tag, bb, instr, where, for_trace, translating, NULL);
    flags = static_cast<dr_emit_flags_t>(flags | func_flags);

    if (!is_first_nonlabel(drcontext, instr))
        return flags;

    num_instrs = (uint)(ptr_uint_t)user_data;
    drmgr_disable_auto_predication(drcontext, bb);
#ifdef DELAYED_CHECK_INLINED
#    if defined(X86_64) || defined(AARCH64)
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
#        ifdef X86_64
    reg_id_t scratch = DR_REG_NULL;
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        /* Contention on a global counter causes high overheads.  We approximate the
         * count by using thread-local counters and only merging into the global
         * every so often.
         */
        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags");
        MINSERT(
            bb, where,
            INSTR_CREATE_sub(
                drcontext,
                dr_raw_tls_opnd(drcontext, tls_seg,
                                tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN),
                OPND_CREATE_INT32(num_instrs)));
        MINSERT(bb, where,
                INSTR_CREATE_jcc(drcontext, OP_jns, opnd_create_instr(skip_call)));
    } else {
        if (!drx_insert_counter_update(
                drcontext, bb, where, (dr_spill_slot_t)(SPILL_SLOT_MAX + 1) /*use drmgr*/,
                &instr_count, num_instrs, DRX_COUNTER_64BIT))
            DR_ASSERT(false);

        if (drreg_reserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve aflags");
        if (instr_count_threshold() < INT_MAX) {
            MINSERT(bb, where,
                    XINST_CREATE_cmp(drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                                     OPND_CREATE_INT32(instr_count_threshold())));
        } else {
            if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch) !=
                DRREG_SUCCESS)
                FATAL("Fatal error: failed to reserve scratch register");
            instrlist_insert_mov_immed_ptrsz(drcontext, instr_count_threshold(),
                                             opnd_create_reg(scratch), bb, where, NULL,
                                             NULL);
            MINSERT(bb, where,
                    XINST_CREATE_cmp(drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                                     opnd_create_reg(scratch)));
        }
        MINSERT(bb, where,
                INSTR_CREATE_jcc(drcontext, OP_jl, opnd_create_instr(skip_call)));
    }
#        elif defined(AARCH64)
    reg_id_t scratch1, scratch2 = DR_REG_NULL;
    if (instr_count_threshold() > DELAY_EXACT_THRESHOLD) {
        /* See the x86_64 comment on using thread-local counters to avoid contention. */
        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch1) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");
        dr_insert_read_raw_tls(drcontext, bb, where, tls_seg,
                               tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN,
                               scratch1);
        /* We're counting down for an aflags-free comparison. */
        MINSERT(bb, where,
                XINST_CREATE_sub(drcontext, opnd_create_reg(scratch1),
                                 OPND_CREATE_INT(num_instrs)));
        dr_insert_write_raw_tls(drcontext, bb, where, tls_seg,
                                tls_offs + sizeof(void *) * MEMTRACE_TLS_OFFS_ICOUNTDOWN,
                                scratch1);
        MINSERT(bb, where,
                INSTR_CREATE_tbz(drcontext, opnd_create_instr(skip_call),
                                 /* If the top bit is still zero, skip the call. */
                                 opnd_create_reg(scratch1), OPND_CREATE_INT(63)));
    } else {
        /* We're counting down for an aflags-free comparison. */
        if (!drx_insert_counter_update(
                drcontext, bb, where, (dr_spill_slot_t)(SPILL_SLOT_MAX + 1) /*use drmgr*/,
                (dr_spill_slot_t)(SPILL_SLOT_MAX + 1), &instr_count, -num_instrs,
                DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ))
            DR_ASSERT(false);

        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch1) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");
        if (drreg_reserve_register(drcontext, bb, where, NULL, &scratch2) !=
            DRREG_SUCCESS)
            FATAL("Fatal error: failed to reserve scratch register");

        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)&instr_count,
                                         opnd_create_reg(scratch1), bb, where, NULL,
                                         NULL);
        MINSERT(bb, where,
                XINST_CREATE_load(drcontext, opnd_create_reg(scratch2),
                                  OPND_CREATE_MEMPTR(scratch1, 0)));
        MINSERT(bb, where,
                INSTR_CREATE_tbz(drcontext, opnd_create_instr(skip_call),
                                 /* If the top bit is still zero, skip the call. */
                                 opnd_create_reg(scratch2), OPND_CREATE_INT(63)));
    }
#        endif

    dr_insert_clean_call_ex(drcontext, bb, where, (void *)hit_instr_count_threshold,
                            static_cast<dr_cleancall_save_t>(
                                DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_MULTIPATH),
                            1, OPND_CREATE_INTPTR((ptr_uint_t)instr_get_app_pc(instr)));
    MINSERT(bb, where, skip_call);

#        ifdef X86_64
    if (drreg_unreserve_aflags(drcontext, bb, where) != DRREG_SUCCESS)
        DR_ASSERT(false);
    if (scratch != DR_REG_NULL) {
        if (drreg_unreserve_register(drcontext, bb, where, scratch) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
#        elif defined(AARCH64)
    if (drreg_unreserve_register(drcontext, bb, where, scratch1) != DRREG_SUCCESS ||
        (scratch2 != DR_REG_NULL &&
         drreg_unreserve_register(drcontext, bb, where, scratch2) != DRREG_SUCCESS))
        DR_ASSERT(false);
#        endif
#    else
#        error NYI
#    endif
#else
    /* XXX: drx_insert_counter_update doesn't support 64-bit counters for ARM_32, and
     * inlining of check_instr_count_threshold is not implemented for i386. For now we pay
     * the cost of a clean call every time for 32-bit architectures.
     */
    dr_insert_clean_call_ex(drcontext, bb, where, (void *)check_instr_count_threshold,
                            DR_CLEANCALL_READS_APP_CONTEXT, 2,
                            OPND_CREATE_INT32(num_instrs),
                            OPND_CREATE_INTPTR((ptr_uint_t)instr_get_app_pc(instr)));
#endif
    return flags;
}

/***************************************************************************
 * Top level.
 */

static offline_file_type_t
get_file_type()
{
    offline_file_type_t file_type = OFFLINE_FILE_TYPE_DEFAULT;
    if (op_L0I_filter.get_value()) {
        file_type =
            static_cast<offline_file_type_t>(file_type | OFFLINE_FILE_TYPE_IFILTERED);
    }
    if (op_L0D_filter.get_value()) {
        file_type =
            static_cast<offline_file_type_t>(file_type | OFFLINE_FILE_TYPE_DFILTERED);
    }
    if (op_disable_optimizations.get_value()) {
        file_type = static_cast<offline_file_type_t>(file_type |
                                                     OFFLINE_FILE_TYPE_NO_OPTIMIZATIONS);
    }
    if (op_instr_only_trace.get_value() ||
        // Data entries are removed from trace if -L0D_filter and -L0D_size 0
        (op_L0D_filter.get_value() && op_L0D_size.get_value() == 0)) {
        file_type = static_cast<offline_file_type_t>(file_type |
                                                     OFFLINE_FILE_TYPE_INSTRUCTION_ONLY);
    }
    file_type = static_cast<offline_file_type_t>(
        file_type |
        IF_X86_ELSE(
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_X86_64, OFFLINE_FILE_TYPE_ARCH_X86_32),
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_AARCH64, OFFLINE_FILE_TYPE_ARCH_ARM32)));
    return file_type;
}

/* Initializes a thread either at process init or fork init, where we want
 * a new offline file or a new thread,process registration pair for online.
 */
static void
init_thread_in_process(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    byte *proc_info;

#ifdef HAS_ZLIB
    if (op_offline.get_value() &&
        (op_raw_compress.get_value() == "zlib" ||
         op_raw_compress.get_value() == "gzip")) {
        data->buf_compressed = static_cast<byte *>(
            dr_raw_mem_alloc(max_buf_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, nullptr));
    }
#endif
#ifdef HAS_LZ4
    if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
        data->buf_lz4_size = LZ4F_compressBound(max_buf_size, &lz4_ops);
        DR_ASSERT(data->buf_lz4_size >= LZ4F_HEADER_SIZE_MAX);
        data->buf_lz4 = static_cast<byte *>(dr_raw_mem_alloc(
            data->buf_lz4_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, nullptr));
    }
#endif

    if (op_use_physical.get_value()) {
        if (!data->physaddr.init()) {
            FATAL("Unable to open pagemap for physical addresses in thread T%d: check "
                  "privileges.\n",
                  dr_get_thread_id(drcontext));
        }
    }

    set_local_window(drcontext, -1);
    if (has_tracing_windows())
        set_local_window(drcontext, tracing_window.load(std::memory_order_acquire));

    if (op_offline.get_value()) {
        if (tracing_disabled.load(std::memory_order_acquire) == BBDUP_MODE_TRACE) {
            open_new_thread_file(drcontext, get_local_window(data));
        }
        if (!has_tracing_windows()) {
            data->init_header_size = prepend_offline_thread_header(drcontext);
        } else {
            // set_local_window() called prepend_offline_thread_header().
        }
    } else {
        /* pass pid and tid to the simulator to register current thread */
        char buf[MAXIMUM_PATH];
        proc_info = (byte *)buf;
        proc_info += reinterpret_cast<online_instru_t *>(instru)->append_thread_header(
            proc_info, dr_get_thread_id(drcontext), get_file_type());
        DR_ASSERT(BUFFER_SIZE_BYTES(buf) >= (size_t)(proc_info - (byte *)buf));
        write_trace_data(drcontext, (byte *)buf, proc_info, get_local_window(data));

        /* put buf_base to TLS plus header slots as starting buf_ptr */
        data->init_header_size = buf_hdr_slots_size;
        BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
    }

    if (op_L0D_filter.get_value() && op_L0D_size.get_value() > 0) {
        data->l0_dcache = (byte *)dr_raw_mem_alloc(
            (size_t)op_L0D_size.get_value() / op_line_size.get_value() * sizeof(void *),
            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        *(byte **)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_DCACHE) = data->l0_dcache;
    }
    if (op_L0I_filter.get_value() && op_L0I_size.get_value() > 0) {
        data->l0_icache = (byte *)dr_raw_mem_alloc(
            (size_t)op_L0I_size.get_value() / op_line_size.get_value() * sizeof(void *),
            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        *(byte **)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICACHE) = data->l0_icache;
    }

    // XXX i#1729: gather and store an initial callstack for the thread.
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    *data = {}; // We must safely zero due to the C++ class member.
    data->file = INVALID_FILE;
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = (byte *)dr_get_dr_segment_base(tls_seg);
    DR_ASSERT(data->seg_base != NULL);

    *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNTDOWN) =
        DELAY_COUNTDOWN_UNIT;

    if ((should_trace_thread_cb != NULL &&
         !(*should_trace_thread_cb)(dr_get_thread_id(drcontext),
                                    trace_thread_cb_user_data)) ||
        is_num_refs_beyond_global_max())
        BUF_PTR(data->seg_base) = NULL;
    else {
        create_buffer(data);
        if (op_use_physical.get_value() && op_offline.get_value()) {
            create_v2p_buffer(data);
        }
        init_thread_in_process(drcontext);
        // XXX i#1729: gather and store an initial callstack for the thread.
    }
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    if (BUF_PTR(data->seg_base) != NULL) {
        /* This thread was *not* filtered out. */

        /* let the simulator know this thread has exited */
        if (is_bytes_written_beyond_trace_max(data)) {
            // If over the limit, we still want to write the footer, but nothing else.
            BUF_PTR(data->seg_base) = data->buf_base + buf_hdr_slots_size;
        }
        if (op_L0I_filter.get_value()) {
            // Include the final instruction count.
            // It might be useful to include the count with each miss as well, but
            // in experiments that adds non-trivial space and time overheads (as
            // a separate marker; squished into the instr_count field might be
            // better but at complexity costs, plus we may need that field for
            // offset-within-block info to adjust the per-block count) and
            // would likely need to be under an off-by-default option and have
            // a mandated use case to justify adding it.
            uintptr_t icount =
                *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNT);
            BUF_PTR(data->seg_base) += instru->append_marker(
                BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_INSTRUCTION_COUNT, icount);
        }
        if (tracing_disabled.load(std::memory_order_acquire) == BBDUP_MODE_TRACE ||
            !op_split_windows.get_value()) {
            BUF_PTR(data->seg_base) += instru->append_thread_exit(
                BUF_PTR(data->seg_base), dr_get_thread_id(drcontext));

            ptr_int_t window = get_local_window(data);
            memtrace(drcontext,
                     /* If this thread already wrote some data, include its exit even
                      * if we're over a size limit.
                      */
                     data->bytes_written > 0);
            if (get_local_window(data) != window) {
                BUF_PTR(data->seg_base) += instru->append_thread_exit(
                    BUF_PTR(data->seg_base), dr_get_thread_id(drcontext));
                memtrace(drcontext, data->bytes_written > 0);
            }
        }

        if (op_L0D_filter.get_value()) {
            if (op_L0D_size.get_value() > 0) {
                dr_raw_mem_free(data->l0_dcache,
                                (size_t)op_L0D_size.get_value() /
                                    op_line_size.get_value() * sizeof(void *));
            }
        }
        if (op_L0I_filter.get_value()) {
            if (op_L0I_size.get_value() > 0) {
                dr_raw_mem_free(data->l0_icache,
                                (size_t)op_L0I_size.get_value() /
                                    op_line_size.get_value() * sizeof(void *));
            }
        }

        if (op_offline.get_value() && data->file != INVALID_FILE)
            close_thread_file(drcontext);

#ifdef HAS_ZLIB
        if (op_offline.get_value() &&
            (op_raw_compress.get_value() == "zlib" ||
             op_raw_compress.get_value() == "gzip")) {
            dr_raw_mem_free(data->buf_compressed, max_buf_size);
        }
#endif
#ifdef HAS_LZ4
        if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
            dr_raw_mem_free(data->buf_lz4, data->buf_lz4_size);
        }
#endif

        dr_mutex_lock(mutex);
        num_refs += data->num_refs;
        num_writeouts += data->num_writeouts;
        num_v2p_writeouts += data->num_v2p_writeouts;
        num_phys_markers += data->num_phys_markers;
        dr_mutex_unlock(mutex);
        dr_raw_mem_free(data->buf_base, max_buf_size);
        if (data->reserve_buf != NULL)
            dr_raw_mem_free(data->reserve_buf, max_buf_size);
    }
    data->~per_thread_t();
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void
event_exit(void)
{
    dr_log(NULL, DR_LOG_ALL, 1, "drcachesim num refs seen: " UINT64_FORMAT_STRING "\n",
           num_refs);
    NOTIFY(1,
           "drmemtrace exiting process " PIDFMT "; traced " UINT64_FORMAT_STRING
           " references in " UINT64_FORMAT_STRING " writeouts.\n",
           dr_get_process_id(), num_refs, num_writeouts);
    if (op_use_physical.get_value() && op_offline.get_value()) {
        dr_log(NULL, DR_LOG_ALL, 1,
               "drcachesim num physical address markers emitted: " UINT64_FORMAT_STRING
               "\n",
               num_phys_markers);
        NOTIFY(1,
               "drmemtrace emitted " UINT64_FORMAT_STRING
               " physical address markers in " UINT64_FORMAT_STRING " writeouts.\n",
               num_phys_markers, num_v2p_writeouts);
    }
    /* we use placement new for better isolation */
    instru->~instru_t();
    dr_global_free(instru, MAX_INSTRU_SIZE);

    if (op_offline.get_value()) {
        file_ops_func.close_file(module_file);
        if (funclist_file != INVALID_FILE)
            file_ops_func.close_file(funclist_file);
    } else
        ipc_pipe.close();

    if (file_ops_func.exit_cb != NULL)
        (*file_ops_func.exit_cb)(file_ops_func.exit_arg);

    if (!dr_raw_tls_cfree(tls_offs, MEMTRACE_TLS_COUNT))
        DR_ASSERT(false);

    drvector_delete(&scratch_reserve_vec);

    instrumentation_exit();

    if (!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);
    if (op_enable_drstatecmp.get_value()) {
        if (drstatecmp_exit() != DRSTATECMP_SUCCESS) {
            DR_ASSERT(false);
        }
    }
    dr_unregister_exit_event(event_exit);

    /* Clear callbacks and globals to support re-attach when linked statically. */
    file_ops_func = file_ops_func_t();
    if (!offline_instru_t::custom_module_data(nullptr, nullptr, nullptr))
        DR_ASSERT(false && "failed to clear custom module callbacks");
    should_trace_thread_cb = nullptr;
    trace_thread_cb_user_data = nullptr;
    thread_filtering_enabled = false;
    num_refs = 0;
    num_refs_racy = 0;
    notify_beyond_global_max_once = 0;

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
    func_trace_exit();
    drx_exit();
    /* Avoid accumulation of option values on static-link re-attach. */
    droption_parser_t::clear_values();
}

static bool
init_offline_dir(void)
{
    char buf[MAXIMUM_PATH];
    int i;
    const int NUM_OF_TRIES = 10000;
    /* open unique dir */
    /* We cannot use malloc mid-run (to support static client use).  We thus need
     * to move all std::strings into plain buffers at init time.
     */
    dr_snprintf(subdir_prefix, BUFFER_SIZE_ELEMENTS(subdir_prefix), "%s",
                op_subdir_prefix.get_value().c_str());
    NULL_TERMINATE_BUFFER(subdir_prefix);
    /* We do not need to call drx_init before using drx_open_unique_appid_file. */
    for (i = 0; i < NUM_OF_TRIES; i++) {
        /* We use drx_open_unique_appid_file with DRX_FILE_SKIP_OPEN to get a
         * directory name for creation.  Retry if the same name directory already
         * exists.  Abort if we fail too many times.
         */
        drx_open_unique_appid_file(op_outdir.get_value().c_str(), dr_get_process_id(),
                                   subdir_prefix, "dir", DRX_FILE_SKIP_OPEN, buf,
                                   BUFFER_SIZE_ELEMENTS(buf));
        NULL_TERMINATE_BUFFER(buf);
        /* open the dir */
        if (file_ops_func.create_dir(buf))
            break;
    }
    if (i == NUM_OF_TRIES)
        return false;
    /* We group the raw thread files in a further subdir to isolate from the
     * processed trace file.
     */
    dr_snprintf(logsubdir, BUFFER_SIZE_ELEMENTS(logsubdir), "%s%s%s", buf, DIRSEP,
                OUTFILE_SUBDIR);
    NULL_TERMINATE_BUFFER(logsubdir);
    if (!file_ops_func.create_dir(logsubdir))
        return false;
    if (has_tracing_windows())
        open_new_window_dir(tracing_window.load(std::memory_order_acquire));
    /* If the ops are replaced, it's up the replacer to notify the user.
     * In some cases data is sent over the network and the replaced create_dir is
     * a nop that returns true, in which case we don't want this message.
     */
    if (file_ops_func.create_dir == dr_create_dir)
        NOTIFY(1, "Log directory is %s\n", logsubdir);
    dr_snprintf(modlist_path, BUFFER_SIZE_ELEMENTS(modlist_path), "%s%s%s", logsubdir,
                DIRSEP, DRMEMTRACE_MODULE_LIST_FILENAME);
    NULL_TERMINATE_BUFFER(modlist_path);
    module_file = file_ops_func.open_file(
        modlist_path, DR_FILE_WRITE_REQUIRE_NEW IF_UNIX(| DR_FILE_CLOSE_ON_FORK));

    dr_snprintf(funclist_path, BUFFER_SIZE_ELEMENTS(funclist_path), "%s%s%s", logsubdir,
                DIRSEP, DRMEMTRACE_FUNCTION_LIST_FILENAME);
    NULL_TERMINATE_BUFFER(funclist_path);
    funclist_file = file_ops_func.open_file(
        funclist_path, DR_FILE_WRITE_REQUIRE_NEW IF_UNIX(| DR_FILE_CLOSE_ON_FORK));

    return (module_file != INVALID_FILE && funclist_file != INVALID_FILE);
}

#ifdef UNIX
static void
fork_init(void *drcontext)
{
    if (op_offline.get_value()) {
        /* XXX i#4660: droption references at fork init time use malloc.
         * We do not fully support static link with fork at this time.
         */
        dr_allow_unsafe_static_behavior();
#    ifdef DRMEMTRACE_STATIC
        NOTIFY(0, "-offline across a fork is unsafe with statically linked clients\n");
#    endif
    }
    /* We use DR_FILE_CLOSE_ON_FORK, and we dumped outstanding data prior to the
     * fork syscall, so we just need to create a new subdir, new module log, and
     * a new initial thread file for offline, or register the new process for
     * online.
     */
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Only count refs in the new process (plus, we use this to set up the
     * initial header in memtrace() for offline).
     */
    data->num_refs = 0;
    if (op_offline.get_value()) {
        data->file = INVALID_FILE;
        if (!init_offline_dir()) {
            FATAL("Failed to create a subdir in %s\n", op_outdir.get_value().c_str());
        }
    }
    init_thread_in_process(drcontext);
}
#endif

/* We export drmemtrace_client_main so that a global dr_client_main can initialize
 * drmemtrace client by calling drmemtrace_client_main in a statically linked
 * multi-client executable.
 */
DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[])
{
    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = { sizeof(ops), 3, false };
    byte buf[MAXIMUM_PATH];

    dr_set_client_name("DynamoRIO Cache Simulator Tracer", "http://dynamorio.org/issues");

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err,
                                       NULL)) {
        FATAL("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    }
    if (!op_offline.get_value() && op_ipc_name.get_value().empty()) {
        FATAL("Usage error: ipc name is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    } else if (op_offline.get_value() && op_outdir.get_value().empty()) {
        FATAL("Usage error: outdir is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
    } else if (!op_offline.get_value() &&
               (op_record_heap.get_value() || !op_record_function.get_value().empty())) {
        FATAL("Usage error: function recording is only supported for -offline\n");
    }

    if (op_L0_filter_deprecated.get_value()) {
        op_L0D_filter.set_value(true);
        op_L0I_filter.set_value(true);
    }
    if ((op_L0I_filter.get_value() && !IS_POWER_OF_2(op_L0I_size.get_value()) &&
         op_L0I_size.get_value() != 0) ||
        (op_L0D_filter.get_value() && !IS_POWER_OF_2(op_L0D_size.get_value()) &&
         op_L0D_size.get_value() != 0)) {
        FATAL("Usage error: L0I_size and L0D_size must be 0 or powers of 2.");
    }
    if (op_raw_compress.get_value() == "none"
#ifdef HAS_SNAPPY
        || op_raw_compress.get_value() == "snappy" ||
        op_raw_compress.get_value() == "snappy_nocrc"
#endif
#ifdef HAS_ZLIB
        || op_raw_compress.get_value() == "gzip" || op_raw_compress.get_value() == "zlib"
#endif
#ifdef HAS_LZ4
        || op_raw_compress.get_value() == "lz4"
#endif
    ) {
        // Valid option.
    } else {
        FATAL("Usage error: unknown -raw_compress type %s.",
              op_raw_compress.get_value().c_str());
    }
    // We cannot elide addresses or ignore offsets when we need to translate
    // all addresses during tracing or when instruction or data address entries
    // are being filtered.
    if (op_use_physical.get_value() || op_L0I_filter.get_value() ||
        op_L0D_filter.get_value())
        op_disable_optimizations.set_value(true);

    DR_ASSERT(std::atomic_is_lock_free(&reached_trace_after_instrs));
    DR_ASSERT(std::atomic_is_lock_free(&tracing_disabled));
    DR_ASSERT(std::atomic_is_lock_free(&tracing_window));

    drreg_init_and_fill_vector(&scratch_reserve_vec, true);
#ifdef X86
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value()) {
        /* We need to preserve the flags so we need xax. */
        drreg_set_vector_entry(&scratch_reserve_vec, DR_REG_XAX, false);
    }
#endif

    if (op_offline.get_value()) {
        void *placement;
        if (!init_offline_dir()) {
            FATAL("Failed to create a subdir in %s\n", op_outdir.get_value().c_str());
        }
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(offline_instru_t));
        placement = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new (placement) offline_instru_t(
            insert_load_buf_ptr, op_L0I_filter.get_value(), &scratch_reserve_vec,
            file_ops_func.write_file, module_file, op_disable_optimizations.get_value());
    } else {
        void *placement;
        /* we use placement new for better isolation */
        DR_ASSERT(MAX_INSTRU_SIZE >= sizeof(online_instru_t));
        placement = dr_global_alloc(MAX_INSTRU_SIZE);
        instru = new (placement)
            online_instru_t(insert_load_buf_ptr, insert_update_buf_ptr,
                            op_L0I_filter.get_value(), &scratch_reserve_vec);
        if (!ipc_pipe.set_name(op_ipc_name.get_value().c_str()))
            DR_ASSERT(false);
#ifdef UNIX
        /* we want an isolated fd so we don't use ipc_pipe.open_for_write() */
        int fd = dr_open_file(ipc_pipe.get_pipe_path().c_str(), DR_FILE_WRITE_ONLY);
        DR_ASSERT(fd != INVALID_FILE);
        if (!ipc_pipe.set_fd(fd))
            DR_ASSERT(false);
#else
        if (!ipc_pipe.open_for_write()) {
            if (GetLastError() == ERROR_PIPE_BUSY) {
                // FIXME i#1727: add multi-process support to Windows named_pipe_t.
                FATAL("Fatal error: multi-process applications not yet supported "
                      "for drcachesim on Windows\n");
            } else {
                FATAL("Fatal error: Failed to open pipe %s.\n",
                      op_ipc_name.get_value().c_str());
            }
        }
#endif
        if (!ipc_pipe.maximize_buffer())
            NOTIFY(1, "Failed to maximize pipe buffer: performance may suffer.\n");
    }

    if (op_offline.get_value() &&
        !func_trace_init(append_marker_seg_base, file_ops_func.write_file,
                         funclist_file)) {
        FATAL("Failed to initialized function tracing.\n");
    }

    /* We need an extra for -L0I_filter and -L0D_filter. */
    if (op_L0I_filter.get_value() || op_L0D_filter.get_value())
        ++ops.num_spill_slots;
    // We use the buf pointer reg plus 2 more in instrument_clean_call, so we want
    // 3 total: thus 1 more than the base.
    if (has_tracing_windows())
        ++ops.num_spill_slots;

    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS ||
        !drx_init())
        DR_ASSERT(false);
    if (op_enable_drstatecmp.get_value()) {
        drstatecmp_options_t drstatecmp_ops = { NULL };
        if (drstatecmp_init(&drstatecmp_ops) != DRSTATECMP_SUCCESS) {
            DR_ASSERT(false);
        }
    }

    /* register events */
    dr_register_exit_event(event_exit);
#ifdef UNIX
    dr_register_fork_init_event(fork_init);
#endif
    /* We need our thread exit event to run *before* drmodtrack's as we may
     * need to translate physical addresses for the thread's final buffer.
     */
    drmgr_priority_t pri_thread_exit = { sizeof(drmgr_priority_t), "", nullptr, nullptr,
                                         -100 };
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event_ex(event_thread_exit, &pri_thread_exit))
        DR_ASSERT(false);

    instrumentation_init();

    trace_buf_size = instru->sizeof_entry() * MAX_NUM_ENTRIES;

    /* The redzone needs to hold one bb's worth of data, until we
     * reach the clean call at the bottom of the bb that dumps the
     * buffer if full.  We leave room for each of the maximum count of
     * instructions accessing memory once, which is fairly
     * pathological as by default that's 256 memrefs for one bb.  We double
     * it to ensure we cover skipping clean calls for sthg like strex.
     * We also check here that the max_bb_instrs can fit in the instr_count
     * bitfield in offline_entry_t.
     */
    uint64 max_bb_instrs;
    if (!dr_get_integer_option("max_bb_instrs", &max_bb_instrs))
        max_bb_instrs = 256; /* current default */
    DR_ASSERT(max_bb_instrs < uint64(1) << PC_INSTR_COUNT_BITS);
    redzone_size = instru->sizeof_entry() * (size_t)max_bb_instrs * 2;

    max_buf_size = ALIGN_FORWARD(trace_buf_size + redzone_size, dr_page_size());
    /* Mark any padding as redzone as well */
    redzone_size = max_buf_size - trace_buf_size;
    /* Append a throwaway header to get its size. */
    buf_hdr_slots_size = append_unit_header(
        NULL /*no TLS yet*/, buf, 0 /*doesn't matter*/, has_tracing_windows() ? 0 : -1);
    DR_ASSERT(BUFFER_SIZE_BYTES(buf) >= buf_hdr_slots_size);

    client_id = id;
    mutex = dr_mutex_create();

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx != -1);
    /* The TLS field provided by DR cannot be directly accessed from the code cache.
     * For better performance, we allocate raw TLS so that we can directly
     * access and update it with a single instruction.
     */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, MEMTRACE_TLS_COUNT, 0))
        DR_ASSERT(false);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "drcachesim client initializing\n");

#ifdef HAS_SNAPPY
    if (op_offline.get_value() && snappy_enabled()) {
        /* Unfortunately libsnappy allocates memory but does not parameterize its
         * allocator, meaning we cannot support it for static linking, so we override
         * the DR_DISALLOW_UNSAFE_STATIC declaration.
         * XXX: Send a patch to libsnappy to parameterize the allocator.
         */
        dr_allow_unsafe_static_behavior();
#    ifdef DRMEMTRACE_STATIC
        NOTIFY(0, "-raw_compress snappy is unsafe with statically linked clients\n");
#    endif
    }
#endif
#ifdef HAS_LZ4
    if (op_offline.get_value() && op_raw_compress.get_value() == "lz4") {
        /* Similarly to libsnappy, lz4 doesn't parameterize its allocator. */
        dr_allow_unsafe_static_behavior();
#    ifdef DRMEMTRACE_STATIC
        NOTIFY(0, "-raw_compress lz4 is unsafe with statically linked clients\n");
#    endif
    }
#endif

    if (op_max_global_trace_refs.get_value() > 0) {
        /* We need the same is-buffer-zero checks in the instrumentation. */
        thread_filtering_enabled = true;
    }

    if (op_use_physical.get_value() && !physaddr_t::global_init())
        FATAL("Unable to open pagemap for physical addresses: check privileges.\n");
}

/* To support statically linked multiple clients, we add drmemtrace_client_main
 * as the real client init function and make dr_client_main a weak symbol.
 * We could also use alias to link dr_client_main to drmemtrace_client_main.
 * A simple call won't add too much overhead, and works both in Windows and Linux.
 * To automate the process and minimize the code change, we should investigate the
 * approach that uses command-line link option to alias two symbols.
 */
DR_EXPORT WEAK void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drmemtrace_client_main(id, argc, argv);
}
