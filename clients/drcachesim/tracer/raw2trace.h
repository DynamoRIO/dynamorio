/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

/* raw2trace.h: shared defines between the tracer and the converter.
 */

#ifndef _RAW2TRACE_H_
#define _RAW2TRACE_H_ 1

/**
 * @file drmemtrace/raw2trace.h
 * @brief DrMemtrace offline trace post-processing customization.
 */

#include "dr_api.h"
#include "drmemtrace.h"
#include "drcovlib.h"
#include <array>
#include <atomic>
#include <memory>
#include "trace_entry.h"
#include <fstream>
#include "hashtable.h"
#include <vector>

#define OUTFILE_PREFIX "drmemtrace"
#define OUTFILE_SUFFIX "raw"
#define OUTFILE_SUBDIR "raw"
#define TRACE_SUBDIR "trace"
#ifdef HAS_ZLIB
#    define TRACE_SUFFIX "trace.gz"
#else
#    define TRACE_SUFFIX "trace"
#endif

struct module_t {
    module_t(const char *path, app_pc orig, byte *map, size_t size, bool external = false)
        : path(path)
        , orig_base(orig)
        , map_base(map)
        , map_size(size)
        , is_external(external)
    {
    }
    const char *path;
    app_pc orig_base;
    byte *map_base;
    size_t map_size;
    bool is_external; // If true, the data is embedded in drmodtrack custom fields.
};

/**
 * instr_summary_t is a compact encapsulation of the information needed by trace
 * conversion from decoded instructions.
 */
struct instr_summary_t final {
    instr_summary_t()
    {
    }

    /**
     * Populates a pre-allocated instr_summary_t description, from the instruction found
     * at pc. Updates pc to the next instruction. Optionally logs translation details
     * (using orig_pc and verbosity).
     */
    static bool
    construct(void *dcontext, INOUT app_pc *pc, app_pc orig_pc, OUT instr_summary_t *desc,
              uint verbosity = 0);

    /**
     * Get the pc after the instruction that was used to produce this instr_summary_t.
     */
    app_pc
    next_pc() const
    {
        return next_pc_;
    }

private:
    template <typename T> friend class trace_converter_t;

    byte
    length() const
    {
        return length_;
    }
    uint16_t
    type() const
    {
        return type_;
    }
    uint16_t
    prefetch_type() const
    {
        return prefetch_type_;
    }

    bool
    reads_memory() const
    {
        return TESTANY(kReadsMemMask, packed_);
    }
    bool
    writes_memory() const
    {
        return TESTANY(kWritesMemMask, packed_);
    }
    bool
    is_prefetch() const
    {
        return TESTANY(kIsPrefetchMask, packed_);
    }
    bool
    is_flush() const
    {
        return TESTANY(kIsFlushMask, packed_);
    }
    bool
    is_cti() const
    {
        return TESTANY(kIsCtiMask, packed_);
    }

    const opnd_t &
    mem_src_at(size_t pos) const
    {
        return mem_srcs_and_dests_[pos];
    }
    const opnd_t &
    mem_dest_at(size_t pos) const
    {
        return mem_srcs_and_dests_[num_mem_srcs_ + pos];
    }
    size_t
    num_mem_srcs() const
    {
        return num_mem_srcs_;
    }
    size_t
    num_mem_dests() const
    {
        return mem_srcs_and_dests_.size() - num_mem_srcs_;
    }

    static const int kReadsMemMask = 0x0001;
    static const int kWritesMemMask = 0x0002;
    static const int kIsPrefetchMask = 0x0004;
    static const int kIsFlushMask = 0x0008;
    static const int kIsCtiMask = 0x0010;

    instr_summary_t(const instr_summary_t &other) = delete;
    instr_summary_t &
    operator=(const instr_summary_t &) = delete;
    instr_summary_t(instr_summary_t &&other) = delete;
    instr_summary_t &
    operator=(instr_summary_t &&) = delete;

    uint16_t type_ = 0;
    uint16_t prefetch_type_ = 0;
    byte length_ = 0;
    app_pc next_pc_ = 0;

    // Squash srcs and dests to save memory usage. We may want to
    // bulk-allocate pages of instr_summary_t objects, instead
    // of piece-meal allocating them on the heap one at a time.
    // One vector and a byte is smaller than 2 vectors.
    std::vector<opnd_t> mem_srcs_and_dests_;
    uint8_t num_mem_srcs_ = 0;
    byte packed_ = 0;
};

/**
 * Functions for encoding memtrace data headers. Each function returns the number of bytes
 * the write operation required: sizeof(trace_entry_t). The buffer is assumed to be
 * sufficiently large.
 */
