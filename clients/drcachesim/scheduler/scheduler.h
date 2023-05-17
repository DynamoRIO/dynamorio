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

#define NOMINMAX // Avoid windows.h messing up std::max.
#include <assert.h>
#include <deque>
#include <limits>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>
#include "archive_istream.h"
#include "archive_ostream.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "reader.h"
#include "record_file_reader.h"
#include "speculator.h"
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
     * get_error_string() provides additional information such as which input file
     * path failed.
     */
    enum scheduler_status_t {
        STATUS_SUCCESS,                 /**< Success. */
        STATUS_ERROR_INVALID_PARAMETER, /**< Error: invalid parameter. */
        STATUS_ERROR_FILE_OPEN_FAILED,  /**< Error: file open failed. */
        STATUS_ERROR_FILE_READ_FAILED,  /**< Error: file read failed. */
        STATUS_ERROR_NOT_IMPLEMENTED,   /**< Error: not implemented. */
        STATUS_ERROR_FILE_WRITE_FAILED, /**< Error: file write failed. */
    };

    /**
     * Status codes used by stream member functions.
     * get_error_string() provides additional information such as which input file
     * path failed.
     */
    enum stream_status_t {
        STATUS_OK,  /**< Stream is healthy and can continue to advance. */
        STATUS_EOF, /**< Stream is at its end. */
        /**
         * For dynamic scheduling with cross-stream dependencies, the scheduler may pause
         * a stream if it gets ahead of another stream it should have a dependence on.
         * This value is also used for schedules following the recorded timestamps
         * (#dynamorio::drmemtrace::scheduler_tmpl_t::DEPENDENCY_TIMESTAMPS) to
         * avoid one stream getting ahead of another.  For replaying a schedule
         * as it was traced with
         * #dynamorio::drmemtrace::scheduler_tmpl_t::MAP_TO_RECORDED_OUTPUT
         * this can indicate an idle period on a core where the traced workload was
         * not currently scheduled.
         */
        STATUS_WAIT,
        STATUS_INVALID,         /**< Error condition. */
        STATUS_REGION_INVALID,  /**< Input region is out of bounds. */
        STATUS_NOT_IMPLEMENTED, /**< Feature not implemented. */
        STATUS_SKIPPED,         /**< Used for internal scheduler purposes. */
        STATUS_RECORD_FAILED,   /**< Failed to record schedule for future replay. */
    };

    /** Identifies an input stream by its index. */
    typedef int input_ordinal_t;

    /** Identifies an output stream by its index. */
    typedef int output_ordinal_t;

    /** Sentinel value indicating that no input stream is specified. */
    static constexpr input_ordinal_t INVALID_INPUT_ORDINAL = -1;

    /** Sentinel value indicating that no input stream is specified. */
    static constexpr output_ordinal_t INVALID_OUTPUT_ORDINAL = -1;

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
        /** The ending point, inclusive.  A stop value of 0 means the end of the trace. */
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
            : regions_of_interest(regions)
        {
        }
        /** Size of the struct for binary-compatible additions. */
        size_t struct_size = sizeof(input_thread_info_t);
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
        std::set<output_ordinal_t> output_binding;
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
         * not before the first range).  These ranges must be non-overlapping and in
         * increasing order.
         */
        std::vector<range_t> regions_of_interest;
    };

    /** Specifies an input that is already opened by a reader. */
    struct input_reader_t {
        /** Creates an empty reader. */
        input_reader_t() = default;
        /** Creates a reader entry. */
        input_reader_t(std::unique_ptr<ReaderType> reader,
                       std::unique_ptr<ReaderType> end, memref_tid_t tid)
            : reader(std::move(reader))
            , end(std::move(end))
            , tid(tid)
        {
        }
        /** The reader for the input stream. */
        std::unique_ptr<ReaderType> reader;
        /** The end reader for 'reader'. */
        std::unique_ptr<ReaderType> end;
        /**
         * A unique identifier to distinguish from other readers for this workload.
         * Typically this will be the thread id but it does not need to be, so long
         * as it is not 0 (DynamoRIO's INVALID_THREAD_ID sentinel).
         * This is used to in the 'thread_modifiers' field of 'input_workload_t'
         * to refer to this input.
         */
        memref_tid_t tid = INVALID_THREAD_ID;
    };

    /** Specifies the input workloads to be scheduled. */
    struct input_workload_t {
        /** Create an empty workload.  This is not a valid final input. */
        input_workload_t()
        {
        }
        /**
         * Create a workload coming from a directory of many trace files or from
         * a single trace file where each trace file uses the given regions of interest.
         */
        input_workload_t(const std::string &trace_path,
                         std::vector<range_t> regions_of_interest = {})
            : path(trace_path)
        {
            if (!regions_of_interest.empty())
                thread_modifiers.push_back(input_thread_info_t(regions_of_interest));
        }
        /**
         * Create a workload with a set of pre-initialized readers which use the given
         * regions of interest.
         */
        input_workload_t(std::vector<input_reader_t> readers,
                         std::vector<range_t> regions_of_interest = {})
            : readers(std::move(readers))
        {
            if (!regions_of_interest.empty())
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
         * is to directly pass in a reader.  This field is only considered if 'path' is
         * empty.  The scheduler will call the init() function for each reader at the time
         * of the first access to it through an output stream (supporting IPC readers
         * whose init() blocks).
         */
        std::vector<input_reader_t> readers;

        // TODO i#5843: This is currently ignored for MAP_TO_RECORDED_OUTPUT +
        // DEPENDENCY_TIMESTAMPS b/c file_reader_t opens the individual files!
        // TODO i#5843: Add a test of this field.
        /**
         * If empty, every trace file in 'path' or every reader in 'readers' becomes
         * an enabled input.  If non-empty, only those inputs whose thread ids are
         * in this vector are enabled and the rest are ignored.
         */
        std::set<memref_tid_t> only_threads;

        /** Scheduling modifiers for the threads in this workload. */
        std::vector<input_thread_info_t> thread_modifiers;

        // Work around a known Visual Studio issue where it complains about deleted copy
        // constructors for unique_ptr by deleting our copies and defaulting our moves.
        input_workload_t(const input_workload_t &) = delete;
        input_workload_t &
        operator=(const input_workload_t &) = delete;
        input_workload_t(input_workload_t &&) = default;
        input_workload_t &
        operator=(input_workload_t &&) = default;
    };

    /** Controls how inputs are mapped to outputs. */
    enum mapping_t {
        // TODO i#5843: Can we get doxygen to link references without the fully-qualified
        // name?
        /**
         * Each input stream is mapped to a single output stream (i.e., no input will
         * appear on more than one output), supporting lock-free parallel analysis when
         * combined with #dynamorio::drmemtrace::scheduler_tmpl_t::DEPENDENCY_IGNORE.
         */
        MAP_TO_CONSISTENT_OUTPUT,
        // TODO i#5843: Currently it is up to the user to figure out the original core
        // count and pass that number in.  It would be convenient to have the scheduler
        // figure out the output count.  We'd need some way to return that count; I
        // imagine we could add a new query routine to get it and so not change the
        // existing interfaces.
        /**
         * Each input stream is assumed to contain markers indicating how it was
         * originally scheduled.  Those markers are honored with respect to which core
         * number (considered to correspond to output stream ordinal) an input is
         * scheduled into.  This requires an output stream count equal to the number of
         * cores occupied by the input stream set.  When combined with
         * #dynamorio::drmemtrace::scheduler_tmpl_t::DEPENDENCY_TIMESTAMPS, this will
         * precisely replay the recorded schedule; for this mode,
         * #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.replay_as_traced_istream
         * must be specified.
         */
        MAP_TO_RECORDED_OUTPUT,
        /**
         * The input streams are scheduling using a new synthetic schedule onto the
         * output streams.  Any input markers indicating how the software threads were
         * originally mapped to cores during tracing are ignored.
         */
        MAP_TO_ANY_OUTPUT,
        /**
         * A schedule recorded previously by this scheduler is to be replayed.
         * The input schedule data is specified in
         * #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.schedule_replay_istream.
         * The same output count and input stream order and count must be re-specified;
         * scheduling details such as regions of interest and core bindings do not
         * need to be re-specified and are in fact ignored.
         */
        MAP_AS_PREVIOUSLY,
    };

    /** Flags specifying how inter-input-stream dependencies are handled. */
    enum inter_input_dependency_t {
        /** Ignores all inter-input dependencies. */
        DEPENDENCY_IGNORE,
        /** Ensures timestamps in the inputs arrive at the outputs in timestamp order. */
        DEPENDENCY_TIMESTAMPS,
        // TODO i#5843: Add inferred data dependencies.
    };

    /**
     * Quantum units used for replacing one input with another pre-emptively.
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
        /**
         * Causes the get_record_ordinal() and get_instruction_ordinal() results
         * for an output stream to equal those values for the current input stream
         * for that output, rather than accumulating across inputs.
         */
        SCHEDULER_USE_INPUT_ORDINALS = 0x4,
        // This was added for the analyzer view tool on a single trace specified via
        // a directory where the analyzer isn't listing the dir so it doesn't know
        // whether to request SCHEDULER_USE_INPUT_ORDINALS.
        /**
         * If there is just one input and just one output stream, this sets
         * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULER_USE_INPUT_ORDINALS;
         * otherwise, it has no effect.
         */
        SCHEDULER_USE_SINGLE_INPUT_ORDINALS = 0x8,
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
        scheduler_options_t(mapping_t mapping, inter_input_dependency_t deps,
                            scheduler_flags_t flags = SCHEDULER_DEFAULTS,
                            int verbosity = 0)
            : mapping(mapping)
            , deps(deps)
            , flags(flags)
            , verbosity(verbosity)
        {
        }

        /** Size of the struct for binary-compatible additions. */
        size_t struct_size = sizeof(scheduler_options_t);
        /** The mapping of inputs to outputs. */
        mapping_t mapping = MAP_TO_ANY_OUTPUT;
        /** How inter-input dependencies are handled. */
        inter_input_dependency_t deps = DEPENDENCY_IGNORE;
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
         * If > 0, diagnostic messages are printed to stderr.  Higher values produce
         * more frequent diagnostics.
         */
        int verbosity = 0;
        /**
         * Output stream for recording the schedule for later replay.
         * write_recorded_schedule() must be called when finished to write the
         * in-memory data out to this stream.
         */
        archive_ostream_t *schedule_record_ostream = nullptr;
        /**
         * Input stream for replaying a previously recorded schedule when
         * #dynamorio::drmemtrace::scheduler_tmpl_t::MAP_AS_PREVIOUSLY is specified.  If
         * this is non-nullptr and MAP_AS_PREVIOUSLY is specified, schedule_record_ostream
         * must be nullptr, and most other fields in this struct controlling scheduling
         * are ignored.
         */
        archive_istream_t *schedule_replay_istream = nullptr;
        /**
         * Input stream for replaying the traced schedule when
         * #dynamorio::drmemtrace::scheduler_tmpl_t::MAP_TO_RECORDED_OUTPUT is specified
         * for more than one output stream (whose count must match the number of
         * traced cores).
         */
        archive_istream_t *replay_as_traced_istream = nullptr;
    };

    /**
     * Constructs options for a parallel no-inter-input-dependencies schedule where
     * the output stream's get_record_ordinal() and get_instruction_ordinal() reflect
     * just the current input rather than all past inputs on that output.
     */
    static scheduler_options_t
    make_scheduler_parallel_options(int verbosity = 0)
    {
        return scheduler_options_t(sched_type_t::MAP_TO_CONSISTENT_OUTPUT,
                                   sched_type_t::DEPENDENCY_IGNORE,
                                   sched_type_t::SCHEDULER_USE_INPUT_ORDINALS, verbosity);
    }
    /**
     * Constructs options for a serial as-recorded schedule where
     * the output stream's get_record_ordinal() and get_instruction_ordinal() honor
     * skipped records in the input if, and only if, there is a single input and
     * a single output.
     */
    static scheduler_options_t
    make_scheduler_serial_options(int verbosity = 0)
    {
        return scheduler_options_t(
            sched_type_t::MAP_TO_RECORDED_OUTPUT, sched_type_t::DEPENDENCY_TIMESTAMPS,
            sched_type_t::SCHEDULER_USE_SINGLE_INPUT_ORDINALS, verbosity);
    }

    /**
     * Represents a stream of RecordType trace records derived from a
     * subset of a set of input recorded traces.  Provides more
     * information about the record stream using the #memtrace_stream_t
     * API.
     */
    class stream_t : public memtrace_stream_t {
    public:
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

        // We deliberately use a regular function which can return a status for things
        // like STATUS_WAIT and abandon attempting to follow std::iterator here as ++;*
        // makes it harder to return multiple different statuses as first-class events.
        // We don’t plan to use range-based for loops or other language features for
        // iterators and our iteration is only forward, so std::iterator's value is
        // diminished.
        /**
         * Advances to the next record in the stream.  Returns a status code on whether
         * and how to continue.
         */
        virtual stream_status_t
        next_record(RecordType &record);

        /**
         * Reports the current time to the scheduler.  This is unitless: it just
         * needs to be called regularly and consistently.
         * This is used for
         * #dynamorio::drmemtrace::scheduler_tmpl_t::QUANTUM_TIME.
         */
        virtual stream_status_t
        report_time(uint64_t cur_time);

        /**
         * Begins a diversion from the regular inputs to a side stream of records
         * representing speculative execution starting at 'start_address'.
         *
         * Because the instruction record after a branch typically needs to be read
         * before knowing whether a simulator is on the wrong path or not, this routine
         * supports putting back the current record so that it will be re-provided as
         * the first record after stop_speculation(), if "queue_current_record" is true.
         * The "queue_current_record" parameter is ignored if speculation is already in
         * effect.
         *
         * This call can be "nested" but only one stop_speculation call is needed to
         * resume the paused stream.
         */
        virtual stream_status_t
        start_speculation(addr_t start_address, bool queue_current_record);

        /**
         * Stops speculative execution and resumes the regular stream of records from
         * the point at which the most distant prior start_speculation() call without an
         * intervening stop_speculation() call was made (either repeating the current
         * record at that time, if "true" was passed for "queue_current_record" to
         * start_speculation(), or continuing on the subsequent record if "false" was
         * passed).  Returns #dynamorio::drmemtrace::scheduler_tmpl_t::STATUS_INVALID if
         * there was no prior start_speculation() call or if stop_speculation() was
         * already called since the last start.
         */
        virtual stream_status_t
        stop_speculation();

        // memtrace_stream_t interface:

        /**
         * Returns the count of #memref_t records from the start of the trace to this
         * point. It does not include synthetic records (see is_record_synthetic()).
         *
         * If
         * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULER_USE_INPUT_ORDINALS
         * is set, then this value matches the record ordinal for the current input stream
         * (and thus might decrease or not change across records if the input changed).
         * Otherwise, if multiple input streams fed into this output stream, this
         * includes the records from all those streams that were presented here: thus,
         * this may be larger than what the current input stream reports (see
         * get_input_stream_interface() and get_input_stream_ordinal()).  This does not
         * advance across skipped records in an input stream from a region of interest
         * (see #dynamorio::drmemtrace::scheduler_tmpl_t::range_t), but it does advance if
         * the output stream skipped ahead.
         */
        uint64_t
        get_record_ordinal() const override
        {
            if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS,
                        scheduler_->options_.flags))
                return scheduler_->get_input_stream(ordinal_)->get_record_ordinal();
            return cur_ref_count_;
        }
        /**
         * Returns the count of instructions from the start of the trace to this point.
         * If
         * #dynamorio::drmemtrace::scheduler_tmpl_t::SCHEDULER_USE_INPUT_ORDINALS
         * is set, then this value matches the instruction ordinal for the current input
         * stream (and thus might decrease or not change across records if the input
         * changed). Otherwise, if multiple input streams fed into this output stream,
         * this includes the records from all those streams that were presented here:
         * thus, this may be larger than what the current input stream reports (see
         * get_input_stream_interface() and get_input_stream_ordinal()).  This does not
         * advance across skipped records in an input stream from a region of interest
         * (see #dynamorio::drmemtrace::scheduler_tmpl_t::range_t), but it does advance if
         * the output stream skipped ahead.
         */
        uint64_t
        get_instruction_ordinal() const override
        {
            if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS,
                        scheduler_->options_.flags))
                return scheduler_->get_input_stream(ordinal_)->get_instruction_ordinal();
            return cur_instr_count_;
        }
        /**
         * Returns a name for the current input stream feeding this output stream. For
         * stored offline traces, this is the base name of the trace on disk. For online
         * traces, this is the name of the pipe.
         */
        std::string
        get_stream_name() const override
        {
            return scheduler_->get_input_name(ordinal_);
        }
        /**
         * Returns the ordinal for the current input stream feeding this output stream.
         */
        virtual input_ordinal_t
        get_input_stream_ordinal()
        {
            return scheduler_->get_input_ordinal(ordinal_);
        }
        /**
         * Returns the value of the most recently seen #TRACE_MARKER_TYPE_TIMESTAMP
         * marker.
         */
        uint64_t
        get_last_timestamp() const override
        {
            if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS,
                        scheduler_->options_.flags))
                return scheduler_->get_input_stream(ordinal_)->get_last_timestamp();
            return last_timestamp_;
        }
        /**
         * Returns the value of the first seen #TRACE_MARKER_TYPE_TIMESTAMP marker.
         */
        uint64_t
        get_first_timestamp() const override
        {
            if (TESTANY(sched_type_t::SCHEDULER_USE_INPUT_ORDINALS,
                        scheduler_->options_.flags))
                return scheduler_->get_input_stream(ordinal_)->get_first_timestamp();
            return first_timestamp_;
        }
        /**
         * Returns the #trace_version_t value from the #TRACE_MARKER_TYPE_VERSION record
         * in the trace header.
         */
        uint64_t
        get_version() const override
        {
            return version_;
        }
        /**
         * Returns the OFFLINE_FILE_TYPE_* bitfields of type #offline_file_type_t
         * identifying the architecture and other key high-level attributes of the trace
         * from the #TRACE_MARKER_TYPE_FILETYPE record in the trace header.
         */
        uint64_t
        get_filetype() const override
        {
            return filetype_;
        }
        /**
         * Returns the cache line size from the #TRACE_MARKER_TYPE_CACHE_LINE_SIZE record
         * in the trace header.
         */
        uint64_t
        get_cache_line_size() const override
        {
            return cache_line_size_;
        }
        /**
         * Returns the chunk instruction count from the
         * #TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT record in the trace header.
         */
        uint64_t
        get_chunk_instr_count() const override
        {
            return chunk_instr_count_;
        }
        /**
         * Returns the page size from the #TRACE_MARKER_TYPE_PAGE_SIZE record in
         * the trace header.
         */
        uint64_t
        get_page_size() const override
        {
            return page_size_;
        }
        /**
         * Returns whether the current record was synthesized and inserted into the record
         * stream and was not present in the original stream.  This is true for timestamp
         * and cpuid headers duplicated after skipping ahead, as well as cpuid markers
         * inserted for synthetic schedules.  Such records do not cound toward the record
         * count and get_record_ordinal() will return the value of the prior record.
         */
        bool
        is_record_synthetic() const override
        {
            return scheduler_->is_record_synthetic(ordinal_);
        }

    protected:
        scheduler_tmpl_t<RecordType, ReaderType> *scheduler_ = nullptr;
        int ordinal_ = -1;
        int verbosity_ = 0;
        uint64_t cur_ref_count_ = 0;
        uint64_t cur_instr_count_ = 0;
        uint64_t last_timestamp_ = 0;
        uint64_t first_timestamp_ = 0;
        // Remember top-level headers for the memtrace_stream_t interface.
        uint64_t version_ = 0;
        uint64_t filetype_ = 0;
        uint64_t cache_line_size_ = 0;
        uint64_t chunk_instr_count_ = 0;
        uint64_t page_size_ = 0;

        // Let the outer class update our state.
        friend class scheduler_tmpl_t<RecordType, ReaderType>;
    };

    /** Default constructor. */
    scheduler_tmpl_t()
    {
    }
    virtual ~scheduler_tmpl_t() = default;

    /**
     * Initializes the scheduler for the given inputs, count of output streams, and
     * options.
     */
    virtual scheduler_status_t
    init(std::vector<input_workload_t> &workload_inputs, int output_count,
         scheduler_options_t options);

    /**
     * Returns the 'ordinal'-th output stream.
     */
    virtual stream_t *
    get_stream(output_ordinal_t ordinal)
    {
        if (ordinal < 0 || ordinal >= static_cast<output_ordinal_t>(outputs_.size()))
            return nullptr;
        return &outputs_[ordinal].stream;
    }

    /** Returns the number of input streams. */
    virtual int
    get_input_stream_count() const
    {
        return static_cast<input_ordinal_t>(inputs_.size());
    }

    /** Returns the #memtrace_stream_t interface for the 'ordinal'-th input stream. */
    virtual memtrace_stream_t *
    get_input_stream_interface(input_ordinal_t input) const
    {
        if (input < 0 || input >= static_cast<input_ordinal_t>(inputs_.size()))
            return nullptr;
        return inputs_[input].reader.get();
    }

    /**
     * Returns the name (from get_stream_name()) of the 'ordinal'-th input stream.
     */
    virtual std::string
    get_input_stream_name(input_ordinal_t input) const
    {
        if (input < 0 || input >= static_cast<input_ordinal_t>(inputs_.size()))
            return "";
        return inputs_[input].reader->get_stream_name();
    }

    /** Returns a string further describing an error code. */
    std::string
    get_error_string() const
    {
        return error_string_;
    }

    /**
     * Writes out the recorded schedule.  Requires that
     * #dynamorio::drmemtrace::scheduler_tmpl_t::
     * scheduler_options_t::schedule_record_ostream was non-nullptr
     * at init time.
     */
    scheduler_status_t
    write_recorded_schedule();

