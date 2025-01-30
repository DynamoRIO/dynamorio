/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

/**
 * decode_cache.h: Library that supports operations related to decoding trace
 * instructions that are common to various trace analysis tools, including:
 * - providing the address where the instr encodings are present, which can
 *   either be from the instr memref_t for traces with embedded encodings, or
 *   from the mapped app binaries otherwise;
 * - decoding the instr raw bytes to create the #instr_t;
 * - caching of data derived from the decoded #instr_t, and updating the cache
 *   appropriately based on the encoding_is_new field for embedded encodings.
 */

#ifndef _DECODE_CACHE_H_
#define _DECODE_CACHE_H_ 1

#include "memref.h"
#include "raw2trace_shared.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace dynamorio {
namespace drmemtrace {

/**
 * Base class for storing instruction decode info. Users should sub-class this
 * base class and implement set_decode_info_derived() to derive and store the decode
 * info they need.
 */
class decode_info_base_t {
    // We need all specializations of decode_cache_t to be able to set the
    // error_string_ with details of the instruction decoding error.
    template <typename T> friend class decode_cache_t;

public:
    virtual ~decode_info_base_t() = default;

    /**
     * Sets the decode info for the provided \p instr which was allocated using the
     * provided \p dcontext for the provided \p memref_instr, decoded from raw bytes
     * at the provided address in \p decode_pc (raw bytes address is valid only for
     * this call).
     *
     * This invokes the set_decode_info_derived() provided by the derived class, and
     * additionally does other required bookkeeping.
     *
     * set_decode_info_derived() is expected to take ownership of the provided \p instr
     * if used with a #dynamorio::drmemtrace::decode_cache_t object constructed with
     * \p persist_decoded_instr_ set to true.
     *
     * The provided \p instr will be nullptr if the
     * #dynamorio::drmemtrace::decode_cache_t was constructed with
     * \p include_decoded_instr_ set to false, which is useful when the user needs only
     * the \p decode_pc to perform the actual decoding themselves.
     */
    void
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr, app_pc decode_pc);

    /**
     * Indicates whether set_decode_info() was successfully invoked on the object by
     * a #dynamorio::drmemtrace::decode_cache_t using a successfully decoded
     * instruction. is_valid() will be false if the object is default-constructed.
     */
    bool
    is_valid() const;

    /**
     * Returns the details of the error encountered when decoding the instruction or
     * during the custom logic in set_decode_info_derived().
     */
    std::string
    get_error_string() const;

private:
    /**
     * Sets the decoding info fields as required by the derived class, based on the
     * provided #instr_t which was allocated using the provided opaque \p dcontext
     * for the provided \p memref_instr, decoded from raw bytes at the provided
     * address in \p decode_pc (raw bytes address is valid only for this call).
     * Derived classes must implement this virtual function. Note that this cannot be
     * invoked directly as it is private, but only through set_decode_info() which
     * does other required bookkeeping.
     *
     * This is meant for use with #dynamorio::drmemtrace::decode_cache_t, which will
     * invoke set_decode_info() for each new decoded instruction.
     *
     * The responsibility for invoking instr_destroy() on the provided \p instr
     * lies with this #decode_info_base_t object, unless the
     * #dynamorio::drmemtrace::decode_cache_t was constructed with
     * \p persist_decoded_instr_ set to false, in which case no heap allocation
     * takes place.
     *
     * The provided \p instr will be nullptr if the
     * #dynamorio::drmemtrace::decode_cache_t was constructed with
     * \p include_decoded_instr_ set to false.
     *
     * Returns an empty string if successful, or the error description.
     */
    virtual std::string
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr, app_pc decode_pc) = 0;

    bool is_valid_ = false;
    std::string error_string_;
};

/**
 * Decode info including the full decoded #instr_t. This should be used with a
 * #dynamorio::drmemtrace::decode_cache_t constructed with
 * \p include_decoded_instr_ and \p persist_decoded_instr_ set to true.
 */
class instr_decode_info_t : public decode_info_base_t {
public:
    virtual ~instr_decode_info_t() override;
    instr_t *
    get_decoded_instr();

private:
    std::string
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr, app_pc decode_pc) override;

    // This is owned by the enclosing instr_decode_info_t object and will be
    // instr_destroy()-ed in the destructor.
    instr_t *instr_ = nullptr;
    void *dcontext_ = nullptr;
};

