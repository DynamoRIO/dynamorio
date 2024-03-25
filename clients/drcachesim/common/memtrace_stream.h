/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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

/* memtrace_stream: an interface to access aspects of the full stream of memory
 * trace records.
 *
 * We had considered other avenues for analysis_tool_t to obtain things like
 * the record and instruction ordinals within the stream, in the presence of
 * skipping: we could add fields to memref but we'd either have to append
 * and have them at different offsets for each type or we'd have to break
 * compatibility to prepend every time we added more; or we could add parameters
 * to process_memref().  Passing an interface to the init routines seems
 * the simplest and most flexible.
 */

#ifndef _MEMTRACE_STREAM_H_
#define _MEMTRACE_STREAM_H_ 1

#include <cstdint>
#include <string>
#include <unordered_map>

/**
 * @file drmemtrace/memtrace_stream.h
 * @brief DrMemtrace interface for obtaining information from analysis
 * tools on the full stream of memory reference records.
 */

namespace dynamorio {  /**< General DynamoRIO namespace. */
namespace drmemtrace { /**< DrMemtrace tracing + simulation infrastructure namespace. */

/**
 * This is an interface for obtaining information from analysis tools
 * on the full stream of memory reference records.
 */
class memtrace_stream_t {
public:
    /** Destructor. */
    virtual ~memtrace_stream_t()
    {
    }
    /**
     * Returns the count of #memref_t records from the start of the
     * trace to this point. This includes records skipped over and not presented to any
     * tool. It does not include synthetic records (see is_record_synthetic()).
     */
    virtual uint64_t
    get_record_ordinal() const = 0;
    /**
     * Returns the count of instructions from the start of the trace to this point.
     * This includes instructions skipped over and not presented to any tool.
     */
    virtual uint64_t
    get_instruction_ordinal() const = 0;

    /**
     * Returns a name for the memtrace stream. For stored offline traces, this
     * is the base name of the trace on disk. For online traces, this is the name
     * of the pipe.
     */
    virtual std::string
    get_stream_name() const = 0;

    /**
     * Returns the value of the most recently seen
     * #TRACE_MARKER_TYPE_TIMESTAMP marker.
     */
    virtual uint64_t
    get_last_timestamp() const = 0;

    /**
     * Returns the value of the first seen
     * #TRACE_MARKER_TYPE_TIMESTAMP marker.
     */
    virtual uint64_t
    get_first_timestamp() const = 0;

    /**
     * Returns the #trace_version_t value from the
     * #TRACE_MARKER_TYPE_VERSION record in the trace header.
     */
    virtual uint64_t
    get_version() const = 0;

    /**
     * Returns the OFFLINE_FILE_TYPE_* bitfields of type
     * #offline_file_type_t identifying the architecture and other
     * key high-level attributes of the trace from the
     * #TRACE_MARKER_TYPE_FILETYPE record in the trace header.
     */
    virtual uint64_t
    get_filetype() const = 0;

    /**
     * Returns the cache line size from the
     * #TRACE_MARKER_TYPE_CACHE_LINE_SIZE record in the trace
     * header.
     */
    virtual uint64_t
    get_cache_line_size() const = 0;

    /**
     * Returns the chunk instruction count from the
     * #TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT record in the trace
     * header.
     */
    virtual uint64_t
    get_chunk_instr_count() const = 0;

    /**
     * Returns the page size from the #TRACE_MARKER_TYPE_PAGE_SIZE
     * record in the trace header.
     */
    virtual uint64_t
    get_page_size() const = 0;

    /**
     * Returns whether the current record was synthesized and inserted into the record
     * stream and was not present in the original stream.  This is true for timestamp
     * and cpuid headers duplicated after skipping ahead, as well as cpuid markers
     * inserted for synthetic schedules.  Such records do not count toward the record
     * count and get_record_ordinal() will return the value of the prior record.
     */
    virtual bool
    is_record_synthetic() const
    {
        return false;
    }

    /**
     * Returns the 0-based ordinal for the current shard.  For parallel analysis,
     * this equals the \p shard_index passed to parallel_shard_init_stream().
     * This is more useful for serial modes where there is no other convenience mechanism
     * to determine such an index; it allows a tool to compute per-shard results even in
     * serial mode.  The shard orderings in serial mode may not always mach the ordering
     * in parallel mode. If not implemented, -1 is returned.
     */
    virtual int
    get_shard_index() const
    {
        return -1;
    }

