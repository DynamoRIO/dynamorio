/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#define NOMINMAX // Avoid windows.h messing up std::min.
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bitset>
#include <deque>
#include <fstream>
#include <limits>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "archive_ostream.h"
#include "dr_api.h"
#include "drcovlib.h"
#include "drmemtrace.h"
#include "hashtable.h"
#include "instru.h"
#include "reader.h"
#include "trace_entry.h"
#include "utils.h"
#ifdef BUILD_PT_POST_PROCESSOR
#    include "../drpt2trace/pt2ir.h"
#endif

namespace dynamorio {
namespace drmemtrace {

#ifdef DEBUG
#    define DEBUG_ASSERT(x) DR_ASSERT(x)
#else
#    define DEBUG_ASSERT(x) /* nothing */
#endif

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
#define WINDOW_SUBDIR_FORMAT "window.%04zd" /* ptr_int_t is the window number type. */
#define WINDOW_SUBDIR_FIRST "window.0000"
#define TRACE_SUBDIR "trace"

#ifdef HAS_LZ4
#    define TRACE_SUFFIX_LZ4 "trace.lz4"
#endif

#ifdef HAS_ZIP
#    define TRACE_SUFFIX_ZIP "trace.zip"
#endif

#ifdef HAS_ZLIB
#    define TRACE_SUFFIX_GZ "trace.gz"
#endif

#define TRACE_SUFFIX "trace"

#define TRACE_CHUNK_PREFIX "chunk."

typedef enum {
    RAW2TRACE_STAT_COUNT_ELIDED,
    RAW2TRACE_STAT_DUPLICATE_SYSCALL,
    RAW2TRACE_STAT_RSEQ_ABORT,
    RAW2TRACE_STAT_RSEQ_SIDE_EXIT,
    RAW2TRACE_STAT_FALSE_SYSCALL,
    RAW2TRACE_STAT_EARLIEST_TRACE_TIMESTAMP,
    RAW2TRACE_STAT_LATEST_TRACE_TIMESTAMP
} raw2trace_statistic_t;

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
 * instr_summary_t is a compact encapsulation of the information needed by trace
 * conversion from decoded instructions.
 */
struct instr_summary_t final {
    /**
     * Caches information about a single memory reference.
     * Note that we reuse the same memref_summary_t object in raw2trace for all
     * memrefs of a scatter/gather instr. To avoid any issues due to mismatch
     * between instru_offline (which sees the expanded scatter/gather instr seq)
     * and raw2trace (which sees the original app scatter/gather instr only), we
     * disable address elision for scatter/gather bbs.
     */
    struct memref_summary_t {
        memref_summary_t(opnd_t opnd)
            : opnd(opnd)
            , remember_base(0)
            , use_remembered_base(0)
        {
        }
        /** The addressing mode of this reference. */
        opnd_t opnd;
        /**
         * A flag for reconstructing elided same-base addresses.  If set, this
         * address should be remembered for use on a later reference with the same
         * base and use_remembered_base set.
         */
        bool remember_base : 1;
        /**
         * A flag for reconstructing elided same-base addresses.  If set, this
         * address is not present in the trace and should be filled in from the prior
         * reference with the same base and remember_base set, or from the PC for
         * a rip-relative reference.
         */
        bool use_remembered_base : 1;
    };

    instr_summary_t()
    {
    }

    /**
     * Populates a pre-allocated instr_summary_t description, from the instruction found
     * at pc. Updates pc to the next instruction. Optionally logs translation details
     * (using orig_pc and verbosity).
     */
    static bool
    construct(void *dcontext, app_pc block_pc, INOUT app_pc *pc, app_pc orig_pc,
              OUT instr_summary_t *desc, uint verbosity = 0);

    /**
     * Get the pc after the instruction that was used to produce this instr_summary_t.
     */
    app_pc
    next_pc() const
    {
        return pc_ + length_;
    }
    /** Get the pc of the start of this instrucion. */
    app_pc
    pc() const
    {
        return pc_;
    }

    /**
     * Sets properties of the "pos"-th source memory operand by OR-ing in the
     * two boolean values.
     */
    void
    set_mem_src_flags(size_t pos, bool use_remembered_base, bool remember_base)
    {
        DEBUG_ASSERT(pos < mem_srcs_and_dests_.size());
        auto target = &mem_srcs_and_dests_[pos];
        target->use_remembered_base = target->use_remembered_base || use_remembered_base;
        target->remember_base = target->remember_base || remember_base;
    }

