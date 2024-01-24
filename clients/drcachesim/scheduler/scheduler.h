/* **********************************************************
 * Copyright (c) 2023-2024 Google, Inc.  All rights reserved.
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
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "archive_istream.h"
#include "archive_ostream.h"
#include "flexible_queue.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "reader.h"
#include "record_file_reader.h"
#include "speculator.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {  /**< General DynamoRIO namespace. */
namespace drmemtrace { /**< DrMemtrace tracing + simulation infrastructure namespace. */

/**
 * Schedules traced software threads onto simulated cpus.
 * Takes in a set of recorded traces and maps them onto a new set of output
 * streams, typically representing simulated cpus.
 *
 * This is a templated class to support not just operating over
 * #dynamorio::drmemtrace::memref_t inputs read by #dynamorio::drmemtrace::reader_t, but
 * also over #dynamorio::drmemtrace::trace_entry_t records read by
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
         * (#DEPENDENCY_TIMESTAMPS) to avoid one stream getting ahead of another.
         * #STATUS_WAIT should be treated as artificial, an artifact of enforcing a
         * recorded schedule on concurrent differently-timed output streams.
         * Simulators are suggested to not advance simulated time for #STATUS_WAIT while
         * they should advance time for #STATUS_IDLE as the latter indicates a true
         * lack of work.
         */
        STATUS_WAIT,
        STATUS_INVALID,         /**< Error condition. */
        STATUS_REGION_INVALID,  /**< Input region is out of bounds. */
        STATUS_NOT_IMPLEMENTED, /**< Feature not implemented. */
        STATUS_SKIPPED,         /**< Used for internal scheduler purposes. */
        STATUS_RECORD_FAILED,   /**< Failed to record schedule for future replay. */
        /**
         * This code indicates that all inputs are blocked waiting for kernel resources
         * (such as i/o).  This is similar to #STATUS_WAIT, but #STATUS_WAIT indicates an
         * artificial pause due to imposing the original ordering while #STATUS_IDLE
         * indicates actual idle time in the application.  Simulators are suggested
         * to not advance simulated time for #STATUS_WAIT while they should advance
         * time for #STATUS_IDLE.
         */
        STATUS_IDLE,
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
        /** Convenience constructor for common usage. */
        input_thread_info_t(memref_tid_t tid, int priority)
            : tids(1, tid)
            , priority(priority)
        {
        }
        /**
         * Convenience constructor for placing all threads for one workload on a set of
         * cores for a static partitioning.
         */
        input_thread_info_t(std::set<output_ordinal_t> output_binding)
            : output_binding(output_binding)
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
         * Relative priority for scheduling.  The default is 0.  Higher values have
         * higher priorities and will starve lower-priority inputs.
         * Higher priorities out-weigh dependencies such as #DEPENDENCY_TIMESTAMPS.
         */
        int priority = 0;
        /**
         * If non-empty, all input records outside of these ranges are skipped: it is as
         * though the input were constructed by concatenating these ranges together.  A
         * #TRACE_MARKER_TYPE_WINDOW_ID marker is inserted between
         * ranges (with a value equal to the range ordinal) to notify the client of the
         * discontinuity (but not before the first range nor between back-to-back regions
         * with no separation), with a #dynamorio::drmemtrace::TRACE_TYPE_THREAD_EXIT
         * record inserted after the final range.  These ranges must be non-overlapping
         * and in increasing order.
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
         * This allows the 'thread_modifiers' field of 'input_workload_t'
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
                thread_modifiers.emplace_back(regions_of_interest);
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
                thread_modifiers.emplace_back(regions_of_interest);
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
         * combined with #DEPENDENCY_IGNORE.
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
         * #DEPENDENCY_TIMESTAMPS, this will
         * precisely replay the recorded schedule; for this mode,
         * #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.replay_as_traced_istream
         * must be specified.
         * The original as-traced cpuid that is mapped to each output stream can be
         * obtained by calling the get_output_cpuid() function on each stream.
         *
         * An alternative use of this mapping is with a single output to interleave
         * inputs in a strict timestamp order, as with make_scheduler_serial_options(),
         * without specifying a schedule file and without recreating core mappings:
         * only timestamps are honored.
         */
        MAP_TO_RECORDED_OUTPUT,
        /**
         * The input streams are scheduled using a new dynamic sequence onto the
         * output streams.  Any input markers indicating how the software threads were
         * originally mapped to cores during tracing are ignored.  Instead, inputs
         * run until either a quantum expires (see
         * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t::quantum_unit)
         * or a (potentially) blocking system call is identified.  At this point,
         * a new input is selected, taking into consideration other options such
         * as priorities, core bindings, and inter-input dependencies.
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

    /**
     * Flags specifying how inter-input-stream dependencies are handled.  The _BITFIELD
     * values can be combined.  Typical combinations are provided so the enum type can be
     * used directly.
     */
    enum inter_input_dependency_t {
        /** Ignores all inter-input dependencies. */
        DEPENDENCY_IGNORE = 0x00,
        /**
         * Ensures timestamps in the inputs arrive at the outputs in timestamp order.
         * For #MAP_TO_ANY_OUTPUT, enforcing asked-for context switch rates is more
         * important that honoring precise trace-buffer-based timestamp inter-input
         * dependencies: thus, timestamp ordering will be followed at context switch
         * points for picking the next input, but timestamps will not preempt an input.
         * To precisely follow the recorded timestamps, use #MAP_TO_RECORDED_OUTPUT.
         */
        DEPENDENCY_TIMESTAMPS_BITFIELD = 0x01,
        /**
         * Ensures timestamps in the inputs arrive at the outputs in timestamp order.
         * For #MAP_TO_ANY_OUTPUT, enforcing asked-for context switch rates is more
         * important that honoring precise trace-buffer-based timestamp inter-input
         * dependencies: thus, timestamp ordering will be followed at context switch
         * points for picking the next input, but timestamps will not preempt an input.
         * To precisely follow the recorded timestamps, use #MAP_TO_RECORDED_OUTPUT.
         */
        DEPENDENCY_DIRECT_SWITCH_BITFIELD = 0x02,
        /**
         * Combines #DEPENDENCY_TIMESTAMPS_BITFIELD and
         * #DEPENDENCY_DIRECT_SWITCH_BITFIELD.  This is the recommended setting for most
         * schedules.
         */
        DEPENDENCY_TIMESTAMPS =
            (DEPENDENCY_TIMESTAMPS_BITFIELD | DEPENDENCY_DIRECT_SWITCH_BITFIELD),
        // TODO i#5843: Add inferred data dependencies.
    };

