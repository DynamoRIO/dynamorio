/* ******************************************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

/***************************************************************************
 * DrMemtrace trace data output logic.
 */

#include "output.h"

#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include "dr_api.h"
#include "drmemtrace.h"
#include "drmgr.h"
#include "droption.h"
#include "drx.h"
#include "instru.h"
#include "named_pipe.h"
#include "options.h"
#include "physaddr.h"
#include "raw2trace.h"
#include "trace_entry.h"
#include "tracer.h"
#include "utils.h"
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

namespace dynamorio {
namespace drmemtrace {

/***************************************************************************
 * Trace thresholds.
 */

/* Similarly to -trace_after_instrs, we use thread-local counters to avoid
 * synchronization costs and only add to the global every N counts.
 */
static std::atomic<uint64> cur_window_instr_count;

static ptr_int_t
get_local_window(per_thread_t *data)
{
    return *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_WINDOW);
}

static ptr_int_t
get_local_mode(per_thread_t *data)
{
    return *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_MODE);
}

static void
set_local_mode(per_thread_t *data, ptr_int_t mode)
{
    *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_MODE) = mode;
}

static uint64
local_instr_count_threshold(uint64 trace_for_instrs)
{
    if (trace_for_instrs > INSTR_COUNT_LOCAL_UNIT * 10)
        return INSTR_COUNT_LOCAL_UNIT;
    else {
        /* For small windows, use a smaller add-to-global trigger. */
        return trace_for_instrs / 10;
    }
}

// Returns whether we've reached the end of this tracing window.
static bool
count_traced_instrs(void *drcontext, uintptr_t toadd, uint64 trace_for_instrs)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    data->cur_window_instr_count += toadd;
    if (data->cur_window_instr_count >= local_instr_count_threshold(trace_for_instrs)) {
        uint64 newval = cur_window_instr_count.fetch_add(data->cur_window_instr_count,
                                                         std::memory_order_release) +
            // fetch_add returns old value.
            data->cur_window_instr_count;
        data->cur_window_instr_count = 0;
        if (newval >= trace_for_instrs)
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
    DR_ASSERT(tracing_mode.load(std::memory_order_acquire) == BBDUP_MODE_TRACE);
    tracing_mode.store(BBDUP_MODE_COUNT, std::memory_order_release);
    cur_window_instr_count.store(0, std::memory_order_release);
    dr_mutex_unlock(mutex);
}

/***************************************************************************
 * Buffer writing to disk.
 */

static int notify_beyond_global_max_once;
static volatile bool exited_process;

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
    if (op_L0_filter_until_instrs.get_value()) {
        file_type = static_cast<offline_file_type_t>(
            file_type | OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP);
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
    if (op_instr_encodings.get_value()) {
        // This is generally only for online tracing, as raw2trace adds this
        // flag during post-processing for offline.
        file_type =
            static_cast<offline_file_type_t>(file_type | OFFLINE_FILE_TYPE_ENCODINGS);
    }
#ifdef BUILD_PT_TRACER
    if (op_enable_kernel_tracing.get_value()) {
        file_type = static_cast<offline_file_type_t>(file_type |
                                                     OFFLINE_FILE_TYPE_KERNEL_SYSCALLS);
    }
#endif
    file_type = static_cast<offline_file_type_t>(
        file_type |
        IF_X86_ELSE(
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_X86_64, OFFLINE_FILE_TYPE_ARCH_X86_32),
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_AARCH64, OFFLINE_FILE_TYPE_ARCH_ARM32)));
    if (!op_L0I_filter.get_value()) {
        file_type = static_cast<offline_file_type_t>(file_type |
                                                     OFFLINE_FILE_TYPE_SYSCALL_NUMBERS);
    }
#ifdef LINUX
    file_type =
        static_cast<offline_file_type_t>(file_type | OFFLINE_FILE_TYPE_BLOCKING_SYSCALLS);
#endif
    return file_type;
}

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

int
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

void
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
            data->zstream.avail_out = static_cast<uInt>(max_buf_size);
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
        file_t new_file = file_ops_func.call_open_file(
            buf, flags, dr_get_thread_id(drcontext), window_num);
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

/* Appends just the thread header (not the unit/buffer header).
 * Returns the size of the added thread header.
 */