    /**
     * Sets properties of the "pos"-th destination memory operand by OR-ing in the
     * two boolean values.
     */
    void
    set_mem_dest_flags(size_t pos, bool use_remembered_base, bool remember_base)
    {
        DEBUG_ASSERT(num_mem_srcs_ + pos < mem_srcs_and_dests_.size());
        auto target = &mem_srcs_and_dests_[num_mem_srcs_ + pos];
        target->use_remembered_base = target->use_remembered_base || use_remembered_base;
        target->remember_base = target->remember_base || remember_base;
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
    uint16_t
    flush_type() const
    {
        return flush_type_;
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
#ifdef AARCH64
    bool
    is_aarch64_dc_zva() const
    {
        return TESTANY(kIsAarch64DcZvaMask, packed_);
    }
#endif
    bool
    is_cti() const
    {
        return TESTANY(kIsCtiMask, packed_);
    }
    bool
    is_scatter_or_gather() const
    {
        return TESTANY(kIsScatterOrGatherMask, packed_);
    }
    bool
    is_syscall() const
    {
        return TESTANY(kIsSyscallMask, packed_);
    }

    const memref_summary_t &
    mem_src_at(size_t pos) const
    {
        return mem_srcs_and_dests_[pos];
    }
    const memref_summary_t &
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
    // Returns 0 for indirect branches or non-branches.
    app_pc
    branch_target_pc() const
    {
        return branch_target_pc_;
    }

    static const int kReadsMemMask = 0x0001;
    static const int kWritesMemMask = 0x0002;
    static const int kIsPrefetchMask = 0x0004;
    static const int kIsFlushMask = 0x0008;
    static const int kIsCtiMask = 0x0010;

    // kIsAarch64DcZvaMask is available during processing of non-AArch64 traces too, but
    // it's intended for use only for AArch64 traces. This declaration reserves the
    // assigned mask and makes it unavailable for future masks.
    static const int kIsAarch64DcZvaMask = 0x0020;

    static const int kIsScatterOrGatherMask = 0x0040;

    static const int kIsSyscallMask = 0x0080;

    instr_summary_t(const instr_summary_t &other) = delete;
    instr_summary_t &
    operator=(const instr_summary_t &) = delete;
    instr_summary_t(instr_summary_t &&other) = delete;
    instr_summary_t &
    operator=(instr_summary_t &&) = delete;

    app_pc pc_ = 0;
    uint16_t type_ = 0;
    uint16_t prefetch_type_ = 0;
    uint16_t flush_type_ = 0;
    byte length_ = 0;
    app_pc branch_target_pc_ = 0;

    // Squash srcs and dests to save memory usage. We may want to
    // bulk-allocate pages of instr_summary_t objects, instead
    // of piece-meal allocating them on the heap one at a time.
    // One vector and a byte is smaller than 2 vectors.
    std::vector<memref_summary_t> mem_srcs_and_dests_;
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
    is_thread_start(const offline_entry_t *entry, OUT std::string *error,
                    OUT int *version, OUT offline_file_type_t *file_type);
    static std::string
    check_entry_thread_start(const offline_entry_t *entry);
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
           const char *(*parse_cb)(const char *src, OUT void **data) = nullptr,
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
            auto const it = encodings_.find(modoffs);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return (map_pc - entry->encodings) +
                reinterpret_cast<app_pc>(entry->start_pc);
        } else {
            size_t idx = static_cast<size_t>(modidx); // Avoid win32 warnings.
            app_pc res = map_pc - modvec_[idx].map_seg_base + modvec_[idx].orig_seg_base;
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
            auto const it = encodings_.find(modoffs);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return reinterpret_cast<app_pc>(entry->start_pc);
        } else {
            size_t idx = static_cast<size_t>(modidx); // Avoid win32 warnings.
            // Cast to unsigned pointer-sized int first to avoid sign-extending.
            return reinterpret_cast<app_pc>(
                       reinterpret_cast<ptr_uint_t>(modvec_[idx].orig_seg_base)) +
                (modoffs - modvec_[idx].seg_offs);
        }
    }