struct trace_metadata_writer_t {
    static int
    write_thread_exit(byte *buffer, thread_id_t tid);
    static int
    write_marker(byte *buffer, trace_marker_type_t type, uintptr_t val);
    static int
    write_iflush(byte *buffer, addr_t start, size_t size);
    static int
    write_pid(byte *buffer, process_id_t pid);
    static int
    write_tid(byte *buffer, thread_id_t tid);
    static int
    write_timestamp(byte *buffer, uint64 timestamp);
};

/**
 * Functions for decoding and verifying raw memtrace data headers.
 */
struct trace_metadata_reader_t {
    static bool
    is_thread_start(const offline_entry_t *entry, OUT std::string *error);
    static std::string
    check_entry_thread_start(const offline_entry_t *entry);
};

/**
 * module_mapper_t maps and unloads application modules.
 * Using it assumes a dr_context has already been setup.
 * This class is not thread-safe.
 */
class module_mapper_t final {
public:
    /**
     * Parses and iterates over the list of modules.  This is provided to give the user a
     * method for iterating modules in the presence of the custom field used by drmemtrace
     * that prevents direct use of drmodtrack_offline_read(). Its parsing of the module
     * data will invoke \p parse_cb, which should advance the module data pointer passed
     * in \p src and return it as its return value (or nullptr on error), returning
     * the resulting parsed data in \p data.  The \p data pointer will afterwards be
     * passed to both \p process_cb, which can update the module path inside \p info
     * (and return a non-empty string on error), and \b free_cb, which can perform
     * cleanup.
     *
     * The callbacks will only be called during object construction.
     *
     * On success, calls the \p process_cb function for every module in the list.
     * On failure, get_last_error() is non-empty, and indicates the cause.
     */
    static std::unique_ptr<module_mapper_t>
    create(const char *module_map_in,
           const char *(*parse_cb)(const char *src, OUT void **data) = nullptr,
           std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                     void *user_data) = nullptr,
           void *process_cb_user_data = nullptr, void (*free_cb)(void *data) = nullptr,
           uint verbosity_in = 0)
    {
        return std::unique_ptr<module_mapper_t>(
            new module_mapper_t(module_map_in, parse_cb, process_cb, process_cb_user_data,
                                free_cb, verbosity_in));
    }

    /**
     * All APIs on this type, including constructor, may fail. get_last_error() returns
     * the last error message. The object should be considered unusable if
     * !get_last_error().empty().
     */
    std::string
    get_last_error(void) const
    {
        return last_error;
    }

    /**
     * module_t vector corresponding to the application modules. Lazily loads and caches
     * modules. If the object is invalid, returns an empty vector. The user may check
     * get_last_error() to ensure no error has occurred, or get the applicable error
     * message.
     */
    const std::vector<module_t> &
    get_loaded_modules()
    {
        if (last_error.empty() && modvec.empty())
            read_and_map_modules();
        return modvec;
    }

    /**
     * This interface is meant to be used with a final trace rather than a raw
     * trace, using the module log file saved from the raw2trace conversion.
     * After the a call to get_loaded_modules(), this routine may be used
     * to convert an instruction program counter in a trace into an address in the
     * current process where the instruction bytes for that instruction are mapped,
     * allowing decoding for obtaining further information than is stored in the trace.
     * Returns the mapped address. Check get_last_error() if an error occurred.
     */
    app_pc
    find_mapped_trace_address(app_pc trace_address);

    /**
     * This is identical to find_mapped_trace_address() but it also returns the
     * bounds of the containing region, allowing the caller to perform its own
     * mapping for any address that is also within those bounds.
     */
    app_pc
    find_mapped_trace_bounds(app_pc trace_address, OUT app_pc *module_start,
                             OUT size_t *module_size);

    /**
     * Unload modules loaded with read_and_map_modules(), freeing associated resources.
     */
    ~module_mapper_t();

private:
    module_mapper_t(const char *module_map_in,
                    const char *(*parse_cb)(const char *src, OUT void **data) = nullptr,
                    std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                              void *user_data) = nullptr,
                    void *process_cb_user_data = nullptr,
                    void (*free_cb)(void *data) = nullptr, uint verbosity_in = 0);
    module_mapper_t(const module_mapper_t &) = delete;
    module_mapper_t &
    operator=(const module_mapper_t &) = delete;
#ifndef WINDOWS
    module_mapper_t(module_mapper_t &&) = delete;
    module_mapper_t &
    operator=(module_mapper_t &&) = delete;