    /**
     * Quantum units used for replacing one input with another pre-emptively.
     */
    enum quantum_unit_t {
        /** Uses the instruction count as the quantum. */
        QUANTUM_INSTRUCTIONS,
        /**
         * Uses the user's notion of time as the quantum.  This must be supplied by the
         * user by calling the next_record() variant that takes in the current time.
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
         * This also changes the behavior of get_shard_index() as documented under that
         * function.
         */
        SCHEDULER_USE_INPUT_ORDINALS = 0x4,
        // This was added for the analyzer view tool on a single trace specified via
        // a directory where the analyzer isn't listing the dir so it doesn't know
        // whether to request SCHEDULER_USE_INPUT_ORDINALS.
        /**
         * If there is just one input and just one output stream, this sets
         * #SCHEDULER_USE_INPUT_ORDINALS.  In all cases, this changes the behavior
         * of get_shard_index() as documented under that function.
         */
        SCHEDULER_USE_SINGLE_INPUT_ORDINALS = 0x8,
        // TODO i#5843: Add more speculation flags for other strategies.
    };

    /**
     * Types of context switches for
     * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t::
     * kernel_switch_trace_path and kernel_switch_reader.
     * The enum value is the subfile component name in the archive_istream_t.
     */
    enum switch_type_t {
        /** Invalid value. */
        SWITCH_INVALID = 0,
        /** Generic thread context switch. */
        SWITCH_THREAD,
        /**
         * Generic process context switch.  A workload is considered a process.
         */
        SWITCH_PROCESS,
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
         * #MAP_AS_PREVIOUSLY is specified.  If this is non-nullptr and
         * #MAP_AS_PREVIOUSLY is specified, schedule_record_ostream must be nullptr, and
         * most other fields in this struct controlling scheduling are ignored.
         */
        archive_istream_t *schedule_replay_istream = nullptr;
        /**
         * Input stream for replaying the traced schedule when #MAP_TO_RECORDED_OUTPUT is
         * specified for more than one output stream (whose count must match the number
         * of traced cores).
         */
        archive_istream_t *replay_as_traced_istream = nullptr;
        /**
         * Determines the minimum latency in the unit of the trace's timestamps
         * (microseconds) for which a non-maybe-blocking system call (one without
         * a #TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL marker) will be treated as
         * blocking and trigger a context switch.
         */
        uint64_t syscall_switch_threshold = 500;
        /**
         * Determines the minimum latency in the unit of the trace's timestamps
         * (microseconds) for which a maybe-blocking system call (one with
         * a #TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL marker) will be treated as
         * blocking and trigger a context switch.
         */
        uint64_t blocking_switch_threshold = 100;
        /**
         * Controls the amount of time inputs are considered blocked at a syscall whose
         * latency exceeds #syscall_switch_threshold or #blocking_switch_threshold.  The
         * syscall latency (in microseconds) is multiplied by this field to produce the
         * blocked time.  For #QUANTUM_TIME, that blocked time in the units reported by
         * the time parameter to next_record() must pass before the input is no longer
         * considered blocked.  Since the system call latencies are in microseconds, this
         * #block_time_scale should be set to the number of next_record() time units in
         * one simulated microsecond.  For #QUANTUM_INSTRUCTIONS, the blocked time in
         * wall-clock microseconds must pass before the input is actually selected
         * (wall-clock time is used as there is no reasonable alternative with no other
         * uniform notion of time); thus, the #block_time_scale value here should equal
         * the slowdown of the instruction record processing versus the original
         * (untraced) application execution.  The blocked time is clamped to a maximum
         * value controlled by #block_time_max.
         */
        double block_time_scale = 1000.;
        /**
         * The maximum time, in microseconds, for an input to be considered blocked
         * for any one system call.  This is applied after multiplying by
         * #block_time_scale.
         */
        uint64_t block_time_max = 25000000;
        // XXX: Should we share the file-to-reader code currently in the scheduler
        // with the analyzer and only then need reader interfaces and not pass paths
        // to the scheduler?
        /**
         * Input file containing template sequences of kernel context switch code.
         * Each sequence must start with a #TRACE_MARKER_TYPE_CONTEXT_SWITCH_START
         * marker and end with #TRACE_MARKER_TYPE_CONTEXT_SWITCH_END.
         * The values of each marker must hold a #switch_type_t enum value
         * indicating which type of switch it corresponds to.
         * Each sequence can be stored as a separate subfile of an archive file,
         * or concatenated into a single file.
         * Each sequence should be in the regular offline drmemtrace format.
         * The sequence is inserted into the output stream on each context switch
         * of the indicated type.
         * The same file (or reader) must be passed when replaying as this kernel
         * code is not stored when recording.
         * An alternative to passing the file path is to pass #kernel_switch_reader
         * and #kernel_switch_reader_end.
         */
        std::string kernel_switch_trace_path;
        /**
         * An alternative to #kernel_switch_trace_path is to pass a reader and
         * #kernel_switch_reader_end.  See the description of #kernel_switch_trace_path.
         * This field is only examined if #kernel_switch_trace_path is empty.
         * The scheduler will call the init() function for the reader.
         */
        std::unique_ptr<ReaderType> kernel_switch_reader;
        /** The end reader for #kernel_switch_reader. */
        std::unique_ptr<ReaderType> kernel_switch_reader_end;
        /**
         * If true, enables a mode where all outputs are serialized into one global outer
         * layer output.  The single global output stream alternates in round-robin
         * lockstep among each core output.  The core outputs operate just like they
         * would with no serialization, other than timing differences relative to other
         * core outputs.
         */
        bool single_lockstep_output = false;
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
     * information about the record stream using the
     * #dynamorio::drmemtrace::memtrace_stream_t API.
     */
    class stream_t : public memtrace_stream_t {
    public:
        stream_t(scheduler_tmpl_t<RecordType, ReaderType> *scheduler, int ordinal,
                 int verbosity = 0, int max_ordinal = -1)
            : scheduler_(scheduler)
            , ordinal_(ordinal)
            , max_ordinal_(max_ordinal)
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
         * Advances to the next record in the stream.  Returns a status code on whether
         * and how to continue.  Supplies the current time for #QUANTUM_TIME.  The time
         * should be considered to be the time prior to processing the returned record.
         * The time is unitless but needs to be a globally consistent increasing value
         * across all output streams.  #STATUS_INVALID is returned if 0 or a value smaller
         * than the start time of the current input's quantum is passed in when
         * #QUANTUM_TIME and #MAP_TO_ANY_OUTPUT are specified.
         */
        virtual stream_status_t
        next_record(RecordType &record, uint64_t cur_time);

