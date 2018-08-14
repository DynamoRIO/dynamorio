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
#include "trace_entry.h"
#include <memory>
#include <fstream>
#include "hashtable.h"
#include <vector>

#define OUTFILE_PREFIX "drmemtrace"
#define OUTFILE_SUFFIX "raw"
#define OUTFILE_SUBDIR "raw"
#define TRACE_FILENAME "drmemtrace.trace"

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

const uint MAX_COMBINED_ENTRIES = 64;

/**
 * Header of raw trace.
 */
struct trace_header_t {
    pid_t pid;
    pid_t tid;
    uint64 timestamp;
};

/**
 * instr_summary is a compact encapsulation of the information needed by trace conversion
 * from decoded instructions.
 */
struct instr_summary_t final {
    instr_summary_t()
    {
    }

    /**
     * Populates a pre-allocated instr_summary_t desc, from the instruction found at pc.
     * Updates pc to the next instruction. Optionally logs translation details (using
     * orig_pc and verbosity)
     */
    static bool
    construct(void *dcontext, INOUT app_pc *pc, app_pc orig_pc,
              OUT instr_summary_t *desc, uint verbosity = 0);

    /**
     * Get the pc of the instruction after this one.
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
        return (1 << kReadsMemPos) & packed_;
    }
    bool
    writes_memory() const
    {
        return (1 << kWritesMemPos) & packed_;
    }
    bool
    is_prefetch() const
    {
        return (1 << kIsPrefetchPos) & packed_;
    }
    bool
    is_flush() const
    {
        return (1 << kIsFlushPos) & packed_;
    }
    bool
    is_cti() const
    {
        return (1 << kIsCtiPos) & packed_;
    }

    const opnd_t &
    src_at(size_t pos) const
    {
        return srcs_and_dests_[pos];
    }
    const opnd_t &
    dest_at(size_t pos) const
    {
        return srcs_and_dests_[srcs_ + pos];
    }
    size_t
    srcs() const
    {
        return srcs_;
    }
    size_t
    dests() const
    {
        return dests_;
    }

    static constexpr int kReadsMemPos = 0;
    static constexpr int kWritesMemPos = 1;
    static constexpr int kIsPrefetchPos = 2;
    static constexpr int kIsFlushPos = 3;
    static constexpr int kIsCtiPos = 4;

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

    std::vector<opnd_t> srcs_and_dests_;
    uint8_t srcs_ = 0;
    uint8_t dests_ = 0;
    byte packed_ = 0;
};

/**
 * Functions for encoding memtrace data headers. Each function returns the number of bytes
 * the write operation required: sizeof(trace_entry_t). The buffer is assumed to be
 * sufficiently large.
 */
struct instruction_converter_t {
    static int
    write_thread_exit(byte *buffer, pid_t tid);
    static int
    write_marker(byte *buffer, trace_marker_type_t type, uintptr_t val);
    static int
    write_iflush(byte *buffer, addr_t start, size_t size);
    static int
    write_pid(byte *buffer, pid_t pid);
    static int
    write_tid(byte *buffer, pid_t tid);
    static int
    write_timestamp(byte *buffer, uint64 timestamp);
};

/**
 * module_mapper_t is a delay-constructed singleton that maps and unloads application
 * modules. None of the members of this type are thread-safe.
 */
class module_mapper_t final {
public:
    /**
     * Returns a module_mapper_t instance, guaranteed to be unique. All subsequent calls
     * will return nullptr, until the instance is released. modmap_in's lifetime is
     * controlled by the caller.
     */
    static std::unique_ptr<module_mapper_t>
    get_or_fail(const char *modmap_in, uint verbosity = 0);

    /**
     * Maps application modules. After this call, get_loaded_modules() will contain valid
     * data.
     */
    std::string
    read_and_map_modules();

    /**
     * module_t vector corresponding to the application modules loaded with
     * read_and_map_modules.
     */
    const std::vector<module_t> &
    get_loaded_modules() const
    {
        return modvec;
    }