#endif
    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    void
    read_and_map_modules(void);

    std::string
    do_module_parsing();

    const char *modmap = nullptr;
    void *modhandle = nullptr;
    std::vector<module_t> modvec;
    void (*const cached_user_free)(void *data) = nullptr;

    // Custom module fields that use drmodtrack are global.
    static const char *(*user_parse)(const char *src, OUT void **data);
    static void (*user_free)(void *data);
    static const char *
    parse_custom_module_data(const char *src, OUT void **data);
    static void
    free_custom_module_data(void *data);
    static bool has_custom_data_global;

    bool has_custom_data = false;

    // We store module info for do_module_parsing.
    std::vector<drmodtrack_info_t> modlist;
    std::string (*user_process)(drmodtrack_info_t *info, void *data,
                                void *user_data) = nullptr;
    void *user_process_data = nullptr;
    app_pc last_orig_base = 0;
    size_t last_map_size = 0;
    byte *last_map_base = nullptr;

    uint verbosity = 0;
    std::string last_error;
};

/**
 * Header of raw trace.
 */
struct trace_header_t {
    process_id_t pid;
    thread_id_t tid;
    uint64 timestamp;
};

// XXX: DR should export this
#define INVALID_THREAD_ID 0

/**
 * #trace_converter_t is a reusable component that encapsulates raw trace conversion.
 *
 * Conversion happens from a data source abstracted by the type parameter T. We make no
 * assumption about how thread buffers are organized. We do assume the internal
 * composition of thread buffers is "as written" by the thread. For example, all thread
 * buffers belonging to different threads may be in a separate files; or buffers may be
 * co-located in one large file, or spread accross multiple, mixed-thread files.
 *
 * #trace_converter_t expects to be instantiated with its type template T which should
 * provide the following APIs.  These pass through an opaque pointer which provides
 * per-traced-thread-local data to the converter:
 *
 * <UL> <LI>const offline_entry_t *get_next_entry(void *tls)
 *
 * Point to the next offline entry_t. There is no assumption about the underlying source
 * of the data, and #trace_converter_t will not attempt to dereference past the provided
 * pointer.</LI>
 *
 * <LI>void unread_last_entry(void *tls)
 *
 * Ensure that the next call to get_next_entry() re-reads the last value.</LI>
 *
 * <LI>trace_entry_t *get_write_buffer(void *tls)
 *
 * Return a writable buffer guaranteed to be at least #WRITE_BUFFER_SIZE large.
 *  get_write_buffer() may reuse the same buffer after write() or write_delayed_branches()
 * is called.</LI>
 *
 * <LI>bool write(void *tls, const trace_entry_t *start, const trace_entry_t *end)
 *
 * Writes the converted traces between start and end, where end is past the last
 * item to write. Both start and end are assumed to be pointers inside a buffer
 * returned by get_write_buffer().</LI>
 *
 * <LI>std::string write_delayed_branches(const trace_entry_t *start, const trace_entry_t
 * *end)
 *
 * Similar to write(), but treat the provided traces as delayed branches: if they
 * are the last values in a record, they belong to the next record of the same
 * thread.</LI>
 *
 * <LI>std::string on_thread_end(void *tls)
 *
 * Callback notifying the currently-processed thread has exited. #trace_converter_t
 * extenders are expected to track record metadata themselves. #trace_converter_t offers
 * APIs for extracting that metadata.</LI>
 *
 * <LI>void log(uint level, const char *fmt, ...)
 *
 * Implementers are given the opportunity to implement their own logging. The level
 * parameter represents severity: the lower the level, the higher the severity.</LI>
 *
 * <LI>const instr_summary_t *get_instr_summary(void *tls, uint64 modidx, uint64 modoffs,
 * INOUT app_pc *pc, app_pc orig)
 *
 * Return the #instr_summary_t representation of the instruction at *pc,
 * updating the value at pc to the PC of the next instruction. It is assumed the app
 * binaries have already been loaded using #module_mapper_t, and the values at *pc point
 * within memory mapped by the module mapper. This API provides an opportunity to cache
 * decoded instructions.</LI>
 *
 * <LI>void set_prev_instr_rep_string(void *tls, bool value)
 *
 * Sets a per-traced-thread cached flag that is read by was_prev_instr_rep_string().
 * </LI>
 *
 * <LI>bool was_prev_instr_rep_string(void *tls)
 *
 * Queries a per-traced-thread cached flag that is set by set_prev_instr_rep_string().
 * </LI>
 * </UL>
 */
