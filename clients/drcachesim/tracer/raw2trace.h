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
 * module_mapper_t maps and unloads application modules.
 */
class module_mapper_t final {
public:
    /**
     * Parses and iterates over the list of modules.  This is provided to give the user a
     * method for iterating modules in the presence of the custom field used by drmemtrace
     * that prevents direct use of drmodtrack_offline_read().
     * On success, calls the \p process_cb function for every module in the list, and
     * returns an empty string at the end. Returns a non-empty error message on failure.
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
     * !get_last_error().empty()
     */
    std::string
    get_last_error(void) const
    {
        return last_error;
    }

    /**
     * module_t vector corresponding to the application modules. Lazily loads and caches
     * modules. If the object is invalid, returns an empty vector.
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
     * Returns a non-empty error message on failure, or if the object is invalid.
     */
    std::string
    find_mapped_trace_address(app_pc trace_address, OUT app_pc *mapped_address);

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
     * Sets up the parameters that will be used to construct a module_mapper_t for this
     * object.
     */
    std::string
    handle_custom_data(const char *(*parse_cb)(const char *src, OUT void **data),
                       std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                                 void *user_data),
                       void *process_cb_user_data, void (*free_cb)(void *data));

    /**
     * Constructs a module_mapper_t.
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
    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    std::string
    read_and_map_modules();
    std::string
    merge_and_process_thread_files();
    std::string
    append_bb_entries(uint tidx, offline_entry_t *in_entry, OUT bool *handled);
    std::string
    append_memref(INOUT trace_entry_t **buf_in, uint tidx, instr_t *instr, opnd_t ref,
                  bool write);
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
};

#endif /* _RAW2TRACE_H_ */
