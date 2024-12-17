/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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
 * decode_cache.h: Library that supports caching of instruction decode
 * information.
 */

#ifndef _DECODE_CACHE_H_
#define _DECODE_CACHE_H_ 1

#include "memref.h"
#include "tracer/raw2trace_shared.h"

#include <cstring>
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
public:
    /**
     * Sets the decode info for the provided \p instr which was allocated using the
     * provided \p dcontext for the provided \p memref_instr. This is done using
     * the set_decode_info_derived() provided by the derived class. Additionally,
     * this does other required bookkeeping.
     */
    void
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr);

    /**
     * Indicates whether the decode info stored in this object is valid. It won't be
     * valid if the object is default-constructed without a subsequent
     * set_decode_info() call. When used with \p decode_cache_t, this indicates
     * that an invalid instruction was observed at some pc.
     */
    bool
    is_valid() const;

private:
    /**
     * Sets the decoding info fields as required by the derived class, based on the
     * provided instr_t which was allocated using the provided opaque \p dcontext
     * for the provided \p memref_instr. Derived classes must implement this virtual
     * function. Note that this cannot be invoked directly as it is private, but
     * only through set_decode_info() which does other required bookkeeping.
     *
     * This is meant for use with \p decode_cache_t, which will invoke
     * set_decode_info() for each new decoded instruction.
     *
     * The responsibility for invoking instr_destroy() on the provided \p instr
     * lies with this \p decode_info_base_t object, unless
     * \p decode_cache_t was constructed with \p persist_decoded_instrs_
     * set to false, in which case no heap allocation takes place.
     */
    virtual void
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr) = 0;

    bool is_valid_ = false;
};

/**
 * Decode info including the full decoded instr_t. This should be used with an
 * \p decode_cache_t constructed with \p persist_decoded_instrs_ set to
 * true.
 */
class instr_decode_info_t : public decode_info_base_t {
public:
    ~instr_decode_info_t();
    instr_t *
    get_decoded_instr();

private:
    void
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr) override;

    instr_t *instr_ = nullptr;
    void *dcontext_ = nullptr;
};

/**
 * Base class for \p decode_cache_t.
 *
 * This is used to allow sharing the static data members among all template instances
 * of \p decode_cache_t.
 */
class decode_cache_base_t {
protected:
    /**
     * Constructor for the base class, intentionally declared as protected so
     * \p decode_cache_base_t cannot be instantiated directly but only via
     * a derived class.
     */
    decode_cache_base_t() = default;
    virtual ~decode_cache_base_t();

    static std::mutex module_mapper_mutex_;
    static std::unique_ptr<module_mapper_t> module_mapper_;
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
     * all decode_cache_t instances (even of different template types).
     * Some analysis tools may deliberately want to look at instr encodings
     * from the module mappings, but that strategy does not provide JIT
     * encodings which are present only as embedded-encodings in the trace.
     * In such a case, other concurrently running analysis tools should still
     * be able to see encodings for JIT code.
     */
    bool use_module_mapper_ = 0;

public:
    /**
     * Initializes the module_mapper_ object using make_module_mapper() and performs
     * other bookkeeping and prerequisites.
     */
    std::string
    init_module_mapper(const std::string &module_file_path,
                       const std::string &alt_module_dir);

    /**
     * Creates a module_mapper_t. This does not need to worry about races as the
     * module_mapper_mutex_ will be acquired before calling.
     * Non-static to allow test sub-classes to override.
     */
    virtual std::string
    make_module_mapper(const std::string &module_file_path,
                       const std::string &alt_module_dir);

    /**
     * Returns the address where the encoding for the instruction at trace_pc can
     * be found.
     */
    static std::string
    find_mapped_trace_address(app_pc trace_pc, app_pc &decode_pc);
};

/**
 * A cache to store decode info for instructions per observed app pc. The template arg
 * DecodeInfo is a class derived from \p decode_info_base_t which implements the
 * set_decode_info_derived() function that derives the required decode info from an
 * \p instr_t object when invoked by \p decode_cache_t. This class handles the
 * heavylifting of actually producing the decoded \p instr_t. The decoded \p instr_t
 * may be made to persist beyond the set_decode_info() calls by constructing the
 * \p decode_cache_t object with \p persist_decoded_instrs_ set to true.
 *
 * Usage note: after constructing an object, init() must be called.
 */