/**
 * Base class for #dynamorio::drmemtrace::decode_cache_t.
 *
 * This is used to allow sharing the static data members among all template instances
 * of #dynamorio::drmemtrace::decode_cache_t.
 */
class decode_cache_base_t {
protected:
    /**
     * Constructor for the base class, intentionally declared as protected so
     * #decode_cache_base_t cannot be instantiated directly but only via
     * a derived class.
     */
    decode_cache_base_t(unsigned int verbosity);
    virtual ~decode_cache_base_t();

    static std::mutex module_mapper_mutex_;
    static std::unique_ptr<module_mapper_t> module_mapper_;
    static std::string module_file_path_used_for_init_;
    // XXX: Maybe the ownership and destruction responsibility for the modfile bytes
    // should be given to module_mapper_t instead.
    static char *modfile_bytes_;
    static int module_mapper_use_count_;

    /**
     * use_module_mapper_ describes whether we lookup the instr encodings
     * from the module map, or alternatively from embedded-encodings in the
     * trace.
     *
     * Note that we store our instr encoding lookup strategy as a non-static
     * data member, unlike module_mapper_t which is static and shared between
     * all #dynamorio::drmemtrace::decode_cache_t instances (even of different
     * template types). Some analysis tools may deliberately want to look at instr
     * encodings from the module mappings, but that strategy does not provide JIT
     * encodings which are present only as embedded-encodings in the trace.
     * In such a case, other concurrently running analysis tools should still
     * be able to see encodings for JIT code.
     */
    bool use_module_mapper_ = false;
    /**
     * Verbosity level for logs.
     */
    unsigned int verbosity_ = 0;

    /**
     * Initializes the module_mapper_ object using make_module_mapper() and performs
     * other bookkeeping and prerequisites.
     *
     * Returns the empty string on success, or an error message.
     */
    std::string
    init_module_mapper(const std::string &module_file_path,
                       const std::string &alt_module_dir);

    /**
     * Returns in the \p decode_pc reference parameter the address where the encoding
     * for the instruction at \p trace_pc can be found.
     *
     * Returns the empty string on success, or an error message.
     */
    std::string
    find_mapped_trace_address(app_pc trace_pc, app_pc &decode_pc);

    /**
     * Returns the #dynamorio::drmemtrace::offline_file_type_t arch bit that
     * corresponds to the current build environment.
     */
    offline_file_type_t
    build_arch_file_type();

private:
    /**
     * Creates a module_mapper_t. This does not need to worry about races as the
     * module_mapper_mutex_ will be acquired before calling.
     *
     * Non-static to allow sub-classes to override. This is guaranteed to
     * be invoked only when the count of existing
     * #dynamorio::drmemtrace::decode_cache_t instances that are initialized with
     * a non-empty module_file_path is zero.
     *
     * Returns the empty string on success, or an error message.
     */
    virtual std::string
    make_module_mapper(const std::string &module_file_path,
                       const std::string &alt_module_dir);

    // Cached values for the last lookup to the module_mapper_t. These help
    // avoid redundant lookups and lock acquisition for consecutive queries
    // corresponding to the same application module in the trace.
    // Any trace_pc that lies in the range [last_trace_module_start_,
    // last_trace_module_start_ + last_mapped_module_size_) can be assumed
    // to be mapped to last_mapped_module_start_ + (trace_pc -
    // last_trace_module_start_).

    // Address where the last-queried module was mapped to in the traced
    // application's address space.
    app_pc last_trace_module_start_ = nullptr;
    // Address where the last-queried module is mapped to in our current
    // address space.
    app_pc last_mapped_module_start_ = nullptr;
    // Size of the mapping for the last-queried module.
    size_t last_mapped_module_size_ = 0;
};