    /**
     * Returns a unique identifier for the current "output cpu".  Generally this only
     * applies when using #SHARD_BY_CORE.  For dynamic schedules, the identifier is
     * typically an output cpu ordinal equal to get_shard_index().  For replaying an
     * as-traced schedule, the
     * identifier is typically the original input cpu which is now mapped directly
     * to this output.  If not implemented for the current mode, -1 is returned.
     */
    virtual int64_t
    get_output_cpuid() const
    {
        return -1;
    }

    /**
     * Returns a unique identifier for the current workload.  This might be an ordinal
     * from the list of active workloads, or some other identifier.  This is guaranteed
     * to be unique among all inputs, unlike the process and thread identifiers in
     * #memref_t. If not implemented for the current mode, -1 is returned.
     */
    virtual int64_t
    get_workload_id() const
    {
        return -1;
    }

    /**
     * Returns a unique identifier for the current input trace.  This might be an ordinal
     * from the list of active inputs, or some other identifier.  This is guaranteed to
     * be unique among all inputs, unlike the process and thread identifiers in
     * #memref_t.  If not implemented for the current mode, -1 is returned.
     */
    virtual int64_t
    get_input_id() const
    {
        return -1;
    }

    /**
     * Returns the thread identifier for the current input trace.
     * This is a convenience method for use in parallel_shard_init_stream()
     * prior to access to any #memref_t records.
     */
    virtual int64_t
    get_tid() const
    {
        return -1;
    }

    /**
     * Returns the stream interface for the current input trace.  This differs from
     * "this" for #SHARD_BY_CORE where multiple inputs are interleaved on one
     * output stream ("this").
     * If not implemented for the current mode, nullptr is returned.
     */
    virtual memtrace_stream_t *
    get_input_interface() const
    {
        return nullptr;
    }

    /**
     * Returns whether the current record is from a part of the trace corresponding
     * to kernel execution.
     */
    virtual bool
    is_record_kernel() const
    {
        return false;
    }
};

/**
 * Implementation of memtrace_stream_t useful for mocks in tests.
 */
class default_memtrace_stream_t : public memtrace_stream_t {
public:
    default_memtrace_stream_t()
        : record_ordinal_(nullptr)
    {
    }
    default_memtrace_stream_t(uint64_t *record_ordinal)
        : record_ordinal_(record_ordinal)
    {
    }
    virtual ~default_memtrace_stream_t()
    {
    }
    uint64_t
    get_record_ordinal() const override
    {
        if (record_ordinal_ == nullptr)
            return 0;
        return *record_ordinal_;
    }
    uint64_t
    get_instruction_ordinal() const override
    {
        return 0;
    }
    std::string
    get_stream_name() const override
    {
        return "";
    }
    uint64_t
    get_last_timestamp() const override
    {
        return 0;
    }
    uint64_t
    get_first_timestamp() const override
    {
        return 0;
    }
    uint64_t
    get_version() const override
    {
        return 0;
    }
    uint64_t
    get_filetype() const override
    {
        return 0;
    }
    uint64_t
    get_cache_line_size() const override
    {
        return 0;
    }
    uint64_t
    get_chunk_instr_count() const override
    {
        return 0;
    }
    uint64_t
    get_page_size() const override
    {
        return 0;
    }

    void
    set_output_cpuid(int64_t cpuid)
    {
        cpuid_ = cpuid;
    }
    int64_t
    get_output_cpuid() const override
    {
        return cpuid_;
    }
    void
    set_shard_index(int index)
    {
        shard_ = index;
    }
    int
    get_shard_index() const override
    {
        return shard_;
    }
    // Also sets the shard index to the dynamic-discovery-order tid ordinal.
    void
    set_tid(int64_t tid)
    {
        tid_ = tid;
        auto exists = tid2shard_.find(tid);
        if (exists == tid2shard_.end()) {
            int index = static_cast<int>(tid2shard_.size());
            tid2shard_[tid] = index;
            set_shard_index(index);
        } else {
            set_shard_index(exists->second);
        }
    }
    int64_t
    get_tid() const override
    {
        return tid_;
    }

private:
    uint64_t *record_ordinal_ = nullptr;
    int64_t cpuid_ = 0;
    int shard_ = 0;
    int64_t tid_ = 0;
    // To let a test set just the tid and get a shard index for free.
    std::unordered_map<int64_t, int> tid2shard_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _MEMTRACE_STREAM_H_ */
