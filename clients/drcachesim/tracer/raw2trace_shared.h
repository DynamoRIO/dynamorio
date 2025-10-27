/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

/* Functions and structs extracted from raw2trace.h for sharing with the tracer.
 */

#ifndef _RAW2TRACE_SHARED_H_
#define _RAW2TRACE_SHARED_H_ 1

/**
 * @file drmemtrace/raw2trace_shared.h
 * @brief DrMemtrace routines and structs shared between raw2trace and tracer.
 */

#include <list>
#include <map>

#include "dr_api.h"
#include "drcovlib.h"
#include "drmemtrace.h"
#include "reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#define OUTFILE_SUFFIX "raw"
#ifdef BUILD_PT_POST_PROCESSOR
#    define OUTFILE_SUFFIX_PT "raw.pt"
#endif
#ifdef HAS_ZLIB
#    define OUTFILE_SUFFIX_GZ "raw.gz"
#    define OUTFILE_SUFFIX_ZLIB "raw.zlib"
#endif
#ifdef HAS_SNAPPY
#    define OUTFILE_SUFFIX_SZ "raw.sz"
#endif
#ifdef HAS_LZ4
#    define OUTFILE_SUFFIX_LZ4 "raw.lz4"
#endif
#define OUTFILE_SUBDIR "raw"
#define WINDOW_SUBDIR_PREFIX "window"
#define WINDOW_SUBDIR_FORMAT                               \
    "window.%04zd" /* ptr_int_t is the window number type. \
                    */
#define WINDOW_SUBDIR_FIRST "window.0000"
#define TRACE_SUBDIR "trace"
#define TRACE_CHUNK_PREFIX "chunk."
// The chunk name is "chunk.nnnnnnnn" with leading 0's for the number suffix.
// Some zip readers end up alphabetizing the component files, so be sure to have enough
// leading zeroes to keep them in numeric order even with large component counts.
#define TRACE_CHUNK_SUFFIX_WIDTH 8
// Dir for auxiliary files. Same level as TRACE_SUBDIR and OUTFILE_SUBDIR.
#define AUX_SUBDIR "aux"

// Versioning for our drmodtrack custom module fields.
#define CUSTOM_MODULE_VERSION 1

/**
 * Functions for decoding and verifying raw memtrace data headers.
 */
struct trace_metadata_reader_t {
    static bool
    is_thread_start(const offline_entry_t *entry, DR_PARAM_OUT std::string *error,
                    DR_PARAM_OUT int *version,
                    DR_PARAM_OUT offline_file_type_t *file_type);
    static std::string
    check_entry_thread_start(const offline_entry_t *entry);
};

// We need to determine the memref_t record count for inserting a marker with
// that count at the start of each chunk.
class memref_counter_t : public reader_t {
public:
    bool
    init() override
    {
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
        return nullptr;
    };
    std::string
    get_stream_name() const override
    {
        return "";
    }
    int
    entry_memref_count(const trace_entry_t *entry)
    {
        // Mirror file_reader_t::open_input_file().
        // In particular, we need to skip TRACE_TYPE_HEADER and to pass the
        // tid and pid to the reader before the 2 markers in front of them.
        if (!saw_pid_) {
            if (entry->type == TRACE_TYPE_HEADER)
                return 0;
            else if (entry->type == TRACE_TYPE_THREAD) {
                list_.push_front(*entry);
                return 0;
            } else if (entry->type != TRACE_TYPE_PID) {
                list_.push_back(*entry);
                return 0;
            }
            saw_pid_ = true;
            auto it = list_.begin();
            ++it;
            list_.insert(it, *entry);
            int count = 0;
            for (auto &next : list_) {
                input_entry_ = &next;
                if (process_input_entry())
                    ++count;
            }
            return count;
        }
        if (entry->type == TRACE_TYPE_FOOTER)
            return 0;
        input_entry_ = const_cast<trace_entry_t *>(entry);
        return process_input_entry() ? 1 : 0;
    }
    unsigned char *
    get_decode_pc(addr_t orig_pc)
    {
        if (encodings_.find(orig_pc) == encodings_.end())
            return nullptr;
        return encodings_[orig_pc].bits;
    }
    void
    set_core_sharded(bool core_sharded)
    {
        core_sharded_ = core_sharded;
    }

private:
    bool saw_pid_ = false;
    std::list<trace_entry_t> list_;
};