/**
 * A cache to store decode info for instructions per observed app pc. The template arg
 * DecodeInfo is a class derived from #dynamorio::drmemtrace::decode_info_base_t which
 * implements the set_decode_info_derived() function that derives the required decode
 * info from an #instr_t object and raw encoding bytes when invoked by
 * #dynamorio::drmemtrace::decode_cache_t. This class handles the heavy lifting of
 * determining the address where the instruction raw bytes can be found (which can be
 * inside the instr #memref_t, or in the mapped application binaries for legacy traces),
 * actually producing the decoded #instr_t, and managing the DecodeInfo cache (which
 * includes invalidating stale DecodeInfo based on the encoding_is_new field in
 * traces with embedded encodings).
 *
 * In general use, \p include_decoded_instr_ should be set to true, but may be set to
 * false if the user wants to perform decoding themselves. In this case, the #instr_t
 * provided to set_decode_info_derived() will be nullptr, and the
 * #dynamorio::drmemtrace::decode_cache_t object merely acts as a cache and provider of
 * the instruction raw bytes.
 *
 * The decoded #instr_t may be made to persist beyond the set_decode_info() calls by
 * constructing the #dynamorio::drmemtrace::decode_cache_t object with
 * \p persist_decoded_instr_ set to true.
 *
 * \p include_decoded_instr_ cannot be false if \p persist_decoded_instr_ is true.
 *
 * Usage note: after constructing an object, init() must be called.
 */