template <typename T> class trace_converter_t {
#define DR_CHECK(val, msg) \
    do {                   \
        if (!(val))        \
            return msg;    \
    } while (0)
public:
    trace_converter_t(const trace_converter_t &) = delete;
    trace_converter_t &
    operator=(const trace_converter_t &) = delete;
#ifndef WINDOWS
    trace_converter_t(trace_converter_t &&) = default;
    trace_converter_t &
    operator=(trace_converter_t &&) = default;
#endif

protected:
    /**
     * Construct a new #trace_converter_t object. If a nullptr dcontext_in is passed,
     * creates a new DR context va dr_standalone_init().
     */
    trace_converter_t(void *dcontext_in)
        : dcontext(dcontext_in == nullptr ? dr_standalone_init() : dcontext_in)
    {
    }

    /**
     * Convert starting from in_entry, and reading more entries as required.
     * Sets end_of_record to true if processing hit the end of a record.
     * set_modvec() must have been called by the implementation before calling this API.
     */
    std::string
    process_offline_entry(void *tls, const offline_entry_t *in_entry, thread_id_t tid,
                          OUT bool *end_of_record, OUT bool *last_bb_handled)
    {
        trace_entry_t *buf_base = impl()->get_write_buffer(tls);
        byte *buf = reinterpret_cast<byte *>(buf_base);
        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED) {
            if (in_entry->extended.ext == OFFLINE_EXT_TYPE_FOOTER) {
                DR_CHECK(tid != INVALID_THREAD_ID, "Missing thread id");
                impl()->log(2, "Thread %d exit\n", (uint)tid);
                buf += trace_metadata_writer_t::write_thread_exit(buf, tid);
                *end_of_record = true;
                if (!impl()->write(tls, buf_base, reinterpret_cast<trace_entry_t *>(buf)))
                    return "Failed to write to output file";
                // Let the user determine what other actions to take, e.g. account for
                // the ending of the current thread, etc.
                return impl()->on_thread_end(tls);
            } else if (in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER) {
                buf += trace_metadata_writer_t::write_marker(
                    buf, (trace_marker_type_t)in_entry->extended.valueB,
                    (uintptr_t)in_entry->extended.valueA);
                impl()->log(3, "Appended marker type %u value %zu\n",
                            (trace_marker_type_t)in_entry->extended.valueB,
                            (uintptr_t)in_entry->extended.valueA);
            } else {
                std::stringstream ss;
                ss << "Invalid extension type " << (int)in_entry->extended.ext;
                return ss.str();
            }
        } else if (in_entry->addr.type == OFFLINE_TYPE_MEMREF ||
                   in_entry->addr.type == OFFLINE_TYPE_MEMREF_HIGH) {
            if (!*last_bb_handled) {
                // For currently-unhandled non-module code, memrefs are handled here
                // where we can easily handle the transition out of the bb.
                trace_entry_t *entry = reinterpret_cast<trace_entry_t *>(buf);
                entry->type = TRACE_TYPE_READ; // Guess.
                entry->size = 1;               // Guess.
                entry->addr = (addr_t)in_entry->combined_value;
                impl()->log(4, "Appended non-module memref to " PFX "\n",
                            (ptr_uint_t)entry->addr);
                buf += sizeof(*entry);
            } else {
                // We should see an instr entry first
                return "memref entry found outside of bb";
            }
        } else if (in_entry->pc.type == OFFLINE_TYPE_PC) {
            DR_CHECK(reinterpret_cast<trace_entry_t *>(buf) == buf_base,
                     "We shouldn't have buffered anything before calling "
                     "append_bb_entries");
            std::string result = append_bb_entries(tls, in_entry, last_bb_handled);
            if (!result.empty())
                return result;
        } else if (in_entry->addr.type == OFFLINE_TYPE_IFLUSH) {
            const offline_entry_t *entry = impl()->get_next_entry(tls);
            if (entry == nullptr || entry->addr.type != OFFLINE_TYPE_IFLUSH)
                return "Flush missing 2nd entry";
            impl()->log(2, "Flush " PFX "-" PFX "\n", (ptr_uint_t)in_entry->addr.addr,
                        (ptr_uint_t)entry->addr.addr);
            buf += trace_metadata_writer_t::write_iflush(
                buf, in_entry->addr.addr,
                (size_t)(entry->addr.addr - in_entry->addr.addr));
        } else {
            std::stringstream ss;
            ss << "Unknown trace type " << (int)in_entry->timestamp.type;
            return ss.str();
        }
        size_t size = reinterpret_cast<trace_entry_t *>(buf) - buf_base;
        DR_CHECK((uint)size < WRITE_BUFFER_SIZE, "Too many entries");
        if (size > 0) {
            if (!impl()->write(tls, buf_base, reinterpret_cast<trace_entry_t *>(buf)))
                return "Failed to write to output file";
        }
        return "";
    }

    /**
     * Read the header of a thread, by calling T's get_next_entry() successively to
     * populate the header values. The timestamp field is populated only
     * for legacy traces.
     */
    std::string
    read_header(void *tls, OUT trace_header_t *header)
    {
        const offline_entry_t *in_entry = impl()->get_next_entry(tls);
        if (in_entry == nullptr)
            return "Failed to read header from input file";
        // Handle legacy traces which have the timestamp first.
        if (in_entry->tid.type == OFFLINE_TYPE_TIMESTAMP) {
            header->timestamp = in_entry->timestamp.usec;
            in_entry = impl()->get_next_entry(tls);
            if (in_entry == nullptr)
                return "Failed to read header from input file";
        }
        DR_ASSERT(in_entry->tid.type == OFFLINE_TYPE_THREAD);
        header->tid = in_entry->tid.tid;
        in_entry = impl()->get_next_entry(tls);
        if (in_entry == nullptr)
            return "Failed to read header from input file";
        DR_ASSERT(in_entry->pid.type == OFFLINE_TYPE_PID);
        header->pid = in_entry->pid.pid;
        return "";
    }

    /**
     * The trace_entry_t buffer returned by get_write_buffer() is assumed to be at least
     * #WRITE_BUFFER_SIZE large.
     */
    static const uint WRITE_BUFFER_SIZE = 64;

    /**
     * The pointer to the DR context.
     */
    void *const dcontext;

    /**
     * Get the module map.
     */
    const std::vector<module_t> &
    modvec() const
    {
        return *modvec_ptr;
    }

    /**
     * Set the module map. Must be called before process_offline_entry() is called.
     */
    void
    set_modvec(const std::vector<module_t> *modvec_in)
    {
        modvec_ptr = modvec_in;
    }