    /**
     * Performs the first step of do_conversion() without further action: parses and
     * iterates over the list of modules.  This is provided to give the user a method
     * for iterating modules in the presence of the custom field used by drmemtrace
     * that prevents direct use of drmodtrack_offline_read().
     * On success, calls the \p process_cb function passed to handle_custom_data()
     * for every module in the list, and returns an empty string at the end.
     * Returns a non-empty error message on failure.
     */
    std::string
    do_module_parsing();

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
     *
     * Only one value for each callback is supported, globally.  Calling this routine
     * again with a different value will replace the existing callbacks.
     */
    std::string
    handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                       std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                                 void *user_data),
                       void *process_cb_user_data, void (*free_cb)(void *data));

    /**
     * This interface is meant to be used with a final trace rather than a raw
     * trace, using the module log file saved from the raw2trace conversion.
     * When do_module_parsing_and_mapping() has been called, this routine can be used
     * to convert an instruction program counter in a trace into an address in the
     * current process where the instruction bytes for that instruction are mapped,
     * allowing decoding for obtaining further information than is stored in the trace.
     * Returns a non-empty error message on failure.
     */
    std::string
    find_mapped_trace_address(app_pc trace_address, OUT app_pc *mapped_address);

    /**
     * Unload modules loaded with read_and_map_modules(), freeing associated resources.
     * get_or_fail() may be called again afterwards.
     */
    ~module_mapper_t();

private:
    module_mapper_t(const char *module_map_in, uint verbosity_in = 0);

    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    const char *modmap = nullptr;
    void *modhandle = nullptr;
    std::vector<module_t> modvec;

    // Custom module fields that use drmodtrack are global.
    static const char *(*user_parse)(const char *src, OUT void **data);
    static void (*user_free)(void *data);
    static const char *
    parse_custom_module_data(const char *src, OUT void **data);
    static void
    free_custom_module_data(void *data);
    static bool has_custom_data;

    // We store module info for do_module_parsing.
    std::vector<drmodtrack_info_t> modlist;
    std::string (*user_process)(drmodtrack_info_t *info, void *data,
                                void *user_data) = nullptr;
    void *user_process_data = nullptr;
    app_pc last_orig_base = 0;
    size_t last_map_size = 0;
    byte *last_map_base = nullptr;

    uint verbosity;

    static bool constructed;
};

/**
 * The trace_converter template encapsulates reusable trace conversion logic.
 * Users may decide data IO details, how to handle logging, and how to handle
 * decoded instruction caching.
 */