        /**
         * Queues the last-read record returned by next_record() such that it will be
         * returned on the subsequent call to next_record() when this same input is
         * active.  Causes ordinal queries on the current input to be off by one until the
         * record is re-read.  Furthermore, the get_last_timestamp() query may still
         * include this record, whether called on the input or output stream, immediately
         * after this call.  Fails if called multiple times in a row without an
         * intervening next_record() call.  Fails if called during speculation (between
         * start_speculation() and stop_speculation() calls).
         */
        virtual stream_status_t
        unread_last_record();

        /**
         * Begins a diversion from the regular inputs to a side stream of records
         * representing speculative execution starting at 'start_address'.
         *
         * Because the instruction record after a branch typically needs to be read before
         * knowing whether a simulator is on the wrong path or not, this routine supports
         * putting back the current record so that it will be re-provided as the first
         * record after stop_speculation(), if "queue_current_record" is true.  The same
         * caveats on the input stream ordinals and last timestamp described under
         * unread_last_record() apply to this record queueing.  Calling
         * start_speculation() immediately after unread_last_record() and requesting
         * queueing will return a failure code.
         *
         * This call can be nested; each call needs to be paired with a corresponding
         * stop_speculation() call.
         */
        virtual stream_status_t
        start_speculation(addr_t start_address, bool queue_current_record);