template <class DecodeInfo> class decode_cache_t : public decode_cache_base_t {
    static_assert(std::is_base_of<decode_info_base_t, DecodeInfo>::value,
                  "DecodeInfo not derived from decode_info_base_t");

public:
    decode_cache_t(void *dcontext, bool include_decoded_instr, bool persist_decoded_instr,
                   unsigned int verbosity = 0)
        : decode_cache_base_t(verbosity)
        , dcontext_(dcontext)
        , include_decoded_instr_(include_decoded_instr)
        , persist_decoded_instr_(persist_decoded_instr)
    {
        // Cannot persist the decoded instr if it is not requested.
        assert(!persist_decoded_instr_ || include_decoded_instr_);
    };
    virtual ~decode_cache_t() override
    {
    }

    /**
     * Returns a pointer to the DecodeInfo available for the instruction at \p pc.
     * Returns nullptr if no instruction is known at that \p pc. Returns the
     * default-constructed DecodeInfo if an instr was seen at that \p pc but there
     * was a decoding error for the instruction.
     *
     * Guaranteed to be non-nullptr and valid if the prior add_decode_info() for
     * that \p pc returned no error.
     *
     * When analyzing #memref_t from a trace, it may be better to just use
     * add_decode_info() instead (as it also returns the added DecodeInfo) if
     * it's possible that the instr at \p pc may have changed (e.g., JIT code)
     * in which case the cache will need to be updated.
     */
    DecodeInfo *
    get_decode_info(app_pc pc)
    {
        auto it = decode_cache_.find(pc);
        if (it == decode_cache_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    /**
     * Adds decode info for the given \p memref_instr if it is not yet recorded.
     * or if it contains a new encoding.
     *
     * Uses the embedded encodings in the trace or, if init() was invoked with
     * a module file path, the encodings from the instantiated
     * #dynamorio::drmemtrace::module_mapper_t.
     *
     * If there is a failure either during decoding or creation of the DecodeInfo
     * object, a DecodeInfo that returns is_valid() == false and the relevant
     * error info in get_error_string() will be added to the cache.
     *
     * Returns a pointer to whatever DecodeInfo is present in the cache in the
     * \p cached_decode_info reference pointer parameter, or a nullptr if none
     * is cached. This helps avoid a repeated lookup in a subsequent
     * get_decode_info() call.
     *
     * Returns the error string, or an empty string if there was no error. A
     * valid DecodeInfo is guaranteed to be in the cache if there was no
     * error.
     */
    std::string
    add_decode_info(const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    DecodeInfo *&cached_decode_info)
    {
        cached_decode_info = nullptr;
        if (!init_done_) {
            return "init() must be called first";
        }
        const app_pc trace_pc = reinterpret_cast<app_pc>(memref_instr.addr);

        // XXX: Simplify using try_emplace when we upgrade to C++17.
        auto info = decode_cache_.find(trace_pc);
        bool already_exists = info != decode_cache_.end();
        if (!already_exists) {
            auto it_inserted =
                decode_cache_.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(trace_pc), std::tuple<> {});
            info = it_inserted.first;
            assert(it_inserted.second);
        }

        if (already_exists &&
            // We can return the existing cached DecodeInfo if:
            // - we're using the module mapper, where we don't support the
            //   change-prone JIT encodings; or
            // - we're using embedded encodings from the trace, and the
            //   current instr explicitly says the encoding in the current
            //   memref_instr isn't new.
            (use_module_mapper_ || !memref_instr.encoding_is_new)) {
            // We return the cached DecodeInfo even if it is !is_valid();
            // attempting decoding again is not useful because the encoding
            // hasn't changed.
            cached_decode_info = &info->second;
            // Return the original error string if any.
            return cached_decode_info->get_error_string();
        } else if (already_exists) {
            // We may end up here if we're using the embedded encodings from
            // the trace and now we have a new instr at trace_pc.
            info->second = DecodeInfo();
        }
        cached_decode_info = &info->second;

        // Get address for the instr encoding raw bytes.
        app_pc decode_pc;
        if (!use_module_mapper_) {
            decode_pc = const_cast<app_pc>(memref_instr.encoding);
        } else {
            // Legacy trace support where we need the binaries.
            std::string err = find_mapped_trace_address(trace_pc, decode_pc);
            if (!err.empty()) {
                cached_decode_info->error_string_ = err;
                return err;
            }
        }

        // Optionally decode the instruction.
        instr_t *instr = nullptr;
        instr_noalloc_t noalloc;
        if (include_decoded_instr_) {
            if (persist_decoded_instr_) {
                instr = instr_create(dcontext_);
            } else {
                instr_noalloc_init(dcontext_, &noalloc);
                instr = instr_from_noalloc(&noalloc);
            }

            app_pc next_pc = decode_from_copy(dcontext_, decode_pc, trace_pc, instr);
            if (next_pc == nullptr || !instr_valid(instr)) {
                if (persist_decoded_instr_) {
                    instr_destroy(dcontext_, instr);
                }
                cached_decode_info->error_string_ = "decode_from_copy failed";
                return cached_decode_info->get_error_string();
            }
        }
        info->second.set_decode_info(dcontext_, memref_instr, instr, decode_pc);
        return info->second.get_error_string();
    }

    /**
     * Performs initialization tasks such as verifying whether the given trace
     * indeed has embedded encodings or not, and initializing the
     * #dynamorio::drmemtrace::module_mapper_t if the module path is provided.
     *
     * It is important to note some nuances in how the filetype can be obtained:
     * - the trace filetype may be obtained using the get_filetype() API on the
     *   #dynamorio::drmemtrace::memtrace_stream_t. However, instances of
     *   #dynamorio::drmemtrace::memtrace_stream_t have the filetype available at
     *   init time (in the #dynamorio::drmemtrace::analysis_tool_t::initialize()
     *   or #dynamorio::drmemtrace::analysis_tool_t::initialize_stream()
     *   functions) only for offline analysis, not for online analysis.
     * - when using the -skip_instrs or -skip_timestamp analyzer options, all
     *   initial header entries are skipped over. Therefore, the analysis tool
     *   may not see a #TRACE_MARKER_TYPE_FILETYPE at all.
     *
     * The most reliable way to obtain the filetype (and call this init() API),
     * would be to use #dynamorio::drmemtrace::memtrace_stream_t::get_filetype()
     * just before processing the first instruction memref in the
     * #dynamorio::drmemtrace::analysis_tool_t::process_memref() or
     * #dynamorio::drmemtrace::analysis_tool_t::parallel_shard_memref() APIs.
     *
     * If the \p module_file_path parameter is not empty, it instructs the
     * #dynamorio::drmemtrace::decode_cache_t object that it should look for the
     * instr encodings in the application binaries using a
     * #dynamorio::drmemtrace::module_mapper_t. A
     * #dynamorio::drmemtrace::module_mapper_t is instantiated only one time and
     * reused for all objects of #dynamorio::drmemtrace::decode_cache_t (of any
     * template type). The user must provide a valid \p module_file_path if decoding
     * instructions from a trace that does not have embedded instruction encodings
     * in it, as indicated by absence of the #OFFLINE_FILE_TYPE_ENCODINGS bit in the
     * #TRACE_MARKER_TYPE_FILETYPE marker. The user may provide a
     * \p module_file_path also if they deliberately need to use the module mapper
     * instead of the embedded encodings. Each instance of
     * #dynamorio::drmemtrace::decode_cache_t must be initialized with either an
     * empty \p module_file_path, or the same one as other instances that also
     * specified a non-empty path (even the ones in other analysis tools).
     *
     * If the provided \p module_file_path is empty, the cache object uses
     * the encodings embedded in the trace records.
     *
     * This also sets the isa_mode in dcontext based on the arch bits in the
     * provided \p filetype, unless the instance was not asked to include
     * decoded instructions via the \p include_decoded_instr_ param to the
     * constructor.
     */
    virtual std::string
    init(offline_file_type_t filetype, const std::string &module_file_path = "",
         const std::string &alt_module_dir = "")
    {
        if (init_done_) {
            return "init already done";
        }
        if (include_decoded_instr_) {
            // We remove OFFLINE_FILE_TYPE_ARCH_REGDEPS from this check since
            // DR_ISA_REGDEPS is not a real ISA and can coexist with any real
            // architecture.
            if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL & ~OFFLINE_FILE_TYPE_ARCH_REGDEPS,
                        filetype) &&
                !TESTANY(build_arch_file_type(), filetype)) {
                return std::string("Architecture mismatch: trace recorded on ") +
                    trace_arch_string(static_cast<offline_file_type_t>(filetype)) +
                    " but tool built for " + trace_arch_string(build_arch_file_type());
            }

            // We do not make any changes to decoding related state in dcontext if we
            // are not asked to decode.
            //
            // If we are dealing with a regdeps trace, we need to set the dcontext
            // ISA mode to the correct synthetic ISA (i.e., DR_ISA_REGDEPS).
            if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, filetype)) {
                // Because the dcontext used in analysis tools is a shared global
                // resource, we guard its access to avoid data races. Though note that
                // writing to the isa_mode is a benign data race, as all threads are
                // writing the same isa_mode value.
                std::lock_guard<std::mutex> guard(dcontext_mutex_);
                dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext_);
                if (isa_mode != DR_ISA_REGDEPS)
                    dr_set_isa_mode(dcontext_, DR_ISA_REGDEPS, nullptr);
            }
        }

        if (!TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype) && module_file_path.empty()) {
            return "Trace does not have embedded encodings, and no module_file_path "
                   "provided";
        }
        if (module_file_path.empty()) {
            init_done_ = true;
            return "";
        }
        std::string err = init_module_mapper(module_file_path, alt_module_dir);
        if (!err.empty()) {
            return err;
        }
        init_done_ = true;
        return "";
    }

    /**
     * Clears all cached decode info entries.
     *
     * Typically analysis tools like to keep their per-shard data around till all shards
     * are done processing (so they can combine the shards and use the results), but
     * this API optionally allows tools to keep memory consumption in check by discarding
     * the decode cache entries in parallel_shard_exit(), since it's very likely that the
     * decode cache is not needed for result computation.
     *
     * This does not affect the state of any initialized module mapper, which is still
     * cleaned up during destruction.
     */
    void
    clear_cache()
    {
        // Just a clear() does not release all memory held by the unordered_map. So we
        // need to fully replace it with a new one.
        decode_cache_ = std::unordered_map<app_pc, DecodeInfo>();
    }