private:
    T *
    impl()
    {
        return static_cast<T *>(this);
    }
    std::string
    append_bb_entries(void *tls, const offline_entry_t *in_entry, OUT bool *handled)
    {
        std::string error = "";
        uint instr_count = in_entry->pc.instr_count;
        const instr_summary_t *instr = nullptr;
        app_pc start_pc = modvec()[in_entry->pc.modidx].map_base + in_entry->pc.modoffs;
        app_pc pc, decode_pc = start_pc;
        if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
            modvec()[in_entry->pc.modidx].map_base == NULL) {
            // FIXME i#2062: add support for code not in a module (vsyscall, JIT, etc.).
            // Once that support is in we can remove the bool return value and handle
            // the memrefs up here.
            impl()->log(3, "Skipping ifetch for %u instrs not in a module\n",
                        instr_count);
            *handled = false;
            return "";
        } else {
            impl()->log(3, "Appending %u instrs in bb " PFX " in mod %u +" PIFX " = %s\n",
                        instr_count, (ptr_uint_t)start_pc, (uint)in_entry->pc.modidx,
                        (ptr_uint_t)in_entry->pc.modoffs,
                        modvec()[in_entry->pc.modidx].path);
        }
        bool skip_icache = false;
        bool truncated = false; // Whether a fault ended the bb early.
        // This indicates that each memref has its own PC entry and that each
        // icache entry does not need to be considered a memref PC entry as well.
        bool instrs_are_separate = false;
        if (instr_count == 0) {
            // L0 filtering adds a PC entry with a count of 0 prior to each memref.
            skip_icache = true;
            instr_count = 1;
            // We set a flag to avoid peeking forward on instr entries.
            instrs_are_separate = true;
        }
        DR_CHECK(!instrs_are_separate || instr_count == 1,
                 "cannot mix 0-count and >1-count");
        for (uint i = 0; !truncated && i < instr_count; ++i) {
            trace_entry_t *buf_start = impl()->get_write_buffer(tls);
            trace_entry_t *buf = buf_start;
            app_pc orig_pc = decode_pc - modvec()[in_entry->pc.modidx].map_base +
                modvec()[in_entry->pc.modidx].orig_base;
            // To avoid repeatedly decoding the same instruction on every one of its
            // dynamic executions, we cache the decoding in a hashtable.
            pc = decode_pc;
            instr = impl()->get_instr_summary(tls, in_entry->pc.modidx,
                                              in_entry->pc.modoffs, &pc, orig_pc);
            if (instr == nullptr) {
                // We hit some error somewhere, and already reported it. Just exit the
                // loop.
                break;
            }
            DR_CHECK(!instr->is_cti() || i == instr_count - 1, "invalid cti");
            // FIXME i#1729: make bundles via lazy accum until hit memref/end.
            buf->type = instr->type();
            if (buf->type == TRACE_TYPE_INSTR_MAYBE_FETCH) {
                // We want it to look like the original rep string, with just one instr
                // fetch for the whole loop, instead of the drutil-expanded loop.
                // We fix up the maybe-fetch here so our offline file doesn't have to
                // rely on our own reader.
                if (!impl()->was_prev_instr_rep_string(tls)) {
                    impl()->set_prev_instr_rep_string(tls, true);
                    buf->type = TRACE_TYPE_INSTR;
                } else {
                    impl()->log(3, "Skipping instr fetch for " PFX "\n",
                                (ptr_uint_t)decode_pc);
                    // We still include the instr to make it easier for core simulators
                    // (i#2051).
                    buf->type = TRACE_TYPE_INSTR_NO_FETCH;
                }
            } else
                impl()->set_prev_instr_rep_string(tls, false);
            buf->size = (ushort)(skip_icache ? 0 : instr->length());
            buf->addr = (addr_t)orig_pc;
            ++buf;
            decode_pc = pc;
            // We need to interleave instrs with memrefs.
            // There is no following memref for (instrs_are_separate && !skip_icache).
            if ((!instrs_are_separate || skip_icache) &&
                // Rule out OP_lea.
                (instr->reads_memory() || instr->writes_memory())) {
                bool interrupted = false;
                for (uint j = 0; j < instr->num_mem_srcs(); j++) {
                    error = append_memref(tls, &buf, instr, instr->mem_src_at(j), false,
                                          &interrupted);
                    if (!error.empty())
                        return error;
                    if (interrupted) {
                        truncated = true;
                        break;
                    }
                }
                for (uint j = 0; !truncated && j < instr->num_mem_dests(); j++) {
                    error = append_memref(tls, &buf, instr, instr->mem_dest_at(j), true,
                                          &interrupted);
                    if (!error.empty())
                        return error;
                    if (interrupted) {
                        truncated = true;
                        break;
                    }
                }
            }
            DR_CHECK((size_t)(buf - buf_start) < WRITE_BUFFER_SIZE, "Too many entries");
            if (instr->is_cti()) {
                // In case this is the last branch prior to a thread switch, buffer it. We
                // avoid swapping threads immediately after a branch so that analyzers can
                // more easily find the branch target.  Doing this in the tracer would
                // incur extra overhead, and in the reader would be more complex and messy
                // than here (and we are ok bailing on doing this for online traces), so
                // we handle it in post-processing by delaying a thread-block-final branch
                // (and its memrefs) to that thread's next block.  This changes the
                // timestamp of the branch, which we live with.
                error = impl()->write_delayed_branches(tls, buf_start, buf);
                if (!error.empty())
                    return error;
            } else {
                if (!impl()->write(tls, buf_start, buf))
                    return "Failed to write to output file";
            }
        }
        *handled = true;
        return "";
    }

    std::string
    append_memref(void *tls, INOUT trace_entry_t **buf_in, const instr_summary_t *instr,
                  opnd_t ref, bool write, OUT bool *interrupted)
    {
        *interrupted = false;
        trace_entry_t *buf = *buf_in;
        const offline_entry_t *in_entry = impl()->get_next_entry(tls);
        bool have_type = false;
        if (in_entry != nullptr && in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
            in_entry->extended.ext == OFFLINE_EXT_TYPE_MEMINFO) {
            // For -L0_filter we have to store the type for multi-memref instrs where
            // we can't tell which memref it is (we'll still come here for the subsequent
            // memref operands but we'll exit early in the check below).
            have_type = true;
            buf->type = in_entry->extended.valueB;
            buf->size = in_entry->extended.valueA;
            impl()->log(4, "Found type entry type %d size %d\n", buf->type, buf->size);
            in_entry = impl()->get_next_entry(tls);
            if (in_entry == nullptr)
                return "Trace ends mid-block";
        }
        if (in_entry == nullptr ||
            (in_entry->addr.type != OFFLINE_TYPE_MEMREF &&
             in_entry->addr.type != OFFLINE_TYPE_MEMREF_HIGH)) {
            // This happens when there are predicated memrefs in the bb, or for a
            // zero-iter rep string loop, or for a multi-memref instr with -L0_filter.
            // For predicated memrefs, they could be earlier, so "instr"
            // may not itself be predicated.
            // XXX i#2015: if there are multiple predicated memrefs, our instr vs
            // data stream may not be in the correct order here.
            impl()->log(4,
                        "Missing memref from predication, 0-iter repstr, or filter "
                        "(next type is 0x" ZHEX64_FORMAT_STRING ")\n",
                        in_entry == nullptr ? 0 : in_entry->combined_value);
            if (in_entry != nullptr) {
                impl()->unread_last_entry(tls);
            }
            return "";
        }
        if (!have_type) {
            if (instr->is_prefetch()) {
                buf->type = instr->prefetch_type();
                buf->size = 1;
            } else if (instr->is_flush()) {
                buf->type = TRACE_TYPE_DATA_FLUSH;
                buf->size = (ushort)opnd_size_in_bytes(opnd_get_size(ref));
            } else {
                if (write)
                    buf->type = TRACE_TYPE_WRITE;
                else
                    buf->type = TRACE_TYPE_READ;
                buf->size = (ushort)opnd_size_in_bytes(opnd_get_size(ref));
            }
        }
        // We take the full value, to handle low or high.
        buf->addr = (addr_t)in_entry->combined_value;
#ifdef X86
        if (opnd_is_near_base_disp(ref) && opnd_get_base(ref) != DR_REG_NULL &&
            opnd_get_index(ref) == DR_REG_NULL) {
            // We stored only the base reg, as an optimization.
            buf->addr += opnd_get_disp(ref);
        }
#endif
        impl()->log(4, "Appended memref type %d size %d to " PFX "\n", buf->type,
                    buf->size, (ptr_uint_t)buf->addr);
        *buf_in = ++buf;
        // To avoid having to backtrack later, we read ahead to see whether this memref
        // faulted.
        in_entry = impl()->get_next_entry(tls);
        if (in_entry == nullptr)
            return "";
        // Put it back.
        impl()->unread_last_entry(tls);
        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
            in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
            in_entry->extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT) {
            // A signal/exception interrupted the bb after the memref.
            impl()->log(4, "Signal/exception interrupted the bb\n");
            *interrupted = true;
        }
        return "";
    }

    const std::vector<module_t> *modvec_ptr = nullptr;