        /**
         * Stops speculative execution, resuming execution at the
         * stream of records from the point at which the prior matching
         * start_speculation() call was made, either repeating the current record at that
         * time (if "true" was passed for "queue_current_record" to start_speculation())
         * or continuing on the subsequent record (if "false" was passed).  Returns
         * #STATUS_INVALID if there was no prior start_speculation() call.
         */
        virtual stream_status_t
        stop_speculation();

        /**
         * Disables or re-enables this output stream.  If "active" is false, this
         * stream becomes inactive and its currently assigned input is moved to the
         * ready queue to be scheduled on other outputs.  The #STATUS_IDLE code is
         * returned to next_record() for inactive streams.  If "active" is true,
         * this stream becomes active again.
         * This is only supported for #MAP_TO_ANY_OUTPUT.
         */
        virtual stream_status_t
        set_active(bool active);

        // memtrace_stream_t interface:

        /**
         * Returns the count of #memref_t records from the start of
         * the trace to this point. It does not include synthetic records (see
         * is_record_synthetic()).
         *
         * If #SCHEDULER_USE_INPUT_ORDINALS is set, then this value matches the record
         * ordinal for the current input stream (and thus might decrease or not change
         * across records if the input changed).  Otherwise, if multiple input streams
         * fed into this output stream, this includes the records from all those streams
         * that were presented here: thus, this may be larger than what the current input
         * stream reports (see get_input_stream_interface() and
         * get_input_stream_ordinal()).  This does not advance across skipped records in
         * an input stream from a region of interest (see
         * #dynamorio::drmemtrace::scheduler_tmpl_t::range_t), but it does advance if the
         * output stream skipped ahead.
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
         * If #SCHEDULER_USE_INPUT_ORDINALS is set, then this value matches the
         * instruction ordinal for the current input stream (and thus might decrease or
         * not change across records if the input changed). Otherwise, if multiple input
         * streams fed into this output stream, this includes the records from all those
         * streams that were presented here: thus, this may be larger than what the
         * current input stream reports (see get_input_stream_interface() and
         * get_input_stream_ordinal()).  This does not advance across skipped records in
         * an input stream from a region of interest (see
         * #dynamorio::drmemtrace::scheduler_tmpl_t::range_t), but it does advance if the
         * output stream skipped ahead.
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
        get_input_stream_ordinal() const
        {
            return scheduler_->get_input_ordinal(ordinal_);
        }
        /**
         * Returns the ordinal for the workload which is the source of the current input
         * stream feeding this output stream.  This workload ordinal is the index into the
         * vector of type #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t
         * passed to init().  Returns -1 if there is no current input for this output
         * stream.
         */
        virtual int
        get_input_workload_ordinal() const
        {
            return scheduler_->get_workload_ordinal(ordinal_);
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
         * Returns the #trace_version_t value from the
         * #TRACE_MARKER_TYPE_VERSION record in the trace header.
         */
        uint64_t
        get_version() const override
        {
            return version_;
        }
        /**
         * Returns the OFFLINE_FILE_TYPE_* bitfields of type
         * #offline_file_type_t identifying the architecture and
         * other key high-level attributes of the trace from the
         * #TRACE_MARKER_TYPE_FILETYPE record in the trace header.
         */
        uint64_t
        get_filetype() const override
        {
            return filetype_;
        }
        /**
         * Returns the cache line size from the
         * #TRACE_MARKER_TYPE_CACHE_LINE_SIZE record in the trace
         * header.
         */
        uint64_t
        get_cache_line_size() const override
        {
            return cache_line_size_;
        }
        /**
         * Returns the chunk instruction count from the
         * #TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT record in the trace
         * header.
         */
        uint64_t
        get_chunk_instr_count() const override
        {
            return chunk_instr_count_;
        }
        /**
         * Returns the page size from the
         * #TRACE_MARKER_TYPE_PAGE_SIZE record in the trace header.
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

        /**
         * Returns a unique identifier for the current output stream.  For
         * #MAP_TO_RECORDED_OUTPUT, the identifier is the as-traced cpuid mapped to this
         * output.  For dynamic schedules, the identifier is the output stream ordinal.
         */
        int64_t
        get_output_cpuid() const override
        {
            return scheduler_->get_output_cpuid(ordinal_);
        }

        /**
         * Returns the ordinal for the current
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.
         */
        int64_t
        get_workload_id() const override
        {
            return static_cast<int64_t>(get_input_workload_ordinal());
        }

        /**
         * Returns the ordinal for the current input stream feeding this output stream.
         */
        int64_t
        get_input_id() const override
        {
            return static_cast<int64_t>(get_input_stream_ordinal());
        }

        /**
         * Returns the thread identifier for the current input stream feeding this
         * output stream.
         */
        int64_t
        get_tid() const override
        {
            return scheduler_->get_tid(ordinal_);
        }

        /**
         * Returns the #dynamorio::drmemtrace::memtrace_stream_t interface for the
         * current input stream feeding this output stream.
         */
        memtrace_stream_t *
        get_input_interface() const override
        {
            return scheduler_->get_input_stream_interface(get_input_stream_ordinal());
        }

        /**
         * Returns the ordinal for the current output stream. If
         * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t::
         * single_lockstep_output
         * is set to true, this returns the ordinal of the currently active "inner"
         * output stream.  Otherwise, this returns the constant ordinal for this output
         * stream as there is no concept of inner or outer streams.
         */
        output_ordinal_t
        get_output_stream_ordinal() const
        {
            return ordinal_;
        }

        /**
         * For #SCHEDULER_USE_INPUT_ORDINALS or
         * #SCHEDULER_USE_SINGLE_INPUT_ORDINALS, returns the input stream ordinal, except
         * for the case of a single combined-stream input with the passed-in thread id
         * set to INVALID_THREAD_ID (the serial analysis mode for analyzer tools) in
         * which case the last trace record's tid is returned; otherwise returns the
         * output stream ordinal.
         */
        int
        get_shard_index() const override
        {
            return scheduler_->get_shard_index(ordinal_);
        }

        /**
         * Returns whether the current record is from a part of the trace corresponding
         * to kernel execution.
         */
        bool
        is_record_kernel() const override
        {
            return scheduler_->is_record_kernel(ordinal_);
        }

    protected:
        scheduler_tmpl_t<RecordType, ReaderType> *scheduler_ = nullptr;
        int ordinal_ = -1;
        // If max_ordinal_ >= 0, ordinal_ is incremented modulo max_ordinal_ at the start
        // of every next_record() invocation.
        int max_ordinal_ = -1;
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
        return outputs_[ordinal].stream;
    }