    app_pc
    get_map_pc(uint64 modidx, uint64 modoffs) const
    {
        if (modidx == PC_MODIDX_INVALID) {
            auto const it = encodings_.find(modoffs);
            if (it == encodings_.end())
                return nullptr;
            encoding_entry_t *entry = it->second;
            return entry->encodings;
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
    find_mapped_trace_bounds(app_pc trace_address, OUT app_pc *module_start,
                             OUT size_t *module_size);

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
                      OUT size_t *wrote);

protected:
    module_mapper_t(const char *module_map,
                    const char *(*parse_cb)(const char *src, OUT void **data) = nullptr,
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
    static const char *(*user_parse_)(const char *src, OUT void **data);
    static void (*user_free_)(void *data);
    static int (*user_print_)(void *data, char *dst, size_t max_len);
    static const char *
    parse_custom_module_data(const char *src, OUT void **data);
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

    uint verbosity_ = 0;
    std::string alt_module_dir_;
    std::string last_error_;

    file_t encoding_file_ = INVALID_FILE;
    std::unordered_map<uint64_t, encoding_entry_t *> encodings_;
};

/**
 * Subclasses module_mapper_t and replaces the module loading with a buffer
 * of encoded instr_t. Useful for tests where we want to mock the module
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

/**
 * Header of raw trace.
 */
struct trace_header_t {
    process_id_t pid;
    thread_id_t tid;
    uint64 timestamp;
    size_t cache_line_size;
};

#define DR_CHECK(val, msg) \
    do {                   \
        if (!(val))        \
            return msg;    \
    } while (0)

/**
 * Bitset hash table for balancing search time in case of enormous count of pc.
 * Each pc represented as pair of high 64-BLOCK_SIZE_BIT bits and
 * lower BLOCK_SIZE_BIT bits.
 * High bits is the key in hash table and give bitset table with BLOCK_SIZE size.
 * Lower bits set bit in bitset that means this pc was processed.
 * BLOCK_SIZE_BIT=13 was picked up to exclude hash collision and save speed up.
 */

template <typename T> class bitset_hash_table_t {
private:
    const static size_t BLOCK_SIZE_BIT = 13;
    const static size_t BLOCK_SIZE = (1 << BLOCK_SIZE_BIT);
    const size_t BASIC_BUCKET_COUNT = (1 << 15);
    std::unordered_map<T, std::bitset<BLOCK_SIZE>> page_table_;
    typename std::unordered_map<T, std::bitset<BLOCK_SIZE>>::iterator last_block_ =
        page_table_.end();

    inline std::pair<T, ushort>
    convert(T pc)
    {
        return std::pair<T, ushort>(
            reinterpret_cast<T>(reinterpret_cast<size_t>(pc) & (~(BLOCK_SIZE - 1))),
            static_cast<ushort>(reinterpret_cast<size_t>(pc) & (BLOCK_SIZE - 1)));
    }

public:
    bitset_hash_table_t()
    {
        static_assert(std::is_pointer<T>::value || std::is_integral<T>::value,
                      "Pointer or integral type required");
        page_table_.reserve(BASIC_BUCKET_COUNT);
        page_table_.emplace(T(0), std::move(std::bitset<BLOCK_SIZE>()));
        last_block_ = page_table_.begin();
    }

    bool
    find_and_insert(T pc)
    {
        auto block = convert(pc);
        if (block.first != last_block_->first) {
            last_block_ = page_table_.find(block.first);
            if (last_block_ == page_table_.end()) {
                last_block_ = (page_table_.emplace(std::make_pair(
                                   block.first, std::move(std::bitset<BLOCK_SIZE>()))))
                                  .first;
                last_block_->second[block.second] = 1;
                return true;
            }
        }
        if (last_block_->second[block.second] == 1)
            return false;
        last_block_->second[block.second] = 1;
        return true;
    }

    void
    erase(T pc)
    {
        auto block = convert(pc);
        if (last_block_->first != block.first)
            last_block_ = page_table_.find(block.first);
        if (last_block_ != page_table_.end())
            last_block_->second[block.second] = 0;
    }

    void
    clear()
    {
        page_table_.clear();
        page_table_.reserve(BASIC_BUCKET_COUNT);
        page_table_.emplace(T(0), std::move(std::bitset<BLOCK_SIZE>()));
        last_block_ = page_table_.begin();
    }

    ~bitset_hash_table_t()
    {
        clear();
    }
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

private:
    bool saw_pid_ = false;
    std::list<trace_entry_t> list_;
};

/**
 * The raw2trace class converts the raw offline trace format to the format
 * expected by analysis tools.  It requires access to the binary files for the
 * libraries and executable that were present during tracing.
 */
class raw2trace_t {
public:
    /* TODO i#6145: The argument list of raw2trace_t has become excessively long. It would
     * be more manageable to have an options struct instead.
     */
    // Only one of out_files and out_archives should be non-empty: archives support fast
    // seeking and are preferred but require zlib.
    // module_map, encoding_file, serial_schedule_file, cpu_schedule_file, thread_files,
    // and out_files are all owned and opened/closed by the caller.  module_map is not a
    // string and can contain binary data.
    // If a nullptr dcontext is passed, creates a new DR context va dr_standalone_init().
    raw2trace_t(
        const char *module_map, const std::vector<std::istream *> &thread_files,
        const std::vector<std::ostream *> &out_files,
        const std::vector<archive_ostream_t *> &out_archives,
        file_t encoding_file = INVALID_FILE, std::ostream *serial_schedule_file = nullptr,
        archive_ostream_t *cpu_schedule_file = nullptr, void *dcontext = nullptr,
        unsigned int verbosity = 0, int worker_count = -1,
        const std::string &alt_module_dir = "",
        uint64_t chunk_instr_count = 10 * 1000 * 1000,
        const std::unordered_map<thread_id_t, std::istream *> &kthread_files_map = {},
        const std::string &kcore_path = "", const std::string &kallsyms_path = "");
    // If a nullptr dcontext_in was passed to the constructor, calls dr_standalone_exit().
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
     * \deprecated #dynamorio::drmemtrace::module_mapper_t should be used instead.
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
     * \deprecated #dynamorio::drmemtrace::module_mapper_t::get_loaded_modules() should be
     * used instead.
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
     * \deprecated #dynamorio::drmemtrace::module_mapper_t::find_mapped_trace_address()
     * should be used instead.
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

#ifdef BUILD_PT_POST_PROCESSOR
    /**
     *  Checks whether the given file is a valid kernel PT file.
     */
    static std::string
    check_kthread_file(std::istream *f);

    /**
     *  Return the tid of the given kernel PT file.
     */
    static std::string
    get_kthread_file_tid(std::istream *f, OUT thread_id_t *tid);
#endif

    uint64
    get_statistic(raw2trace_statistic_t stat);

protected:
    /**
     * The trace_entry_t buffer returned by get_write_buffer() is assumed to be at least
     * #WRITE_BUFFER_SIZE large.
     */
    static const uint WRITE_BUFFER_SIZE = 64;

    struct block_summary_t {
        block_summary_t(app_pc start, int instr_count)
            : start_pc(start)
            , instrs(instr_count)
        {
        }
        app_pc start_pc;
        std::vector<instr_summary_t> instrs;
    };

    struct branch_info_t {
        branch_info_t(app_pc pc, app_pc target, int idx)
            : pc(pc)
            , target_pc(target)
            , buf_idx(idx)
        {
        }
        branch_info_t()
            : pc(0)
            , target_pc(0)
            , buf_idx(-1)
        {
        }
        app_pc pc;
        app_pc target_pc;
        int buf_idx; // Index into rseq_buffer_.
    };

    // Per-traced-thread data is stored here and accessed without locks by having each
    // traced thread processed by only one processing thread.
    struct raw2trace_thread_data_t {
        raw2trace_thread_data_t()
            : index(0)
            , tid(0)
            , worker(0)
            , thread_file(nullptr)
            , out_archive(nullptr)
            , out_file(nullptr)
            , version(0)
            , file_type(OFFLINE_FILE_TYPE_DEFAULT)
            , saw_header(false)
            , last_entry_is_split(false)
            , prev_instr_was_rep_string(false)
            , last_decode_block_start(nullptr)
            , last_decode_modidx(0)
            , last_decode_modoffs(0)
            , last_block_summary(nullptr)
#ifdef BUILD_PT_POST_PROCESSOR
            , kthread_file(nullptr)
#endif
        {
        }
        // Support subclasses extending this struct.
        virtual ~raw2trace_thread_data_t()
        {
        }

        int index;
        thread_id_t tid;
        int worker;
        std::istream *thread_file;
        archive_ostream_t *out_archive; // May be nullptr.
        std::ostream *out_file;         // Always set; for archive, == "out_archive".
        std::string error;
        int version;
        offline_file_type_t file_type;
        size_t cache_line_size = 0;
        std::deque<offline_entry_t> pre_read;

        // Used to delay a thread-buffer-final branch to keep it next to its target.
        std::vector<trace_entry_t> delayed_branch;
        // This is the first step of optimization of using delayed_branch_ vector.
        // Checking the bool value is cheaper than delayed_branch.empty().
        // In general it's better to avoid this vector at all.
        bool delayed_branch_empty_ = true;
        // Records the decode pcs for delayed_branch instructions for re-inserting
        // encodings across a chunk boundary.
        std::vector<app_pc> delayed_branch_decode_pcs;
        // Records the targets for delayed branches.  We don't merge this with
        // delayed_branch and delayed_branch_decode_pcs as the other vectors are
        // passed as raw arrays to write().
        std::vector<app_pc> delayed_branch_target_pcs;

        // Current trace conversion state.
        bool saw_header;
        offline_entry_t last_entry;
        // For 2-entry markers we need a 2nd current-entry struct we can unread.
        bool last_entry_is_split;
        offline_entry_t last_split_first_entry;
        std::array<trace_entry_t, WRITE_BUFFER_SIZE> out_buf;
        bool prev_instr_was_rep_string;
        // There is no sentinel available for modidx+modoffs so we use the pc for that.
        app_pc last_decode_block_start;
        uint64 last_decode_modidx;
        uint64 last_decode_modoffs;
        block_summary_t *last_block_summary;
        uint64 last_window = 0;

        // Statistics on the processing.
        uint64 count_elided = 0;
        uint64 count_duplicate_syscall = 0;
        uint64 count_false_syscall = 0;
        uint64 count_rseq_abort = 0;
        uint64 count_rseq_side_exit = 0;
        uint64 earliest_trace_timestamp = (std::numeric_limits<uint64>::max)();
        uint64 latest_trace_timestamp = 0;

        uint64 cur_chunk_instr_count = 0;
        uint64 cur_chunk_ref_count = 0;
        memref_counter_t memref_counter;
        uint64 chunk_count_ = 0;
        uint64 last_timestamp_ = 0;
        uint last_cpu_ = 0;
        app_pc last_pc_if_syscall_ = 0;

        bitset_hash_table_t<app_pc> encoding_emitted;
        app_pc last_encoding_emitted = nullptr;

        std::vector<schedule_entry_t> sched;
        std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched;

        // State for rolling back rseq aborts and side exits.
        bool rseq_want_rollback_ = false;
        bool rseq_ever_saw_entry_ = false;
        bool rseq_buffering_enabled_ = false;
        bool rseq_past_end_ = false;
        addr_t rseq_commit_pc_ = 0;
        addr_t rseq_start_pc_ = 0;
        addr_t rseq_end_pc_ = 0;
        std::vector<trace_entry_t> rseq_buffer_;
        int rseq_commit_idx_ = -1; // Index into rseq_buffer_.
        std::vector<branch_info_t> rseq_branch_targets_;
        std::vector<app_pc> rseq_decode_pcs_;

#ifdef BUILD_PT_POST_PROCESSOR
        std::istream *kthread_file;
        std::vector<syscall_pt_entry_t> pre_read_pt_entries;
        bool pt_metadata_processed = false;
        pt2ir_t pt2ir;
#endif
    };

    /**
     * Convert starting from in_entry, and reading more entries as required.
     * Sets end_of_record to true if processing hit the end of a record.
     * read_and_map_modules() must have been called by the implementation before
     * calling this API.
     */
    std::string
    process_offline_entry(raw2trace_thread_data_t *tdata, const offline_entry_t *in_entry,
                          thread_id_t tid, OUT bool *end_of_record,
                          OUT bool *last_bb_handled, OUT bool *flush_decode_cache);

    /**
     * Read the header of a thread, by calling get_next_entry() successively to
     * populate the header values. The timestamp field is populated only
     * for legacy traces.
     */
    std::string
    read_header(raw2trace_thread_data_t *tdata, OUT trace_header_t *header);

    /**
     * Point to the next offline entry_t.  Will not attempt to dereference past the
     * returned pointer.
     */
    virtual const offline_entry_t *
    get_next_entry(raw2trace_thread_data_t *tdata);

    /**
     * Records the currently stored last entry in order to remember two entries at once
     * (for handling split two-entry markers) and then reads and returns a pointer to the
     * next entry.  A subsequent call to unread_last_entry() will put back both entries.
     * Returns an emptry string on success or an error description on an error.
     */
    virtual const offline_entry_t *
    get_next_entry_keep_prior(raw2trace_thread_data_t *tdata);

    /* Adds the last read entry to the front of the read queue for get_next_entry(). */
    virtual void
    unread_last_entry(raw2trace_thread_data_t *tdata);

    /* Adds "entry" to the back of the read queue for get_next_entry(). */
    void
    queue_entry(raw2trace_thread_data_t *tdata, offline_entry_t &entry);

    /**
     * Callback notifying the currently-processed thread has exited. Subclasses are
     * expected to track record metadata themselves.  APIs for extracting that metadata
     * are exposed.
     */
    virtual std::string
    on_thread_end(raw2trace_thread_data_t *tdata);

    /**
     * The level parameter represents severity: the lower the level, the higher the
     * severity.
     */
    virtual void
    log(uint level, const char *fmt, ...);

    /**
     * Similar to log() but this disassembles the given PC.
     */
    virtual void
    log_instruction(uint level, app_pc decode_pc, app_pc orig_pc);

    virtual std::string
    read_and_map_modules();

#ifdef BUILD_PT_POST_PROCESSOR
    /**
     * Process the PT data associated with the provided syscall index.
     */
    std::string
    process_syscall_pt(raw2trace_thread_data_t *tdata, uint64_t syscall_idx);
#endif

    /**
     * Processes a raw buffer which must be the next buffer in the desired (typically
     * timestamp-sorted) order for its traced thread.  For concurrent buffer processing,
     * all buffers from any one traced thread must be processed by the same worker
     * thread, both for correct ordering and correct synchronization.
     */
    std::string
    process_next_thread_buffer(raw2trace_thread_data_t *tdata, OUT bool *end_of_record);

    std::string
    aggregate_and_write_schedule_files();

    std::string
    write_footer(raw2trace_thread_data_t *tdata);

    std::string
    open_new_chunk(raw2trace_thread_data_t *tdata);

    /**
     * The pointer to the DR context.
     */
    void *const dcontext_;

    /**
     * Whether a non-nullptr dcontext was passed to the constructor.
     */
    bool passed_dcontext_ = false;

    /* TODO i#2062: Remove this after replacing all uses in favor of queries to
     * module_mapper_t to handle both generated and module code.
     */
    /**
     * Get the module map.
     */
    const std::vector<module_t> &
    modvec_() const
    {
        return *modvec_ptr_;
    }

    /* TODO i#2062: Remove this after replacing all uses in favor of queries to
     * module_mapper_t to handle both generated and module code.
     */
    /**
     * Set the module map. Must be called before process_offline_entry() is called.
     */
    void
    set_modvec_(const std::vector<module_t> *modvec)
    {
        modvec_ptr_ = modvec;
    }

    /**
     * Get the module mapper.
     */
    const module_mapper_t &
    modmap_() const
    {
        return *modmap_ptr_;
    }

    /**
     * Set the module mapper. Must be called before process_offline_entry() is called.
     */
    void
    set_modmap_(const module_mapper_t *modmap)
    {
        modmap_ptr_ = modmap;
    }

    const module_mapper_t *modmap_ptr_ = nullptr;

    uint64 count_elided_ = 0;
    uint64 count_duplicate_syscall_ = 0;
    uint64 count_false_syscall_ = 0;
    uint64 count_rseq_abort_ = 0;
    uint64 count_rseq_side_exit_ = 0;
    uint64 earliest_trace_timestamp_ = (std::numeric_limits<uint64>::max)();
    uint64 latest_trace_timestamp_ = 0;

    std::unique_ptr<module_mapper_t> module_mapper_;

    std::vector<std::unique_ptr<raw2trace_thread_data_t>> thread_data_;

private:
    // We store this in drmodtrack_info_t.custom to combine our binary contents
    // data with any user-added module data from drmemtrace_custom_module_data.
    struct custom_module_data_t {
        size_t contents_size;
        const char *contents;
        void *user_data;
    };

    // Return a writable buffer guaranteed to be at least #WRITE_BUFFER_SIZE large.
    //  get_write_buffer() may reuse the same buffer after write() or
    //  write_delayed_branches()
    // is called.
    trace_entry_t *
    get_write_buffer(raw2trace_thread_data_t *tdata);

    // Writes the converted traces between start and end, where end is past the last
    // item to write. Both start and end are assumed to be pointers inside a buffer
    // returned by get_write_buffer().  decode_pcs is only needed if there is more
    // than one instruction in the buffer; in that case it must contain one entry per
    // instruction.
    std::string
    write(raw2trace_thread_data_t *tdata, const trace_entry_t *start,
          const trace_entry_t *end, app_pc *decode_pcs = nullptr,
          size_t decode_pcs_size = 0);

    // Similar to write(), but treat the provided traces as delayed branches: if they
    // are the last values in a record, they belong to the next record of the same
    // thread.  The start..end sequence must contain one instruction.
    std::string
    write_delayed_branches(raw2trace_thread_data_t *tdata, const trace_entry_t *start,
                           const trace_entry_t *end, app_pc decode_pc = nullptr,
                           app_pc target_pc = nullptr);

    // Writes encoding entries for pc..pc+instr_length to buf.
    std::string
    append_encoding(raw2trace_thread_data_t *tdata, app_pc pc, size_t instr_length,
                    trace_entry_t *&buf, trace_entry_t *buf_start);

    std::string
    insert_post_chunk_encodings(raw2trace_thread_data_t *tdata,
                                const trace_entry_t *instr, app_pc decode_pc);

    // Returns true if there are currently delayed branches that have not been emitted
    // yet.
    bool
    delayed_branches_exist(raw2trace_thread_data_t *tdata);

    // Returns false if an encoding was already emitted.
    // Otherwise, remembers that an encoding is being emitted now, and returns true.
    bool
    record_encoding_emitted(raw2trace_thread_data_t *tdata, app_pc pc);

    // Removes the record of the last encoding remembered in record_encoding_emitted().
    // This can only be called once between calls to record_encoding_emitted()
    // and only after record_encoding_emitted() returns true.
    void
    rollback_last_encoding(raw2trace_thread_data_t *tdata);

    // Writes out the buffered entries for an rseq region, after rolling back to
    // a side exit or abort if necessary.
    std::string
    adjust_and_emit_rseq_buffer(raw2trace_thread_data_t *tdata, addr_t next_pc,
                                addr_t abort_pc = 0);

    // Removes entries from tdata->rseq_buffer_ between and including the instructions
    // starting at or after remove_start_rough_idx and before or equal to
    // remove_end_rough_idx.  These "rough" indices can be on the encoding or instr
    // fetch to include that instruction.
    std::string
    rollback_rseq_buffer(raw2trace_thread_data_t *tdata, int remove_start_rough_idx,
                         // This is inclusive.
                         int remove_end_rough_idx);

    // Returns whether an #instr_summary_t representation of the instruction at pc inside
    // the block that begins at block_start_pc in the specified module exists.
    bool
    instr_summary_exists(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                         app_pc block_start, int index, app_pc pc);
    block_summary_t *
    lookup_block_summary(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                         app_pc block_start);
    instr_summary_t *
    lookup_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                         app_pc block_start, int index, app_pc pc,
                         OUT block_summary_t **block_summary);
    instr_summary_t *
    create_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                         block_summary_t *block, app_pc block_start, int instr_count,
                         int index, INOUT app_pc *pc, app_pc orig);