#undef DR_CHECK
};

/**
 * The raw2trace class converts the raw offline trace format to the format
 * expected by analysis tools.  It requires access to the binary files for the
 * libraries and executable that were present during tracing.
 */
class raw2trace_t : public trace_converter_t<raw2trace_t> {
public:
    // module_map, thread_files and out_files are all owned and opened/closed by the
    // caller.  module_map is not a string and can contain binary data.
    raw2trace_t(const char *module_map, const std::vector<std::istream *> &thread_files,
                const std::vector<std::ostream *> &out_files, void *dcontext = NULL,
                unsigned int verbosity = 0, int worker_count = -1);
    virtual ~raw2trace_t();

    /**
     * Adds handling for custom data fields that were stored with each module via
     * drmemtrace_custom_module_data() during trace generation.  When do_conversion()
     * or do_module_parsing() is subsequently called, its parsing of the module data
     * will invoke \p parse_cb, which should advance the module data pointer passed
     * in \p src and return it as its return value (or nullptr on error), returning
     * the resulting parsed data in \p data.  The \p data pointer will later be
     * passed to both \p process_cb, which can update the module path inside \p info
     * (and return a non-empty string on error), and \b free_cb, which can perform
     * cleanup.
     *
     * A custom callback value \p process_cb_user_data can be passed to \p
     * process_cb.  The same is not provided for the other callbacks as they end up
     * using the drmodtrack_add_custom_data() framework where there is no support for
     * custom callback parameters.
     *
     * Returns a non-empty error message on failure.
     */
    std::string
    handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                       std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                                 void *user_data),
                       void *process_cb_user_data, void (*free_cb)(void *data));

    /**
     * Performs the first step of do_conversion() without further action: parses and
     * iterates over the list of modules.  This is provided to give the user a method
     * for iterating modules in the presence of the custom field used by drmemtrace
     * that prevents direct use of drmodtrack_offline_read().
     * On success, calls the \p process_cb function passed to handle_custom_data()
     * for every module in the list, and returns an empty string at the end.
     * Returns a non-empty error message on failure.
     *
     * \deprecated #module_mapper_t should be used instead.
     */
    std::string
    do_module_parsing();

    /**
     * This interface is meant to be used with a final trace rather than a raw
     * trace, using the module log file saved from the raw2trace conversion.
     * This routine first calls do_module_parsing() and then maps each module into
     * the current address space, allowing the user to augment the instruction
     * information in the trace with additional information by decoding the
     * instruction bytes.  The routine find_mapped_trace_address() should be used
     * to convert from memref_t.instr.addr to the corresponding mapped address in
     * the current process.
     * Returns a non-empty error message on failure.
     *
     * \deprecated #module_mapper_t::get_loaded_modules() should be used instead.
     */
    std::string
    do_module_parsing_and_mapping();

    /**
     * This interface is meant to be used with a final trace rather than a raw
     * trace, using the module log file saved from the raw2trace conversion.
     * When do_module_parsing_and_mapping() has been called, this routine can be used
     * to convert an instruction program counter in a trace into an address in the
     * current process where the instruction bytes for that instruction are mapped,
     * allowing decoding for obtaining further information than is stored in the trace.
     * Returns a non-empty error message on failure.
     *
     * \deprecated #module_mapper_t::find_mapped_trace_address() should be used instead.
     */
    std::string
    find_mapped_trace_address(app_pc trace_address, OUT app_pc *mapped_address);

    /**
     * Performs the conversion from raw data to finished trace files.
     * Returns a non-empty error message on failure.
     */
    virtual std::string
    do_conversion();

    static std::string
    check_thread_file(std::istream *f);