    /** Returns the number of input streams. */
    virtual int
    get_input_stream_count() const
    {
        return static_cast<input_ordinal_t>(inputs_.size());
    }

    /**
     * Returns the #dynamorio::drmemtrace::memtrace_stream_t interface for the
     * 'ordinal'-th input stream.
     */
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

    /**
     * Returns the get_output_cpuid() value for the given output.
     * This interface is exported so that a user can get the cpuids at initialization
     * time when using single_lockstep_output where there is just one output stream
     * even with multiple output cpus.
     */
    int64_t
    get_output_cpuid(output_ordinal_t output) const;

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
        bool
        is_combined_stream()
        {
            // If the tid is invalid, this is a combined stream (online analysis mode).
            return tid == INVALID_THREAD_ID;
        }
        int index = -1; // Position in inputs_ vector.
        std::unique_ptr<ReaderType> reader;
        std::unique_ptr<ReaderType> reader_end;
        // While the scheduler only hands an input to one output at a time, during
        // scheduling decisions one thread may need to access another's fields.
        // We use a unique_ptr to make this moveable for vector storage.
        // For inputs not actively assigned to a core but sitting in the ready_queue,
        // sched_lock_ suffices to synchronize access.
        std::unique_ptr<std::mutex> lock;
        // A tid can be duplicated across workloads so we need the pair of
        // workload index + tid to identify the original input.
        int workload = -1;
        // If left invalid, this is a combined stream (online analysis mode).
        memref_tid_t tid = INVALID_THREAD_ID;
        memref_tid_t last_record_tid = INVALID_THREAD_ID;
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
        // This is a per-workload value, stored in each input for convenience.
        uint64_t base_timestamp = 0;
        // This equals 'options_.deps == DEPENDENCY_TIMESTAMPS', stored here for
        // access in InputTimestampComparator which is static and has no access
        // to the schedule_t.  (An alternative would be to try to get a lambda
        // with schedule_t "this" access for the comparator to compile: it is not
        // simple to do so, however.)
        bool order_by_timestamp = false;
        // Global ready queue counter used to provide FIFO for same-priority inputs.
        uint64_t queue_counter = 0;
        // Used to switch on the instruction *after* a long-latency syscall.
        bool processing_syscall = false;
        bool processing_maybe_blocking_syscall = false;
        uint64_t pre_syscall_timestamp = 0;
        // Use for special kernel features where one thread specifies a target
        // thread to replace it.
        input_ordinal_t switch_to_input = INVALID_INPUT_ORDINAL;
        // Used to switch before we've read the next instruction.
        bool switching_pre_instruction = false;
        // Used for time-based quanta.
        uint64_t prev_time_in_quantum = 0;
        uint64_t time_spent_in_quantum = 0;
        // These fields model waiting at a blocking syscall.
        // The units are us for instr quanta and simuilation time for time quanta.
        uint64_t blocked_time = 0;
        uint64_t blocked_start_time = 0;
    };