private:
    std::unordered_map<app_pc, DecodeInfo> decode_cache_;
    void *dcontext_ = nullptr;
    std::mutex dcontext_mutex_;
    bool include_decoded_instr_ = false;
    bool persist_decoded_instr_ = false;

    // Describes whether init() was invoked.
    // This helps in detecting cases where a module mapper was not specified
    // when decoding a trace without embedded encodings.
    bool init_done_ = false;
};

/**
 * A #dynamorio::drmemtrace::decode_cache_t for testing which uses a
 * #dynamorio::drmemtrace::test_module_mapper_t.
 */
template <class DecodeInfo>
class test_decode_cache_t : public decode_cache_t<DecodeInfo> {
public:
    // The ilist arg is required only for testing the module_mapper_t
    // decoding strategy.
    test_decode_cache_t(void *dcontext, bool include_decoded_instr,
                        bool persist_decoded_instr,
                        instrlist_t *ilist_for_test_module_mapper = nullptr)
        : decode_cache_t<DecodeInfo>(dcontext, include_decoded_instr,
                                     persist_decoded_instr)
        , dcontext_(dcontext)
        , ilist_for_test_module_mapper_(ilist_for_test_module_mapper)
    {
    }

private:
    using decode_cache_base_t::module_mapper_;

    void *dcontext_;
    instrlist_t *ilist_for_test_module_mapper_;

    std::string
    make_module_mapper(const std::string &unused_module_file_path,
                       const std::string &unused_alt_module_dir) override
    {
        if (ilist_for_test_module_mapper_ == nullptr)
            return "No ilist to init test_module_mapper_t";
        module_mapper_ = std::unique_ptr<module_mapper_t>(
            new test_module_mapper_t(ilist_for_test_module_mapper_, dcontext_));
        return "";
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INSTR_DECODE_CACHE_H_ */