static size_t
prepend_offline_thread_header(void *drcontext)
{
    DR_ASSERT(op_offline.get_value());
    /* Write initial headers at the top of the first buffer. */
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    size_t size = reinterpret_cast<offline_instru_t *>(instru)->append_thread_header(
        data->buf_base, dr_get_thread_id(drcontext), get_file_type());
    BUF_PTR(data->seg_base) = data->buf_base + size;
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
                data->zstream.avail_in = static_cast<uInt>(size);
                int res;
                do {
                    data->zstream.next_out = (Bytef *)data->buf_compressed;
                    data->zstream.avail_out = static_cast<uInt>(max_buf_size);
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
// For a new window, appends the thread headers, but not the unit headers;
// returns true if that happens else returns false.
static bool
set_local_window(void *drcontext, ptr_int_t value)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    bool prepended = false;
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
                prepended = true;
            }
            // We delay opening the next window's file to avoid an empty final file.
            // The initial file is opened at thread init.
            if (data->file != INVALID_FILE && value > 0 && op_split_windows.get_value())
                close_thread_file(drcontext);
        }
    }
    *(ptr_int_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_WINDOW) = value;
    return prepended;
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
    // We can split before the start of each sequence: we don't want to split
    // an <encoding, instruction, address> combination.
    return (op_instr_encodings.get_value()
                ? type == TRACE_TYPE_ENCODING
                : (type_is_instr(type) || type == TRACE_TYPE_INSTR_MAYBE_FETCH)) ||
        type == TRACE_TYPE_MARKER || type == TRACE_TYPE_THREAD_EXIT ||
        op_L0I_filter.get_value();
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
    uintptr_t mode = tracing_mode.load(std::memory_order_acquire);
    if (mode != BBDUP_MODE_L0_FILTER)
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
    NOTIFY(4, "%s: type=%s (%2d) virt=%p phys=%p\n", __FUNCTION__, trace_type_names[type],
           type, virt, phys);
    if (!success) {
        // XXX i#1735: Unfortunately this happens; currently we use the virtual
        // address and continue.
        // Cases where xl8 fails include:
        // - vsyscall/kernel page,
        // - wild access (NULL or very large bogus address) by app
        // - page is swapped out (unlikely since we're querying *after* the
        //   the app just accessed, but could happen)
        NOTIFY(1, "virtual2physical translation failure for type=%s (%2d) addr=%p\n",
               trace_type_names[type], type, virt);
        phys = virt;
    }
    // We keep the main entries as virtual but add markers showing
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
        }
        // With timestamps at buffer start, we want to use the same timestamp+cpu
        // to avoid out-of-order times.
        memcpy(v2p_ptr, data->buf_base + header_size - buf_hdr_slots_size,
               buf_hdr_slots_size);
        v2p_ptr += buf_hdr_slots_size;
        *emitted = true;
    }
    if (v2p_ptr + 2 * instru->sizeof_entry() - data->v2p_buf >=
        static_cast<ssize_t>(get_v2p_buffer_size())) {
        NOTIFY(1, "Reached v2p buffer limit: emitting multiple times\n");
        data->num_phys_markers +=
            output_buffer(drcontext, data, data->v2p_buf, v2p_ptr, header_size);
        v2p_ptr = data->v2p_buf;
        memcpy(v2p_ptr, data->buf_base + header_size - buf_hdr_slots_size,
               buf_hdr_slots_size);
        v2p_ptr += buf_hdr_slots_size;
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
            NOTIFY(2, "Emitting physaddr for next page %p for type=%s (%2d), addr=%p\n",
                   virt_page + page_size, trace_type_names[type], type, virt);
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

// We are looking for the first unfiltered record so that we can insert a FILTER_ENDPOINT
// marker to demarcate filtered and unfiltered records. If there is a PC record with 1
// instr, we cannot be sure if it is a filtered record or an unfiltered record (unless it
// has memref records, in which case we know that it is unfiltered). For such records, we
// err on the side of treating it as a filtered record.
offline_entry_t *
find_unfiltered_record(byte *start, byte *end)
{
    // The end variable points to the next writable location.
    offline_entry_t *last = (offline_entry_t *)(end - sizeof(offline_entry_t));

    int num_memrefs = 0;

    for (offline_entry_t *entry = last; entry >= (offline_entry_t *)start; entry--) {
        if (entry->pc.type == OFFLINE_TYPE_PC) {
            NOTIFY(4, "PC: instr count = %d, num_memrefs = %d\n", entry->pc.instr_count,
                   num_memrefs);
            if ((entry->pc.instr_count == 1 && num_memrefs > 0) ||
                entry->pc.instr_count > 1) {
                NOTIFY(4, "Found unfiltered entry=%d\n",
                       entry - (offline_entry_t *)start);
                return entry;
            }
            // We can stop once we reach a PC record
            return NULL;
        } else if (entry->addr.type == OFFLINE_TYPE_MEMREF ||
                   entry->addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
            num_memrefs++;
        }
    }

    return NULL;
}

// Should be invoked only in the middle of an active tracing window.
void
process_and_output_buffer(void *drcontext, bool skip_size_cap)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    byte *mem_ref, *buf_ptr;
    byte *redzone;
    bool do_write = true;
    uint current_num_refs = 0;

    if (op_offline.get_value() && data->file == INVALID_FILE) {
        // We've delayed opening a new window file to avoid an empty final file.
        DR_ASSERT(has_tracing_windows() || op_trace_after_instrs.get_value() > 0 ||
                  attached_midway);
        open_new_thread_file(drcontext, get_local_window(data));
    }

    size_t header_size = buf_hdr_slots_size;
    // For online we already wrote the thread header but for offline it is in
    // the first buffer.
    if (data->has_thread_header && op_offline.get_value())
        header_size += data->init_header_size;

    if (align_attach_detach_endpoints()) {
        // This is the attach counterpart to instru_t::set_frozen_timestamp(): we place
        // timestamps at buffer creation, but that can be before we're fully attached.
        // We update any too-early timestamps to reflect when we actually started
        // tracing.  (Switching back to timestamps at buffer output is actually
        // worse as we then have the identical frozen timestamp for all the flushes
        // during detach, plus they are all on the same cpu too.)
        uint64 min_timestamp = attached_timestamp.load(std::memory_order_acquire);
        if (min_timestamp == 0) {
            // This data is too early: we drop it.
            NOTIFY(1, "Dropping too-early data for T%zd\n", dr_get_thread_id(drcontext));
            BUF_PTR(data->seg_base) = data->buf_base + header_size;
            return;
        }
        size_t stamp_offs =
            header_size > buf_hdr_slots_size ? header_size - buf_hdr_slots_size : 0;
        instru->refresh_unit_header_timestamp(data->buf_base + stamp_offs, min_timestamp);
    }

    buf_ptr = BUF_PTR(data->seg_base);
    // We may get called with nothing to write: e.g., on a syscall for
    // -L0I_filter and -L0D_filter.
    if (buf_ptr == data->buf_base + header_size) {
        ptr_int_t window = -1;
        if (has_tracing_windows()) {
            // If there is no data to write, we do not emit an empty header here
            // unless this thread is exiting (set_local_window will also write out
            // entries on a window change for offline; for online multiple windows
            // may pass with no output at all for an idle thread).
            window = tracing_window.load(std::memory_order_acquire);
            if (set_local_window(drcontext, window))
                header_size = data->init_header_size;
        }
        // Refresh header.
        append_unit_header(drcontext, data->buf_base + header_size - buf_hdr_slots_size,
                           dr_get_thread_id(drcontext), window);
        return;
    }

    // Clear after we know we're not dropping the data for non-size-cap reasons.
    data->has_thread_header = false;

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
    // Switch to instruction-tracing mode by adding FILTER_ENDPOINT marker if another
    // thread triggered the switch.
    ptr_int_t mode = tracing_mode.load(std::memory_order_acquire);
    if (get_local_mode(data) != mode) {
        if (get_local_mode(data) == BBDUP_MODE_L0_FILTER) {
            NOTIFY(0, "Thread %d: filter mode changed\n", dr_get_thread_id(drcontext));

            // If a switch occurred, then it is possible that the buffer
            // contains a mix of filtered and unfiltered records. We look for the first
            // unfiltered record and if such a record is found, we insert the
            // FILTER_ENDPOINT marker before it.
            //
            // Only the most recent basic block can have unfiltered data. Once the mode
            // switch is made, it will take effect in some thread at the top of a block in
            // the drbbdup mode dispatch. Then at the bottom of that block it will hit the
            // new check and enter the clean call. So if we walk backward to the first PC
            // entry we find (since unfiltered has just one PC at the start) that must be
            // the transition point. However, if the mode change occurred after dispatch
            // and before the end of block check, then we will have filtered entries in
            // the buffer.
            //
            // So if this PC has just 1 instr (and no memrefs), it coule be either a
            // filtered or an unfiltered entry. We assume it is a filtered record and
            // assume that the transition occurred at a later point.
            byte *end =
                (byte *)find_unfiltered_record(data->buf_base + header_size, buf_ptr);
            if (end == NULL) {
                // Add a FILTER_ENDPOINT marker to indicate that filtering stops here.
                buf_ptr +=
                    instru->append_marker(buf_ptr, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0);
            } else {
                // Write the filtered data.
                output_buffer(drcontext, data, data->buf_base, end, 0);
                // Add the FILTER_ENDPOINT.
                offline_entry_t marker[2];
                byte *marker_buf = (byte *)&marker[0];
                int size = instru->append_marker(marker_buf,
                                                 TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0);
                DR_ASSERT(size <= (int)sizeof(marker));
                output_buffer(drcontext, data, marker_buf, marker_buf + size, 0);

                // Set the pointer to unfiltered data.
                data->buf_base = end;
            }
        }
        set_local_mode(data, mode);
    }
    // When -L0_filter_until_instrs is used with -max_trace_size/-max_global_trace_refs,
    // the max size/refs limit applies to the full trace and not the filtered trace so we
    // can skip the check in filter mode.
    if (!skip_size_cap && mode != BBDUP_MODE_L0_FILTER &&
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
        if (op_L0_filter_until_instrs.get_value() && mode == BBDUP_MODE_L0_FILTER) {
            uintptr_t toadd =
                *(uintptr_t *)TLS_SLOT(data->seg_base, MEMTRACE_TLS_OFFS_ICOUNT);
            bool reached_L0_filter_until_instrs_limit = count_traced_instrs(
                drcontext, toadd, op_L0_filter_until_instrs.get_value());
            if (reached_L0_filter_until_instrs_limit) {
                NOTIFY(0, "Adding filter endpoint marker for -L0_filter_until_instrs\n");
                size_t add =
                    instru->append_marker(buf_ptr, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0);
                buf_ptr += add;
                NOTIFY(0,
                       "Hit tracing window #%zd filter limit: switching to full trace.\n",
                       tracing_window.load(std::memory_order_acquire));

                tracing_mode.store(BBDUP_MODE_TRACE, std::memory_order_release);
                set_local_mode(data, BBDUP_MODE_TRACE);
            }
        } else if (op_trace_for_instrs.get_value() > 0) {
            bool hit_window_end = false;
            for (mem_ref = data->buf_base + header_size; mem_ref < buf_ptr;
                 mem_ref += instru->sizeof_entry()) {
                if (!window_changed && !hit_window_end &&
                    op_trace_for_instrs.get_value() > 0) {
                    hit_window_end =
                        count_traced_instrs(drcontext, instru->get_instr_count(mem_ref),
                                            op_trace_for_instrs.get_value());
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
    BUF_PTR(data->seg_base) = data->buf_base;
    ptr_int_t window = -1;
    if (has_tracing_windows()) {
        window = tracing_window.load(std::memory_order_acquire);
        set_local_window(drcontext, window);
    }
    BUF_PTR(data->seg_base) += append_unit_header(drcontext, BUF_PTR(data->seg_base),
                                                  dr_get_thread_id(drcontext), window);
    num_refs_racy += current_num_refs;
    if (mode == BBDUP_MODE_L0_FILTER) {
        num_filter_refs_racy += current_num_refs;
    }
    // When -L0_filter_until_instrs is used with -exit_after_tracing, the
    // exit_after_tracing limit applies to the full trace and not the filtered trace so we
    // can skip this check in filter mode.
    if (mode != BBDUP_MODE_L0_FILTER && op_exit_after_tracing.get_value() > 0 &&
        (num_refs_racy - num_filter_refs_racy) > op_exit_after_tracing.get_value()) {
        dr_mutex_lock(mutex);
        if (!exited_process) {
            exited_process = true;
            dr_mutex_unlock(mutex);
            // XXX i#2644: we would prefer detach_after_tracing rather than exiting
            // the process but that requires a client-triggered detach so for now
            // we settle for exiting.
            NOTIFY(0, "Exiting process after ~" UINT64_FORMAT_STRING " references.\n",
                   num_refs_racy - num_filter_refs_racy);
            dr_exit_process(0);
        }
        dr_mutex_unlock(mutex);
    }
}

void
init_buffers(per_thread_t *data)
{
    create_buffer(data);
    if (op_use_physical.get_value()) {
        create_v2p_buffer(data);
    }
}

void
init_thread_io(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    byte *proc_info;

    NOTIFY(1, "T" TIDFMT " in init_thread_io.\n", dr_get_thread_id(drcontext));
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
    set_local_mode(data, tracing_mode.load(std::memory_order_acquire));

    if (op_offline.get_value()) {
        if (is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire))) {
            open_new_thread_file(drcontext, get_local_window(data));
        }
        if (!has_tracing_windows()) {
            data->init_header_size = prepend_offline_thread_header(drcontext);
        } else {
            // set_local_window() called prepend_offline_thread_header().
        }
        BUF_PTR(data->seg_base) +=
            append_unit_header(drcontext, BUF_PTR(data->seg_base),
                               dr_get_thread_id(drcontext), get_local_window(data));
        if (op_L0_filter_until_instrs.get_value()) {
            // If we have switched to instruction trace already, then add a
            // FILTER_ENDPOINT marker.
            uintptr_t mode = tracing_mode.load(std::memory_order_acquire);
            if (mode == BBDUP_MODE_TRACE) {
                BUF_PTR(data->seg_base) += instru->append_marker(
                    BUF_PTR(data->seg_base), TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0);
            }
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
        data->init_header_size =
            append_unit_header(drcontext, data->buf_base, dr_get_thread_id(drcontext),
                               get_local_window(data));
        BUF_PTR(data->seg_base) = data->buf_base + data->init_header_size;
    }
}

void
exit_thread_io(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

#ifdef UNIX
    /**
     * i#2384:
     * On Linux, the thread exit event may be invoked twice for the same thread
     * if that thread is alive during a process fork, but doesn't call the fork
     * itself.  The first time the event callback is executed from the fork child
     * immediately after the fork, the second time it is executed during the
     * regular thread exit.
     * data->file could be already closed. Write file operation is fail
     * and it is asserted.
     */
    if (dr_get_process_id() != dr_get_process_id_from_drcontext(drcontext)) {
        return;
    }
#endif

    if (is_in_tracing_mode(tracing_mode.load(std::memory_order_acquire)) ||
        (has_tracing_windows() && !op_split_windows.get_value()) ||
        // For attach we switch to BBDUP_MODE_NOP but still need to finalize
        // each thread.  However, we omit threads that did nothing the entire time
        // we were attached.
        (align_attach_detach_endpoints() &&
         (data->bytes_written > 0 ||
          BUF_PTR(data->seg_base) - data->buf_base >
              static_cast<ssize_t>(data->init_header_size + buf_hdr_slots_size)))) {
        BUF_PTR(data->seg_base) += instru->append_thread_exit(
            BUF_PTR(data->seg_base), dr_get_thread_id(drcontext));

        ptr_int_t window = get_local_window(data);
        process_and_output_buffer(drcontext,
                                  /* If this thread already wrote some data, include
                                   * its exit even if we're over a size limit.
                                   */
                                  data->bytes_written > 0);
        if (get_local_window(data) != window) {
            BUF_PTR(data->seg_base) += instru->append_thread_exit(
                BUF_PTR(data->seg_base), dr_get_thread_id(drcontext));
            process_and_output_buffer(drcontext, data->bytes_written > 0);
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
}

void
init_io()
{
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

    DR_ASSERT(cur_window_instr_count.is_lock_free());
}

void
exit_io()
{
    notify_beyond_global_max_once = 0;
}

} // namespace drmemtrace
} // namespace dynamorio