    // Return the #instr_summary_t representation of the index-th instruction (at *pc)
    // inside the block that begins at block_start_pc and contains instr_count
    // instructions in the specified module.  Updates the value at pc to the PC of the
    // next instruction. It is assumed the app binaries have already been loaded using
    // #module_mapper_t, and the values at *pc point within memory mapped by the module
    // mapper. This API provides an opportunity to cache decoded instructions.
    const instr_summary_t *
    get_instr_summary(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                      app_pc block_start, int instr_count, int index, INOUT app_pc *pc,
                      app_pc orig);

    // Sets two flags stored in the memref_summary_t inside the instr_summary_t for the
    // index-th instruction (at pc) inside the block that begins at block_start_pc and
    // contains instr_count instructions in the specified module.  The flags
    // use_remembered_base and remember_base are set for the source (write==false) or
    // destination (write==true) operand of index memop_index. The flags are OR-ed in:
    // i.e., they are never cleared.
    bool
    set_instr_summary_flags(raw2trace_thread_data_t *tdata, uint64 modidx, uint64 modoffs,
                            app_pc block_start, int instr_count, int index, app_pc pc,
                            app_pc orig, bool write, int memop_index,
                            bool use_remembered_base, bool remember_base);
    void
    set_last_pc_if_syscall(raw2trace_thread_data_t *tdata, app_pc value);
    app_pc
    get_last_pc_if_syscall(raw2trace_thread_data_t *tdata);

