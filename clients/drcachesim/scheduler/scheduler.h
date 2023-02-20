/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* Scheduler of traced software threads onto simulated cpus. */

#ifndef _DRMEMTRACE_SCHEDULER_H_
#define _DRMEMTRACE_SCHEDULER_H_ 1

/**
 * @file drmemtrace/scheduler.h
 * @brief DrMemtrace top-level trace scheduler.
 */

#include <assert.h>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>
#include "memref.h"
#include "memtrace_stream.h"
#include "reader.h"
#include "record_file_reader.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Schedules traced software threads onto simulated cpus.
 * Takes in a set of recorded traces and maps them onto a new set of output
 * streams, typically representing simulated cpus.
 *
 * This is a templated class to support not just operating over #memref_t inputs
 * read by #reader_t, but also over #trace_entry_t records read by
 * #dynamorio::drmemtrace::record_reader_t.
 */
template <typename RecordType, typename ReaderType> class scheduler_tmpl_t {
public:
    /**
     * Status codes used by non-stream member functions.
     * get_error_string() provides additional information such was which input file
     * path failed.
     */
    enum scheduler_status_t {
        STATUS_SUCCESS,                 /**< Success. */
        STATUS_ERROR_INVALID_PARAMETER, /**< Error: invalid parameter. */
        STATUS_ERROR_FILE_OPEN_FAILED,  /**< Error: file open failed. */
        STATUS_ERROR_FILE_READ_FAILED,  /**< Error: file read failed. */
        STATUS_ERROR_NOT_IMPLEMENTED,   /**< Error: not implemented. */
    };

    /**
     * Status codes used by stream member functions.
     * get_error_string() provides additional information such was which input file
     * path failed.
     */
    enum stream_status_t {
        STATUS_OK,              /**< Stream is healthy and can continue to advance. */
        STATUS_EOF,             /**< Stream is at its end. */
        STATUS_IDLE,            /**< No records at this time. */
        STATUS_WAIT,            /**< Waiting for other streams to advance. */
        STATUS_INVALID,         /**< Error condition. */
        STATUS_REGION_INVALID,  /**< Input region is out of bounds. */
        STATUS_NOT_IMPLEMENTED, /**< Feature not implemented. */
        STATUS_SKIPPED,         /**< Used for internal scheduler purposes. */
    };

    /** A bounded sequence of instructions. */
    struct range_t {
        /** Convenience constructor. */
        range_t(uint64_t start, uint64_t stop)
            : start_instruction(start)
            , stop_instruction(stop)
        {
        }
        /**
         * The starting point as a trace instruction ordinal.  These ordinals
         * begin at 1, so a 'start_instruction' value of 0 is invalid.
         */
        uint64_t start_instruction;
        /** The ending point.  A stop value of 0 means the end of the trace. */
        uint64_t stop_instruction;
    };

    /**
     * Specifies details about one set of input trace files from one workload,
     * each input file representing a single software thread.
     * It is assumed that there is no thread id duplication within one workload.
     */
    struct input_thread_info_t {
        /** Convenience constructor for common usage. */
        explicit input_thread_info_t(std::vector<range_t> regions)
            : struct_size(sizeof(input_thread_info_t))
            , regions_of_interest(regions)
        {
        }
        size_t struct_size; /**< Size of the struct for binary-compatible additions. */
        /**
         * Which threads the details in this structure apply to.
         * If empty, the details apply to all not-yet-mentioned (by other 'tids'
         * vectors in prior entries for this workload) threads in the
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.
         */
        std::vector<memref_tid_t> tids;
        /**
         * Limits these threads to this set of output streams.  They will not
         * be scheduled on any other output streams.
         */
        std::set<int> output_binding;
        /**
         * Relative priority for scheduling.  The default is 0.
         */
        // TODO i#5843: Decide and document whether these priorities are strict
        // and higher will starve lower or whether they are looser.
        int priority = 0;
        /**
         * If non-empty, all input records outside of these ranges are skipped: it is as
         * though the input were constructed by concatenating these ranges together.  A
         * #TRACE_MARKER_TYPE_WINDOW_ID marker is inserted between ranges (with a value
         * equal to the range ordinal) to notify the client of the discontinuity (but
         * not before the first range).
         */
        std::vector<range_t> regions_of_interest;
    };

    /** Specifies the input workloads to be scheduled. */
    struct input_workload_t {
        /**
         * Create a workload coming from a directory of many trace files or from
         * a single trace file.
         */
        explicit input_workload_t(const std::string &trace_path)
            : path(trace_path)
        {
        }
        /** Create a workload with a pre-initialized reader. */
        explicit input_workload_t(std::unique_ptr<ReaderType> reader,
                                  std::unique_ptr<ReaderType> reader_end)
            : reader(std::move(reader))
            , reader_end(std::move(reader_end))
        {
        }
        /**
         * Create a workload coming from a directory of many trace files or from
         * a single trace file where each trace file uses the given regions of interest.
         */
        input_workload_t(const std::string &trace_path,
                         std::vector<range_t> regions_of_interest)
            : struct_size(sizeof(input_workload_t))
            , path(trace_path)
        {
            thread_modifiers.push_back(input_thread_info_t(regions_of_interest));
        }
        /** Size of the struct for binary-compatible additions. */
        size_t struct_size = sizeof(input_workload_t);
        /**
         * A directory of trace files or a single trace file.
         * A reader will be opened for each input file and its init() function
         * will be called; that function will not block (variants where it
         * does block, such as for IPC, should leave 'path' empty and use 'reader'
         * below instead).
         */
        std::string path;
        /**
         * An alternative to passing in a path and having the scheduler open that file(s)
         * is to directly pass in a reader, reader_end, and a thread id for that
         * reader.  These are only considered if 'path' is empty.  The scheduler will
         * call the init() function for 'reader' at the time of the first access to
         * it through an output stream (supporting IPC readers whose init() blocks).
         * There are no tids associated with a workload specified in this way, and
         * any thread modifiers must leave the
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_thread_info_t::tids
         * field empty.
         */
        std::unique_ptr<ReaderType> reader;
        /** The end reader for 'reader'. */
        std::unique_ptr<ReaderType> reader_end;

        /** Scheduling modifiers for the threads in this workload. */
        std::vector<input_thread_info_t> thread_modifiers;
    };

    /** Controls the primary mode of the scheduler regarding the input streams. */
    enum stream_type_t {
        /**
         * Each input stream is mapped without interleaving directly onto a single
         * output stream (i.e., no input will appear on more than one output),
         * supporting parallel analysis.  The only strategy supported here is
         * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULE_RUN_TO_COMPLETION.
         */
        STREAM_BY_INPUT_SHARD,
        /**
         * Each input stream is assumed to contain markers indicating how it was
         * originally scheduled.  This recorded schedule is replayed.  This requires an
         * output stream count equal to the number of cores occupied by the input stream
         * set.  The only strategy supported here is
         * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULE_INTERLEAVE_AS_RECORDED.
         */
        STREAM_BY_RECORDED_CPU,
        /**
         * The input streams are scheduling using a new synthetic schedule onto the
         * output streams.  Any input markers indicating how the software threads were
         * originally scheduled during tracing are ignored.
         */
        STREAM_BY_SYNTHETIC_CPU,
    };

    /** Controls the output of the scheduler. */
    enum schedule_strategy_t {
        /**
         * Runs each input stream to completion without interleaving it with other
         * inputs.  Ignores inter-input dependences.
         */
        SCHEDULE_RUN_TO_COMPLETION,
        /**
         * Interleaves and schedules the inputs exactly as they were recorded,
         * following the markers in the input streams.
         * This requires an output stream count equal to the number of cores
         * occupied by the input stream set.
         * The only stream type supported here is
         * #dynamorio::drmemtrace::scheduler_tmpl_t::STREAM_BY_RECORDED_CPU.
         */
        SCHEDULE_INTERLEAVE_AS_RECORDED,
        // TODO i#5843: Can we get doxygen to link references without the
        // fully-qualified name?  These are so long I had to omit the
        // 'quantum_duration' to stay in the vera++-checked line limit.
        /**
         * Uses a synthetic schedule with pre-emption of an input to be replaced
         * by another input on a given output stream when it uses up its
         * quantum.  The quantum unit is specified by the 'quantum_unit` and
         * the size by the `quantum_duration` fields of
         * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t.
         */
        SCHEDULE_BY_QUANTUM,
    };

    /**
     * Quantum units used by
     * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULE_BY_QUANTUM.
     */
    enum quantum_unit_t {
        /** Uses the instruction count as the quantum. */
        QUANTUM_INSTRUCTIONS,
        /**
         * Uses the user's notion of time as the quantum.
         * This must be supplied by the user by calling
         * dynamorio::drmemtrace::scheduler_tmpl_t::stream_t::report_time()
         * periodically.
         */
        QUANTUM_TIME,
    };

    /** Flags controlling aspects of the scheduler beyond the core scheduling. */
    enum scheduler_flags_t {
        /** Default behavior. */
        SCHEDULER_DEFAULTS = 0,
        // TODO i#5843: Decide and then describe whether the physaddr is in every
        // record or obtained via an API call.
        /**
         * Whether physical addresses should be provided in addition to virtual
         * addresses.
         */
        SCHEDULER_PROVIDE_PHYSICAL_ADDRESSES = 0x1,
        /**
         * Specifies that speculation should supply just NOP instructions.
         */
        SCHEDULER_SPECULATE_NOPS = 0x2,
        // TODO i#5843: Add more speculation flags for other strategies.
    };

    /**
     * Collects the parameters specifying how the scheduler should behave, outside
     * of the workload inputs and the output count.
     */
    struct scheduler_options_t {
        /** Constructs a default set of options. */
        scheduler_options_t()
        {
        }
        /** Constructs a set of options with the given type and strategy. */
        scheduler_options_t(stream_type_t stream_type, schedule_strategy_t strategy)
            : how_split(stream_type)
            , strategy(strategy)
        {
        }

        /** Size of the struct for binary-compatible additions. */
        size_t struct_size = sizeof(scheduler_options_t);
        /** The base mode of the scheduler with respect to input streams. */
        stream_type_t how_split = STREAM_BY_SYNTHETIC_CPU;
        /** The scheduling strategy. */
        schedule_strategy_t strategy = SCHEDULE_BY_QUANTUM;
        /** Optional flags affecting scheduler behavior. */
        scheduler_flags_t flags = SCHEDULER_DEFAULTS;
        /** The unit of the schedule time quantum. */
        quantum_unit_t quantum_unit = QUANTUM_INSTRUCTIONS;
        /**
         * The scheduling quantum duration for preemption.  The units are
         * specified by
         * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t::quantum_unit.
         */
        uint64_t quantum_duration = 10 * 1000 * 1000;
        /**
         * Whether input stream inter-dependencies should be considered when preempting
         * input streams.
         */
        bool consider_input_dependencies = false;
        /**
         * If > 0, diagnostic messages are printed to stderr.  Higher values produce
         * more frequent diagnostics.
         */
        int verbosity = 0;
    };

    /**
     * Represents a stream of RecordType trace records derived from a
     * subset of a set of input recorded traces.  Provides more
     * information about the record stream using the #memtrace_stream_t
     * API.
     */
    class stream_t : public memtrace_stream_t {
    public:
        // TODO i#5843: Add Doxygen comments to all the remaining public elements
        // below this line.
        stream_t(scheduler_tmpl_t<RecordType, ReaderType> *scheduler, int ordinal,
                 int verbosity = 0)
            : scheduler_(scheduler)
            , ordinal_(ordinal)
            , verbosity_(verbosity)
        {
        }

        virtual ~stream_t()
        {
        }

        virtual stream_status_t
        next_record(RecordType &record);

        // This is unitless: it just needs to be called regularly and consistently.
        virtual stream_status_t
        report_time(uint64_t cur_time);

        // These can be "nested"; only one stop_speculation call is needed to resume
        // the paused stream.
        virtual stream_status_t
        start_speculation(addr_t start_address);

        // Returns STATUS_INVALID if there was no prior start_speculation() call.
        virtual stream_status_t
        stop_speculation();

        // memtrace_stream_t interface:

        uint64_t
        get_record_ordinal() const override
        {
            return cur_ref_count_;
        }
        uint64_t
        get_instruction_ordinal() const override
        {
            return cur_instr_count_;
        }
        std::string
        get_stream_name() const override
        {
            return scheduler_->get_input_name(ordinal_);
        }
        uint64_t
        get_last_timestamp() const override
        {
            return last_timestamp_;
        }
        uint64_t
        get_version() const override
        {
            return version_;
        }
        uint64_t
        get_filetype() const override
        {
            return filetype_;
        }
        uint64_t
        get_cache_line_size() const override
        {
            return cache_line_size_;
        }
        uint64_t
        get_chunk_instr_count() const override
        {
            return chunk_instr_count_;
        }
        uint64_t
        get_page_size() const override
        {
            return page_size_;
        }

        // Supplied for better error messages from the client.
        std::string
        get_source_file() const
        {
            return cur_src_path_;
        }

    protected:
        scheduler_tmpl_t<RecordType, ReaderType> *scheduler_ = nullptr;
        int ordinal_ = -1;
        int verbosity_ = 0;
        uint64_t cur_ref_count_ = 0;
        uint64_t cur_instr_count_ = 0;
        uint64_t last_timestamp_ = 0;
        std::string cur_src_path_;
        // Remember top-level headers for the memtrace_stream_t interface.
        uint64_t version_ = 0;
        uint64_t filetype_ = 0;
        uint64_t cache_line_size_ = 0;
        uint64_t chunk_instr_count_ = 0;
        uint64_t page_size_ = 0;

        // Let the outer class update our state.
        friend class scheduler_tmpl_t<RecordType, ReaderType>;
    };

    scheduler_tmpl_t()
    {
    }
    virtual ~scheduler_tmpl_t()
    {
    }

    virtual scheduler_status_t
    init(std::vector<input_workload_t> &workload_inputs, int output_count,
         scheduler_options_t options);

    virtual stream_t *
    get_stream(int ordinal)
    {
        if (ordinal < 0 || ordinal >= static_cast<int>(outputs_.size()))
            return nullptr;
        return &outputs_[ordinal].stream;
    }

    std::string
    get_error_string()
    {
        return error_string_;
    }

protected:
    typedef scheduler_tmpl_t<RecordType, ReaderType> sched_type_t;

    struct input_info_t {
        int index = -1; // Position in inputs_ vector.
        std::unique_ptr<ReaderType> reader;
        std::unique_ptr<ReaderType> reader_end;
        // A tid can be duplicated across workloads so we need the pair of
        // workload index + tid to identify the original input.
        int workload = -1;
        memref_tid_t tid = INVALID_THREAD_ID;
        // If non-empty these records should be returned before incrementing
        // the reader.
        std::queue<RecordType> queue;
        std::set<int> binding;
        int priority = 0;
        std::vector<range_t> regions_of_interest;
        int cur_region = 0;
        bool has_modifier = false;
        bool needs_init = false;
        bool needs_advance = false;
        bool needs_roi = true;
        bool at_eof = false;
    };

    struct output_info_t {
        output_info_t(scheduler_tmpl_t<RecordType, ReaderType> *scheduler, int ordinal,
                      int verbosity = 0)
            : stream(scheduler, ordinal, verbosity)
        {
        }
        stream_t stream;
        int cur_input = -1;
        // For static schedules we can populate this up front and avoid needing a
        // lock for dynamically finding the next input, keeping things parallel.
        std::vector<int> input_indices;
        int input_indices_index = 0;
    };

    scheduler_status_t
    open_readers(const std::string &path,
                 std::unordered_map<memref_tid_t, int> &workload_tids);

    scheduler_status_t
    open_reader(const std::string &path,
                std::unordered_map<memref_tid_t, int> &workload_tids);

    std::unique_ptr<ReaderType>
    get_default_reader();

    std::unique_ptr<ReaderType>
    get_reader(const std::string &path, int verbosity);

    stream_status_t
    next_record(int output_ordinal, RecordType &record, input_info_t *&input);

    stream_status_t
    advance_region_of_interest(int output_ordinal, RecordType &record,
                               input_info_t &input);

    stream_status_t
    pick_next_input(int output_ordinal);

    bool
    record_type_has_tid(RecordType record, memref_tid_t &tid);

    bool
    record_type_is_instr(RecordType record);

    bool
    record_type_is_marker(RecordType record, trace_marker_type_t &type, uintptr_t &value);

    RecordType
    create_region_separator_marker(memref_tid_t tid, uintptr_t value);

    RecordType
    create_thread_exit(memref_tid_t tid);

    void
    print_record(const RecordType &record);

    std::string
    get_input_name(int output_ordinal);

    // This has the same value as scheduler_options_t.verbosity (for use in VPRINT).
    int verbosity_ = 0;
    const char *output_prefix_ = "[scheduler]";
    std::string error_string_;
    scheduler_options_t options_;
    std::vector<input_info_t> inputs_;
    std::vector<output_info_t> outputs_;
};

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<memref_t, reader_t> scheduler_t;

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_scheduler_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SCHEDULER_H_ */
