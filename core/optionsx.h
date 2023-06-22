/* *******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * optionsx.h
 *
 * Options and corresponding command line options
 *
 */

/* This file is included multiple times
   - in globals.h once for structure definition,
   - in options.c, four times: for initialization, option parsing and enumeration
   - TODO: should also be included in the GUI
*/

/* Client files that include this header should define the following macro
#define OPTION_COMMAND(type, name, default_value, command_line_option, \
                       statement, description, dynamic_flag, pcache)
#include "optionsx.h"
#undef OPTION_COMMAND
   dynamic_flag takes the values
     DYNAMIC - option can be modified externally
     STATIC  - option cannot be modified externally and changes are ignored
   pcache takes values from op_pcache_t
*/

/* PR 330860: the statement of an OPTION_COMMAND is allowed to read
 * and write to the "options" struct.  It can also change other state
 * but only inside of a check for the bool "for_this_process".
 */

/* The liststring_t type is assumed to contain ;-separated values that are
 * appended to if multiple option instances are specified
 */

/* Shortcuts for the common cases */
#define OPTION_DEFAULT(type, name, value, description) \
    OPTION_COMMAND(type, name, value, #name, {}, description, STATIC, OP_PCACHE_NOP)

#define OPTION_NAME_DEFAULT(type, name, value, option_name, description) \
    OPTION_COMMAND(type, name, value, option_name, {}, description, STATIC, OP_PCACHE_NOP)

#define OPTION_NAME(type, name, option_name, description) \
    OPTION_NAME_DEFAULT(type, name, 0, option_name, description)

#define OPTION(type, name, description) OPTION_DEFAULT(type, name, 0, description)

#define PC_OPTION_DEFAULT(type, name, value, description) \
    OPTION_COMMAND(type, name, value, #name, {}, description, STATIC, OP_PCACHE_GLOBAL)

#define PC_OPTION(type, name, description) PC_OPTION_DEFAULT(type, name, 0, description)

#define PCL_OPTION_DEFAULT(type, name, value, description) \
    OPTION_COMMAND(type, name, value, #name, {}, description, STATIC, OP_PCACHE_LOCAL)

#define PCL_OPTION(type, name, description) PCL_OPTION_DEFAULT(type, name, 0, description)

/* Dynamic option shortcut definitions,
   CHECK: if most options end up being dynamic just add a flag to the above */
#define DYNAMIC_OPTION_DEFAULT(type, name, value, description) \
    OPTION_COMMAND(type, name, value, #name, {}, description, DYNAMIC, OP_PCACHE_NOP)

#define DYNAMIC_OPTION(type, name, description) \
    DYNAMIC_OPTION_DEFAULT(type, name, 0, description)

#define DYNAMIC_PCL_OPTION_DEFAULT(type, name, value, description) \
    OPTION_COMMAND(type, name, value, #name, {}, description, DYNAMIC, OP_PCACHE_LOCAL)

#define DYNAMIC_PCL_OPTION(type, name, description) \
    DYNAMIC_PCL_OPTION_DEFAULT(type, name, 0, description)

/* Shortcut for defining alias command line options to set another option to a constant
   value, Note we can't read the real type/description, and aliases are not transitive */
#define OPTION_ALIAS(new_alias, real_internal_name, real_value, dynamic_flag, pcache) \
    OPTION_COMMAND(                                                                   \
        bool, new_alias##_aka_##real_internal_name, false, #new_alias,                \
        {                                                                             \
            if (options->new_alias##_aka_##real_internal_name)                        \
                options->real_internal_name = real_value;                             \
        },                                                                            \
        #new_alias " is an alias for " #real_internal_name, dynamic_flag, pcache)

/* OPTION_COMMAND_INTERNAL is parsed separately in options.h to
   define constants with default values that are used by INTERNAL_OPTION */
/* FIXME: dynamic internal options not yet supported */
#define OPTION_DEFAULT_INTERNAL(type, name, value, description)                \
    OPTION_COMMAND_INTERNAL(type, name, value, #name, {}, description, STATIC, \
                            OP_PCACHE_NOP)
#define OPTION_NAME_INTERNAL(type, name, option_name, description)               \
    OPTION_COMMAND_INTERNAL(type, name, 0, option_name, {}, description, STATIC, \
                            OP_PCACHE_NOP)
#define OPTION_INTERNAL(type, name, description) \
    OPTION_DEFAULT_INTERNAL(type, name, 0, description)

#define PC_OPTION_DEFAULT_INTERNAL(type, name, value, description)             \
    OPTION_COMMAND_INTERNAL(type, name, value, #name, {}, description, STATIC, \
                            OP_PCACHE_GLOBAL)
#define PC_OPTION_INTERNAL(type, name, description) \
    PC_OPTION_DEFAULT_INTERNAL(type, name, 0, description)

/* option helper macros */
#define DISABLE_RESET(prefix)                                 \
    {                                                         \
        (prefix)->enable_reset = false;                       \
        IF_INTERNAL((prefix)->reset_at_fragment_count = 0;)   \
        (prefix)->reset_at_nth_thread = 0;                    \
        (prefix)->reset_at_switch_to_os_at_vmm_limit = false; \
        (prefix)->reset_at_vmm_percent_free_limit = 0;        \
        (prefix)->reset_at_vmm_free_limit = 0;                \
        (prefix)->reset_at_vmm_full = false;                  \
        (prefix)->reset_at_commit_percent_free_limit = 0;     \
        (prefix)->reset_at_commit_free_limit = 0;             \
        (prefix)->reset_every_nth_pending = 0;                \
        (prefix)->reset_at_nth_bb_unit = 0;                   \
        (prefix)->reset_at_nth_trace_unit = 0;                \
        (prefix)->reset_every_nth_bb_unit = 0;                \
        (prefix)->reset_every_nth_trace_unit = 0;             \
    }
#define REENABLE_RESET(prefix)                                                 \
    {                                                                          \
        (prefix)->enable_reset = true;                                         \
        (prefix)->reset_at_vmm_full = DEFAULT_OPTION_VALUE(reset_at_vmm_full); \
        (prefix)->reset_every_nth_pending =                                    \
            DEFAULT_OPTION_VALUE(reset_every_nth_pending);                     \
        (prefix)->reset_at_switch_to_os_at_vmm_limit =                         \
            DEFAULT_OPTION_VALUE(reset_at_switch_to_os_at_vmm_limit);          \
        (prefix)->reset_at_vmm_percent_free_limit =                            \
            DEFAULT_OPTION_VALUE(reset_at_vmm_percent_free_limit);             \
        (prefix)->reset_at_vmm_free_limit =                                    \
            DEFAULT_OPTION_VALUE(reset_at_vmm_free_limit);                     \
        (prefix)->reset_at_commit_percent_free_limit =                         \
            DEFAULT_OPTION_VALUE(reset_at_commit_percent_free_limit);          \
        (prefix)->reset_at_commit_free_limit =                                 \
            DEFAULT_OPTION_VALUE(reset_at_commit_free_limit);                  \
    }
/* FIXME: case 9014 reenabling more resets or maybe
 * reset_at_vmm_threshold reset_at_commit_threshold shouldn't be
 * disabled in DISABLE_RESET but left under the main option
 */

#define DISABLE_TRACES(prefix)                      \
    {                                               \
        (prefix)->disable_traces = true;            \
        (prefix)->enable_traces = false;            \
        (prefix)->shared_traces = false;            \
        (prefix)->shared_trace_ibl_routine = false; \
        (prefix)->bb_ibl_targets = true;            \
    }
#define REENABLE_TRACES(prefix)                    \
    {                                              \
        (prefix)->disable_traces = false;          \
        (prefix)->enable_traces = true;            \
        (prefix)->shared_traces = true;            \
        (prefix)->shared_trace_ibl_routine = true; \
        (prefix)->bb_ibl_targets = false;          \
    }

#define ENABLE_SECURITY(prefix)                                                        \
    {                                                                                  \
        options->native_exec = true;                                                   \
        options->code_origins = true;                                                  \
        options->ret_after_call = true;                                                \
                                                                                       \
        /* we need to know ib sources */                                               \
        options->indirect_stubs = true;                                                \
                                                                                       \
        IF_RCT_IND_BRANCH(                                                             \
            options->rct_ind_call = IF_WINDOWS_ELSE(                                   \
                OPTION_ENABLED | OPTION_BLOCK | OPTION_REPORT, OPTION_DISABLED);)      \
        IF_RCT_IND_BRANCH(                                                             \
            options->rct_ind_jump = IF_WINDOWS_ELSE(                                   \
                OPTION_ENABLED | OPTION_BLOCK | OPTION_REPORT, OPTION_DISABLED);)      \
        IF_WINDOWS(options->apc_policy =                                               \
                       OPTION_ENABLED | OPTION_BLOCK | OPTION_REPORT | OPTION_CUSTOM;) \
    }

/* options start here */
DYNAMIC_OPTION_DEFAULT(bool, dynamic_options, true, "dynamically update options")

#ifdef EXPOSE_INTERNAL_OPTIONS
OPTION_COMMAND_INTERNAL(
    bool, dummy_version, 0, "version",
    {
        if (for_this_process)
            print_file(STDERR, "<%s>\n", dynamorio_version_string);
    },
    "print version number", STATIC, OP_PCACHE_NOP)
#endif /* EXPOSE_INTERNAL_OPTIONS */

PC_OPTION_INTERNAL(bool, nolink, "disable linking")
PC_OPTION_DEFAULT_INTERNAL(bool, link_ibl, true, "link indirect branches")
OPTION_INTERNAL(bool, tracedump_binary, "binary dump of traces (after optimization)")
OPTION_INTERNAL(bool, tracedump_text, "text dump of traces (after optimization)")
OPTION_INTERNAL(bool, tracedump_origins, "write out original instructions for each trace")
OPTION(bool, syntax_intel, "use Intel disassembly syntax")
OPTION(bool, syntax_att, "use AT&T disassembly syntax")
OPTION(bool, syntax_arm, "use ARM (32-bit) disassembly syntax")
OPTION(bool, syntax_riscv, "use RISC-V disassembly syntax")
/* TODO i#4382: Add syntax_aarch64. */
/* whether to mark gray-area instrs as invalid when we know the length (i#1118) */
OPTION(bool, decode_strict, "mark all known-invalid instructions as invalid")
OPTION(uint, disasm_mask, "disassembly style as a dr_disasm_flags_t bitmask")
OPTION_INTERNAL(bool, bbdump_tags, "dump tags, sizes, and sharedness of all bbs")
OPTION_INTERNAL(bool, gendump, "dump generated code")
OPTION_DEFAULT(bool, global_rstats, true, "enable global release-build statistics")
OPTION_DEFAULT_INTERNAL(bool, rstats_to_stderr, false,
                        "print the final global rstats to stderr")

/* this takes precedence over the DYNAMORIO_VAR_LOGDIR config var */
OPTION_DEFAULT(pathstring_t, logdir, EMPTY_STRING, "directory for log files")
OPTION(bool, log_to_stderr, "log to stderr instead of files")
#ifdef DEBUG /* options that only work for debug build */
/* we do allow logging for customers for forensics/diagnostics that requires
 * debug build for more information.
 * FIXME: somehow restrict info we dump out to prevent IP leakage for INTERNAL=0?
 * restrict loglevel to <= 2 for INTERNAL=0?
 */
/* log control fields will be kept in dr_statistics_t structure so they can be updated,
 yet we'll also have the initial value in options_t at the cost of 8 bytes */
OPTION_COMMAND(
    uint, stats_logmask, 0, "logmask",
    {
        if (d_r_stats != NULL && for_this_process)
            d_r_stats->logmask = options->stats_logmask;
    },
    "set mask for logging from specified modules", DYNAMIC, OP_PCACHE_NOP)

OPTION_COMMAND(
    uint, stats_loglevel, 0, "loglevel",
    {
        if (d_r_stats != NULL && for_this_process)
            d_r_stats->loglevel = options->stats_loglevel;
    },
    "set level of detail for logging", DYNAMIC, OP_PCACHE_NOP)
OPTION_INTERNAL(uint, log_at_fragment_count,
                "start execution at loglevel 1 and raise to the specified -loglevel at "
                "this fragment count")
/* For debugging purposes.  The bb count is distinct from the fragment count. */
OPTION_INTERNAL(
    uint, go_native_at_bb_count,
    "once this count is reached, each thread will go native when creating a new bb")
/* Note that these are not truly DYNAMIC, and they don't get synchronized before each LOG
 */
OPTION_DEFAULT(uint, checklevel, 2, "level of asserts/consistency checks (PR 211887)")

OPTION_DEFAULT_INTERNAL(bool, thread_stats, true, "enable thread local statistics")
OPTION_DEFAULT_INTERNAL(bool, global_stats, true, "enable global statistics")
OPTION_DEFAULT_INTERNAL(
    uint, thread_stats_interval, 10000,
    "per-thread statistics dump interval in fragments, 0 to disable periodic dump")
OPTION_DEFAULT_INTERNAL(
    uint, global_stats_interval, 5000,
    "global statistics dump interval in fragments, 0 to disable periodic dump")
#    ifdef HASHTABLE_STATISTICS
OPTION_DEFAULT_INTERNAL(bool, hashtable_study, true, "enable hashtable studies")
OPTION_DEFAULT_INTERNAL(bool, hashtable_ibl_stats, true,
                        "enable hashtable statistics for IBL routines")
/* off by default until non-sharing bug 5846 fixed */
OPTION_DEFAULT_INTERNAL(bool, hashtable_ibl_entry_stats, false,
                        "enable hashtable statistics per IBL entry")
OPTION_DEFAULT_INTERNAL(uint, hashtable_ibl_study_interval, 50,
                        "dump stats after some IBL entry additions")
OPTION_DEFAULT_INTERNAL(bool, stay_on_trace_stats, false,
                        "enable stay on trace statistics")
OPTION_DEFAULT_INTERNAL(bool, speculate_last_exit_stats, false,
                        "enable speculative last stay_on_trace_stats")
#    endif
#endif /* DEBUG */
#ifdef KSTATS
/* turn on kstats by default for debug builds */
/* For ARM we have no cheap tsc so we disable by default (i#1581) */
OPTION_DEFAULT(bool, kstats, IF_DEBUG_ELSE_0(IF_X86_ELSE(true, false)),
               "enable path timing statistics")
#endif

#ifdef DEADLOCK_AVOIDANCE
OPTION_DEFAULT_INTERNAL(bool, deadlock_avoidance, true,
                        "enable deadlock avoidance checks")
OPTION_DEFAULT_INTERNAL(
    uint, mutex_callstack, 0 /* 0 to disable, 4 recommended, MAX_MUTEX_CALLSTACK */,
    "collect a callstack up to specified depth when a mutex is locked")
#endif

#ifdef CALL_PROFILE
OPTION(uint, prof_caller, /* 0 to disable, 3-5 recommended */
       "collect caller data for instrumented routines to this depth")
#endif

#ifdef HEAP_ACCOUNTING
OPTION_DEFAULT_INTERNAL(bool, heap_accounting_assert, true,
                        "enable heap accounting assert")
#endif

#if defined(UNIX)
OPTION_NAME_INTERNAL(bool, profile_pcs, "prof_pcs", "pc-sampling profiling")
OPTION_DEFAULT_INTERNAL(uint_size, prof_pcs_heap_size, 24 * 1024,
                        "special heap size for pc-sampling profiling")
#else
#    ifdef WINDOWS_PC_SAMPLE
OPTION_NAME(bool, profile_pcs, "prof_pcs", "pc-sampling profiling")
#    endif
#endif

/* XXX i#1114: enable by default when the implementation is complete */
OPTION_DEFAULT(bool, opt_jit, false, "optimize translation of dynamically generated code")

#ifdef UNIX
OPTION_COMMAND(
    pathstring_t, xarch_root, EMPTY_STRING, "xarch_root",
    {
        /* Running under QEMU requires timing out and then leaving
         * the failed-takeover QEMU thread native, so we bundle that
         * here for convenience.  We target the common use case of a
         * small app, for which we want a small timeout.
         */
        if (options->xarch_root[0] != '\0') {
            options->unsafe_ignore_takeover_timeout = true;
            options->takeover_timeout_ms = 400;
        }
    },
    "QEMU support: prefix to add to opened files for emulation; also sets "
    "-unsafe_ignore_takeover_timeout and -takeover_timeout_ms 400",
    STATIC, OP_PCACHE_NOP)
#endif

#ifdef EXPOSE_INTERNAL_OPTIONS
#    ifdef PROFILE_RDTSC
OPTION_NAME_INTERNAL(bool, profile_times, "prof_times", "profiling via measuring time")
#    endif

/* -prof_counts and PROFILE_LINKCOUNT are no longer supported and have been removed */

/* FIXME (xref PR 215082): make these external now that our product is our API? */
/* XXX: These -client_lib* options do affect pcaches, but we don't want the
 * client option strings to matter, so we check them separately
 * from the general -persist_check_options.
 */
/* This option is ignored for STATIC_LIBRARY. */
/* XXX: We could save space by removing -client_lib and using only the {32,64}
 * versions (or by having some kind of true alias where _lib points to the other).
 */
OPTION_DEFAULT_INTERNAL(liststring_t, client_lib, EMPTY_STRING,
                        ";-separated string containing client "
                        "lib paths, IDs, and options")
#    ifdef X64
OPTION_COMMAND_INTERNAL(
    liststring_t, client_lib64, EMPTY_STRING, "client_lib64",
    {
        snprintf(options->client_lib, BUFFER_SIZE_ELEMENTS(options->client_lib), "%s",
                 options->client_lib64);
        NULL_TERMINATE_BUFFER(options->client_lib);
    },
    ";-separated string containing client "
    "lib paths, IDs, and options",
    STATIC, OP_PCACHE_NOP /* See note about pcache above. */)
#        define ALT_CLIENT_LIB_NAME client_lib32
#    else
OPTION_COMMAND_INTERNAL(
    liststring_t, client_lib32, EMPTY_STRING, "client_lib32",
    {
        snprintf(options->client_lib, BUFFER_SIZE_ELEMENTS(options->client_lib), "%s",
                 options->client_lib32);
        NULL_TERMINATE_BUFFER(options->client_lib);
    },
    ";-separated string containing client "
    "lib paths, IDs, and options",
    STATIC, OP_PCACHE_NOP /* See note about pcache above. */)
#        define ALT_CLIENT_LIB_NAME client_lib64
#    endif
OPTION_COMMAND_INTERNAL(liststring_t, ALT_CLIENT_LIB_NAME, EMPTY_STRING,
                        IF_X64_ELSE("client_lib32", "client_lib64"), {},
                        ";-separated string containing client "
                        "lib paths, IDs, and options for other-bitwidth children",
                        STATIC, OP_PCACHE_GLOBAL)

/* If we revive hotpatching should use this there as well: but for now
 * don't want to mess up any legacy tools that rely on hotp libs in
 * regular loader list
 */
/* XXX i#1285: MacOS private loader is NYI */
OPTION_DEFAULT_INTERNAL(bool, private_loader,
                        /* i#2117: for UNIX static DR we disable TLS swaps. */
                        IF_STATIC_LIBRARY_ELSE(IF_WINDOWS_ELSE(true, false),
                                               IF_MACOS_ELSE(false, true)),
                        "use private loader for clients and dependents")
#    ifdef UNIX
/* We cannot know the total tls size when allocating tls in os_tls_init,
 * so use the runtime option to control the tls size.
 */
OPTION_DEFAULT_INTERNAL(uint, client_lib_tls_size, 1,
                        "number of pages used for client libraries' TLS memory")
/* Controls whether we register symbol files with gdb.  This has very low
 * overhead if gdb is not attached, and if it is, we probably want to have
 * symbols anyway.
 */
OPTION_DEFAULT_INTERNAL(bool, privload_register_gdb, true,
                        "register private loader DLLs with gdb")
#    endif
#    ifdef WINDOWS
/* Heap isolation for private dll copies.  Valid only with -private_loader. */
OPTION_DEFAULT_INTERNAL(bool, privlib_privheap, true,
                        "redirect heap usage by private libraries to DR heap")
/* PEB and select TEB field isolation for private dll copies (i#249).
 * Valid only with -private_loader.
 * XXX: turning this option off is not supported.  Should we remove it?
 */
OPTION_DEFAULT_INTERNAL(bool, private_peb, true,
                        "use private PEB + TEB fields for private libraries")
#    endif

/* PR 200418: Code Manipulation API.  This option enables the code
 * manipulation events and sets some default options.  We can't
 * afford to check for this in our exported routines, so we allow
 * ourselves to be used as a utility or standalone library
 * regardless of this option.
 * For the static library, we commit to use with code_api and enable
 * it by default as it's more of a pain to set options with this model.
 */
OPTION_COMMAND_INTERNAL(
    bool, code_api, IF_STATIC_LIBRARY_ELSE(true, false), "code_api",
    {
        if (options->code_api) {
            options_enable_code_api_dependences(options);
        }
    },
    "enable Code Manipulation API", STATIC, OP_PCACHE_NOP)

/* PR 200418: Probe API.  Note that the code manip API is off by
 * default, so -probe_api by itself will give users the lightweight
 * and more restricted version of the probe API.  If users want the
 * flexibility to place a probe anywhere, or if they also want code
 * manipulation ability, they'll have to enable the code manip API
 * as well.  For simplicity, we assume they're willing to accept
 * any associated performance hit (e.g., turning off elision), when
 * enabling the code manip API.
 */
OPTION_COMMAND_INTERNAL(
    bool, probe_api, false, "probe_api",
    {
        if (options->probe_api) { /* else leave alone */
            IF_HOTP(options->hot_patching = true);
            IF_HOTP(options->liveshields = false;)
            IF_GBOP(options->gbop = 0;)
        }
    },
    "enable Probe API", STATIC, OP_PCACHE_NOP)
#    define DISABLE_PROBE_API(prefix)                \
        {                                            \
            (prefix)->probe_api = false;             \
            IF_HOTP((prefix)->hot_patching = false;) \
        }

/* PR 326610: provide -opt_speed option.  In future we may want to
 * expose these separately (PR 225139), and fully support their
 * robustness esp in presence of clients: for now we consider this
 * less robust.
 * FIXME: actually we can't turn elision on b/c it violates our
 * translation and client trace building assumptions: plus Tim
 * hit crashes w/ max_elide_call and code_api on in 32-bit linux.
 * So leaving option here for now but not documenting it.
 */
OPTION_COMMAND(
    bool, opt_speed, false, "opt_speed",
    {
        if (options->opt_speed) {
            /* We now have -coarse_units and -indirect_stubs off by default,
             * so elision is the only thing left here, but -indcall2direct
             * is significant on windows server apps.
             */
            /* See comments under -code_api about why these cause problems
             * with clients: but we risk it here.
             */
            options->max_elide_jmp = 16;
            options->max_elide_call = 16;
            options->indcall2direct = true;
        }
    },
    "enable high performance at potential loss in client fidelity", STATIC, OP_PCACHE_NOP)

/* We turned -coarse_units off by default due to PR 326815 */
OPTION_COMMAND(
    bool, opt_memory, false, "opt_memory",
    {
        if (options->opt_memory) {
            ENABLE_COARSE_UNITS(options);
        }
    },
    "enable memory savings at potential loss in performance", STATIC, OP_PCACHE_NOP)

PC_OPTION_DEFAULT_INTERNAL(bool, bb_prefixes, IF_AARCH64_ELSE(true, false),
                           "give all bbs a prefix")
/* If a client registers a bb hook, we force a full decode.  This option
 * requests a full decode regardless of whether there is a bb hook.
 * Note that there is no way to make this available for non-CI builds
 * yet be exposed in non-internal CI builds, so we make it CI-only.
 */
OPTION_INTERNAL(bool, full_decode, "decode all instrs to level 3 during bb building")
/* Provides a speed boost at startup for observation-only clients that don't
 * use any libraries that need to see all instructions.
 * Not officially supported yet: see i#805 and i#1112.
 * Not compatible with DR_EMIT_STORE_TRANSLATIONS.
 */
OPTION_INTERNAL(bool, fast_client_decode,
                "avoid full decoding even when clients are present (risky)")
#endif /* EXPOSE_INTERNAL_OPTIONS */
#ifdef UNIX
OPTION_DEFAULT_INTERNAL(bool, separate_private_bss, true,
                        "place empty page to separate private lib .bss")
#endif

/* i#42: Optimize and shrink clean call sequences */
/* Optimization level of clean call instrumentation:
 * 0 - no optimization
 * 1 - callee's register usage analysis, e.g. use of XMM registers
 * 2 - simple callee inline optimization,
 *     callee save reg analysis
 *     aflags usage analysis and optimization on the instrumented ilist
 * 3 - more aggressive callee inline optimization
 * All the optimizations assume that clean callee will not be changed
 * later.
 */
/* FIXME i#2094: NYI on ARM. */
/* FIXME i#2796: Clean call inlining is missing a few bits on AArch64. */
OPTION_DEFAULT_INTERNAL(uint, opt_cleancall, IF_X86_ELSE(2, IF_AARCH64_ELSE(1, 0)),
                        "optimization level on optimizing clean call sequences")
/* Assuming the client's clean call does not rely on the cleared eflags,
 * i.e., initialize the eflags before using it, we can skip the eflags
 * clear code.
 * Note: we still clear DF for string instructions.
 * Note: this option is ignored for ARM.
 */
OPTION_DEFAULT(bool, cleancall_ignore_eflags, true,
               "skip eflags clear code with assumption that clean call does not rely on "
               "cleared eflags")
#ifdef X86
/* TLS handling summary:
 * On X86, we use -mangle_app_seg to control if we will steal app's TLS.
 * If -mangle_app_seg is true, DR steals app's TLS and monitors/mangles all
 * accesses to app's TLS.  This provides better isolation between app and DR.
 * Private loader and libraries (-private_loader) also relies on -mangle_app_seg
 * for better transparency with separate copy of TLS used by client libraries.
 *
 * On ARM, we want to steal app's TLS for similar reason (better transparaency).
 * In addition, because monitoring app's TLS is easier (we only need mangle simple
 * thread register read instruction) and more robust (fewer assumptions about
 * app's TLS layout for storing DR's TLS base), we decide to always steal the
 * app's TLS, and so no option is needed. Also, we cannot easily handle
 * raw threads created without CLONE_SETTLS without stealing TLS.
 */
/* i#107: To handle app using same segment register that DR uses, we should
 * mangle the app's segment usage.
 * It cannot be used with DGC_DIAGNOSTICS.
 */
OPTION_DEFAULT_INTERNAL(bool, mangle_app_seg,
                        IF_WINDOWS_ELSE(false, IF_LINUX_ELSE(true, false)),
                        "mangle application's segment usage.")
#endif /* X86 */
#ifdef X64
#    ifdef WINDOWS
/* TODO i#49: This option is still experimental and is not fully tested/supported yet. */
OPTION_DEFAULT(bool, inject_x64, false,
               "Inject 64-bit DynamoRIO into 32-bit child processes.")
#    endif
OPTION_COMMAND(
    bool, x86_to_x64, false, "x86_to_x64",
    {
        /* i#1494: to avoid decode_fragment messing up the 32-bit/64-bit mode,
         * we do not support any cases of using decode_fragment, including
         * trace and coarse_units (coarse-grain code cache management).
         */
        if (options->x86_to_x64) {
            DISABLE_TRACES(options);
            DISABLE_COARSE_UNITS(options);
        }
    },
    "translate x86 code to x64 when on a 64-bit kernel.", STATIC, OP_PCACHE_GLOBAL)
OPTION_DEFAULT(bool, x86_to_x64_ibl_opt, false,
               "Optimize ibl code with extra 64-bit registers in x86_to_x64 mode.")
#endif

#ifdef AARCHXX
/* we only allow register between r8 and r12(A32)/r29(A64) to be used */
OPTION_DEFAULT_INTERNAL(uint, steal_reg, IF_X64_ELSE(28 /*r28*/, 10 /*r10*/),
                        "the register stolen/used by DynamoRIO")
OPTION_DEFAULT_INTERNAL(uint, steal_reg_at_reset, 0, "reg to switch to at first reset")
/* Optimization level of mangling:
 * 0 - no optimization,
 * 1 - simple optimization with fast and simple analysis for low overhead
 *     at instrumentation time,
 * 2 - aggressive optimization with complex analysis for better performance
 *     at execution time.
 */
OPTION_DEFAULT_INTERNAL(uint, opt_mangle, 1,
                        "optimization level on optimizing mangle sequences")
#endif
#ifdef AARCH64
OPTION_DEFAULT_INTERNAL(bool, unsafe_build_ldstex, false,
                        "replace blocks using exclusive load/store with a "
                        "macro-instruction (unsafe)")
#endif
#ifdef AARCHXX
/* TODO i#1698: ARM is still missing the abilty to convert the following:
 * + ldrexd..strexd.
 * + Predicated exclusive loads or stores.
 * It will continue with a debug build warning if it sees those.
 */
OPTION_DEFAULT_INTERNAL(bool, ldstex2cas, true,
                        "replace exclusive load/store with compare-and-swap to "
                        "allow instrumentation, at the risk of ABA errors")
#endif

#ifdef WINDOWS_PC_SAMPLE
OPTION_DEFAULT(uint, prof_pcs_DR, 2,
               "PC profile dynamorio.dll, value is bit shift to use, < 2 or > 32 "
               "disables, requires -prof_pcs")
OPTION_DEFAULT(uint, prof_pcs_gencode, 2,
               "PC profile generated code, value is bit shift to use, < 2 or > 32 "
               "disables, requires -prof_pcs")
OPTION_DEFAULT(uint, prof_pcs_fcache, 30,
               "PC profile fcache units, value is bit shift to use, < 2 or > 32 "
               "disables, requires -prof_pcs")
OPTION_DEFAULT(uint, prof_pcs_stubs, 30,
               "PC profile separate stub units.  Value is bit shift to use: < 2 or > 32 "
               "disables.  Requires -prof_pcs.")
OPTION_DEFAULT(uint, prof_pcs_ntdll, 30,
               "PC profile ntdll.dll, value is bit shift to use, < 2 or > 32 disables, "
               "requires -prof_pcs")
OPTION_DEFAULT(uint, prof_pcs_global, 30,
               "PC profile global, value is bit shift to use, < 8 or > 32 sets to "
               "default, requires -prof_pcs")
OPTION_DEFAULT(uint, prof_pcs_freq, 10000,
               "Profiling sample frequency in 100's of nanoseconds, requires -prof_pcs")
#endif

DYNAMIC_OPTION_DEFAULT(
    uint, msgbox_mask,
    /* Enable for client debug builds so DR ASSERTS are visible (xref PR 232783) */
    /* i#116/PR 394985: for Linux off by default since won't work for all apps */
    /* For CI builds, interactive use is the norm: so we enable, esp since
     * we can't print to the cmd console.  The user must explicitly disable
     * for automation or running daemons.
     */
    IF_WINDOWS_ELSE(IF_UNIT_TEST_ELSE(0, IF_AUTOMATED_ELSE(0, 0xC)), 0),
    "show a messagebox for events")
#ifdef WINDOWS
OPTION_DEFAULT(uint_time, eventlog_timeout, 10000,
               "gives the timeout (in ms) to use for an eventlog transaction")
#else
DYNAMIC_OPTION(bool, pause_via_loop,
               "For -msgbox_mask, use an infinite loop instead of waiting for stdin")
#endif                                       /* WINDOWS */
DYNAMIC_OPTION_DEFAULT(uint, syslog_mask, 0, /* PR 232126: re-enable: SYSLOG_ALL */
                       "log only specified message types")
/* Example: -syslog_mask 0x4 - error messages
 *          -syslog_mask 0x6 - error and warning messages
 */
OPTION_DEFAULT_INTERNAL(uint, syslog_internal_mask,
                        0, /* PR 232126: re-enable: SYSLOG_ALL */
                        "log only specified internal message types")

OPTION_DEFAULT(
    bool, syslog_init, false,
    "initialize syslog, unnecessary if correctly installed") /* PR 232126: re-enable for
                                                                product: true */
#ifdef WINDOWS
DYNAMIC_OPTION(uint, internal_detach_mask,
               "indicates what events the core should detach from the app on")
#endif
/* Leaving dumpcore off by default even for DEBUG + INTERNAL b/c that's
 * now what's packaged up.  For test suites we can explicitly turn it on
 * in the future by setting up some other define.
 * Good defaults for Windows are 0x8bff, for Linux 0x837f
 */
DYNAMIC_OPTION_DEFAULT(uint, dumpcore_mask, 0, "indicate events to dump core on")
/* This is basically superseded by -msgbox_mask + -pause_via_loop (i#1665) */
IF_UNIX(OPTION_ALIAS(pause_on_error, dumpcore_mask, DUMPCORE_OPTION_PAUSE, STATIC,
                     OP_PCACHE_NOP))
/* Note that you also won't get more then -report_max violation core dumps */
DYNAMIC_OPTION_DEFAULT(uint, dumpcore_violation_threshold, 3,
                       "maximum number of violations to core dump on")

DYNAMIC_OPTION_DEFAULT(
    bool, live_dump, IF_WINDOWS_ELSE(true, IF_VMX86_ELSE(true, false)),
    "do a live core dump (no outside dependencies) when warranted by the dumpcore_mask")
#ifdef WINDOWS
/* XXX: make a dynamic option */
OPTION_INTERNAL(
    bool, external_dump,
    "do a core dump using an external debugger (specified in the ONCRASH registry value) "
    "when warranted by the dumpcore_mask (kills process on win2k or w/ drwtsn32)")
#endif
#if defined(STATIC_LIBRARY) && defined(UNIX)
/* i#2119: invoke app handler on DR crash.
 * If this were off by default it could be a dumpcore bitflag instead.
 */
OPTION_DEFAULT_INTERNAL(bool, invoke_app_on_crash, true,
                        "On a DR crash, invoke the app fault handler if it exists.")
#endif

OPTION_DEFAULT(uint, stderr_mask,
               /* Enable for client linux debug so ASSERTS are visible (PR 232783) */
               IF_DEBUG_ELSE(SYSLOG_ALL, SYSLOG_CRITICAL | SYSLOG_ERROR),
               "show messages onto stderr")

OPTION_DEFAULT(uint, appfault_mask, IF_DEBUG_ELSE(APPFAULT_CRASH, 0),
               "report diagnostic information on application faults")

#ifdef UNIX
/* Xref PR 258731 - options to duplicate stdout/stderr for our or client logging if
 * application tries to close them. */
OPTION_DEFAULT(bool, dup_stdout_on_close, true,
               "Duplicate stdout for DynamoRIO "
               "or client usage if app tries to close it.")
OPTION_DEFAULT(bool, dup_stderr_on_close, true,
               "Duplicate stderr for DynamoRIO "
               "or client usage if app tries to close it.")
OPTION_DEFAULT(bool, dup_stdin_on_close, true,
               "Duplicate stdin for DynamoRIO "
               "or client usage if app tries to close it.")
/* Clients using drsyms can easily load dozens of files (i#879).
 * No downside to raising since we'll let the app have ours if it
 * runs out.
 */
OPTION_DEFAULT(uint, steal_fds, 96,
               "number of fds to steal from the app outside the app's reach")
OPTION_DEFAULT(bool, fail_on_stolen_fds, true,
               "return failure on app operations on fds preserved for DR's usage")

/* Xref PR 308654 where calling dlclose on the client lib at exit time can lead
 * to an app crash. */
OPTION_DEFAULT(bool, avoid_dlclose, true, "Avoid calling dlclose from DynamoRIO.")

/* PR 304708: we intercept all signals for a better client interface */
OPTION_DEFAULT(bool, intercept_all_signals, true, "intercept all signals")
OPTION_DEFAULT(bool, reroute_alarm_signals, true,
               "reroute alarm signals arriving in a blocked-for-app thread")
OPTION_DEFAULT(uint, max_pending_signals, 8,
               "maximum count of pending signals per thread")

/* i#2080: we have had some problems using sigreturn to set a thread's
 * context to a given state.  Turning this off will instead use a direct
 * mechanism that will set only the GPR's and will assume the target stack
 * is valid and its beyond-TOS slot can be clobbered.  X86-only.
 */
OPTION_DEFAULT_INTERNAL(bool, use_sigreturn_setcontext, true,
                        "use sigreturn to set a thread's context")

/* i#853: Use our all_memory_areas address space cache when possible.  This
 * avoids expensive reads of /proc/pid/maps, but if the cache becomes stale,
 * we may have incorrect results.
 * This option has no effect on platforms with a direct memory query, such
 * as MacOS.
 */
OPTION_DEFAULT(bool, use_all_memory_areas, true,
               "Use all_memory_areas "
               "address space cache to query page protections.")
#endif /* UNIX */

/* Disable diagnostics by default. -security turns it on */
DYNAMIC_OPTION_DEFAULT(bool, diagnostics, false, "enable diagnostic reporting")

/* For MacOS, set to 0 to disable the check */
OPTION_DEFAULT(uint, max_supported_os_version, IF_WINDOWS_ELSE(105, IF_MACOS_ELSE(19, 0)),
               /* case 447, defaults to supporting NT, 2000, XP, 2003, and Vista.
                * Windows 7 added with i#218
                * Windows 8 added with i#565
                * Windows 8.1 added with i#1203
                * Windows 10 added with i#1714
                */
               "Warn on unsupported (but workable) operating system versions greater "
               "than max_supported_os_version")

OPTION_DEFAULT(
    uint, os_aslr,
    /* case 8225 - for now we disable our own ASLR.
     * we do not disable persistent caches b/c they're off by default
     * anyway and if someone turns them on then up to him/her to understand
     * that we don't have relocation support (i#661)
     */
    0x1 /* OS_ASLR_DISABLE_ASLR_ALL */,
    "disable selectively pcache or our ASLR when OS provides ASLR on most modules")
OPTION_DEFAULT_INTERNAL(uint, os_aslr_version, 60, /* WINDOWS_VERSION_VISTA, Vista RTM+ */
                        "minimal OS version to assume ASLR may be provided by OS")

/* case 10509: we only use this on <= win2k as it significantly impacts boot time */
OPTION_DEFAULT(uint_time, svchost_timeout, 1000,
               "timeout (in ms) on an untimely unloaded library on Windows NT or Windows "
               "2000") /* case
                          374
                        */

OPTION_DEFAULT(uint_time, deadlock_timeout,
               IF_DEBUG_ELSE_0(60) * 3 * 1000, /* disabled in release */
               "timeout (in ms) before assuming a deadlock had occurred (0 to disable)")

/* stack_size may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, stack_size,
               /* For clients we have a larger MAX_OPTIONS_STRING so we need
                * a larger stack even w/ no client present.
                * 32KB is the max that will still allow sharing per-thread
                * gencode in the same 64KB alloc as the stack on Windows.
                */
               /* Mac M1's page size is 16K. */
               IF_MACOSA64_ELSE(32 * 1024, 24 * 1024),
               "size of thread-private stacks, in KB")
#ifdef UNIX
/* signal_stack_size may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, signal_stack_size, 24 * 1024,
               "size of signal handling stacks, in KB")
#endif
/* PR 415959: smaller vmm block size makes this both not work and not needed
 * on Linux.
 * FIXME PR 403008: stack_shares_gencode fails on vmkernel
 */
OPTION_DEFAULT(bool, stack_shares_gencode,
               /* We disable for client builds for DrMi#1723 for high-up stacks
                * that aren't nec reachable.  Plus, client stacks are big
                * enough now (56K) that this option was internally never triggered.
                */
               false,
               "stack and thread-private generated code share an allocation region")

OPTION_DEFAULT(uint, spinlock_count_on_SMP, 1000U, "spinlock loop cycles on SMP")
/* This is a true spinlock where on an SMP we check for availability in a tight loop
   instead of yielding and waiting on a kernel object. See Robbins for a discussion on a
   good value for the above -
   it should be based on expectations on how long does it take to finish a task. */

/* i#1111: try to improve startup-time performance by avoiding the bb lock
 * in the initial thread.  However, we can have races if a new thread
 * appears for which we did not see the creation call: e.g., for a nudge,
 * or any other externally-created thread.  Thus this is off by default.
 */
OPTION_INTERNAL(bool, nop_initial_bblock,
                "nop bb building lock until 2nd thread is created")

/* INTERNAL options */
/* These options should be used with a wrapper INTERNAL_OPTION(opt) which in external */
/* builds is turned into the default value, hence all non-default code is dead.  */
/* This should be used for experimental internal options */
PC_OPTION_INTERNAL(bool, nullcalls, "do not take over")
/* replace dynamorio_app_init & _start w/ empty functions */

OPTION_COMMAND_INTERNAL(
    uint, trace_threshold, 50U, "trace_threshold",
    { options->disable_traces = options->trace_threshold == 0; },
    "hot threshold value for trace creation", STATIC, OP_PCACHE_NOP)
/* Override the default threshold for hot trace selection. */
/* PR 200418: Traces are off by default for the BT API.  We now have
 * -enable_traces to turn them on; plus, -probe and -security
 * turn them on.
 * We mark as pcache-affecting though we have other explicit checks
 */
/* FIXME i#1551, i#1569: enable traces on ARM/AArch64 once we have them working */
OPTION_COMMAND(
    bool, disable_traces, IF_X86_ELSE(false, true), "disable_traces",
    {
        if (options->disable_traces) { /* else leave alone */
            DISABLE_TRACES(options);
        }
    },
    "disable trace creation (block fragments only)", STATIC, OP_PCACHE_GLOBAL)
/* FIXME i#1551, i#1569: enable traces on ARM/AArch64 once we have them working */
OPTION_COMMAND(
    bool, enable_traces, IF_X86_ELSE(true, false), "enable_traces",
    {
        if (options->enable_traces) { /* else leave alone */
            REENABLE_TRACES(options);
        }
    },
    "enable trace creation", STATIC, OP_PCACHE_GLOBAL)
OPTION_DEFAULT_INTERNAL(
    uint, trace_counter_on_delete, 0U,
    "trace head counter will be reset to this value upon trace deletion")

OPTION_DEFAULT(uint, max_elide_jmp, 16, "maximum direct jumps to elide in a basic block")
OPTION_DEFAULT(uint, max_elide_call, 16, "maximum direct calls to elide in a basic block")
OPTION_DEFAULT(bool, elide_back_jmps, true,
               "elide backward unconditional jumps in basic blocks?")
OPTION_DEFAULT(bool, elide_back_calls, true,
               "elide backward direct calls in basic blocks?")
/* Xref case 8163, if selfmod_max_writes is too small may cause problems
 * with pattern reverify from case 4020. Though if too big can cause issues
 * with max bb size (even 64 may be too big), xref case 7893. */
OPTION_DEFAULT(uint, selfmod_max_writes, 5, "maximum write instrs per selfmod fragment")
/* If this is too large, clients with heavyweight instrumentation hit the
 * "exceeded maximum size" failure.
 */
OPTION_DEFAULT(uint, max_bb_instrs, 256, "maximum instrs per basic block")
PC_OPTION_DEFAULT(bool, process_SEH_push, IF_RETURN_AFTER_CALL_ELSE(true, false),
                  "break bb's at an SEH push so we can see the frame pushed on in "
                  "interp, required for -borland_SEH_rct")
OPTION_DEFAULT_INTERNAL(
    bool, check_for_SEH_push, true,
    "extra debug build checking to ensure -process_SEH_push is catching "
    "all SEH frame pushes")

/* PR 361894: if no TLS available, we fall back to thread-private */
PC_OPTION_DEFAULT(bool, shared_bbs, IF_HAVE_TLS_ELSE(true, false),
                  "use thread-shared basic blocks")
/* Note that if we want traces off by default we would have to turn
 * off -shared_traces to avoid tripping over un-initialized ibl tables
 * PR 361894: if no TLS available, we fall back to thread-private
 */
/* FIXME i#1551, i#1569: enable traces on ARM/AArch64 once we have them working */
OPTION_COMMAND(
    bool, shared_traces, IF_HAVE_TLS_ELSE(IF_X86_ELSE(true, false), false),
    "shared_traces",
    {
        /* for -no_shared_traces, set options back to defaults for private
         * traces: */
        IF_NOT_X64_OR_ARM(options->private_ib_in_tls = options->shared_traces;)
        options->atomic_inlined_linking = options->shared_traces;
        options->shared_trace_ibl_routine = options->shared_traces;
        /* private on by default, shared off until proven stable FIXME */
        /* we prefer -no_indirect_stubs to inlining, though should actually
         * measure it: FIXME */
        if (!options->shared_traces && options->indirect_stubs)
            options->inline_trace_ibl = true;
        IF_NOT_X64(IF_WINDOWS(options->shared_fragment_shared_syscalls =
                                  (options->shared_traces && options->shared_syscalls);))
    },
    "use thread-shared traces", STATIC, OP_PCACHE_GLOBAL)

/* PR 361894: if no TLS available, we fall back to thread-private */
OPTION_COMMAND(
    bool, thread_private, IF_HAVE_TLS_ELSE(false, true), "thread_private",
    {
        options->shared_bbs = !options->thread_private;
        options->shared_traces = !options->thread_private;
        /* i#871: set code cache infinite for thread private as primary cache
         */
        options->finite_bb_cache = !options->thread_private;
        options->finite_trace_cache = !options->thread_private;
        if (options->thread_private && options->indirect_stubs) {
            IF_NOT_ARM(options->coarse_units = true); /* i#1575: coarse NYI on ARM */
        }
        IF_NOT_X64_OR_ARM(options->private_ib_in_tls = !options->thread_private;)
        options->atomic_inlined_linking = !options->thread_private;
        options->shared_trace_ibl_routine = !options->thread_private;
        /* we prefer -no_indirect_stubs to inlining, though should actually
         * measure it: FIXME */
        if (options->thread_private && options->indirect_stubs)
            options->inline_trace_ibl = true;
        IF_NOT_X64(
            IF_WINDOWS(options->shared_fragment_shared_syscalls =
                           (!options->thread_private && options->shared_syscalls)));
        /* If most stubs are private, turn on separate ones and pay
         * the cost of individual frees on thread exit (i#4334) for
         * more compact caches.  (ARM can't reach, so x86-only.)
         */
        IF_X86(options->separate_private_stubs = !options->thread_private);
        IF_X86(options->free_private_stubs = !options->thread_private);
    },
    "use thread-private code caches", STATIC, OP_PCACHE_GLOBAL)

OPTION_DEFAULT_INTERNAL(bool, remove_shared_trace_heads, true,
                        "remove a shared trace head replaced with a trace")

OPTION_DEFAULT(bool, remove_trace_components, false, "remove bb components of new traces")

OPTION_DEFAULT(bool, shared_deletion, true, "enable shared fragment deletion")
OPTION_DEFAULT(bool, syscalls_synch_flush, true,
               "syscalls are flush synch points (currently for shared_deletion only)")
OPTION_DEFAULT(uint, lazy_deletion_max_pending, 128,
               "maximum size of lazy shared deletion list before moving to normal list")

OPTION_DEFAULT(bool, free_unmapped_futures, true,
               "free futures on app mem dealloc (potential perf hit)")

/* Default TRUE as it's needed for shared_traces (which is on by default)
 * and for x64 (PR 244737, PR 215396)
 * PR 361894: if no TLS available, we fall back to thread-private
 */
/* FIXME: private_ib_in_tls option should go away once case 3701 has all
 * ibl using tls when any fragments are shared
 */
PC_OPTION_DEFAULT(bool, private_ib_in_tls,
                  IF_HAVE_TLS_ELSE(true, IF_X64_ELSE(true, false)),
                  "use tls for indirect branch slot in private caches")

OPTION_INTERNAL(bool, single_thread_in_DR, "only one thread in DR at a time")
/* deprecated: we have finer-grained synch that works now */

/* Due to ARM reachability complexities we only support local stubs there.
 * For x86, we avoid separate private stubs when they are rare due to shared
 * caches being on by default, to avoid having to walk and free individual fragments
 * in order to free the stubs on thread exit (i#4334).
 */
OPTION_DEFAULT(bool, separate_private_stubs, false,
               "place private direct exit stubs in a separate area from the code cache")

/* Due to ARM reachability complexities we only support local stubs */
OPTION_DEFAULT(bool, separate_shared_stubs, IF_X86_ELSE(true, false),
               "place shared direct exit stubs in a separate area from the code cache")

OPTION_DEFAULT(bool, free_private_stubs, false,
               "free separated private direct exit stubs when not pointed at")

/* FIXME Freeing shared stubs is currently an unsafe option due to a lack of
 * linking atomicity. (case 2081). */
OPTION_DEFAULT(bool, unsafe_free_shared_stubs, false,
               "free separated shared direct exit stubs when not pointed at")

/* XXX i#1611: for ARM, our far links go through the stub and hence can't
 * be shared with an unlinked fall-through.  If we switch to allowing
 * "ldr pc, [pc + X]" as an exit cti we can turn this back on for 32-bit ARM.
 */
OPTION_DEFAULT_INTERNAL(bool, cbr_single_stub, IF_X86_ELSE(true, false),
                        "both sides of a cbr share a single stub")

/* PR 210990: Improvement is in the noise for spec2k on P4, but is noticeable on
 * Core2, and on IIS on P4.  Note that this gets disabled if
 * coarse_units is on (PR 213262 covers supporting it there).
 */
/* XXX i#1611: For ARM, reachability concerns make it difficult to
 * avoid a stub unless we use "ldr pc, [r10+offs]" as an exit cti, which
 * complicates the code that handles exit ctis and doesn't work for A64.
 */
OPTION_COMMAND(
    bool, indirect_stubs, IF_X86_ELSE(false, true), "indirect_stubs",
    {
        /* we put inlining back in place if we have stubs, for private,
         * though should re-measure whether inlining is worthwhile */
        if (options->thread_private && options->indirect_stubs) {
            options->inline_trace_ibl = true;
            /* we also turn coarse on (xref PR 213262) */
            options->coarse_units = true;
        }
    },
    "use indirect stubs to keep source information", STATIC, OP_PCACHE_GLOBAL)

/* control inlining of fast path of indirect branch lookup routines */
/* NOTE : Since linking inline_indirect branches is not atomic (see bug 751)
 * don't turn this on (need atomic linking for trace building in a shared
 * cache) without turning on atomic_inlined_linking,
 * should be ok for traces since ?think? only need atomic unlinking
 * there (for flushing), reconsider esp. if we go to a shared trace cache
 */
OPTION_DEFAULT(bool, inline_bb_ibl, false, "inline head of ibl routine in basic blocks")
/* Default TRUE as it's needed for shared_traces (which is on by default) */
/* PR 361894: if no TLS available, we fall back to thread-private */
OPTION_DEFAULT(bool, atomic_inlined_linking, IF_HAVE_TLS_ELSE(true, false),
               "make linking of inlined_ibls atomic with respect to thread in the cache, "
               "required for inline_{bb,traces}_ibl with {bb,traces} being shared, cost "
               "is an extra 7 bytes per inlined stub (77 bytes instead of 70)")
/* Default FALSE since not supported for shared_traces (which is on by default) */
OPTION_DEFAULT(bool, inline_trace_ibl, false, "inline head of ibl routine in traces")

OPTION_DEFAULT(bool, shared_bb_ibt_tables, false, "use thread-shared BB IBT tables")

OPTION_DEFAULT(bool, shared_trace_ibt_tables, false, "use thread-shared trace IBT tables")

OPTION_DEFAULT(bool, ref_count_shared_ibt_tables, true,
               "use ref-counting to free thread-shared IBT tables prior to process exit")

/* PR 361894: if no TLS available, we fall back to thread-private */
OPTION_DEFAULT(bool, ibl_table_in_tls, IF_HAVE_TLS_ELSE(true, false),
               "use TLS to hold IBL table addresses & masks")

/* FIXME i#1551, i#1569: enable traces on ARM/AArch64 once we have them working */
OPTION_DEFAULT(bool, bb_ibl_targets, IF_X86_ELSE(false, true), "enable BB to BB IBL")

/* IBL code cannot target both single restore prefix and full prefix frags
 * simultaneously since the restore of %eax in the former case means that the
 * 2nd flags restore in the full prefix would be wrong. So if the BB table
 * is including trace targets, bb_single_restore_prefix and
 * trace_single_restore_prefix must be the same value. See options.c for
 * what is currently done for options compatibility.
 */
OPTION(bool, bb_ibt_table_includes_traces, "BB IBT tables holds trace targets also")

PC_OPTION(bool, bb_single_restore_prefix, "BBs use single restore prefixes")

OPTION(bool, trace_single_restore_prefix, "Traces use single restore prefixes")

OPTION_DEFAULT_INTERNAL(
    uint, rehash_unlinked_threshold, 100,
    "%-age of #unlinked entries to trigger a rehash of a shared BB IBT table")

OPTION_DEFAULT_INTERNAL(bool, rehash_unlinked_always, false,
                        "always rehash a shared BB IBT table when # unlinked entries > 0")

#ifdef SHARING_STUDY
OPTION_COMMAND_INTERNAL(
    bool, fragment_sharing_study, false, "fragment_sharing_study",
    {
        if (options->fragment_sharing_study) { /* else leave alone */
            options->shared_bbs = false;
            options->shared_traces = false;
            /* undo things that the default-on shared_traces turns on */
            IF_NOT_X64(IF_WINDOWS(options->shared_fragment_shared_syscalls = false;))
            IF_NOT_X64_OR_ARM(options->private_ib_in_tls = false;)
            options->shared_trace_ibl_routine = false;
            /* will work w/ wset but let's not clutter creation count stats */
            options->finite_bb_cache = false;
            options->finite_trace_cache = false;
        }
    },
    "counts duplication of bbs and traces among threads (requires all-private fragments)",
    STATIC, OP_PCACHE_NOP)
#endif

OPTION_COMMAND(
    bool, shared_bbs_only, false, "shared_bbs_only",
    {
        if (options->shared_bbs_only) { /* else leave alone */
            DISABLE_TRACES(options);
            options->shared_bbs = true;
            options->private_ib_in_tls = true;
        }
    },
    "Run in shared BBs, no traces mode", STATIC, OP_PCACHE_NOP)

/* control sharing of indirect branch lookup routines */
/* Default TRUE as it's needed for shared_traces (which is on by default) */
/* PR 361894: if no TLS available, we fall back to thread-private */
/* FIXME i#1551, i#1569: enable traces on ARM/AArch64 once we have them working */
OPTION_DEFAULT(bool, shared_trace_ibl_routine,
               IF_HAVE_TLS_ELSE(IF_X86_ELSE(true, false), false),
               "share ibl routine for traces")
OPTION_DEFAULT(bool, speculate_last_exit, false,
               "enable speculative linking of trace last IB exit")

OPTION_DEFAULT(uint, max_trace_bbs, 128, "maximum number of basic blocks in a trace")

/* FIXME i#3522: re-enable SELFPROT_DATA_RARE on linux */
OPTION_DEFAULT(
    uint, protect_mask,
    IF_STATIC_LIBRARY_ELSE(
        0x100 /*SELFPROT_GENCODE*/,
        /* XXX i#5383: Can we enable for M1 with the JIT_WRITE calls? */
        IF_MACOSA64_ELSE(0,
                         IF_WINDOWS_ELSE(0x101 /*SELFPROT_DATA_RARE|SELFPROT_GENCODE*/,
                                         0x100 /*SELFPROT_GENCODE*/))),
    "which memory regions to protect")
OPTION_INTERNAL(bool, single_privileged_thread,
                "suspend all other threads when one is out of cache")

/*  1 == HASH_FUNCTION_MULTIPLY_PHI */
OPTION_DEFAULT_INTERNAL(uint, alt_hash_func, 1,
                        "use to select alternate hashing functions for all fragment "
                        "tables except those that have in cache lookups")

OPTION_DEFAULT(
    uint, ibl_hash_func_offset, 0,
    /* Ignore LSB bits for ret and indjmp hashtables (use ibl_indcall_hash_offset
     * for indcall hashtables).
     * This may change the hash function distribution and for offsets
     * larger than 3 (4 on x64) will add an extra instruction to the IBL hit path.
     */
    "mask out lower bits in IBL table hash function")

/* PR 263331: call* targets on x64 are often 16-byte aligned so ignore LSB 4 */
OPTION_DEFAULT(uint, ibl_indcall_hash_offset, IF_X64_ELSE(4, 0),
               /* Ignore LSB bits for indcall hashtables. */
               "mask out lower bits in indcall IBL table hash function")

OPTION_DEFAULT_INTERNAL(
    uint, shared_bb_load,
    /* FIXME: since resizing is costly (no delete) this used to be up to 65 but that
     * hurt us lot (case 1677) when we hit a bad hash function distribution -
     * My current theory is that since modules addresses are 64KB
     * aligned we are doing bad on the 16-bit capacity.
     */
    55, "load factor percent for shared bb hashtable")
OPTION_DEFAULT_INTERNAL(uint, shared_trace_load,
                        /* not used for in-cache lookups */
                        55, "load factor percent for shared trace hashtable")
OPTION_DEFAULT_INTERNAL(uint, shared_future_load,
                        /* performance not critical, save some memory */
                        60, "load factor percent for shared future hashtable")

OPTION_DEFAULT(uint, shared_after_call_load,
               /* performance not critical */
               /* we use per-module tables despite the name of the option */
               80, "load factor percent for after call hashtables")

OPTION_DEFAULT(uint, global_rct_ind_br_load,
               /* performance not critical */
               /* we use per-module tables despite the name of the option */
               80, "load factor percent for global rct ind branch hashtable")

OPTION_DEFAULT_INTERNAL(uint, private_trace_load,
                        /* trace table is no longer used for in-cache lookups
                         * we can probably use higher than \alpha = 1/2
                         */
                        55, "load factor percent for private trace hashtables")

OPTION_DEFAULT(uint, private_ibl_targets_load,
               /* IBL tables are performance critical, so we use a smaller load.
                * we started with \alpha = 1/2,
                * 40 seemed to be the best tradeoff of memory & perf for
                * crafty, which is of course our primary nemesis.
                * Increasing to accommodate IIS for private tables.
                * FIXME: case 4902 this doesn't really control the effective load.
                */
               50, "load factor percent for private ibl target trace hashtables")

OPTION_DEFAULT(uint, private_bb_ibl_targets_load,
               /* BB IBL tables are not proven to be performance critical trying a 1/2
                */
               60, "load factor percent for private ibl hashtables targeting shared bbs")

OPTION_DEFAULT(uint, shared_ibt_table_trace_init, 7,
               "Shared trace shared IBT tables initial size, log_2 (in bits)")

OPTION_DEFAULT(uint, shared_ibt_table_bb_init, 7,
               "Shared BB shared IBT tables initial size, log_2 (in bits)")

OPTION_DEFAULT(uint, shared_ibt_table_trace_load, 50,
               "load factor percent for shared ibl hashtables targeting shared traces")

OPTION_DEFAULT(uint, shared_ibt_table_bb_load, 70,
               "load factor percent for shared ibl hashtables targeting shared bbs")

OPTION_DEFAULT(uint, coarse_htable_load,
               /* there is a separate table per module so we keep the load high */
               80, "load factor percent for all coarse module hashtables")

OPTION_DEFAULT(uint, coarse_th_htable_load,
               /* there is a separate table per module so we keep the load high */
               80, "load factor percent for all coarse module trace head hashtables")

OPTION_DEFAULT(uint, coarse_pclookup_htable_load,
               /* there is a separate table per module so we keep the load high */
               80, "load factor percent for all coarse module trace head hashtables")

/* FIXME: case 4814 currently disabled */
OPTION_DEFAULT(
    uint, bb_ibt_groom,
    /* Should either be 0 to disable grooming or be <= private_bb_ibl_targets_load */
    0, "groom factor percent for ibl hashtables targeting bb's")

OPTION_DEFAULT(uint, trace_ibt_groom,
               /* Should either be 0 to disable grooming or be <= private_ibl_targets_load
                * since traces are considered hot already, grooming the table may not work
                * as well here
                */
               0, "groom factor percent for ibl hashtables targeting traces")

/* for small table sizes resize is not an expensive operation and we start smaller */
OPTION_DEFAULT(uint, private_trace_ibl_targets_init, 7,
               "Trace IBL tables initial size, log_2 (in bits)")

OPTION_DEFAULT(uint, private_bb_ibl_targets_init, 6,
               "BB IBL tables initial size, log_2 (in bits)")

/* maximum size of IBL table - table is reset instead of resized when reaching load factor
 */
OPTION_DEFAULT(uint, private_trace_ibl_targets_max, 0, /* 0 for unlimited */
               "Trace IBL tables maximum size, log_2 (in bits)")

OPTION_DEFAULT(uint, private_bb_ibl_targets_max, 0, /* 0 for unlimited */
               "BB IBL tables maximum size, log_2 (in bits)")

OPTION_DEFAULT_INTERNAL(uint, private_bb_load,
                        /* Note there are usually no private bbs when using shared_bbs */
                        60, "load factor percent for private bb hashtables")
OPTION_DEFAULT_INTERNAL(uint, private_future_load,
                        /* performance not critical, save memory */
                        /* This table is suffering from the worst collisions */
                        65, "load factor percent for private future hashtables")

/* FIXME: remove this once we are happy with new rwlocks */
OPTION_DEFAULT_INTERNAL(bool, spin_yield_rwlock, false,
                        "use old spin-yield rwlock implementation")

OPTION_INTERNAL(bool, simulate_contention,
                "simulate lock contention for testing purposes only")

/* Virtual memory manager.
 * We assume that our reservations cover 99% of the usage, and that we do not
 * need to tune our sizes for standalone allocations where we would want 64K
 * units for Windows.  If we exceed the reservations we'll end up with less
 * efficient sizing, but that is worth the simpler and cleaner common case
 * sizing.  We save a lot of wasted alignment space by using a smaller
 * granularity (PR 415959, i#2575) and we avoid the complexity of adding guard
 * pages while maintaining larger-than-page sizing (i#2607).
 *
 * vmm_block_size may be adjusted by adjust_defaults_for_page_size().
 */
OPTION_DEFAULT(uint_size, vmm_block_size, 4 * 1024,
               "allocation unit for virtual memory manager")
/* initial_heap_unit_size may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, initial_heap_unit_size, 24 * 1024,
               "initial private heap unit size")
/* We avoid wasted space for every thread on UNIX for the
 * non-persistent heap which often stays under 12K (i#2575).
 */
/* initial_heap_nonpers_size may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, initial_heap_nonpers_size, IF_WINDOWS_ELSE(24, 12) * 1024,
               "initial private non-persistent heap unit size")
/* initial_global_heap_unit_size may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, initial_global_heap_unit_size, 24 * 1024,
               "initial global heap unit size")
/* if this is too small then once past the vm reservation we have too many
 * DR areas and subsequent problems with DR areas and allmem synch (i#369)
 */
OPTION_DEFAULT_INTERNAL(uint_size, max_heap_unit_size, 256 * 1024,
                        "maximum heap unit size")
/* heap_commit_increment may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, heap_commit_increment, 4 * 1024, "heap commit increment")
/* cache_commit_increment may be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, cache_commit_increment, 4 * 1024, "cache commit increment")

/* cache capacity control
 * FIXME: these are external for now while we study the right way to
 * tune them for server apps as well as desktop apps
 * Should we make then internal again once we're happy with the defaults?
 *
 * FIXME: unit params aren't that user-friendly -- there's an ordering required:
 * init < quadruple < max && init < upgrade < max
 * We complain if that's violated, but if you just set max we make you go and
 * set the others instead of forcing the others to be ==max.
 *
 * FIXME: now that we have cache commit-on-demand we should make the
 * private-configuration caches larger.  We could even get rid of the
 * fcache shifting.
 */
OPTION(uint_size, cache_bb_max, "max size of bb cache, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
/* for default configuration of all-shared we want a tiny bb cache for
 * our temp private bbs
 */
/* The 56K values below are to hit 64K with two 4K guard pages.
 * We no longer need to hit 64K since VMM blocks are now 4K, but we keep the
 * sizes to match historical values and to avoid i#4433.
 */
/* x64 does not support resizing individual cache units so start at the max. */
OPTION_DEFAULT(uint_size, cache_bb_unit_init, (IF_X64_ELSE(56, 4) * 1024),
               "initial bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_bb_unit_max, (56 * 1024),
               "maximum bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
/* w/ init at 4, we quadruple to 16 and then to 64 */
OPTION_DEFAULT(uint_size, cache_bb_unit_quadruple, (56 * 1024),
               "bb cache units are grown by 4X until this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

OPTION(uint_size, cache_trace_max, "max size of trace cache, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
/* x64 does not support resizing individual cache units so start at the max. */
OPTION_DEFAULT(uint_size, cache_trace_unit_init, (IF_X64_ELSE(56, 8) * 1024),
               "initial trace cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_trace_unit_max, (56 * 1024),
               "maximum trace cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_trace_unit_quadruple, (IF_X64_ELSE(56, 32) * 1024),
               "trace cache units are grown by 4X until this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

OPTION(uint_size, cache_shared_bb_max, "max size of shared bb cache, in KB or MB")
/* override the default shared bb fragment cache size */
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_bb_unit_init, (56 * 1024),
               /* FIXME: cannot handle resizing of cache setting to unit_max, FIXME:
                  should be 32*1024 */
               "initial shared bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
/* May be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, cache_shared_bb_unit_max, (56 * 1024),
               "maximum shared bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_bb_unit_quadruple,
               (56 * 1024), /* FIXME: should be 32*1024 */
               "shared bb cache units are grown by 4X until this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

OPTION(uint_size, cache_shared_trace_max, "max size of shared trace cache, in KB or MB")
/* override the default shared trace fragment cache size */
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_trace_unit_init, (56 * 1024),
               /* FIXME: cannot handle resizing of cache setting to unit_max, FIXME:
                  should be 32*1024 */
               "initial shared trace cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
/* May be adjusted by adjust_defaults_for_page_size(). */
OPTION_DEFAULT(uint_size, cache_shared_trace_unit_max, (56 * 1024),
               "maximum shared trace cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_trace_unit_quadruple,
               (56 * 1024), /* FIXME: should be 32*1024 */
               "shared trace cache units are grown by 4X until this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

OPTION(uint_size, cache_coarse_bb_max, "max size of coarse bb cache, in KB or MB")
/* override the default coarse bb fragment cache size */
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_coarse_bb_unit_init, (56 * 1024),
               /* FIXME: cannot handle resizing of cache setting to unit_max, FIXME:
                  should be 32*1024 */
               "initial coarse bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_coarse_bb_unit_max, (56 * 1024),
               "maximum coarse bb cache unit size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_coarse_bb_unit_quadruple,
               (56 * 1024), /* FIXME: should be 32*1024 */
               "coarse bb cache units are grown by 4X until this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

/* adaptive working set */
OPTION_DEFAULT(bool, finite_bb_cache, true, "adaptive working set bb cache management")
OPTION_DEFAULT(bool, finite_trace_cache, true,
               "adaptive working set trace cache management")
OPTION_DEFAULT(bool, finite_shared_bb_cache, false,
               "adaptive working set shared bb cache management")
OPTION_DEFAULT(bool, finite_shared_trace_cache, false,
               "adaptive working set shared trace cache management")
OPTION_DEFAULT(bool, finite_coarse_bb_cache, false,
               "adaptive working set shared bb cache management")
OPTION_DEFAULT(uint_size, cache_bb_unit_upgrade, (56 * 1024),
               "bb cache units are always upgraded to this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_trace_unit_upgrade, (56 * 1024),
               "trace cache units are always upgraded to this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_bb_unit_upgrade, (56 * 1024),
               "shared bb cache units are always upgraded to this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_shared_trace_unit_upgrade, (56 * 1024),
               "shared trace cache units are always upgraded to this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */
OPTION_DEFAULT(uint_size, cache_coarse_bb_unit_upgrade, (56 * 1024),
               "shared coarse cache units are always upgraded to this size, in KB or MB")
/* default size is in Kilobytes, Examples: 4, 4k, 4m, or 0 for unlimited */

OPTION_DEFAULT(uint, cache_bb_regen, 10, "#regen per #replaced ratio for sizing bb cache")
OPTION_DEFAULT(uint, cache_bb_replace, 50,
               "#regen per #replaced ratio for sizing bb cache")
OPTION_DEFAULT(uint, cache_trace_regen, 10,
               "#regen per #replaced ratio for sizing trace cache")
OPTION_DEFAULT(uint, cache_trace_replace, 50,
               "#regen per #replaced ratio for sizing trace cache")
OPTION_DEFAULT(uint, cache_shared_bb_regen, 20,
               "#regen per #replaced ratio for sizing shared bb cache")
OPTION_DEFAULT(uint, cache_shared_bb_replace, 100,
               /* doesn't mean much for shared sizing, so default 100 makes
                * regen param a percentage */
               "#regen per #replaced ratio for sizing shared bb cache")
OPTION_DEFAULT(uint, cache_shared_trace_regen, 10,
               "#regen per #replaced ratio for sizing shared trace cache")
OPTION_DEFAULT(uint, cache_shared_trace_replace, 100,
               /* doesn't mean much for shared sizing, so default 100 makes
                * regen param a percentage */
               "#regen per #replaced ratio for sizing shared trace cache")
OPTION_DEFAULT(uint, cache_coarse_bb_regen, 20,
               "#regen per #replaced ratio for sizing shared coarse cache")
OPTION_DEFAULT(uint, cache_coarse_bb_replace, 100,
               /* doesn't mean much for shared sizing, so default 100 makes
                * regen param a percentage */
               "#regen per #replaced ratio for sizing shared coarse cache")

OPTION_DEFAULT(uint, cache_trace_align, 8, "alignment of trace cache slots")
OPTION_DEFAULT(uint, cache_bb_align, 4, "alignment of bb cache slots")
OPTION_DEFAULT(uint, cache_coarse_align, 1, "alignment of coarse bb cache slots")

OPTION_DEFAULT(uint, ro2sandbox_threshold, 10,
               "#write faults in a region before switching to sandboxing, 0 to disable")
OPTION_DEFAULT(
    uint, sandbox2ro_threshold, 20,
    "#executions in a sandboxed region before switching to page prot, 0 to disable")

OPTION_COMMAND(
    bool, sandbox_writable, false, "sandbox_writable",
    {
        if (options->sandbox_writable) {
            options->sandbox2ro_threshold = 0;
        }
    },
    "always sandbox writable regions", STATIC, OP_PCACHE_GLOBAL)

/* FIXME: Do we want to turn this on by default?  If we do end up
 * making this the default for a single process only, we might
 * want to make it OP_PCACHE_NOP.
 */
OPTION_COMMAND(
    bool, sandbox_non_text, false, "sandbox_non_text",
    {
        if (options->sandbox_non_text) {
            options->sandbox2ro_threshold = 0;
        }
    },
    "always sandbox non-text writable regions", STATIC, OP_PCACHE_GLOBAL)

/* FIXME: separate for bb and trace shared caches? */
OPTION_DEFAULT(bool, cache_shared_free_list, true,
               "use size-separated free lists to manage empty shared cache slots")

/* FIXME i#1674: enable on ARM once bugs are fixed, along with all the
 * reset_* trigger options as well.
 */
OPTION_COMMAND(
    bool, enable_reset, IF_X86_ELSE(true, false), "enable_reset",
    {
        if (!options->enable_reset) {
            DISABLE_RESET(options);
        }
    },
    "separate persistent memory from non-persistent for resets", STATIC, OP_PCACHE_NOP)

OPTION_DEFAULT_INTERNAL(uint, reset_at_fragment_count, 0,
                        "reset all caches at a certain fragment count")
OPTION(uint, reset_at_nth_thread,
       "reset all caches when the nth thread is explicitly created")
/* FIXME - is potentially using up all the os allocation leaving nothing for the
 * app, however that's prob. better then us spinning (xref 9145). It would be nice
 * to switch back to resetting when the address space is getting low, but there's
 * no easy way to figure that out. */
OPTION_DEFAULT(
    bool, switch_to_os_at_vmm_reset_limit, true,
    "if we hit the reset_at_vmm_*_limit switch to requesting from the os (so we'll "
    "only actually reset once the os is out and we're at the limit)")
OPTION_DEFAULT(bool, reset_at_switch_to_os_at_vmm_limit,
               IF_X86_ELSE(true,
                           false), /* i#1674: re-enable on ARM once xl8 bugs are fixed */
               "schedule a reset the first (and only the first) time we switch to the os "
               "allocations from -switch_to_os_at_vmm_reset_limit above")
OPTION_DEFAULT(
    uint, reset_at_vmm_percent_free_limit,
    IF_X86_ELSE(10, 0), /* i#1674: re-enable on ARM once xl8 bugs are fixed */
    "reset all when vmm heap % free is < reset_at_vmm_percent_free (0 disables)")
OPTION_DEFAULT(
    uint_size, reset_at_vmm_free_limit, 0,
    "reset all when vmm heap has less then reset_at_vmm_free free memory remaining")
OPTION_DEFAULT(
    uint, report_reset_vmm_threshold, 3,
    "syslog one thrash warning message after this many resets at low vmm heap free")
OPTION_DEFAULT(bool, reset_at_vmm_full,
               IF_X86_ELSE(true,
                           false), /* i#1674: re-enable on ARM once xl8 bugs are fixed */
               "reset all caches the first time vmm heap runs out of space")
OPTION_DEFAULT(uint, reset_at_commit_percent_free_limit, 0,
               "reset all less than this % of the commit limit remains free (0 disables)")
OPTION_DEFAULT(uint_size, reset_at_commit_free_limit,
               IF_X86_ELSE((32 * 1024 * 1024),
                           0), /* i#1674: re-enable once ARM bugs fixed */
               "reset all when less then this much free committable memory remains")
OPTION_DEFAULT(uint, report_reset_commit_threshold, 3,
               "syslog one thrash warning message after this many resets at low commit")
OPTION_DEFAULT(uint, reset_every_nth_pending,
               IF_X86_ELSE(35, 0), /* i#1674: re-enable on ARM once xl8 bugs are fixed */
               "reset all caches when pending deletion has this many entries")
/* the reset-by-unit options focus on filled units and not created units
 * to avoid being triggered by new, empty, private units for new threads
 */
OPTION(uint, reset_at_nth_bb_unit,
       "reset all caches once, when the nth new bb cache unit is created/reused")
OPTION(uint, reset_at_nth_trace_unit,
       "reset all caches once, when the nth new trace cache unit is created/reused")
/* these options essentially put a ceiling on the size of the cache */
OPTION(uint, reset_every_nth_bb_unit,
       "reset all caches every nth bb cache unit that is created/reused")
OPTION(uint, reset_every_nth_trace_unit,
       "reset all caches every nth trace cache unit that is created/reused")

/* virtual memory management */
/* See case 1990 for examples where we have to fight for virtual address space with the
 * app */
/* FIXME: due to incomplete implementation for detaching we will leave memory behind */
OPTION_DEFAULT_INTERNAL(
    bool, skip_out_of_vm_reserve_curiosity, false,
    "skip the assert curiosity on out of vm_reserve (for regression tests)")
OPTION_DEFAULT(bool, vm_reserve, true, "reserve virtual memory")
OPTION_DEFAULT(uint_size, vm_size,
               /* The 64-bit default is 1G instead of the full 32-bit-reachable
                * 2G to allow for -vm_base_near_app to reduce overheads.
                * If this is set to 2G, -vm_base_near_app will always fail.
                */
               /* TODO i#3570: Add support for private loading inside the vm_size
                * region so Windows can support a 2G size.
                */
               IF_X64_ELSE(IF_WINDOWS_ELSE(512, 1024UL), 128) * 1024 * 1024,
               "capacity of virtual memory region reserved (maximum supported is "
               "512MB for 32-bit and 2GB for 64-bit) for code and reachable heap")
OPTION_DEFAULT(uint_size, vmheap_size, IF_X64_ELSE(8192ULL, 128) * 1024 * 1024,
               /* XXX: default value is currently not good enough for 32-bit sqlserver,
                * for which we need more than 256MB.
                */
               "capacity of virtual memory region reserved for unreachable heap")
#ifdef WINDOWS
OPTION_DEFAULT(uint_size, vmheap_size_wow64, 128 * 1024 * 1024,
               /* XXX: default value is currently not good enough for 32-bit sqlserver,
                * for which we need more than 256MB.
                */
               "capacity of virtual memory region reserved for unreachable heap "
               "on WoW64 processes")
#endif
/* We hardcode an address in the mmap_text region here, but verify via
 * in vmk_init().
 * For Linux we start higher to avoid limiting the brk (i#766), but with our
 * new default -vm_size of 0x20000000 we want to stay below our various
 * preferred addresses of 0x7xxx0000 so we keep the base plus offset plus
 * size below that.
 * For a 64-bit process on MacOS __PAGEZERO takes up the first 4GB by default.
 * We ignore this for x64 if -vm_base_near_app and the app is far away.
 */
OPTION_DEFAULT(uint_addr, vm_base,
               IF_VMX86_ELSE(IF_X64_ELSE(0x40000000, 0x10800000),
                             IF_WINDOWS_ELSE(0x16000000,
                                             IF_MACOS_ELSE(IF_X64_ELSE(0x120000000,
                                                                       0x3f000000),
                                                           0x3f000000))),
               "preferred base address hint for reachable code+heap")
/* FIXME: we need to find a good location with no conflict with DLLs or apps allocations
 */
OPTION_DEFAULT(uint_addr, vm_max_offset,
               IF_VMX86_ELSE(IF_X64_ELSE(0x18000000, 0x05800000), 0x10000000),
               "base address maximum random jitter")
OPTION_DEFAULT(bool, vm_allow_not_at_base, true,
               "if we can't allocate vm heap at "
               "preferred base (plus random jitter) allow the os to choose where to "
               "place it instead of dying")
OPTION_DEFAULT(bool, vm_allow_smaller, true,
               "if we can't allocate vm heap of "
               "requested size, try smaller sizes instead of dying")
OPTION_DEFAULT(bool, vm_base_near_app, true,
               "allocate vm region near the app if possible (if not, if "
               "-vm_allow_not_at_base, will try elsewhere)")
#ifdef X64
/* We prefer low addresses in general, and only need this option if it's
 * an absolute requirement (XXX i#829: it is required for mixed-mode).
 */
OPTION_DEFAULT(bool, heap_in_lower_4GB, false,
               "on 64bit request that the dr heap be allocated entirely within the "
               "lower 4GB of address space so that it can be accessed directly as a "
               "32bit address. See PR 215395.  Requires -reachable_heap.")
/* By default we separate heap from code and do not require reachability for heap. */
OPTION_DEFAULT(bool, reachable_heap, false,
               "guarantee that all heap memory is 32-bit-displacement "
               "reachable from the code cache.")
/* i#3570: For static DR we do not guarantee reachability. */
OPTION_DEFAULT(bool, reachable_client, IF_STATIC_LIBRARY_ELSE(false, true),
               "guarantee that clients are reachable from the code cache.")
#endif
/* XXX i#3566: Support for W^X has some current limitations:
 * + It is not implemented for Windows or Mac.
 * + Pcaches are not supported.
 * + -native_exec_list is not supported.
 * + dr_nonheap_alloc(rwx) is not supported.
 * + DR_ALLOC_CACHE_REACHABLE is not supported.
 *   Clients using other non-vmcode sources of +wx memory will also not comply.
 */
OPTION_DEFAULT(bool, satisfy_w_xor_x, false,
               "avoids ever allocating memory that is both writable and executable.")
/* FIXME: the lower 16 bits are ignored - so this here gives us
 * 12bits of randomness.  Could make it larger if we verify as
 * collision free the whole range [vm_base, * vm_base+vm_size+vm_max_offset)
 */
OPTION_INTERNAL(bool, vm_use_last, "use the vm reservation only as a last resort")

OPTION(uint, silent_oom_mask,
       /* a mask of the oom_source_t constants, usually 12 ==
        * (OOM_COMMIT | OOM_EXTEND) on commit limit either when
        * system running out of pagefile or process hitting job limit
        */
       "silently die when out of memory")

/* FIXME: case 6919 forcing a hardcoded name in the core, this
 * should rather go into a configuration file.  Since
 * per-executable no real need for an append list. */
OPTION_DEFAULT(liststring_t, silent_commit_oom_list, OPTION_STRING("wmiprvse.exe"),
               "silently die on reachinig commit limit in these ;-separated executables")

OPTION_DEFAULT(uint_time, oom_timeout, 5 * 1000, /* 5s */
               /* 5 second x 2 -> adds at least 10 seconds before we terminate
                * when out of memory, but gives us a chance to not die */
               /* case 2294 pagefile resize, or case 7032 where we hope that
                * a memory hog on the machine would die by the time we retry.
                * Applies only to committed memory, for reservations sleeping
                * is futile (other than artificial ballooning).
                *
                * Note: Two of these timeouts on the same thread's request
                * and we'll terminate - we will also try freeing up our own
                * memory after the first timeout.
                */
               "short sleep (in ms) and retry after a commit failure")

/* The follow children options control when we inject into a child. We inject if
 * any one of the three says we should, see their descriptions for more details. */
DYNAMIC_OPTION_DEFAULT(bool, follow_children, true,
                       "inject into all spawned processes unless preinjector is set up "
                       "to inject into them or they have app-specific RUNUNDER_OFF")
/* not dynamic do to interactions with -early_inject */
OPTION_DEFAULT(bool, follow_systemwide, true,
               "inject into all spawned processes that are configured to run under dr "
               "(app specific RUNUNDER_ON, or no app specific and RUNUNDER_ALL in the "
               "global key), dangerous without either -early_inject or "
               "-block_mod_load_list_default preventing double injection")
DYNAMIC_OPTION_DEFAULT(
    bool, follow_explicit_children, true,
    "inject into all spawned processes that have app-specific RUNUNDER_EXPLICIT")

/* XXX: do we want to make any of the -early_inject* options dynamic?
 * if so be sure we update os.c:early_inject_location on change etc. */
/* XXX i#47: for Linux, we can't easily have this option on by default as
 * code like get_application_short_name() called from drpreload before
 * even _init is run needs to have a non-early default.
 * Thus we turn this on in privload_early_inject.
 */
/* On Windows this does *not* imply early injection anymore: it just enables control
 * over where to inject via a hook and alternate injection methods, rather than using
 * the old thread injection.
 * XXX: Clean up by removing this option and thread injection completely?
 */
OPTION_COMMAND(
    bool, early_inject, IF_UNIX_ELSE(false /*see above*/, true), "early_inject",
    {
        if (options->early_inject) {
            /* i#1004: we need to emulate the brk for early injection */
            IF_UNIX(options->emulate_brk = true;)
        }
    },
    "inject early", STATIC, OP_PCACHE_GLOBAL)
/* To support cross-arch follow-children injection we need to use the map option. */
OPTION_DEFAULT(bool, early_inject_map, true, "inject earliest via map")
/* See enum definition is os_shared.h for notes on what works with which
 * os version.  Our default is late injection to make it easier on clients
 * (as noted in i#980, we don't want to be too early for a private kernel32).
 */
OPTION_DEFAULT(uint, early_inject_location, 8 /* INJECT_LOCATION_ThreadStart */,
               "where to hook for early_injection.  Use 5 =="
               "INJECT_LOCATION_KiUserApcdefault for earliest injection; use "
               "4 == INJECT_LOCATION_LdrDefault for easier-but-still-early.")
OPTION_DEFAULT(uint_addr, early_inject_address, 0,
               "specify the address to hook at for INJECT_LOCATION_LdrCustom")
#ifdef WINDOWS /* probably the surrounding options should also be under this ifdef */
OPTION_DEFAULT(
    pathstring_t, early_inject_helper_dll, OPTION_STRING(INJECT_HELPER_DLL1_NAME),
    "path to 1st early inject helper dll that is used to auto find LdrpLoadImportModule")
OPTION_DEFAULT(pathstring_t, early_inject_helper_name,
               OPTION_STRING(INJECT_HELPER_DLL2_NAME),
               "PE name of 2nd early inject helper dll that is used to auto find "
               "LdrpLoadImportModule")
#endif
OPTION_DEFAULT_INTERNAL(
    bool, early_inject_stress_helpers, false,
    "When early injected and using early_inject_location LdprLoadImportModule, don't use "
    "parent's address, instead always use helper dlls to find it")
/* FIXME - won't work till we figure out how to get the process parameters
 * in maybe_inject_into_proccess() in os.c (see notes there) */
OPTION_DEFAULT(bool, inject_at_create_process, false,
               "inject at post create process instead of create first thread, requires "
               "early injection")
/* Separated from above option since on Vista+ we have to inject at create
 * process (there is no separate create first thread).  On vista+ proc
 * params are available at create process so everything works out. */
OPTION_DEFAULT(
    bool, vista_inject_at_create_process, true,
    "if os version is vista+, inject at post create (requires early injection)")

OPTION_DEFAULT(bool, inject_primary, false,
               /* case 9347 - we may leave early threads as unknown */
               "check and wait for injection in the primary thread")
#ifdef UNIX
/* Should normally only be on if -early_inject is on */
OPTION_DEFAULT(bool, emulate_brk, false, "i#1004: emulate brk for early injection")
#endif

/* options for controlling the synch_with_* routines */
OPTION_DEFAULT(uint, synch_thread_max_loops, 10000,
               "max number of wait loops in "
               "synch_with_thread before we give up (UINT_MAX loops forever)")
OPTION_DEFAULT(uint, synch_all_threads_max_loops, 10000,
               "max number of wait loops "
               "in synch_with_all_threads before we give up (UINT_MAX loops forever)")
OPTION_DEFAULT(bool, synch_thread_sleep_UP, true,
               "for uni-proc machines : if true "
               "use sleep in synch_with_* wait loops instead of yield")
OPTION_DEFAULT(bool, synch_thread_sleep_MP, true,
               "for multi-proc machines : if "
               "true use sleep in synch_with_* wait loops instead of yield")
OPTION_DEFAULT(uint_time, synch_with_sleep_time, 5,
               "time in ms to sleep for each "
               "wait loop in synch_with_* routines")
#ifdef WINDOWS
/* FIXME - only an option since late in the release cycle - should always be on */
OPTION_DEFAULT(
    bool, suspend_on_synch_failure_for_app_suspend, true,
    "if we fail "
    "to synch with a thread for an app suspend, suspend anyways to preserved the "
    "apps suspend count")
#endif

/* see in case 2520 why this is off by default for Windows */
OPTION_DEFAULT(bool, ignore_syscalls, IF_WINDOWS_ELSE(false, true),
               "ignore system calls that do not need to be intercepted")
/* Whether we inline ignoreable syscalls inside of bbs (xref PR 307284) */
OPTION_DEFAULT(bool, inline_ignored_syscalls, true,
               "inline ignored system calls in the middle of bbs")
#ifdef LINUX
OPTION_DEFAULT(bool, hook_vsyscall, true, "hook vdso vsyscall if possible")
/* PR 356503: workaround to allow clients to make syscalls */
OPTION_ALIAS(sysenter_is_int80, hook_vsyscall, false, STATIC, OP_PCACHE_GLOBAL)
OPTION_DEFAULT(bool, disable_rseq, false,
               "cause the restartable sequence SYS_rseq "
               "system call to return -ENOSYS as a workaround for rseq features not "
               "supportable by DR")
#endif
#ifdef UNIX
OPTION_DEFAULT(bool, restart_syscalls, true,
               "restart appropriate syscalls when interrupted by a signal")
#endif

/* These should be made internal when sufficiently tested */
#if defined(WINDOWS) || defined(MACOS64)
/* We mark as pcache-affecting though we have other explicit checks */
PC_OPTION_DEFAULT(uint, tls_align,
                  IF_WINDOWS_ELSE(1 /* case 6770: for disabling alignment */, 0),
                  /* 0 - use processor cache line */
                  /* 1, 2, 4 - no alignment
                   * 32 - Pentium III, Pentium M cache line
                   * 64 - Pentium 4 cache line
                   */
                  /* XXX: if we ever change our -tls_align default from 1 we should
                   * consider implications on platform-independence of persisted caches
                   */
                  "TLS slots preferred alignment")
#endif
#ifdef WINDOWS
/* FIXME There's gotta be a better name for this. */
OPTION_DEFAULT(bool, ignore_syscalls_follow_sysenter, true,
               "for ignore_syscalls, continue interp after the sysenter")
/* Optimize syscall handling for syscalls that don't need to be intercepted
 * by DR by executing them using shared syscall. */
OPTION_DEFAULT(
    bool, shared_syscalls, true,
    "syscalls that do not need to be intercepted are executed by shared syscall")

/* Default TRUE as it's needed for shared_traces (which is on by default) */
/* PR 361894: if no TLS available, we fall back to thread-private */
OPTION_DEFAULT(bool, shared_fragment_shared_syscalls, IF_HAVE_TLS_ELSE(true, false),
               "enable fragments that use shared syscall to be share-able")

/* Optimize shared syscall handling by using a faster code sequence if
 * possible. This currently works only w/-disable_traces. */
OPTION_INTERNAL(bool, shared_syscalls_fastpath, "use a faster version of shared syscall")
/* This option only applies when shared_syscalls is 'true'. 'true' means that
 * only ignorable syscalls can be optimized. 'false' means that a much broader
 * set of syscalls can be optimized -- any syscall that DR doesn't need to
 * intercept, as identified in the syscall_requires_action array in win32/os.c.
 */
OPTION_INTERNAL(bool, shared_eq_ignore,
                "use ignorable syscall classification for shared_syscalls")

/* We mark as pcache-affecting though we have other explicit checks */
PC_OPTION_DEFAULT(uint, tls_flags, 1 | 2, /* TLS_FLAG_BITMAP_TOP_DOWN |
                                           * TLS_FLAG_CACHE_LINE_START */
                  "TLS allocation choices")
PC_OPTION_DEFAULT(bool, alt_teb_tls, true,
                  "Use other parts of the TEB for TLS once out of real TLS slots")
#endif /* WINDOWS */

/* i#2089: whether to use a special safe read of a magic field to determine
 * whether a thread's TLS is initialized yet, on x86.
 * XXX: we plan to remove this once we're sure it's stable.
 */
OPTION_DEFAULT_INTERNAL(bool, safe_read_tls_init, IF_LINUX_ELSE(true, false),
                        "use a safe read to identify uninit TLS")

OPTION_DEFAULT(bool, guard_pages, true,
               "add guard pages to all thread-shared vmm allocations; if disabled, also "
               "disables -per_thread_guard_pages")
OPTION_DEFAULT(
    bool, per_thread_guard_pages, true,
    "add guard pages to all thread-private vmm allocations, if -guard_pages is also on")
/* Today we support just one stack guard page.  We may want to turn this
 * option into a number in the future to catch larger strides beyond TOS.
 * There are problems on Windows where the PAGE_GUARD pages must be used, yet
 * the kernel's automated stack expansion does not do the right thing vs
 * our -vm_reserve.
 */
OPTION_DEFAULT(bool, stack_guard_pages, IF_WINDOWS_ELSE(false, true),
               "add guard pages to detect stack overflow")

#ifdef PROGRAM_SHEPHERDING
/* PR 200418: -security_api just turns on the bits of -security needed for the
 * Memory Firewall API */
OPTION_COMMAND_INTERNAL(
    bool, security_api, false, "security_api",
    {
        if (options->security_api) {
            ENABLE_SECURITY(options);
        }
    },
    "enable Security API", STATIC, OP_PCACHE_NOP)

/* PR 200418: program shepherding is now runtime-option-controlled. Note - not
 * option_command_internal so that we can use for release builds. */
OPTION_COMMAND(
    bool, security, false, "security",
    {
        if (options->security) {

            options->diagnostics = true;

            /* xref PR 232126 */
            options->syslog_mask = SYSLOG_ALL_NOVERBOSE;
            options->syslog_init = true;
            IF_INTERNAL(options->syslog_internal_mask = SYSLOG_ALL;)

            /* We used to have -use_moduledb by default (disabled with
             * -staged).
             */
            ENABLE_SECURITY(options);

            /* memory wins over gcc/gap perf issues (PR 326815)
             * (ENABLE_SECURITY turns on -indirect_stubs for us)
             */
            options->coarse_units = true;
        }
    },
    "enable Memory Firewall security checking", STATIC, OP_PCACHE_NOP)

/* attack handling options */
DYNAMIC_PCL_OPTION(bool, detect_mode,
                   "only report security violations - will execute attackers code!")
OPTION_COMMAND(
    uint, detect_mode_max, 0, "detect_mode_max", { options->detect_mode = true; },
    "max number of security violations to allow in detect_mode - will revert to "
    "next higher-priority handling option after the max",
    DYNAMIC, OP_PCACHE_NOP)
DYNAMIC_OPTION(bool, diagnose_violation_mode,
               "on a security violations, report whether a trampoline")
DYNAMIC_OPTION_DEFAULT(
    uint, report_max, 20,
    "max number of security violations to report, (0 is infinitely many)")

/* alternatives to kill application */
DYNAMIC_OPTION(bool, kill_thread,
               "kill offending thread only, WARNING: application may hang")
OPTION_COMMAND(
    uint, kill_thread_max, 10, "kill_thread_max", { options->kill_thread = true; },
    "max number of threads to kill before killing process", DYNAMIC, OP_PCACHE_NOP)
DYNAMIC_OPTION(bool, throw_exception,
               "throw exception on security violations, WARNING: application may die")
OPTION_COMMAND(
    uint, throw_exception_max, 10, "throw_exception_max",
    { options->throw_exception = true; },
    "max number of exceptions before killing thread or process", DYNAMIC, OP_PCACHE_NOP)
DYNAMIC_OPTION_DEFAULT(uint, throw_exception_max_per_thread, 10,
                       "max number of exceptions per single thread")
DYNAMIC_OPTION(uint_time, timeout, "timeout value to throttle down an attack")
/* FIXME: should this apply to the whole process or the attacked thread only? */
#    ifdef SIMULATE_ATTACK
DYNAMIC_OPTION_DEFAULT(pathstring_t, simulate_at, EMPTY_STRING,
                       "fragment count list for simulated attacks")
#    endif

/* case 280: remove futureexec areas for selfmod regions.
 * Disabled for now since -sandbox2ro_threshold invalidates its assumptions
 * (case 8167)
 */
OPTION_DEFAULT(bool, selfmod_futureexec, true,
               "leave selfmod areas on the future-exec list")

/* our default policies --- true by default, so you'd use no_ to turn them off
 * example: -no_executable_if_flush
 * note that if_flush and if_alloc use the future list, and so their regions
 * are considered executable until de-allocated -- even if written to!
 * N.B.: any changes in defaults must also change the -X command below!
 */
/* N.B.: case 9799: any changes in policy or exemption default values may
 * require changing from PC_ to PCL_, and changing whether we mark modules as
 * exempted!
 */
OPTION(bool, code_origins, "check that code origins meet security policies")
#    ifdef WINDOWS
PC_OPTION_DEFAULT(bool, executable_if_flush, true,
                  "allow execution after a region has been NtFlushInstructionCache-d")
PC_OPTION(bool, executable_after_load,
          "allow execution from region marked x but modified during load time (normal "
          "behavior for relocation or rebinding)")
PC_OPTION_DEFAULT(bool, emulate_IAT_writes, true, "keep IAT non-w, emulate writes there")
PC_OPTION_DEFAULT(
    bool, unsafe_ignore_IAT_writes, false,
    "ignore IAT writes by the loader, assuming nothing else writes at the same time")
#    endif
PC_OPTION_DEFAULT(bool, executable_if_rx_text, true,
                  "allow execution from any rx portion of the text section, subsumes "
                  "-executable_after_load")
PC_OPTION_DEFAULT(bool, executable_if_alloc, true,
                  "allow execution from certain regions marked x at allocation time")
PC_OPTION_DEFAULT(bool, executable_if_trampoline, true,
                  "allow execution from pattern-matched trampoline blocks")
PC_OPTION_DEFAULT(bool, executable_if_hook, true,
                  "allow execution from text section briefly marked rwx")

/* specific trampoline exemptions */
PC_OPTION_DEFAULT(bool, trampoline_dirjmp, true, "allow direct jmp trampoline")
PC_OPTION_DEFAULT(bool, trampoline_dircall, true, "allow direct call trampoline")
/* not needed w/ native_exec but may be needed in the future */
PC_OPTION(bool, trampoline_com_ret, "allow .NET COM method table ret trampoline")
/* allow simple hook displacement of original code */
PC_OPTION_DEFAULT(bool, trampoline_displaced_code, true,
                  "allow hook-displaced code trampoline")
PC_OPTION_DEFAULT(bool, executable_if_driver, true,
                  "allow execution from a kernel-mode address (case 9022)")
PC_OPTION_DEFAULT(
    bool, driver_rct, true,
    /* marked as VM_DRIVER_ADDRESS */
    "allow any RCT if source is from a kernel-mode address (case 9022/9096)")

/* methods to loosen policies */
PCL_OPTION(
    bool, executable_if_text,
    "allow execution from text sections of modules, subsumes -executable_if_rx_text")
PCL_OPTION(bool, executable_if_dot_data, "allow execution from .data sections of modules")
PCL_OPTION(bool, executable_if_dot_data_x,
           "allow execution from .data sections of modulesif marked x")
/* ..x means care about execute permission, but not read or write;
 * .-x means care about execute with no write permission. See case 3287.
 */
PCL_OPTION(bool, executable_if_x, "allow execution from regions marked ..x")
PCL_OPTION(bool, executable_if_rx, "allow execution from regions marked .-x")
PCL_OPTION(bool, executable_if_image,
           "allow execution from any mapped section from an executable or library image")
OPTION(bool, executable_stack, "allow execution from the stack")
OPTION(bool, executable_heap, "allow execution from the heap")
/* Obfuscated options how to suppress security violations of types A and B */
OPTION_ALIAS(A, executable_stack, true, STATIC, OP_PCACHE_NOP /*no effect on images*/)
OPTION_ALIAS(B, executable_heap, true, STATIC, OP_PCACHE_NOP /*no effect on images*/)
/* each exempt list has a corresponding boolean for easy disabling */
PCL_OPTION_DEFAULT(bool, exempt_text, true, "allow execution from exempt text sections")
PCL_OPTION_DEFAULT(liststring_t, exempt_text_list, EMPTY_STRING,
                   "allow execution from text sections of these ;-separated modules")
PC_OPTION_DEFAULT(liststring_t, exempt_mapped_image_text_default_list,
                  /* case 9385 - loaded in unknown thread  */
                  OPTION_STRING("LVPrcInj.dll"),
                  "allow execution from text sections in MEM_IMAGE mappings"
                  " of these ;-separated modules, default")
PCL_OPTION_DEFAULT(liststring_t, exempt_mapped_image_text_list, EMPTY_STRING,
                   "allow execution from text sections in MEM_IMAGE mappings"
                   " of these ;-separated modules, append")

PCL_OPTION_DEFAULT(bool, exempt_dot_data, true,
                   "allow execution from exempt .data sections")
/* xref case 4244 on SM2USER.dll */
/* FIXME case 9799: since default not split out, anything on this list
 * by default will not have shared pcaches for any process w/ ANY non-default
 * exemption lists */
PCL_OPTION_DEFAULT(liststring_t, exempt_dot_data_list, OPTION_STRING("SM2USER.dll"),
                   "allow execution from .data sections of these ;-separated modules")
PCL_OPTION_DEFAULT(bool, exempt_dot_data_x, true,
                   "allow execution from exempt .data sections if marked x")
/* Case 7345: allow all kdb*.dlls for shark; later on (porpoise?) implement
 * -executable_if_dot_data_rx, turn it on by default and set this exempt
 * list to empty.
 */
/* FIXME case 9799: since default not split out, anything on this list
 * by default will not have shared pcaches for any process w/ ANY non-default
 * exemption lists */
PCL_OPTION_DEFAULT(
    liststring_t, exempt_dot_data_x_list,
    OPTION_STRING("kbd??.dll;kbd???.dll;kbd????.dll;kbd?????.dll"),
    "allow execution from .data sections of these ;-separated modules if marked x")
PCL_OPTION_DEFAULT(bool, exempt_image, true, "allow execution from exempt image modules")
PCL_OPTION_DEFAULT(
    liststring_t, exempt_image_list, EMPTY_STRING,
    "allow execution from anywhere in the image of these ;-separated modules")
OPTION_DEFAULT(bool, exempt_dll2heap, true,
               "allow execution in heap first targeted by exempt modules")
OPTION_DEFAULT(liststring_t, exempt_dll2heap_list, EMPTY_STRING,
               "allow execution in heap first targeted by these ;-separated modules")
OPTION_DEFAULT(bool, exempt_dll2stack, true,
               "allow execution in stack first targeted by exempt modules")
OPTION_DEFAULT(liststring_t, exempt_dll2stack_list, EMPTY_STRING,
               "allow execution in stack first targeted by these ;-separated modules")
DYNAMIC_PCL_OPTION_DEFAULT(bool, exempt_threat, true, "allow exempt threat ids")
DYNAMIC_PCL_OPTION_DEFAULT(
    liststring_t, exempt_threat_list, EMPTY_STRING,
    "silently allow these ;-separated threat ids, ? wildcards allowed")
DYNAMIC_OPTION_DEFAULT(liststring_t, silent_block_threat_list, EMPTY_STRING,
                       "silently block these ;-separated threat ids, ? wildcards allowed")
/* note that exempt_threat_list takes precedence over silent_block_threat_list */

#    ifdef RETURN_AFTER_CALL
PC_OPTION_DEFAULT(bool, ret_after_call, false,
                  "return after previous call instructions only")

/* Obfuscated option how to suppress security violations of type C */
OPTION_ALIAS(C, ret_after_call, false, STATIC, OP_PCACHE_GLOBAL)

PC_OPTION_DEFAULT(bool, vbjmp_allowed, true, "allow execution of VB direct jmp via ret")
PC_OPTION_DEFAULT(bool, vbpop_rct, true, "allow execution of VB pop via ret")
PC_OPTION_DEFAULT(bool, fiber_rct, true, "allow execution of fiber initialization")
PC_OPTION_DEFAULT(bool, mso_rct, true,
                  "allow execution of MSO continuations") /* FIXME: no clue what it is */
PC_OPTION_DEFAULT(
    bool, licdll_rct, true,
    "allow execution of licdll obfuscated call") /* FIXME: no clue what it is */
PC_OPTION_DEFAULT(bool, seh_rct, true, "allow execution of SEH ret constructs")
/* xref case 5752 */
PC_OPTION_DEFAULT(bool, borland_SEH_rct, true,
                  "allow execution of borland SEH constructs")
/* case 7317, from SQL2005 case 6534
 * off by default as case 7266 has us currently running these dlls natively
 */
PC_OPTION(bool, pushregret_rct, "allow execution of push;ret constructs")
/* PR 276529: ntdll64!RtlRestoreContext uses iret as a general jmp* */
PC_OPTION_DEFAULT(bool, iret_rct, IF_X64_ELSE(true, false),
                  "allow ntdll64!RtlRestoreContext iret")
/* case 7319, from SQL2005 cases 6541 and 6534
 * off by default as case 7266 has us currently running these dlls natively
 */
PC_OPTION(bool, xdata_rct, "allow ret to .xdata NtFlush targets")
PC_OPTION_DEFAULT(bool, exempt_rct, true, "allow rct in exempt modules")
/* case 9725 slsvc.exe->heap .C (software licensing service on Vista)
 * FIXME slsvc.exe is also in exempt_rct_to_default_list for slsvc.exe -> slsvc.exe
 * .C violations which would be covered here if it weren't for case 285. */
PC_OPTION_DEFAULT(
    liststring_t, exempt_rct_default_list,
    OPTION_STRING("dpcdll.dll;licdll.dll;mso.dll;winlogon.exe;sysfer.dll;slsvc.exe"),
    "allow rct within these ;-separated modules or to DGC")
PCL_OPTION_DEFAULT(liststring_t, exempt_rct_list, EMPTY_STRING,
                   "allow rct within these ;-separated modules or to DGC, append")

/* exempt_rct_from_{default,}_list are less strict than exempt_rct_list */
PCL_OPTION_DEFAULT(liststring_t, exempt_rct_from_default_list, EMPTY_STRING,
                   "allow rct from these ;-separated modules")
PCL_OPTION_DEFAULT(liststring_t, exempt_rct_from_list, EMPTY_STRING,
                   "allow rct from these ;-separated modules, append")

/* exempt_rct_to_{default,}_list are less strict than exempt_rct_list */
/* case 1690 dpcdll.dll, licdll.dll; case 1158 mso.dll */
/* case 1214 winlogon.exe */
/* case 5912 .F sysfer.dll,
 * FIXME: should be in exempt_rct_default_list, case 285 */
/* case 6076 blackd.exe: .F iss-pam1.dll ---> iss-pam1.dll,
 * FIXME: should be in exempt_rct_default_list, case 285 */
/* case 5051 w3wp.exe: .C jmail.dll ---> jmail.dll,
 * FIXME: should be in exempt_rct_default_list, case 285 */
/* case 6412, 7659: .E msvbvm50.dll;msvbvm60.dll;vbe6.dll */
/* case 9385 LVPrcInj.dll loaded by unknown thread */
/* case 9716 slc.dll (software licensing dll) on Vista .C */
/* case 9724 slsvc.exe->slsvc.exe (software licensing service) on Vista .C
 * FIXME: should be only in exempt_rct_default_list, case 285 */
PC_OPTION_DEFAULT(
    liststring_t, exempt_rct_to_default_list,
    OPTION_STRING(
        "dpcdll.dll;licdll.dll;mso.dll;winlogon.exe;sysfer.dll;iss-pam1.dll;jmail.dll;"
        "msvbvm50.dll;msvbvm60.dll;vbe6.dll;LVPrcInj.dll;slc.dll;slsvc.exe"),
    "allow rct to these ;-separated modules")
PCL_OPTION_DEFAULT(liststring_t, exempt_rct_to_list, EMPTY_STRING,
                   "allow rct to these ;-separated modules, append")

/* case 2144 - FIXME: should we have a general exemption for .A and .B as well? */
/* note we want to silently handle a .C - and to preserve compatibility with previous
 * releases we use the default attack handling (e.g. kill thread for services)
 */
OPTION_DEFAULT(uint, rct_ret_unreadable,
               OPTION_ENABLED | OPTION_BLOCK | OPTION_NO_HANDLING | OPTION_NO_REPORT,
               "alternative handling of return targets in unreadable memory")
/* note indirect call and indirect jump will always just throw an exception */

/* case 5329 - leaving for bug-compatibility with previous releases */
PC_OPTION(bool, rct_sticky, "leaves all RCT tables on unmap, potential memory leak")
/* case 9331 - FIXME: still leaking on DGC */
PC_OPTION_DEFAULT(bool, rac_dgc_sticky, true,
                  "leaves all RAC tables from DGC, potential memory leak")

PC_OPTION_DEFAULT(
    uint, rct_cache_exempt, 1, /* RCT_CACHE_EXEMPT_MODULES */
    "whether to cache exempted addresses, 0 never, 1 only in modules, 2 always")

PC_OPTION_DEFAULT(uint, rct_section_type, 0x20, /* IMAGE_SCN_CNT_CODE */
                  "bitflag to enable RCT checks on module code 0x20,data 0x40, or "
                  "uninitialized sections 0x80")
PC_OPTION_DEFAULT(uint, rct_section_type_exclude,
                  0x80000020, /* IMAGE_SCN_MEM_WRITE|IMAGE_SCN_CNT_CODE, xref case 8360 */
                  "TESTALL bitflag to disable RCT checks for specific module sections "
                  "sections that are matched by rct_section_type")

PC_OPTION_DEFAULT(
    bool, rct_modified_entry, true,
    "if not within module, lookup image entry point"
    "in LDR list for already mapped modules, and at MapViewOfSection for late")
/* expected to be overwritten by mscoree.dll */

#        ifdef RCT_IND_BRANCH
/* case 286 */
/* FIXME: currently OPTION_ENABLED doesn't do much yet, see rct_process_module_mmap() for
 * details */
/* FIXME: not yet supported on Linux (case 4983) */
PC_OPTION_DEFAULT(uint, rct_ind_call, OPTION_DISABLED,
                  "indirect call policy: address taken instructions only")
PC_OPTION_DEFAULT(uint, rct_ind_jump, OPTION_DISABLED,
                  "indirect jump policy: address taken or return targets")

PC_OPTION_DEFAULT(bool, rct_analyze_at_load, true,
                  "analyze modules for ind branch targets at module load time")

PC_OPTION_DEFAULT(bool, rct_reloc, true, "use relocation information to find references")

/* PR 215408: even when we have reloc info, we need to scan for rip-rel lea,
 * but only in modules that executed code we didn't see.
 * This option controls whether we proactively scan or only rely on our
 * scan-on-violation (PR 277044/277064).
 */
PC_OPTION_DEFAULT(
    bool, rct_scan_at_init, IF_X64_ELSE(true, false),
    "scan modules present at inject time for rip-rel lea even when relocs are present")
/* PR 275723: RVA-table-based switch statements.  Not on for Linux b/c we
 * don't have per-module RCT tables there.
 */
PC_OPTION_DEFAULT(bool, rct_exempt_intra_jmp,
                  IF_X64_ELSE(IF_WINDOWS_ELSE(true, false), false),
                  "allow jmps to target any intra-module address")

/* Obfuscated options to suppress security violations of types E and F */
OPTION_ALIAS(E, rct_ind_call, OPTION_DISABLED, STATIC, OP_PCACHE_GLOBAL)
OPTION_ALIAS(F, rct_ind_jump, OPTION_DISABLED, STATIC, OP_PCACHE_GLOBAL)
#        endif /* RCT_IND_BRANCH */
#    endif     /* RETURN_AFTER_CALL */

/* FIXME: there must be a way to make sure that new security
 * options are added here; and less importantly make sure that
 * when options are turned off by default they are taken out
 */
OPTION_COMMAND(
    bool, X, false, "X",
    {
        IF_RETURN_AFTER_CALL(options->ret_after_call = false;)
        IF_WINDOWS(options->executable_if_flush = false;)
        options->executable_if_alloc = false;
        options->executable_if_trampoline = false;
        options->executable_if_hook = false;
        options->executable_if_x = true;
        IF_RCT_IND_BRANCH(options->rct_ind_call = OPTION_DISABLED;)
        IF_RCT_IND_BRANCH(options->rct_ind_jump = OPTION_DISABLED;)
    },
    "duplicate Microsoft's nx: allow x memory only and don't enforce RCT", DYNAMIC,
    OP_PCACHE_GLOBAL /*since is not only a relaxation*/)

#endif /* PROGRAM_SHEPHERDING */

OPTION_DEFAULT(bool, enable_block_mod_load, true,
               "switch for enabling the block module from being loaded feature, if "
               "enabled the modules to block from loading are specified by the "
               "block_mod_load_list[_default] options")
/* dynamorio.dll : on this list to prevent non early_inject follow
 * children from double injecting if the process is already under dr */
/* entapi.dll;hidapi.dll : case 2871 for Entercept/VirusScan */
/* Caution: add to this list only DLLs whose callers don't crash
 * if LdrLoadDll calls fail, e.g. do not use the result for
 * GetProcAddress.
 */
OPTION_DEFAULT(liststring_t, block_mod_load_list_default,
               OPTION_STRING("dynamorio.dll;entapi.dll;hidapi.dll"),
               "if -enable_block_mod_load block the loading (at LdrLoadDll) of the "
               "following ;-separated modules, note that since this is blocking at "
               "LdrLoadDll the module match will be based on the filename of module "
               "being loaded NOT the PE name (which is used by most other options)")
OPTION_DEFAULT(liststring_t, block_mod_load_list, EMPTY_STRING,
               "if -enable_block_mod_load, block the loading (at LdrLoadDll) of the "
               "following ;-separated modules, note that since this is blocking at "
               "LdrLoadDll the module match will be based on the filename of module "
               "being loaded NOT the PE name (which is used by most other options)")

OPTION_DEFAULT(
    uint, handle_DR_modify, 1 /*DR_MODIFY_NOP*/,
    "specify how to handle app attempts to modify DR memory protection: either halt with "
    "an error, turn into a nop (default), or return failure to the app")

OPTION_DEFAULT(uint, handle_ntdll_modify,
               /* i#467: for CI builds the goal is to run an arbitrary app and
                * err on the side of DR missing stuff while native rather than
                * messing up the app's behavior.
                */
               3 /*DR_MODIFY_ALLOW*/,
               "specify how to handle app attempts to modify ntdll code: either halt "
               "with an error, turn into a nop (default), or return failure to the app")

/* generalized DR_MODIFY_NOP for customizable list of modules */
OPTION_DEFAULT(liststring_t, patch_proof_default_list, EMPTY_STRING,
               "ignore protection changes and writes to text of ;-separated module list, "
               "or * for all")
/* note '*' has to be at first position to mean all modules */
OPTION_DEFAULT(liststring_t, patch_proof_list, EMPTY_STRING,
               "ignore protection changes and writes to text of ;-separated module list, "
               "append, or * for all")
/* note '*' has to be at first position to mean all modules */

PC_OPTION_DEFAULT(bool, use_moduledb, false, "activate module database")
OPTION_ALIAS(staged, use_moduledb, false, STATIC, OP_PCACHE_GLOBAL) /* xref case 8924 */
/* FIXME - can't handle a company name with a ; in it */
PC_OPTION_DEFAULT(liststring_t, allowlist_company_names_default,
                  OPTION_STRING(COMPANY_LONG_NAME),
                  "don't relax protections as part of moduledb matching for these ; "
                  "separated company names")
PC_OPTION_DEFAULT(liststring_t, allowlist_company_names,
                  OPTION_STRING("Microsoft Corporation"),
                  "don't relax protections as part of moduledb matching for these ; "
                  "separated company names")
PC_OPTION_DEFAULT(
    uint, unknown_module_policy,
    0xf, /*MODULEDB_RCT_EXEMPT_TO|MODULEDB_ALL_SECTIONS_BITS:SECTION_ALLOW|MODULEDB_REPORT_ON_LOAD*/
    "module policy control field for non-allowlisted modules")
OPTION_DEFAULT(uint, unknown_module_load_report_max, 10,
               "max number of non allowlist modules to log/report at load time")
OPTION_DEFAULT(uint, moduledb_exemptions_report_max, 3,
               "max number of moduledb security exemptions to report")
/* case 9330 detect race in our security policies during DLL
 * unload, and also case 9364 for .C only after unload
 */
OPTION_DEFAULT(bool, unloaded_target_exception, true,
               "detect and silently handle as exceptions app races during DLL unload")

#ifdef WINDOWS
OPTION_DEFAULT(bool, hide, true, "remove DR dll from module lists")
OPTION_DEFAULT(uint, hide_from_query,
               3 /* HIDE_FROM_QUERY_BASE_SIZE|HIDE_FROM_QUERY_TYPE_PROTECT */,
               "mask to control what option to take to hide dr when the app does a query "
               "virtual memory call on our dll base")

OPTION_DEFAULT(bool, track_module_filenames, true,
               "track module file names by watching section creation")
#endif

/* XXX: since we have dynamic options this option can be false for most of the time,
 * and the gui should set true only when going to detach to prevent a security risk.
 * The setting should be removed when detach is complete.
 * In vault mode: -no_allow_detach -no_dynamic_options
 */
DYNAMIC_OPTION_DEFAULT(bool, allow_detach, true, "allow detaching from process")

/* turn off critical features, right now for experimentation only */
#ifdef WINDOWS
PC_OPTION_INTERNAL(bool, noasynch, "disable asynchronous event interceptions")
#endif
PC_OPTION_DEFAULT_INTERNAL(
    bool, hw_cache_consistency, IF_X86_ELSE(true, false),
    "keep code cache consistent in face of hardware implicit icache sync")
OPTION_DEFAULT_INTERNAL(bool, sandbox_writes, true,
                        "check each sandboxed write for selfmod?")
/* FIXME: off by default until dll load perf issues are solved: case 3559 */
OPTION_DEFAULT_INTERNAL(bool, safe_translate_flushed, false,
                        "store info at flush time for safe post-flush translation")
PC_OPTION_INTERNAL(bool, store_translations,
                   "store info at emit time for fragment translation")
/* i#698: our fpu state xl8 is a perf hit for some apps */
PC_OPTION(bool, translate_fpu_pc,
          "translate the saved last floating-point pc when FPU state is saved")

/* case 8812 - owner validation possible only on Win32 */
/* Note that we expect correct ACLs to prevent anyone other than
 * owner to have overwritten the files.
 * XXX: not supported on linux
 */
OPTION_DEFAULT(bool, validate_owner_dir, IF_WINDOWS_ELSE(true, false),
               "validate owner of persisted cache or ASLR directory")
OPTION_DEFAULT(bool, validate_owner_file, false,
               "validate owner of persisted cache or ASLR, on each file")

/* PR 326815: off until we fix gcc+gap perf */
#define ENABLE_COARSE_UNITS(prefix)                                         \
    {                                                                       \
        (prefix)->coarse_units = true;                                      \
        /* PR 326610: we turned off -indirect_stubs by default, but         \
         * -coarse_units doesn't support that yet (that's i#659/PR 213262). \
         * XXX: duplicated in -persist                                      \
         */                                                                 \
        (prefix)->indirect_stubs = true;                                    \
    }
#define DISABLE_COARSE_UNITS(prefix)                    \
    {                                                   \
        (prefix)->coarse_units = false;                 \
        /* We turned off -indirect_stubs by default. */ \
        (prefix)->indirect_stubs = false;               \
    }
OPTION_COMMAND(
    bool, coarse_units, false, "coarse_units",
    {
        if (options->coarse_units) {
            ENABLE_COARSE_UNITS(options);
        }
    },
    "use coarse-grain code cache management when possible", STATIC, OP_PCACHE_GLOBAL)

/* Currently a nop, but left in for the future */
OPTION_ALIAS(enable_full_api, coarse_units, false, STATIC, OP_PCACHE_GLOBAL)

OPTION_DEFAULT(bool, coarse_enable_freeze, false, "enable freezing of coarse units")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_at_exit, false,
                       "freeze coarse units at process exit")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_at_unload, false,
                       "freeze coarse units at module unload or other flush")
/* Remember that this is a threshold on per-module per-run new generated code.
 * Though bb building time would recommend a fragment count threshold, for
 * sharing benefits we care about code size.
 * Remember also that we get wss and time improvements from RCT table
 * being persisted; yet a large module w/ many RCT targets is likely to have
 * a lot of code, though we may not execute much of it, so we have
 * the coarse_freeze_rct_min option.
 * Xref case 10362 on using pcache files for RCT independently of code caches.
 */
DYNAMIC_OPTION_DEFAULT(uint_size, coarse_freeze_min_size, 512,
                       "only freeze new coarse code > this cache size (bytes)")
DYNAMIC_OPTION_DEFAULT(uint_size, coarse_freeze_append_size, 256,
                       "only append new coarse code > this cache size (bytes)")
DYNAMIC_OPTION_DEFAULT(uint_size, coarse_freeze_rct_min, 2 * 1024,
                       "freeze a coarse module w/ > this RCT entries")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_clobber, false,
                       "overwrite existing persisted temp files")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_rename, true,
                       "rename existing persisted files when writing new ones")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_clean, true, "delete renamed persisted files")
DYNAMIC_OPTION_DEFAULT(bool, coarse_freeze_merge, true,
                       "merge unfrozen coarse code with frozen code when persisting")
DYNAMIC_OPTION_DEFAULT(bool, coarse_lone_merge, true,
                       "merge un-persisted unit w/ disk file when persisting")
DYNAMIC_OPTION_DEFAULT(bool, coarse_disk_merge, true,
                       "merge persisted unit w/ disk file when persisting")
#ifdef WINDOWS
DYNAMIC_OPTION_DEFAULT(
    bool, coarse_freeze_rebased_aslr, false,
    "freeze modules with ASLR enabled that failed to load due to rebasing")
#endif
/* We have explicit support for mixing elision at gen and use so not PC_ */
OPTION_DEFAULT(bool, coarse_freeze_elide_ubr, true,
               "elide fall-through ubr when freezing coarse units")
/* case 9677: unsafe to elide entire-bb-ubr since creates backmap ambiguity */
PC_OPTION(bool, unsafe_freeze_elide_sole_ubr,
          "elide sole-ubr-bb fall-through ubr when freezing coarse units")
OPTION_DEFAULT(bool, coarse_pclookup_table, true,
               "use a reverse cache lookup table for faster entry-pc lookup,"
               "critical for performance of frozen units + trace building")
OPTION_DEFAULT(bool, persist_per_app, false,
               "use separate persisted cache per executable")
OPTION_DEFAULT(bool, persist_per_user, true, "use separate persisted cache per user")
OPTION_DEFAULT(bool, use_persisted, false, "use persisted cache if it exists")
/* exemptions are based on canonical DR names (case 3858) */
OPTION_DEFAULT(liststring_t, persist_exclude_list, EMPTY_STRING,
               "exclude these ;-separated modules from persisted use and generation")
#ifdef RCT_IND_BRANCH
OPTION_DEFAULT(bool, persist_rct, true,
               "persist RCT (call* target) tables; if this option is off, "
               "we will still persist for Borland modules, but no others")
OPTION_DEFAULT(bool, persist_rct_entire, true,
               "if -persist_rct, persist RCT tables for entire module")
OPTION_DEFAULT(bool, use_persisted_rct, true,
               "use persisted RCT info, if available, instead of analyzing module")
#endif
#ifdef HOT_PATCHING_INTERFACE
OPTION_DEFAULT(bool, use_persisted_hotp, true,
               "use persisted hotp info to avoid flushing perscaches")
#endif
OPTION_DEFAULT(uint, coarse_fill_ibl, 1, /* mask from 1<<IBL_type (1=ret|2=call*|4=jmp*)
                                          * indicating which per-type table(s) to fill */
               "fill 1st thread's ibl tables from persisted RAC/RCT tables")
/* FIXME case 9599: w/ MEM_MAPPED this option removes COW from the cache */
/* FIXME: could make PC_ to coexist for separate values */
OPTION_DEFAULT(bool, persist_map_rw_separate, true,
               "map persisted read-only sections separately to support sharing"
               "(option must be on both when generated and when using)")
OPTION_DEFAULT(bool, persist_lock_file, true,
               "keep persisted file handle open to prevent writes/deletes")
/* FIXME: could make PC_ to coexist for separate values */
/* PR 215036: linux does not support PERSCACHE_MODULE_MD5_AT_LOAD */
OPTION_DEFAULT(uint, persist_gen_validation, IF_WINDOWS_ELSE(0x1d, 0xd),
               /* PERSCACHE_MODULE_MD5_SHORT | PERSCACHE_MODULE_MD5_AT_LOAD |
                  PERSCACHE_GENFILE_MD5_{SHORT,COMPLETE} */
               "controls md5 values that we store when we persist")
OPTION_DEFAULT(uint, persist_load_validation, 0x5,
               /* PERSCACHE_MODULE_MD5_SHORT | PERSCACHE_GENFILE_MD5_SHORT */
               "controls which md5 values we check when we load a persisted file; must "
               "be a subset of -persist_gen_validation, else we won't load anything")
/* Size of short checksum of file header and footer for PERSCACHE_MODULE_MD5_SHORT.
 * Size must match that in effect at persist time. */
PC_OPTION_DEFAULT(uint_size, persist_short_digest, (4 * 1024),
                  "size of file header and footer to check, in KB or MB")
OPTION_DEFAULT(bool, persist_check_options, true,
               "consider pcache-affecting options when using pcaches")
PC_OPTION_DEFAULT(bool, persist_check_local_options, false,
                  "include all local options in -persist_check_options")
PC_OPTION_DEFAULT(bool, persist_check_exempted_options, true,
                  "only check local options for modules affected by exemptions")
/* FIXME: make this part of -protect_mask? */
OPTION_DEFAULT(bool, persist_protect_stubs, true, "keep persisted cache stubs read-only")
OPTION_DEFAULT(uint, persist_protect_stubs_limit, 0,
               "stop write-protecting stubs after this many writes (0 protects forever)")
/* case 10074: we can trade working set to reduce pagefile, strangely */
OPTION_DEFAULT(bool, persist_touch_stubs, true,
               "touch stubs prior to protecting to avoid pagefile cost")
/* case 8640: relies on -executable_{if_rx_text,after_load} */
PC_OPTION_DEFAULT(bool, coarse_merge_iat, true,
                  "merge iat page into coarse unit at +rx transition")
/* PR 214084: avoid push of abs addr in pcache
 * TODO: should auto-enable (on Linux or Vista+ only?) for -coarse_enable_freeze?
 */
PC_OPTION_DEFAULT(bool, coarse_split_calls, false,
                  "make all calls fine-grained and in own bbs")
#ifdef X64
PC_OPTION_DEFAULT(bool, coarse_split_riprel, false,
                  "make all rip-relative references fine-grained and in own bbs")
#endif
#ifdef UNIX
OPTION_DEFAULT(bool, persist_trust_textrel, true,
               "if textrel flag is not set, assume module has no text relocs")
#endif
/* the DYNAMORIO_VAR_PERSCACHE_ROOT config var takes precedence over this */
OPTION_DEFAULT(pathstring_t, persist_dir, EMPTY_STRING,
               "base per-user directory for persistent caches")
/* the DYNAMORIO_VAR_PERSCACHE_SHARED config var takes precedence over this */
OPTION_DEFAULT(pathstring_t, persist_shared_dir, EMPTY_STRING,
               "base shared directory for persistent caches")
/* convenience option */
OPTION_COMMAND(
    bool, persist, false, "persist",
    {
        if (options->persist) {
            ENABLE_COARSE_UNITS(options);
            options->coarse_enable_freeze = true;
            options->coarse_freeze_at_exit = true;
            options->coarse_freeze_at_unload = true;
            options->use_persisted = true;
            /* these two are for correctness */
            IF_UNIX(options->coarse_split_calls = true;)
            IF_X64(options->coarse_split_riprel = true;)
            /* FIXME: i#660: not compatible w/ Probe API */
            DISABLE_PROBE_API(options);
            /* i#1051: disable reset until we decide how it interacts w/
             * pcaches */
            DISABLE_RESET(options);
        } else {
            options->coarse_enable_freeze = false;
            options->use_persisted = false;
            REENABLE_RESET(options);
        }
    },
    "generate and use persisted caches", STATIC, OP_PCACHE_GLOBAL)

/* case 10339: tuned for boot and memory performance, not steady-state */
OPTION_COMMAND(
    bool, desktop, false, "desktop",
    {
        if (options->desktop) {
            options->coarse_enable_freeze = true;
            options->use_persisted = true;
            options->coarse_freeze_at_unload = true;
            DISABLE_TRACES(options);
            options->shared_bb_ibt_tables = true;
            /* case 10525/8711: reduce # links via single fine-grained vsyscall
             * bb N.B.: if we re-enable traces we'll want to turn this back on
             */
            options->indcall2direct = false;
            /* i#1051: disable reset until we decide how it interacts w/
             * pcaches */
            DISABLE_RESET(options);
        } else {
            /* -no_desktop: like -no_client, only use in simple
             * sequences of -desktop -no_desktop
             */
            options->coarse_enable_freeze = false;
            options->use_persisted = false;
            options->coarse_freeze_at_unload = false;
            REENABLE_TRACES(options);
            options->shared_bb_ibt_tables = false;
            options->indcall2direct = true;
            REENABLE_RESET(options);
        }
    },
    "desktop process protection", STATIC, OP_PCACHE_GLOBAL)

/* should probably always turn on -executable_if_text if turning this on, for
 * modules loaded by natively-executed modules
 * these don't affect pcaches since the trampoline bbs won't be coarse-grain.
 */
/* XXX i#1582: add ARM support for native_exec */
OPTION_DEFAULT(bool, native_exec, IF_X86_ELSE(true, false),
               "attempt to execute certain libraries natively (WARNING: lots of issues "
               "with this, use at own risk)")
/* initially populated w/ all dlls we've needed to get .NET, MS JVM, Sun JVM,
 * Symantec JVM, and Panda AV working, but with very limited workload testing so far
 */
OPTION_DEFAULT(
    liststring_t, native_exec_default_list,
    /* case 3453, case 1962 .NET 1.0, 1.1 : mscorsvr.dll;mscorwks.dll;aspnet_isapi.dll */
    /* case 6189 .NET 2.0: mscorwks_ntdef.dll(PE name of mscorwks.dll);aspnet_isapi.dll */
    /* case 3453 MS JVM: msjava.dll;msawt.dll, Sun JVM: jvm.dll */
    /* case 3749 Symantec Java! JIT: symcjit.dll */
    /* case 3762 Panda AV: pavdll.dll */
    OPTION_STRING("mscorsvr.dll;mscorwks.dll;aspnet_isapi.dll;mscorwks_ntdef.dll;msjava."
                  "dll;msawt.dll;jvm.dll;symcjit.dll;pavdll.dll"),
    "execute these ;-separated modules natively")
/* easy way to add dlls w/o having to re-specify default list, while keeping
 * default list visible and settable at global level
 */
OPTION_DEFAULT(liststring_t, native_exec_list, EMPTY_STRING,
               "execute these ;-separated modules natively")
OPTION_DEFAULT(bool, native_exec_syscalls, true,
               "intercept system calls while application is native")
OPTION_DEFAULT(bool, native_exec_dircalls, true,
               "check direct calls as gateways to native_exec modules")
OPTION_DEFAULT(bool, native_exec_callcall, true,
               "put gateway on 1st call of a pair where 2nd targets a native_exec module")
OPTION_DEFAULT(
    bool, native_exec_guess_calls, true,
    "if TOS looks like a ret addr, assume transition to a new module was via call*")
/* case 7266: add exes and dlls with managed code to native_exec_areas */
OPTION_DEFAULT(bool, native_exec_managed_code, true,
               "if module has managed code, execute it natively")
/* case 10998: add modules with .pexe sections to native_exec_areas,
 * FIXME - for case 6765 turn this into a liststring_t so we can consider native exec
 * for other potentially problematic sections like .aspack, .pcle, and .sforce */
OPTION_DEFAULT(
    bool, native_exec_dot_pexe, true,
    "if module has .pexe section (proxy for strange int 3 behavior), execute it natively")
OPTION_DEFAULT(
    bool, native_exec_retakeover, false,
    "attempt to re-takeover when a native module calls out to a non-native module")
/* XXX i#1238-c#1: we do not support inline optimization in Windows. */
OPTION_COMMAND(
    bool, native_exec_opt, false, "native_exec_opt",
    {
        if (options->native_exec_opt) {
            IF_KSTATS(options->kstats = false); /* i#1238-c#4 */
            DISABLE_TRACES(options);            /* i#1238-c#6 */
        }
    },
    "optimize control flow transition between native and non-native modules", STATIC,
    OP_PCACHE_GLOBAL)

#ifdef WINDOWS
OPTION_DEFAULT(bool, skip_terminating_threads, false,
               "do not takeover threads that are terminating")
#endif

OPTION_DEFAULT(bool, sleep_between_takeovers, false,
               "sleep between takeover attempts to allow threads to exit syscalls")

OPTION_DEFAULT(uint, takeover_attempts, 8, "number of takeover attempts")

/* vestiges from our previous life as a dynamic optimizer */
OPTION_DEFAULT_INTERNAL(bool, inline_calls, true, "inline calls in traces")

/* control-flow optimization to convert indirect calls to direct calls.
 * This is separated from the optimization flags that follow below as it
 * applies to all fragments, not just traces.
 * FIXME Delete the setting after sufficient testing & qualification?
 */
PC_OPTION_DEFAULT(bool, indcall2direct, true,
                  "optimization: convert indirect calls to direct calls")

/* case 85 - for optimization, and case 1948 for its basis for a stronger security check
 */
/* similar to both indcall2direct and emulate_IAT_writes */
PC_OPTION_DEFAULT(bool, IAT_convert, false,
                  "convert indirect call or jmp through IAT to direct call or jmp")
PC_OPTION_DEFAULT(bool, IAT_elide, false,
                  "elide indirect call or jmp converted by IAT_convert"
                  "unless reached max_elide_{jmp,call}; requires IAT_convert")
OPTION_DEFAULT_INTERNAL(bool, unsafe_IAT_ignore_hooker, false, "ignore IAT writes")

/* compatibility options */
OPTION_DEFAULT(
    uint, thread_policy,
    OPTION_DISABLED | OPTION_NO_BLOCK | OPTION_NO_REPORT | OPTION_NO_CUSTOM,
    "thread delivered to a writable region allowed or squashed (optionally silently)")
/* custom bit off restricts thread_policy to VSE shellcode, on makes it general */
#ifdef WINDOWS
OPTION_DEFAULT(
    uint, apc_policy,
    OPTION_DISABLED | OPTION_NO_BLOCK | OPTION_NO_REPORT | OPTION_NO_CUSTOM,
    "APC delivered to a writable region allowed or squashed (optionally silently)")
/* custom bit off restricts apc_policy to VSE shellcode, on makes it general */

OPTION_DEFAULT_INTERNAL(
    bool, hook_image_entry, true,
    "Allow hooking of the image "
    "entry point when we lose control at a pre-image-entry-point callback return. "
    "Typically it's not needed to regain control if -native_exec_syscalls is on.")
OPTION_DEFAULT_INTERNAL(
    bool, hook_ldr_dll_routines, false,
    "Hook LdrLoadDll and LdrUnloadDll even with no direct reason other than "
    "regaining control on AppInit injection.")
OPTION_DEFAULT(
    bool, clean_testalert, true, /* case 9288, 10414 SpywareDoctor etc. */
    "restore NtTestAlert to a pristine state at load by clearing away any hooks")
OPTION_DEFAULT(uint, hook_conflict, 1 /* HOOKED_TRAMPOLINE_SQUASH */, /* case 2525 */
               "action on conflict with existing non Nt* hooks: die, squash or chain")
OPTION_DEFAULT(uint, native_exec_hook_conflict,
               /* i#467: for CI builds, better to let the app run correctly, even if
                * DR missing something while native.  Native intercept matters only
                * for AppInit, or tools that go native, or future attach: all rare
                * things.
                */
               4 /* HOOKED_TRAMPOLINE_NO_HOOK */,
               "action on conflict with existing Nt* hooks: die, squash, or deeper")
/* NOTE - be careful about using the default value till the options are
 * read */
OPTION_DEFAULT(
    bool, dr_sygate_int, false,
    "Perform dr int system calls in a sygate compatible fashion (indirected via ntdll)")
OPTION_DEFAULT(
    bool, dr_sygate_sysenter, false,
    "Perform dr int system calls in a sygate compatible fashion (indirected via ntdll)")
/* Turn off sygate compatibility int syscall indirection for app system
 * calls.  Dr system calls will still indirect as controlled by
 * above options. */
OPTION_DEFAULT(
    bool, sygate_int, false,
    "Perform app int system calls in Sygate compatible fashion (indirected via ntdll)")
OPTION_DEFAULT(bool, sygate_sysenter, false,
               "Perform app sysenter system calls in Sygate compatible fashion "
               "(indirected via ntdll)")
OPTION_DEFAULT(bool, native_exec_hook_create_thread, true,
               "if using native_exec hooks, decides whether or not to hook CreateThread "
               "(disable for Sygate compatibility)")
OPTION_COMMAND(
    bool, sygate, false, "sygate",
    {
        options->dr_sygate_int = true;
        options->dr_sygate_sysenter = true;
        options->sygate_int = true;
        options->sygate_sysenter = true;
        options->native_exec_hook_conflict = 3; /* HOOK_CONFLICT_HOOK_DEEPER */
        options->native_exec_hook_create_thread = false;
    },
    "Sets necessary options for running in Sygate compatible mode", STATIC, OP_PCACHE_NOP)

/* FIXME - disabling for 64bit due to layout considerations xref PR 215395,
 * should re-enable once we have a better solution. xref PR 253621 */
OPTION_DEFAULT(bool, aslr_dr, IF_X64_ELSE(false, true),
               /* case 5366 randomize location of dynamorio.dll, uses
                * aslr_parent_offset to control randomization padding which
                * currently gives us 8 bits of randomness for wasting 16MB
                * virtual space; breaks sharing for the relocated
                * portions of our DLL
                */
               "randomization needs to be set in parent process")

/* Address Space Layout Randomization */
/* FIXME: case 2491 for stacks/heaps/PEBs/TEBs, sharing */
OPTION_DEFAULT(uint, aslr, 0 /* ASLR_DISABLED */,
               "address space layout randomization, from aslr_option_t")
OPTION_ALIAS(R, aslr, 0 /* ASLR_DISABLED */, STATIC, OP_PCACHE_NOP)

OPTION_DEFAULT(
    uint, aslr_action, 0x111,
    /* case 7017 0x111 = ASLR_REPORT | ASLR_DETECT_EXECUTE | ASLR_TRACK_AREAS */
    "address space layout handling and reporting, from aslr_action_t")

OPTION_DEFAULT(uint, aslr_retry, 2,
               /* case 6739 - allow private ASLR to search for a good fit,
                * 1  would allow linear range choice to leapfrog other DLLs
                * 2+ would also have a chance to deal with unlikely races
                */
               "private ASLR attempts for a good fit after failure, 0 fallback to native")

OPTION_DEFAULT(
    uint, aslr_cache, 0, /* ASLR_DISABLED */
    "address space layout process shared and persistent cache, from aslr_cache_t")
#endif
/* min_free_disk applies to both ASLR and persisted caches */
OPTION_DEFAULT(uint_size, min_free_disk, 50 * (1024 * 1024), /* 50MB */
               "minimum free disk space (or quota) on DLL cache volume")
/* case 8494 cache capacity management for ASLR and pcaches
 * Note default size is in Kilobytes, so it's best to always add M qualifier.
 * Examples: 1m, 10M
 */
#ifdef WINDOWS
OPTION_DEFAULT_INTERNAL(
    uint, aslr_internal, 0 /* ASLR_INTERNAL_DEFAULT */,
    "address space layout randomization, internal flags from aslr_internal_option_t")

/* FIXME: we need to find a good location to allow growth for other allocations */
OPTION_DEFAULT(uint_addr, aslr_dll_base, 0x40000000, "starting DLL base addresses")

/* limit for ASLR_RANGE_BOTTOM_UP, or starting point for ASLR_RANGE_TOP_DOWN */
/* FIXME: case 6739 - what to do when reaching top,
 *
 * FIXME: how is STATUS_ILLEGAL_DLL_RELOCATION determined, we
 * have to know stay out of the way since loader fails app if it
 * finds that user32.dll is not where it wanted?
 */
OPTION_DEFAULT(uint_addr, aslr_dll_top, 0x77000000, "top of DLL range")

/* FIXME: the lower 16 bits are ignored (Windows/x86
 * AllocationGranularity is 64KB).  This here gives us 12bits of
 * randomness.  Could make it larger if necessary. */
OPTION_DEFAULT(uint_addr, aslr_dll_offset, 0x10000000,
               "maximum random jitter for first DLL")

/* FIXME: too little (4 bits) randomness between DLLs, vs too much fragmentation */
OPTION_DEFAULT(uint_addr, aslr_dll_pad, 0x00100000, "maximum random jitter between DLLs")

/* case 6287 - first thread's stack can be controlled only by parent */
/* ASLR_STACK activates, though affect real heap reservations as well */
/* FIXME: the lower 16 bits are ignored (Windows/x86
 * AllocationGranularity is 64KB).  FIXME: This here gives us 8 bits of
 * randomness.  Could make it larger if necessary. */
OPTION_DEFAULT(uint_addr, aslr_parent_offset, 0x01000000,
               "maximum random jitter for parent reservation")
/* ASLR_HEAP activates, though affect real stack reservations as
 * well This here gives us 12 bits of randomness.  Could make it
 * larger if necessary.
 */
OPTION_DEFAULT(uint_addr, aslr_heap_reserve_offset, 0x10000000,
               "random jitter for first child reservation (large)")
OPTION_DEFAULT(
    uint_addr, aslr_heap_exe_reserve_offset, 0x01000000,
    /* if executable ImageBaseAddress is in the middle of virtual address space */
    "random jitter for reservation after executable (smaller)")

/* ASLR_HEAP_FILL activates */
/* FIXME: too little (4 bits) randomness between heap reservations,
 * vs too much fragmentation */
OPTION_DEFAULT(uint_addr, aslr_reserve_pad, 0x00100000,
               "random jitter between reservations (tiny)")

/* FIXME: plan for 4.3 only after aslr_safe_save is checked in
 * ASLR_PERSISTENT_SOURCE_DIGEST | ASLR_PERSISTENT_SHORT_DIGESTS */
OPTION_DEFAULT(uint, aslr_validation, 0x1, /* ASLR_PERSISTENT_PARANOID */
               "consistency and security validation level of stringency")

OPTION_DEFAULT(uint_size, aslr_short_digest, (16 * 1024),
               "size of file header and footer to check, in KB or MB")
/* used for checksum comparison of file header and footer,
 * note that if this value is changed previously persisted
 * files will not be accepted.
 * 0 turns into a full file digest
 */
/* default size is in Kilobytes, Examples: 1024, 1024k, 1m */

OPTION_DEFAULT(uint_size, aslr_section_prefix, (16 * 1024),
               "size of section prefix to match, in KB or MB")
/* used for byte comparison of a prefix of each file section,
 * if enabled by ASLR_PERSISTENT_PARANOID_PREFIX.
 * 0 disables section validation
 */
/* default size is in Kilobytes, Examples: 1024, 1024k, 1m */

OPTION_DEFAULT(
    liststring_t, exempt_aslr_default_list,
    /* user32.dll - case 6620 on STATUS_ILLEGAL_DLL_RELOCATION. Not
     *              clear whether loader is just whining, or there
     *              is a good reason it shouldn't be rebased.
     *       FIXME: without early injection affects only RU=5 processes that load it late,
     *              SHOWSTOPPER on XP.
     *       FIXME: Hopefully shouldn't be necessary for all KnownDlls, investigate full
     * list. ole32.dll - case 7746 on Win2000 and case 7743 on NT complaining about
     * STATUS_ILLEGAL_DLL_RELOCATION sfc.dll - case 8705 update.exe targeting directly
     *      sfc.dll!MySfcTerminateWatcherThread in winlogon.exe
     * kbdus.dll,kbdbg.dll - case 6671 FIXME: list not complete,
     *              FIXME: case 6740: we don't actually support kbd*.dll
     * kernel32.dll - with early injection this one also complains of being
     *                rebased.
     */
    OPTION_STRING(
        "kernel32.dll;user32.dll;ole32.dll;sfc.dll;kbdus.dll;kbdbu.dll;kbd*.dll"),
    "exempt from randomization these ;-separated modules")
OPTION_DEFAULT(liststring_t, exempt_aslr_list,
               /* Note that allows '*' as a stress option */
               EMPTY_STRING,
               "exempt from randomization these ;-separated modules, append")

/* case 7794 - when using private ASLR these large DLLs have a higher
 * impact on visible memory, and in stress testing on performance.
 * not really recommended but available until we enable sharing
 */
OPTION_DEFAULT(bool, aslr_extra, false, "ASLR DLL exempt longer list")
OPTION_DEFAULT(liststring_t, exempt_aslr_extra_list,
               /* see case 7794 exempt from both private and shared */
               /* note that these names are matched to PE name for private,
                * but to file names for ASLR cache */
               OPTION_STRING("mshtml.dll;msi.dll;mso.dll;shell32.dll"),
               "exempt from randomization these ;-separated modules")

/* case 9495 - include or exclude list for DLLs to share and
 * persist. Note that one cannot use simultaneously an include and
 * exclude list, so to remove an entry from either need to provide the
 * whole list as '#advapi32.dll'
 */
OPTION_DEFAULT(uint, aslr_cache_list, 1, /* ASLR_CACHE_LIST_INCLUDE */
               "controls DLLs to be shared via aslr_cache {all,include,exclude}")
/* (note aslr_cache_list_t values match meaning in process_control) */
/* exemptions are based on file names, not PE names */
OPTION_DEFAULT(
    liststring_t, aslr_cache_include_list,
    /* enabled via -aslr_cache_list 1 (ASLR_CACHE_LIST_INCLUDE) (default) */
    /* All other DLLs are left for private ASLR if not exempt by
     * exempt_aslr_extra_list. */
    /* Need a slightly longer list to be worth enabling */
    /* Note we're bound by MAX_LIST_OPTION_LENGTH so check total
     * length before adding, and leave some room for DLLs that we
     * may want to add in the field.
     */
    /* Note DLLs that have no or very few relocation pages shouldn't be added */
    OPTION_STRING(
        "advapi32.dll;"
        /* FIXME: browseui.dll; also has high wset in IE after empty.exe */
        "comctl32.dll;" /* note two different SxS versions */
        "gdi32.dll;"
        "jscript.dll;" /* IE */
        "msctf.dll;"
        "mshtml.dll;" /* shared by IE and Explorer */
        /* FIXME: mso.dll is the top DLL in Office apps, yet has startup effects */
        "msvcrt.dll;"
        "riched20.dll;" /* different versions of in Office11\ and system32\ */
        "rpcrt4.dll;"
        "sapi.dll;" /* only Speech\ for Office apps */
        "setupapi.dll;"
        "shell32.dll;"
        /* FIXME: shdocvw.dll; is a top DLL in Office and IE */
        /* shdoclc.dll companion DLL has resources for shdocvw.dll,
         * and has no relocations so it's fine to randomize privately
         * the resources, while not having to validate them if in
         * shared ASLR cache
         */
        "sptip.dll;"   /* only Speech\ related */
        "sxs.dll;"     /* XP, in all processes, provides Side x Side  */
        "uxtheme.dll;" /* FIXME: XP, not very high WSet after startup */
        "ws2_32.dll"   /* case 9627 make sure any ASLR at all */
        ),
    "use shared cache only for these ;-separated modules")
/* exemptions are based on file names, not PE names */
OPTION_DEFAULT(liststring_t, aslr_cache_exclude_list,
               /* enabled via -aslr_cache_list 2 (ASLR_CACHE_LIST_EXCLUDE) */
               /* exempt from shared ASLR but still apply private ASLR */
               OPTION_STRING("mso.dll;xpsp2res.dll"),
               "exclude from shared cache these ;-separated modules")
OPTION_DEFAULT(bool, aslr_safe_save, true,
               /* see case 9696 */
               "ASLR DLL safe file creation in temporary file before rename")

/* Syntactic sugar for memory savings at the cost of security controlled from core */
OPTION_COMMAND(
    bool, medium, false, "medium",
    {
        if (options->medium) {
            options->aslr_extra = true;
            options->thin_client = false; /* Case 9037. */
        }
    },
    "medium security/memory mapping", STATIC, OP_PCACHE_NOP)

OPTION_COMMAND(
    bool, low, false, "low",
    {
        if (options->low) {
            IF_HOTP(options->hot_patching = true);
            IF_HOTP(options->hotp_only = true;)
            IF_HOTP(options->liveshields = true;)
            IF_HOTP(options->hotp_diagnostics = true;)
            IF_HOTP(if (options->hotp_only == true) {
                /* coordinate with hotp_only any additional option changes */
                IF_RETURN_AFTER_CALL(options->ret_after_call = false;)
                IF_RCT_IND_BRANCH(options->rct_ind_call = OPTION_DISABLED;)
                IF_RCT_IND_BRANCH(options->rct_ind_jump = OPTION_DISABLED;)
            });
            /* Matching old behavior */
            IF_WINDOWS(options->apc_policy =
                           OPTION_ENABLED | OPTION_BLOCK | OPTION_REPORT | OPTION_CUSTOM;)

            options->vm_size = 32 * 1024 * 1024; /* 32MB */
            IF_GBOP(options->gbop = 0x6037;)     /* GBOP_CLIENT_DEFAULT */
            options->aslr = 0x0;                 /* ASLR_DISABLED */
            /* Reset has no meaning for hotp_only; see case 8389. */
            DISABLE_RESET(options);
            IF_KSTATS(options->kstats = false); /* Cases 6837 & 8869. */
            options->thin_client = false;       /* Case 9037. */
        }
    },
    "low security/memory mapping", STATIC, OP_PCACHE_NOP)

OPTION_COMMAND(
    bool, client, false, "client",
    {
        if (options->client) {
            IF_HOTP(options->hot_patching = true);
            IF_HOTP(options->hotp_only = true;)
            IF_HOTP(options->liveshields = true;)
            IF_HOTP(options->hotp_diagnostics = true;)
            IF_HOTP(if (options->hotp_only == true) {
                /* coordinate with hotp_only any additional option changes */
                IF_RETURN_AFTER_CALL(options->ret_after_call = false;)
                IF_RCT_IND_BRANCH(options->rct_ind_call = OPTION_DISABLED;)
                IF_RCT_IND_BRANCH(options->rct_ind_jump = OPTION_DISABLED;)
            });
            /* Matching old behavior */
            IF_WINDOWS(options->apc_policy =
                           OPTION_ENABLED | OPTION_BLOCK | OPTION_REPORT | OPTION_CUSTOM;)

            options->vm_size = 32 * 1024 * 1024; /* 32MB */
            IF_GBOP(options->gbop = 0x6037;)     /* GBOP_CLIENT_DEFAULT */
            /* making sure -client -low  == -low -client*/
            if (options->low) {
                options->aslr = 0x0;       /* ASLR_DISABLED */
                options->aslr_cache = 0x0; /* ASLR_DISABLED */
            } else {
                options->aslr = 0x7;         /* ASLR_CLIENT_DEFAULT */
                options->aslr_cache = 0x192; /* ASLR_CACHE_DEFAULT */
            }

            /* case 2491 ASLR_SHARED_CONTENTS |
             * ASLR_SHARED_ANONYMOUS_CONSUMER | ASLR_SHARED_FILE_PRODUCER,
             */

            /* Reset has no meaning for hotp_only; see case 8389. */
            DISABLE_RESET(options);
            IF_KSTATS(options->kstats = false); /* Cases 6837 & 8869. */
            options->thin_client = false;       /* Case 9037. */
        } else {
            /*
             * case 8283 -no_client.  Note that this will work well
             * only for simple sequences of -client -no_client, as
             * we do not attempt to actually recover and is very
             * very likely we'll forget to keep this in synch
             */
            IF_HOTP(options->hotp_only = false;)
            /* coordinate with hotp_only any additional option changes */
            IF_RETURN_AFTER_CALL(options->ret_after_call =
                                     DEFAULT_OPTION_VALUE(ret_after_call);)
            IF_RCT_IND_BRANCH(options->rct_ind_call = DEFAULT_OPTION_VALUE(rct_ind_call);)
            IF_RCT_IND_BRANCH(options->rct_ind_jump = DEFAULT_OPTION_VALUE(rct_ind_jump);)
            options->vm_size = DEFAULT_OPTION_VALUE(vm_size);
            IF_GBOP(options->gbop = DEFAULT_OPTION_VALUE(gbop);)
            options->aslr = DEFAULT_OPTION_VALUE(aslr);
            options->aslr_cache = DEFAULT_OPTION_VALUE(aslr_cache);

            REENABLE_RESET(options);
            IF_KSTATS(options->kstats = true);
        }
    },
    "client process protection", STATIC, OP_PCACHE_NOP)

#    ifdef GBOP
/* Generically Bypassable Overflow Protection in user mode */
OPTION_DEFAULT(uint, gbop, 0 /* GBOP_DISABLED */, "GBOP control, from GBOP_OPTION")
OPTION_ALIAS(O, gbop, 0 /* GBOP_DISABLED */, STATIC, OP_PCACHE_NOP)
DYNAMIC_OPTION_DEFAULT(uint, gbop_frames, 0,
                       "GBOP stack backtrace frame depth") /* >0 NYI */

OPTION_DEFAULT(uint, gbop_include_set,
               0x1        /* GBOP_SET_NTDLL_BASE */
                   | 0x2  /* KERNEL32 BASE */
                   | 0x4  /* MSVCRT BASE */
                   | 0x8  /* WS2_32 BASE */
                   | 0x10 /* WININET BASE */
                   | 0x20 /* USER32 BASE */
                   | 0x40 /* SHELL32 BASE */
               /* FIXME: case 8006 should enable MORE NTDLL KERNEL32 MSVCRT WS2_32 in a
                  later round */
               ,
               "GBOP policy group control, 0 means all")
/* bit positions are as defined in GBOP_ALL_HOOKS */

OPTION_DEFAULT(uint, gbop_last_hook, 0 /* automatically determine number of hooks */,
               /* note this should be internal, but currently provides good
                * control over the performance impact of having extra GBOP hooks
                */
               "GBOP number of hooks length, crude override")

/* FIXME: may want to duplicate the above options for the extra
 * hooks, that may have different flags and different frame depth
 */
OPTION_DEFAULT(liststring_t, gbop_include_list,
               /* NYI: case 7127 list of additional hook points module!func */
               /* FIXME: should allow module!* for stress testing */
               EMPTY_STRING,
               "include for GBOP these ;-separated module!func descriptors, append")
OPTION_DEFAULT(
    liststring_t, gbop_exclude_list,
    /* case 7127 list of GBOP hook points to turn the mode off (disable hook) for */
    /* e.g. 'KERNEL32.dll!FreeLibrary;WININET.dll!*',
     * Note the only wildcards supported are module.dll!*,
     * or '*' as a stress test option to exclude all hooks.
     */
    EMPTY_STRING,
    "disable GBOP hook for these ;-separated module!func descriptors, append")
OPTION_DEFAULT(liststring_t, exempt_gbop_from_default_list, EMPTY_STRING,
               "allow GBOP violations from these ;-separated modules")
OPTION_DEFAULT(liststring_t, exempt_gbop_from_list, EMPTY_STRING,
               "allow GBOP violations from these ;-separated modules, append")

/* FIXME: case 7127 - can make all gbop options dynamic, though
 * extra hook injection will need a more explicit nudge-like
 * mechanism
 */
#    endif /* GBOP */
/* FIXME: temporary fix for case 9467 - option to disable if not needed */
OPTION_DEFAULT(bool, mute_nudge, true, "mute nudges for thin_clients")
#endif /* WINDOWS */

/* Pseudo Random Number Generator seed affects all random number users:
 * (currently vm_max_offset, aslr_dll_offset, aslr_dll_pad) */
OPTION_DEFAULT(uint, prng_seed, 0 /* get a good seed from the OS */,
               "if non-0 allows reproducible pseudo random number generator sequences")

/* TODO i#4045: Remove this define. */
#if defined(TRACE_HEAD_CACHE_INCR)
OPTION_DEFAULT(bool, pad_jmps, false,
               "nop pads jmps in the cache that we might need to patch so that the "
               "offset doesn't cross a L1 cache line boundary (necessary for atomic "
               "linking/unlinking on an mp machine)")
#else
/* No need to pad on ARM with fixed-width instructions */
OPTION_DEFAULT(bool, pad_jmps, IF_X86_ELSE(true, false),
               "nop pads jmps in the cache that we might need to patch so that the "
               "offset doesn't cross a L1 cache line boundary (necessary for atomic "
               "linking/unlinking on an mp machine)")
#endif
OPTION_DEFAULT_INTERNAL(bool, pad_jmps_return_excess_padding, true,
                        "if -pad_jmps returns any excess requested memory to fcache")
OPTION_DEFAULT_INTERNAL(bool, pad_jmps_shift_bb, true,
                        "if -pad_jmps shifts the start_pc for padding the first jmp of a "
                        "bb instead of inserting a nop")
OPTION_DEFAULT_INTERNAL(bool, pad_jmps_shift_trace, true,
                        "if -pad_jmps shifts the start_pc for padding the first jmp of a "
                        "trace instead of inserting a nop")
OPTION_DEFAULT_INTERNAL(
    uint, pad_jmps_set_alignment, 0,
    "if non-zero sets the pad_jmps alignment (useful for stress testing -pad_jmps code)")

OPTION_DEFAULT_INTERNAL(bool, ibl_sentinel_check,
                        true, /* case 2174: FIXME: remove when working fine */
                        "check for sentinel overwraps in IBL routine instead of exit")

OPTION_DEFAULT(
    bool, ibl_addr_prefix, false, /* case 5231: FIXME: remove when working fine */
    "uses shorter but slower encode with addr16 prefix in IBL routine and elsewhere")

/* Artificial Slowdown Options */
OPTION_DEFAULT_INTERNAL(uint, slowdown_ibl_found, 0,
                        "add a loop to slow down the IBL hit path")

#ifdef ARM
/* Provides a nice debugging option for identifying the most recently
 * executed fragment.
 */
OPTION_INTERNAL(bool, store_last_pc,
                "Inserts a store of the PC to TLS at the top of each fragment.")
#endif

/* Stress Testing Options */
OPTION_DEFAULT_INTERNAL(bool, stress_recreate_pc, false,
                        "stress test recreate pc after each trace or bb")
OPTION_COMMAND_INTERNAL(
    bool, stress_recreate_state, false, "stress_recreate_state",
    {
        if (options->stress_recreate_state)
            options->stress_recreate_pc = true;
    },
    "stress test recreate state after each trace or bb", STATIC, OP_PCACHE_NOP)
OPTION_DEFAULT_INTERNAL(bool, detect_dangling_fcache, false,
                        "detect any execution of a freed fragment")
OPTION_DEFAULT_INTERNAL(
    bool, stress_detach_with_stacked_callbacks, false,
    "detach once a thread has 2 levels of nested callbacks (for internal testing)")
OPTION_DEFAULT_INTERNAL(
    bool, detach_fix_sysenter_on_stack, true /* default true */,
    "if false then detach does not fix sysenter callbacks on the stack and instead "
    "uses the emitted d_r_dispatch code used for other system calls (a fairly minor "
    "transparency violation).  Used for internal testing.")

/* for stress testing can use 1 */
OPTION_DEFAULT_INTERNAL(uint, vmarea_initial_size, 100, "initial vmarea vector size")
/* FIXME: case 4471 should start smaller and double instead */
OPTION_DEFAULT_INTERNAL(uint, vmarea_increment_size, 100,
                        "incremental vmarea vector size")
OPTION_INTERNAL(uint_addr, stress_fake_userva,
                "pretend system address space starts at this address (case 9022)")

/* degenerate options: only used for run-once testing (case 3990) */
OPTION_INTERNAL(bool, unsafe_crash_process, "unsafe: generates a DR exception")
OPTION_INTERNAL(bool, unsafe_hang_process, "unsafe: hang the process")

/* unsafe experimental options */
OPTION_DEFAULT_INTERNAL(bool, unsafe_ignore_overflow, false,
                        "do not preserve OF flag, unsafe")
OPTION_COMMAND_INTERNAL(
    bool, unsafe_ignore_eflags, false, "unsafe_ignore_eflags",
    {
        if (options->unsafe_ignore_eflags) { /* else leave alone */
            options->unsafe_ignore_eflags_trace = options->unsafe_ignore_eflags;
            options->unsafe_ignore_eflags_prefix = options->unsafe_ignore_eflags;
            options->unsafe_ignore_eflags_ibl = options->unsafe_ignore_eflags;
        }
    },
    "do not preserve EFLAGS on any part of ind br handling, unsafe", STATIC,
    OP_PCACHE_NOP)

OPTION_DEFAULT_INTERNAL(bool, unsafe_ignore_eflags_trace, false,
                        "do not preserve EFLAGS on in-trace cmp, unsafe")
OPTION_DEFAULT_INTERNAL(bool, unsafe_ignore_eflags_prefix, false,
                        "do not preserve EFLAGS on prefixes, unsafe")
OPTION_DEFAULT_INTERNAL(bool, unsafe_ignore_eflags_ibl, false,
                        "do not preserve EFLAGS in ibl proper, unsafe")

OPTION_DEFAULT(liststring_t, ignore_assert_list, EMPTY_STRING,
               "convert into warnings these ;-separated assert identifiers")
/* Should be an exact match of message after Internal Error.
 * Most common ones look like 'arch/arch.c:142', but could also
 * look like 'Not implemented @arch/arch.c:142' or
 * 'Bug #4809 @arch/arch.c:145;Ignore message @arch/arch.c:146'
 */

/* Needed primarily for clients but technically all configurations
 * can have racy crashes at exit time (xref PR 470957).
 */
OPTION(bool, synch_at_exit, "synchronize with all threads at exit in release build")
OPTION(bool, multi_thread_exit,
       "do not guarantee that process exit event callback is invoked single-threaded")
OPTION(bool, skip_thread_exit_at_exit, "skip thread exit events at process exit")
OPTION(bool, unsafe_ignore_takeover_timeout,
       "ignore timeouts trying to take over one or more threads when initializing, "
       "leaving those threads native, which is potentially unsafe")
OPTION_DEFAULT(uint, takeover_timeout_ms, 30000,
               "timeout in milliseconds for each thread when taking over at "
               "initialization/attach.  Reaching a timeout is fatal, unless "
               "-unsafe_ignore_takeover_timeout is set.")

#ifdef EXPOSE_INTERNAL_OPTIONS
OPTION_NAME(bool, optimize, " synthethic", "set if ANY opts are on")

#    define OPTIMIZE_OPTION(type, name)                                                  \
        OPTION_COMMAND(                                                                  \
            type, name, 0, #name, { options->optimize = true; }, "optimization", STATIC, \
            OP_PCACHE_NOP)

#    ifdef SIDELINE
OPTION(bool, sideline, "use sideline thread for optimization")
#    endif
/* optimizations */

#    if 0 /* this flag does nothing yet...disable so people don't try to use it */
    OPTION(uint, aggressiveness, "level of aggressiveness in optimizations")
#    endif

OPTIMIZE_OPTION(bool, prefetch)
OPTIMIZE_OPTION(bool, rlr)
OPTIMIZE_OPTION(bool, vectorize)
OPTIMIZE_OPTION(bool, unroll_loops)

OPTIMIZE_OPTION(bool, instr_counts)
OPTIMIZE_OPTION(bool, stack_adjust)
#    ifdef LOAD_TO_CONST
OPTIMIZE_OPTION(bool, loads_to_const)
OPTIMIZE_OPTION(bool, safe_loads_to_const)
#    endif

OPTIMIZE_OPTION(uint, remove_dead_code) /* aggressiveness level */
OPTIMIZE_OPTION(uint, constant_prop)    /* aggressiveness level */
/* 2 digits, first controls local aggressiveness second global */
/* aggressiveness, i.e. 14 is local aggressiveness 1 and global */
/* aggressiveness 4, possible values are 0, 1, 2 for global and */
/* 0, 1, 2, 3 for local, larger numbers = more aggressive at the */
/* possible (but hopefully unlikely) expense of correctness    */

OPTIMIZE_OPTION(bool, call_return_matching)
OPTIMIZE_OPTION(bool, remove_unnecessary_zeroing) // FIXME: unnecessarily long option
OPTIMIZE_OPTION(bool, peephole)
#    undef OPTIMIZE_OPTION
#endif /* EXPOSE_INTERNAL_OPTIONS */
#ifdef HOT_PATCHING_INTERFACE
OPTION_DEFAULT(bool, hot_patching, false, "enable hot patching")

/* This is used to create forensics files when a hot patch event is
 * logged.  There can be numerous hotpatch events and dumping a
 * forensics file for each one by default would result in too many,
 * hence the need for this option.  Note: hot patch exceptions and
 * internal errors are not hot patch requested events, so this option
 * won't be used to control forensics files for those.
 */
DYNAMIC_OPTION_DEFAULT(bool, hotp_diagnostics, false,
                       "produces forensics for hot patch events")
/* There are many technical challenges to switching dynamically between
 * full core control mode and hotp_only mode.  So hotp_only is not DYNAMIC.
 * TODO: Revisit when starting on "attach" mode.
 * NOTE: hotp_only specifies the non-code cache mode, not liveshields; for
 *       liveshields must use the -liveshields option.
 */
OPTION_COMMAND(
    bool, hotp_only, false, "hotp_only",
    {
        if (options->hotp_only) {
            IF_RETURN_AFTER_CALL(options->ret_after_call = false;)
            IF_RCT_IND_BRANCH(options->rct_ind_call = OPTION_DISABLED;)
            IF_RCT_IND_BRANCH(options->rct_ind_jump = OPTION_DISABLED;)

            /* no kstats for -hotp_only; case 6837 */
            IF_KSTATS(options->kstats = false);

            /* reset has no meaning for hotp_only; see case 8389 */
            DISABLE_RESET(options);

            /* -low and -client set their sizes afterward so no conflict */
            options->vm_size = 32 * 1024 * 1024; /* 32MB */

            options->thin_client = false; /* Case 9037. */
            options->native_exec = false;

            /* FIXME: add other options we should turn off */
            /* FIXME: coordinate with -client any other option changes
             * _required_ for -hotp_only */
        }
    },
    "enable hot patching only mode, i.e., no code cache", STATIC, OP_PCACHE_NOP)
/* NOTE: as of today probe_api and liveshields are mutually exclusive, i.e,
 * ls-defs and probes can't be specified by one or more clients.  The
 * ultimate goal is to abstract out the probe api and convert LiveShields
 * into a client using probe api, at which point -probe_api will be the
 * same as -hot_patching.  The same applies to gbop too, i.e., excluded
 * when probe_api is turned on; gbop & livshields can co-exist as before.
 */
OPTION_COMMAND(
    bool, liveshields, false, "liveshields",
    {
        if (options->liveshields) {
            options->hot_patching = true;
            options->hotp_diagnostics = true;
            options->probe_api = false;
        }
    },
    "enables LiveShields", STATIC, OP_PCACHE_NOP)
#endif                 /* HOT_PATCHING_INTERFACE */
#ifdef PROCESS_CONTROL /* case 8594 */
/* Dynamic because it can be turned on or off using a nudge. */
DYNAMIC_OPTION_DEFAULT(uint, process_control, 0,
                       "sets process control mode {off,allowlist,blocklist} thereby "
                       "deciding if a process is allowed to execute or not")
/* FIXME: remove this after md5s are obtained from a mapped file; case 9252.*/
DYNAMIC_OPTION_DEFAULT(uint, pc_num_hashes, 100,
                       "sets the number of hashes a process control hashlist can contain")

/* detect_mode for process_control; see case 10610.  Only reason to have it
 * separate is that it is distinctly different than other core features,
 * and is exposed as orthogonal to other security features.
 *
 * FIXME: having a separate detect_mode option for each
 * security mechanism won't scale even if each used the OPTION_* flags
 * internally; should make the actual security mechanism parameterized to
 * take in OPTION_* flags (which can't be done today because OPTION_* flags
 * assume only on or off state and things like process control, gbop, aslr
 * have more than one state, in some cases, they have their own set of
 * operational flags) or parameterize individual actions like
 * detect_mode, dumpcore, remediation action, forensics file, etc.  */
DYNAMIC_OPTION_DEFAULT(
    bool, pc_detect_mode, false,
    "provides detect_mode for process control independent of -detect_mode")

/* Case 11023: don't produce forensices by default; needless load for EV. */
DYNAMIC_OPTION_DEFAULT(
    bool, pc_diagnostics, false,
    "provides forensics for process control independent of -diagnostics")
#endif

/* thin_client mode is just a light weight mode in which the core
 * executes where there is no code cache, hotp_only, gbop or aslr.
 * It hooks one or two system calls, enough to follow into child
 * processes.  Though it is intended to be used for process_control
 * today, it has value independent of process_control.  One example
 * is to inflate/switch the core to hotp_only, gbop or aslr on the
 * fly for any unprotected process, which I think would be very handy.
 * Case 8576.
 */
OPTION_COMMAND(
    bool, thin_client, false, "thin_client",
    {
        if (options->thin_client) {
            /* Will be running native mostly, so need native_exec_syscalls
             * to hook syscalls to follow children.
             */
            options->native_exec_syscalls = true;

            /* thin_client is just that, thin; so no hot patching, gbopping
             * or aslring here.
             */
            IF_HOTP(options->hot_patching = false;)
            IF_HOTP(options->hotp_only = false;)
            IF_GBOP(options->gbop = 0;)
            IF_WINDOWS(options->aslr = 0;)

            /* similarly, client/low/medium modes are incompatible with
             * thin_client; though check_options_compatibility() enforces this,
             * this is a special case as the user may legitimately set both
             * -client and -thin_client.  Mostly for debugging; case 9037. */
            IF_WINDOWS(options->client = false;)
            IF_WINDOWS(options->low = false;)
            IF_WINDOWS(options->medium = false;)

            /* thin_client mode is intended to have a low foot print;
             * reserving the default 128 mb takes 256 kb of page table
             * space (case 8491), so reserve just 4 mb, in case we inflate
             * to hotp_only mode.
             */
            options->vm_size = 4 * 1024 * 1024;

            /* Don't randomize the core heap; cygwin app's stack & heaps
             * will move, causing them to crash.  Also memory
             * contiguity expected by .NET apps would be broken.  The
             * problem is when we inflate we still won't be randomized!
             */
            options->vm_base = 0;
            options->vm_max_offset = 0;

            /* Reset has no meaning for thin_client; see case 8389. */
            DISABLE_RESET(options);

            /* No kstats for -thin_client - same issue as case 6837. */
            IF_KSTATS(options->kstats = false); /* See case 8869 also. */
        }
    },
    "run dr in a light weight mode with nothing but a few hooks", STATIC, OP_PCACHE_NOP)

#undef OPTION
#undef OPTION_NAME
#undef OPTION_DEFAULT

#undef DYNAMIC_OPTION
#undef DYNAMIC_OPTION_DEFAULT