protected:
    typedef scheduler_tmpl_t<RecordType, ReaderType> sched_type_t;
    typedef speculator_tmpl_t<RecordType> spec_type_t;

    struct input_info_t {
        input_info_t()
            : lock(new std::mutex)
        {
        }
        int index = -1; // Position in inputs_ vector.
        std::unique_ptr<ReaderType> reader;
        std::unique_ptr<ReaderType> reader_end;
        // While the scheduler only hands an input to one output at a time, during
        // scheduling decisions one thread may need to access another's fields.
        // We use a unique_ptr to make this moveable for vector storage.
        std::unique_ptr<std::mutex> lock;
        // A tid can be duplicated across workloads so we need the pair of
        // workload index + tid to identify the original input.
        int workload = -1;
        memref_tid_t tid = INVALID_THREAD_ID;
        // If non-empty these records should be returned before incrementing the reader.
        // This is used for read-ahead and inserting synthetic records.
        // We use a deque so we can iterate over it.
        std::deque<RecordType> queue;
        std::set<output_ordinal_t> binding;
        int priority = 0;
        std::vector<range_t> regions_of_interest;
        // Index into regions_of_interest.
        int cur_region = 0;
        // Whether we have reached the current region proper (or are still on the
        // preceding inserted timestamp+cpuid).
        bool in_cur_region = false;
        bool has_modifier = false;
        bool needs_init = false;
        bool needs_advance = false;
        bool needs_roi = true;
        bool at_eof = false;
        uintptr_t next_timestamp = 0;
        uint64_t instrs_in_quantum = 0;
        bool recorded_in_schedule = false;
    };

    // Format for recording a schedule to disk.  A separate sequence of these records
    // is stored per output stream; each output stream's sequence is in one component
    // (subfile) of an archive file.
    START_PACKED_STRUCTURE
    struct schedule_record_t {
        enum record_type_t {
            // A regular entry denoting one thread sequence between context switches.
            DEFAULT,
            // The first entry in each component must be this type.  The "key" field
            // holds a version number.
            VERSION,
            FOOTER,        // The final entry in the component.  Other fields are ignored.
            SKIP,          // Skip ahead to the next region of interest.
            SYNTHETIC_END, // A synthetic thread exit record must be supplied.
        };
        static constexpr int VERSION_CURRENT = 0;
        schedule_record_t() = default;
        schedule_record_t(record_type_t type, input_ordinal_t input, uint64_t start,
                          uint64_t stop, uint64_t time)
            : type(type)
            , key(input)
            , start_instruction(start)
            , stop_instruction(stop)
            , timestamp(time)
        {
        }
        record_type_t type;
        START_PACKED_STRUCTURE
        union key {
            key() = default;
            key(input_ordinal_t input)
                : input(input)
            {
            }
            // We assume the user will repeat the precise input workload specifications
            // (including directory ordering of thread files) and we can simply store
            // the ordinal and rely on the same ordinal on replay being the same input.
            input_ordinal_t input = -1;
            int version; // For record_type_t::VERSION.
        } END_PACKED_STRUCTURE key;
        // Input stream ordinal of starting point.
        uint64_t start_instruction = 0;
        // Input stream ordinal, inclusive.  Max numeric value means continue until EOF.
        uint64_t stop_instruction = 0;
        // Timestamp in microseconds to keep context switches ordered.
        // XXX: To add more fine-grained ordering we could emit multiple entries
        // per thread segment, and update the context switching code to recognize
        // that a new entry does not always mean a context switch.
        uint64_t timestamp = 0;
    } END_PACKED_STRUCTURE;

    struct output_info_t {
        output_info_t(scheduler_tmpl_t<RecordType, ReaderType> *scheduler,
                      output_ordinal_t ordinal,
                      typename spec_type_t::speculator_flags_t speculator_flags,
                      int verbosity = 0)
            : stream(scheduler, ordinal, verbosity)
            , speculator(speculator_flags, verbosity)
        {
        }
        stream_t stream;
        // This is an index into the inputs_ vector so -1 is an invalid value.
        // This is set to >=0 for all non-empty outputs during init().
        input_ordinal_t cur_input = INVALID_INPUT_ORDINAL;
        // For static schedules we can populate this up front and avoid needing a
        // lock for dynamically finding the next input, keeping things parallel.
        std::vector<input_ordinal_t> input_indices;
        int input_indices_index = 0;
        // Speculation support.
        bool speculating = false;
        speculator_tmpl_t<RecordType> speculator;
        addr_t speculate_pc = 0;
        RecordType last_record;
        // A list of schedule segments.  These are accessed only while holding
        // sched_lock_.
        std::vector<schedule_record_t> record;
        int record_index = 0;
        bool waiting = false;
    };
    scheduler_status_t
    get_initial_timestamps();

    // Opens up all the readers for each file in 'path' which may be a directory.
    // Returns a map of the thread id of each file to its index in inputs_.
    scheduler_status_t
    open_readers(const std::string &path, const std::set<memref_tid_t> &only_threads,
                 std::unordered_map<memref_tid_t, input_ordinal_t> &workload_tids);

    // Opens up a single reader for the (non-directory) file in 'path'.
    // Returns a map of the thread id of the file to its index in inputs_.
    scheduler_status_t
    open_reader(const std::string &path, const std::set<memref_tid_t> &only_threads,
                std::unordered_map<memref_tid_t, input_ordinal_t> &workload_tids);

    // Creates a reader for the default file type we support.
    std::unique_ptr<ReaderType>
    get_default_reader();

    // Creates a reader for the specific file type at (non-directory) 'path'.
    std::unique_ptr<ReaderType>
    get_reader(const std::string &path, int verbosity);

    // Advances the 'output_ordinal'-th output stream.
    stream_status_t
    next_record(output_ordinal_t output, RecordType &record, input_info_t *&input);

    // Skips ahead to the next region of interest if necessary.
    // The caller must hold the input.lock.
    stream_status_t
    advance_region_of_interest(output_ordinal_t output, RecordType &record,
                               input_info_t &input);

    // Does a direct skip, unconditionally.
    // The caller must hold the input.lock.
    stream_status_t
    skip_instructions(output_ordinal_t output, input_info_t &input, uint64_t skip_amount);

    scheduler_status_t
    read_traced_schedule();

    scheduler_status_t
    read_recorded_schedule();

    // The caller must hold the lock for the input.
    stream_status_t
    record_schedule_segment(
        output_ordinal_t output, typename schedule_record_t::record_type_t type,
        // "input" can instead be a version of type int.
        // As they are the same underlying type we cannot overload.
        input_ordinal_t input, uint64_t start_instruction,
        // Wrap max in parens to work around Visual Studio compiler issues with the
        // max macro (even despite NOMINMAX defined above).
        uint64_t stop_instruction = (std::numeric_limits<uint64_t>::max)());

    // The caller must hold the input.lock.
    stream_status_t
    close_schedule_segment(output_ordinal_t output, input_info_t &input);

    std::string
    recorded_schedule_component_name(output_ordinal_t output);

    // The sched_lock_ must be held when this is called.
    stream_status_t
    set_cur_input(output_ordinal_t output, input_ordinal_t input);

    // Finds the next input stream for the 'output_ordinal'-th output stream.
    // No input_info_t lock can be held on entry.
    stream_status_t
    pick_next_input(output_ordinal_t output);

    // If the given record has a thread id field, returns true and the value.
    bool
    record_type_has_tid(RecordType record, memref_tid_t &tid);

    // Returns whether the given record is an instruction.
    bool
    record_type_is_instr(RecordType record);

    // If the given record is a marker, returns true and its fields.
    bool
    record_type_is_marker(RecordType record, trace_marker_type_t &type, uintptr_t &value);

    // If the given record is a timestamp, returns true and its fields.
    bool
    record_type_is_timestamp(RecordType record, uintptr_t &value);

    // Creates the marker we insert between regions of interest.
    RecordType
    create_region_separator_marker(memref_tid_t tid, uintptr_t value);

    // Creates a thread exit record.
    RecordType
    create_thread_exit(memref_tid_t tid);

    // Used for diagnostics: prints record fields to stderr.
    void
    print_record(const RecordType &record);

    // Returns the get_stream_name() value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    std::string
    get_input_name(output_ordinal_t output);

    // Returns the input ordinal value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    input_ordinal_t
    get_input_ordinal(output_ordinal_t output);

    // Returns whether the current record for the current input stream scheduled on
    // the 'output_ordinal'-th output stream is synthetic.
    bool
    is_record_synthetic(output_ordinal_t output);

    // Returns the direct handle to the current input stream interface for the
    // 'output_ordinal'-th output stream.
    memtrace_stream_t *
    get_input_stream(output_ordinal_t output);

    stream_status_t
    start_speculation(output_ordinal_t output, addr_t start_address,
                      bool queue_current_record);

    stream_status_t
    stop_speculation(output_ordinal_t output);

    // This has the same value as scheduler_options_t.verbosity (for use in VPRINT).
    int verbosity_ = 0;
    const char *output_prefix_ = "[scheduler]";
    std::string error_string_;
    scheduler_options_t options_;
    // Each vector element has a mutex which should be held when accessing its fields.
    std::vector<input_info_t> inputs_;
    // Each vector element is accessed only by its owning thread, except the
    // record and record_index fields which are accessed under sched_lock_.
    std::vector<output_info_t> outputs_;
    // We use a central lock for global scheduling.  We assume the synchronization
    // cost is outweighed by the simulator's overhead.  This protects concurrent
    // access to inputs_.size(), outputs_.size(), and the ready_ field.
    std::mutex sched_lock_;
    // Input indices ready to be scheduled.
    std::queue<input_ordinal_t> ready_;
};

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<memref_t, reader_t> scheduler_t;

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_scheduler_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SCHEDULER_H_ */