template <typename T> class trace_converter_t {
#define RETURN_IF_FALSE(val, msg) \
    do {                          \
        if (!(val))               \
            return msg;           \
    } while (0)

#define INVALID_THREAD_ID 0

public:
    trace_converter_t(void *context)
        : dcontext(context)
    {
    }

    std::string
    process_offline_entry(const offline_entry_t *in_entry, pid_t tid,
                          OUT bool *end_of_record, OUT bool *last_bb_handled)
    {
        trace_entry_t *buf_base = impl()->get_write_buffer();
        byte *buf = reinterpret_cast<byte *>(buf_base);

        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED) {
            if (in_entry->extended.ext == OFFLINE_EXT_TYPE_FOOTER) {
                RETURN_IF_FALSE(tid != INVALID_THREAD_ID, "Missing thread id");
                impl()->log(2, "Thread %d exit\n", (uint)tid);
                buf += instruction_converter_t::write_thread_exit(buf, tid);
                *end_of_record = true;
                return impl()->on_thread_end();
            } else if (in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER) {
                buf += instruction_converter_t::write_marker(
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
            RETURN_IF_FALSE(reinterpret_cast<trace_entry_t *>(buf) == buf_base,
                            "We shouldn't have buffered anything before calling "
                            "append_bb_entries");
            auto result = append_bb_entries(in_entry, last_bb_handled);
            if (!result.empty())
                return result;
        } else if (in_entry->addr.type == OFFLINE_TYPE_IFLUSH) {
            const offline_entry_t *entry = nullptr;
            if (!(entry = impl()->get_next_entry()) ||
                entry->addr.type != OFFLINE_TYPE_IFLUSH)
                return "Flush missing 2nd entry";
            impl()->log(2, "Flush " PFX "-" PFX "\n", (ptr_uint_t)in_entry->addr.addr,
                        (ptr_uint_t)entry->addr.addr);
            buf += instruction_converter_t::write_iflush(
                buf, in_entry->addr.addr,
                (size_t)(entry->addr.addr - in_entry->addr.addr));
        } else {
            std::stringstream ss;
            ss << "Unknown trace type " << (int)in_entry->timestamp.type;
            return ss.str();
        }
        size_t size = reinterpret_cast<trace_entry_t *>(buf) - buf_base;
        RETURN_IF_FALSE((uint)size < MAX_COMBINED_ENTRIES, "Too many entries");
        if (size > 0) {
            if (!impl()->write(buf_base, reinterpret_cast<trace_entry_t *>(buf)))
                return "Failed to write to output file";
        }
        return "";
    }

    std::string
    read_header(OUT trace_header_t *header)
    {
        const offline_entry_t *in_entry;
        if (!(in_entry = impl()->get_next_entry()))
            return "Failed to read header from input file";
        // Handle legacy traces which have the timestamp first.
        if (in_entry->tid.type == OFFLINE_TYPE_TIMESTAMP) {
            header->timestamp = in_entry->timestamp.usec;
            if (!(in_entry = impl()->get_next_entry()))
                return "Failed to read header from input file";
        }
        DR_ASSERT(in_entry->tid.type == OFFLINE_TYPE_THREAD);
        header->timestamp = in_entry->tid.tid;
        if (!(in_entry = impl()->get_next_entry()))
            return "Failed to read header from input file";
        DR_ASSERT(in_entry->pid.type == OFFLINE_TYPE_PID);
        header->pid = in_entry->pid.pid;
        return "";
    }

    static std::string
    check_entry_thread_start(const offline_entry_t *entry)
    {
        std::string error;
        if (is_thread_start(entry, &error))
            return "";
        if (error.empty())
            return "Thread log file is corrupted: missing version entry";
        ;
        return error;
    }

    static bool
    is_thread_start(const offline_entry_t *entry, std::string *error)
    {
        *error = "";
        if (entry->extended.type != OFFLINE_TYPE_EXTENDED ||
            entry->extended.ext != OFFLINE_EXT_TYPE_HEADER) {
            return false;
        }
        if (entry->extended.valueA != OFFLINE_FILE_VERSION) {
            std::stringstream ss;
            ss << "Version mismatch: expect " << OFFLINE_FILE_VERSION << " vs "
               << (int)entry->extended.valueA;
            *error = ss.str();
            return false;
        }
        return true;
    }

protected:
    void *const dcontext;
    const std::vector<module_t> &
    get_modvec() const
    {
        return *modvec;
    }
    void
    set_modvec(const std::vector<module_t> *modvec_in)
    {
        modvec = modvec_in;
    }

private:
#define FAULT_INTERRUPTED_BB "INTERRUPTED"

    T *
    impl()
    {
        return static_cast<T *>(this);
    }

    // Returns FAULT_INTERRUPTED_BB if a fault occurred on this memref.
    // Any other non-empty string is a fatal error.
    std::string
    append_memref(INOUT trace_entry_t **buf_in, const instr_summary_t *instr,
                  const opnd_t &ref, bool write)
    {
        trace_entry_t *buf = *buf_in;
        const offline_entry_t *in_entry;
        bool have_type = false;
        if (!(in_entry = impl()->get_next_entry()))
            return "Trace ends mid-block";
        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
            in_entry->extended.ext == OFFLINE_EXT_TYPE_MEMINFO) {
            // For -L0_filter we have to store the type for multi-memref instrs where
            // we can't tell which memref it is (we'll still come here for the subsequent
            // memref operands but we'll exit early in the check below).
            have_type = true;
            buf->type = in_entry->extended.valueB;
            buf->size = in_entry->extended.valueA;
            impl()->log(4, "Found type entry type %d size %d\n", buf->type, buf->size);
            if (!(in_entry = impl()->get_next_entry()))
                return "Trace ends mid-block";
        }
        if (in_entry->addr.type != OFFLINE_TYPE_MEMREF &&
            in_entry->addr.type != OFFLINE_TYPE_MEMREF_HIGH) {
            // This happens when there are predicated memrefs in the bb, or for a
            // zero-iter rep string loop, or for a multi-memref instr with -L0_filter.
            // For predicated memrefs, they could be earlier, so "instr"
            // may not itself be predicated.
            // XXX i#2015: if there are multiple predicated memrefs, our instr vs
            // data stream may not be in the correct order here.
            impl()->log(4,
                        "Missing memref from predication, 0-iter repstr, or filter "
                        "(next type is 0x" ZHEX64_FORMAT_STRING ")\n",
                        in_entry->combined_value);
            impl()->unread_last_entry();
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
        // faulted.  There's a footer so this should always succeed.
        if (!(in_entry = impl()->get_next_entry()))
            return "Trace ends mid-block";
        // Put it back.
        impl()->unread_last_entry();
        if (in_entry->extended.type == OFFLINE_TYPE_EXTENDED &&
            in_entry->extended.ext == OFFLINE_EXT_TYPE_MARKER &&
            in_entry->extended.valueB == TRACE_MARKER_TYPE_KERNEL_EVENT) {
            // A signal/exception interrupted the bb after the memref.
            impl()->log(4, "Signal/exception interrupted the bb\n");
            return FAULT_INTERRUPTED_BB;
        }
        return "";
    }

    std::string
    append_bb_entries(const offline_entry_t *in_entry, OUT bool *handled)
    {
        uint instr_count = in_entry->pc.instr_count;
        const instr_summary_t *instr = nullptr;
        bool prev_instr_was_rep_string = false;

        app_pc start_pc =
            get_modvec()[in_entry->pc.modidx].map_base + in_entry->pc.modoffs;
        app_pc decode_pc = start_pc;
        if ((in_entry->pc.modidx == 0 && in_entry->pc.modoffs == 0) ||
            get_modvec()[in_entry->pc.modidx].map_base == NULL) {
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
                        get_modvec()[in_entry->pc.modidx].path);
        }
        bool skip_icache = false;
        bool truncated = false; // Whether a fault ended the bb early.
        if (instr_count == 0) {
            // L0 filtering adds a PC entry with a count of 0 prior to each memref.
            skip_icache = true;
            instr_count = 1;
            // We set a flag to avoid peeking forward on instr entries.
            instrs_are_separate = true;
        }
        RETURN_IF_FALSE(!instrs_are_separate || instr_count == 1,
                        "cannot mix 0-count and >1-count");
        for (uint i = 0; !truncated && i < instr_count; ++i) {
            trace_entry_t *buf_start = impl()->get_write_buffer();
            trace_entry_t *buf = buf_start;
            app_pc orig_pc = decode_pc - get_modvec()[in_entry->pc.modidx].map_base +
                get_modvec()[in_entry->pc.modidx].orig_base;
            // To avoid repeatedly decoding the same instruction on every one of its
            // dynamic executions, we cache the decoding in a hashtable.
            auto current_pc = decode_pc;
            instr = impl()->get_instr_summary(in_entry->pc.modidx, in_entry->pc.modoffs,
                                              &decode_pc, orig_pc);
            RETURN_IF_FALSE(!instr->is_cti() || i == instr_count - 1, "invalid cti");
            // FIXME i#1729: make bundles via lazy accum until hit memref/end.
            buf->type = instr->type();
            if (buf->type == TRACE_TYPE_INSTR_MAYBE_FETCH) {
                // We want it to look like the original rep string, with just one instr
                // fetch for the whole loop, instead of the drutil-expanded loop.
                // We fix up the maybe-fetch here so our offline file doesn't have to
                // rely on our own reader.
                if (!prev_instr_was_rep_string) {
                    prev_instr_was_rep_string = true;
                    buf->type = TRACE_TYPE_INSTR;
                } else {
                    impl()->log(3, "Skipping instr fetch for " PFX "\n",
                                (ptr_uint_t)current_pc);
                    // We still include the instr to make it easier for core simulators
                    // (i#2051).
                    buf->type = TRACE_TYPE_INSTR_NO_FETCH;
                }
            } else
                prev_instr_was_rep_string = false;
            buf->size = (ushort)(skip_icache ? 0 : instr->length());
            buf->addr = (addr_t)orig_pc;
            ++buf;

            // We need to interleave instrs with memrefs.
            // There is no following memref for (instrs_are_separate && !skip_icache).
            if ((!instrs_are_separate || skip_icache) &&
                // Rule out OP_lea.
                (instr->reads_memory() || instr->writes_memory())) {
                for (uint j = 0; j < instr->srcs(); j++) {
                    std::string error =
                        append_memref(&buf, instr, instr->src_at(j), false);
                    if (error == FAULT_INTERRUPTED_BB) {
                        truncated = true;
                        break;
                    } else if (!error.empty())
                        return error;
                }
                for (uint j = 0; !truncated && j < instr->dests(); j++) {
                    std::string error =
                        append_memref(&buf, instr, instr->dest_at(j), true);
                    if (error == FAULT_INTERRUPTED_BB) {
                        truncated = true;
                        break;
                    } else if (!error.empty())
                        return error;
                }
            }
            if (instr->is_cti()) {
                // In case this is the last branch prior to a thread switch, buffer it. We
                // avoid swapping threads immediately after a branch so that analyzers can
                // more easily find the branch target.  Doing this in the tracer would
                // incur extra overhead, and in the reader would be more complex and messy
                // than here (and we are ok bailing on doing this for online traces), so
                // we handle it in post-processing by delaying a thread-block-final branch
                // (and its memrefs) to that thread's next block.  This changes the
                // timestamp of the branch, which we live with.
                std::string error;
                if (!(error = impl()->write_delayed_branches(buf_start, buf)).empty())
                    return error;
            } else {
                if (!impl()->write(buf_start, buf))
                    return "Failed to write to output file";
            }
        }
        *handled = true;
        return "";
    }

    bool instrs_are_separate = false;
    const std::vector<module_t> *modvec = nullptr;

#undef RETURN_IF_FALSE
};