/**
 * Reads the module file at the given \p modfilename path. Returns an
 * empty string if successful, or the error string if not.
 *
 * This uses the DR file APIs to read the module file. Generally we
 * attempt to isolate implementation that uses DR file APIs to
 * raw2trace_directory_t, and have raw2trace and raw2trace_shared
 * deal only with file streams, but we make an exception here. This
 * is acceptable because no current users of read_module_file
 * need to read it from a stream. Also, this is a simple convenience
 * routine that only reads the file without any module file specific
 * logic.
 *
 * If successful, the returned \p modfile must be closed using
 * \p dr_close_file, and the returned \p modefilebytes must be freed
 * using a delete[].
 */
std::string
read_module_file(const std::string &modfilename, file_t &modfile, char *&modfile_bytes);

struct module_t {
    module_t(const char *path, app_pc orig, byte *map, size_t offs, size_t size,
             size_t total_size, bool external = false)
        : path(path)
        , orig_seg_base(orig)
        , map_seg_base(map)
        , seg_offs(offs)
        , seg_size(size)
        , total_map_size(total_size)
        , is_external(external)
    {
    }
    const char *path;
    // We have to handle segments within a module separately, as there can be
    // gaps between them that contain other objects (xref i#4731).
    app_pc orig_seg_base;
    byte *map_seg_base;
    size_t seg_offs;
    size_t seg_size;
    // Despite tracking segments separately, we have a single mapping.
    // The first segment stores that mapping size here; subsequent segments
    // have 0 for this field.
    size_t total_map_size;
    bool is_external; // If true, the data is embedded in drmodtrack custom fields.
};

/**
 * module_mapper_t maps and unloads application modules, as well as non-module
 * instruction encodings (for raw traces, or if not present in the final trace).
 * Using it assumes a dr_context has already been setup.
 * This class is not thread-safe.
 */
