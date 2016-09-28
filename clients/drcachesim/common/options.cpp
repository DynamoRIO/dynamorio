/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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

/* shared options for both the frontend and the client */

#include <string>
#include "droption.h"
#include "options.h"

droption_t<bool> op_offline
(DROPTION_SCOPE_ALL, "offline", false, "Store trace files for offline analysis",
 "By default, traces are processed online, sent over a pipe to a simulator.  "
 "If this option is enabled, trace data is instead written to files in -outdir "
 "for later offline analysis.  No simulator is executed.");

droption_t<std::string> op_ipc_name
(DROPTION_SCOPE_ALL, "ipc_name", "drcachesimpipe", "Base name of named pipe",
 "For online tracing and simulation (the default, unless -offline is requested), "
 "specifies the base name of the named pipe used to communicate between the target "
 "application processes and the caching device simulator.  A unique name must be chosen "
 "for each instance of the simulator being run at any one time.  On Windows, the name "
 "is limited to 247 characters.");

droption_t<std::string> op_outdir
(DROPTION_SCOPE_ALL, "outdir", ".", "Target directory for offline trace files",
 "For the offline analysis mode (when -offline is requested), specifies the path "
 "to a directory where per-thread trace files will be written.");

droption_t<std::string> op_indir
(DROPTION_SCOPE_ALL, "indir", "", "Offline directory of raw data for input",
 "After a trace file is produced via -offline into -outdir, it can be passed to the "
 "simulator via this flag pointing at the subdirectory created in -outdir.");

droption_t<std::string> op_infile
(DROPTION_SCOPE_ALL, "infile", "", "Offline trace file for input to the simulator",
 "Directs the simulator to use a trace file (not a raw data file from -offline: "
 "such a file neeeds to be converted via drposttrace or -indir first).");

droption_t<unsigned int> op_num_cores
(DROPTION_SCOPE_FRONTEND, "cores", 4, "Number of cores",
 "Specifies the number of cores to simulate.");

droption_t<unsigned int> op_line_size
(DROPTION_SCOPE_FRONTEND, "line_size", 64, "Cache line size",
 "Specifies the cache line size, which is assumed to be identical for L1 and L2 "
 "caches.");

droption_t<bytesize_t> op_L1I_size
(DROPTION_SCOPE_FRONTEND, "L1I_size", 32*1024U, "Instruction cache total size",
 "Specifies the total size of each L1 instruction cache.");

droption_t<bytesize_t> op_L1D_size
(DROPTION_SCOPE_FRONTEND, "L1D_size", bytesize_t(32*1024), "Data cache total size",
 "Specifies the total size of each L1 data cache.");

droption_t<unsigned int> op_L1I_assoc
(DROPTION_SCOPE_FRONTEND, "L1I_assoc", 8, "Instruction cache associativity",
 "Specifies the associativity of each L1 instruction cache.");

droption_t<unsigned int> op_L1D_assoc
(DROPTION_SCOPE_FRONTEND, "L1D_assoc", 8, "Data cache associativity",
 "Specifies the associativity of each L1 data cache.");

droption_t<bytesize_t> op_LL_size
(DROPTION_SCOPE_FRONTEND, "LL_size", 8*1024*1024, "Last-level cache total size",
 "Specifies the total size of the unified last-level (L2) cache.");

droption_t<unsigned int> op_LL_assoc
(DROPTION_SCOPE_FRONTEND, "LL_assoc", 16, "Last-level cache associativity",
 "Specifies the associativity of the unified last-level (L2) cache.");

droption_t<bool> op_use_physical
(DROPTION_SCOPE_CLIENT, "use_physical", false, "Use physical addresses if possible",
 "If available, the default virtual addresses will be translated to physical.  "
 "This is not possible from user mode on all platforms.");

droption_t<unsigned int> op_virt2phys_freq
(DROPTION_SCOPE_CLIENT, "virt2phys_freq", 0, "Frequency of physical mapping refresh",
 "This option only applies if -use_physical is enabled.  The virtual to physical "
 "mapping is cached for performance reasons, yet the underlying mapping can change "
 "without notice.  This option controls the frequency with which the cached value is "
 "ignored in order to re-access the actual mapping and ensure accurate results.  "
 "The units are the number of memory accesses per forced access.  A value of 0 "
 "uses the cached values for the entire application execution.");

droption_t<std::string> op_replace_policy
(DROPTION_SCOPE_FRONTEND, "replace_policy", REPLACE_POLICY_LRU,
 "Cache replacement policy", "Specifies the replacement policy for caches. "
 "Supported policies: LRU (Least Recently Used), LFU (Least Frequently Used), "
 "FIFO (First-In-First-Out).");