    // Sets a per-traced-thread cached flag that is read by was_prev_instr_rep_string().
    void
    set_prev_instr_rep_string(raw2trace_thread_data_t *tdata, bool value);

    // Queries a per-traced-thread cached flag that is set by set_prev_instr_rep_string().
    bool
    was_prev_instr_rep_string(raw2trace_thread_data_t *tdata);

    // Returns the trace file version (an OFFLINE_FILE_VERSION* constant).
    int
    get_version(raw2trace_thread_data_t *tdata);

    // Returns the trace file type (a combination of OFFLINE_FILE_TYPE* constants).
    offline_file_type_t

    get_file_type(raw2trace_thread_data_t *tdata);
    void
    set_file_type(raw2trace_thread_data_t *tdata, offline_file_type_t file_type);

    size_t
    get_cache_line_size(raw2trace_thread_data_t *tdata);

    // Accumulates the given value into the per-thread value for the statistic
    // identified by stat. This may involve a simple addition, or any other operation
    // like std::min or std::max, depending on the statistic.
    void
    accumulate_to_statistic(raw2trace_thread_data_t *tdata, raw2trace_statistic_t stat,
                            uint64 value);
    void
    log_instruction(app_pc decode_pc, app_pc orig_pc);

    // Flush the branches sent to write_delayed_branches().
    std::string
    append_delayed_branch(raw2trace_thread_data_t *tdata, app_pc next_pc);