template <class DecodeInfo> class decode_cache_t : public decode_cache_base_t {
    static_assert(std::is_base_of<decode_info_base_t, DecodeInfo>::value,
                  "DecodeInfo not derived from decode_info_base_t");

public:
    decode_cache_t(void *dcontext, bool persist_decoded_instrs)
        : dcontext_(dcontext)
        , persist_decoded_instrs_(persist_decoded_instrs) {};
    virtual ~decode_cache_t()
    {
    }

    /**
     * Returns a pointer to the DecodeInfo available for the instruction at \p pc.
     * Returns nullptr if no instruction is known at that \p pc. Returns the
     * default-constructed DecodeInfo if there was a decoding error for the
     * instruction.
     *
     * Guaranteed to be non-nullptr if the prior add_decode_info for that pc
     * returned no error.
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
     *
     * Uses the embedded encodings in the trace or, if init() was invoked with
     * a module file path, the encodings from the instantiated
     * \p module_mapper_t.
     *
     * If there is a decoding failure, the default-constructed DecodeInfo that
     * returns is_valid() = false will be added to the cache.
     *
     * Returns the error string, or an empty string if there was no error.
     */
    std::string
    add_decode_info(const dynamorio::drmemtrace::_memref_instr_t &memref_instr)
    {
        if (!init_done_) {
            return "init() must be called first";
        }
        const app_pc trace_pc = reinterpret_cast<app_pc>(memref_instr.addr);
        if (!use_module_mapper_ && memref_instr.encoding_is_new) {
            // If the user asked to use the module mapper despite having
            // embedded encodings in the trace, we don't invalidate the
            // cached encoding.
            decode_cache_.erase(trace_pc);
        }
        auto it = decode_cache_.find(trace_pc);
        // If the prior cached info was invalid, we try again.
        if (it != decode_cache_.end() && it->second.is_valid()) {
            return "";
        }
        decode_cache_[trace_pc] = DecodeInfo();
        instr_t *instr = nullptr;
        instr_noalloc_t noalloc;
        if (persist_decoded_instrs_) {
            instr = instr_create(dcontext_);
        } else {
            instr_noalloc_init(dcontext_, &noalloc);
            instr = instr_from_noalloc(&noalloc);
        }

        app_pc decode_pc;
        if (!use_module_mapper_) {
            decode_pc = const_cast<app_pc>(memref_instr.encoding);
        } else {
            // Legacy trace support where we need the binaries.
            std::string err = find_mapped_trace_address(trace_pc, decode_pc);
            if (err != "")
                return err;
        }
        app_pc next_pc = decode_from_copy(dcontext_, decode_pc, trace_pc, instr);
        if (next_pc == nullptr || !instr_valid(instr)) {
            if (persist_decoded_instrs_) {
                instr_destroy(dcontext_, instr);
            }
            return "decode_from_copy failed";
        }
        decode_cache_[trace_pc].set_decode_info(dcontext_, memref_instr, instr);
        return "";
    }

    /**
     * Performs initialization tasks such as verifying whether the given trace
     * indeed has embedded encodings or not, and initializing the
     * \p module_mapper_t if the module path is provided.
     *
     * It is important to note that the trace filetype may be obtained using the
     * get_filetype() API on a \p memtrace_stream_t. However, not all instances of
     * \p memtrace_stream_t have the filetype available at init time (before the
     * \p TRACE_MARKER_TYPE_FILETYPE is actually returned by the stream).
     * Particularly, the \p stream_t provided by the \p scheduler_t to analysis
     * tools provides the file type at init time when #dynamorio::drmemtrace::
     * scheduler_tmpl_t::scheduler_options_t.read_inputs_in_init is set. In other
     * cases, the user may call this API after init time when the
     * \p TRACE_MARKER_TYPE_FILETYPE marker is seen (still before any instr
     * record).
     *
     * If the provided \p module_file_path is empty, the cache object uses
     * the encodings embedded in the trace records.
     *
     * If the \p module_file_path parameter is not empty, it instructs the
     * \p decode_cache_t object that it should look for the instr encodings in the
     * application binaries using a \p module_mapper_t. A \p module_mapper_t is
     * instantiated only one time and reused for all objects
     * of \p decode_cache_t (of any template type). The user must provide a valid
     * \p module_file_path if decoding instructions from a trace that does not
     * have embedded instruction encodings in it, as indicated by absence of
     * the \p OFFLINE_FILE_TYPE_ENCODINGS bit in the \p TRACE_MARKER_TYPE_FILETYPE
     * marker. The user may provide a \p module_file_path also if they
     * deliberately need to use the module mapper instead of the embedded
     * encodings.
     */
    std::string
    init(offline_file_type_t filetype, const std::string &module_file_path = "",
         const std::string &alt_module_dir = "")
    {
        // TODO i#7113: Should we perhaps also do dr_set_isa_mode for DR_ISA_REGDEPS
        // that cannot be figured out automatically using the build but needs the
        // trace filetype? Or does that belong in the analyzer framework which is more
        // central than this decode_cache_t.
        init_done_ = true;
        if (!TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype) && module_file_path.empty()) {
            return "Trace does not have embedded encodings, and no module_file_path "
                   "provided";
        }
        if (module_file_path.empty())
            return "";
        return init_module_mapper(module_file_path, alt_module_dir);
    }

private:
    std::unordered_map<app_pc, DecodeInfo> decode_cache_;
    void *dcontext_ = nullptr;
    bool persist_decoded_instrs_ = false;

    // Describes whether init() was invoked.
    // This helps in detecting cases where a module mapper was not specified
    // when decoding a trace without embedded encodings.
    bool init_done_ = false;
};

/**
 *  An \p decode_cache_t for testing which uses a \p test_module_mapper_t.
 */
template <class DecodeInfo>
class test_decode_cache_t : public decode_cache_t<DecodeInfo> {
public:
    using decode_cache_base_t::module_mapper_;

    // The ilist arg is required only for testing the module_mapper_t
    // decoding strategy.
    test_decode_cache_t(void *dcontext, bool persist_decoded_instrs,
                        instrlist_t *ilist_for_test_module_mapper = nullptr)
        : decode_cache_t<DecodeInfo>(dcontext, persist_decoded_instrs)
        , dcontext_(dcontext)
        , ilist_for_test_module_mapper_(ilist_for_test_module_mapper)
    {
    }

    std::string
    make_module_mapper(const std::string &unused_module_file_path,
                       const std::string &unused_alt_module_dir) override
    {
        if (ilist_for_test_module_mapper_ == nullptr)
            return "No ilist to init test_module_mapper_t";
        module_mapper_ = std::unique_ptr<module_mapper_t>(
            new test_module_mapper_t(ilist_for_test_module_mapper_, dcontext_));
        module_mapper_->get_loaded_modules();
        std::string error = module_mapper_->get_last_error();
        if (!error.empty())
            return "Failed to load binaries: " + error;
        return "";
    }

private:
    void *dcontext_;
    instrlist_t *ilist_for_test_module_mapper_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INSTR_DECODE_CACHE_H_ */