droption_t<bytesize_t> op_page_size
(DROPTION_SCOPE_FRONTEND, "page_size", bytesize_t(4*1024), "Virtual/physical page size",
 "Specifies the virtual/physical page size.");

droption_t<unsigned int> op_TLB_L1I_entries
(DROPTION_SCOPE_FRONTEND, "TLB_L1I_entries", 32, "Number of entries in instruction TLB",
 "Specifies the number of entries in each L1 instruction TLB.");

droption_t<unsigned int> op_TLB_L1D_entries
(DROPTION_SCOPE_FRONTEND, "TLB_L1D_entries", 32, "Number of entries in data TLB",
 "Specifies the number of entries in each L1 data TLB.");

droption_t<unsigned int> op_TLB_L1I_assoc
(DROPTION_SCOPE_FRONTEND, "TLB_L1I_assoc", 32, "Instruction TLB associativity",
 "Specifies the associativity of each L1 instruction TLB.");

droption_t<unsigned int> op_TLB_L1D_assoc
(DROPTION_SCOPE_FRONTEND, "TLB_L1D_assoc", 32, "Data TLB associativity",
 "Specifies the associativity of each L1 data TLB.");

droption_t<unsigned int> op_TLB_L2_entries
(DROPTION_SCOPE_FRONTEND, "TLB_L2_entries", 1024, "Number of entries in L2 TLB",
 "Specifies the number of entries in each unified L2 TLB.");

droption_t<unsigned int> op_TLB_L2_assoc
(DROPTION_SCOPE_FRONTEND, "TLB_L2_assoc", 4, "L2 TLB associativity",
 "Specifies the associativity of each unified L2 TLB.");

droption_t<std::string> op_TLB_replace_policy
(DROPTION_SCOPE_FRONTEND, "TLB_replace_policy", REPLACE_POLICY_LFU,
 "TLB replacement policy", "Specifies the replacement policy for TLBs. "
 "Supported policies: LFU (Least Frequently Used).");

droption_t<std::string> op_simulator_type
(DROPTION_SCOPE_FRONTEND, "simulator_type", CPU_CACHE,
 "Simulator type", "Specifies the type of the simulator. "
 "Supported types: " CPU_CACHE", " TLB".");

droption_t<unsigned int> op_verbose
(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64, "Verbosity level",
 "Verbosity level for notifications.");

droption_t<std::string> op_dr_root
(DROPTION_SCOPE_FRONTEND, "dr", "", "Path to DynamoRIO root directory",
 "Specifies the path of the DynamoRIO root directory.");

droption_t<bool> op_dr_debug
(DROPTION_SCOPE_FRONTEND, "dr_debug", false, "Use DynamoRIO debug build",
 "Requests use of the debug build of DynamoRIO rather than the release build.");

droption_t<std::string> op_dr_ops
(DROPTION_SCOPE_FRONTEND, "dr_ops", "", "Options to pass to DynamoRIO",
 "Specifies the options to pass to DynamoRIO.");

droption_t<std::string> op_tracer
(DROPTION_SCOPE_FRONTEND, "tracer", "", "Path to the tracer",
 "The full path to the tracer library.");

droption_t<std::string> op_tracer_ops
(DROPTION_SCOPE_FRONTEND, "tracer_ops",
 DROPTION_FLAG_SWEEP | DROPTION_FLAG_ACCUMULATE | DROPTION_FLAG_INTERNAL,
 "", "(For internal use: sweeps up tracer options)",
 "This is an internal option that sweeps up other options to pass to the tracer.");

droption_t<bytesize_t> op_skip_refs
(DROPTION_SCOPE_FRONTEND, "skip_refs", 0, "Number of memory references to skip",
 "Specifies the number of references to skip "
 "in the beginning of the application execution. "
 "These memory references are dropped instead of being simulated.");

droption_t<bytesize_t> op_warmup_refs
(DROPTION_SCOPE_FRONTEND, "warmup_refs", 0,
 "Number of memory references to warm caches up",
 "Specifies the number of memory references to warm up caches before simulation. "
 "The warmup references come after the skipped references "
 "and before the simulated references.");

droption_t<bytesize_t> op_sim_refs
(DROPTION_SCOPE_FRONTEND, "sim_refs", bytesize_t(1ULL << 63),
 "Number of memory references simulated",
 "Specifies the number of memory references simulated. "
 "The simulated references come after the skipped and warmup references, "
 "and the references following the simulated ones are dropped.");

droption_t<unsigned int> op_report_top
(DROPTION_SCOPE_FRONTEND, "report_top", 10,
 "Number of top results to be reported",
 "Specifies the number of top results to be reported.");