class module_mapper_t {
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
     * Additionally parses the non-module instruction encodings file if 'encoding_file'
     * is not nullptr.
     *
     * On success, calls the \p process_cb function for every module in the list.
     * On failure, get_last_error() is non-empty, and indicates the cause.
     */
    static std::unique_ptr<module_mapper_t>
    create(const char *module_map,
           const char *(*parse_cb)(const char *src, DR_PARAM_OUT void **data) = nullptr,
           std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                     void *user_data) = nullptr,
           void *process_cb_user_data = nullptr, void (*free_cb)(void *data) = nullptr,
           uint verbosity = 0, const std::string &alt_module_dir = "",
           file_t encoding_file = INVALID_FILE)
    {
        return std::unique_ptr<module_mapper_t>(
            new module_mapper_t(module_map, parse_cb, process_cb, process_cb_user_data,
                                free_cb, verbosity, alt_module_dir, encoding_file));
    }

    /**
     * All APIs on this type, including constructor, may fail. get_last_error() returns
     * the last error message. The object should be considered unusable if
     * !get_last_error().empty().
     */
    std::string
    get_last_error(void) const
    {
        return last_error_;
    }

    /**
     * module_t vector corresponding to the application modules. Lazily loads and caches
     * modules. If the object is invalid, returns an empty vector. The user may check
     * get_last_error() to ensure no error has occurred, or get the applicable error
     * message.
     */
    virtual const std::vector<module_t> &
    get_loaded_modules()
    {
        if (last_error_.empty() && modvec_.empty())
            read_and_map_modules();
        return modvec_;
    }

    app_pc
    get_orig_pc_from_map_pc(app_pc map_pc, uint64 modidx, uint64 modoffs) const
    {
        if (modidx == PC_MODIDX_INVALID) {
            uint64 blockidx = 0;
            uint64 blockoffs = 0;
            convert_modoffs_to_non_mod_block(modoffs, blockidx, blockoffs);
            auto const it = encodings_.find(blockidx);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return (map_pc - entry->encodings) +
                reinterpret_cast<app_pc>(entry->start_pc);
        } else {
            size_t idx = static_cast<size_t>(modidx); // Avoid win32 warnings.
            app_pc res = modvec_[idx].orig_seg_base - modvec_[idx].map_seg_base + map_pc;
#ifdef ARM
            // Match Thumb vs Arm mode by setting LSB.
            if (TESTANY(1, modoffs))
                res = reinterpret_cast<app_pc>(reinterpret_cast<uint64>(res) | 1);
#endif
            return res;
        }
    }

    app_pc
    get_orig_pc(uint64 modidx, uint64 modoffs) const
    {
        if (modidx == PC_MODIDX_INVALID) {
            uint64 blockidx = 0;
            uint64 blockoffs = 0;
            convert_modoffs_to_non_mod_block(modoffs, blockidx, blockoffs);
            auto const it = encodings_.find(blockidx);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return reinterpret_cast<app_pc>(entry->start_pc + blockoffs);
        } else {
            size_t idx = static_cast<size_t>(modidx); // Avoid win32 warnings.
            // Cast to unsigned pointer-sized int first to avoid sign-extending.
            return reinterpret_cast<app_pc>(
                reinterpret_cast<ptr_uint_t>(modvec_[idx].orig_seg_base) +
                (modoffs - modvec_[idx].seg_offs));
        }
    }

    app_pc
    get_map_pc(uint64 modidx, uint64 modoffs) const
    {
        if (modidx == PC_MODIDX_INVALID) {
            uint64 blockidx = 0;
            uint64 blockoffs = 0;
            convert_modoffs_to_non_mod_block(modoffs, blockidx, blockoffs);
            auto const it = encodings_.find(blockidx);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return &entry->encodings[blockoffs];
        } else {
            size_t idx = static_cast<size_t>(modidx); // Avoid win32 warnings.
            return modvec_[idx].map_seg_base + (modoffs - modvec_[idx].seg_offs);
        }
    }

    /**
     * This interface is meant to be used with a final trace rather than a raw
     * trace, using the module log file saved from the raw2trace conversion.
     * After a call to get_loaded_modules(), this routine may be used
     * to convert an instruction program counter in a trace into an address in the
     * current process where the instruction bytes for that instruction are mapped,
     * allowing decoding for obtaining further information than is stored in the trace.
     * This interface only supports code inside modules; generated code is
     * expected to have instruction encodings in the trace itself.
     * Returns the mapped address. Check get_last_error_() if an error occurred.
     */
    app_pc
    find_mapped_trace_address(app_pc trace_address);

    /**
     * This is identical to find_mapped_trace_address() but it also returns the
     * bounds of the containing region, allowing the caller to perform its own
     * mapping for any address that is also within those bounds.
     */
    app_pc
    find_mapped_trace_bounds(app_pc trace_address, DR_PARAM_OUT app_pc *module_start,
                             DR_PARAM_OUT size_t *module_size);

    /**
     * Unload modules loaded with read_and_map_modules(), freeing associated resources.
     */
    virtual ~module_mapper_t();

    /**
     * Writes out the module list to \p buf, whose capacity is \p buf_size.
     * The written data includes any modifications made by the \p process_cb
     * passed to create().  Any custom data returned by the \p parse_cb passed to
     * create() is passed to \p print_cb here for serialization.  The \p print_cb
     * must return the number of characters printed or -1 on error.
     */
    drcovlib_status_t
    write_module_data(char *buf, size_t buf_size,
                      int (*print_cb)(void *data, char *dst, size_t max_len),
                      DR_PARAM_OUT size_t *wrote);