protected:
    // Overridable parts of the interface expected by trace_converter_t.
    virtual const offline_entry_t *
    get_next_entry(void *tls);
    virtual void
    unread_last_entry(void *tls);
    virtual std::string
    on_thread_end(void *tls);
    virtual void
    log(uint level, const char *fmt, ...);

    // Per-traced-thread data is stored here and accessed without locks by having each
    // traced thread processed by only one processing thread.
    // This is what trace_converter_t passes as void* to our routines.
    struct raw2trace_thread_data_t {
        raw2trace_thread_data_t()
            : index(0)
            , tid(0)
            , worker(0)
            , thread_file(nullptr)
            , out_file(nullptr)
            , saw_header(false)
            , prev_instr_was_rep_string(false)
            , last_decode_pc(nullptr)
            , last_summary(nullptr)
        {
        }

        int index;
        thread_id_t tid;
        int worker;
        std::istream *thread_file;
        std::ostream *out_file;
        std::string error;
        std::vector<offline_entry_t> pre_read;

        // Used to delay a thread-buffer-final branch to keep it next to its target.
        std::vector<char> delayed_branch;

        // Current trace conversion state.
        bool saw_header;
        offline_entry_t last_entry;
        std::array<trace_entry_t, WRITE_BUFFER_SIZE> out_buf;
        bool prev_instr_was_rep_string;
        app_pc last_decode_pc;
        const instr_summary_t *last_summary;
    };

    std::string
    read_and_map_modules();

    // Processes a raw buffer which must be the next buffer in the desired (typically
    // timestamp-sorted) order for its traced thread.  For concurrent buffer processing,
    // all buffers from any one traced thread must be processed by the same worker
    // thread, both for correct ordering and correct synchronization.
    std::string
    process_next_thread_buffer(raw2trace_thread_data_t *tdata, OUT bool *end_of_record);

    std::string
    write_footer(void *tls);

