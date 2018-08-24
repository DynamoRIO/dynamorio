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
#include <memory>
#include "trace_entry.h"
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
    friend class raw2trace_t;

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
    module_mapper_t(const char *module_map_in,
                    const char *(*parse_cb)(const char *src, OUT void **data) = nullptr,
                    std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                              void *user_data) = nullptr,
                    void *process_cb_user_data = nullptr,
                    void (*free_cb)(void *data) = nullptr, uint verbosity_in = 0);

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
     * Unload modules loaded with read_and_map_modules(), freeing associated resources.
     */
    ~module_mapper_t();

private:
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

    // since the dtor frees resources, disable copy constructor but allow
    // move semantics
    module_mapper_t(const module_mapper_t &) = delete;
    module_mapper_t &
    operator=(const module_mapper_t &) = delete;
    // VS2013 does not support defaulted move ctors/assign operators
#ifndef WINDOWS
    module_mapper_t(module_mapper_t &&) = default;
    module_mapper_t &
    operator=(module_mapper_t &&) = default;
#endif
};

/**
 * Header of raw trace.
 */
struct trace_header_t {
    process_id_t pid;
    thread_id_t tid;
    uint64 timestamp;
};

/**
 * The raw2trace class converts the raw offline trace format to the format
 * expected by analysis tools.  It requires access to the binary files for the
 * libraries and executable that were present during tracing.
 */
class raw2trace_t {
public:
    // module_map, thread_files and out_file are all owned and opened/closed by the
    // caller.  module_map is not a string and can contain binary data.
    raw2trace_t(const char *module_map, const std::vector<std::istream *> &thread_files,
                std::ostream *out_file, void *dcontext = NULL,
                unsigned int verbosity = 0);
    ~raw2trace_t();

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
    std::string
    do_conversion();

    static std::string
    check_thread_file(std::istream *f);

private:
    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    raw2trace_t *
    impl()
    {
        return this;
    }
    std::string
    process_offline_entry(const offline_entry_t *in_entry, thread_id_t tid,
                          OUT bool *end_of_record, OUT bool *last_bb_handled);
    std::string
    read_header(OUT trace_header_t *header);
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
    void
    log(uint level, const char *fmt, ...);

    const instr_summary_t *
    get_instr_summary(uint64 modx, uint64 modoffs, INOUT app_pc *pc, app_pc orig);
    std::string
    read_and_map_modules();
    std::string
    merge_and_process_thread_files();
    std::string
    append_bb_entries(const offline_entry_t *in_entry, OUT bool *handled);
    std::string
    append_memref(INOUT trace_entry_t **buf_in, uint tidx, const instr_summary_t *instr,
                  opnd_t ref, bool write);
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

    const std::vector<module_t> &
    modvec() const
    {
        return *modvec_ptr;
    }
    std::vector<std::vector<offline_entry_t>> pre_read;

    static const uint MAX_COMBINED_ENTRIES = 64;
    const char *modmap;
    const std::vector<module_t> *modvec_ptr;
    std::vector<std::istream *> thread_files;
    std::ostream *out_file;
    void *dcontext;
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

    // Store optional parameters for the module_mapper_t until we need to construct it.
    const char *(*user_parse)(const char *src, OUT void **data) = nullptr;
    void (*user_free)(void *data) = nullptr;
    std::string (*user_process)(drmodtrack_info_t *info, void *data,
                                void *user_data) = nullptr;
    void *user_process_data = nullptr;

    std::unique_ptr<module_mapper_t> module_mapper;

    // Current trace conversion state.
    offline_entry_t last_entry;
    uint tidx = 0;
    trace_entry_t out_buf[MAX_COMBINED_ENTRIES];
    uint thread_count = 0;
};

#endif /* _RAW2TRACE_H_ */