    // Format for recording a schedule to disk.  A separate sequence of these records
    // is stored per output stream; each output stream's sequence is in one component
    // (subfile) of an archive file.
    // All fields are little-endian.
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
            // Indicates that the output is idle.  The value.idle_duration field holds
            // a duration in microseconds.
            IDLE,
        };
        static constexpr int VERSION_CURRENT = 0;
        schedule_record_t() = default;
        schedule_record_t(record_type_t type, input_ordinal_t input, uint64_t start,
                          uint64_t stop, uint64_t time)
            : type(type)
            , key(input)
            , value(start)
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
        START_PACKED_STRUCTURE
        union value {
            value() = default;
            value(uint64_t start)
                : start_instruction(start)
            {
            }
            // For record_type_t::IDLE, the duration in microseconds of the idling.
            uint64_t idle_duration;
            // Input stream ordinal of starting point, for non-IDLE types.
            uint64_t start_instruction = 0;
        } END_PACKED_STRUCTURE value;
        // Input stream ordinal, exclusive.  Max numeric value means continue until EOF.
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
                      RecordType last_record_init, int verbosity = 0)
            : self_stream(scheduler, ordinal, verbosity)
            , stream(&self_stream)
            , speculator(speculator_flags, verbosity)
            , last_record(last_record_init)
        {
        }
        stream_t self_stream;
        // Normally stream points to &self_stream, but for single_lockstep_output
        // it points to a global stream shared among all outputs.
        stream_t *stream;
        // This is an index into the inputs_ vector so -1 is an invalid value.
        // This is set to >=0 for all non-empty outputs during init().
        input_ordinal_t cur_input = INVALID_INPUT_ORDINAL;
        // Holds the prior non-invalid input.
        input_ordinal_t prev_input = INVALID_INPUT_ORDINAL;
        // For static schedules we can populate this up front and avoid needing a
        // lock for dynamically finding the next input, keeping things parallel.
        std::vector<input_ordinal_t> input_indices;
        int input_indices_index = 0;
        // Speculation support.
        std::stack<addr_t> speculation_stack; // Stores PC of resumption point.
        speculator_tmpl_t<RecordType> speculator;
        addr_t speculate_pc = 0;
        // Stores the value of speculate_pc before asking the speculator for the current
        // record.  So if that record was an instruction, speculate_pc holds the next PC
        // while this field holds the instruction's start PC.  The use case is for
        // queueing a read-ahead instruction record for start_speculation().
        addr_t prev_speculate_pc = 0;
        RecordType last_record; // Set to TRACE_TYPE_INVALID in constructor.
        // A list of schedule segments.  These are accessed only while holding
        // sched_lock_.
        std::vector<schedule_record_t> record;
        int record_index = 0;
        bool waiting = false; // Waiting or idling.
        bool active = true;
        bool in_kernel_code = false;
        bool in_context_switch_code = false;
        bool hit_switch_code_end = false;
        // Used for time-based quanta.
        uint64_t cur_time = 0;
        // Used for MAP_TO_RECORDED_OUTPUT get_output_cpuid().
        int64_t as_traced_cpuid = -1;
        // Used for MAP_AS_PREVIOUSLY with live_replay_output_count_.
        bool at_eof = false;
        // Used for replaying wait periods.
        uint64_t wait_start_time = 0;
    };

    // Called just once at initialization time to set the initial input-to-output
    // mappings and state.
    scheduler_status_t
    set_initial_schedule(std::unordered_map<int, std::vector<int>> &workload2inputs);

    // Assumed to only be called at initialization time.
    // Reads ahead in each input to find its first timestamp (queuing the records
    // read to feed to the user's first requests).
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
    next_record(output_ordinal_t output, RecordType &record, input_info_t *&input,
                uint64_t cur_time = 0);

    // Undoes the last read.  May only be called once between next_record() calls.
    // Is not supported during speculation nor prior to speculation with queueing,
    // as documented in the stream interfaces.
    stream_status_t
    unread_last_record(output_ordinal_t output, RecordType &record, input_info_t *&input);

    // Skips ahead to the next region of interest if necessary.
    // The caller must hold the input.lock.
    stream_status_t
    advance_region_of_interest(output_ordinal_t output, RecordType &record,
                               input_info_t &input);

    // Discards the contents of the input queue.  Meant to be used when skipping
    // input records.
    void
    clear_input_queue(input_info_t &input);

    // Does a direct skip, unconditionally.
    // The caller must hold the input.lock.
    stream_status_t
    skip_instructions(output_ordinal_t output, input_info_t &input, uint64_t skip_amount);

    scheduler_status_t
    read_traced_schedule();

    scheduler_status_t
    check_and_fix_modulo_problem_in_schedule(
        std::vector<std::vector<schedule_record_t>> &input_sched,
        std::vector<std::set<uint64_t>> &start2stop,
        std::vector<std::vector<schedule_record_t>> &all_sched);

    scheduler_status_t
    read_recorded_schedule();

    scheduler_status_t
    read_switch_sequences();

    uint64_t
    get_time_micros();

    uint64_t
    get_output_time(output_ordinal_t output);

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
    pick_next_input(output_ordinal_t output, uint64_t blocked_time);

    // Helper for pick_next_input() for MAP_AS_PREVIOUSLY.
    // No input_info_t lock can be held on entry.
    // The sched_lock_ must be held on entry.
    stream_status_t
    pick_next_input_as_previously(output_ordinal_t output, input_ordinal_t &index);

    // If the given record has a thread id field, returns true and the value.
    bool
    record_type_has_tid(RecordType record, memref_tid_t &tid);

    // For trace_entry_t, only sets the tid for record types that have it.
    void
    record_type_set_tid(RecordType &record, memref_tid_t tid);

    // Returns whether the given record is an instruction.
    bool
    record_type_is_instr(RecordType record);

    // If the given record is a marker, returns true and its fields.
    bool
    record_type_is_marker(RecordType record, trace_marker_type_t &type, uintptr_t &value);

    // If the given record is a timestamp, returns true and its fields.
    bool
    record_type_is_timestamp(RecordType record, uintptr_t &value);

    bool
    record_type_is_invalid(RecordType record);

    // Creates the marker we insert between regions of interest.
    RecordType
    create_region_separator_marker(memref_tid_t tid, uintptr_t value);

    // Creates a thread exit record.
    RecordType
    create_thread_exit(memref_tid_t tid);

    RecordType
    create_invalid_record();

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

    // Returns the thread identifier for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int64_t
    get_tid(output_ordinal_t output);

    // Returns the shard index for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int
    get_shard_index(output_ordinal_t output);

    // Returns the workload ordinal value for the current input stream scheduled on
    // the 'output_ordinal'-th output stream.
    int
    get_workload_ordinal(output_ordinal_t output);

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

    stream_status_t
    set_output_active(output_ordinal_t output, bool active);

    // Caller must hold the input's lock.
    void
    mark_input_eof(input_info_t &input);

    stream_status_t
    eof_or_idle(output_ordinal_t output);

    // Returns whether the current record for the current input stream scheduled on
    // the 'output_ordinal'-th output stream is from a part of the trace corresponding
    // to kernel execution.
    bool
    is_record_kernel(output_ordinal_t output);
    ///////////////////////////////////////////////////////////////////////////
    // Support for ready queues for who to schedule next:

    // I tried using a lambda where we could capture "this" and so use int indices
    // in the queues instead of pointers but hit problems (weird crash while running)
    // so I'm sticking with this solution of a separate struct.
    struct InputTimestampComparator {
        bool
        operator()(input_info_t *a, input_info_t *b) const
        {
            if (a->priority != b->priority)
                return a->priority < b->priority; // Higher is better.
            if (a->order_by_timestamp &&
                (a->reader->get_last_timestamp() - a->base_timestamp) !=
                    (b->reader->get_last_timestamp() - b->base_timestamp)) {
                // Lower is better.
                return (a->reader->get_last_timestamp() - a->base_timestamp) >
                    (b->reader->get_last_timestamp() - b->base_timestamp);
            }
            // We use a counter to provide FIFO order for same-priority inputs.
            return a->queue_counter > b->queue_counter; // Lower is better.
        }
    };

    // sched_lock_ must be held by the caller.
    bool
    ready_queue_empty();

    // sched_lock_ must be held by the caller.
    void
    add_to_ready_queue(input_info_t *input);

    // The input's lock must be held by the caller.
    // Returns a multiplier for how long the input should be considered blocked.
    bool
    syscall_incurs_switch(input_info_t *input, uint64_t &blocked_time);

    // sched_lock_ must be held by the caller.
    // "for_output" is which output stream is looking for a new input; only an
    // input which is able to run on that output will be selected.
    stream_status_t
    pop_from_ready_queue(output_ordinal_t for_output, input_info_t *&new_input);

    ///
    ///////////////////////////////////////////////////////////////////////////

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
    // access to inputs_.size(), outputs_.size(), ready_priority_, and
    // ready_counter_.
    std::mutex sched_lock_;
    // Inputs ready to be scheduled, sorted by priority and then timestamp if timestamp
    // dependencies are requested.  We use the timestamp delta from the first observed
    // timestamp in each workload in order to mix inputs from different workloads in the
    // same queue.  FIFO ordering is used for same-priority entries.
    flexible_queue_t<input_info_t *, InputTimestampComparator> ready_priority_;
    // Trackes the count of blocked inputs.  Protected by sched_lock_.
    int num_blocked_ = 0;
    // Global ready queue counter used to provide FIFO for same-priority inputs.
    uint64_t ready_counter_ = 0;
    // Count of inputs not yet at eof.
    std::atomic<int> live_input_count_;
    // In replay mode, count of outputs not yet at the end of the replay sequence.
    std::atomic<int> live_replay_output_count_;
    // Map from workload,tid pair to input.
    struct workload_tid_t {
        workload_tid_t(int wl, memref_tid_t tid)
            : workload(wl)
            , tid(tid)
        {
        }
        bool
        operator==(const workload_tid_t &rhs) const
        {
            return workload == rhs.workload && tid == rhs.tid;
        }
        int workload;
        memref_tid_t tid;
    };
    struct workload_tid_hash_t {
        std::size_t
        operator()(const workload_tid_t &wt) const
        {
            return std::hash<int>()(wt.workload) ^ std::hash<memref_tid_t>()(wt.tid);
        }
    };
    std::unordered_map<workload_tid_t, input_ordinal_t, workload_tid_hash_t> tid2input_;
    struct switch_type_hash_t {
        std::size_t
        operator()(const switch_type_t &st) const
        {
            return std::hash<int>()(static_cast<int>(st));
        }
    };
    std::unordered_map<switch_type_t, std::vector<RecordType>, switch_type_hash_t>
        switch_sequence_;
    // For single_lockstep_output.
    std::unique_ptr<stream_t> global_stream_;
    // For online where we currently have to map dynamically observed thread ids
    // to the 0-based shard index.
    std::unordered_map<memref_tid_t, int> tid2shard_;
};

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<memref_t, reader_t> scheduler_t;

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_scheduler_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SCHEDULER_H_ */