/**
 * The raw2trace class converts the raw offline trace format to the format
 * expected by analysis tools.  It requires access to the binary files for the
 * libraries and executable that were present during tracing.
 */
class raw2trace_t final : public trace_converter_t<raw2trace_t> {
public:
    // module_map, thread_files and out_file are all owned and opened/closed by the
    // caller.  module_map is not a string and can contain binary data.
    raw2trace_t(const char *module_map, const std::vector<std::istream *> &thread_files,
                std::ostream *out_file, void *dcontext = NULL,
                unsigned int verbosity = 0);
    ~raw2trace_t();

    /**
     * Delegates to module_mapper::handle_custom_data()
     */
    std::string
    handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                       std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                                 void *user_data),
                       void *process_cb_user_data, void (*free_cb)(void *data));

    /**
     * Delegates to module_mapper_t::do_module_parsing()
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
     */
    std::string
    do_module_parsing_and_mapping();

    /**
     * Delegates to module_mapper_t::find_mapped_trace_address()
     */
    std::string
    find_mapped_trace_address(app_pc trace_address, OUT app_pc *mapped_address);

    /**
     * Performs the conversion from raw data to finished trace files.
     * Returns a non-empty error message on failure.
     */
    std::string
    do_conversion();

    static std::string
    check_thread_file(std::istream *f);