    bool
    thread_file_at_eof(raw2trace_thread_data_t *tdata);
    std::string
    process_header(raw2trace_thread_data_t *tdata);

    std::string
    process_thread_file(raw2trace_thread_data_t *tdata);

    void
    process_tasks(std::vector<raw2trace_thread_data_t *> *tasks);

    std::string
    emit_new_chunk_header(raw2trace_thread_data_t *tdata);

    std::string
    analyze_elidable_addresses(raw2trace_thread_data_t *tdata, uint64 modidx,
                               uint64 modoffs, app_pc start_pc, uint instr_count);

    std::string
    process_memref(raw2trace_thread_data_t *tdata, trace_entry_t **buf_in,
                   const instr_summary_t *instr, instr_summary_t::memref_summary_t memref,
                   bool write, std::unordered_map<reg_id_t, addr_t> &reg_vals,
                   uint64_t cur_pc, uint64_t cur_offs, bool instrs_are_separate,
                   OUT bool *reached_end_of_memrefs, OUT bool *interrupted);

    std::string
    append_bb_entries(raw2trace_thread_data_t *tdata, const offline_entry_t *in_entry,
                      OUT bool *handled);

    // Returns true if a kernel interrupt happened at cur_pc.
    // Outputs a kernel interrupt if this is the right location.
    // Outputs any other markers observed if !instrs_are_separate, since they
    // are part of this block and need to be inserted now. Inserts all
    // intra-block markers (i.e., the higher level process_offline_entry() will
    // never insert a marker intra-block) and all inter-block markers are
    // handled at a higher level (process_offline_entry()) and are never
    // inserted here.
    std::string
    handle_kernel_interrupt_and_markers(raw2trace_thread_data_t *tdata,
                                        INOUT trace_entry_t **buf_in, uint64_t cur_pc,
                                        uint64_t cur_offs, int instr_length,
                                        bool instrs_are_separate, OUT bool *interrupted);