protected:
    module_mapper_t(const char *module_map,
                    const char *(*parse_cb)(const char *src,
                                            DR_PARAM_OUT void **data) = nullptr,
                    std::string (*process_cb)(drmodtrack_info_t *info, void *data,
                                              void *user_data) = nullptr,
                    void *process_cb_user_data = nullptr,
                    void (*free_cb)(void *data) = nullptr, uint verbosity = 0,
                    const std::string &alt_module_dir = "",
                    file_t encoding_file = INVALID_FILE);

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
    convert_modoffs_to_non_mod_block(uint64 modoffs, uint64 &blockidx,
                                     uint64 &blockoffs) const
    {
        if (!separate_non_mod_instrs_) {
            blockidx = modoffs;
            blockoffs = 0;
            return;
        }
        auto it = cum_block_enc_len_to_encoding_id_.upper_bound(modoffs);
        // Since modoffs >= 0 and the smallest key in cum_block_enc_len_to_encoding_id_ is
        // always zero, `it` should never be the first element of the map.
        DR_ASSERT(it != cum_block_enc_len_to_encoding_id_.begin());
        auto it_prev = it;
        it_prev--;
        DR_ASSERT(it_prev->first <= modoffs &&
                  (it == cum_block_enc_len_to_encoding_id_.end() || it->first > modoffs));
        blockidx = it_prev->second;
        blockoffs = modoffs - it_prev->first;
    }

    virtual void
    read_and_map_modules(void);

    std::string
    do_module_parsing();

    std::string
    do_encoding_parsing();

    const char *modmap_ = nullptr;
    void *modhandle_ = nullptr;
    std::vector<module_t> modvec_;
    void (*const cached_user_free_)(void *data) = nullptr;

    // Custom module fields that use drmodtrack are global.
    static const char *(*user_parse_)(const char *src, DR_PARAM_OUT void **data);
    static void (*user_free_)(void *data);
    static int (*user_print_)(void *data, char *dst, size_t max_len);
    static const char *
    parse_custom_module_data(const char *src, DR_PARAM_OUT void **data);
    static int
    print_custom_module_data(void *data, char *dst, size_t max_len);
    static void
    free_custom_module_data(void *data);
    static bool has_custom_data_global_;

    bool has_custom_data_ = false;

    // We store module info for do_module_parsing.
    std::vector<drmodtrack_info_t> modlist_;
    std::string (*user_process_)(drmodtrack_info_t *info, void *data,
                                 void *user_data) = nullptr;
    void *user_process_data_ = nullptr;
    app_pc last_orig_base_ = 0;
    size_t last_map_size_ = 0;
    byte *last_map_base_ = nullptr;
    bool separate_non_mod_instrs_ = false;
    std::map<uint64_t, uint64_t> cum_block_enc_len_to_encoding_id_;

    uint verbosity_ = 0;
    std::string alt_module_dir_;
    std::string last_error_;

    file_t encoding_file_ = INVALID_FILE;
    std::unordered_map<uint64_t, encoding_entry_t *> encodings_;
};

int
print_module_data_fields(char *dst, size_t max_len, const void *custom_data,
                         size_t custom_size,
                         int (*user_print_cb)(void *data, char *dst, size_t max_len),
                         void *user_cb_data);

/**
 * Subclasses module_mapper_t and replaces the module loading with a buffer
 * of encoded #instr_t. Useful for tests where we want to mock the module
 * files with an in-memory buffer of instrs.
 */
class test_module_mapper_t : public module_mapper_t {
public:
    test_module_mapper_t(instrlist_t *instrs, void *drcontext)
        : module_mapper_t(nullptr)
    {
        // We encode for 1-based addresses for simpler tests with low values while
        // avoiding null pointer manipulation complaints (xref i#6196).
        byte *pc = instrlist_encode_to_copy(
            drcontext, instrs, decode_buf_,
            reinterpret_cast<byte *>(static_cast<ptr_uint_t>(4)), nullptr, true);
        DR_ASSERT(pc != nullptr);
        DR_ASSERT(pc - decode_buf_ < MAX_DECODE_SIZE);
        // Clear do_module_parsing error; we can't cleanly make virtual b/c it's
        // called from the constructor.
        last_error_ = "";
    }

protected:
    void
    read_and_map_modules() override
    {
        modvec_.push_back(module_t("fake_exe", 0, decode_buf_, 0, MAX_DECODE_SIZE,
                                   MAX_DECODE_SIZE, true));
    }

private:
    static const int MAX_DECODE_SIZE = 1024;
    byte decode_buf_[MAX_DECODE_SIZE];
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RAW2TRACE_SHARED_H_ */