private:
    friend class trace_converter_t<raw2trace_t>;

    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    // Non-overridable parts of the interface expected by trace_converter_t.
    trace_entry_t *
    get_write_buffer(void *tls);
    bool
    write(void *tls, const trace_entry_t *start, const trace_entry_t *end);
    std::string
    write_delayed_branches(void *tls, const trace_entry_t *start,
                           const trace_entry_t *end);
    const instr_summary_t *
    get_instr_summary(void *tls, uint64 modidx, uint64 modoffs, INOUT app_pc *pc,
                      app_pc orig);
    void
    set_prev_instr_rep_string(void *tls, bool value);
    bool
    was_prev_instr_rep_string(void *tls);

    std::string
    append_delayed_branch(void *tls);

    bool
    thread_file_at_eof(void *tls);
    std::string
    process_header(raw2trace_thread_data_t *tdata);

    std::string
    process_thread_file(raw2trace_thread_data_t *tdata);

    void
    process_tasks(std::vector<raw2trace_thread_data_t *> *tasks);

    std::vector<raw2trace_thread_data_t> thread_data;

    int worker_count;
    std::vector<std::vector<raw2trace_thread_data_t *>> worker_tasks;

    // We use a hashtable to cache decodings.  We compared the performance of
    // hashtable_t to std::map.find, std::map.lower_bound, std::tr1::unordered_map,
    // and c++11 std::unordered_map (including tuning its load factor, initial size,
    // and hash function), and hashtable_t outperformed the others (i#2056).
    // We use a per-worker cache to avoid locks.
    std::vector<hashtable_t> decode_cache;

    // Store optional parameters for the module_mapper_t until we need to construct it.
    const char *(*user_parse)(const char *src, OUT void **data) = nullptr;
    void (*user_free)(void *data) = nullptr;
    std::string (*user_process)(drmodtrack_info_t *info, void *data,
                                void *user_data) = nullptr;
    void *user_process_data = nullptr;

    const char *modmap;
    std::unique_ptr<module_mapper_t> module_mapper;

    unsigned int verbosity = 0;

    // Our decode_cache duplication will not scale forever on very large code
    // footprint traces, so we set a cap for the default.
    static const int kDefaultJobMax = 16;
};

#endif /* _RAW2TRACE_H_ */