    std::string
    get_marker_value(raw2trace_thread_data_t *tdata, INOUT const offline_entry_t **entry,
                     OUT uintptr_t *value);

    std::string
    append_memref(raw2trace_thread_data_t *tdata, INOUT trace_entry_t **buf_in,
                  const instr_summary_t *instr, instr_summary_t::memref_summary_t memref,
                  bool write, std::unordered_map<reg_id_t, addr_t> &reg_vals,
                  OUT bool *reached_end_of_memrefs);

    bool
    should_omit_syscall(raw2trace_thread_data_t *tdata);

    bool
    is_maybe_blocking_syscall(uintptr_t number);

    int worker_count_;
    std::vector<std::vector<raw2trace_thread_data_t *>> worker_tasks_;

    class block_hashtable_t {
        // We use a hashtable to cache decodings.  We compared the performance of
        // hashtable_t to std::map.find, std::map.lower_bound, std::tr1::unordered_map,
        // and c++11 std::unordered_map (including tuning its load factor, initial size,
        // and hash function), and hashtable_t outperformed the others (i#2056).
        // Update: that measurement was when we did a hashtable lookup on every
        // instruction pc.  Now that we use block_summary_t and only look up each block,
        // the hashtable performance matters much less.
        // Plus, for 32-bit we cannot fit our modidx:modoffs key in the 32-bit-limited
        // hashtable_t key, so we now use std::unordered_map there.
    public:
        explicit block_hashtable_t(int worker_count)
        {
#ifdef X64
            // We go ahead and start with a reasonably large capacity.
            // We do not want the built-in mutex: this is per-worker so it can be
            // lockless.
            hashtable_init_ex(&table, 16, HASH_INTPTR, false, false, free_payload,
                              nullptr, nullptr);
            // We pay a little memory to get a lower load factor, unless we have
            // many duplicated tables.
            hashtable_config_t config = { sizeof(config), true,
                                          worker_count <= 8
                                              ? 40U
                                              : (worker_count <= 16 ? 50U : 60U) };
            hashtable_configure(&table, &config);
#endif
        }
#ifdef X64
        ~block_hashtable_t()
        {
            hashtable_delete(&table);
        }
#else
        // Work around a known Visual Studio issue where it complains about deleted copy
        // constructors for unique_ptr by deleting our copies and defaulting our moves.
        block_hashtable_t(const block_hashtable_t &) = delete;
        block_hashtable_t &
        operator=(const block_hashtable_t &) = delete;
        block_hashtable_t(block_hashtable_t &&) = default;
        block_hashtable_t &
        operator=(block_hashtable_t &&) = default;
#endif
        block_summary_t *
        lookup(uint64 modidx, uint64 modoffs)
        {
#ifdef X64
            return static_cast<block_summary_t *>(hashtable_lookup(
                &table, reinterpret_cast<void *>(hash_key(modidx, modoffs))));
#else
            return table[hash_key(modidx, modoffs)].get();
#endif
        }
        // Takes ownership of "block".
        void
        add(uint64 modidx, uint64 modoffs, block_summary_t *block)
        {
#ifdef X64
            hashtable_add(&table, reinterpret_cast<void *>(hash_key(modidx, modoffs)),
                          block);
#else
            table[hash_key(modidx, modoffs)].reset(block);
#endif
        }