private:
    // Interface with trace_converter_t:
    const offline_entry_t *
    get_next_entry();
    void
    unread_last_entry();
    trace_entry_t *
    get_write_buffer();
    bool
    write(const trace_entry_t *start, const trace_entry_t *end);
    std::string
    write_delayed_branches(const trace_entry_t *start, const trace_entry_t *end);
    std::string
    on_thread_end();
    const instr_summary_t *
    get_instr_summary(int modx, uint modoffs, app_pc *pc, app_pc orig);
    void
    log(uint level, const char *fmt, ...);

    std::string
    merge_and_process_thread_files();
    std::string
    append_delayed_branch(uint tidx);

    // We do some internal buffering to avoid istream::seekg whose performance is
    // detrimental for some filesystem types.
    bool
    read_from_thread_file(uint tidx, offline_entry_t *dest, size_t count,
                          OUT size_t *num_read = nullptr);
    void
    unread_from_thread_file(uint tidx, offline_entry_t *dest, size_t count);
    bool
    thread_file_at_eof(uint tidx);
    std::vector<std::vector<offline_entry_t>> pre_read;

    std::vector<std::istream *> thread_files;
    std::ostream *out_file;
    bool prev_instr_was_rep_string;
    // This indicates that each memref has its own PC entry and that each
    // icache entry does not need to be considered a memref PC entry as well.
    bool instrs_are_separate;
    unsigned int verbosity;
    // We use a hashtable to cache decodings.  We compared the performance of
    // hashtable_t to std::map.find, std::map.lower_bound, std::tr1::unordered_map,
    // and c++11 std::unordered_map (including tuning its load factor, initial size,
    // and hash function), and hashtable_t outperformed the others (i#2056).
    hashtable_t decode_cache;

    // Used to delay thread-buffer-final branch to keep it next to its target.
    std::vector<std::vector<char>> delayed_branch;

    offline_entry_t last_entry;
    uint tidx;
    trace_entry_t out_buf[MAX_COMBINED_ENTRIES];
    uint thread_count;
    std::unique_ptr<module_mapper_t> module_mapper;

    friend class trace_converter_t<raw2trace_t>;
};

#endif /* _RAW2TRACE_H_ */
