/* **********************************************************
 * Copyright (c) 2023-2025 Google, Inc.  All rights reserved.
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
#include <map>
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
#include "mutex_dbg_owned.h"
#include "reader.h"
#include "record_file_reader.h"
#include "speculator.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {  /**< General DynamoRIO namespace. */
namespace drmemtrace { /**< DrMemtrace tracing + simulation infrastructure namespace. */

template <typename RecordType, typename ReaderType> class scheduler_impl_tmpl_t;

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
        STATUS_ERROR_RANGE_INVALID,     /**< Error: region of interest invalid. */
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
        /**
         * Indicates an input has a binding whose outputs are all marked inactive.
         */
        STATUS_IMPOSSIBLE_BINDING,
        STATUS_STOLE, /**< Used for internal scheduler purposes. */
    };

    /** Identifies an input stream by its index (0-based). */
    typedef int input_ordinal_t;

    /** Identifies an output stream by its index (0-based). */
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
     * A time range in units of the microsecond timestamps in the traces..
     */
    struct timestamp_range_t {
        /** Convenience constructor. */
        timestamp_range_t(uint64_t start, uint64_t stop)
            : start_timestamp(start)
            , stop_timestamp(stop)
        {
        }
        /**
         * The starting time in the microsecond timestamp units in the trace.
         */
        uint64_t start_timestamp;
        /**
         * The ending time in the microsecond timestamp units in the trace.
         * The ending time is inclusive.  0 means the end of the trace.
         */
        uint64_t stop_timestamp;
    };

    /**
     * Specifies details about one set of  input trace files from one workload.  Each
     * input file typically represents  either one software thread ("thread-sharded")
     * or one hardware thread ("core-sharded").  The  details in this struct apply to
     * the inputs listed in the 'tids' (for thread-sharded, if identifying by tid) or
     * 'shards' (for any sharding, if identifying  by ordinal).  When using 'tids' it
     * is assumed that there is no thread id duplication within one workload.
     */
    struct input_thread_info_t {
        /** Convenience constructor for common usage. */
        explicit input_thread_info_t(std::vector<range_t> regions)
            : regions_of_interest(regions)
        {
        }
        /** Convenience constructor for common usage. */
        input_thread_info_t(memref_tid_t tid, std::vector<range_t> regions)
            : tids(1, tid)
            , regions_of_interest(regions)
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
        /** Convenience constructor for placing one thread on a set of cores. */
        input_thread_info_t(memref_tid_t tid, std::set<output_ordinal_t> output_binding)
            : tids(1, tid)
            , output_binding(output_binding)
        {
        }
        /**
         * Size of the struct.  Not used as this structure cannot support
         * binary-compatible additions due to limits of C++ offsetof support with some
         * implementations of std::set where it is not a standard-layout class: thus,
         * changes here require recompiling tools using this code.
         */
        size_t struct_size = sizeof(input_thread_info_t);
        /**
         * Which input threads the details in this structure apply to.  Only one of
         * 'tids' and 'shards' can be non-empty.  If 'tids' is empty and 'shards' is
         * empty, the details apply to all unmentioned (by other 'tids' or
         * 'shards' vectors in other entries for this workload) inputs in the
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.  When using
         * 'tids' it is assumed that there is no thread id duplication within one
         * workload.  If multiple entries list the same tid, only the last one
         * is honored.
         */
        std::vector<memref_tid_t> tids;
        /**
         * Which inputs the details in this structure apply to, expressed as 0-based
         * ordinals in the 'readers' vector or in the files opened at 'path' (which
         * are sorted lexicographically by path) in the containing
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.  Only one of
         * 'tids' and 'shards' can be non-empty.  If 'tids' is empty and 'shards' is
         * empty, the details apply to all not-yet-mentioned (by other 'tids' or
         * 'shards' vectors in prior entries for this workload) inputs in the
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.
         * If multiple entries list the same shard ordinal, only the last one is
         * honored.
         */
        std::vector<int> shards;
        /**
         * Limits these threads to this set of output streams, which are specified by
         * ordinal 0 through the output count minus oner.  They will not be scheduled
         * on any other output streams.
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
         * discontinuity.  This marker is inserted between back-to-back regions with no
         * separation, but it is not inserted prior to the first range.  A
         * #dynamorio::drmemtrace::TRACE_TYPE_THREAD_EXIT record is inserted after the
         * final range.  These ranges must be non-overlapping and in increasing order.
         *
         * Be aware that selecting a subset of code can remove inter-input
         * communication steps that could be required for forward progress.
         * For example, if selected subsets include #TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE
         * with no timeout but do not include a corresponding
         * #TRACE_MARKER_TYPE_SYSCALL_SCHEDULE for wakeup, an input could remain
         * unscheduled.
         *
         * Also beware that this can skip over trace header entries (like
         * #TRACE_MARKER_TYPE_FILETYPE), which should ideally be obtained from the
         * #dynamorio::drmemtrace::memtrace_stream_t API instead.
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
         * A unique identifier to distinguish from other readers for this workload.  This
         * value is used to fill in the #memref_t "tid" field for synthesized context
         * switch records in dynamic schedules, for synthesized thread exits when skipping
         * beyond the end of an input, and for synthesized region separator markers when
         * skipping.  It is also used to refer to this input in the
         * 'thread_modifiers.tids' field of 'input_workload_t' (though an alternative is
         * to use the 'thread_modifiers.shards' field).
         *
         * This identifier can be non-unique if the aforementioned uses are not
         * relevant: which would typically only be in non-dynamically-scheduled modes.
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
         * in this set are enabled and the rest are ignored.  It is an error to
         * have both this and 'only_shards' be non-empty.
         */
        std::set<memref_tid_t> only_threads;

        /** Scheduling modifiers for the threads in this workload. */
        std::vector<input_thread_info_t> thread_modifiers;

        /**
         * If non-empty, all input records outside of these ranges are skipped.  These
         * times cut across all inputs of this workload.  The times are converted into a
         * separate sequence of instruction
         * #dynamorio::drmemtrace::scheduler_tmpl_t::range_t for each input.  The end
         * result is as though the #dynamorio::drmemtrace::scheduler_tmpl_t::
         * input_thread_info_t.regions_of_interest
         * field were set for each input.  See the comments by that field for further
         * details such as the marker inserted between ranges.  Although the times cut
         * across all inputs for determining the per-input instruction-ordinal ranges, no
         * barrier is applied to align the resulting regions of interest: i.e., one input
         * can finish its initial region and move to its next region before all other
         * inputs finish their initial regions.
         *
         * If non-empty, the
         * #dynamorio::drmemtrace::scheduler_tmpl_t::
         * input_thread_info_t.regions_of_interest
         * field must be empty for each modifier for this workload.
         *
         * If non-empty, the #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.replay_as_traced_istream field must also be specified to
         * provide the timestamp-to-instruction-ordinal mappings.  These mappings are not
         * precise due to the coarse-grained timestamps and the elision of adjacent
         * timestamps in the istream.  Interpolation is used to estimate instruction
         * ordinals when timestamps fall in between recorded points.
         *
         * This field is only supported for thread-sharded inputs, as core-sharded
         * do not have an automatically generated replay-as-traced file.
         */
        std::vector<timestamp_range_t> times_of_interest;

        /**
         * If empty, every trace file in 'path' or every reader in 'readers' becomes
         * an enabled input.  If non-empty, only those inputs whose indices are
         * in this set are enabled and the rest are ignored.  An index is the
         * 0-based ordinal in the 'readers' vector or in the files opened at
         * 'path' (which are sorted lexicographically by path).  It is an error to
         * have both this and 'only_threads' be non-empty.
         */
        std::set<input_ordinal_t> only_shards;

        /**
         * If greater than zero, imposes a maximum number of outputs that the inputs
         * comprising this workload can execute upon simultaneously.  If an input would
         * be executed next but would exceed this cap, a different input is selected
         * instead or the output goes idle if none are found.
         */
        int output_limit = 0;

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
         * In this mode, input #TRACE_MARKER_TYPE_CPU_ID marker values are modified
         * to reflect the virtual cores; input #TRACE_MARKER_TYPE_TIMESTAMP values are
         * modified to reflect a notion of virtual time; and input .tid and .pid
         * #memref_t fields have the workload ordinal set in the top
         * (64 - #MEMREF_ID_WORKLOAD_SHIFT) bits in order
         * to ensure the values are unique across multiple workloads (see also
         * workload_from_memref_pid(), workload_from_memref_tid(),
         * pid_from_memref_tid(), and tid_from_memref_tid()).
         * (The tid and pid changes are not supported for 32-bit builds, and
         * do not support tid values occupying more than #MEMREF_ID_WORKLOAD_SHIFT bits.)
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
         * In this mode, input #TRACE_MARKER_TYPE_CPU_ID marker values are modified
         * to reflect the virtual cores; input #TRACE_MARKER_TYPE_TIMESTAMP values are
         * modified to reflect a notion of virtual time; and input .tid and .pid
         * #memref_t fields have the workload ordinal set in the top 32
         * (64 - #MEMREF_ID_WORKLOAD_SHIFT) bits in order
         * to ensure the values are unique across multiple workloads (see also
         * workload_from_memref_pid(), workload_from_memref_tid(),
         * pid_from_memref_tid(), and tid_from_memref_tid()).
         * (The tid and pid changes are not supported for 32-bit builds, and
         * do not support tid values occupying more than #MEMREF_ID_WORKLOAD_SHIFT bits.)
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
         * If this flag is on, #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.read_inputs_in_init must be set to true.
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
         * Deprecated: use #quantum_duration_us and #time_units_per_us for #QUANTUM_TIME,
         * or #quantum_duration_instrs for #QUANTUM_INSTRUCTIONS, instead.  It
         * is an error to set this to a non-zero value when #struct_size includes
         * #quantum_duration_us.  When #struct_size does not include
         * #quantum_duration_us and this value is non-zero, the value in
         * #quantum_duration_us is replaced with this value divided by the default
         * value of #time_units_per_us.
         */
        uint64_t quantum_duration = 0;
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
         * of traced cores).  Alternatively, if
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.times_of_interest
         * is non-empty, this stream is required for obtaining the mappings between
         * timestamps and instruction ordinals.
         */
        archive_istream_t *replay_as_traced_istream = nullptr;
        /**
         * Determines the minimum latency in the unit of the trace's timestamps
         * (microseconds) for which a non-maybe-blocking system call (one without
         * a #TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL marker) will be treated as
         * blocking and trigger a context switch.
         */
        uint64_t syscall_switch_threshold = 30000000;
        /**
         * Determines the minimum latency in the unit of the trace's timestamps
         * (microseconds) for which a maybe-blocking system call (one with
         * a #TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL marker) will be treated as
         * blocking and trigger a context switch.
         */
        uint64_t blocking_switch_threshold = 500;
        /**
         * Deprecated: use #block_time_multiplier instead.  It is an error to set
         * this to a non-zero value when #struct_size includes #block_time_multiplier.
         * When #struct_size does not include #block_time_multiplier and this value is
         * non-zero, the value in #block_time_multiplier is replaced with this value
         * divided by the default value of #time_units_per_us.
         */
        double block_time_scale = 0.;
        /**
         * Deprecated: use #block_time_max_us and #time_units_per_us instead.  It is
         * an error to set this to a non-zero value when #struct_size includes
         * #block_time_max_us.  When #struct_size does not include #block_time_max_us
         * and this value is non-zero, the value in #block_time_max_us is replaced
         * with this value divided by the default value of #time_units_per_us.
         */
        uint64_t block_time_max = 0;
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
         * of the indicated type. Each record in the inserted sequence holds the
         * next input stream's tid and pid.
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
        /**
         * If true, enables a mode where the normal methods of choosing the next input
         * based on priority, timestamps (if -sched_order_time is set), and FIFO order
         * are disabled.  Instead, the scheduler selects the next input randomly.  Output
         * bindings are still honored.  This is intended for experimental use in
         * sensitivity studies.
         */
        bool randomize_next_input = false;
        /**
         * If true, the scheduler will read from each input to determine its filetype
         * during initialization.  If false, the filetype will not be available prior
         * to explicit record retrieval by the user, but this may be required for
         * inputs whose sources are not yet set up at scheduler init time (e.g.,
         * inputs over blocking pipes with data only becoming available after
         * initializing the scheduler, as happens with online trace analyzers).
         * This must be true for #DEPENDENCY_TIMESTAMPS as it also requires reading
         * ahead.
         */
        bool read_inputs_in_init = true;
        /**
         * If true, the scheduler will attempt to switch to the recorded targets of
         * #TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH system call metadata markers
         * regardless of system call latency.  Furthermore, the scheduler will model
         * "unscheduled" semantics and honor the #TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE
         * and #TRACE_MARKER_TYPE_SYSCALL_SCHEDULE markers.  If false, these markers are
         * ignored and only system call latency thresholds are used to determine switches
         * (these markers remain: they are not removed from the trace).
         */
        bool honor_direct_switches = true;
        /**
         * How many time units for the "cur_time" value passed to next_record() are
         * equivalent to one simulated microsecond.  E.g., if the time units are in
         * picoseconds, pass one million here.  This is used to scale all of the
         * other parameters that are in microseconds (they all end in "_us": e.g.,
         * #quantum_duration_us) so that they operate on the right time scale for the
         * passed-in simulator time (or wall-clock microseconds if no time is passed).
         * The default value is a rough estimate when no accurate simulated time is
         * available: the instruction count is used in that case, and we use the
         * instructions per microsecond for a 2GHz clock at 0.5 IPC as our default.
         */
        double time_units_per_us = 1000.;
        /**
         * The scheduling quantum duration for preemption, in simulated microseconds,
         * for #QUANTUM_TIME.  This value is multiplied by #time_units_per_us to
         * produce a value that is compared to the "cur_time" parameter to
         * next_record() to determine when to force a quantum switch.
         */
        uint64_t quantum_duration_us = 5000;
        /**
         * The scheduling quantum duration for preemption, in instruction count,
         * for #QUANTUM_INSTRUCTIONS.  The time passed to next_record() is ignored
         * for purposes of quantum preempts.
         *
         * Instructions executed in a quantum may end up higher than the specified
         * value to avoid interruption of the kernel system call sequence.
         */
        // We pick 10 million to match 2 instructions per nanosecond with a 5ms quantum.
        uint64_t quantum_duration_instrs = 10 * 1000 * 1000;
        /**
         * Controls the amount of time inputs are considered blocked at a syscall
         * whose as-traced latency (recorded in timestamp records in the trace)
         * exceeds #syscall_switch_threshold or #blocking_switch_threshold.  The
         * as-traced syscall latency (which is in traced microseconds) is multiplied
         * by this field to produce the blocked time in simulated microseconds.  Once
         * that many simulated microseconds have passed according to the "cur_time"
         * value passed to next_record() (multiplied by #time_units_per_us), the
         * input will be no longer considered blocked.  The blocked time is clamped
         * to a maximum value controlled by #block_time_max.
         *
         * While there is no direct overhead during tracing, indirect overhead
         * does result in some inflation of recorded system call latencies.
         * Thus, a value below 0 is typically used here.  This value, in combination
         * with #block_time_max_us, can be tuned to achieve a desired idle rate.
         * The default value errs on the side of less idle time.
         */
        double block_time_multiplier = 0.1;
        /**
         * The maximum time in microseconds for an input to be considered blocked for
         * any one system call.  This value is multiplied by #time_units_per_us to
         * produce a value that is compared to the "cur_time" parameter to
         * next_record().  If any block time (see #block_time_multiplier) exceeds
         * this value, it is capped to this value.  This value is also used as a
         * fallback to avoid hangs when there are no scheduled inputs: if the only
         * inputs left are "unscheduled" (see #TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE),
         * after this amount of time those inputs are all re-scheduled.
         */
        // TODO i#6959: Once we have -exit_if_all_unscheduled raise this.
        uint64_t block_time_max_us = 2500;
        /**
         * The minimum time in microseconds that must have elapsed after an input last
         * ran on an output before that input is allowed to be migrated to a different
         * output.  This value is multiplied by #time_units_per_us to produce a value
         * that is compared to the "cur_time" parameter to next_record().
         */
        uint64_t migration_threshold_us = 500;
        /**
         * The period in microseconds at which rebalancing is performed to keep output
         * run queues from becoming uneven.  This value is multiplied by
         * #time_units_per_us to produce a value that is compared to the "cur_time"
         * parameter to next_record().
         */
        uint64_t rebalance_period_us = 50000;
        /**
         * Determines whether an unscheduled-indefinitely input really is unscheduled for
         * an infinite time, or instead is treated as blocked for the maxiumim time
         * (#block_time_max_us) scaled by #block_time_multiplier.
         */
        bool honor_infinite_timeouts = false;
        /**
         * For #MAP_TO_ANY_OUTPUT, when an input reaches EOF, if the number of non-EOF
         * inputs left as a fraction of the original inputs is equal to or less than
         * this value then the scheduler exits (sets all outputs to EOF) rather than
         * finishing off the final inputs.  This helps avoid long sequences of idles
         * during staggered endings with fewer inputs left than cores and only a small
         * fraction of the total instructions left in those inputs.  Since the remaining
         * instruction count is not considered (as it is not available), use discretion
         * when raising this value on uneven inputs.
         */
        double exit_if_fraction_inputs_left = 0.1;
        /**
         * Input file containing template sequences of kernel system call code.
         * Each sequence must start with a #TRACE_MARKER_TYPE_SYSCALL_TRACE_START
         * marker and end with #TRACE_MARKER_TYPE_SYSCALL_TRACE_END.
         * The value of each marker must hold the system call number for the system call
         * it corresponds to. Sequences for multiple system calls are concatenated into a
         * single file. Each sequence should be in the regular offline drmemtrace format.
         * Whenever a #TRACE_MARKER_TYPE_SYSCALL marker is encountered in a trace, if a
         * corresponding sequence with the same marker value exists it is inserted into
         * the output stream after the #TRACE_MARKER_TYPE_SYSCALL marker.
         * The same file (or reader) must be passed when replaying as this kernel
         * code is not stored when recording.
         * An alternative to passing the file path is to pass #kernel_syscall_reader
         * and #kernel_syscall_reader_end.
         */
        std::string kernel_syscall_trace_path;
        /**
         * An alternative to #kernel_syscall_trace_path is to pass a reader and
         * #kernel_syscall_reader_end.  See the description of #kernel_syscall_trace_path.
         * This field is only examined if #kernel_syscall_trace_path is empty.
         * The scheduler will call the init() function for the reader.
         */
        std::unique_ptr<ReaderType> kernel_syscall_reader;
        /** The end reader for #kernel_syscall_reader. */
        std::unique_ptr<ReaderType> kernel_syscall_reader_end;
        // When adding new options, also add to print_configuration().
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
        stream_t(scheduler_impl_tmpl_t<RecordType, ReaderType> *scheduler, int ordinal,
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
         * and how to continue.  Uses the instruction count plus idle count to this point
         * as the time; use the variant that takes "cur_time" to instead provide a time.
         */
        virtual stream_status_t
        next_record(RecordType &record);

        /**
         * Advances to the next record in the stream.  Returns a status code on whether
         * and how to continue.  Supplies the current time for #QUANTUM_TIME.  The time
         * should be considered to be the simulated time prior to processing the returned
         * record.  The time's units can be chosen by the caller, with
         * #dynamorio::drmemtrace::scheduler_tmpl_t::scheduler_options_t.time_units_per_us
         * providing the conversion to simulated microseconds.  #STATUS_INVALID is
         * returned if 0 or a value smaller than the start time of the current input's
         * quantum is passed in when #QUANTUM_TIME and #MAP_TO_ANY_OUTPUT are specified.
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
        get_record_ordinal() const override;
        /**
         * Identical to get_record_ordinal() but ignores the
         * #SCHEDULER_USE_INPUT_ORDINALS flag.
         */
        uint64_t
        get_output_record_ordinal() const
        {
            return cur_ref_count_;
        }
        /**
         * Returns the count of instructions from the start of the trace to this point.
         * For record_scheduler_t, if any encoding records or the internal record
         * TRACE_MARKER_TYPE_BRANCH_TARGET records are present prior to an instruction
         * marker, the count will increase at the first of those records as they are
         * considered part of the instruction.
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
        get_instruction_ordinal() const override;
        /**
         * Identical to get_instruction_ordinal() but ignores the
         * #SCHEDULER_USE_INPUT_ORDINALS flag.
         */
        uint64_t
        get_output_instruction_ordinal() const
        {
            return cur_instr_count_;
        }
        /**
         * Returns a name for the current input stream feeding this output stream. For
         * stored offline traces, this is the base name of the trace on disk. For online
         * traces, this is the name of the pipe.
         */
        std::string
        get_stream_name() const override;
        /**
         * Returns the ordinal for the current input stream feeding this output stream.
         */
        virtual input_ordinal_t
        get_input_stream_ordinal() const;
        /**
         * Returns the ordinal for the workload which is the source of the current input
         * stream feeding this output stream.  This workload ordinal is the index into the
         * vector of type #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t
         * passed to init().  Returns -1 if there is no current input for this output
         * stream.
         */
        virtual int
        get_input_workload_ordinal() const;
        /**
         * Returns the value of the most recently seen #TRACE_MARKER_TYPE_TIMESTAMP
         * marker.
         */
        uint64_t
        get_last_timestamp() const override;
        /**
         * Returns the value of the first seen #TRACE_MARKER_TYPE_TIMESTAMP marker.
         */
        uint64_t
        get_first_timestamp() const override;
        /**
         * Returns the #trace_version_t value from the
         * #TRACE_MARKER_TYPE_VERSION record in the trace header.
         * This can be queried prior to explicitly retrieving any records from
         * output streams, unless #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.read_inputs_in_init is false (which is the
         * case for online drmemtrace analysis).
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
         * This can be queried prior to explicitly retrieving any records from
         * output streams, unless #dynamorio::drmemtrace::scheduler_tmpl_t::
         * scheduler_options_t.read_inputs_in_init is false (which is the
         * case for online drmemtrace analysis).
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
        is_record_synthetic() const override;

        /**
         * Returns a unique identifier for the current output stream.  For
         * #MAP_TO_RECORDED_OUTPUT, the identifier is the as-traced cpuid mapped to this
         * output.  For dynamic schedules, the identifier is the output stream ordinal,
         * except for #OFFLINE_FILE_TYPE_CORE_SHARDED inputs where the identifier
         * is the input stream ordinal.
         */
        int64_t
        get_output_cpuid() const override;

        /**
         * Returns the ordinal for the current
         * #dynamorio::drmemtrace::scheduler_tmpl_t::input_workload_t.
         *
         * For a core-sharded-on-disk trace (#OFFLINE_FILE_TYPE_CORE_SHARDED), which
         * is already scheduled, get_workload_id() will not return the original
         * separate inputs but rather the new inputs as seen by the scheduler which
         * are a single workload with one input per core.  Use the modified #memref_t
         * tid and pid fields with the helpers workload_from_memref_pid() and
         * workload_from_memref_tid() to obtain the workload in this case.
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
        get_tid() const override;

        /**
         * Returns the #dynamorio::drmemtrace::memtrace_stream_t interface for the
         * current input stream feeding this output stream.
         */
        memtrace_stream_t *
        get_input_interface() const override;

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
         * set to INVALID_THREAD_ID (the online analysis mode for analyzer tools where the
         * inputs for multiple threads are all combined in one process-wide pipe) in which
         * case the last trace record's tid as an ordinal (in the order observed in the
         * output stream) is returned; otherwise returns the output stream ordinal.
         */
        int
        get_shard_index() const override;

        /**
         * Returns whether the current record is from a part of the trace corresponding
         * to kernel execution.
         */
        bool
        is_record_kernel() const override;

        /**
         * Returns the value of the specified statistic for this output stream.
         * The values for all output streams must be summed to obtain global counts.
         * These statistics are not guaranteed to be accurate when replaying a
         * prior schedule via #MAP_TO_RECORDED_OUTPUT.
         */
        double
        get_schedule_statistic(schedule_statistic_t stat) const override;

    protected:
        scheduler_impl_tmpl_t<RecordType, ReaderType> *scheduler_ = nullptr;
        int ordinal_ = -1;
        // If max_ordinal_ >= 0, ordinal_ is incremented modulo max_ordinal_ at the start
        // of every next_record() invocation.
        int max_ordinal_ = -1;
        int verbosity_ = 0;
        uint64_t cur_ref_count_ = 0;
        uint64_t cur_instr_count_ = 0;
        uint64_t last_timestamp_ = 0;
        uint64_t first_timestamp_ = 0;
        bool in_kernel_trace_ = false;
        // Remember top-level headers for the memtrace_stream_t interface.
        uint64_t version_ = 0;
        uint64_t filetype_ = 0;
        uint64_t cache_line_size_ = 0;
        uint64_t chunk_instr_count_ = 0;
        uint64_t page_size_ = 0;
        RecordType prev_record_ = {};

        // Let the impl class update our state.
        friend class scheduler_impl_tmpl_t<RecordType, ReaderType>;
    };

    /** Default constructor. */
    scheduler_tmpl_t() = default;

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
    stream_t *
    get_stream(output_ordinal_t ordinal);

    /** Returns the number of input streams. */
    int
    get_input_stream_count() const;

    /**
     * Returns the #dynamorio::drmemtrace::memtrace_stream_t interface for the
     * 'ordinal'-th input stream.
     */
    memtrace_stream_t *
    get_input_stream_interface(input_ordinal_t input) const;

    /**
     * Returns the name (from get_stream_name()) of the 'ordinal'-th input stream.
     */
    std::string
    get_input_stream_name(input_ordinal_t input) const;

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
    get_error_string() const;

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
    // We use the "pImpl" idiom to allow us to create the appropriate subclass
    // without changing callers who are using the scheduler_t constructor.
    // We need a custom deleter to use an incomplete templated type in unique_ptr.
    struct scheduler_impl_deleter_t {
        void
        operator()(scheduler_impl_tmpl_t<RecordType, ReaderType> *p);
    };
    std::unique_ptr<scheduler_impl_tmpl_t<RecordType, ReaderType>,
                    scheduler_impl_deleter_t>
        impl_;

    friend class scheduler_impl_tmpl_t<RecordType, ReaderType>;
};

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<memref_t, reader_t> scheduler_t;

/** See #dynamorio::drmemtrace::scheduler_tmpl_t. */
typedef scheduler_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>
    record_scheduler_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SCHEDULER_H_ */