        void
        clear()
        {
#ifdef X64
            hashtable_clear(&table);
#else
            table.clear();
#endif
        }

    private:
        static void
        free_payload(void *ptr)
        {
            delete (static_cast<block_summary_t *>(ptr));
        }
        static inline uint64
        hash_key(uint64 modidx, uint64 modoffs)
        {
            return (modidx << PC_MODOFFS_BITS) | modoffs;
        }

#ifdef X64
        hashtable_t table;
#else
        std::unordered_map<uint64, std::unique_ptr<block_summary_t>> table;
#endif
    };

    // We use a per-worker cache to avoid locks.
    std::vector<block_hashtable_t> decode_cache_;

    // Store optional parameters for the module_mapper_t until we need to construct it.
    const char *(*user_parse_)(const char *src, OUT void **data) = nullptr;
    void (*user_free_)(void *data) = nullptr;
    std::string (*user_process_)(drmodtrack_info_t *info, void *data,
                                 void *user_data) = nullptr;
    void *user_process_data_ = nullptr;

    const char *modmap_bytes_;
    file_t encoding_file_ = INVALID_FILE;
    std::ostream *serial_schedule_file_ = nullptr;
    archive_ostream_t *cpu_schedule_file_ = nullptr;

    unsigned int verbosity_ = 0;

    std::string alt_module_dir_;

    // Our decode_cache duplication will not scale forever on very large code
    // footprint traces, so we set a cap for the default.
    static const int kDefaultJobMax = 16;

    // Chunking for seeking support in compressed files.
    uint64_t chunk_instr_count_ = 0;

    offline_instru_t instru_offline_;
    const std::vector<module_t> *modvec_ptr_ = nullptr;

    /* The following member variables are utilized for decoding kernel PT traces. */
    const std::unordered_map<thread_id_t, std::istream *> kthread_files_map_;
    const std::string kcore_path_;
    const std::string kallsyms_path_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RAW2TRACE_H_ */
