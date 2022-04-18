/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2005-2007 Determina Corp. */

/*
 * hotpatch.c - Hot patching mechanism
 */

#include "globals.h"
#include "fragment.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "decode.h"
#include "instrument.h"
#include "hotpatch.h"
#include "hotpatch_interface.h"
#include "probe_api.h"
#include "moduledb.h" /* macros for nudge; can be moved with nudge to os.c */

#ifndef WINDOWS
#endif

#include <limits.h> /* for ULLONG_MAX */

#include "fcache.h" /* for fcache_reset_all_caches_proactively */
#ifdef GBOP
#    include "aslr.h"
#endif

#include "perscache.h"
#include "synch.h"

/* Note: HOT_PATCHING_INTERFACE refers to the mechanism for injecting code at
 *       arbitrary points in the application text.  It was formerly known as
 *       constraint injection.  It has nothing to do with the DR mechanism that
 *       allows for dynamically changing existing instructions, as indicated
 *       by INSTR_HOT_PATCHABLE in instr.h
 */
#ifdef HOT_PATCHING_INTERFACE /* Around the whole file */
/*----------------------------------------------------------------------------*/
/* Local typed constants. */

/* Local untyped constants. */

/* defined for non-GBOP as well since used for -probe_api */
#    define HOTP_ONLY_GBOP_PRECEDENCE 10

/* Limits for vulnerability data provided by the constraint writer. */
enum {
    MIN_HOTP_INTERFACE_VERSION = HOTP_INTERFACE_VERSION,
    MAX_HOTP_INTERFACE_VERSION = HOTP_INTERFACE_VERSION,
    MIN_NUM_VULNERABILITIES = 1,
    MAX_NUM_VULNERABILITIES = 10000,
    MIN_VULNERABILITY_ID_LENGTH = 1,
    MAX_VULNERABILITY_ID_LENGTH = 32,
    MIN_POLICY_ID_LENGTH = HOTP_POLICY_ID_LENGTH,
    MAX_POLICY_ID_LENGTH = HOTP_POLICY_ID_LENGTH,
    MIN_POLICY_VERSION = 1,
    MAX_POLICY_VERSION = 10000,
    MIN_NUM_SETS = 1,
    MAX_NUM_SETS = 10000,
    MIN_NUM_MODULES = 1,
    MAX_NUM_MODULES = 10000,

    /* We don't expect PE files to be more than 1 GB in size. */
    MAX_MODULE_SIZE = 1024 * 1024 * 1024,

    /* Can have PEs with time stamp as zero, though fairly unlikely; zero
     * checksum is more likely, zero file version is somewhat likely and zero
     * {image,code} size is extremely unlikely.  The max values though are
     * unlikely to exist in reality, we use these limits as the don't care
     * values for timestamp, checksum, {image,code} size & file version.
     */
    MIN_PE_TIMESTAMP = 0,
    MAX_PE_TIMESTAMP = UINT_MAX,
    PE_TIMESTAMP_IGNORE = UINT_MAX,
    PE_TIMESTAMP_UNAVAILABLE = PE_TIMESTAMP_IGNORE - 1,

    MIN_PE_CHECKSUM = 0,
    MAX_PE_CHECKSUM = UINT_MAX,
    PE_CHECKSUM_IGNORE = UINT_MAX,
    PE_CHECKSUM_UNAVAILABLE = PE_CHECKSUM_IGNORE - 1,

    MIN_PE_IMAGE_SIZE = 0,
    MAX_PE_IMAGE_SIZE = UINT_MAX,
    PE_IMAGE_SIZE_IGNORE = UINT_MAX,
    PE_IMAGE_SIZE_UNAVAILABLE = PE_IMAGE_SIZE_IGNORE - 1,

    MIN_PE_CODE_SIZE = 0, /* kbdus.dll has only data in it */
    MAX_PE_CODE_SIZE = UINT_MAX,
    PE_CODE_SIZE_IGNORE = UINT_MAX,
    PE_CODE_SIZE_UNAVAILABLE = PE_CODE_SIZE_IGNORE - 1,

    MIN_PE_FILE_VERSION = 0,
    MAX_PE_FILE_VERSION = ULLONG_MAX,
    PE_FILE_VERSION_IGNORE = IF_WINDOWS_ELSE(MODULE_FILE_VERSION_INVALID, ULLONG_MAX),
    PE_FILE_VERSION_UNAVAILABLE = PE_FILE_VERSION_IGNORE - 1,

    MIN_NUM_PATCH_POINT_HASHES = 1,
    MAX_NUM_PATCH_POINT_HASHES = 10000,
    MIN_HASH_START_OFFSET = 1,
    MAX_HASH_START_OFFSET = MAX_MODULE_SIZE, /* Can't exceed module size. */
    MIN_HASH_LENGTH = 1,
    MAX_HASH_LENGTH = MAX_MODULE_SIZE, /* Can't exceed module size. */
    MIN_HASH_VALUE = 0,
    MAX_HASH_VALUE = UINT_MAX,

    MIN_NUM_PATCH_POINTS = MIN_NUM_PATCH_POINT_HASHES,
    MAX_NUM_PATCH_POINTS = MAX_NUM_PATCH_POINT_HASHES,
    MIN_PATCH_OFFSET = 1,
    MAX_PATCH_OFFSET = MAX_MODULE_SIZE, /* Can't exceed module size. */
    MIN_PATCH_PRECEDENCE = 1,
    MAX_PATCH_PRECEDENCE = 10000,
    MIN_DETECTOR_OFFSET = 1,
    /* Hot patch dlls shouldn't be anywhere near 10 MB in size;
     * this check is just to catch some wrong file being loaded by accident.
     * Today a typical hot patch is far less than 1k in size, so to hit 10 MB
     * we would need a mininum of 10000 constraints of 1k each - unlikely.
     */
    MAX_DETECTOR_OFFSET = 10 * 1024 * 1024,
    /* Protectors should exist for all hot patches; even if it does nothing. */
    MIN_PROTECTOR_OFFSET = 1,
    MAX_PROTECTOR_OFFSET = MAX_DETECTOR_OFFSET,
    /* Zero offset either means there is no protector or no control flow
     * change is requested by the protector.
     */
    MIN_RETURN_ADDR = 0,
    /* We don't expect return addresses to be across modules; given that we
     * don't expect a module to be more than 1 GB in size, the return address
     * offset shouldn't be more than 1 GB too.
     */
    MAX_RETURN_ADDR = MAX_PATCH_OFFSET,
    MIN_MODE = HOTP_MODE_OFF,
    MAX_MODE = HOTP_MODE_PROTECT,

    /* case 8051: > 256KB per-process means we should start thinking about
     * sharing.  24-Apr-07: sharing is in plan for 4.3 or 4.4; upping to 384k.
     * Note: this is used only in debug builds; release builds can handle all
     * sizes as long as we don't run out of memory. */
    MAX_POLICY_FILE_SIZE = 384 * 1024

};
#    define PE_NAME_IGNORE "*" /* Can't have strings in the enum. */
#    define PE_NAME_UNAVAILABLE '\0'
/*----------------------------------------------------------------------------*/
/* Local type definitions. */

/* Module signature is used to uniquely describe a module, in our case, a Win32
 * PE module.
 */
typedef struct { /* xref case 4688 */
    const char *pe_name;

    /* Don't-care values for pe_{checksum,timestamp,{image,code}_size,
     * file_version} will be their respective MAX values.  See enum above.
     */
    uint pe_checksum;
    uint pe_timestamp;
    size_t pe_image_size;

    /* Refers to the sum of the unpadded sizes of all executable sections in the
     * PE image.  The section size used is from get_image_section_unpadded_size()
     * which equals VirtualSize (unless that is 0 in which case it equals SizeOfRawData).
     *
     * As an aside note that VirtualSize usually has no alignment padding while
     * SizeOfRawData is typically padded to FileAlignment (the image loader pads
     * VirtualSize to SectionAligment), so SizeOfRawData is often larger than
     * VirtualSize for fully initialized sections (- this is the opposite of how it is
     * in unix/elf, i.e., raw/file size is usually smaller than virtual/mem size because
     * the latter does the alignment; also in unix, there are usually two different
     * mmaps as opposed to one on windows to load the image).  Though xref case 5355,
     * what is actually accepted (and generated by some compilers) differs from what is
     * typical/legal in pe specifications.
     *
     * Using _code_ rather than _text_ in the name because text usually refers
     * only to the .text section.
     */
    size_t pe_code_size;

    /* Found in the resource section, some PE file may not have it, in which
     * case it will be set to its don't-care value.
     */
    uint64 pe_file_version;
} hotp_module_sig_t;

/* A patch point describes what application address to patch and the address of
 * the hot patches that will be used for patching.  If a hot patch (only a
 * protector) intends to change the flow of application's execution, then the
 * address to which control should go to after the hot patch is executed is
 * also specified.  A precedence attribute defines the order (rank) in which a
 * particular patch is to be applied if more than one need to be applied at the
 * same application offset.  All addresses are relative to the base of the
 * module.
 */
/* TODO: typedef uint app_rva_t to define offsets; app_pc is actually an address,
 *       not an offset, so don't use it for defining offsets
 */
/* app_pc is a pointer, not an offset; using it to a compute pointer with
 * base address gives a compiler error about adding two pointers.  Hence, a new
 * type to define module offsets.
 */
typedef struct {
    app_rva_t offset; /* offset relative to the base of the module where
                       * thepatch is to be applied */

    /* TODO: clearly split each structure into read only and runtime data
     * because things are tending to go out of synch again; can create a
     * parallel tree later on.
     */
    app_rva_t detector_fn; /* Offset of the detector function from the base
                            * of the hot patch dll */
    app_rva_t protector_fn;
    app_rva_t return_addr;

    /* NYI (was never needed in practice, though at design time I thought this
     * was needed for supporting multiple patches at the same address); lower
     * numbers mean high precedence.
     */
    uint precedence;

    /*------------------------------------------------------------------------*/
    /* The following fields are part of runtime policy/vulnerability data, not
     * part of vulnerability definitions, i.e., shouldn't be shared across
     * processes.
     */
    /* TODO: num_injected at the vulnerability level; relevant here? */

    /* Buffer to hold the trampoline with which a patch point was hooked in
     * order to execute a hot patch in hotp_only mode.  Should be NULL for
     * regular hot patching, i.e., with fcache.
     */
    byte *trampoline;

    /* Pointer to the copy of app code that resides inside the trampoline,
     * that gets executed at the end of trampoline execution; this is the app
     * code that existed at the injection point.  Used only by hotp_only.
     */
    byte *app_code_copy;

    /* Pointer to the cti target inside the trampoline (the one that is used to
     * implement AFTER_INTERCEPT_LET_GO_ALT_DYN) that is used to
     * change control flow.  Used only in hotp_only mode for a patch point
     * that requests a control flow change, i.e., has non-zero return_addr.
     */
    byte *tramp_exit_tgt;
} hotp_patch_point_t;

/* Experiments showed that the maximum size of a single interception
 * trampoline/hook is about 400 to 450 bytes, so 512 should be adequate.
 */
#    define HOTP_ONLY_TRAMPOLINE_SIZE 512
#    define HOTP_ONLY_NUM_THREADS_AT_INIT -1

/* A patch region size of 5 is used for hotp_only mode.  This is done so
 * that the same vm_area_t vector (hotp_patch_point_areas) can be used for
 * patch point overlap checks and address lookup.  Note: 5 is the minimum
 * bytes needed to encode/insert a direct jmp with 32-bit displacement, i.e.,
 * a hook.
 * For hotp in code cache, all patch regions are points, so patch region size
 * 1 is used.  In this mode it is used only for patch address lookup.
 *
 * NOTE: Investigate issues when implementing hotp_only for native_exec dlls as
 *       we would have to have regions with different sizes - might trigger a
 *       few hotp asserts.
 *
 * Use -1 as an error catching value if this macro is used without -hot_patching.
 */
#    define HOTP_ONLY_PATCH_REGION_SIZE (5)
#    define HOTP_CACHE_PATCH_REGION_SIZE (1)
#    define HOTP_BAD_PATCH_REGION_SIZE (-1)
#    define HOTP_PATCH_REGION_SIZE                                       \
        (DYNAMO_OPTION(hot_patching)                                     \
             ? (DYNAMO_OPTION(hotp_only) ? HOTP_ONLY_PATCH_REGION_SIZE   \
                                         : HOTP_CACHE_PATCH_REGION_SIZE) \
             : HOTP_BAD_PATCH_REGION_SIZE)

/* This structure is used to define a hash value for a specified region
 * around a patch point as decided by the hot patch writer.  This hash, which
 * is provided by the hot patch writer will be used at run time as part of the
 * mechanism to identify a given PE module for injecting hot patches.
 */
typedef struct {
    /* Offset, relative to the base of the module, that should be used as the
     * starting point of hash computation string; for the module to be patched.
     */
    app_rva_t start;
    uint len; /* number of bytes to be used for hash computation */
    uint hash_value;
} hotp_patch_point_hash_t;

typedef struct {
    hotp_module_sig_t sig;
    uint num_patch_points;
    hotp_patch_point_t *patch_points;
    uint num_patch_point_hashes;
    hotp_patch_point_hash_t *hashes;

    /* Data computed at run time; should be zeroed out at read time */
    bool matched; /* True if current module is loaded & matched. */
    app_pc base_address;
} hotp_module_t;

typedef struct {
    uint num_modules;
    hotp_module_t *modules;
} hotp_set_t;

/* Note: status and statistics are kept in a separate structure to allow for
 *       easy output, either via a file or via read only memory.
 * Note: whole struct is runtime data; hence separated out.
 */
typedef struct {
    hotp_exec_status_t exec_status;

    /* Points to the one in hotp_policy_status_t to avoid duplication. */
    hotp_inject_status_t *inject_status;

    /* TODO: num_injected at the vulnerability level */
    /* TODO: decide on the size of stats (uint or uint64) before finalizing
     *       interface with jim
     */
    uint64 num_detected;
    uint64 num_not_detected;
    uint64 num_detector_error;
    uint64 num_protected;
    uint64 num_not_protected;
    uint64 num_kill_thread;
    uint64 num_kill_process;
    uint64 num_raise_exception;
    uint64 num_change_control_flow;
    uint64 num_protector_error;
    uint64 num_aborted;
} hotp_vul_info_t;

/* The types are defined as unique bit flags because it may be possible in
 * future that we have a case that is more than one type.
 * For example a hot patch with a symbolic offset may be
 * HOTP_TYPE_SYMBOLIC_TYPE | HOTP_TYPE_HOT_PATCH, whereas a gbop hook may be
 * HOTP_TYPE_SYMBOLIC_TYPE | HOTP_TYPE_GBOP_HOOK.
 */
typedef enum {
    /* This represents the patches that fix vulnerabilities, as described by
     * the hot patch injection design.
     */
    HOTP_TYPE_HOT_PATCH = 0x1,

    /* This represents all gbop hook points.  This type is different in that it:
     * 1. Isn't specified by a config file; well not as of now (FIXME?),
     * 2. Is specified by gbop_hooks and/or gbop_include_list (FIXME: NYI),
     * 3. Can't be turned off by modes file; will not as of now (FIXME?),
     * 4. Can be turned of by gbop_exclude_list (FIXME: NYI),
     * 5. Uses a symbolic name rather than identifying the PE uniquely,
     * 6. Has a generic detector and protector which is part of the core, and,
     * 7. Uses the core defaults for events, actions, dumps & forensics
     *      (FIXME: NYI).
     */
    HOTP_TYPE_GBOP_HOOK = 0x2,

    /* Currently will be exclusive with HOTP_TYPE_{HOT_PATCH,GBOP_HOOK},
     * eventually will co-exist.
     */
    HOTP_TYPE_PROBE = 0x4
} hotp_type_t;

/* hotp_vul_t defines a vulnerability */
/* the entire expanded structure of hotp_vul_t consists of constant data,
 *  except for a couple of runtime data; this is so that policies can be
 *  easily read in from file/memory in a binary format, thus eliminating the
 *  need to do any data formatting/processing inside the core.
 */
typedef struct {
    const char *vul_id;

    /* policy_id is of the format XXXX.XXXX so that it can be used to
     * generate the corresponding threat_id; so use
     *      char policy_id[MAX_POLICY_ID_LENGTH + 1];
     * to be consistent with hotp_policy_status_t;  TODO
     * not done now because SET_STR doesn't handle arrays.
     */
    const char *policy_id;
    uint policy_version;
    const char *hotp_dll;
    const char *hotp_dll_hash;
    hotp_policy_mode_t mode;

    uint num_sets;
    hotp_set_t *sets;

    /* Data computed at run time; should be zeroed out at read time */
    hotp_vul_info_t *info;
    app_pc hotp_dll_base;
    /* TODO: if policy data is going to be shared across multiple processes,
     *       info (i.e., runtime data) can't be part of this; a parallel runtime
     *       structure must be created;  not a big issue till hot patches reach
     *       thousands in number
     */

    /* FIXME: right now this isn't specified by the config file because
     * config files are assumed to define only hotpatches.  Also, gbop_hooks
     * are added to the table by a different routine, so there is no room
     * for ambiguity.  If we decide to use the config file for all, then
     * this type should come from there - that would involve revving up
     * the hotp interface, i.e., engine version.
     * Note: probe types are provided by client libraries directly via
     *       dr_register_probes.
     */
    hotp_type_t type;

    /* The following fields were introduced for probe api. */

    /* Unique ID for each probe; must be unique across different clients in the
     * same process to avoid one client from controlling another's probes. */
    unsigned int id;
} hotp_vul_t;

/* Maintain a list of vulnerability tables so that they can be freed at exit
 * time.  Nudge for policy reading creates new tables.  The old ones should
 * be left alone so that races between hot patch execution and table freeing
 * are avoided (case 5521).  All such tables are freed during DR exit.
 * FIXME: Release tables using a ref_count in case there are many & memory usage
 *        is high.  It is highly unlikely that a given process will get more
 *        than a couple of policy read nudges during its lifetime.
 *        memory usage issue not correctness one, work on it after beta.
 */
typedef struct hotp_vul_tab_t {
    hotp_vul_t *vul_tab;
    uint num_vuls;
    struct hotp_vul_tab_t *next;
} hotp_vul_tab_t;

/* TODO: for now this just has debug information; later on move all hot patch
 * related globals into this structure.  The debug variable listed below needed
 * to be updated during loader activity and that conflicts with our data segment
 * protection.
 */
#    ifdef DEBUG
typedef struct hotp_globals_t {
    /* The variables below help catch removing the same patch twice and
     * injecting it twice, which is ok only for loader safety.  Technically
     * each patch point should have this variable, but given that the loader
     * loads/relocates one dll at a time, this should be ok.
     */
    bool ldr_safe_hook_removal;   /* used only in -hotp_only mode */
    bool ldr_safe_hook_injection; /* used only in -hotp_only mode */
} hotp_globals_t;
#    endif
/*----------------------------------------------------------------------------*/
/* Macro definitions. */

/* These macros serve two purposes.  Firstly they provide a clean interface
 * to access the global hotp_vul_table, so that direct use of the global
 * variable can be avoided.  Secondly they improve readability; given that
 * these structures are nested, accessing a member directly would result in
 * long lines of code, which aren't very readable.
 */
/* TODO: Derek feels that these macros obfuscate the code rather than making
 *       them readable, which is opposite to what I thought.  Try using local
 *       variables and if that looks good, remove these macros.
 */
#    define VUL(vul_table, i) (vul_table[i])
#    define SET(vul_table, v, i) (VUL(vul_table, v).sets[i])
#    define MODULE(vul_table, v, s, i) (SET(vul_table, v, s).modules[i])
#    define SIG(vul_table, v, s, m) (MODULE(vul_table, v, s, m).sig)
#    define PPOINT(vul_table, v, s, m, i) (MODULE(vul_table, v, s, m).patch_points[i])
#    define PPOINT_HASH(vul_table, v, s, m, i) (MODULE(vul_table, v, s, m).hashes[i])

#    define NUM_GLOBAL_VULS (hotp_num_vuls)
#    define GLOBAL_VUL_TABLE (hotp_vul_table)
#    define GLOBAL_VUL(i) VUL(GLOBAL_VUL_TABLE, i)
#    define GLOBAL_SET(v, i) SET(GLOBAL_VUL_TABLE, v, i)
#    define GLOBAL_MODULE(v, s, i) MODULE(GLOBAL_VUL_TABLE, v, s, i)
#    define GLOBAL_SIG(v, s, m) SIG(GLOBAL_VUL_TABLE, v, s, m)
#    define GLOBAL_PPOINT(v, s, m, i) PPOINT(GLOBAL_VUL_TABLE, v, s, m, i)
#    define GLOBAL_HASH(v, s, m, i) PPOINT_HASH(GLOBAL_VUL_TABLE, v, s, m, i)

/* TODO: change it to model ATOMIC_ADD; can't use ATOMIC_ADD directly because
 *       it wants only uint, not uint64 which is what all vulnerability stats
 *       are;  maybe the easy way is to make the vul stat uint, but don't know
 *       if that will result in overflows fairly quickly, esp. for long running
 *       apps.  either way, make this increment non racy, the users of this
 *       macro assume atomic increments.
 */
#    define VUL_STAT_INC(x) ((x)++);

#    define SET_NUM(var, type, limit_str, input_ptr)                                    \
        {                                                                               \
            char *str = hotp_get_next_str(&(input_ptr));                                \
            const char *hex_fmt, *dec_fmt;                                              \
            type temp;                                                                  \
                                                                                        \
            ASSERT(sizeof(type) == sizeof(uint) || sizeof(type) == sizeof(uint64));     \
            hex_fmt =                                                                   \
                (sizeof(type) == sizeof(uint)) ? "0x%x" : "0x" HEX64_FORMAT_STRING;     \
            dec_fmt = (sizeof(type) == sizeof(uint)) ? "%d" : INT64_FORMAT_STRING;      \
                                                                                        \
            if (sscanf(str, hex_fmt, &temp) == 1 || sscanf(str, dec_fmt, &temp) == 1) { \
                if (temp < (type)(MIN_##limit_str) || temp > (type)(MAX_##limit_str))   \
                    goto error_reading_policy; /* Range error */                        \
                (var) = temp;                                                           \
            } else                                                                      \
                goto error_reading_policy; /* Parse error. */                           \
        }

/* FIXME: range check strs for min & max length; null check already done. */
#    define SET_STR_DUP(var, input_ptr)                         \
        {                                                       \
            char *str = hotp_get_next_str(&(input_ptr));        \
                                                                \
            if (str == NULL)                                    \
                goto error_reading_policy;                      \
            (var) = dr_strdup(str HEAPACCT(ACCT_HOT_PATCHING)); \
        }

#    define SET_STR_PTR(var, input_ptr)                  \
        {                                                \
            char *str = hotp_get_next_str(&(input_ptr)); \
                                                         \
            if (str == NULL)                             \
                goto error_reading_policy;               \
            (var) = str;                                 \
        }

#    define SET_STR(var, input_ptr) SET_STR_DUP(var, input_ptr)

#    define HOTP_IS_IN_REGION(region_start, region_size, addr) \
        (((addr) >= (region_start)) && ((addr) < ((region_start) + (region_size))))

/* This checks addresses. */
#    define HOTP_ONLY_IS_IN_TRAMPOLINE(ppoint, addr)      \
        (((ppoint)->trampoline == NULL || (addr) == NULL) \
             ? false                                      \
             : HOTP_IS_IN_REGION((ppoint)->trampoline, HOTP_ONLY_TRAMPOLINE_SIZE, addr))

/* This checks offsets/RVAs. */
#    define HOTP_ONLY_IS_IN_PATCH_REGION(ppoint, addr) \
        (((ppoint)->offset <= 0 || (addr) <= 0)        \
             ? false                                   \
             : HOTP_IS_IN_REGION((ppoint)->offset, HOTP_PATCH_REGION_SIZE, addr))

/* TODO: PR 225550 - make this a better function so that each probe is
 * identified uniquely so as to prevent clients from modifying each others'
 * probes - make it a function of the client name, probe def & this counter.
 * Note: probe id is generated outside hotp_vul_table_lock because of having
 *       to load probe/callback dlls without hitting dr hooks, so updates to
 *       probe_id_counter haver to be atomic.
 */
#    define GENERATE_PROBE_ID() (atomic_add_exchange_int((int *)&probe_id_counter, 4))
/*----------------------------------------------------------------------------*/
/* Local function prototypes. */
static void
hotp_change_control_flow(const hotp_context_t *app_reg_ptr, const app_pc target);

static after_intercept_action_t
hotp_gateway(const hotp_vul_t *vul_tab, const uint num_vuls, const uint vul_index,
             const uint set_index, const uint module_index, const uint ppoint_index,
             hotp_context_t *app_reg_ptr, const bool own_hot_patch_lock);

static void
hotp_free_vul_table(hotp_vul_t *tab, uint num_vuls_alloc);
static hotp_exec_status_t
hotp_execute_patch(hotp_func_t hotp_fn_ptr, hotp_context_t *hotp_cxt,
                   hotp_policy_mode_t mode, bool dump_excpt_info, bool dump_error_info);
static void
hotp_update_vul_stats(const hotp_exec_status_t exec_status, const uint vul_index);
static void
hotp_event_notify(hotp_exec_status_t exec_status, bool protected,
                  const hotp_offset_match_t *inject_point, const app_pc bad_addr,
                  const hotp_context_t *hotp_cxt);
#    if defined(DEBUG) && defined(INTERNAL)
static void
hotp_dump_reg_state(const hotp_context_t *reg_state, const app_pc eip,
                    const uint loglevel);
#    endif
static void
hotp_only_inject_patch(const hotp_offset_match_t *ppoint_desc,
                       const thread_record_t **thread_table, const int num_threads);
static void
hotp_only_remove_patch(dcontext_t *dcontext, const hotp_module_t *module,
                       hotp_patch_point_t *cur_ppoint);
after_intercept_action_t
hotp_only_gateway(app_state_at_intercept_t *state);
static uint
hotp_compute_hash(app_pc base, hotp_patch_point_hash_t *hash);
#    ifdef GBOP
static void
hotp_only_read_gbop_policy_defs(hotp_vul_t *tab, uint *num_vuls);
#    endif

/* TODO: add function prototypes for all functions in this file */
/*----------------------------------------------------------------------------*/
/* Local data. */

hotp_policy_status_table_t *hotp_policy_status_table;

/* FIXME: create hotp_vul_table_t and put these three into it */
static hotp_vul_t *hotp_vul_table;
static uint hotp_num_vuls;
static hotp_vul_tab_t *hotp_old_vul_tabs;

DECLARE_CXTSWPROT_VAR(static read_write_lock_t hotp_vul_table_lock, { { 0 } });
/* Special heap for hotp_only trampolines; heap is executable. */
static void *hotp_only_tramp_heap;
/* Leak to handle case 9593.  This should go if we find a cleaner solution. */
#    if defined(DEBUG) && defined(HEAP_ACCOUNTING)
DECLARE_NEVERPROT_VAR(int hotp_only_tramp_bytes_leaked, 0);
#    endif
/* This is used to cache hotp_only_tramp_heap for handling leak asserts during
 * detach and to track whether or not any hotp_only patch was removed.  Case
 * 9593 & PR 215520. */
static void *hotp_only_tramp_heap_cache;

/* Trampoline area vector; currently used only to identify if a thread is in
 * the middle of hot patch execution during suspension - for multiprocessor
 * safe hot patch removal in hotp_only mode.
 * Kept on the heap for selfprot (case 7957).
 */
static vm_area_vector_t *hotp_only_tramp_areas;

/* This has all the matched patch points, i.e., patch points that have been
 * determined by hotp_process_image() to be ready to be injected.  Only that
 * function adds or removes from this vector because only that function does
 * module matching.
 * The custom data stored is a hotp_offset_match_t structure which describes
 * the patch point precisely in the GLOBAL_VUL_TABLE.
 * For hotp_only this refers to all injected patches because they get injected
 * during match/dll load time.  For fcache based hot patches, this may or may
 * not specify patch injection, but will specify matches.  This is because for
 * hotp_only matching & injection are done in one shot, whereas they are split
 * for fcache based hot patches.
 * This vector is not static, it is on the heap because of selfprot; case 8074.
 * Uses:
 *  1. for hotp_only to solve the overlapping hashes problem (case 7279).
 *  2. for offset lookup for both hotp and hotp_only (case 8132).
 *  3. NYI - all patch removal & injection; perscache stuff (case 10728).
 */
static vm_area_vector_t *hotp_patch_point_areas;

#    ifdef DEBUG
static hotp_globals_t *hotp_globals;
#    endif

/* Global counter used to generate unique ids for probes.  This is updated
 * atomically and isn't guarded by any lock.  See GENERATE_PROBE_ID() for
 * details.
 */
static unsigned int probe_id_counter;
/*----------------------------------------------------------------------------*/
/* Function definitions. */

/* Don't expose the hot patch lock directly outside this module. */
read_write_lock_t *
hotp_get_lock(void)
{
    ASSERT(DYNAMO_OPTION(hot_patching));
    return &hotp_vul_table_lock;
}

static inline app_pc
hotp_ppoint_addr(const hotp_module_t *module, const hotp_patch_point_t *ppoint)
{
    app_pc ppoint_offset;
    ASSERT(module != NULL && ppoint != NULL);
    ASSERT(module->base_address != NULL && ppoint->offset != 0);

    ppoint_offset = module->base_address + ppoint->offset;

    /* The patch point should be inside the code section of a loaded module. */
    ASSERT(is_in_code_section(module->base_address, ppoint_offset, NULL, NULL));

    return ppoint_offset;
}

static void
hotp_ppoint_areas_add(hotp_offset_match_t *ppoint_desc)
{
    hotp_module_t *module;
    hotp_patch_point_t *ppoint;
    hotp_offset_match_t *copy;
    app_pc ppoint_start, ppoint_end;

    ASSERT(ppoint_desc != NULL);
    ASSERT(GLOBAL_VUL_TABLE != NULL && hotp_patch_point_areas != NULL);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    module = &GLOBAL_MODULE(ppoint_desc->vul_index, ppoint_desc->set_index,
                            ppoint_desc->module_index);
    ppoint = &module->patch_points[ppoint_desc->ppoint_index];

    /* Shouldn't be adding to hotp_patch_point_areas if the module hasn't been
     * matched.
     */
    ASSERT(module->matched);
    ppoint_start = hotp_ppoint_addr(module, ppoint);
    ppoint_end = ppoint_start + HOTP_PATCH_REGION_SIZE;

    /* Each matched (or injected) patch point should be added only
     * once and removed only once, so before adding, make sure that it
     * is not already in there.
     */
    ASSERT(!vmvector_overlap(hotp_patch_point_areas, ppoint_start, ppoint_end));

    copy = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, hotp_offset_match_t, ACCT_HOT_PATCHING,
                           PROTECTED);
    *copy = *ppoint_desc;
    vmvector_add(hotp_patch_point_areas, ppoint_start, ppoint_end, (void *)copy);
}

static void
hotp_ppoint_areas_remove(app_pc pc)
{
    hotp_offset_match_t *ppoint_desc;
    DEBUG_DECLARE(bool ok;)

    ASSERT(pc != NULL);
    ASSERT(GLOBAL_VUL_TABLE != NULL && hotp_patch_point_areas != NULL);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    ppoint_desc = (hotp_offset_match_t *)vmvector_lookup(hotp_patch_point_areas, pc);

    DOCHECK(1, {
        hotp_module_t *module;
        hotp_patch_point_t *ppoint;

        /* Shouldn't be trying to remove something that wasn't added. */
        ASSERT(ppoint_desc != NULL);

        /* Verify that the ppoint_desc in the vmvector corresponds to pc. */
        module = &GLOBAL_MODULE(ppoint_desc->vul_index, ppoint_desc->set_index,
                                ppoint_desc->module_index);
        ppoint = &module->patch_points[ppoint_desc->ppoint_index];
        ASSERT(pc == hotp_ppoint_addr(module, ppoint));
    });

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ppoint_desc, hotp_offset_match_t, ACCT_HOT_PATCHING,
                   PROTECTED);

    DEBUG_DECLARE(ok =)
    vmvector_remove(hotp_patch_point_areas, pc, pc + HOTP_PATCH_REGION_SIZE);
    ASSERT(ok);
}

static void
hotp_ppoint_areas_release(void)
{
    app_pc vm_start, vm_end;
    hotp_offset_match_t *ppoint_desc;
    vmvector_iterator_t iterator;

    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    /* Release all patch point descriptors. */
    vmvector_iterator_start(hotp_patch_point_areas, &iterator);
    while (vmvector_iterator_hasnext(&iterator)) {
        ppoint_desc = vmvector_iterator_next(&iterator, &vm_start, &vm_end);
        ASSERT(ppoint_desc != NULL);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ppoint_desc, hotp_offset_match_t,
                       ACCT_HOT_PATCHING, PROTECTED);
    }
    vmvector_iterator_stop(&iterator);

    /* Remove all vm_areas in the vmvector. */
    vmvector_remove(hotp_patch_point_areas, UNIVERSAL_REGION_BASE, UNIVERSAL_REGION_END);
    ASSERT(vmvector_empty(hotp_patch_point_areas));
}

/* Used to read in vulnerability definitions from file */
static char *
hotp_get_next_str(char **start)
{
    char *end = *start, *temp = *start;
    bool dos_line_terminator = false;

    if (start == NULL || *start == NULL)
        return NULL;

    while (*end != '\n' && *end != '\r' && *end != '\0')
        end++;

    if (*end != '\0') {
        if (*end == '\r') {
            if (*(end + 1) == '\n')
                dos_line_terminator = true;
            else
                SYSLOG_INTERNAL_WARNING("Mac OS style line separator!");
        }

        *end++ = '\0';
        if (dos_line_terminator)
            end++;
    }
    *start = end;
    return temp;
}

/* Used to read either the policy file or the modes file. */
enum { POLICY_FILE = 1, MODES_FILE };

static char *
hotp_read_data_file(uint type, size_t *buf_len /* OUT */)
{
    int retval;
    char file[MAXIMUM_PATH];

    ASSERT(type == POLICY_FILE || type == MODES_FILE);
    ASSERT(buf_len != NULL);

    *buf_len = 0;

    retval = d_r_get_parameter(type == POLICY_FILE
                                   ? PARAM_STR(DYNAMORIO_VAR_HOT_PATCH_POLICIES)
                                   : PARAM_STR(DYNAMORIO_VAR_HOT_PATCH_MODES),
                               file, BUFFER_SIZE_ELEMENTS(file));
    if (IS_GET_PARAMETER_FAILURE(retval)) {
        SYSLOG_INTERNAL_WARNING("Can't find %s definition directory name.",
                                (type == POLICY_FILE) ? "policy" : "mode");
        return NULL;
    }

    /* The {defs,modes} file is
     * $DYNAMORIO_HOT_PATCH_POLICIES/<engine>/HOTP_{POLICIES,MODES}_FILENAME
     */
    CAT_AND_TERMINATE(file, "\\" STRINGIFY(HOTP_INTERFACE_VERSION) "\\");
    CAT_AND_TERMINATE(file,
                      type == POLICY_FILE ? HOTP_POLICIES_FILENAME : HOTP_MODES_FILENAME);

    LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Hot patch %s definition file: %s\n",
        (type == POLICY_FILE) ? "policy" : "mode", file);

    return read_entire_file(file, buf_len HEAPACCT(ACCT_HOT_PATCHING));
}

/* On a successful read, this should return a valid pointer to a vulnerability
 * table and modify the size argument passed to it.  If it fails, it should
 * dump a log event, return NULL & not modify the size.
 *
 * The caller should release the old table & make the return value the new
 * table; the reason for doing this table swap outside this function is to
 * allow (future work) identification of vulnerabilities that have actually
 * changed; from this set of changed vulnerabilities, identify those that have
 * been injected and flush only those (an optimization issue).
 *
 * Policy file format: (indentations don't appear in the actual file, they exist
 * here to illustrate the format & to show where multiple data can occur; also
 * format is close to binary as it is now)
 * All integers/hex_numbers are 32-bits unless explicitly stated otherwise.

<engine_version-str>
<num_vulnerabilities-decimal_integer>
  <vulnerability_id-str>
  <policy_id-str>
  <version-decimal_integer>
  <hotpatch_dll-str>
  <hotpatch_dll_hash-str>
  <num_sets-decimal_integer>
    <num_modules-decimal_integer>
    <pe_name-str>
    <pe_timestamp-hex_number>
    <pe_checksum-hex_number>
    <pe_image_size-hex_number>
    <pe_code_size-hex_number>
    <pe_file_version-hex_number-64_bits>
    <num_hashes-decimal_integer>
      <start-hex_number>
      <length-hex_number>
      <hash-decimal_integer>
    <num_patch_points-decimal_integer>
      <offset-hex_number>
      <precedence-decimal_integer>
      <detector_offset-hex_number>
      <protector_offset-hex_number>
      <return_addr-hex_number>

 * TODO: all unused fields, i.e., runtime fields in the data structures should
 *       be set to NULL/0 to avoid any assumption violations down stream.
 * TODO: after reading in the vulnerability data, that region should be write
 *       protected
 */
static hotp_vul_t *
hotp_read_policy_defs(uint *num_vuls_read)
{
    hotp_vul_t *tab = NULL;
    uint hotp_interface_version;
    uint vul = 0, set, module, hash, ppoint;
    uint num_vuls = 0, num_vuls_alloc = 0;
    char *buf = NULL; /* TODO: for now only; will go after file mapping */
    size_t buf_len = 0;
    char *start = NULL;
    DEBUG_DECLARE(bool started_parsing = false;)

    /* Read the config file only if -liveshields is turned on.  If it isn't
     * turned on, read gbop hooks if -gbop is specified.
     */
    if (!DYNAMO_OPTION(liveshields)) {
#    ifdef GBOP
        if (DYNAMO_OPTION(gbop))
            goto read_gbop_only;
#    endif
        return NULL;
    }

    buf = hotp_read_data_file(POLICY_FILE, &buf_len);
    if (buf == NULL) {
        ASSERT(buf_len == 0);
        goto error_reading_policy;
    } else {
        ASSERT(buf_len > 0);
        ASSERT_CURIOSITY(buf_len < MAX_POLICY_FILE_SIZE);
    }

    start = buf;
    DEBUG_DECLARE(started_parsing = true;)
    SET_NUM(hotp_interface_version, uint, HOTP_INTERFACE_VERSION, start);
    SET_NUM(num_vuls, uint, NUM_VULNERABILITIES, start);
#    ifdef GBOP
    if (DYNAMO_OPTION(gbop))
        num_vuls_alloc = gbop_get_num_hooks();
#    endif
    num_vuls_alloc += num_vuls;
    ASSERT(num_vuls_alloc > 0 && num_vuls_alloc <= MAX_NUM_VULNERABILITIES);

    /* Zero out all dynamically allocated hotpatch table structures to avoid
     * leaks when there is a parse error.  See case 8272, 9045.
     */
    tab = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_vul_t, num_vuls_alloc,
                                  ACCT_HOT_PATCHING, PROTECTED, 0);

    for (vul = 0; vul < num_vuls; vul++) {
        /* FIXME: bounds checking; length should be > 2 && < 32; not null */
        SET_STR(VUL(tab, vul).vul_id, start);
        SET_STR(VUL(tab, vul).policy_id, start);
        SET_NUM(VUL(tab, vul).policy_version, uint, POLICY_VERSION, start);

        /* FIXME: strdup strings because the buffer/mapped file will be deleted
         *        after processing; don't use strdup though!
         *        works right now till the next time I read in a policy file
         *        into buf[]; if that read fails the old data will be corrupt!
         *        remember, if not strdup'ed, all strings are in writable memory
         */
        SET_STR(VUL(tab, vul).hotp_dll, start);
        SET_STR(VUL(tab, vul).hotp_dll_hash, start);
        SET_NUM(VUL(tab, vul).num_sets, uint, NUM_SETS, start);

        /* Initialize all runtime values in the structure. */
        VUL(tab, vul).mode = HOTP_MODE_OFF; /* Fix for case 5326. */
        VUL(tab, vul).type = HOTP_TYPE_HOT_PATCH;

        VUL(tab, vul).sets =
            HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_set_t, VUL(tab, vul).num_sets,
                                    ACCT_HOT_PATCHING, PROTECTED, 0);
        VUL(tab, vul).info = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_vul_info_t, 1,
                                                     ACCT_HOT_PATCHING, PROTECTED, 0);

        for (set = 0; set < VUL(tab, vul).num_sets; set++) {
            SET_NUM(SET(tab, vul, set).num_modules, uint, NUM_MODULES, start);
            SET(tab, vul, set).modules = HEAP_ARRAY_ALLOC_MEMSET(
                GLOBAL_DCONTEXT, hotp_module_t, SET(tab, vul, set).num_modules,
                ACCT_HOT_PATCHING, PROTECTED, 0);
            for (module = 0; module < SET(tab, vul, set).num_modules; module++) {
                SET_STR(SIG(tab, vul, set, module).pe_name, start);
                SET_NUM(SIG(tab, vul, set, module).pe_timestamp, uint, PE_TIMESTAMP,
                        start);
                SET_NUM(SIG(tab, vul, set, module).pe_checksum, uint, PE_CHECKSUM, start);
                SET_NUM(SIG(tab, vul, set, module).pe_image_size, uint, PE_IMAGE_SIZE,
                        start);
                SET_NUM(SIG(tab, vul, set, module).pe_code_size, uint, PE_CODE_SIZE,
                        start);
                SET_NUM(SIG(tab, vul, set, module).pe_file_version, uint64,
                        PE_FILE_VERSION, start);

                /* Initialize all runtime values in the structure. */
                MODULE(tab, vul, set, module).matched = false;
                MODULE(tab, vul, set, module).base_address = NULL;

                SET_NUM(MODULE(tab, vul, set, module).num_patch_point_hashes, uint,
                        NUM_PATCH_POINT_HASHES, start);
                MODULE(tab, vul, set, module).hashes = HEAP_ARRAY_ALLOC_MEMSET(
                    GLOBAL_DCONTEXT, hotp_patch_point_hash_t,
                    MODULE(tab, vul, set, module).num_patch_point_hashes,
                    ACCT_HOT_PATCHING, PROTECTED, 0);

                for (hash = 0;
                     hash < MODULE(tab, vul, set, module).num_patch_point_hashes;
                     hash++) {
                    SET_NUM(PPOINT_HASH(tab, vul, set, module, hash).start, app_rva_t,
                            HASH_START_OFFSET, start);
                    SET_NUM(PPOINT_HASH(tab, vul, set, module, hash).len, uint,
                            HASH_LENGTH, start);
                    SET_NUM(PPOINT_HASH(tab, vul, set, module, hash).hash_value, uint,
                            HASH_VALUE, start);
                }

                SET_NUM(MODULE(tab, vul, set, module).num_patch_points, uint,
                        NUM_PATCH_POINTS, start);
                MODULE(tab, vul, set, module).patch_points = HEAP_ARRAY_ALLOC_MEMSET(
                    GLOBAL_DCONTEXT, hotp_patch_point_t,
                    MODULE(tab, vul, set, module).num_patch_points, ACCT_HOT_PATCHING,
                    PROTECTED, 0);

                for (ppoint = 0; ppoint < MODULE(tab, vul, set, module).num_patch_points;
                     ppoint++) {
                    SET_NUM(PPOINT(tab, vul, set, module, ppoint).offset, app_rva_t,
                            PATCH_OFFSET, start);
                    SET_NUM(PPOINT(tab, vul, set, module, ppoint).precedence, uint,
                            PATCH_PRECEDENCE, start);
                    SET_NUM(PPOINT(tab, vul, set, module, ppoint).detector_fn, app_rva_t,
                            DETECTOR_OFFSET, start);

                    /* Both protector and return address can be NULL */
                    SET_NUM(PPOINT(tab, vul, set, module, ppoint).protector_fn, app_rva_t,
                            PROTECTOR_OFFSET, start);
                    SET_NUM(PPOINT(tab, vul, set, module, ppoint).return_addr, app_rva_t,
                            RETURN_ADDR, start);
                    PPOINT(tab, vul, set, module, ppoint).trampoline = NULL;
                    PPOINT(tab, vul, set, module, ppoint).app_code_copy = NULL;
                    PPOINT(tab, vul, set, module, ppoint).tramp_exit_tgt = NULL;
                }
            }
        }
    }

#    ifdef GBOP
    if (DYNAMO_OPTION(gbop)) {
        hotp_only_read_gbop_policy_defs(tab, &num_vuls /* IN OUT arg */);
        ASSERT(num_vuls_alloc == num_vuls);
    }
#    endif
    *num_vuls_read = num_vuls;
    LOG(GLOBAL, LOG_HOT_PATCHING, 1, "read %d vulnerability definitions\n", num_vuls);
    heap_free(GLOBAL_DCONTEXT, buf, buf_len HEAPACCT(ACCT_HOT_PATCHING));
    return tab;

error_reading_policy:
    /* TODO: log error, free stuff, set tab to null, leave size intact & exit
     *       for now just assert to make sure that bugs don't escape.
     */
    /* TODO: provide line #, not offset; alex couldn't use the offset */
    SYSLOG_INTERNAL_WARNING("Error reading or parsing hot patch definitions");
    /* Need this curiosity to make qa notice; the warning is handy for
     * development testing only.  No hot patching on Linux, so don't assert.
     * FIXME: Convert to assert after case 9066 has been fixed & tested.
     * Note: Warn for missing file, but assert for parsing error; latter is
     * bug, former may just be a hotpatch-less installation - mostly coredev.
     */
    IF_WINDOWS(
        ASSERT_CURIOSITY(!started_parsing && "Error parsing hot patch definitions");)
    *num_vuls_read = 0;
    if (tab != NULL) {
        ASSERT(num_vuls_alloc > 0);
        /* If gbop is on, then during a parse error num_vuls (parsed) must be
         * less than num_vuls_alloc because if table as been allocated space
         * has been allocated for gbop entries as well which wouldn't have been
         * read on a parse error.  It is read after this point; see below.
         */
        IF_GBOP(ASSERT(!DYNAMO_OPTION(gbop) || num_vuls < num_vuls_alloc);)
        /* On error free the whole table, not just what was read; case 9044. */
        hotp_free_vul_table(tab, num_vuls_alloc);
        tab = NULL;
    }

    /* buf can be allocated even if vulnerability table hasn't been allocated.
     * See case 8332.
     */
    if (buf != NULL) {
        ASSERT(buf_len > 0);
        heap_free(GLOBAL_DCONTEXT, buf, buf_len HEAPACCT(ACCT_HOT_PATCHING));
        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
            "error reading vulnerability file at offset " SZFMT "\n",
            (ptr_uint_t)(start - buf));
    }

#    ifdef GBOP
    /* Even if we couldn't read the hot patch policies, we should still
     * allocate a new table and read in the gbop hooks.
     */
read_gbop_only:
    if (DYNAMO_OPTION(gbop)) {
        num_vuls_alloc = gbop_get_num_hooks();
        ASSERT(num_vuls_alloc > 0 && num_vuls_alloc <= MAX_NUM_VULNERABILITIES);
        num_vuls = 0;

        tab = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_vul_t, num_vuls_alloc,
                               ACCT_HOT_PATCHING, PROTECTED);
        hotp_only_read_gbop_policy_defs(tab, &num_vuls /* IN OUT arg */);

        ASSERT(num_vuls_alloc == num_vuls);
        *num_vuls_read = num_vuls;
    }
#    endif

    return tab;
}

/* TODO: An efficiency issue: don't load all hot patch dlls unless the mode
 *       for at least one corresponding policy is detect or protect; this will
 *       avoid loading all hot patch dlls whether they are used or not.  Note:
 *       this is still eager loading as per the design.
 */
static void
hotp_load_hotp_dlls(hotp_vul_t *vul_tab, uint num_vuls)
{
    uint vul;
    int retval;
    /* TODO: these arrays are large so make them static with a lock to avoid
     *        a potential runtime stack overflow.
     */
    char hotp_dll_path[MAXIMUM_PATH];
    char hotp_dll_cache[MAXIMUM_PATH];

    /* Only liveshields need to know DYNAMORIO_HOME, probes give full paths. */
    if (DYNAMO_OPTION(liveshields)) {
        /* If null or non-existent hotp_dll_cache directory raise error log,
         * disable all associated vuls?   We are going to assert/log if we can't
         * find the dll (below) anyway.
         */
        retval = d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_HOME), hotp_dll_cache,
                                   BUFFER_SIZE_ELEMENTS(hotp_dll_cache));
        if (IS_GET_PARAMETER_FAILURE(retval)) {
            SYSLOG_INTERNAL_WARNING("Can't read %s.  Hot patch dll loading "
                                    "failed; hot patching won't work.",
                                    DYNAMORIO_VAR_HOME);
            return;
        }
    } else {
        ASSERT(DYNAMO_OPTION(probe_api));
    }

    /* Compute dll cache path, i.e., $DYNAMORIO_HOME/lib/hotp/<engine>/ */
    NULL_TERMINATE_BUFFER(hotp_dll_cache);
    CAT_AND_TERMINATE(hotp_dll_cache, HOT_PATCHING_DLL_CACHE_PATH);
    CAT_AND_TERMINATE(hotp_dll_cache, STRINGIFY(HOTP_INTERFACE_VERSION) "\\");

    for (vul = 0; vul < num_vuls; vul++) {
        /* Hot patch dlls exist only for the type hot_patch and probe, not for
         * gbop hooks; well, not at least for now.
         */
        if (!TESTANY(HOTP_TYPE_HOT_PATCH | HOTP_TYPE_PROBE, VUL(vul_tab, vul).type)) {
            ASSERT(TESTALL(HOTP_TYPE_GBOP_HOOK, VUL(vul_tab, vul).type));
            /* TODO: also assert that the base is dynamorio.dll & remediator
             * offsets are what they should be - use a DODEBUG
             */
            continue;
        }

        if (VUL(vul_tab, vul).hotp_dll_base == NULL) { /* Not loaded yet. */
            ASSERT(
                TESTANY(HOTP_TYPE_HOT_PATCH | HOTP_TYPE_PROBE, VUL(vul_tab, vul).type));
            ASSERT(VUL(vul_tab, vul).hotp_dll != NULL);

            /* Liveshields give just the base name which is used to compute
             * full path, i.e., DYNAMORIO_HOME/lib/hotp/hotp_dll. */
            if (TEST(HOTP_TYPE_HOT_PATCH, VUL(vul_tab, vul).type)) {
                strncpy(hotp_dll_path, hotp_dll_cache,
                        BUFFER_SIZE_ELEMENTS(hotp_dll_path) - 1);
                NULL_TERMINATE_BUFFER(hotp_dll_path);

                /* Hot patch dll names should just be base names; with no / or \. */
                ASSERT(strchr(VUL(vul_tab, vul).hotp_dll, '\\') == NULL &&
                       strchr(VUL(vul_tab, vul).hotp_dll, '/') == NULL);
                strncat(hotp_dll_path, VUL(vul_tab, vul).hotp_dll,
                        BUFFER_SIZE_ELEMENTS(hotp_dll_path) - strlen(hotp_dll_path) - 1);
            } else {
                /* Probe api calls provide full path to hotp dlls. */
                strncpy(hotp_dll_path, VUL(vul_tab, vul).hotp_dll,
                        BUFFER_SIZE_ELEMENTS(hotp_dll_path) - 1);
            }
            NULL_TERMINATE_BUFFER(hotp_dll_path);
            ASSERT(strlen(hotp_dll_path) < BUFFER_SIZE_ELEMENTS(hotp_dll_path));

            /* TODO: check if file exists; if not log, turn off associated
             *       vulnerabilities & bail out; need to think through the
             *       error exit mechanism while reading polcy-{defs,modes}.
             */

            /* FIXME: currently our loadlibrary hits our own syscall_while_native
             * hook and goes to d_r_dispatch, which expects protected data sections.
             * Once we have our own loader we can remove this.
             */
            VUL(vul_tab, vul).hotp_dll_base =
                load_shared_library(hotp_dll_path, false /*!reachable*/);

            /* TODO: if module base is null, raise a log event, mark vul as not
             *       usable (probably a new status) and move on; for now just
             *       assert.
             */
            /* TODO: assert that all detector_fn & protector_fn offsets
             *       associated with this hotp_dll actually lie within its
             *       text space.
             */
            if (VUL(vul_tab, vul).hotp_dll_base == NULL) {
                LOG(GLOBAL, LOG_HOT_PATCHING, 1, "unable to load hotp_dll: %s\n",
                    hotp_dll_path);
                ASSERT(VUL(vul_tab, vul).hotp_dll_base != NULL);
            }
            LOG(GLOBAL, LOG_HOT_PATCHING, 1, "loaded hotp_dll: %s at " PFX "\n",
                hotp_dll_path, VUL(vul_tab, vul).hotp_dll_base);

            /* TODO: this one must be done asap; add the hot patch dll's text
             *       either to a new vm_area_vector_t or executable_vm_areas;
             *       check with the team first. case 5381
             *  add_executable_vm_area(hotp_dll_text_base, size_of_text,
             *                         VM_UNMOD_IMAGE, false
                                       _IF_DEBUG("hot patch dll loading"));
             */
            /* TODO: assert that hotp_dll's dllmain is null to prevent control
             *       flow from going there during the thread creation due to
             *       nudge; but how?
             */
        }
    }
}

/* TODO: need a lot more LOG, ASSERT and SYSLOG statements */
/*----------------------------------------------------------------------------*/

/* TODO: for now just read from a flat file; change it in next phase to
 *       file/shmem depending upon what we decide; same goes for binary vs.
 *       text format;  either way, the format of communication has to be defined
 *       so that nodemanager & core know what to write & read - key items
 *       include number of mode changes transmitted & the structure of each.
 *
 * Mode file format:
 * <num_mode_update_entries>
 * <policy_id-str>:<mode-decimal_integer>
 * ...
 * mode 0 - off, 1 - detect, 2 - protect;
 *
 * TODO: eventually, modes will be strings (good idea?, not binary; might be
 *       better to leave it as it is today.
 */
static void
hotp_read_policy_modes(hotp_policy_mode_t **old_modes)
{
    /* TODO: for the next phase decide whether to use registry key or option
     *        string; for use a registry key.
     */
    uint mode = 0, vul, num_mode_update_entries, i;
    char *buf = NULL;
    size_t buf_len = 0;
    char *start = NULL;

    /* Old modes are needed only by regular hotp for flushing patches;
     * hotp_only shouldn't use them.
     */
    ASSERT(!DYNAMO_OPTION(hotp_only) || old_modes == NULL);
    if (old_modes != NULL) /* init to NULL because there are error exits */
        *old_modes = NULL;

    /* Can be called only during hotp_init() or during a nudge. */
    ASSERT_OWN_WRITE_LOCK(true, &hotp_vul_table_lock);

    /* This function shouldn't be called before policies are read.
     * Sometimes, the node manager can nudge for a mode read without specifying
     * policies first!  This may happen during startup.  Case 5448.
     */
    if (GLOBAL_VUL_TABLE == NULL) {
        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
            "Modes can't be set without "
            "policy definitions.  Probably caused due to a nudge by the node "
            "manager to read modes when there were no policies.");
        return;
    }

    buf = hotp_read_data_file(MODES_FILE, &buf_len);
    if (buf == NULL) {
        ASSERT(buf_len == 0);
        return;
    }
    ASSERT(buf_len > 0);

    /* Allocate space to save the old modes if they were requested for. */
    if (old_modes != NULL) {
        *old_modes = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_policy_mode_t,
                                      NUM_GLOBAL_VULS, ACCT_HOT_PATCHING, PROTECTED);
        ASSERT(*old_modes != NULL); /* make sure that space was allocated */
    }

    /* Turn off all vulnerabilities before reading modes.  Only those for which
     * a mode is specified should be on.  Fix for case 5565.  As the write lock
     * is held, there is no danger of any lookup providing a no-match when there
     * is one.
     */
    for (vul = 0; vul < NUM_GLOBAL_VULS; vul++) {
        if (old_modes != NULL)
            (*old_modes)[vul] = GLOBAL_VUL(vul).mode;

        /* Only hot patch types can be turned off by mode files.  Other types
         * like gbop hooks can't be.
         */
        if (TESTALL(HOTP_TYPE_HOT_PATCH, GLOBAL_VUL(vul).type))
            GLOBAL_VUL(vul).mode = HOTP_MODE_OFF;
    }

    start = buf;
    SET_NUM(num_mode_update_entries, uint, NUM_VULNERABILITIES, start);

    /* TODO: what if num_mode_update_entries is more than the entries in the
     * file?
     */
    for (i = 0; i < num_mode_update_entries; i++) {
        bool matched = false;
        char *split, *policy_id;

        SET_STR_PTR(policy_id, start);
        split = strchr(policy_id, ':');
        if (split == NULL)
            goto error_reading_policy;
        *split++ = '\0'; /* TODO: during file mapping, this won't work */

        SET_NUM(mode, uint, MODE, split);

        /* Must set mode for all vulnerabilities with a matching policy_id, not
         * just the first one.
         */
        for (vul = 0; vul < NUM_GLOBAL_VULS; vul++) {
            if (strncmp(GLOBAL_VUL(vul).policy_id, policy_id, MAX_POLICY_ID_LENGTH) ==
                0) {
                GLOBAL_VUL(vul).mode = mode;
                matched = true;
            }
        }

        /* If during mode update policy_id from a mode file doesn't have
         * a corresponding vul_t, log a warning.  When the node manager is
         * starting up, modes file can be inconsistent, so this may happen
         * (cases 5500 & 5526).  However this could be a bug somewhere in the
         * pipe line (EV, nm, policy package, etc) too.
         */
        if (!matched) {
            SYSLOG_INTERNAL_WARNING("While reading modes, found a mode "
                                    "definition for a policy (%s) that didn't exist",
                                    policy_id);
        }
    }

    /* TODO: make the macro take this as an argument or find a neutral name */
error_reading_policy:
    ASSERT(buf != NULL);
    heap_free(GLOBAL_DCONTEXT, buf, buf_len HEAPACCT(ACCT_HOT_PATCHING));
    return;
}

static void
hotp_set_policy_status(const uint vul_index, const hotp_inject_status_t status)
{
    uint crc_buf_size;

    ASSERT_OWN_WRITE_LOCK(true, &hotp_vul_table_lock);

    ASSERT(hotp_policy_status_table != NULL);
    ASSERT(status == HOTP_INJECT_NO_MATCH || status == HOTP_INJECT_PENDING ||
           status == HOTP_INJECT_IN_PROGRESS || status == HOTP_INJECT_DETECT ||
           status == HOTP_INJECT_PROTECT || status == HOTP_INJECT_ERROR);

    /* Given that no other thread, app or nudge, will change this without the
     * hot patch lock, this can be done without an atomic write.
     */
    ASSERT(GLOBAL_VUL(vul_index).info->inject_status != NULL);
    *(GLOBAL_VUL(vul_index).info->inject_status) = status;

    /* Compute CRC after this status update and put it in the
     * policy status table so that the node manager is protected from
     * reading invalid status due to policy status table being reset/
     * relllocated due to hotp_init or nudge or detach taking place.
     *
     * Note: The CRC write to the table doesn't need to be atomic too.  Also,
     *       the CRC value is for all bytes of the policy status table except
     *       the CRC itself.  Otherwise we would have to do the CRC computation
     *       twice; wastefully expensive.
     */
    crc_buf_size = hotp_policy_status_table->size - sizeof(hotp_policy_status_table->crc);
    hotp_policy_status_table->crc =
        d_r_crc32((char *)&hotp_policy_status_table->size, crc_buf_size);
}

/* The status of hot patches is directly read by the node manager from the
 * memory address specified in the drmarker; no nudge is needed.  While the
 * table is being created, the drmarker pointer will be null and set only
 * after the table is fully initialized.  Also, updates to the table entries
 * are made with the hot patch lock, as with creation.  The only way the node
 * manager can get invalid data is after it reads drmarker, this routine
 * releases the old policy status table before the node manager can read it.
 * That is guarded by the table CRC, which is likely to be wrong.  If drmarker
 * points to memory released to the os or NULL, node manager will get a memory
 * read error and it should be able to reattempt within which the new table will
 * be ready.
 *
 * Format of policy status table in memory:
 * <CRC32-uint> - CRC of size_in_bytes - sizeof(CRC32, i.e, uint).
 * <size_in_bytes-uint>
 * <num_policy_entries-uint>
 * <hotp_policy_status_t>*
 */
static void
hotp_init_policy_status_table(void)
{
    uint i, num_policies = NUM_GLOBAL_VULS, size_in_bytes, crc_buf_size;
    hotp_policy_status_table_t *temp;

    /* Can be called only during hotp_init() or during a nudge. */
    ASSERT_OWN_WRITE_LOCK(true, &hotp_vul_table_lock);
    ASSERT(!DATASEC_PROTECTED(DATASEC_RARELY_PROT));

    /* This function shouldn't be called before policies and/or modes are read.
     * Sometimes, the node manager can nudge for a mode read without specifying
     * policies first!  This may happen during startup.  Case 5448.
     */
    if (GLOBAL_VUL_TABLE == NULL) {
        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
            "Policy status table can't be created "
            "without policy definitions.  Probably caused due to a nudge by the "
            "node manager to read modes when there were no policies.  Or "
            "because all probes registered using the probe api were invalid.");
        return;
    }

    /* This function is called each time a new policies and/or modes are read
     * in.  Each such time all existing injected hot patches are removed, so
     * the policy status table associated with the old global vulnerability
     * table must be released or resized to fit only the new set of
     * hot patches turned on.  The former is simpler to do.
     *
     * Note: if the optimization of flushing only those policies that
     * have changed is implemented, which is not the case today, then just
     * releasing policy status table will result in incorrect inject status.
     * It should be released after the new table is created and filled with
     * old values.
     */
    if (hotp_policy_status_table != NULL) {
        temp = hotp_policy_status_table;
        hotp_policy_status_table = NULL;

        /* If dr_marker_t isn't initialized, this won't be set.  In that case,
         * the dr_marker_t initialization code will set up the policy status table.
         * This can happen at init time because hotp_init() is called before
         * callback_interception_init().
         */
        set_drmarker_hotp_policy_status_table(NULL);

        heap_free(GLOBAL_DCONTEXT, temp, temp->size HEAPACCT(ACCT_HOT_PATCHING));
    }

    /* Right now, the status table contains as many elements as
     * vulnerabilities.  The original idea was to have only policies which
     * are turned on in the table.  This caused failures in the core
     * because we need to maintain status internally for vulnerabilities
     * that are turned off too.  Case 5326.
     */
    size_in_bytes =
        sizeof(hotp_policy_status_table_t) + sizeof(hotp_policy_status_t) * num_policies;
    temp = heap_alloc(GLOBAL_DCONTEXT, size_in_bytes HEAPACCT(ACCT_HOT_PATCHING));
    temp->size = size_in_bytes;
    temp->policy_status_array =
        (hotp_policy_status_t *)((char *)temp + sizeof(hotp_policy_status_table_t));

    /* Init status buffer elements & set up global vul table pointers */
    /* TODO: two vulnerabilities can belong to the same policy; need to check
     *       for that and avoid duplication in the table;  not needed now
     *       because we don't have such policies yet.
     */
    for (i = 0; i < NUM_GLOBAL_VULS; i++) {
        strncpy(temp->policy_status_array[i].policy_id, GLOBAL_VUL(i).policy_id,
                MAX_POLICY_ID_LENGTH);
        NULL_TERMINATE_BUFFER(temp->policy_status_array[i].policy_id);
        temp->policy_status_array[i].inject_status = HOTP_INJECT_NO_MATCH;

        /* Fix for case 5484, where the node manager wasn't able to tell if an
         * inject status was for a policy that was turned on or off.
         */
        temp->policy_status_array[i].mode = GLOBAL_VUL(i).mode;

        /* The inject status in global vulnerability table should point
         * to the corresponding element in this table.
         */
        GLOBAL_VUL(i).info->inject_status = &temp->policy_status_array[i].inject_status;
    }
    temp->num_policies = i;

    /* Set the table CRC now that the table has been initialized. */
    crc_buf_size = temp->size - sizeof(temp->crc);
    temp->crc = d_r_crc32((char *)&temp->size, crc_buf_size);

    /* Make the policy status table live.  If the dr_marker_t isn't initialized
     * this won't be set.  In that case, the dr_marker_t initialization code will
     * set up the policy status table; happens during startup/initialization.
     */
    hotp_policy_status_table = temp;

    set_drmarker_hotp_policy_status_table((void *)temp);
}

/* Frees all the dynamically allocated members of vul (strings, info, sets,
 * modules and patch points).  NOTE: It doesn't free the vul itself.
 */
static void
hotp_free_one_vul(hotp_vul_t *vul)
{
    uint set_idx, module_idx, ppoint_idx;

    /* If this routine is called with a NULL for argument then there is a bug
     * somewhere.*/
    ASSERT(vul != NULL);
    if (vul == NULL)
        return;

    if (vul->vul_id != NULL)
        dr_strfree(vul->vul_id HEAPACCT(ACCT_HOT_PATCHING));
    if (vul->policy_id != NULL)
        dr_strfree(vul->policy_id HEAPACCT(ACCT_HOT_PATCHING));
    if (vul->hotp_dll != NULL)
        dr_strfree(vul->hotp_dll HEAPACCT(ACCT_HOT_PATCHING));
    if (vul->hotp_dll_hash != NULL)
        dr_strfree(vul->hotp_dll_hash HEAPACCT(ACCT_HOT_PATCHING));
    if (vul->info != NULL) {
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, vul->info, hotp_vul_info_t, ACCT_HOT_PATCHING,
                       PROTECTED);
    }

    if (vul->sets == NULL)
        return;

    /* If a set's array isn't null, then the number of sets can't be zero. */
    ASSERT(vul->num_sets > 0);
    for (set_idx = 0; set_idx < vul->num_sets; set_idx++) {
        hotp_set_t *set = &vul->sets[set_idx];

        if (set->modules == NULL)
            continue;

        /* If a modules array isn't null, then the number of modules can't
         * be zero.
         */
        ASSERT(set->num_modules > 0);
        for (module_idx = 0; module_idx < set->num_modules; module_idx++) {
            hotp_module_t *module = &set->modules[module_idx];
            if (module->sig.pe_name != NULL) {
                dr_strfree(module->sig.pe_name HEAPACCT(ACCT_HOT_PATCHING));
            }

            if (module->hashes != NULL) {
                ASSERT(module->num_patch_point_hashes > 0);
                HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, module->hashes, hotp_patch_point_hash_t,
                                module->num_patch_point_hashes, ACCT_HOT_PATCHING,
                                PROTECTED);
            }

            if (module->patch_points != NULL) {
                ASSERT(module->num_patch_points > 0);
                for (ppoint_idx = 0; ppoint_idx < module->num_patch_points;
                     ppoint_idx++) {
                    hotp_patch_point_t *ppoint = &module->patch_points[ppoint_idx];
                    if (ppoint->trampoline != NULL) {
                        ASSERT(DYNAMO_OPTION(hotp_only));
                        ASSERT(ppoint->app_code_copy != NULL);
                        special_heap_free(hotp_only_tramp_heap,
                                          (void *)ppoint->trampoline);
                    }
                }
                HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, module->patch_points, hotp_patch_point_t,
                                module->num_patch_points, ACCT_HOT_PATCHING, PROTECTED);
            }
        }
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, set->modules, hotp_module_t, set->num_modules,
                        ACCT_HOT_PATCHING, PROTECTED);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, vul->sets, hotp_set_t, vul->num_sets,
                    ACCT_HOT_PATCHING, PROTECTED);
}

/* Release all memory used by the hot patch vulnerability table, tab.
 * num_vuls_alloc is number of vulnerability defs. the table has space for.
 * The table may not always contain num_vuls_alloc policy defs.  Where there
 * is an error during policy defs file parsing they can be fewer in number with
 * the last one (one where the error happened) being partial.  Cases 8272, 9045.
 */
static void
hotp_free_vul_table(hotp_vul_t *tab, uint num_vuls_alloc)
{
    uint vul_idx;

    if (tab == NULL) {
        ASSERT(num_vuls_alloc == 0);
        return;
    }

    /* If the table isn't NULL, the number of vulnerabilities can't be zero. */
    ASSERT(num_vuls_alloc > 0);

    for (vul_idx = 0; vul_idx < num_vuls_alloc; vul_idx++) {
        hotp_free_one_vul(&tab[vul_idx]);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, tab, hotp_vul_t, num_vuls_alloc, ACCT_HOT_PATCHING,
                    PROTECTED);
}

/* This routine flushes all fragments in fcache that have been injected with a
 * hot patch, i.e., restoring an app text to its pre-hot-patch state.
 *
 * Note: hot patch removal is not optimized, i.e., changes to existing
 * policy definitions, modes or actual injection status aren't used to limit
 * flushing.  Not a performance issue for now.
 * TODO: flush only those vulnerabilities that have actually changed, not
 *       every thing that is active or has been injected.
 * TODO: make this use loaded_module_areas & get rid off the 4-level nested
 *          loops.
 */
static void
hotp_remove_patches_from_module(const hotp_vul_t *vul_tab, const uint num_vuls,
                                const bool hotp_only, const app_pc mod_base,
                                const hotp_policy_mode_t *old_modes)
{
    uint vul_idx, set_idx, module_idx, ppoint_idx;
    hotp_module_t *module;
    hotp_patch_point_t *ppoint;
    dcontext_t *dcontext = get_thread_private_dcontext();

    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    /* For hotp_only patch removal, we should be running in hotp_only mode. */
    ASSERT(!hotp_only || DYNAMO_OPTION(hotp_only));
    /* Old vulnerability modes shouldn't be used with hotp_only. */
    ASSERT(!DYNAMO_OPTION(hotp_only) || old_modes == NULL);
    /* Alternate modes shouldn't be used during module specific removal also. */
    ASSERT(mod_base == NULL || old_modes == NULL);

    /* Though trying to flush a NULL vul table is a bug, this can happen
     * because the node manager can nudge the core to read modes when it hasn't
     * provided the policies!  See case 5448.  Hence just a warning & no assert.
     */
    if (vul_tab == NULL) {
        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
            "Hot patch flushing has been invoked "
            "with a NULL table");
        return;
    }

    LOG(GLOBAL, LOG_HOT_PATCHING, 1, "flushing as a result of nudge\n");
    for (vul_idx = 0; vul_idx < num_vuls; vul_idx++) {
        bool set_processed = false;
        const hotp_vul_t *vul = &VUL(vul_tab, vul_idx);

        /* Nothing to remove or flush if the mode is off, i.e., nothing would
         * have been injected.
         * Note: Both vul's current mode & its old mode should be off to skip
         * removal; even if one is not, then that vulnerability's patches need
         * to be removed.  In otherwords, if patch was previously on (injected)
         * or is now on (to be injected), corresponding bbs must be flushed;
         * this is for regular hotp, not for hotp_only which has no flushing.
         */
        if (vul->mode == HOTP_MODE_OFF) {
            if (old_modes == NULL) {
                /* If there is no old_mode, skip right here. */
                continue;
            } else if (old_modes[vul_idx] == HOTP_MODE_OFF) {
                /* If old_mode exists, that must be off too in order to skip. */
                continue;
            }
        }
        ASSERT(vul->mode == HOTP_MODE_DETECT || vul->mode == HOTP_MODE_PROTECT ||
               (old_modes != NULL &&
                (old_modes[vul_idx] == HOTP_MODE_DETECT ||
                 old_modes[vul_idx] == HOTP_MODE_PROTECT)));

        for (set_idx = 0; set_idx < VUL(vul_tab, vul_idx).num_sets; set_idx++) {
            /* Only the first matching set should be used; case 10248. */
            if (set_processed)
                break;

            for (module_idx = 0; module_idx < SET(vul_tab, vul_idx, set_idx).num_modules;
                 module_idx++) {
                module = &MODULE(vul_tab, vul_idx, set_idx, module_idx);
                if (module->matched) {
                    /* If a specific module is mentioned remove patches from
                     * just that.
                     */
                    if (mod_base != NULL && mod_base != module->base_address)
                        continue;

                    set_processed = true;
                    /* Otherwise, flush all patch points in any module that
                     * matches.  Nothing to flush in unmatched modules.
                     */
                    for (ppoint_idx = 0; ppoint_idx < module->num_patch_points;
                         ppoint_idx++) {
                        ppoint = &module->patch_points[ppoint_idx];
                        if (hotp_only) {
                            /* For a hotp_only patch, we can only remove that
                             * which has been injected, unlike the hotp mode
                             * where we might just be flushing out uninjected
                             * fragments or don't know which particular patch
                             * point has been injected (in hotp_only mode all
                             * of them should be injected if one is injected).
                             */
                            if (ppoint->trampoline != NULL)
                                hotp_only_remove_patch(dcontext, module, ppoint);
                            else {
                                /* If module is matched and mode is on, then
                                 * hotp_only patch targeting the current
                                 * ppoint must be injected unless it has
                                 * been removed to handle loader-safety issues.
                                 */
                                ASSERT((ppoint->trampoline == NULL ||
                                        hotp_globals->ldr_safe_hook_removal) &&
                                       "hotp_only - double patch removal");
                            }
                        } else {
                            app_pc flush_addr = hotp_ppoint_addr(module, ppoint);

                            ASSERT_OWN_NO_LOCKS();
                            LOG(GLOBAL, LOG_HOT_PATCHING, 4,
                                "flushing " PFX " due to a nudge\n", flush_addr);
                            flush_fragments_in_region_start(
                                dcontext, flush_addr, 1, false /* no lock */,
                                false /* keep futures */, false /*exec still valid*/,
                                false /*don't force synchall*/
                                _IF_DGCDIAG(NULL));
                            flush_fragments_in_region_finish(dcontext, false);
                            /* TODO: ASSERT (flushed fragments have really been)
                             *       flushed but how, using a vm_areas_overlap()
                             *       or fragment_lookup() check?
                             */
                        }
                    }
                }
            }
        }
    }
}

/* TODO: make this use hotp_patch_point_areas & get rid off the 4-level nested
 *          loops which is used in hotp_remove_patches_from_module.
 */
static void
hotp_remove_hot_patches(const hotp_vul_t *vul_tab, const uint num_vuls,
                        const bool hotp_only, const hotp_policy_mode_t *old_modes)
{
    /* Old vulnerability modes shouldn't be used with hotp_only. */
    ASSERT(!DYNAMO_OPTION(hotp_only) || old_modes == NULL);
    hotp_remove_patches_from_module(vul_tab, num_vuls, hotp_only, NULL, old_modes);
}

/* TODO: vlad wanted the ability to ignore some attributes during checking;
 *       this is not for constraints, but if he wants an ad-hoc patch to fix
 *       something other than a vulnerability, say, broken code that is not
 *       a vulnerability;  for hot patches/constraints all attributes must be
 *       checked, no ignoring stuff.
 */
static bool
hotp_module_match(const hotp_module_t *module, const app_pc base, const uint checksum,
                  const uint timestamp, const size_t image_size, const size_t code_size,
                  const uint64 file_version, const char *name, hotp_type_t type)
{
    uint hash_index, computed_hash;
    hotp_patch_point_hash_t *hash;
    bool matched;

    ASSERT(module != NULL && base != NULL);
    ASSERT(TESTANY(HOTP_TYPE_HOT_PATCH | HOTP_TYPE_GBOP_HOOK | HOTP_TYPE_PROBE, type));

    LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Matching module base " PFX " %s\n", base, name);

    /* For library offset or export function based patch points, the probe will
     * define a library by name (if needed we expand it to include the
     * liveshield type matching, but the client can do it outside) */
    /* gbop type patches provide a symbolic name to hook, so there is nothing
     * to match it with other than the pe name.
     */
    if (TESTALL(HOTP_TYPE_PROBE, type) IF_GBOP(|| (TESTALL(HOTP_TYPE_GBOP_HOOK, type)))) {
        ASSERT(module->sig.pe_checksum == 0 && module->sig.pe_timestamp == 0 &&
               module->sig.pe_image_size == 0 && module->sig.pe_code_size == 0 &&
               module->sig.pe_file_version == 0 && module->num_patch_points == 1 &&
               module->patch_points != NULL && module->num_patch_point_hashes == 0 &&
               module->hashes == NULL);
        if (name == NULL) {
            /* if the only check is the module name, then a NULL name means
             * the module wasn't matched;  otherwise this check would be bogus.
             */
            return false;
        } else if (IF_UNIX_ELSE(strncmp, strncasecmp)(module->sig.pe_name, name,
                                                      MAXIMUM_PATH) == 0) {
            /* FIXME: strcmp() is faster than the ignore case version,
             * but we shouldn't rely on the PE name case to be the
             * same in all versions of Windows.
             */
            return true;
        } else {
            return false;
        }
    }

    /* These checks are for hot patch types, i.e., ones that have offset rvas
     * specified for each known version.
     * First stage check: PE timestamp, PE checksum, PE code_size, PE file
     * version & PE name, i.e., signature match.
     *
     * FIXME: Today error handling of PE parsing is not done by the core, so
     * unavailability of an attribute isn't recorded.  Thus IGNORE and
     * UNAVAILABLE are treated the same for module matching.  When the core can
     * handle it the UNAVAILABLE part should be removed from the checks, and
     * checks for unavailability should be done.  Case 9215 tracks the core not
     * handling PE parsing for malformed files and their impact on hot patching.
     */
    ASSERT(TESTALL(HOTP_TYPE_HOT_PATCH, type));

    matched = module->sig.pe_timestamp == timestamp ||
        module->sig.pe_timestamp == PE_TIMESTAMP_IGNORE ||
        module->sig.pe_timestamp == PE_TIMESTAMP_UNAVAILABLE;

    matched = matched &&
        (module->sig.pe_checksum == checksum ||
         module->sig.pe_checksum == PE_CHECKSUM_IGNORE ||
         module->sig.pe_checksum == PE_CHECKSUM_UNAVAILABLE);

    matched = matched &&
        (module->sig.pe_image_size == image_size ||
         module->sig.pe_image_size == PE_IMAGE_SIZE_IGNORE ||
         module->sig.pe_image_size == PE_IMAGE_SIZE_UNAVAILABLE);

    matched = matched &&
        (module->sig.pe_code_size == code_size ||
         module->sig.pe_code_size == PE_CODE_SIZE_IGNORE ||
         module->sig.pe_code_size == PE_CODE_SIZE_UNAVAILABLE);

    matched = matched &&
        (module->sig.pe_file_version == file_version ||
         module->sig.pe_file_version == PE_FILE_VERSION_IGNORE ||
         module->sig.pe_file_version == PE_FILE_VERSION_UNAVAILABLE);

    matched = matched &&
        ((strncmp(module->sig.pe_name, PE_NAME_IGNORE, sizeof(PE_NAME_IGNORE)) == 0) ||
         (name == NULL && /* no name case */
          module->sig.pe_name[0] == PE_NAME_UNAVAILABLE) ||
         (name != NULL && (strncmp(module->sig.pe_name, name, MAXIMUM_PATH) == 0)));

    if (matched) {
        LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Module signature check passed\n");

        /* First stage check was true, now let us do the second stage check,
         * i.e., check the hashes of patch points in the module.
         */
        ASSERT(module->num_patch_point_hashes > 0 && module->hashes != NULL);
        for (hash_index = 0; hash_index < module->num_patch_point_hashes; hash_index++) {
            hash = &module->hashes[hash_index];
            computed_hash = hotp_compute_hash(base, hash);
            if (computed_hash != hash->hash_value)
                return false;
        }
        LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Patch point hash check passed\n");
        return true;
    }
    return false;
}

/* Used to compute the hash of a patch point hash region.  In hotp_only mode,
 * if there is an overlap between a hash region and a patch region, the
 * image bytes, stored at the top of the trampoline, are used to create copy
 * of the image on which d_r_crc32 is computed.  In regular hotp mode, d_r_crc32 is
 * called directly.
 */
static uint
hotp_compute_hash(app_pc base, hotp_patch_point_hash_t *hash)
{
    uint crc, copy_size;
    char *hash_buf, *copy;
    app_pc hash_start, hash_end, vm_start, vm_end, src, dst, trampoline;
    vmvector_iterator_t iterator;
    hotp_offset_match_t *ppoint_desc;

    ASSERT(base != NULL && hash != NULL);
    ASSERT(hash->start > 0 && hash->len > 0);

    hash_start = base + hash->start;
    hash_end = hash_start + hash->len;

    /* If the hash region overlaps with any patch point region, then use the
     * original image bytes to compute the d_r_crc32.  Valid for hotp_only because
     * in hotp mode, i.e., with a code cache, we don't modify the original code.
     */
    if (DYNAMO_OPTION(hotp_only) &&
        vmvector_overlap(hotp_patch_point_areas, hash_start, hash_end)) {

        /* Make sure that the patch region size for hotp_only is correct. */
        ASSERT(HOTP_PATCH_REGION_SIZE == HOTP_ONLY_PATCH_REGION_SIZE);

        /* Allocate a buffer & copy the image bytes represented by the hash.
         * This will include bytes modified by a prior hotp_only patch.
         * Note: an extra 2 x HOTP_PATCH_REGION_SIZE is allocated to be used
         * as overflow buffers at the front & back of the copy; makes handling
         * the overlap scenarios (4 different ones) easy.
         */
        copy_size = hash->len + (2 * HOTP_PATCH_REGION_SIZE);
        copy = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, copy_size, ACCT_HOT_PATCHING,
                                PROTECTED);
        hash_buf = copy + HOTP_PATCH_REGION_SIZE;
        memcpy(hash_buf, hash_start, hash->len);

        /* Now, for each vmarea that overlaps, copy the original image bytes
         * into the buffer.
         * FIXME: we do a linear walk as opposed to walking over only those
         *  regions that overlap, ineffcient; see case 8211 about a new
         *  vmvector iterator that walks over only overlapping regions.
         */
        vmvector_iterator_start(hotp_patch_point_areas, &iterator);
        while (vmvector_iterator_hasnext(&iterator)) {
            ppoint_desc = vmvector_iterator_next(&iterator, &vm_start, &vm_end);
            trampoline =
                GLOBAL_PPOINT(ppoint_desc->vul_index, ppoint_desc->set_index,
                              ppoint_desc->module_index, ppoint_desc->ppoint_index)
                    .trampoline;

            /* If the patch isn't injected, overlap doesn't matter because
             * the image hasn't been changed.  Overlap with an uninjected patch
             * region can only happen when loader safety is in progress during
             * which a patch point is removed (only from the image, not
             * hotp_patch_point_areas) and it is re-injected; the re-injection
             * of the patch point will overlap with itself.  See case 8222.
             */
            if (trampoline == NULL) {
                /* If hash belongs ppoint_desc, i.e., overlaps with self, then
                 * base and module's base must match.
                 */
                ASSERT(base ==
                       GLOBAL_MODULE(ppoint_desc->vul_index, ppoint_desc->set_index,
                                     ppoint_desc->module_index)
                           .base_address);
                continue;
            }

            /* If the trampoline exists, it better be a valid one, i.e., the
             * patch corresponding to this vmarea must be injected.
             */
            ASSERT(vmvector_overlap(hotp_only_tramp_areas, trampoline,
                                    trampoline + HOTP_ONLY_TRAMPOLINE_SIZE));

            /* The size of each vmarea in hotp_patch_point_areas must be
             * equal to that of the patch region.
             */
            ASSERT(vm_end - vm_start == HOTP_PATCH_REGION_SIZE);

            /* The module corresponding to this vm area (patch point) should
             * have been matched by a vul. def. (in hotp_process_image).
             */
            ASSERT(GLOBAL_MODULE(ppoint_desc->vul_index, ppoint_desc->set_index,
                                 ppoint_desc->module_index)
                       .matched);

            /* There are a few scenarios for a hash & patch point to overlap,
             * vmarea fully within the hash area, vice versa, partial below,
             * partial above, and exact on either side or both
             * Using an extra buffer the size of a patch region at the front
             * and back allows all the scenarios to be handled with a single
             * equation - eliminates messy code; worth allocating 10 bytes
             * extra.
             * Note: the extra buffer can be 1 byte shorter on either side, but
             *  leaving it at patch point region size, just to be safe.
             */
            if (vm_start < hash_end && vm_end > hash_start) {
                src = trampoline;
                dst = (app_pc)hash_buf + (vm_start - hash_start);

                /* Just make sure that we don't trash anything when copying the
                 * original image over the bytes in hash_buf.
                 */
                ASSERT((dst >= (app_pc)copy) &&
                       ((dst + HOTP_PATCH_REGION_SIZE) <= ((app_pc)copy + copy_size)));

                /* If the hash overlaps with a patch point region, then the
                 * current image & the copy should be different, i.e., a patch
                 * must exist at that point.
                 */
                ASSERT(memcmp(dst, src, HOTP_PATCH_REGION_SIZE) != 0);

                /* CAUTION: this memcpy assumes the location & size of
                 * app code copy in the trampoline, i.e., the first 5 bytes of
                 * trampoline contain the original app code; so any changes
                 * should be kept in sync.
                 */
                memcpy(dst, src, HOTP_PATCH_REGION_SIZE);
            }
            /* FIXME: if the iterator guaranteed order, we can break out after
             *  the first non-match - optimization.
             */
        }
        vmvector_iterator_stop(&iterator);
        crc = d_r_crc32(hash_buf, hash->len);
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, copy, char, copy_size, ACCT_HOT_PATCHING,
                        PROTECTED);
    } else {
        /* No overlap; image is unmodified, so image's d_r_crc32 should be valid. */
        crc = d_r_crc32((char *)hash_start, hash->len);
    }
    return crc;
}

/* TODO: this function should be used for startup & nudge dll list walking,
 *          dll loading and unloading.
 * TODO: assert somehow that every time this function is invoked there must
 *          be a flush preceding or succeeding it, except at startup
 * TODO: os specific routine; move to win32/
 * TODO: this function is called when vm_areas_init() is invoked, but
 *          hotp_init() is called after vm_areas_init()!  bogus - check
 *          other start up scenarios like retakeover to see if policy reading
 *          & activation get out of order;  this is the same issue that vlad
 *          pointed out: make sure that process_image() is called after
 *          hotp_init()
 * TODO: process_{image,mmap}() should never be called on hot patch dlls
 *       because dr is loading them;  assert for this somewhere to prevent
 *       assumption violation bugs.
 */
static void
hotp_process_image_helper(const app_pc base, const bool loaded,
                          const bool own_hot_patch_lock, const bool just_check,
                          bool *needs_processing, const thread_record_t **thread_table,
                          const int num_threads, const bool ldr_safety,
                          vm_area_vector_t *toflush);
void
hotp_process_image(const app_pc base, const bool loaded, const bool own_hot_patch_lock,
                   const bool just_check, bool *needs_processing,
                   const thread_record_t **thread_table, const int num_threads)
{
    hotp_process_image_helper(base, loaded, own_hot_patch_lock, just_check,
                              needs_processing, thread_table, num_threads, false, NULL);
}

/* Helper routine for seeing if point is in hotp_ppoint_vec */
bool
hotp_ppoint_on_list(app_rva_t ppoint, app_rva_t *hotp_ppoint_vec,
                    uint hotp_ppoint_vec_num)
{
    bool on_list = false;
    uint i;
    /* We assume there are at most a handful of these so we don't sort.
     * If we add GBOP hooks we may want to do that. */
#    ifdef GBOP
    ASSERT(DYNAMO_OPTION(gbop) == 0);
#    endif
    ASSERT(ppoint != 0);
    ASSERT(hotp_ppoint_vec != NULL && hotp_ppoint_vec_num > 0);
    if (hotp_ppoint_vec == NULL)
        return false;
    for (i = 0; i < hotp_ppoint_vec_num; i++) {
        if (ppoint == hotp_ppoint_vec[i]) {
            on_list = true;
            break;
        }
    }
    return on_list;
}

/* Returns true if there is a persistent cache in [base,base+image_size) that
 * may contain code for any of the patch points of module
 */
static bool
hotp_perscache_overlap(uint vul, uint set, uint module, app_pc base, size_t image_size)
{
    vmvector_iterator_t vmvi;
    coarse_info_t *info;
    uint pp;
    bool flush_perscache = false;
    ASSERT(DYNAMO_OPTION(use_persisted_hotp));
    ASSERT(!DYNAMO_OPTION(hotp_only));
    vm_area_coarse_iter_start(&vmvi, base);
    /* We have a lot of nested linear walks here, esp. when called from
     * hotp_process_image_helper inside nested loops, but typically the coarse
     * iterator involves one binary search and only one match, and
     * hotp_ppoint_on_list and the pp for loop here only a few entries each;
     * so this routine shouldn't be a perf bottleneck by itself.
     */
    while (!flush_perscache && vm_area_coarse_iter_hasnext(&vmvi, base + image_size)) {
        info = vm_area_coarse_iter_next(&vmvi, base + image_size);
        ASSERT(info != NULL);
        if (info == NULL) /* be paranoid */
            continue;
        if (info->hotp_ppoint_vec == NULL)
            flush_perscache = true;
        else {
            ASSERT(info->persisted);
            for (pp = 0; pp < GLOBAL_MODULE(vul, set, module).num_patch_points; pp++) {
                if (!hotp_ppoint_on_list(GLOBAL_PPOINT(vul, set, module, pp).offset,
                                         info->hotp_ppoint_vec,
                                         info->hotp_ppoint_vec_num)) {
                    flush_perscache = true;
                    break;
                }
            }
        }
        /* Should be able to ignore 2ndary unit */
        ASSERT(info->non_frozen == NULL || info->non_frozen->hotp_ppoint_vec == NULL);
    }
    vm_area_coarse_iter_stop(&vmvi);
    return flush_perscache;
}

/* This helper exists mainly to handle the loader safety case for adding
 * ppoint areas.  vm_areas should be added to ppoint_areas only during module
 * load/unload (including the initial stack walk) and during policy read
 * nudge, not during a reinjection during loader safety.
 * The same holds good for removal, but today that isn't an
 * issue because loader safety uses hotp_remove_patches_from_module() to do
 * it, which doesn't modify ppoint areas.
 * FIXME: once hotp_inject_patches_into_module() is implemented
 * based on loaded_module_areas and used in hotp_only_mem_prot_change() instead
 * of hotp_process_image_helper, this can go.
 */
static void
hotp_process_image_helper(const app_pc base, const bool loaded,
                          const bool own_hot_patch_lock, const bool just_check,
                          bool *needs_processing, const thread_record_t **thread_table,
                          const int num_threads_arg, const bool ldr_safety,
                          vm_area_vector_t *toflush)
{
    uint vul_idx, set_idx, module_idx, ppoint_idx;
    hotp_module_t *module;
    dcontext_t *dcontext = get_thread_private_dcontext();
    uint checksum, timestamp;
    size_t image_size = 0, code_size;
    uint64 file_version;
    module_names_t *names = NULL;
    const char *name = NULL, *pe_name = NULL, *mod_name = NULL;
    int num_threads = num_threads_arg;
    bool any_matched = false;
    bool flush_perscache = false;
    bool perscache_range_overlap = false;

    ASSERT(base != NULL); /* Is it a valid dll in loaded memory? */

    LOG(GLOBAL, LOG_HOT_PATCHING, 2, "hotp_process_image " PFX " %s w/ %d vuls\n", base,
        loaded ? "load" : "unload", NUM_GLOBAL_VULS);

    ASSERT(dcontext != GLOBAL_DCONTEXT);
    /* note that during startup processing due to
     * find_executable_vm_areas() dcontext can in fact be NULL
     */
    if (dcontext != NULL && dcontext->nudge_thread) /* Fix for case 5367. */
        return;
#    ifdef WINDOWS
    if (num_threads == 0 && !just_check && DYNAMO_OPTION(hotp_only)) {
        /* FIXME PR 225578: dr_register_probes passes 0 for the thread count
         * b/c post-init probes are NYI: but to enable at-your-own risk probes
         * relaxing the assert
         */
        ASSERT_CURIOSITY_ONCE(!dynamo_initialized &&
                              "post-init probes at your own risk: PR 225578!");
        num_threads = HOTP_ONLY_NUM_THREADS_AT_INIT;
        /* For hotp_only, all threads should be suspended before patch injection.
         * However, at this point in startup, callback hooks aren't in place and
         * we don't know if any other thread is running around that the core
         * doesn't know about.  This would be rare and with early injection, rarer.
         * However, if that thread is executing in a region being patched we can
         * fail spectacularly.  Curiosity in the meanwhile.
         * Also, to be on the safe side grab the synchronization locks.
         */
        ASSERT_CURIOSITY(check_sole_thread());
        ASSERT(!own_hot_patch_lock); /* can't get hotp lock before sync locks */
        d_r_mutex_lock(&all_threads_synch_lock);
        d_r_mutex_lock(&thread_initexit_lock);
    }
#    endif

    if (!own_hot_patch_lock)
        d_r_write_lock(&hotp_vul_table_lock);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    /* Caller doesn't want to process the image, but to know if it matches. */
    if (just_check) {
        /* Only hotp_only needs this; not regular hot patching. */
        ASSERT(DYNAMO_OPTION(hotp_only));
        ASSERT(needs_processing != NULL);
        *needs_processing = false; /* will be set it to true, if needed */
    } else
        ASSERT(needs_processing == NULL);

    /* get module information from PE once (case 7990) */
    /* FIXME: once all pe information is available in loaded_module_areas, use
     *        that here
     * FIXME: file_version is obtained by walking the resouce section which is expensive;
     *        the same is true for code_size to some extent, i.e., expensive but not
     *        that much.  So we may be better off by computing them in separate routines
     *        predicated by the first check - and put all these into hotp_get_module_sig()
     */
    os_get_module_info_lock();
    if (!os_get_module_info_all_names(base, &checksum, &timestamp, &image_size, &names,
                                      &code_size, &file_version)) {
        /* FIXME: case 9778 - module info is now obtained from
         * loaded_module_areas vector, which doesn't seem to have hotp dll
         * info, so we hit this.  As a first step this is converted to a log
         * to make tests work;  will have to read it from pe directly (using
         * try/except) if it isn't a hotp dll - if that doesn't work then be
         * curious.  Also, need to find out if it was triggered only for hotp
         * dlls. */
        LOG(GLOBAL, LOG_HOT_PATCHING, 2, "unreadable PE base (" PFX ")?\n", base);
        os_get_module_info_unlock();
        goto hotp_process_image_exit;
    } else {
        /* Make our own copy of both the pe name and the general module name.
         * This is because pe name can be null for executables, which is fine
         * for liveshields, but not for gbop or probe api - they just specify a
         * module name, so we have to use any name that is available.  Note: as
         * of today, gbop hasn't been done on executables, which is why it
         * worked - it is broken for hooks in exes - a FIXME, but gbop is going
         * away anyway. */
        pe_name = dr_strdup(names->module_name HEAPACCT(ACCT_HOT_PATCHING));
        mod_name = dr_strdup(GET_MODULE_NAME(names) HEAPACCT(ACCT_HOT_PATCHING));
        os_get_module_info_unlock();
        /* These values can't be read in from a module, they are used by the
         * patch writer to hint to the core to ignore the corresponding checks.
         */
        ASSERT_CURIOSITY(timestamp != PE_TIMESTAMP_IGNORE &&
                         checksum != PE_CHECKSUM_IGNORE &&
                         image_size != PE_IMAGE_SIZE_IGNORE);
    }
#    ifdef WINDOWS
    DOCHECK(1, {
        if (TEST(ASLR_DLL, DYNAMO_OPTION(aslr)) &&
            TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache))) {
            /* case 8507 - the timestamp and possibly checksum of the current mapping,
               possibly ASLRed, may not be the same as the application DLL */
            uint pe_timestamp;
            uint pe_checksum;
            bool ok = os_get_module_info(base, &pe_checksum, &pe_timestamp, NULL, NULL,
                                         NULL, NULL);
            ASSERT_CURIOSITY(timestamp != 0);
            /* Note that if we don't find the DLL in the module list,
             * we'll keep using the previously found checksum and
             * timestamp.  Although normally all DLLs are expected to be
             * listed, currently that is done only with ASLR_TRACK_AREAS.
             */
            /* case 5381: we don't ASSERT(ok) b/c hotpatch DLLs aren't listed in our
             * own module areas, so we don't always find all modules */
            /* with the current scheme the checksum is still the original DLLs checksum
             * though it won't check, and the timestamp is bumped by one second
             */
            ASSERT(!ok || pe_checksum == checksum);
            ASSERT_CURIOSITY(!ok || pe_timestamp == timestamp ||
                             pe_timestamp == timestamp + 1);
        }
    });
#    endif /* WINDOWS */

    if (!DYNAMO_OPTION(hotp_only)) {
        perscache_range_overlap =
            executable_vm_area_persisted_overlap(base, base + image_size);
    }

    /* TODO: assert that 'base' is the module's base address,
     *       get_dll_short_name() expects this; will be used for sig check
     *       use the fn() that gets only what is in the PE
     * FIXME: eliminate this n^4 loop for each module {load,unload}; case 10683
     */
    for (vul_idx = 0; vul_idx < NUM_GLOBAL_VULS; vul_idx++) {
        bool set_matched = false;

        if (TESTALL(HOTP_TYPE_PROBE, GLOBAL_VUL(vul_idx).type)
                IF_GBOP(|| (TESTALL(HOTP_TYPE_GBOP_HOOK, GLOBAL_VUL(vul_idx).type)))) {
            /* FIXME PR 533522: state in the docs/comments which name is
             * used where!  pe_name vs mod_name
             */
            name = mod_name;
        } else {
            ASSERT(TESTALL(HOTP_TYPE_HOT_PATCH, GLOBAL_VUL(vul_idx).type));
            /* FIXME PR 533522: state in the docs/comments which name is
             * used where!  pe_name vs mod_name
             */
            name = pe_name;
        }

        for (set_idx = 0; set_idx < GLOBAL_VUL(vul_idx).num_sets; set_idx++) {
            /* Case 10248 - multiple sets can match, but only the first such set
             * should be used, the rest discarded.  In the old model only one
             * set matched, but it was changed to let the patch writer to
             * progressively relax the matching criteria. */
            if (set_matched)
                break;

            for (module_idx = 0; module_idx < GLOBAL_SET(vul_idx, set_idx).num_modules;
                 module_idx++) {
                module = &GLOBAL_MODULE(vul_idx, set_idx, module_idx);

                /* When unloading a matched dll in hotp_only mode, all injected
                 * patches must be removed before proceeding any further.
                 * Otherwise hotp_module_match() will fail in the id hash
                 * computation part due to a changed image, due to injection.
                 */
                if (base == module->base_address && !loaded) {
                    if (just_check) { /* caller doesn't want processing */
                        *needs_processing = true;
                        goto hotp_process_image_exit;
                    }

                    /* For hotp_only if a module matches all patch points
                     * in it must be removed in one shot;  just as they are
                     * injected in one shot.
                     */
                    if (GLOBAL_VUL(vul_idx).mode == HOTP_MODE_DETECT ||
                        GLOBAL_VUL(vul_idx).mode == HOTP_MODE_PROTECT) {
                        for (ppoint_idx = 0; ppoint_idx < module->num_patch_points;
                             ppoint_idx++) {
                            hotp_patch_point_t *ppoint;
                            ppoint = &module->patch_points[ppoint_idx];

                            if (DYNAMO_OPTION(hotp_only)) {
                                if (ppoint->trampoline != NULL) {
                                    hotp_only_remove_patch(dcontext, module, ppoint);
                                } else {
                                    /* If module is matched & mode is on, then the
                                     * patch must be injected unless it has been
                                     * removed to handle loader-safety issues.
                                     */
                                    ASSERT(hotp_globals->ldr_safe_hook_removal &&
                                           "hotp_only - double patch removal");
                                }
                            }
                            /* xref case 10736.
                             * For hotp_only, module load & inject, and
                             * similarly, module unload and remove are done
                             * together, so hot_patch_point_areas won't be out
                             * of synch.  However, for hotp with fcache, a
                             * module unload can remove the patches from
                             * hotp_patch_point_areas before flushing them.
                             * This can prevent the flush from happening if
                             * hotp_patch_point_areas is used for it (which
                             * isn't done today; case 10728).  It can also
                             * result in voiding a patch injection for a new bb
                             * in that module, i.e., module can be without a
                             * patch for a brief period till it is unloaded.
                             */
                            hotp_ppoint_areas_remove(hotp_ppoint_addr(module, ppoint));
                        }
                    }

                    /* Once hotp_only patches are removed, the module must
                     * match at this point.
                     * TODO: multiple vulnerabilities targeting the same module
                     *       & whose hashes overlap, won't be {inject,remove}d
                     *       because the image gets modified with the injection
                     *       of the first one and the hash check for the second
                     *       one will fail.
                     */
                    ASSERT_CURIOSITY(hotp_module_match(
                        module, base, checksum, timestamp, image_size, code_size,
                        file_version, name, GLOBAL_VUL(vul_idx).type));
                }

                /* FIXME: there's no reason to compute whether an OFF patch
                 * matches; just wasted cycles, as we come back here on
                 * any path that later turns the patch on, and no external
                 * stats rely on knowing whether an off patch matches.
                 */
                if (hotp_module_match(module, base, checksum, timestamp, image_size,
                                      code_size, file_version, name,
                                      GLOBAL_VUL(vul_idx).type)) {
                    set_matched = true;
                    if (just_check) { /* caller doesn't want processing */
                        *needs_processing = true;
                        goto hotp_process_image_exit;
                    }

                    if (loaded) { /* loading dll */
                        bool patch_enabled =
                            (GLOBAL_VUL(vul_idx).mode == HOTP_MODE_DETECT ||
                             GLOBAL_VUL(vul_idx).mode == HOTP_MODE_PROTECT);
                        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
                            "activating vulnerability %s while loading %s\n",
                            GLOBAL_VUL(vul_idx).vul_id, module->sig.pe_name);

                        any_matched = true;
                        /* Case 9970: See if we need to flush any
                         * perscaches in the region.  Once we decide to flush
                         * we're going to flush everything.  We avoid the later
                         * flush on a nudge in vm_area_allsynch_flush_fragments().
                         * We currently come here for OFF patches, so we explicitly
                         * check for that before flushing.
                         */
                        if (patch_enabled && perscache_range_overlap &&
                            !flush_perscache && DYNAMO_OPTION(use_persisted_hotp)) {
                            flush_perscache = hotp_perscache_overlap(
                                vul_idx, set_idx, module_idx, base, image_size);
                        }

                        /* TODO: check if all modules in the current
                         *        vulnerability are active; if so activate the
                         *        policy
                         *        also, add patch points to lookup structures
                         *        only if entire vulnerability is active;
                         *        needed to enforce atomicity of patch injection
                         */
                        /* the base is used to find the runtime address of
                         * patch offset in the current lookup routine; till a
                         * offset lookup hash is constructed the base address
                         * is needed because the offset in the patchpoint
                         * structure is read only data that should be fixed to
                         * point to the runtime address.  even then, the flush
                         * routine would need to know which offset, i.e.,
                         * runtime offset, to flush; so this base_address is
                         * needed or a runtime data field must be created.
                         */
                        module->base_address = base;
                        module->matched = true;
                        hotp_set_policy_status(vul_idx, HOTP_INJECT_PENDING);

                        /* gbop type hooks don't have patch offsets defined,
                         * as they use function names; must set them otherwise
                         * patching will blow up.
                         */
                        if (TESTALL(HOTP_TYPE_GBOP_HOOK, GLOBAL_VUL(vul_idx).type)) {
                            /* FIXME: assert on all patch point fields being 0,
                             * except precedence.
                             * also, ASSERT on func_addr & func_name;
                             */
                            app_pc func_addr;
                            app_rva_t offset;

                            /* gbop is only in -client mode, i.e., hotp_only */
                            ASSERT(DYNAMO_OPTION(hotp_only));

                            func_addr = (app_pc)d_r_get_proc_address(
                                (module_handle_t)base, GLOBAL_VUL(vul_idx).vul_id);
                            if (func_addr != NULL) { /* fix for case 7969 */
                                ASSERT(func_addr > base);
                                offset = func_addr - base;
                                module->patch_points[0].offset = offset;
                            } else {
                                /* Some windows versions won't have some gbop
                                 * hook funcs or get_proc_address might just
                                 * fail; either way just ignore such hooks.
                                 * TODO: think about this - design issue.
                                 */
                                module->base_address = NULL;
                                module->matched = false;
                                continue;
                            }
                        }

                        /* For hotp_only if a module matches all patch points
                         * in it must be injected in one shot.
                         */
                        if (patch_enabled) {
                            hotp_offset_match_t ppoint_desc;

                            ppoint_desc.vul_index = vul_idx;
                            ppoint_desc.set_index = set_idx;
                            ppoint_desc.module_index = module_idx;
                            for (ppoint_idx = 0; ppoint_idx < module->num_patch_points;
                                 ppoint_idx++) {
                                ppoint_desc.ppoint_index = ppoint_idx;

                                /* ldr_safety can happen only for hotp_only. */
                                ASSERT(DYNAMO_OPTION(hotp_only) || !ldr_safety);

                                /* Don't re-add a patch point to the vector
                                 * during patch injection while handling
                                 * loader safe injection.
                                 */
                                if (!ldr_safety)
                                    hotp_ppoint_areas_add(&ppoint_desc);

                                if (DYNAMO_OPTION(hotp_only)) {
                                    hotp_only_inject_patch(&ppoint_desc, thread_table,
                                                           num_threads);
                                }
                            }
                        }
                    } else { /* unloading dll */
                        /* TODO: same issues as in the 'if' block above, but
                         *        reverse.
                         */
                        module->base_address = NULL;
                        module->matched = false;
                        hotp_set_policy_status(vul_idx, HOTP_INJECT_NO_MATCH);
                    }
                }
            }
        }
    }

    if (!DYNAMO_OPTION(use_persisted_hotp)) /* else we check in loop above */
        flush_perscache = any_matched && perscache_range_overlap;
    if (flush_perscache) {
        ASSERT(any_matched && perscache_range_overlap);
        ASSERT(!DYNAMO_OPTION(hotp_only));
        /* During startup we process hotp before we add exec areas, so we
         * should only get a match in a later nudge, when we pass in toflush.
         */
        ASSERT(dynamo_initialized);
        ASSERT(toflush != NULL);
#    ifdef WINDOWS
        ASSERT(dcontext->nudge_target != NULL);
#    else
        ASSERT_NOT_REACHED(); /* No nudge on Linux, should only be here for nudge. */
#    endif
        if (toflush != NULL) { /* be paranoid (we fail otherwise though) */
            LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                "Hotp for " PFX "-" PFX " %s overlaps perscache, flushing\n", base,
                base + image_size, name);
            /* As we hold the hotp_vul_table_lock we cannot flush here;
             * instead we add to a pending-flush vmvector.
             */
            vmvector_add(toflush, base, base + image_size, NULL);
            STATS_INC(hotp_persist_flush);
            /* FIXME: we could eliminate this and rely on our later flush of the
             * patch area, as we're only coming here for nudges; we technically
             * only need an explicit check when loading a perscache, as long as
             * hotp defs are set up first.
             */
        }
    }

hotp_process_image_exit:
    if (pe_name != NULL)
        dr_strfree(pe_name HEAPACCT(ACCT_HOT_PATCHING));
    if (mod_name != NULL)
        dr_strfree(mod_name HEAPACCT(ACCT_HOT_PATCHING));
    /* Don't unlock in case the lock was already obtained before reaching this
     * function.  Only in that case lock_acquired will be false.
     */
    /* TODO: or does this go after flush? */
    if (!own_hot_patch_lock)
        d_r_write_unlock(&hotp_vul_table_lock);
        /* TODO: also there are some race conditions with nudging & policy lookup/
         *       injection; sort those out; flushing before or after reading the
         *       policy plays a role too.
         */
#    ifdef WINDOWS
    if (num_threads == HOTP_ONLY_NUM_THREADS_AT_INIT) {
        ASSERT(DYNAMO_OPTION(hotp_only));
        ASSERT(!just_check);
        ASSERT_CURIOSITY(check_sole_thread());
        d_r_mutex_unlock(&thread_initexit_lock);
        d_r_mutex_unlock(&all_threads_synch_lock);
    }
#    endif
}

/* If vec==NULL, returns the number of patch points for the
 *   matched vuls in [start,end).
 * Else, stores in vec the offsets for all the matched patch points in [start,end).
 * Returns -1 if vec!=NULL and vec_num is too small (still fills it up).
 * For now this routine assumes that [start,end) is contained in a single module.
 * The caller must own the hotp_vul_table_lock (as a read lock).
 */
static int
hotp_patch_point_persist_helper(const app_pc start, const app_pc end, app_rva_t *vec,
                                uint vec_num)
{
    uint num_ppoints = 0;
    uint vul, set, module, pp;
    /* FIXME: check [start,end) instead of module */
    app_pc modbase = get_module_base(start);
    ASSERT(modbase == get_module_base(end));
    ASSERT(start != NULL); /* only support single module for now */
    ASSERT_OWN_READ_LOCK(true, &hotp_vul_table_lock);
    if (GLOBAL_VUL_TABLE == NULL)
        return 0;
    /* FIXME: once hotp_patch_point_areas is not just hotp_only, use it here */
    for (vul = 0; vul < NUM_GLOBAL_VULS; vul++) {
        bool set_processed = false;

        /* Ignore if off or dll wasn't loaded */
        if (GLOBAL_VUL(vul).mode == HOTP_MODE_OFF ||
            GLOBAL_VUL(vul).hotp_dll_base == NULL)
            continue;
        for (set = 0; set < GLOBAL_VUL(vul).num_sets; set++) {
            /* Only the first matching set should be used; case 10248. */
            if (set_processed)
                break;

            for (module = 0; module < GLOBAL_SET(vul, set).num_modules; module++) {
                if (!GLOBAL_MODULE(vul, set, module).matched ||
                    modbase != GLOBAL_MODULE(vul, set, module).base_address)
                    continue;
                set_processed = true;
                if (vec == NULL) {
                    num_ppoints += GLOBAL_MODULE(vul, set, module).num_patch_points;
                } else {
                    for (pp = 0; pp < GLOBAL_MODULE(vul, set, module).num_patch_points;
                         pp++) {
                        if (num_ppoints >= vec_num) {
                            /* It's ok to get here, just currently no callers do */
                            ASSERT_NOT_TESTED();
                            return -1;
                        }
                        vec[num_ppoints++] = GLOBAL_PPOINT(vul, set, module, pp).offset;
                    }
                }
            }
        }
    }
    return num_ppoints;
}

/* Returns the number of patch points for the matched vuls in [start,end).
 * For now this routine assumes that [start,end) is contained in a single module.
 * The caller must own the hotp_vul_table_lock (as a read lock).
 */
int
hotp_num_matched_patch_points(const app_pc start, const app_pc end)
{
    return hotp_patch_point_persist_helper(start, end, NULL, 0);
}

/* Stores in vec the offsets for all the matched patch points in [start,end).
 * Returns -1 if vec_num is too small (still fills it up).
 * For now this routine assumes that [start,end) is contained in a single module.
 * The caller must own the hotp_vul_table_lock (as a read lock).
 */
int
hotp_get_matched_patch_points(const app_pc start, const app_pc end, app_rva_t *vec,
                              uint vec_num)
{
    return hotp_patch_point_persist_helper(start, end, vec, vec_num);
}

/* Checks whether any matched patch point in [start, end) is not listed on
 * hotp_ppoint_vec.  If hotp_ppoint_vec is NULL just checks whether any patch
 * point is matched in the region.  For now this routine assumes that
 * [start,end) is contained in a single module.
 */
bool
hotp_point_not_on_list(const app_pc start, const app_pc end, bool own_hot_patch_lock,
                       app_rva_t *hotp_ppoint_vec, uint hotp_ppoint_vec_num)
{
    /* We could use hotp_process_image_helper()'s just_check but would have
     * to add hotp_ppoint_vec arg; plus we don't care about module matching.
     */
    bool not_on_list = false;
    uint vul, set, module, pp;
    /* FIXME: check [start,end) instead of module */
    app_pc modbase = get_module_base(start);
    DEBUG_DECLARE(bool matched = false;)
    ASSERT(modbase == get_module_base(end));
    if (!own_hot_patch_lock)
        d_r_read_lock(&hotp_vul_table_lock);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);
    if (GLOBAL_VUL_TABLE == NULL)
        goto hotp_policy_list_exit;
    /* FIXME: I would make an iterator to share w/ patch_point_persist_helper but
     * this many-nested loop lookup should go away in general ASAP and be
     * replaced w/ hotp_patch_point_areas which is currently only hotp_only.
     */
    for (vul = 0; vul < NUM_GLOBAL_VULS; vul++) {
        bool set_processed = false;

        /* Ignore if off or dll wasn't loaded */
        if (GLOBAL_VUL(vul).mode == HOTP_MODE_OFF ||
            GLOBAL_VUL(vul).hotp_dll_base == NULL)
            continue;
        for (set = 0; set < GLOBAL_VUL(vul).num_sets; set++) {
            /* Only the first matching set should be used; case 10248. */
            if (set_processed)
                break;

            for (module = 0; module < GLOBAL_SET(vul, set).num_modules; module++) {
                if (!GLOBAL_MODULE(vul, set, module).matched ||
                    modbase != GLOBAL_MODULE(vul, set, module).base_address)
                    continue;
                /* We have a match; only ok if on the list */
                DODEBUG({ matched = true; });
                set_processed = true;
                ASSERT(!not_on_list); /* should have exited if not on list */
                not_on_list = true;
                if (hotp_ppoint_vec != NULL && DYNAMO_OPTION(use_persisted_hotp)) {
                    for (pp = 0; pp < GLOBAL_MODULE(vul, set, module).num_patch_points;
                         pp++) {
                        if (!hotp_ppoint_on_list(
                                GLOBAL_PPOINT(vul, set, module, pp).offset,
                                hotp_ppoint_vec, hotp_ppoint_vec_num))
                            goto hotp_policy_list_exit;
                    }
                    not_on_list = false;
                } else
                    goto hotp_policy_list_exit;
            }
        }
    }
hotp_policy_list_exit:
    if (!own_hot_patch_lock)
        d_r_read_unlock(&hotp_vul_table_lock);
    DOSTATS({
        if (matched && !not_on_list) {
            ASSERT(hotp_ppoint_vec != NULL && DYNAMO_OPTION(use_persisted_hotp));
            STATS_INC(perscache_hotp_conflict_avoided);
        }
    });
    return not_on_list;
}

/* TODO: change this to walk the new PE list (not for now though); needed only
 *          during nudge; start up walk is already done by the core, piggyback
 *          on that and call hotp_process_image() there; basically, get rid
 *          of the need to walk the loader list
 *          Note: for -probe_api, we walk the module list at start up
 *                because client init is done after vmareas_init, i.e., after
 *                scanning for modules in memory and processing them.
 *
 */
static void
hotp_walk_loader_list(thread_record_t **thread_table, const int num_threads,
                      vm_area_vector_t *toflush, bool probe_init)
{
    /* This routine will go away; till then need to compile on linux.  Not walking
     * the module list on linux means that no vulnerability will get activated
     * for injection; that is ok as we aren't trying to build a linux version now.
     */
#    ifdef WINDOWS
    /* TODO: this routine uses PEB, etc, this has to be os specific */
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *start;
    LDR_MODULE *mod;

    /* For hotp_only, thread_table can be valid, i.e., all known threads may be
     * suspended.  Even if they are not, all synch locks will be held, so that
     * module processing can happen without races.  Check for that.
     * Note: for -probe_api, this routine can be called during dr init time,
     * i.e., synch locks won't be held, so we shouldn't assert.
     */
    if (!probe_init) {
        ASSERT_OWN_MUTEX(DYNAMO_OPTION(hotp_only), &all_threads_synch_lock);
        ASSERT_OWN_MUTEX(DYNAMO_OPTION(hotp_only), &thread_initexit_lock);
    }

    /* Flushing of pcaches conflicting with hot patches is handled for dll
     * loads by the pcache loads.  Conflicts at hotp_init time can't happen as
     * pcaches won't be loaded then (they are loaded in vm_areas_init which
     * comes afterwards).  However for nudging and client init
     * (dr_register_probes) this is needed because pcaches can be loaded by
     * then.  Note even though client init happens during startup, it happens
     * after vm_areas_init, hence pcaches can be loaded.  PR 226578 tracks
     * implementing pcache flushes for probe api - till then this assert is
     * relaxed.
     */
    ASSERT(toflush != NULL || DYNAMO_OPTION(hotp_only) ||
           (DYNAMO_OPTION(probe_api) && !DYNAMO_OPTION(use_persisted)));

    start = &ldr->InLoadOrderModuleList;
    for (e = start->Flink; e != start; e = e->Flink) {
        mod = (LDR_MODULE *)e;

        /* TODO: ASSERT that the module is loaded? */
        hotp_process_image_helper(mod->BaseAddress, true, probe_init ? false : true,
                                  false, NULL, thread_table, num_threads, false /*!ldr*/,
                                  toflush);

        /* TODO: make hotp_process_image() emit different log messages
         *          depending upon which path it is invoked from.
         */
    }
#    endif /* WINDOWS */
}

void
hotp_init(void)
{
    ASSIGN_INIT_READWRITE_LOCK_FREE(hotp_vul_table_lock, hotp_vul_table_lock);

    /* Assuming no locks are held on while initializing hot patching. */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(DYNAMO_OPTION(hot_patching));
#    ifdef GBOP
    /* gbop can't be turned on without hotp_only. */
    ASSERT(DYNAMO_OPTION(hotp_only) || !DYNAMO_OPTION(gbop));
#    endif

    if (DYNAMO_OPTION(hotp_only)) {
        VMVECTOR_ALLOC_VECTOR(hotp_only_tramp_areas, GLOBAL_DCONTEXT,
                              VECTOR_SHARED | VECTOR_NEVER_MERGE,
                              hotp_only_tramp_areas_lock);
    }

    d_r_write_lock(&hotp_vul_table_lock);

#    ifdef DEBUG
    hotp_globals =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, hotp_globals_t, ACCT_HOT_PATCHING, PROTECTED);
    hotp_globals->ldr_safe_hook_removal = false;
    hotp_globals->ldr_safe_hook_injection = false;
#    endif
    /* Currently hotp_patch_point_areas is used for hotp_only to do module
     * matching (case 7279) and offset lookup (case 8132), and offset lookup
     * only for hotp with fcache (case 10075).  Later on it will be
     * used for patch injection, removal, perscache stuff, etc; case 10728.
     */
    VMVECTOR_ALLOC_VECTOR(hotp_patch_point_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE,
                          hotp_patch_point_areas_lock);

    /* hotp_only trampolines should be allocated on a special heap that allows
     * code to be executed in it.
     */
    if (DYNAMO_OPTION(hotp_only)) {
        hotp_only_tramp_heap =
            special_heap_init(HOTP_ONLY_TRAMPOLINE_SIZE, true, /* yes, use a lock */
                              true,                            /* make it executable */
                              true /* it is persistent */);
    }
    ASSERT(GLOBAL_VUL_TABLE == NULL && NUM_GLOBAL_VULS == 0);
    GLOBAL_VUL_TABLE = hotp_read_policy_defs(&NUM_GLOBAL_VULS);
    if (GLOBAL_VUL_TABLE != NULL) {
        hotp_load_hotp_dlls(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS);
        hotp_read_policy_modes(NULL);
        /* Policy status table must be initialized after the global
         * vulnerability table and modes are read, but before module list
         * is iterated over.
         */
        hotp_init_policy_status_table();

        /* We don't need to call hotp_walk_loader_list() here as
         * find_executable_vm_areas() will call hotp_process_image() for us.
         */
    } else {
        LOG(GLOBAL, LOG_HOT_PATCHING, 2, "No hot patch definitions to read\n");
    }

    /* Release locks. */
    d_r_write_unlock(&hotp_vul_table_lock);

    /* Can't hold any locks at the end of hot patch initializations. */
    ASSERT_OWN_NO_LOCKS();
}

/* thread-shared initialization that should be repeated after a reset */
void
hotp_reset_init(void)
{
    /* nothing to do */
}

/* Free all thread-shared state not critical to forward progress;
 * hotp_reset_init() will be called before continuing.
 */
void
hotp_reset_free(void)
{
    /* Free old tables.  Hot patch code must ensure that no old table
     * pointer is kept across a synch-all point, which is also a reset
     * point (case 7760 & 8921).
     */
    hotp_vul_tab_t *current_tab, *temp_tab;
    if (!DYNAMO_OPTION(hot_patching))
        return;
    d_r_write_lock(&hotp_vul_table_lock);
    temp_tab = hotp_old_vul_tabs;
    while (temp_tab != NULL) {
        current_tab = temp_tab;
        temp_tab = temp_tab->next;
        hotp_free_vul_table(current_tab->vul_tab, current_tab->num_vuls);
        heap_free(GLOBAL_DCONTEXT, current_tab,
                  sizeof(hotp_vul_tab_t) HEAPACCT(ACCT_HOT_PATCHING));
    }
    hotp_old_vul_tabs = NULL;
    d_r_write_unlock(&hotp_vul_table_lock);
}

/* Free up all allocated memory and delete hot patching lock. */
void
hotp_exit(void)
{
    /* This assert will ensure that there is only one thread in the process
     * during exit.  Grab the hot patch lock all the same because a nudge
     * can come in at this point; freeing things without the lock is bad.
     */
    ASSERT(dynamo_exited);
    ASSERT(DYNAMO_OPTION(hot_patching));
    d_r_write_lock(&hotp_vul_table_lock);

    /* Release the hot patch policy status table if allocated.  This table
     * may not be allocated till the end if there were no hot patch definitions
     * but -hot_patching was turned on.
     */
    if (hotp_policy_status_table != NULL) {
        heap_free(GLOBAL_DCONTEXT, hotp_policy_status_table,
                  hotp_policy_status_table->size HEAPACCT(ACCT_HOT_PATCHING));
        hotp_policy_status_table = NULL;
    }

    /* Release the patch point areas vector before the table. */
    hotp_ppoint_areas_release();
    vmvector_delete_vector(GLOBAL_DCONTEXT, hotp_patch_point_areas);
    hotp_patch_point_areas = NULL;

    /* Release the global vulnerability table and old tables if any. */
    hotp_free_vul_table(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS);
    /* case 8118: set to NULL since referenced in hotp_print_diagnostics() */
    GLOBAL_VUL_TABLE = NULL;

#    ifdef DEBUG
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, hotp_globals, hotp_globals_t, ACCT_HOT_PATCHING,
                   PROTECTED);
#    endif
    d_r_write_unlock(&hotp_vul_table_lock);

    hotp_reset_free();

    if (DYNAMO_OPTION(hotp_only)) {
#    ifdef WINDOWS
        /* Don't free the heap upon detach - app may have hooked with our
         * trampoline code; case 9593.  Make this memory efficient, i.e., delete
         * the heap if no collisions were detected; part of bookkeepping needed
         * to not leak all removed hotp trampolines, but only those that have a
         * potential collision; a minor TODO - - would save a max of 50kb.
         * Note: heap lock should be deleted even if heap isn't! */
        /* If hotp_only_tramp_heap_cache is NULL, it means that no patches
         * were removed (either because they weren't injected or just not
         * removed).  This means we don't have to leak the trampolines even
         * for detach (PR 215520). */
        if (!doing_detach || hotp_only_tramp_heap_cache == NULL)
            special_heap_exit(hotp_only_tramp_heap);
#        ifdef DEBUG
        else
            special_heap_delete_lock(hotp_only_tramp_heap);
#        endif
#    else
        special_heap_exit(hotp_only_tramp_heap);
#    endif

        hotp_only_tramp_heap = NULL;
        vmvector_delete_vector(GLOBAL_DCONTEXT, hotp_only_tramp_areas);
        hotp_only_tramp_areas = NULL;
    }

    DELETE_READWRITE_LOCK(hotp_vul_table_lock);
}

/* Hot patch policy update action handler */
bool
nudge_action_read_policies(void)
{
    hotp_vul_t *old_vul_table = NULL, *new_vul_table = NULL;
    uint num_old_vuls = 0, num_new_vuls;
    int num_threads = 0;
    thread_record_t **thread_table = NULL;

    STATS_INC(hotp_num_policy_nudge);
    /* Fix for case 6090;  TODO: remove when -hotp_policy_size is removed */
    synchronize_dynamic_options();
    new_vul_table = hotp_read_policy_defs(&num_new_vuls);
    if (new_vul_table != NULL) {
        bool old_value;
        hotp_vul_tab_t *temp;
        dr_where_am_i_t wherewasi;
        dcontext_t *dcontext = get_thread_private_dcontext();
        vm_area_vector_t toflush; /* never initialized for hotp_only */

        /* If dynamo_exited was false till the check in this routine, then
         * this thread would have been intercepted by the core, i.e., it
         * would have got a dcontext.  The assert is to catch
         * bugs; the if is to make sure that the release build doesn't
         * crash in case this happens.
         */
        ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
        if (dcontext == NULL) {
            return false; /* skip further processing */
        }

        /* When the nudge thread starts up, the core takes control and
         * lets it go once it is identified as nudge.  However,
         * under_dynamo_control is still true because we come here from the cache.
         * We need to set under_dynamo_control to false during hot patch dll loading,
         * otherwise the core will take over again at the dll loading interception point.
         * Once hot patch dlls are loaded we restore under_dynamo_control in case it's
         * relied on elsewhere. Note - this isn't needed for
         * loading hot patch dlls at startup because thread init comes
         * after hotp_init(), so under_dynamo_control isn't set.  Only
         * hot patch dll loading during nudge needs this.
         * TODO: under_dynamo_control needs cleanup - see case 529, 5183.
         */
        old_value = dcontext->thread_record->under_dynamo_control;
        dcontext->thread_record->under_dynamo_control = false;

        /* Fix for case 5367.  TODO: undo fix after writing own loader. */
        wherewasi = dcontext->whereami;
        dcontext->whereami = DR_WHERE_APP; /* DR_WHERE_APP?  more like DR_WHERE_DR */
        dcontext->nudge_thread = true;

        /* Fix for case 5376.  There can be a deadlock if a nudge happened
         * to result in hot patch dlls being loaded when at the same time
         * an app dll was being loaded; hotp_vul_table_lock & LoaderLock
         * would create a deadlock.  So while loading the hot patch dlls
         * the hotp_vul_table_lock shouldn't be held.
         * To avoid this the table is read, stored in a temporary variable
         * and hot patch dlls are loaded using that temp. table - all this
         * is now done without the hotp_vul_table_lock.  Then the vul table
         * lock is grabbed (see below) and the global table is setup.
         *
         * FIXME: The longer term solution is to have our own loader to
         * load hot patch dlls.
         */
        hotp_load_hotp_dlls(new_vul_table, num_new_vuls);

        /* Must be set to false, otherwise the subsequent module list
         * walking will be useless, i.e., won't be able to identify
         * modules for hot patching because hotp_process_image() won't work.
         */
        dcontext->nudge_thread = false;

        /* If whereami changed, that means, the probably was a callback,
         * which can lead to other bugs.  So, let us make sure it doesn't.
         */
        ASSERT(dcontext->whereami == DR_WHERE_APP);
        dcontext->whereami = wherewasi;
        dcontext->thread_record->under_dynamo_control = old_value;

        /* Suspend all threads (for hotp_only) and grab locks. */
        if (DYNAMO_OPTION(hotp_only)) {
#    ifdef WINDOWS
            DEBUG_DECLARE(bool ok =)
            synch_with_all_threads(THREAD_SYNCH_SUSPENDED, &thread_table,
                                   /* Case 6821: other synch-all-thread uses that
                                    * only care about threads carrying fcache
                                    * state can ignore us
                                    */
                                   &num_threads, THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                   /* if we fail to suspend a thread (e.g., privilege
                                    * problems) ignore it. FIXME: retry instead? */
                                   THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
            ASSERT(ok);
#    endif
        }
        /* CAUTION: Setting up the global table, reading modes, setting up
         * policy status table and module list walking MUST all be
         * done in that order with the table lock held as all of them
         * update the global table.
         */
        d_r_write_lock(&hotp_vul_table_lock);

        /* For hotp_only, all patches have to be removed before doing
         * anything with new vulnerability data, and nothing after that,
         * which is unlike hotp, where removal has to be done before & after.
         */
        if (DYNAMO_OPTION(hotp_only)) {
            hotp_remove_hot_patches(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, true, NULL);
        }
        /* Save the old table for flushing & launch the new table. */
        old_vul_table = GLOBAL_VUL_TABLE;
        num_old_vuls = NUM_GLOBAL_VULS;
        hotp_ppoint_areas_release(); /* throw out the old patch points */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        GLOBAL_VUL_TABLE = new_vul_table;
        NUM_GLOBAL_VULS = num_new_vuls;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

        hotp_read_policy_modes(NULL);

        /* Policy status table must be initialized after the global
         * vulnerability table and modes are read, but before module list
         * is iterated over.
         */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        hotp_init_policy_status_table();
        if (!DYNAMO_OPTION(hotp_only))
            vmvector_init_vector(&toflush, 0); /* no lock init needed since not used */
        hotp_walk_loader_list(thread_table, num_threads,
                              DYNAMO_OPTION(hotp_only) ? NULL : &toflush,
                              false /* !probe_init */);
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

        /* Release all locks. */
        d_r_write_unlock(&hotp_vul_table_lock);
#    ifdef WINDOWS
        if (DYNAMO_OPTION(hotp_only)) {
            end_synch_with_all_threads(thread_table, num_threads, true /*resume*/);
        }
#    endif

        /* If a new vulnerability table was created, then flush the bbs
         * with hot patches from the old table and then free that table.
         * Note, the old table has to be freed outside the scope of the
         * hotp_vul_table_lock because bbs corresponding to that table
         * can't be flushed inside it.  See flushing comments below.
         */
        ASSERT(old_vul_table != GLOBAL_VUL_TABLE);

        if (!DYNAMO_OPTION(hotp_only)) {
            if (!vmvector_empty(&toflush)) {
                ASSERT(DYNAMO_OPTION(coarse_units) && DYNAMO_OPTION(use_persisted));
                /* case 9970: we must flush the perscache and ibl tables.
                 * FIXME optimization: don't flush the fine-grained fragments
                 * or non-persisted unit(s) (there can be multiple).
                 */
                flush_vmvector_regions(get_thread_private_dcontext(), &toflush,
                                       false /*keep futures*/,
                                       false /*exec still valid*/);
            }
            /* FIXME: don't need to flush non-persisted coarse units since
             * patch points are fine-grained: would have to widen
             * flush interface.  Note that we do avoid flushing perscaches
             * that do not contain the old patch points.
             */
            hotp_remove_hot_patches(old_vul_table, num_old_vuls, false, NULL);
            vmvector_reset_vector(GLOBAL_DCONTEXT, &toflush);
        } /* else toflush is uninitialized */

        /* Freeing the old vulnerability table immediately causes a race
         * with hot patch execution (see case 5521), so it is put on a free
         * list and freed at a reset or dr exit. hotp_vul_table_lock must be held here;
         * though this list is a new structure, a new lock is unnecessary.
         * Also, don't chain empty tables; a NULL table can occur when
         * no hot patches are loaded during startup, but are nudged in.
         *
         * Case 8921: We can't add to the old list prior to removing
         * hot patches since the synch-all for -coarse_unit or
         * -hotp_only flushing is a reset point and the table can then
         * be freed underneath us.  Thus we pay the cost of re-acquiring the lock.
         * This can also end up with tables on the old list in a different order
         * than their nudges, but that's not a problem.
         * FIXME case 8921: -hotp_only should free the table up front
         * FIXME: we should synch-all once, up front, and then avoid this ugliness
         * as well as multiple flush synchs.
         * FIXME: hotp could indirect the table like hotp_only to allow
         * earlier freeing.
         */
        /* FIXME: don't add to old table list if in hotp_only mode, there is no
         * eed because there is a lookup before execution and there is no lazy
         * flush going on.
         */
        if (old_vul_table != NULL) {
            temp = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, hotp_vul_tab_t, ACCT_HOT_PATCHING,
                                   PROTECTED);
            temp->vul_tab = old_vul_table;
            temp->num_vuls = num_old_vuls;
            d_r_write_lock(&hotp_vul_table_lock);
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            temp->next = hotp_old_vul_tabs;
            hotp_old_vul_tabs = temp;
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
            d_r_write_unlock(&hotp_vul_table_lock);
        }
    } else {
        /* Note, if the new table wasn't read in successfully, then the old
         * table isn't touched, i.e., status quo is maintained.
         */
        LOG(GLOBAL, LOG_HOT_PATCHING, 2, "No hot patch policies to read\n");
    }
    return true;
}

#    ifdef WINDOWS /* no nudging yet on Linux */
/* This routine handles hot patch nudges. */
void
hotp_nudge_handler(uint nudge_action_mask)
{
    /* Note, multiple nudges will be synchronized by the hotp_vul_table_lock.
     * It is irrelevant if nudge threads change order between reading and
     * flushing. */

    ASSERT(DYNAMO_OPTION(liveshields) && DYNAMO_OPTION(hot_patching));
    ASSERT(nudge_action_mask != 0); /* else shouldn't be called */

    if (TEST(NUDGE_GENERIC(lstats), nudge_action_mask)) {
        SYSLOG_INTERNAL_WARNING("Stat dumping for hot patches not done yet.");
    }

    if (TEST(NUDGE_GENERIC(policy), nudge_action_mask)) {
        LOG(GLOBAL, LOG_HOT_PATCHING, 1, "\n nudged to read in policy\n");
        nudge_action_read_policies();
    }

    if (TEST(NUDGE_GENERIC(mode), nudge_action_mask)) {
        thread_record_t **thread_table = NULL;
        int num_threads = 0;
        hotp_policy_mode_t *old_modes = NULL;
        vm_area_vector_t toflush; /* never initialized for hotp_only */

        LOG(GLOBAL, LOG_HOT_PATCHING, 1, "\n nudged to read in policy\n");

        STATS_INC(hotp_num_mode_nudge);

        /* If -liveshields isn't on, then modes nudges are irrelevant. */
        if (!DYNAMO_OPTION(liveshields))
            return;

        /* Suspend all threads (for hotp_only) and grab locks. */
        if (DYNAMO_OPTION(hotp_only)) {
            DEBUG_DECLARE(bool ok =)
            synch_with_all_threads(THREAD_SYNCH_SUSPENDED, &thread_table,
                                   /* Case 6821: other synch-all-thread uses that
                                    * only care about threads carrying fcache
                                    * state can ignore us
                                    */
                                   &num_threads, THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                   /* if we fail to suspend a thread (e.g., privilege
                                    * problems) ignore it. FIXME: retry instead? */
                                   THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
            ASSERT(ok);
        }
        d_r_write_lock(&hotp_vul_table_lock);

        /* For hotp_only, all patches have to be removed before doing anything
         * with new mode data; loader list walking will inject new ones.
         */
        if (DYNAMO_OPTION(hotp_only)) {
            hotp_remove_hot_patches(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, true, NULL);
        }
        hotp_ppoint_areas_release(); /* throw out the old patch points */
        /* Old modes are for regular hot patching, not for hotp_only. */
        hotp_read_policy_modes(DYNAMO_OPTION(hotp_only) ? NULL : &old_modes);

        /* Policy status table must be initialized after the global
         * vulnerability table and modes are read, but before module list
         * is iterated over.
         */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        hotp_init_policy_status_table();
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

        if (!DYNAMO_OPTION(hotp_only))
            vmvector_init_vector(&toflush, 0); /* no lock init needed since not used */
        hotp_walk_loader_list(thread_table, num_threads,
                              DYNAMO_OPTION(hotp_only) ? NULL : &toflush,
                              false /* !probe_init */);

        /* Release all locks. */
        d_r_write_unlock(&hotp_vul_table_lock);
        if (DYNAMO_OPTION(hotp_only)) {
            end_synch_with_all_threads(thread_table, num_threads, true /*resume*/);
        }

        /* If modes did change, then we need to flush out patches that were
         * injected because their old modes were on (detect or protect).
         * Fix for case 6619; resulted in using old_modes for patch removal.
         * Note: Just like policy reading, flushing has to be done outside the
         * scope of the hotp_vul_table_lock & ONLY after reading the new modes.
         */
        if (!DYNAMO_OPTION(hotp_only)) {
            if (!vmvector_empty(&toflush)) {
                /* case 9970: we must flush the perscache and ibl tables.
                 * FIXME optimization: don't flush the fine-grained fragments
                 * or non-persisted unit(s) (there can be multiple).
                 */
                flush_vmvector_regions(get_thread_private_dcontext(), &toflush,
                                       false /*keep futures*/,
                                       false /*exec still valid*/);
            }
            hotp_remove_hot_patches(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, false, old_modes);
            if (old_modes != NULL) {
                HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, old_modes, hotp_policy_mode_t,
                                NUM_GLOBAL_VULS, ACCT_HOT_PATCHING, PROTECTED);
            }
            vmvector_reset_vector(GLOBAL_DCONTEXT, &toflush);
        } /* else toflush is uninitialized */
    }

    /* Flushing injected bbs must be outside the scope of hotp_vul_table_lock.
     * Otherwise, flushing will deadlock.  See case 5415.
     * Though it happens so, it is safe.  The side effect of this is that
     * bbs with hot patches that have been turned off would still be active
     * till the flush below, which is ok as they were already active.  Similarly
     * hot patches that have been turned on will not work until the flush
     * happens.
     *
     * There are two flushes for hotp mode per nudge (policy or mode read) and
     * one for hotp_only mode.  For hotp, the first flush is to clean out bbs
     * with old/injected patches and is done above (nudge_action_read_policies -
     * in the case of policy nudge).
     *
     * The second flush is to remove bbs corresponding to new policies/modes,
     * i.e., bbs that were already translated but weren't injected based on
     * any old policies/modes, but are by new ones.  This is applicable to
     * both policy & mode reading.
     * Note that for case 9995 we avoided flushing perscaches that do not contain
     * the new patch points at match time, and we avoid flushing here with
     * checks in vm_area_allsynch_flush_fragments.
     */
    if (TEST(NUDGE_GENERIC(mode), nudge_action_mask) ||
        TEST(NUDGE_GENERIC(policy), nudge_action_mask)) {
        if (!DYNAMO_OPTION(hotp_only))
            hotp_remove_hot_patches(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, false, NULL);
    }
}
#    endif /* WINDOWS */

/* This is a faster lookup of hotp_vul_table, see case 8132.
 * FIXME: try to see if this can be merged with hotp_lookup_patch_addr.
 */
static bool
hotp_only_lookup_patch_addr(const app_pc pc, hotp_offset_match_t *match)
{
    hotp_offset_match_t *ppoint_desc;

    ASSERT(pc != NULL && match != NULL);
    ASSERT(DYNAMO_OPTION(hotp_only));

    /* This is always initialized at startup, so can't be NULL at this point. */
    ASSERT(hotp_patch_point_areas != NULL);

    /* Table read & injection as done together, if a module matches; even if it
     * doesn't no patching will take place when table is NULL.  Similarly, no
     * patch is left when table is emptied/cleared for update.  Thus, hotp
     * won't execute if the global table is null, which is where this lookup is
     * done. */
    ASSERT(GLOBAL_VUL_TABLE != NULL);

    ASSERT_OWN_READ_LOCK(true, &hotp_vul_table_lock);

    ppoint_desc = (hotp_offset_match_t *)vmvector_lookup(hotp_patch_point_areas, pc);
    if (ppoint_desc == NULL) {
        /* Custom data for this vector can't be NULL, so NULL means failure. */
        return false;
    } else {
        *match = *ppoint_desc;
        return true;
    }
}

/* TODO: need to use the concept of a policy activation in addition with pc
 *       to ensure that the library of the patch point is actually loaded!
 *       i think this is best if done at the time of adding/removing patch
 *       points to lookup structures
 * TODO: start using the source (i.e., pc's) dll name to verify
 *       patch point/policy with the policy's dll; similar issue as above.
 * TODO: take an argument for lock; in the bb stage call it with no lock; in
 *       the injecting stage call it with lock;
 * TODO: split up lookup into two, a vm area lookup in the outer decode loop in
 *       build_bb_ilist() & a pc lookup inside hotp_inject(); the former
 *       will be racy and will serve as a first level check; the latter is to
 *       be used only for injection purposes and won't be racy (because it will
 *       be called within the scope of GLOBAL_VUL_TABLE or pc hash lock),
 *       and won't be visible outside the hotpatch module;
 *       see is_executable_address() for sample.
 *       may need new locks for lookup data structures.
 *
 * pc lookup should match only if pc matches, dll matches, mode is not
 * off and all dlls are available (i.e., vulnerability is active).
 */
static bool
hotp_lookup_patch_addr(const app_pc pc, hotp_offset_match_t *match,
                       bool own_hot_patch_lock)
{
    bool res = false;
    hotp_offset_match_t *ppoint_desc = NULL;

    ASSERT(pc != NULL && match != NULL);
    if (pc == NULL) /* Defensively exit. */
        return false;

    /* This is called only during patch clean call injection into fcache, hence
     * not applicable to hotp_only.
     */
    ASSERT(!DYNAMO_OPTION(hotp_only));

    /* There is a remote possibility that GLOBAL_VUL_TABLE can become NULL
     * between the time the hotp lookup in bb building succeeded and the time
     * actual patch injection takes place.  This can be caused by a nudge
     * with an empty or faulty policy config file.  So, can't assert on
     * GLOBAL_VUL_TABLE not being NULL.
     */
    if (GLOBAL_VUL_TABLE == NULL) /* Nothing to lookup. */
        return res;

    /* This is always initialized at startup, so can't be NULL at this point. */
    ASSERT(hotp_patch_point_areas != NULL);

    if (!own_hot_patch_lock) /* Fix for case 5323. */
        d_r_read_lock(&hotp_vul_table_lock);

    /* Can come here with either the read lock (during instruction matching) or
     * with the write lock (during injection).
     */
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    ppoint_desc = (hotp_offset_match_t *)vmvector_lookup(hotp_patch_point_areas, pc);
    if (ppoint_desc != NULL) {

        /* If the hot patch dll for this vulnerability wasn't loaded for any
         * reason, don't even bother with pc matching; we can't execute the
         * corresponding patch as it hasn't been loaded.  Fix for case 6032.
         * TODO: when splitting up the pc lookup, this should be taken care
         * of too.
         * Assert as it is a LiveShield product bug, not dr bug; but handle it.
         */
        ASSERT(GLOBAL_VUL(ppoint_desc->vul_index).hotp_dll_base != NULL &&
               "hot patch dll loaded");
        if (GLOBAL_VUL(ppoint_desc->vul_index).hotp_dll_base == NULL)
            goto hotp_lookup_patch_addr_exit; /* lookup failed */

        /* TODO: check if vul. is ready, i.e., all modules match */
        ASSERT(GLOBAL_MODULE(ppoint_desc->vul_index, ppoint_desc->set_index,
                             ppoint_desc->module_index)
                   .matched);
        ASSERT(GLOBAL_VUL(ppoint_desc->vul_index).mode == HOTP_MODE_DETECT ||
               GLOBAL_VUL(ppoint_desc->vul_index).mode == HOTP_MODE_PROTECT);

        /* TODO: assert that the indices are within limits */
        /* TODO: vulnerability is returned without a lock for it,
         * definitely a problem because it can be updated while being used.
         * TODO: Also, need to figure out a way to return multiple matches.
         */

        res = true;        /* vmvector lookup succeeded */
        if (match != NULL) /* match can't be NULL, but be cautious. */
            *match = *ppoint_desc;

        LOG(GLOBAL, LOG_HOT_PATCHING, 1,
            "lookup for " PFX " succeeded with vulnerability #%s\n", pc,
            GLOBAL_VUL(ppoint_desc->vul_index).vul_id);
    }

hotp_lookup_patch_addr_exit:
    if (!own_hot_patch_lock)
        d_r_read_unlock(&hotp_vul_table_lock);
    return res;
}

/* Returns true if the region passed in should be patched and the module is
 * ready, i.e., loaded & matched.
 * Note: start and end define a region that is looked up a vmvector,
 * hotp_patch_point_ares.  Though our vmvector can accept end being NULL,
 * signifying no upper ceiling, it doesn't make sense of hot patch lookup - at
 * best it signifies an error somewhere.  So a NULL for end will be treated as a
 * lookup failure.
 */
bool
hotp_does_region_need_patch(const app_pc start, const app_pc end, bool own_hot_patch_lock)
{
    bool res = false;
    ASSERT(start != NULL && end != NULL);

    if (start == NULL || end == NULL)
        return false;

    /* This is called only for finding out if a bb needs a hot patch, so can't
     * be used for hotp_only.
     */
    ASSERT(!DYNAMO_OPTION(hotp_only));

    /* Called during bb building even when there is no hot patch info available. */
    if (GLOBAL_VUL_TABLE == NULL)
        return false;

    /* This is always initialized at startup, so can't be NULL at this point. */
    ASSERT(hotp_patch_point_areas != NULL);

    if (!own_hot_patch_lock) /* Fix for case 5323. */
        d_r_read_lock(&hotp_vul_table_lock);

    /* Caller must come in with lock - that is the use today.  However, this
     * doesn't need the caller to hold the hotp_vul_table_lock; can do so by
     * itself.  Imposed by fix for case 8780 - excessive holding of hotp lock.
     * need to find a better solution (FIXME).
     */
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    res = vmvector_overlap(hotp_patch_point_areas, start, end);

    if (!own_hot_patch_lock)
        d_r_read_unlock(&hotp_vul_table_lock);

    return res;
}

/* for the given ilist, it will insert the call to hot patch gateway before
 * instruction 'where' such that the hot patch corresponding to the given
 * 'policy' will be invoked with the mode specified by the policy.
 *
 * The disassembly of what is injected at each patch point is shown below.
 * Note: the disassembly below may change based on the new design.
 *
    Clean call preparation
        Note: this clean call prep shows accessing dcontext directly, i.e., as
        in thread private case.  In the shared fragments case dcontext will
        first be loaded from the TLS.  See prepare_for_clean_call() for details.

        mov    %esp -> dcontext.mcontext.mcontext.xsp
        mov    dcontext.dstack -> %esp
        pushf
        pusha  ; save app reg. state
        push   $0x00000000 %esp -> %esp (%esp)
        popf   %esp (%esp) -> %esp
        addr16 mov    %fs:0x34 -> %eax  ; last error value
        push   %eax %esp -> %esp (%esp)

    Save app state & make call to hotp_gateway
        if (SHARED_FRAGMENTS_ENABLED()) {
            mov    %fs:TLS_DCONTEXT_SLOT -> %eax
            mov    %eax(DSTACK_OFFSET) -> %eax
        } else {
            mov    dcontext.dstack -> %eax   ; locate app reg state on stack
        }
        sub    HOTP_CONTEXT_OFFSET_ON_DSTACK,%eax -> %eax

    pusha was done on dr stack, so esp is dr's; get and spill the app's esp
        if (SHARED_FRAGMENTS_ENABLED()) {
            mov    %fs:TLS_DCONTEXT_SLOT -> %ecx
            mov    %ecx(XSP_OFFSET) -> %ecx
        } else {
            mov    dcontext.mcontext.mcontext.xsp -> %ecx
        }
        mov    %ecx -> [%eax + 0xc] ; 0xc == offsetof(hotp_context_t, xsp)

 Note: Don't send func_ptr; security hazard; use indices into hotp_vul_table -
       one for vul, set, mod & ppt; this way the gateway can pick out the exact
       hotpatch offset from the table which is in read only memory; this also
       avoids the need to maintain a hash for hot patch offsets which can be
       looked up by hotp_gateway() before doing the hot patch call.

        push   false;   don't have the hotp_vul_table_lock
        push   $eax ;   app_reg_ptr
        push   ppoint_index
        push   module_index
        push   set_index
        push   vul_index
        push   num_vuls
        push   vul_table_ptr
        call   hotp_gateway()

    The hot patch could have changed esp we sent to it via app_reg_ptr.  As we
    restore esp from dcontext, save app_reg_ptr->xsp in the dcontext.
    Fix for case 5594.
        app_reg_ptr = dstack - HOTP_CONTEXT_OFFSET_ON_DSTACK
        app_esp_p = app_reg_ptr + offsetof(hotp_context_t, xsp)
    See clean call above.

        if (SHARED_FRAGMENTS_ENABLED()) {
            mov    %fs:TLS_DCONTEXT_SLOT -> %eax
            mov    %eax(DSTACK_OFFSET) -> %eax
        } else {
            mov    dcontext.dstack -> %eax
        }
        mov    (%eax-$0x14) -> %eax   ; eax = [app_esp_p]
        if (SHARED_FRAGMENTS_ENABLED()) {
            mov    %fs:TLS_DCONTEXT_SLOT -> %ecx
            mov    $eax -> %ecx(XSP_OFFSET)
        } else {
            mov    %eax -> dcontext.mcontext.mcontext.xsp
        }

    Clean call cleanup
        add    $0x1c %esp -> %esp   ; pop off the 7 args to hotp_gateway()
        pop    %esp (%esp) -> %eax %esp
        addr16 mov    %eax -> %fs:0x34  ; last error value
        popa   ; restore app reg. state
        popf
        mov    dcontext.mcontext.mcontext.xsp -> %esp

 * CAUTION: Any change to this function will affect hotp_change_control_flow().
 *          What is stored in the app/dr stack by the code generated by this
 *          routine is used and modified by hotp_change_control_flow().
 */
/* TODO: PR 226888 - make hotp bbs shared - they are enabled to be shared, but
 *          actually aren't shared yet.
 */
static int
hotp_inject_gateway_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                         const hotp_offset_match_t *match)
{
    /* TODO: use a separate stack later on; don't pollute the dr stack;
     * for now use dr stack for executing the hot patch code.
     */
    /* NOTE: app_reg pointer computation assumes certain behavior from
     * dr_prepare_for_call, i.e., first thing is all app registers are
     * pushed on to DR stack; eax is scratch at this point.
     * TODO: Add asserts here for these.
     */
#    define HOTP_CONTEXT_OFFSET_ON_DSTACK sizeof(hotp_context_t)

    /* Loads contents of dcontext at offset to reg.  For shared fragments it is
     * loaded via dc_reg; load dc into dc_reg if it isn't available (!have_dc).
     */
#    define GET_FROM_DC_OFFS_TO_REG(offset, reg, have_dc, dc_reg)                   \
        do {                                                                        \
            if (SHARED_FRAGMENTS_ENABLED()) {                                       \
                if (!have_dc)                                                       \
                    insert_get_mcontext_base(dcontext, ilist, where, dc_reg);       \
                MINSERT(ilist, where,                                               \
                        instr_create_restore_from_dc_via_reg(dcontext, dc_reg, reg, \
                                                             offset));              \
            } else {                                                                \
                MINSERT(ilist, where,                                               \
                        instr_create_restore_from_dcontext(dcontext, reg, offset)); \
            }                                                                       \
        } while (0)

    /* Using client api to avoid duplicating code. */
    /* FIXME PR 226036: set hotp_context_t pc field?  left as 0 by dr_prepare_for_call */
    dr_prepare_for_call(dcontext, ilist, where);

    /* DSTACK_OFFSET isn't within the upcontext so if it's separate our use of
     * insert_get_mcontext_base() above is incorrect. */
    ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));

    /* We push eax as a parameter to the call */
    GET_FROM_DC_OFFS_TO_REG(DSTACK_OFFSET, REG_XAX, false /* !have_dc */, REG_XBX);

    /* app reg ptr is put in eax */
    MINSERT(ilist, where,
            INSTR_CREATE_sub(dcontext, opnd_create_reg(REG_XAX),
                             OPND_CREATE_INT8(HOTP_CONTEXT_OFFSET_ON_DSTACK)));

    /* Get the app esp stored in dcontext.mcontext & spill it in the right
     * location for the hot patch code.
     */
    GET_FROM_DC_OFFS_TO_REG(XSP_OFFSET, REG_XCX, true /* have_dc */, REG_XBX);
    MINSERT(ilist, where,
            INSTR_CREATE_mov_st(dcontext,
                                OPND_CREATE_MEM32(REG_XAX, offsetof(hotp_context_t, xsp)),
                                opnd_create_reg(REG_XCX)));

    dr_insert_call(
        dcontext, ilist, where, (app_pc)&hotp_gateway, 8,
        OPND_CREATE_INTPTR(GLOBAL_VUL_TABLE), OPND_CREATE_INT32(NUM_GLOBAL_VULS),
        OPND_CREATE_INT32(match->vul_index), OPND_CREATE_INT32(match->set_index),
        OPND_CREATE_INT32(match->module_index), OPND_CREATE_INT32(match->ppoint_index),
        /* app reg ptr put in eax above */
        opnd_create_reg(REG_XAX), OPND_CREATE_INT32(false));

    /* TODO: also, for multiple patch points for one offset, gateway will have
     *       to take variable arguments, i.e., one set per patch.
     */

    /* Copy app esp from context passed to hot patch into mcontext to set up
     * for restore.  Fix for case 5594.
     */
    GET_FROM_DC_OFFS_TO_REG(DSTACK_OFFSET, REG_XAX, false /* !have_dc */, REG_XBX);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(offsetof(hotp_context_t, xsp))));
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(HOTP_CONTEXT_OFFSET_ON_DSTACK)));
    MINSERT(
        ilist, where,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                            OPND_CREATE_MEM32(REG_XAX,
                                              (int)offsetof(hotp_context_t, xsp) -
                                                  (int)HOTP_CONTEXT_OFFSET_ON_DSTACK)));

    if (SHARED_FRAGMENTS_ENABLED()) {
        MINSERT(ilist, where,
                instr_create_save_to_dc_via_reg(dcontext, REG_XBX, REG_XAX, XSP_OFFSET));
    } else {
        MINSERT(ilist, where,
                instr_create_save_to_dcontext(dcontext, REG_XAX, XSP_OFFSET));
    }

    dr_cleanup_after_call(dcontext, ilist, where, 0);

    return 1; /* TODO: why return anything here? */
}

/* If the given ilist has instructions that are targeted by any vulnerabilities,
 * this routine will identify those policies and insert code into the basic block to
 * call the hot patch code corresponding to the matching vulnerabilities.
 *
 * Note: Expand the ilist corresponding to the bb only if a hot patch needs to
 * be injected into it; taken care of by the boolean that predicates the
 * call to this function.
 */
bool
hotp_inject(dcontext_t *dcontext, instrlist_t *ilist)
{
    bool injected_hot_patch = false;
    instr_t *instr, *next;
    hotp_offset_match_t match = { -1, -1, -1, -1 };
    app_pc translation_target = NULL; /* Fix for case 5981. */
    bool caller_owns_hotp_lock = self_owns_write_lock(hotp_get_lock());

    /* This routine is for injecting hot patches into an ilist, i.e., into the
     * fcache.  Shouldn't be here for -hotp_only which patches the image.
     */
    ASSERT(!DYNAMO_OPTION(hotp_only));

    if (!caller_owns_hotp_lock)
        d_r_write_lock(&hotp_vul_table_lock); /* Fix for case 5323. */

    /* Expand the ilist corresponding to the basic block and for each
     * instruction in the ilist, check if one or more injections to
     * the gateway should be made and then do so.
     */
    instr = instrlist_first_expanded(dcontext, ilist);
    while (instr != NULL) {
        next = instr_get_next_expanded(dcontext, ilist, instr);

        /* TODO: must have way to ensure that all offsets matched for this
         * basic block are patched (not missed) and correctly too. but how?
         */

        /* TODO: hotp_lookup_patch_addr(), i.e., the second/internal lookup should
         *       be able to return multiple matching vulnerabilities/ppoints -
         *       need a new data structure for it.
         */
        /* TODO: for now this is just one vul, so no loop is used inside
         * this if; must change to handle multiple matching policies, i.e,
         * multiple injections;  that should handle precedences if offsets are
         * the same.
         */
        if (hotp_lookup_patch_addr(instr_get_raw_bits(instr), &match,
                                   true /* own hotp_vul_table_lock */)) {
            /* The mode better be either protect or detect at this point! */
            hotp_policy_mode_t mode = GLOBAL_VUL(match.vul_index).mode;
            ASSERT(mode == HOTP_MODE_DETECT || mode == HOTP_MODE_PROTECT);

            LOG(GLOBAL, LOG_HOT_PATCHING, 1, "injecting into %s at " PIFX "\n",
                GLOBAL_MODULE(match.vul_index, match.set_index, match.module_index)
                    .sig.pe_name,
                GLOBAL_PPOINT(match.vul_index, match.set_index, match.module_index,
                              match.ppoint_index)
                    .offset);
            /* TODO: assert somewhere that a given vul can't patch the same
             * offset twice in a given module.  guess this can be done at vul
             * table creation time, i.e., during startup or nudge from nodemgr.
             */
            /* The translation target for the inserted instructions is set to
             * the address of the instruction preceding the one to be patched.
             * Otherwise if an app exception happens in this bb
             * recreate_app_state_from_ilist() would fail.
             * Note: the only problem is if the first instruction in a bb is
             *       the patchee; in that case we use that address itself
             *       though the exception handler will complain about not
             *       being able to create app state.  However, it will get the
             *       right state, so we are fine in release builds.  FIXME.
             * Part of fix for case 5981.
             */
            if (translation_target == NULL)
                translation_target = instr_get_raw_bits(instr);
            instrlist_set_translation_target(ilist, translation_target);
            instrlist_set_our_mangling(ilist, true); /* PR 267260 */
            hotp_inject_gateway_call(dcontext, ilist, instr, &match);
            instrlist_set_translation_target(ilist, NULL);
            instrlist_set_our_mangling(ilist, false); /* PR 267260 */
            STATS_INC(hotp_num_inject);
            injected_hot_patch = true;
            if (mode == HOTP_MODE_DETECT)
                hotp_set_policy_status(match.vul_index, HOTP_INJECT_DETECT);
            else
                hotp_set_policy_status(match.vul_index, HOTP_INJECT_PROTECT);
        }
        translation_target = instr_get_raw_bits(instr);
        instr = next;
    }
    if (!caller_owns_hotp_lock)
        d_r_write_unlock(&hotp_vul_table_lock);
    return injected_hot_patch;
}

/* For hotp_only, a patch region shouldn't contain any jmp, call, ret or int
 * instructions that start and end within it; it is ok if a jmp, a call,
 * a ret or a int spans the entire patch region or beyond it.  This is to
 * ensure that no control flow can come into the middle of a patch region.
 * Those valid calls/jmps that can exist in the patch region should only target
 * some image address that belongs to the app, not stack or heap ==>
 * ==> FIXME case 7657: need to relax that to allow 3rd party hookers (and, app
 * itself could be targeting heap).
 * Also, the patch region shouldn't be already hooked by the core's hooks, i.e.,
 * non hotp_only core hooks.
 * TODO: strengthen this function; today it checks for what is not allowed and
 *       allows all else; make it check for what is allow too, i.e., be precise
 *       because assumptions can break with instruction extensions.
 */
static bool
hotp_only_patch_region_valid(const app_pc addr_to_hook)
{
    instr_t *inst;
    app_pc pc = addr_to_hook;
    app_pc start_pc = addr_to_hook;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool res = true;

    /* As of today hot patches can only target the .text section in a module. */
    DODEBUG({
        if (!is_in_code_section(get_module_base(addr_to_hook), addr_to_hook, NULL, NULL))
            return false;
    });

    /* Happens during hotp_init(); thread init happens afterwards so dcontext
     * isn't set up.
     */
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;

    inst = instr_create(dcontext);
    while (pc < addr_to_hook + HOTP_PATCH_REGION_SIZE) {
        instr_reset(dcontext, inst);
        pc = decode(dcontext, pc, inst);
        if (instr_is_cti(inst) || instr_is_interrupt(inst)) {
            /* cti is in patch region followed by other instructions in it.
             * Shouldn't patch this as control can come into the middle of the
             * patch region.
             */
            if ((start_pc + instr_length(dcontext, inst)) <
                (addr_to_hook + HOTP_PATCH_REGION_SIZE)) {
                LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                    "invalid hotp_only patch point"
                    " at " PFX "; there is cti inside it\n",
                    start_pc);
                res = false;
                goto hotp_only_patch_region_valid_exit;
            } else {
                /* cti is in the patch region & spans till or beyond the end
                 * of the patch region, i.e., this region is valid.
                 */
                if (instr_is_call(inst)) {
                    /* FIXME: Mangling calls in patch regions hasn't been done
                     * yet.  See case 6839.
                     */
                    LOG(GLOBAL, LOG_HOT_PATCHING, 1,
                        "Warning: not mangling "
                        "valid call in hotp_only patch region; not supported "
                        "yet, see case 6839.");
                }
                if (instr_is_call_direct(inst) || instr_is_ubr(inst) ||
                    instr_is_cbr(inst)) {
                    app_pc target = instr_get_branch_target_pc(inst);
                    /* FIXME: core doesn't handle far ctis today, see case 6962;
                     * when far ctis are handled, this assert can go.
                     */
                    ASSERT(!instr_is_far_abs_cti(inst));
#    ifdef WINDOWS
                    /* Does it overlap with any of the core's hooks?
                     * Note: native_exec_syscalls don't use the landing pad as
                     * of now, so we still have to look at the
                     * interception_buffer.  Also, the vmvector_overlap may
                     * trigger for hotp_only hooks too.  Once native_exec
                     * hooking uses landing pads change this so that the target
                     * of the landing pad is checked to see if it is in the
                     * interception buffer.  Not a big deal as both result in
                     * the hooking being aborted - just the log message
                     * changes.
                     */
                    if (is_part_of_interception(target)) {
                        LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                            "invalid hotp_only "
                            "patch point at " PFX "; it collides with a core hook\n",
                            start_pc);
                        res = false;
                        goto hotp_only_patch_region_valid_exit;
                    }
#    endif /* WINDOWS */
                    /* Overlaps with any injected hot patch?  Case 7998.
                     * This is not infrequent, some dlls like urlmon or rpcrt4
                     * have a .orpc section which results in an unmatched page
                     * protection change, like rw-, r-x, r-x; the last one
                     * results in double injection, which should be ignored.
                     * See case 9588 and 9906 where this causes a crash.
                     * Note: as all hotp_only hooks go through landing pads we
                     * don't have to check hotp_only_tramp_areas.
                     */
                    if (vmvector_overlap(landing_pad_areas, target, target + 1)) {
#    ifdef WINDOWS /* WINDOWS_VERSION_2003 doesn't exist on linux. */
                        DODEBUG({
                            const char *reason = "unknown";
                            if (hotp_globals->ldr_safe_hook_injection) {
                                reason = "due to loader safety";
                            } else if (get_os_version() >= WINDOWS_VERSION_2003) {
                                /* On 2k3 loader lock isn't held during dll
                                 * loading before executing image entry, so we
                                 * can't tell for sure. */
                                /* FIXME case 10636: what about vista? */
                                reason = "2003; may be due to loader safety";
                            } else {
                                ASSERT_NOT_REACHED(); /* Unknown reason. */
                            }
                            LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                                "Blocking double "
                                "injection at " PFX " in module at " PFX " - %s\n",
                                start_pc, get_module_base(start_pc), reason);
                        });
#    endif
                        res = false;
                        goto hotp_only_patch_region_valid_exit;
                    }

                    /* This check concludes it is a 3rd party hook if
                     * target is not in current image; target may be in another
                     * image, mapped read-only file, or heap.  The first two
                     * may not be 3rd party hook conflicts (rare).  For now, we
                     * conservatively conclude these to be hook conflicts.
                     * Note: Whether a hook targets image or heap has no
                     * bearing on how easily we can interop with it.
                     * FIXME: track patch point from mmap to point of hooking
                     * to see if it is hooked before concluding hook conflict;
                     * case 10433.
                     */
                    DODEBUG({
                        if (!is_in_any_section(get_module_base(start_pc), target, NULL,
                                               NULL)) {
                            LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                                "cti in patch region " PFX "; cti target " PFX
                                " isn't inside image "
                                "- potential 3rd-party hooker",
                                start_pc, target);
                            SYSLOG_INTERNAL_WARNING("Potential 3rd party hook "
                                                    "conflict at " PFX,
                                                    start_pc);
                        }
                    });

                    /* No app jump should be targeting the core. */
                    if (is_in_dynamo_dll(target)) {
                        LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                            "invalid hotp_only "
                            "patch point at " PFX "; cti targets dynamorio.dll!\n",
                            start_pc);
                        ASSERT_NOT_REACHED();
                        res = false;
                        goto hotp_only_patch_region_valid_exit;
                    }
                    SYSLOG_INTERNAL_WARNING_ONCE("cti found at hotp point, will chain");
                    LOG(GLOBAL, LOG_HOT_PATCHING, 2,
                        "found chainable cti at "
                        "patch point at " PFX "\n",
                        start_pc);
                }
            }
        }
        start_pc = pc;
    }
hotp_only_patch_region_valid_exit:
    instr_destroy(dcontext, inst);
    return res;
}

static void
patch_cti_tgt(byte *tgt_loc, byte *new_val, bool hot_patch)
{
#    ifdef X64
    ATOMIC_8BYTE_WRITE(tgt_loc, (int64)new_val, hot_patch);
#    else
    insert_relative_target(tgt_loc, new_val, hot_patch);
#    endif
}

/* Injects one hotp_only patch, i.e., inserts trampoline to execute a hot patch.
 *
 * FIXME: multi-thread safe injection hasn't been implemented; when that is
 *        implemented this routine will have to assert that all threads in this
 *        process have stopped.  see case 6662.
 *        Note: injections are done per module, not for the whole policy table,
 *              so there might be performance issues with stopping and resuming
 *              all threads for each module to be patched.
 *
 * FIXME: injection currently doesn't check if loader is finished with a module
 *        before injecting; needs to be done.  also, while injecting the loader
 *        shouldn't be allowed to modify the module.  see case 6662.
 *
 * FIXME: patch removal hasn't been implemented yet for hotp_only; when doing so
 *        trampoline code must be released, hook removed & image processed to
 *        set it to unmatched.  see case 6663.
 */
static void
hotp_only_inject_patch(const hotp_offset_match_t *ppoint_desc,
                       const thread_record_t **thread_table, const int num_threads)
{
    hotp_vul_t *vul;
    hotp_set_t *set;
    hotp_module_t *module;
    hotp_patch_point_t *ppoint, *cur_ppoint;
    uint ppoint_idx;
    app_pc addr_to_hook, cflow_target;
    byte *end;
    bool patched = false;

    ASSERT(DYNAMO_OPTION(hotp_only));

    /* At startup there should be no other thread than this, so thread_table
     * won't be valid.
     */
    if (num_threads != HOTP_ONLY_NUM_THREADS_AT_INIT) {
        ASSERT(ppoint_desc != NULL && thread_table != NULL);
    } else {
        ASSERT(ppoint_desc != NULL && thread_table == NULL);
    }

    /* Check if it is safe to patch, i.e., no known threads should be running
     * around (of course for the unknown thread this won't help; see
     * hotp_init() for the comment about that corner case).
     */
    ASSERT_OWN_MUTEX(true, &all_threads_synch_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    vul = &GLOBAL_VUL(ppoint_desc->vul_index);
    set = &vul->sets[ppoint_desc->set_index];
    module = &set->modules[ppoint_desc->module_index];
    cur_ppoint = &module->patch_points[ppoint_desc->ppoint_index];
    addr_to_hook = hotp_ppoint_addr(module, cur_ppoint);

    /* Can't inject a hot patch if its container dll isn't loaded.  This can
     * happen if there is a bug in the policy def file or while core was loading
     * the dll.
     */
    if (vul->hotp_dll_base == NULL) {
        SYSLOG_INTERNAL_WARNING("Hot patch dll (%s) hasn't been loaded; "
                                "aborting hotp_only injection",
                                vul->hotp_dll);
        ASSERT(false);
        return;
    }

    /* If addr_to_hook doesn't conform to the patch region definition, then
     * don't inject the patch.
     */
    if (!hotp_only_patch_region_valid(addr_to_hook)) {
        STATS_INC(hotp_only_aborted_injects);
        return;
    }

    if (cur_ppoint->trampoline != NULL) {
        /* FIXME case 9148/7657: we can have hookers who chain off our old
         * trampoline via a +rwx prot change followed by a +rx change that
         * triggers us adding new hooks without removing the old.  We go ahead
         * and leave that bug in and live with the leak for now since it works
         * out better in terms of chaining (o/w we will re-use old trampoline
         * buffers that are pointed to by the hooker's chaining, causing
         * infinite recursion, incorrect API calls, or worse).  Our hook code is
         * then called twice, but this can only happen for GBOP (o/w the hash
         * wouldn't match) which can handle duplicate checks.  We need a
         * comprehensive hooker + loader compatibility policy that minimizes
         * these types of problems (case 7657).
         */
        SYSLOG_INTERNAL_WARNING("patch point " PFX " in module %s being re-patched; "
                                "old patch leaked",
                                addr_to_hook, module->sig.pe_name);
        ASSERT(cur_ppoint->app_code_copy != NULL);
    } else {
        ASSERT(cur_ppoint->app_code_copy == NULL);
        ASSERT(cur_ppoint->tramp_exit_tgt == NULL);
    }

    /* Shouldn't be injecting anything that is isn't turned on. */
    ASSERT(vul->mode == HOTP_MODE_DETECT || vul->mode == HOTP_MODE_PROTECT);

    /* Make sure that patch region size isn't messed up. */
    ASSERT(HOTP_PATCH_REGION_SIZE == HOTP_ONLY_PATCH_REGION_SIZE);

    cur_ppoint->trampoline = (byte *)special_heap_alloc(hotp_only_tramp_heap);

    /* The patch region has been checked for validity by now, so if there are
     * other hooks in there smash them.  Also, control flow change is
     * implemented using AFTER_INTERCEPT_DYNAMIC_DECISION hooking model and
     * using AFTER_INTERCEPT_LET_GO_ALT_DYN; the only difference being that the
     * alternate target is not provided at hook time because it is unknown till
     * hooking is completed.  The alternate target is provided after hooking;
     * see below in the hook conflict resolution code.
     */
    end = hook_text(cur_ppoint->trampoline, addr_to_hook, hotp_only_gateway,
                    (void *)addr_to_hook,
                    cur_ppoint->return_addr != 0 ? AFTER_INTERCEPT_DYNAMIC_DECISION
                                                 : AFTER_INTERCEPT_LET_GO,
                    false, /* don't abort if hooked, smash it */
                    true,  /* ignore ctis; they have been checked for already */
                    &cur_ppoint->app_code_copy,
                    cur_ppoint->return_addr != 0 ? &cur_ppoint->tramp_exit_tgt : NULL);

    /* Did we hook it successfully? */
    ASSERT(*addr_to_hook == JMP_REL32_OPCODE);

    /* Trampoline code shouldn't overflow the trampoline buffer here.  By now
     * the damage is already done.  In a debug build it is ok, but in a release
     * build?  FIXME: need to make intercept_call() take a buffer length.
     */
    ASSERT((end - cur_ppoint->trampoline) <= HOTP_ONLY_TRAMPOLINE_SIZE);

    /* The copy of the hooked app code should be within the trampoline. */
    ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(cur_ppoint, cur_ppoint->app_code_copy));

    /* If the current hot patch has a control flow change address then the
     * cti that does the control flow change should be inside the trampoline.
     */
    ASSERT(cur_ppoint->return_addr == 0 ||
           HOTP_ONLY_IS_IN_TRAMPOLINE(cur_ppoint, cur_ppoint->tramp_exit_tgt));

    /* Now that the trampoline has been created to our satisfaction, add it to
     * the trampoline vector.  Note, all thread synch locks & hot patch locks
     * must be held before adding anything to the vector.
     */
    vmvector_add(hotp_only_tramp_areas, cur_ppoint->trampoline,
                 cur_ppoint->trampoline + HOTP_ONLY_TRAMPOLINE_SIZE, (void *)cur_ppoint);

    if (cur_ppoint->return_addr != 0) {
        /* A hot patch can't change control flow to go to the point where it
         * is injected; would lead to an infinite loop.
         */
        ASSERT(cur_ppoint->return_addr != cur_ppoint->offset);

        /* Go through all the patch points in this module, including the
         * current one, to see if the current patch point's
         * control-flow-change-target is in the middle of any patch region
         * that has been hooked by the core; this is to make sure that we
         * end up jumping to the copy of the app code in the trampoline as
         * opposed to jumping to the hook itself!
         */
        for (ppoint_idx = 0; ppoint_idx < module->num_patch_points; ppoint_idx++) {
            ppoint = &module->patch_points[ppoint_idx];

            /* If a ppoint hasn't been patched yet, don't do try to a resolve
             * control flow change conflict targeting it!  If a ppoint has
             * been patched, is cur_ppoint inside it?  If so, resolve conflict.
             */
            if (ppoint->trampoline != NULL &&
                HOTP_ONLY_IS_IN_PATCH_REGION(ppoint, cur_ppoint->return_addr)) {

                /* If ppoint has been injected, then its app_code_copy must
                 * point to the copy of the app code that was overwritten by
                 * the hook.
                 */
                ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(ppoint, ppoint->app_code_copy));

                /* Without multiple patch points at the same offset, a control
                 * flow change target can collide with only one patch region.
                 */
                ASSERT(!patched);

                /* Control flow transfer is going to the middle of another
                 * hot patch's patch region; one which has been injected.
                 * So fix the cur_ppoint trampoline's exit cti to target the
                 * app code copy stored in the target hot patch's trampoline as
                 * opposed to actual image.
                 */
                cflow_target =
                    ppoint->app_code_copy + (cur_ppoint->return_addr - ppoint->offset);
                ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(ppoint, cflow_target));
                patch_cti_tgt(cur_ppoint->tramp_exit_tgt, cflow_target, false);
                patched = true;

#    ifndef DEBUG
                /* Cycle through all patches even if patched for debug builds;
                 * it helps to catch multiple ppoints in the same offset.  In
                 * release builds, this is an inefficiency, so just break.
                 */
                break;
#    endif
                STATS_INC(hotp_only_cflow_collision);
            }
        }

        /* Control flow change is to a point inside the module which isn't a
         * patch point.
         */
        if (!patched) {
            cflow_target = module->base_address + cur_ppoint->return_addr;
            patch_cti_tgt(cur_ppoint->tramp_exit_tgt, cflow_target, false);
        }
    }

    /* Now, check in the current module, if any other injected patch point's
     * control-flow-change target is the current patch point's region; if so
     * make it jump to the app_code_copy in the trampoline buffer of the
     * current ppoint.
     */
    for (ppoint_idx = 0; ppoint_idx < module->num_patch_points; ppoint_idx++) {
        ppoint = &module->patch_points[ppoint_idx];
        /* No point in checking the current patch point with itself; of course
         * there will be a collision.
         */
        if (ppoint != cur_ppoint) {
            /* If ppoint hasn't been injected, nothing to do.  If it has been &
             * its return_addr collides with cur_ppoint's patch region,
             * then resolve conflict, i.e., change control flow to the copy
             * of app code inside cur_ppoint's trampoline.
             */
            if (ppoint->trampoline != NULL &&
                HOTP_ONLY_IS_IN_PATCH_REGION(cur_ppoint, ppoint->return_addr)) {

                ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(ppoint, ppoint->app_code_copy));
                ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(ppoint, ppoint->tramp_exit_tgt));

                cflow_target = cur_ppoint->app_code_copy +
                    (ppoint->return_addr - cur_ppoint->offset);
                ASSERT(HOTP_ONLY_IS_IN_TRAMPOLINE(cur_ppoint, cflow_target));
                patch_cti_tgt(ppoint->tramp_exit_tgt, cflow_target, false);
                STATS_INC(hotp_only_cflow_collision);
            }
        }
    }

#    ifdef WINDOWS
    /* If any suspended app thread is in the middle of the current patch point
     * then it needs to be relocated, i.e., its eip needs to be changed to point
     * to the correct offset in the app_code_copy in the trampoline.
     */
    if (num_threads != HOTP_ONLY_NUM_THREADS_AT_INIT) {
        int i;
        bool res;
        app_pc eip;
        CONTEXT cxt;
        thread_id_t my_tid = d_r_get_thread_id();

        for (i = 0; i < num_threads; i++) {
            /* Skip the current thread; nudge thread's Eip isn't relevant. */
            if (my_tid == thread_table[i]->id)
                continue;

            /* App thread can't be in the core holding a lock when suspended. */
            ASSERT(thread_owns_no_locks(thread_table[i]->dcontext));

            cxt.ContextFlags = CONTEXT_FULL; /* PR 264138: don't need xmm regs */
            res = thread_get_context((thread_record_t *)thread_table[i], &cxt);
            ASSERT(res);
            eip = (app_pc)cxt.CXT_XIP;

            /* 3 conditions have to be met to relocate an app thread during
             * hotp_only patching.
             * 1. thread's eip should be greater than the module base of the
             *      current ppoint; if not, negative offsets will result which
             *      can cause wrap arounds in the HOTP_ONLY_IS_IN_PATCH_REGION
             *      check which uses app_rva_t (size_t).
             * 2. if eip is at the start of the patch region, don't relocate
             *      it; just let it go to the trampoline.  Fixes a security
             *      issue: a live process which is blocked on a system call can
             *      be patched right after the syscall so that a vulnerability
             *      in the results can be caught;  if relocated, the first time,
             *      the hotpatch won't execute, just the app code copy, thereby
             *      letting the attack slip.  Rare & theoritical (because we
             *      don't allow returns inside the ppoint & because it is hard
             *      the attack has to be timed to be after the patch but
             *      before it is executed) hole.
             * 3. eip should be inside the patch region defined by cur_ppoint.
             */
            if (eip > module->base_address && eip != addr_to_hook &&
                HOTP_ONLY_IS_IN_PATCH_REGION(cur_ppoint,
                                             (app_rva_t)(eip - module->base_address))) {
                /* FIXME: this is one place that may need work if we mangle
                 *       cti_short in the patch region;  see case 6839.
                 */
                cxt.CXT_XIP =
                    (ptr_uint_t)(cur_ppoint->app_code_copy +
                                 (eip - (module->base_address + cur_ppoint->offset)));
                res = thread_set_context((thread_record_t *)thread_table[i], &cxt);
                ASSERT(res);
            }
        }
    }
#    endif /* WINDOWS */

    STATS_INC(hotp_only_num_inject);

    if (vul->mode == HOTP_MODE_DETECT)
        hotp_set_policy_status(ppoint_desc->vul_index, HOTP_INJECT_DETECT);
    else
        hotp_set_policy_status(ppoint_desc->vul_index, HOTP_INJECT_PROTECT);
}

/* Does mp safe removal of one hotp_only patch.  At the point of suspension,
 * each thread shouldn't be in all of the following: dr, hotp_dll and dr_stack.
 */
static void
hotp_only_remove_patch(dcontext_t *dcontext, const hotp_module_t *module,
                       hotp_patch_point_t *cur_ppoint)
{
    bool res;
    app_pc addr_to_unhook;
    DEBUG_DECLARE(char ppoint_content[HOTP_ONLY_PATCH_REGION_SIZE]);

    ASSERT(DYNAMO_OPTION(hotp_only));

    /* Are we at a mp-safe spot to remove the patches? */
    ASSERT_OWN_MUTEX(true, &all_threads_synch_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT_OWN_READWRITE_LOCK(true, &hotp_vul_table_lock);

    addr_to_unhook = hotp_ppoint_addr(module, cur_ppoint);

    /* Is there a hook at this place? */
    ASSERT(*addr_to_unhook == JMP_REL32_OPCODE);

    /* Is there a valid trampoline? */
    ASSERT(cur_ppoint->trampoline != NULL);
    ASSERT(cur_ppoint->app_code_copy != NULL);

    /* Save the 5 original app code bytes by getting it from the trampoline
     * (today we store those at the start of the trampoline).  Check that
     * those bytes match after unhooking.
     */
    ASSERT(HOTP_PATCH_REGION_SIZE == HOTP_ONLY_PATCH_REGION_SIZE);
    DODEBUG({ memcpy(ppoint_content, cur_ppoint->trampoline, HOTP_PATCH_REGION_SIZE); });
    unhook_text(cur_ppoint->trampoline, addr_to_unhook);
    ASSERT(!memcmp(ppoint_content, addr_to_unhook, HOTP_PATCH_REGION_SIZE));
    /* Don't release the trampoline, just leak it, i.e., don't call
     * special_heap_free.  This is how we handle the interop and detach
     * problems created by hotp & 3rd-party hooks colliding.  Not elegant or
     * memory efficient, but will handle the cases of the 3rd party reading our
     * hook before hooking and/or leaving the page marked rwx.   See cases
     * 9906, 9588, 9593, 9148 & 9157.
     * FIXME: have a better mechanism to resolve hook conflict issues; currently
     *          only a minimalist solution is in place; case 7657, case 10433.
     * FIXME: make leaking selective, i.e., don't leak all trampoline, leak
     *          only the ones that collide with 3rd party hooks - need to do
     *          some bookkeepping; not a big issue, but about 20k to 50k can be
     *          lost for each process otherwise, case 10433.
     */
    /* The ifdef mess in the next few lines is to handle the leak for case 9593. */
#    ifdef DEBUG
#        ifdef HEAP_ACCOUNTING
    hotp_only_tramp_bytes_leaked += HOTP_ONLY_TRAMPOLINE_SIZE;
#        endif
#    endif
    /* Tramp heap is freed before memory leak is checked, so cache the value.
     * hotp_only_tramp_heap_cache also tracks if there was patch removal.  */
    if (hotp_only_tramp_heap_cache == NULL) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        hotp_only_tramp_heap_cache = hotp_only_tramp_heap;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }

    /* Note: as we aren't freeing the trampoline, we have to nop it else we can
     * have bad consequences like a conflicting 3rd party hook jumping to our
     * trampoline after we detach!  See case 9593.
     * This is done by bypassing the whole trampoline and jumping to the part
     * that executes the original app code and returns to the address after
     * the hook point.  */
    insert_jmp_at_tramp_entry(dcontext, cur_ppoint->trampoline,
                              cur_ppoint->app_code_copy);

    /* Note, all thread synch locks & hot patch locks must be held before
     * removing anything from the vector.
     */
    res = vmvector_remove(hotp_only_tramp_areas, cur_ppoint->trampoline,
                          cur_ppoint->trampoline + HOTP_ONLY_TRAMPOLINE_SIZE);
    ASSERT(res);

    /* Today for hotp_only all patches in a module are applied and removed in
     * one shot, and control flow change doesn't go across modules, so there
     * is no need to patch no tramp_exit_tgt (to make sure that control flow
     * change requested is not affected) as a result of patch removal
     * (remember that all threads are suspended at outside of any hot patches
     * during the patch removal process).  If in future we allow control flow
     * change to go across modules, then we will need to go through all
     * modules & their patch points to fix the tramp_exit_tgt.
     */

    cur_ppoint->trampoline = NULL;
    cur_ppoint->tramp_exit_tgt = NULL;
    cur_ppoint->app_code_copy = NULL;
}

/* Returns true if the eip is inside any hotp_only trampoline. */
bool
hotp_only_in_tramp(const app_pc pc)
{
    /* Only after successfully stopping all threads will hotp_only_tramp_areas
     * vector will be written to.  This means that during synching when each
     * thread is suspended, where this function is called, there should be no
     * one updating the hotp_only_tramp_areas vector.
     */
    if (DYNAMO_OPTION(hotp_only)) {
        ASSERT(!WRITE_LOCK_HELD(&hotp_only_tramp_areas->lock));
        return vmvector_overlap(hotp_only_tramp_areas, pc, pc + 1);
    } else
        return false; /* check is moot if there is no trampoline */
}

/* This routine is used to remove hotp_only patches on a detach.
 * Note: Though hotp_exit gets called by detach, the removal of patches can't
 * be done there because the synch locks and thread data structures won't be
 * available at that point.  Hence the patch removal has to be done earlier
 * inside detach.
 */
void
hotp_only_detach_helper(void)
{
    /* Can't be removing hotp_only patches when hotp_only mode isn't on.  Though
     * we assert, it is safe to doing nothing in release builds and just return.
     */
    ASSERT(DYNAMO_OPTION(hotp_only));
    if (!DYNAMO_OPTION(hotp_only))
        return;

    /* Thread synch locks must be held before removing. */
    ASSERT_OWN_MUTEX(true, &all_threads_synch_lock);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);

    d_r_write_lock(&hotp_vul_table_lock);
    hotp_remove_hot_patches(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, true, NULL);
    d_r_write_unlock(&hotp_vul_table_lock);
}

/* This function is used to handle loader safe injection for hotp_only mode.
 * This is done by removing patches in regions the loader wants to write to
 * and reinjecting them afterwards; done by monitoring memory protection
 * changes made.
 *
 * FIXME case 9148: this causes problems with hookers who read prior to marking
 * +w and thus chain with our old trampoline that we are about to remove here!
 * That's why we don't call here for +rwx changes, where we live with a leak on
 * the +rx change (case 9148), which is better than hookers who mark +rw and can
 * end up with infinite recursions or wrong API calls.  We need a better
 * approach to handling both loader and hooker interop (case 7657).
 *
 * Note: all patches in a module come out, not just the page in question because
 * if a non-loader-agent changes the image and messes up our hash checks, then
 * we wouldn't be able to reinsert any into that page, leaving a multiple ppoint
 * policy in an inconsistent state or a ppoint in protect mode unprotected.
 * Another reason for pulling out all is module atomicity; set atomicity will
 * involve removing patches from other modules too!
 *
 * Note: we don't handle with some one trying to change memory protection
 * across two modules in with a single syscall; don't think it is allowed.
 */
void
hotp_only_mem_prot_change(const app_pc start, const size_t size, const bool remove,
                          const bool inject)
{
    app_pc base;
    bool needs_processing = false;
    int num_threads = 0;
    thread_record_t **thread_table = NULL;
#    ifdef WINDOWS
    DEBUG_DECLARE(bool ok;)
#    endif

    /* For hotp_only, for regular mode, vmarea tracking will flush the
     * necessary fragments.
     */
    ASSERT(DYNAMO_OPTION(hotp_only));
    ASSERT(start != NULL && size > 0);
    ASSERT((remove == true || remove == false) && (inject == true || inject == false));

    ASSERT(remove != inject); /* One and only one must be true. */
    if (remove == inject)     /* Defensively just ignore. */
        return;

    base = get_module_base(start);

    /* If base doesn't belong to any module; ignore.  We don't hot patch DGC. */
    if (base == NULL)
        return;
    /* The end of the region better be in the image! */
    ASSERT(base == get_module_base(base + size));

#    ifdef WINDOWS
    DODEBUG({
        if (get_loader_lock_owner() != d_r_get_thread_id()) {
            LOG(GLOBAL, LOG_HOT_PATCHING, 1,
                "Warning: Loader lock not held "
                "during image memory protection change; possible incompatible "
                "hooker or w2k3 loader.");
        }
    });
#    endif

    /* Inefficient check to see if this module has been matched for hot
     * patching.  hotp_process_image() is needed only when loading or
     * unloading a dll, not here, which is post module loading.
     * FIXME: Use vmvector_overlap check on loaded_module_areas after
     * integrating it with hotp.  Optimization.
     */
    hotp_process_image(base, inject ? true : false, false, true, &needs_processing, NULL,
                       0);
    if (!needs_processing) { /* Ignore if it isn't a vulnerable module. */
        LOG(THREAD_GET, LOG_HOT_PATCHING, 2,
            "hotp_only_mem_prot_change: no work to be done for base " PFX "\n", base);
        return;
    }

#    ifdef WINDOWS
    /* Ok, let's suspend all threads and do the injection/removal. */
    DEBUG_DECLARE(ok =)
    synch_with_all_threads(THREAD_SYNCH_SUSPENDED, &thread_table, &num_threads,
                           /* Case 6821: other synch-all-thread uses that
                            * only care about threads carrying fcache
                            * state can ignore us
                            */
                           THREAD_SYNCH_NO_LOCKS_NO_XFER,
                           /* if we fail to suspend a thread (e.g., privilege
                            * problems) ignore it. FIXME: retry instead? */
                           THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
    ASSERT(ok);
#    endif
    d_r_write_lock(&hotp_vul_table_lock);

    /* Using hotp_process_image to inject is inefficient because it goes
     * through the whole vul table.
     * FIXME: Optimization: write hotp_only_inject_patches() which should use
     *      hotp_patch_point_areas; use that to do the injection here.
     */
    if (inject) {
        LOG(THREAD_GET, LOG_HOT_PATCHING, 1,
            "hotp_only_mem_prot_change: injecting for base " PFX "\n", base);
        DODEBUG(hotp_globals->ldr_safe_hook_injection = true;); /* Case 7998. */
        DODEBUG(hotp_globals->ldr_safe_hook_removal = false;);  /* Case 7832. */
        hotp_process_image_helper(base, true, true, false, NULL,
                                  (const thread_record_t **)thread_table, num_threads,
                                  true, NULL);
        DODEBUG(hotp_globals->ldr_safe_hook_injection = false;);
        /* Similarly, hotp_remove_patches_from_module() is inefficient too.
         * FIXME: using loaded_module_areas in that routine.
         */
    } else if (remove) {
        LOG(THREAD_GET, LOG_HOT_PATCHING, 1,
            "hotp_only_mem_prot_change: removing for base " PFX "\n", base);
        hotp_remove_patches_from_module(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, true, base,
                                        NULL);
        /* Used to detect double removal while handling loader-safety. */
        DODEBUG(hotp_globals->ldr_safe_hook_removal = true;); /* Case 7832. */
    }
    d_r_write_unlock(&hotp_vul_table_lock);
#    ifdef WINDOWS
    end_synch_with_all_threads(thread_table, num_threads, true /*resume*/);
#    endif
}

/* This is the routine that will serve as the entry point into the core for
 * executing hot patches in the -hotp_only mode.
 * FIXME: for now, dr stack is used to execute the hot patch; later on a
 *        separate stack should be used.
 */
after_intercept_action_t
hotp_only_gateway(app_state_at_intercept_t *state)
{
    hotp_offset_match_t match;
    hotp_context_t cxt;
    app_pc hook_addr = (app_pc)state->callee_arg;
    after_intercept_action_t res = AFTER_INTERCEPT_LET_GO;

    d_r_read_lock(&hotp_vul_table_lock);
    ASSERT(DYNAMO_OPTION(hotp_only));

    /* Callee_arg contains the application eip to be patched.  It better be
     * inside a code region.
     */
    ASSERT(is_in_code_section(get_module_base(hook_addr), hook_addr, NULL, NULL));

    /* Note: for -hotp_only vulnerability table access during hot patch
     * execution is indirect, i.e., we do a lookup.  For hot patches in the
     * code cache, this information is embedded in the injected code.
     */
    if (hotp_only_lookup_patch_addr(hook_addr, &match)) {
        cxt = state->mc;
        res = hotp_gateway(GLOBAL_VUL_TABLE, NUM_GLOBAL_VULS, match.vul_index,
                           match.set_index, match.module_index, match.ppoint_index, &cxt,
                           true /* have lock */);
        /* The hot patch could have modified app state as part of the fix, so
         * copy it back.
         */
        state->mc = cxt;
    } else {
        /* If we reached here, there was a hot patch that was injected that no
         * longer matches, i.e., there is no matching definition.  Could be
         * because the mode was changed, the module got unloaded or new defs
         * came in, etc.  With mp-safe hotp_only patch injection, vulnerability/
         * policy data changes are preceded by removal of all injected patches;
         * and patch removal guarantees that no patch will be executing.
         * This means that offset lookup should always succeed in this routine.
         */
        ASSERT_NOT_REACHED();
    }
    d_r_read_unlock(&hotp_vul_table_lock);
    return res;
}

/* TODO: for multiple patch points, need to pass the number of patch points;
 *       preferably as the first argument.
 * TODO: this routine calls dr routines, i.e., switches to dr from the fcache.
 *        the switching involves protections changes (ENTER_DR_HOOK); however,
 *        the clean call mechanism used to reach here doesn't call the hook!
 *       also, there are assumptions in dr about locks being held across fcache
 *        - Derek raised these issues as some that came up during client
 *          interface design; he also raised some interesing points about
 *          generating control flow change code rather than doing it in C.
 *        - Derek also mentioned if the gateway was called from C code within
 *          dr, then things should be fine; in other words, bail out of the
 *          code cache for bb that need hot patching and execute the gateway
 *          from within dr - this is the model we will be switching to in
 *          the immediate future.
 */
static after_intercept_action_t
hotp_gateway(const hotp_vul_t *vul_tab, const uint num_vuls, const uint vul_index,
             const uint set_index, const uint module_index, const uint ppoint_index,
             hotp_context_t *app_reg_ptr, const bool own_hot_patch_lock)
{
    /* FIXME: racy access here; getting lock may be expensive;  even if that is
     *        ok, must make sure that no one will come in here and wait on the
     *        lock while a hot patch flush happens due to a nudge - deadlock.
     *        also, before executing each hot patch, it must be verfied
     *        that it still is valid because it could have been changed by a
     *        nudge;  see case 5052.
     *        looks like the whole hot patch execution should be covered by a
     *        lock - same hotp_vul_table lock or a new one?  new one I think.
     *      Derek: let flush worry about invalidation; just grab locks for
     *        table lookup or stats update.
     *      see case 5521.
     */
    hotp_policy_mode_t mode;
    hotp_type_t hotp_type;
    app_rva_t detector_offset, protector_offset;
    hotp_func_t detector_fn, protector_fn = NULL;
    hotp_exec_status_t exec_status, temp;
    hotp_offset_match_t ppoint = { vul_index, set_index, module_index, ppoint_index };
    bool dump_excpt_info, dump_error_info;
    after_intercept_action_t res = AFTER_INTERCEPT_LET_GO;

    /* FIXME: till hotp interface is expanded to send arguments to detectors
     *  and protectors, this spill is the simplest way to send/receive args.
     *  xref case 6804.
     */
    reg_t gbop_eax_spill = 0, gbop_edx_spill = 0;
    app_pc gbop_bad_addr = NULL;
    app_pc ppoint_addr; /* Fix for case 6054.  Exposed for gbop. */

    DOCHECK(1, {
        dcontext_t *dcontext = get_thread_private_dcontext();
        ASSERT_CURIOSITY(dcontext != NULL && dcontext != GLOBAL_DCONTEXT &&
                         "unknown thread");

        /* App esp should neither be on dr stack nor on d_r_initstack; see case 7058.
         * TODO: when the hot patch interface expands to having eip, assert that
         *       the eip isn't inside dr.
         */
        ASSERT(!is_on_dstack(dcontext, (byte *)app_reg_ptr->xsp) &&
               !is_on_initstack((byte *)app_reg_ptr->xsp));
    });
    ppoint_addr = hotp_ppoint_addr(
        &MODULE(vul_tab, vul_index, set_index, module_index),
        &PPOINT(vul_tab, vul_index, set_index, module_index, ppoint_index));

    /* If we change this to be invoked from d_r_dispatch, should remove this.
     * Note that we assume that hotp_only, which is invoked from
     * interception code that has its own enter hook embedded, will
     * not call any of these hooks -- else we do a double-enter here and
     * the exit via hotp_change_control_flow() results in unprotected .data!
     */
    ENTERING_DR();

    if (!own_hot_patch_lock) {
        /* Note: for regular hot patches (!hotp_only) vulnerability table
         * access during execution isn't via a lookup and all the old tables
         * are alive, so we don't need to grab the lock here; if we do an
         * indirect access then we need it.  It is left in there for safety.
         */
        d_r_read_lock(&hotp_vul_table_lock); /* Part of fix for case 5521. */
    } else
        ASSERT_OWN_READ_LOCK(true, &hotp_vul_table_lock);

    /* Check the validity of the input indices before using them.  The injection
     * routine should be generating code to send the right values.  These
     * asserts will trigger if either the injected code is messed up or a nudge
     * resulted in a vulnerability change that didn't have a corresponding
     * flush of injected bbs/traces.  One other possibility is that while in
     * this function the vulnerability table changed due to a nudge - this can't
     * happen because a nudge would result in a flush, which would wait for all
     * threads to come out of the cache, thus out of this function before
     * modifying the table.
     */
    ASSERT(vul_index >= 0 && vul_index < num_vuls);
    ASSERT(set_index >= 0 && set_index < VUL(vul_tab, vul_index).num_sets);
    ASSERT(module_index >= 0 &&
           module_index < SET(vul_tab, vul_index, set_index).num_modules);
    ASSERT(ppoint_index >= 0 &&
           ppoint_index <
               MODULE(vul_tab, vul_index, set_index, module_index).num_patch_points);

    mode = VUL(vul_tab, vul_index).mode; /* Racy; see assert comments above. */
    hotp_type = VUL(vul_tab, vul_index).type;

    /* For hotp_only mode control can't reach here if the mode is off because
     * in order to change modes all patches are removed first.  However, for
     * regular hot patching, patch removal (flushing) is done after mode change
     * and outside the scope of the hotp_vul_table_lock, so control can be in
     * the gateway with the mode set to off, but only for one execution per
     * thread because the fragment has been unlinked by the flush and
     * scheduled for deletion, so there is no entry to it.
     */
    DODEBUG({
        if (mode == HOTP_MODE_OFF) {
            ASSERT(!DYNAMO_OPTION(hotp_only));
            STATS_INC(hotp_exec_mode_off);
        } else {
            ASSERT(mode == HOTP_MODE_DETECT || mode == HOTP_MODE_PROTECT);
        }
    });

    /* The hot patch dll specified by the vulnerability better be loaded by
     * this point.  Unloaded hot patch dlls will result in the vulnerability
     * being deactivated, so we should never reach this point for such
     * vulnerabilities.
     */
    ASSERT(VUL(vul_tab, vul_index).hotp_dll_base != NULL);

    detector_offset =
        PPOINT(vul_tab, vul_index, set_index, module_index, ppoint_index).detector_fn;
    /* TODO: make the assertion range tigher by using the actual size of the
     *       text section of the hot patch dll.
     */
    ASSERT((detector_offset >= MIN_DETECTOR_OFFSET &&
            detector_offset <= MAX_DETECTOR_OFFSET) ||
           TESTALL(HOTP_TYPE_PROBE, hotp_type)); /* no detector for probes */

    protector_offset =
        PPOINT(vul_tab, vul_index, set_index, module_index, ppoint_index).protector_fn;
    ASSERT(protector_offset >= MIN_PROTECTOR_OFFSET &&
           protector_offset <= MAX_PROTECTOR_OFFSET);

    /* Compute the hot patch function addresses with the hot patch dll base. */
    detector_fn = (hotp_func_t)((ptr_uint_t)VUL(vul_tab, vul_index).hotp_dll_base +
                                detector_offset);

    protector_fn = (hotp_func_t)((ptr_uint_t)VUL(vul_tab, vul_index).hotp_dll_base +
                                 protector_offset);
    ASSERT(detector_fn != protector_fn); /* can't be the same code! */

    LOG(GLOBAL, LOG_HOT_PATCHING, 2, "Invoking detector for vulnerability %s\n",
        VUL(vul_tab, vul_index).vul_id);
    LOG(GLOBAL, LOG_HOT_PATCHING, 4, "Register state sent to detector\n");
    DOLOG(4, LOG_HOT_PATCHING, { hotp_dump_reg_state(app_reg_ptr, ppoint_addr, 4); });

    /* gbop hooks need to know current pc & will return the bad return address
     * if it is faulty.  As hotp_context_t doesn't have eip as of now, we pass
     * it via edx.  eax is used to get the return value.
     * TODO: make this a function & move it to the gbop section
     * FIXME PR 226036: hotp_context_t does have eip now, use it!
     */
    if (TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type)) {
#    ifdef GBOP
        ASSERT(DYNAMO_OPTION(gbop) && DYNAMO_OPTION(hotp_only));
#    endif
        gbop_eax_spill = APP_XAX(app_reg_ptr);
        gbop_edx_spill = APP_XDX(app_reg_ptr);
        APP_XDX(app_reg_ptr) = (reg_t)ppoint_addr;
    } else {
        /* A hot patch can be only one type. */
        ASSERT(TEST(HOTP_TYPE_HOT_PATCH, hotp_type) ^ TEST(HOTP_TYPE_PROBE, hotp_type));
    }
    /* Under the current design, a detector will always be called; a protector
     * will be called only if the mode says so.
     *
     * Forensics dumped for hot patch exceptions and errors are done so once
     * for each vulnerability; otherwise we could flood the machine.  Events
     * are logged every time; for errors, this is predicated by the patch
     * returning HOTP_EXEC_LOG_EVENT.  Cores are dumped only if the
     * dumpcore mask is set;  for exceptions, it is done every time and
     * for errors it is done once, if the mask is set (because for exceptions
     * it hard to convey the "once only" information to the exception handler).
     * We use num_{aborted,detector_error,protector_error} as booleans to
     * control this.
     * TODO: area to revisit when we work on information throttling.
     */
    dump_excpt_info = (VUL(vul_tab, vul_index).info->num_aborted == 0);
    dump_error_info = (VUL(vul_tab, vul_index).info->num_detector_error == 0);

    if (TESTALL(HOTP_TYPE_PROBE, hotp_type)) {
        /* No detectors for probes.  This status means execute the protector. */
        exec_status = HOTP_EXEC_EXPLOIT_DETECTED;
    } else {
        /* A hot patch can be only one type. */
        ASSERT(TEST(HOTP_TYPE_HOT_PATCH, hotp_type) ^
               TEST(HOTP_TYPE_GBOP_HOOK, hotp_type));

        exec_status = hotp_execute_patch(detector_fn, app_reg_ptr, mode, dump_excpt_info,
                                         dump_error_info);

        LOG(GLOBAL, LOG_HOT_PATCHING, 3, "Detector finished for vulnerability %s\n",
            VUL(vul_tab, vul_index).vul_id);
        STATS_INC(hotp_num_det_exec);
        hotp_update_vul_stats(exec_status, vul_index);
    }
    temp = exec_status & ~HOTP_EXEC_LOG_EVENT;
    ASSERT(temp == HOTP_EXEC_EXPLOIT_DETECTED || temp == HOTP_EXEC_EXPLOIT_NOT_DETECTED ||
           temp == HOTP_EXEC_DETECTOR_ERROR || temp == HOTP_EXEC_ABORTED);

    /* Restore eax & edx spilled for executing gbop remediators.
     * TODO: make this a function & move it to the gbop section
     */
    if (TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type)) {
#    ifdef GBOP
        ASSERT(DYNAMO_OPTION(gbop) && DYNAMO_OPTION(hotp_only));
#    endif
        if (temp == HOTP_EXEC_EXPLOIT_DETECTED)
            gbop_bad_addr = (app_pc)APP_XAX(app_reg_ptr);
        APP_XAX(app_reg_ptr) = gbop_eax_spill;
        APP_XDX(app_reg_ptr) = gbop_edx_spill;
    }

    if (temp == HOTP_EXEC_ABORTED || temp == HOTP_EXEC_DETECTOR_ERROR)
        goto hotp_gateway_ret;

    /* If the patch asked for violation notification, do so only if its mode
     * is set to detect.  For protect mode, the protector will report the
     * violation if asked, so don't worry about it.  The exception here is for
     * gbop hooks, which currently always run in protect mode, which need to
     * honor -detect_mode, in which case we report the violation right here.
     * Note: In this case, the gbop protector won't get executed even though
     * its mode is set to protect.  See below.
     */
    if (TEST(exec_status, HOTP_EXEC_LOG_EVENT) &&
        ((mode == HOTP_MODE_DETECT) ||
         (TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type)
#    ifdef PROGRAM_SHEPHERDING
          && DYNAMO_OPTION(detect_mode)
#    endif
              ))) {
        hotp_event_notify(exec_status, false, &ppoint, gbop_bad_addr, app_reg_ptr);
    }

    /* The protector should be invoked only if an exploit was detected and the
     * mode was set to protect.
     * In the case of gbop hooks, -detect_mode shouldn't invoke the protector.
     * Note: Unlike hot patches, gbop hooks must conform to core reporting and
     * remediation options.  As of today hot patch actions are specified by the
     * patch {writer}.  There are plans to have an override, case 8095.
     */
    if (TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type)
#    ifdef PROGRAM_SHEPHERDING
        && DYNAMO_OPTION(detect_mode)
#    endif
    )
        goto hotp_gateway_ret;

    if (mode == HOTP_MODE_PROTECT && temp == HOTP_EXEC_EXPLOIT_DETECTED) {

        LOG(GLOBAL, LOG_HOT_PATCHING, 2, "Invoking protector for vulnerability %s\n",
            VUL(vul_tab, vul_index).vul_id);
        LOG(GLOBAL, LOG_HOT_PATCHING, 6, "Register state sent to protector\n");
        DOLOG(6, LOG_HOT_PATCHING, { hotp_dump_reg_state(app_reg_ptr, ppoint_addr, 6); });

        /* See detector execution comments above for details about hot patch
         * error & exception handling.
         * TODO: area to revisit when we work on information throttling.
         */
        dump_error_info = (VUL(vul_tab, vul_index).info->num_protector_error == 0);
        exec_status = hotp_execute_patch(protector_fn, app_reg_ptr, mode, dump_excpt_info,
                                         dump_error_info);

        /* TODO: probes have no return codes defined. PR 229881. */
        if (TESTALL(HOTP_TYPE_PROBE, hotp_type)) {
            exec_status = HOTP_EXEC_EXPLOIT_PROTECTED;
        }

        temp = exec_status & ~HOTP_EXEC_LOG_EVENT;
        ASSERT(temp == HOTP_EXEC_EXPLOIT_PROTECTED ||
               temp == HOTP_EXEC_EXPLOIT_NOT_PROTECTED ||
               temp == HOTP_EXEC_EXPLOIT_KILL_THREAD ||
               temp == HOTP_EXEC_EXPLOIT_KILL_PROCESS ||
               temp == HOTP_EXEC_EXPLOIT_RAISE_EXCEPTION ||
               temp == HOTP_EXEC_CHANGE_CONTROL_FLOW ||
               temp == HOTP_EXEC_PROTECTOR_ERROR || temp == HOTP_EXEC_ABORTED);

        LOG(GLOBAL, LOG_HOT_PATCHING, 4, "Register state after protector\n");
        DOLOG(4, LOG_HOT_PATCHING, { hotp_dump_reg_state(app_reg_ptr, ppoint_addr, 4); });
        LOG(GLOBAL, LOG_HOT_PATCHING, 3, "Protector finished for vulnerability %s\n",
            VUL(vul_tab, vul_index).vul_id);
        STATS_INC(hotp_num_prot_exec);
        hotp_update_vul_stats(exec_status, vul_index);

        if (temp == HOTP_EXEC_ABORTED || temp == HOTP_EXEC_PROTECTOR_ERROR)
            goto hotp_gateway_ret;

        /* Which one comes first, esp with kill/raise & cflow change? */
        /* Raise an event only if requested by the protector. */
        if (TEST(exec_status, HOTP_EXEC_LOG_EVENT)) {
            hotp_event_notify(exec_status, true, &ppoint, gbop_bad_addr, app_reg_ptr);
        }
        if (TEST(exec_status, HOTP_EXEC_CHANGE_CONTROL_FLOW)) {

            app_rva_t return_rva =
                PPOINT(vul_tab, vul_index, set_index, module_index, ppoint_index)
                    .return_addr;
            app_pc module_base =
                MODULE(vul_tab, vul_index, set_index, module_index).base_address;

            /* gbop hooks shouldn't be changing control flow. */
            ASSERT(!TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type));

            /* hotp_only control flow change is implemented using the alt exit
             * feature in our intercept_call() mechanism.
             */
            if (DYNAMO_OPTION(hotp_only)) {
                res = AFTER_INTERCEPT_LET_GO_ALT_DYN;
                goto hotp_gateway_ret;
            }

            /* If control flow change is requested by a protector, then the
             * offset to which the control should go to shouldn't be zero
             * and the dll should be in memory!
             */
            ASSERT(return_rva != 0 && module_base != NULL);
            LOG(GLOBAL, LOG_HOT_PATCHING, 1,
                "Control flow change requested by "
                "vulnerability %s\n",
                VUL(vul_tab, vul_index).vul_id);

            /* Release the lock because control flow change won't return. */
            d_r_read_unlock(&hotp_vul_table_lock); /* Part of fix for case 5521. */
            hotp_change_control_flow(app_reg_ptr, module_base + return_rva);
            ASSERT_NOT_REACHED();
        }
    }

hotp_gateway_ret:
    if (!own_hot_patch_lock)
        d_r_read_unlock(&hotp_vul_table_lock); /* Part of fix for case 5521. */

    /* if we change this to be invoked from d_r_dispatch, should remove this */
    EXITING_DR();
    return res;
}

/* This routine will execute the given hot patch (either detector or protector)
 * and return the appropriate execution status.  If the hot patch causes an
 * exception, it will be terminated and the exception handler will automatically
 * return to this function and this function will return with status
 * HOTP_EXEC_ABORTED.
 * If a hot patch exception occurs
 *     - it dumps a forenscis file if asked for (using dump_excpt_info)
 *     - the exception handler dumps a core if the mask is set and logs an event
 * If a hot patch returns HOTP_{DETECTOR,PROTECTOR}_ERROR,
 *     - it dumps a forenscis file if asked for (using dump_error_info)
 *     - it dumps a core if asked for and the mask is set
 *     - it logs an event if the patch returns HOTP_EXEC_LOG_EVENT too.
 * Hot patch exceptions and errors are treated similarly because they both
 * point to a faulty hot patch.  The only differences are in the string of the
 * event logged and the cause-string of the forensics file.
 *
 * Note: this routine uses a shadow app reg state to recover from a hot patch
 *       exception cleanly.
 * FIXME: using setjmp & longjmp can cause problems if the compiler decides
 *        to reuse unused args/locals; should probably use volatile for those.
 */
static hotp_exec_status_t
hotp_execute_patch(hotp_func_t hotp_fn_ptr, hotp_context_t *hotp_cxt,
                   hotp_policy_mode_t mode, bool dump_excpt_info, bool dump_error_info)
{
    hotp_exec_status_t exec_status, exec_status_only;
    hotp_context_t local_cxt;
    dcontext_t *dcontext = get_thread_private_dcontext();
    dr_where_am_i_t wherewasi;

    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    if (dcontext == NULL || /* case 9385: unknown thread */
        dcontext == GLOBAL_DCONTEXT /* just bug */) {
        SYSLOG_INTERNAL_WARNING("hotp_execute_patch: unknown thread");
        return HOTP_EXEC_ABORTED;
    }

    /* For hot patching with fcache, today, hot patches are executed only from
     * within the fcache.  For hotp_only, there is no fcache; hot patches are
     * executed directly when they are DR_WHERE_APP.

Question for reviewer: for hotp_only, when control comes to the gateway, should
    whereami be changed to something other than DR_WHERE_APP because we are technically
    in the core now; if so, would it be DR_WHERE_TRAMPOLINE?
     */
    ASSERT(dcontext->whereami == DR_WHERE_FCACHE ||
           (dcontext->whereami == DR_WHERE_APP && DYNAMO_OPTION(hotp_only)));
    wherewasi = dcontext->whereami;

    /* In case the hot patch causes an exception, the context may be in an
     * inconsistent state.  To prevent that make a copy of the app's context
     * and pass the copy to the hot patch.
     * Note: nothing is done for partial memory writes.  TODO: how to fix this?
     */
    memcpy(&local_cxt, hotp_cxt, sizeof(hotp_context_t));
    dcontext->whereami = DR_WHERE_HOTPATCH;

    if (DR_SETJMP(&dcontext->hotp_excpt_state) == 0) { /* TRY block */
        exec_status = (*hotp_fn_ptr)(&local_cxt);
        exec_status_only = exec_status & ~HOTP_EXEC_LOG_EVENT;

        /* Successful execution can't return exception code. */
        ASSERT(exec_status_only != HOTP_EXEC_ABORTED);

        if (mode == HOTP_MODE_DETECT) {
            /* The detector shouldn't have modified register state.
             * Note: currently there is no way to find out if the memory state
             *       was modified.
             */
            ASSERT(memcmp(hotp_cxt, &local_cxt, sizeof(hotp_context_t)) == 0);
        } else if (mode == HOTP_MODE_PROTECT) {
            /* Copy back local context which may have been modified by the
             * protector back to the context passed in, i.e., apply the
             * changes enforced by the hot patch.  Note: this is applicable
             * only for registers not memory
             */
            memcpy(hotp_cxt, &local_cxt, sizeof(hotp_context_t));
        }

        if ((TEST(HOTP_EXEC_DETECTOR_ERROR, exec_status_only) ||
             (TEST(HOTP_EXEC_PROTECTOR_ERROR, exec_status_only)))) {
            const char *msg = TEST(HOTP_EXEC_DETECTOR_ERROR, exec_status_only)
                ? "Hot patch detector error"
                : "Hot patch protector error";
            if (dump_error_info) {
                if (TEST(DUMPCORE_HOTP_FAILURE, DYNAMO_OPTION(dumpcore_mask)))
                    os_dump_core(msg);
            }
            if (TEST(HOTP_EXEC_LOG_EVENT, exec_status)) {
                SYSLOG_CUSTOM_NOTIFY(SYSLOG_ERROR, MSG_HOT_PATCH_FAILURE, 3,
                                     "Hot patch error, continuing.",
                                     get_application_name(), get_application_pid(),
                                     "<none>");
            }
#    ifdef PROGRAM_SHEPHERDING
            if (dump_error_info) {
                report_diagnostics(msg, NULL, HOT_PATCH_FAILURE);
            }
#    endif
        }
    } else { /* EXCEPT block */
        /* Hot patch crashed! */
        if (dump_excpt_info) {
            /* Usually logging the event, dumping core and forensics are done
             * together.  In this case the first two are done in the exception
             * handler because that is where the exception specific information
             * is available.  Forensics are dumped here because this is where
             * the failing vulnerability's information is available.  Trying to
             * do all in one place would require too many pieces of information
             * being passed around.
             */
            /* TODO: no hot patch exception specific information is dumped
             *       in the forensics files today;  need to do so.
             */
            /* TODO: title should say detector or protector exception. */
#    ifdef PROGRAM_SHEPHERDING
            report_diagnostics("Hot patch exception", NULL, HOT_PATCH_FAILURE);
#    endif
        }
        exec_status = HOTP_EXEC_ABORTED;
    }
    dcontext->whereami = wherewasi;

    /* Reset hotp_excpt_state to unused.  This will be used in
     * create_callback_dcontext() to catch potential callbacks, which might
     * lead to nested hot patch exceptions, that result due to system calls
     * mades from a hot patch.  Hot patches shouldn't make system calls.
     */
    DODEBUG(memset(&dcontext->hotp_excpt_state, -1, sizeof(dr_jmp_buf_t)););

    return exec_status;
}

/* This routine plugs the hot patch violation event into the core's existing
 * reporting mechanism.  A new threat id (.H) will be generated for hot patch
 * violations and the event log will mention whether the violation was detected
 * or protected.  -report_max will apply these violations.  However,
 * -detect_mode and other termination options like -kill_thread, etc. won't be.
 *
 * TODO: currently, -kill_thread and such will apply to hot patches.  They
 * must be decoupled.  Not done currently because a clean way of doing it is
 * out of the scope of blowfish beta.
 */
static void
hotp_event_notify(hotp_exec_status_t exec_status, bool protected,
                  const hotp_offset_match_t *inject_point, const app_pc bad_addr,
                  const hotp_context_t *hotp_cxt)
{
#    ifdef PROGRAM_SHEPHERDING
    const char *threat_id = NULL;
    hotp_type_t hotp_type = GLOBAL_VUL(inject_point->vul_index).type;
    action_type_t action;
    app_pc faulting_addr = NULL, inject_addr, old_next_tag;
    security_violation_t violation_type = INVALID_VIOLATION, res;
    dcontext_t *dcontext = get_thread_private_dcontext();
    fragment_t src_frag = { 0 }, *old_last_frag;
    priv_mcontext_t old_mc;

    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    ASSERT(inject_point != NULL && hotp_cxt != NULL);

    /* action mapping */
    switch (exec_status & ~HOTP_EXEC_LOG_EVENT) {
    case HOTP_EXEC_EXPLOIT_KILL_THREAD: action = ACTION_TERMINATE_THREAD; break;
    case HOTP_EXEC_EXPLOIT_KILL_PROCESS: action = ACTION_TERMINATE_PROCESS; break;
    case HOTP_EXEC_EXPLOIT_RAISE_EXCEPTION: action = ACTION_THROW_EXCEPTION; break;
    default: action = ACTION_CONTINUE;
    };
    inject_addr = GLOBAL_MODULE(inject_point->vul_index, inject_point->set_index,
                                inject_point->module_index)
                      .base_address +
        GLOBAL_PPOINT(inject_point->vul_index, inject_point->set_index,
                      inject_point->module_index, inject_point->ppoint_index)
            .offset;

    /* Determine the faulting address, violation type and threat id. */
    if (TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type)) { /* gbop hook type */
#        ifdef GBOP
        /* FIXME: share reporting code with gbop_validate_and_act() - have
         *  one reporting interface for gbop.  Case 8096.  Changes here or
         *  in gbop_validate_and_act() should be kept in sync.
         */
        /* Even though many gbop hooks are implemented with hotp_only interface
         * gbop violations are treated separately.
         */
        ASSERT(DYNAMO_OPTION(hotp_only)); /* no gbopping in code cache mode */

        /* NOTE: For gbop, the source is actually the hook address and the
         * target is the failing address, not vice versa.
         */
        faulting_addr = bad_addr;
        violation_type = GBOP_SOURCE_VIOLATION;

        /* gbop remediations are decided in security_violation(), so set it to
         * the expected default here otherwise security_violation would assert.
         */
        action = ACTION_TERMINATE_PROCESS;
#        endif
    } else { /* hot patch type */
        ASSERT(TESTALL(HOTP_TYPE_HOT_PATCH, hotp_type));
        ASSERT(bad_addr == NULL);
        faulting_addr = inject_addr;
        if (protected) {
            violation_type = HOT_PATCH_PROTECTOR_VIOLATION;
        } else {
            violation_type = HOT_PATCH_DETECTOR_VIOLATION;
        }
        threat_id = GLOBAL_VUL(inject_point->vul_index).policy_id;
        ASSERT(threat_id != NULL);
    }

    /* Save the last fragment, next tag & register state, set them up with the
     * right values, report and restore afterwards.
     * Note: for hotp_only, src & tgt are the same; for gbop see comment above.
     */
    hotp_spill_before_notify(dcontext, &old_last_frag, &src_frag, inject_addr,
                             &old_next_tag, faulting_addr, &old_mc, (void *)hotp_cxt,
                             CXT_TYPE_HOT_PATCH);

    res = security_violation_internal(dcontext, faulting_addr, violation_type,
                                      OPTION_REPORT | OPTION_BLOCK, threat_id, action,
                                      &hotp_vul_table_lock);

    /* Some sanity checks before we go on our merry way.  BTW, couldn't use
     * DODEBUG because gcc complained about using the #ifdef GBOP inside a
     * macro; not an issue; the compiler should optimize out the whole if-block.
     */
    if (res == ALLOWING_BAD) {
        /* Threat exemptions are only for gbop hooks, they don't make sense
         * for hot patches - if you don't want a hot patch's event, just
         * turn it off.
         */
        ASSERT(TESTALL(HOTP_TYPE_GBOP_HOOK, hotp_type));
        ASSERT(!TESTALL(HOTP_TYPE_HOT_PATCH, hotp_type));
    } else if (res == HOT_PATCH_DETECTOR_VIOLATION ||
               res == HOT_PATCH_PROTECTOR_VIOLATION) {
        /* Can return only to continue. */
        ASSERT(action == ACTION_CONTINUE);
    } else {
#        ifdef GBOP
        ASSERT(res == GBOP_SOURCE_VIOLATION);
        ASSERT(action == ACTION_CONTINUE || DYNAMO_OPTION(detect_mode));
#        endif
    }

    hotp_restore_after_notify(dcontext, old_last_frag, old_next_tag, &old_mc);

    /* Can't leave this function without holding the hotp lock! */
    ASSERT_OWN_READ_LOCK(true, &hotp_vul_table_lock);
#    endif
}

/* This is a hack to make hotp use our existing security violation reporting
 * mechanism, which relies on fragments & tags to report violations & generate
 * forensics.  Case 8079 talks about cleaning up the reporting interface.
 */
void
hotp_spill_before_notify(dcontext_t *dcontext, fragment_t **frag_spill /* OUT */,
                         fragment_t *new_frag, const app_pc new_frag_tag,
                         app_pc *new_tag_spill /* OUT */, const app_pc new_next_tag,
                         priv_mcontext_t *cxt_spill /* OUT */, const void *new_cxt,
                         cxt_type_t cxt_type)
{
    priv_mcontext_t *mc;
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    ASSERT(frag_spill != NULL && new_frag != NULL && new_frag_tag != NULL);
    ASSERT(new_tag_spill != NULL && new_next_tag != NULL);
    ASSERT(cxt_spill != NULL && new_cxt != NULL);
    ASSERT(cxt_type == CXT_TYPE_HOT_PATCH || cxt_type == CXT_TYPE_CORE_HOOK);

    *frag_spill = dcontext->last_fragment;
    *new_tag_spill = dcontext->next_tag;

    new_frag->tag = new_frag_tag;
    dcontext->last_fragment = (fragment_t *)new_frag;
    dcontext->next_tag = new_next_tag;

    /* For hotp_only the last_fragment should be linkstub_empty_fragment,
     * which is static in link.c
     *
     * next_tag can be set to BACK_TO_NATIVE_AFTER_SYSCALL, so can't
     * easily assert on that.
     */
    ASSERT(!DYNAMO_OPTION(hotp_only) ||
           ((*frag_spill)->tag == NULL && (*frag_spill)->flags == FRAG_FAKE));

    /* Saving & swapping contexts - this is needed to produce the
     * correct machine context for forensics; there can be two types, viz.,
     * hotp_context_t if called from hotp_event_notify() and
     * app_state_at_intercept_t if called from gbop_validate_and_act().
     */
    mc = get_mcontext(dcontext);
    ASSERT(mc != NULL);
    memcpy(cxt_spill, mc, sizeof(*cxt_spill));
    if (cxt_type == CXT_TYPE_HOT_PATCH) {
        hotp_context_t *new = (hotp_context_t *)new_cxt;
        *mc = *new;
        /* FIXME PR 226036: use hotp_context_t.xip */
        mc->pc = NULL; /* pc reported in source, so NULL here is ok. */
    } else if (cxt_type == CXT_TYPE_CORE_HOOK) {
        app_state_at_intercept_t *new = (app_state_at_intercept_t *)new_cxt;
        *mc = new->mc;
        /* FIXME PR 226036: use hotp_context_t.xip */
        mc->pc = NULL; /* pc reported in source, so NULL here is ok. */
    } else
        ASSERT_NOT_REACHED();
}

/* Restore dcontext last_fragment & next_tag after reporting the violation. */
/* FIXME: old_cxt is unused; see case 8099 about dumping context. */
void
hotp_restore_after_notify(dcontext_t *dcontext, const fragment_t *old_frag,
                          const app_pc old_next_tag, const priv_mcontext_t *old_cxt)
{
    priv_mcontext_t *mc;
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    ASSERT(old_cxt != NULL);

    dcontext->last_fragment = (fragment_t *)old_frag;
    dcontext->next_tag = old_next_tag;

    mc = get_mcontext(dcontext);
    ASSERT(mc != NULL);
    memcpy(mc, old_cxt, sizeof(*old_cxt));
}

#    if defined(DEBUG) && defined(INTERNAL)
/* FIXME PR 226036: eip is now part of hotp_context_t */
static void
hotp_dump_reg_state(const hotp_context_t *reg_state, const app_pc eip,
                    const uint loglevel)
{
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "eax: " PFX "\n", APP_XAX(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "ecx: " PFX "\n", APP_XCX(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "edx: " PFX "\n", APP_XDX(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "ebx: " PFX "\n", APP_XBX(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "esp: " PFX "\n", APP_XSP(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "ebp: " PFX "\n", APP_XBP(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "esi: " PFX "\n", APP_XSI(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "edi: " PFX "\n", APP_XDI(reg_state));
    LOG(GLOBAL, LOG_HOT_PATCHING, loglevel, "eip: " PFX "\n", eip);
}
#    endif

static void
hotp_update_vul_stats(const hotp_exec_status_t exec_status, const uint vul_index)
{
    hotp_exec_status_t temp = exec_status & ~HOTP_EXEC_LOG_EVENT;

    ASSERT(temp == HOTP_EXEC_EXPLOIT_DETECTED || temp == HOTP_EXEC_EXPLOIT_NOT_DETECTED ||
           temp == HOTP_EXEC_DETECTOR_ERROR || temp == HOTP_EXEC_EXPLOIT_PROTECTED ||
           temp == HOTP_EXEC_EXPLOIT_NOT_PROTECTED ||
           temp == HOTP_EXEC_EXPLOIT_KILL_THREAD ||
           temp == HOTP_EXEC_EXPLOIT_KILL_PROCESS ||
           temp == HOTP_EXEC_EXPLOIT_RAISE_EXCEPTION ||
           temp == HOTP_EXEC_CHANGE_CONTROL_FLOW || temp == HOTP_EXEC_PROTECTOR_ERROR ||
           temp == HOTP_EXEC_ABORTED);

    /* FIXME: Grabbing the hot patch lock here to update stats will deadlock
     *        if a nudge is waiting for this thread to get out.  If a lock
     *        isn't grabbed, then the stats may be slightly inaccurate if 2
     *        threads update the same stat for a given vulnerability at
     *        the same time; odds are low and inaccurate stats aren't a problem.
     *        We aren't trying to provide accurate stats; besides stats for a
     *        vulnerability for all process in all nodes using it is vague
     *        data anyway.
     *        if VUL_STAT_INC becomes atomic, we won't need a lock here.
     * FIXME: Vlad suggested creating a stats lock; good idea;
     */
    switch (temp) {
    case HOTP_EXEC_EXPLOIT_DETECTED: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_detected);
        break;
    }
    case HOTP_EXEC_EXPLOIT_NOT_DETECTED: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_not_detected);
        break;
    }
    case HOTP_EXEC_DETECTOR_ERROR: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_detector_error);
        break;
    }
    case HOTP_EXEC_EXPLOIT_PROTECTED: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_protected);
        break;
    }
    case HOTP_EXEC_EXPLOIT_NOT_PROTECTED: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_not_protected);
        break;
    }
    case HOTP_EXEC_EXPLOIT_KILL_THREAD: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_kill_thread);
        break;
    }
    case HOTP_EXEC_EXPLOIT_KILL_PROCESS: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_kill_process);
        break;
    }
    case HOTP_EXEC_EXPLOIT_RAISE_EXCEPTION: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_raise_exception);
        break;
    }
    case HOTP_EXEC_CHANGE_CONTROL_FLOW: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_change_control_flow);
        break;
    }
    case HOTP_EXEC_PROTECTOR_ERROR: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_protector_error);
        break;
    }
    case HOTP_EXEC_ABORTED: {
        VUL_STAT_INC(GLOBAL_VUL(vul_index).info->num_aborted);
        break;
    }
    default: ASSERT_NOT_REACHED();
    }
}

/* Note: 1. This function will not return, unless there is an error.
 *       2. The number of patch points must be passed to this function to handle
 *          control flow changes with multiple patches at the same offset.
 *
 * CAUTION: Any change to the code generated by hotp_inject_gateway_call() (and,
 *          thus, prepare_for_clean_call()), will affect how the app. state
 *          is spilled on the dr stack.  This function uses that app. state,
 *          hence, relies on that order being constant.
 *          TODO: How to link an assert to these two, so that any change is
 *                caught immediately?
 *
 * TODO: show stack diagrams otherwise it is going to be messy
 */
/* These constants refer to the offset of eflags and errno that are saved on
 * the stack as part of the clean call.  The offsets are relative to the
 * location of the pushed register state, i.e., esp after pusha in the clean
 * call sequence.  Any change to prepare_for_clean_call() will affect this.
 */
#    define CLEAN_CALL_XFLAGS_OFFSET 1
#    define CLEAN_CALL_ERRNO_OFFSET 2
static void
hotp_change_control_flow(const hotp_context_t *app_reg_ptr, const app_pc target)
{
    dcontext_t *dcontext;
    priv_mcontext_t mc = *app_reg_ptr;

    /* TODO: Eventually, must assert that target is in some module. */
    ASSERT(app_reg_ptr != NULL && target != NULL);

    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);

    dcontext->next_tag = target; /* Set up actual control flow change. */
    dcontext->whereami = DR_WHERE_FCACHE;
    /* FIXME: should determine the actual fragment exiting from */
    set_last_exit(dcontext, (linkstub_t *)get_hot_patch_linkstub());

    STATS_INC(hotp_num_cflow_change);
    LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Changing control flow to " PFX "\n", target);
    transfer_to_dispatch(dcontext, &mc, true /*full_DR_state*/);
    ASSERT_NOT_REACHED();
}

/* Prints hotpatch vulnerability table information to forensics file in xml
 * format.
 */
void
hotp_print_diagnostics(file_t diagnostics_file)
{
    uint vul, set, module, pp, hash;

    if (GLOBAL_VUL_TABLE == NULL) {
        print_file(diagnostics_file,
                   "<hotpatching-information>\n"
                   "Hotpatch vulnerability table is NULL\n"
                   "</hotpatching-information>\n");
        return;
    }

    print_file(diagnostics_file,
               "<hotpatching-information>\n"
               "<vulnerability-table num-vulnerabilities=\"%u\">\n",
               NUM_GLOBAL_VULS);
    for (vul = 0; vul < NUM_GLOBAL_VULS; vul++) {
        hotp_inject_status_t inject_status;

        print_file(diagnostics_file, "  <vulnerability id=\"%s\" num-sets=\"%u\">\n",
                   GLOBAL_VUL(vul).vul_id, GLOBAL_VUL(vul).num_sets);
        print_file(diagnostics_file,
                   "    <policy id=\"%s\" mode=\"%d\" version=\"%u\"/>\n",
                   GLOBAL_VUL(vul).policy_id, GLOBAL_VUL(vul).mode,
                   GLOBAL_VUL(vul).policy_version);

        /* For vulnerabilities that haven't been used, print only the
         * {vul,policy}_id and policy_version; helps to prevent clustter in the
         * forensics file.  Using the ids & version we can get the hot patch
         * definition from our packages/code in house, so not dumping them
         * doesn't hamper diagnosis.  Case 8549.
         */
        inject_status = *(GLOBAL_VUL(vul).info->inject_status);
        if (inject_status == HOTP_INJECT_NO_MATCH || inject_status == HOTP_INJECT_OFF) {
            print_file(diagnostics_file, "  </vulnerability>\n");
            continue;
        }

        print_file(diagnostics_file,
                   "    <hotpatch-dll name=\"%s\" base=\"" PFX "\" hash=\"%s\"/>\n",
                   GLOBAL_VUL(vul).hotp_dll, GLOBAL_VUL(vul).hotp_dll_base,
                   GLOBAL_VUL(vul).hotp_dll_hash);
        for (set = 0; set < GLOBAL_VUL(vul).num_sets; set++) {
            bool print_sets = false;
            for (module = 0; module < GLOBAL_SET(vul, set).num_modules; module++) {
                /* If a module isn't matched, then it hasn't been used, don't
                 * dump it; case 8549.
                 */
                if (!GLOBAL_MODULE(vul, set, module).matched)
                    continue;

                if (!print_sets) { /* Print set title if needed, case 8549. */
                    print_file(diagnostics_file, "    <set num-modules=\"%u\">\n",
                               GLOBAL_SET(vul, set).num_modules);
                    print_sets = true;
                }

                print_file(diagnostics_file,
                           "      <module pe_name=\"%s\" pe_checksum=\"0x%x\" "
                           "pe_timestamp=\"0x%x\" pe_image_size=\"" PIFX "\" "
                           "pe_code_size=\"" PIFX "\" "
                           "pe_file_version=\"0x" HEX64_FORMAT_STRING "\" "
                           "num-hashes=\"%u\" num-patch-points=\"%u\">\n",
                           GLOBAL_SIG(vul, set, module).pe_name,
                           GLOBAL_SIG(vul, set, module).pe_checksum,
                           GLOBAL_SIG(vul, set, module).pe_timestamp,
                           GLOBAL_SIG(vul, set, module).pe_image_size,
                           GLOBAL_SIG(vul, set, module).pe_code_size,
                           GLOBAL_SIG(vul, set, module).pe_file_version,
                           GLOBAL_MODULE(vul, set, module).num_patch_point_hashes,
                           GLOBAL_MODULE(vul, set, module).num_patch_points);
                for (hash = 0;
                     hash < GLOBAL_MODULE(vul, set, module).num_patch_point_hashes;
                     hash++) {
                    print_file(diagnostics_file,
                               "        <hash start=\"" PFX "\" length=\"0x%x\" "
                               "hash=\"%u\"/>\n",
                               GLOBAL_HASH(vul, set, module, hash).start,
                               GLOBAL_HASH(vul, set, module, hash).len,
                               GLOBAL_HASH(vul, set, module, hash).hash_value);
                }
                for (pp = 0; pp < GLOBAL_MODULE(vul, set, module).num_patch_points;
                     pp++) {
                    print_file(diagnostics_file,
                               "        <hotpatch precedence=\"%u\" offset=\"" PIFX
                               "\">\n",
                               GLOBAL_PPOINT(vul, set, module, pp).precedence,
                               GLOBAL_PPOINT(vul, set, module, pp).offset);
                    print_file(diagnostics_file,
                               "          <function type=\"detector\" "
                               "offset=\"" PIFX "\"/>\n",
                               GLOBAL_PPOINT(vul, set, module, pp).detector_fn);
                    print_file(diagnostics_file,
                               "          <function type=\"protector\" "
                               "offset=\"" PIFX "\" return=\"" PFX "\"/>\n",
                               GLOBAL_PPOINT(vul, set, module, pp).protector_fn,
                               GLOBAL_PPOINT(vul, set, module, pp).return_addr);
                    print_file(diagnostics_file, "        </hotpatch>\n");
                }
                print_file(diagnostics_file, "      </module>\n");
            }
            if (print_sets) { /* xref case 8549 */
                print_file(diagnostics_file, "    </set>\n");
            }
        }
        print_file(
            diagnostics_file,
            "    <stats "
            "num-detected=\"" UINT64_FORMAT_STRING "\" "
            "num-not-detected=\"" UINT64_FORMAT_STRING "\" "
            "num-detector-error=\"" UINT64_FORMAT_STRING "\" "
            "num-protected=\"" UINT64_FORMAT_STRING "\" "
            "num-not-protected=\"" UINT64_FORMAT_STRING "\" "
            "num-kill-thread=\"" UINT64_FORMAT_STRING "\" "
            "num-kill-process=\"" UINT64_FORMAT_STRING "\" "
            "num-raise-exception=\"" UINT64_FORMAT_STRING "\" "
            "num-change-control-flow=\"" UINT64_FORMAT_STRING "\" "
            "num-protector-error=\"" UINT64_FORMAT_STRING "\" "
            "num-aborted=\"" UINT64_FORMAT_STRING "\">\n",
            GLOBAL_VUL(vul).info->num_detected, GLOBAL_VUL(vul).info->num_not_detected,
            GLOBAL_VUL(vul).info->num_detector_error, GLOBAL_VUL(vul).info->num_protected,
            GLOBAL_VUL(vul).info->num_not_protected,
            GLOBAL_VUL(vul).info->num_kill_thread, GLOBAL_VUL(vul).info->num_kill_process,
            GLOBAL_VUL(vul).info->num_raise_exception,
            GLOBAL_VUL(vul).info->num_change_control_flow,
            GLOBAL_VUL(vul).info->num_protector_error, GLOBAL_VUL(vul).info->num_aborted);
        print_file(diagnostics_file,
                   "      <status type=\"execution\">%d</status>\n"
                   "      <status type=\"injection\">%d</status>\n",
                   GLOBAL_VUL(vul).info->exec_status,
                   *(GLOBAL_VUL(vul).info->inject_status));
        print_file(diagnostics_file,
                   "    </stats>\n"
                   "  </vulnerability>\n");
    }
    print_file(diagnostics_file,
               "</vulnerability-table>\n"
               "</hotpatching-information>\n");
}

#    if defined(DEBUG) && defined(DEBUG_MEMORY)
/* Part of bug fix for case 9593 which required leaking trampolines. */
bool
hotp_only_contains_leaked_trampoline(byte *pc, size_t size)
{
    if (!DYNAMO_OPTION(hotp_only) IF_WINDOWS(|| !doing_detach))
        return false;

        /* Today memory debug checks for special heap units only do heap accouting,
         * but not memcmp, both of which are done for regular heaps.  Special heaps
         * are where the leaked trampolines are located.  If we do implement that
         * check then this code in #if 0 would be needed.  Case 10434. */
#        if 0
    for (i = 0; i < hotp_only_num_tramps_leaked; i++) {
        if (hotp_only_tramps_leaked[i] >= pc &&
            hotp_only_tramps_leaked[i] < (pc + size)) {
            /* Make sure we don't have trampolines across heap units! */
            ASSERT((hotp_only_tramps_leaked[i] + HOTP_ONLY_TRAMPOLINE_SIZE) <=
                   (pc + size));
            return true;
        }
    }
#        endif

    /* The actual special_units_t structure (pointed to by
     * hotp_only_tramp_heap) is also leaked.
     * Note: hotp_only_tramp_heap_cache can be NULL if no hotp_only type
     * patches were ever removed either because they were never injected or
     * just weren't removed.
     */
    if ((byte *)hotp_only_tramp_heap_cache >= pc &&
        (byte *)hotp_only_tramp_heap_cache < (pc + size))
        return true;

    return false;
}
#    endif
/*----------------------------------------------------------------------------*/
#    ifdef GBOP

/* This section contains most of the functionality needed to treat gbop hooks
 * as hotp_only patches, thus giving gbop hooks access to all hotp_only patch
 * functionality.  See case 7949 & 7127.
 */

/* Note: Both the gbop detector and protector request for log events.  However,
 *  the detector events are reported only in -detect_mode and protector ones
 *  in !-detect_mode.
 * Note: The app eax & edx are spilled by the gateway and used as scratch;
 *  eax to get the faulting address & edx to send in the current pc (which is
 *  also set by the gateway). xref case 6804 about hotp interface expansion.
 */
static hotp_exec_status_t
hotp_only_gbop_detector(hotp_context_t *cxt)
{
    if (gbop_check_valid_caller((app_pc)APP_XBP(cxt), (app_pc)APP_XSP(cxt),
                                (app_pc)APP_XDX(cxt), (app_pc *)&APP_XAX(cxt))) {
        return HOTP_EXEC_EXPLOIT_NOT_DETECTED;
    } else {
        /* Ask for event; needed to log event if -detect_mode is specified. */
        return HOTP_EXEC_EXPLOIT_DETECTED | HOTP_EXEC_LOG_EVENT;
    }
}

static hotp_exec_status_t
hotp_only_gbop_protector(hotp_context_t *cxt)
{
#        ifdef PROGRAM_SHEPHERDING
    ASSERT(!DYNAMO_OPTION(detect_mode)); /* No protection in detect_mode. */
#        endif

    /* Just log the event; the remediation action for gbop is determined by
     * security_violation() using core options like -kill_thread.
     */
    return HOTP_EXEC_EXPLOIT_PROTECTED | HOTP_EXEC_LOG_EVENT;
}

/* Note: num_vuls is an IN OUT argument; is specifies the current table size and
 * is updated to the new size after reading gbop hooks.  The IN value is used
 * as the append index into the table.
 */
static void
hotp_only_read_gbop_policy_defs(hotp_vul_t *tab, uint *num_vuls)
{
    uint vul_idx;
    hotp_vul_t *vul;
    hotp_set_t *set;
    hotp_module_t *module;
    hotp_patch_point_t *patch_point;
    uint gbop_num_hooks = gbop_get_num_hooks();
    app_pc dr_base = NULL;

    ASSERT(tab != NULL && num_vuls != NULL);
    ASSERT(gbop_num_hooks > 0);
    /* No gbopping for regular hotp, at least not until hotp_only and regular
     * hotp coexist, i.e., hotp_only for native_exec dlls (case 6892)
     */
    ASSERT(DYNAMO_OPTION(hotp_only) && DYNAMO_OPTION(gbop));

    dr_base = get_module_base((app_pc)hotp_only_read_gbop_policy_defs);
    ASSERT(dr_base != NULL);

    for (vul_idx = *num_vuls; vul_idx < (*num_vuls + gbop_num_hooks); vul_idx++) {
        uint gbop_hook_idx = vul_idx - *num_vuls;
        const gbop_hook_desc_t *gbop_hook = gbop_get_hook(gbop_hook_idx);
        ASSERT(gbop_hook != NULL);
        vul = &tab[vul_idx];
        vul->vul_id = dr_strdup(gbop_hook->func_name HEAPACCT(ACCT_HOT_PATCHING));
        /* FIXME: construct this from a combination of {mod,func}_name, or
         * func_name and bad_ret_address.  For now, just hard code it.
         * strdup because hotp_free() thinks this is allocated.
         */
        vul->policy_id = dr_strdup("GBOP.VIOL" HEAPACCT(ACCT_HOT_PATCHING));

        /* FIXME: should this be used to track changes to the gbop detector
         * and protector?  does it matter, after all these patches will only
         * go as part of the core?
         */
        vul->policy_version = 1;

        vul->hotp_dll = NULL;

        vul->hotp_dll_hash = NULL;
        /* There is no notion of only detecting and doing nothing for gbop, so
         * the mode is always protect.
         */
        /* gbop_execlude_filter handles any os specific gbop set removals, xref 9772 */
        if (gbop_exclude_filter(gbop_hook)) {
            vul->mode = HOTP_MODE_OFF;
            LOG(GLOBAL, LOG_HOT_PATCHING, 1, "Excluding %s!%s\n", gbop_hook->mod_name,
                gbop_hook->func_name);
        } else {
            vul->mode = HOTP_MODE_PROTECT;
        }

        vul->num_sets = 1;
        set = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_set_t, 1, ACCT_HOT_PATCHING,
                               PROTECTED);
        vul->sets = set;
        vul->info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, hotp_vul_info_t, ACCT_HOT_PATCHING,
                                    PROTECTED);
        memset(vul->info, 0, sizeof(hotp_vul_info_t)); /* Initialize stats. */

        vul->hotp_dll_base = dr_base;
        vul->type = HOTP_TYPE_GBOP_HOOK;

        set->num_modules = 1;
        module = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_module_t, 1, ACCT_HOT_PATCHING,
                                  PROTECTED);
        set->modules = module;

        module->sig.pe_name = dr_strdup(gbop_hook->mod_name HEAPACCT(ACCT_HOT_PATCHING));
        module->sig.pe_checksum = 0;
        module->sig.pe_timestamp = 0;
        module->sig.pe_image_size = 0;
        module->sig.pe_code_size = 0;
        module->sig.pe_file_version = 0;
        module->num_patch_points = 1;
        patch_point = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_patch_point_t, 1,
                                       ACCT_HOT_PATCHING, PROTECTED);
        module->patch_points = patch_point;
        module->num_patch_point_hashes = 0;
        module->hashes = NULL;
        module->matched = false;
        module->base_address = NULL;

        /* The actual patch offset will be computed if the module matches.
         * vul_id holds the function name, which will be used to compute the
         * offset.  See hotp_process_image().
         * FIXME: we could use a union for offset to hold offset or func_name;
         *  that would be elegant, but would require changing too many things
         *  in hotp - not a good idea, not at least for the first implementation
         */
        patch_point->offset = 0;
        patch_point->detector_fn = (app_pc)hotp_only_gbop_detector - dr_base;
        patch_point->protector_fn = (app_pc)hotp_only_gbop_protector - dr_base;

        /* Longer term issue: do we want to have the notion of changing
         * control flow for gbop hooks?
         */
        patch_point->return_addr = 0;
        /* Precedence hasn't been implemented yet, however, if it had been,
         * then we don't want gbop hooks to interfere with other patches.
         */
        patch_point->precedence = HOTP_ONLY_GBOP_PRECEDENCE;
        patch_point->trampoline = NULL;
        patch_point->app_code_copy = NULL;
        patch_point->tramp_exit_tgt = NULL;
    }

    *num_vuls += gbop_num_hooks;
    ASSERT(*num_vuls == vul_idx);
}

#    endif /* GBOP */

/* Both dr_{insert,update}_probes() will be replaced by dr_register_probes() -
 * PR 225547.  The user will call the same routine to insert or update probes.
 * Subsequent calls will result in old probes being removed and new ones
 * inserted.  By manipulating the input array the user can do inserts (adding
 * new defs. to the array), updates (modifying existing defs) or removes (just
 * removing unwanted defs from the array).
 * Depending upon the context (init or other place) an internal nudge will be
 * created.
 * NOTE: for beta, there is no update, i.e., this routine can be called only
 * once.
 * NOTE: The input is an array of probes because allowing the user to do
 * individual probe registration will result in a nudge for each one, which is
 * very expensive.  Also of note is that it isn't uncommon for API to request
 * arrays; WIN32 native API does it many places.
 * TODO: change hotp vul table to be a list - better for clients, esp. multiple
 * ones; also good if probes are used with LS - PR 225673.
 */
void
dr_register_probes(dr_probe_desc_t *probes, unsigned int num_probes)
{
    unsigned int i, valid_probes;
    hotp_vul_t *tab, *vul;
    hotp_set_t *set;
    hotp_module_t *module;
    hotp_patch_point_t *ppoint;
    static bool probes_initialized = false;

    /* For now, probes are supported iff probe api is explicitly turned on.
     * Also, liveshields shouldn't be on when probe api is on.
     */
    CLIENT_ASSERT(DYNAMO_OPTION(hot_patching) && DYNAMO_OPTION(probe_api) &&
                      !DYNAMO_OPTION(liveshields),
                  "To use Probe API, "
                  "-hot_patching, -probe_api and -no_liveshields options "
                  "should be used.");

    if (!DYNAMO_OPTION(probe_api))
        return; /* be safe */

    /* Hot patching subsystem should be initialized by now. */
    ASSERT(hotp_patch_point_areas != NULL);
    ASSERT(!DYNAMO_OPTION(hotp_only) || hotp_only_tramp_areas != NULL);

    if (num_probes < MIN_NUM_VULNERABILITIES || num_probes > MAX_NUM_VULNERABILITIES ||
        probes == NULL) {
        /* FIXME PR 533384: return a status code! */
        return;
    }

    /* For beta probe registration can be done only once.  However, multiple
     * calls to this routine should be allowed during
     * 1. dr init time - which doesn't need a nudge but needs remove & reinsert
     *      - PR 225580
     * 2. any other point in dr - which requires an internal nudge - PR 225578
     *    what about DR event callbacks like module load/unload?
     * Once both these are implemented the probes_initialized bool can go.
     *
     * Note: as we don't have multiple clients and at startup as we are single
     *       threaded here, there is no need for a lock for this temp. bool.
     */
    if (probes_initialized) {
        /* FIXME PR 533384: return a status code!
         * actually I'm having this continue for at-your-own-risk probes
         */
        ASSERT_CURIOSITY_ONCE(false &&
                              "register probes >1x at your own risk: PR 225580!");
    } else {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        probes_initialized = true;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }

    /* Zero out all dynamically allocated hotpatch table structures to avoid
     * leaks when there is a parse error.  See PR 212707, 213480.
     */
    tab = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_vul_t, num_probes,
                                  ACCT_HOT_PATCHING, PROTECTED, 0);

    for (i = 0, valid_probes = 0; i < num_probes; i++) {
        char *temp;

        vul = &tab[valid_probes];
        /* memset vul to 0 here because parse errors can leave freed pointers */
        memset(vul, 0, sizeof(hotp_vul_t));

        /* TODO: remove this once support is added for exported functions
         * (PR 225654) & raw addresses (PR 225658); for now just prevent
         * needless user errors.
         */
        if (probes[i].insert_loc.type != DR_PROBE_ADDR_LIB_OFFS ||
            probes[i].callback_func.type != DR_PROBE_ADDR_LIB_OFFS) {
            probes[i].status = DR_PROBE_STATUS_UNSUPPORTED;
            goto dr_probe_parse_error;
        }

        /* TODO: validate probe def PR 225663 */
        vul->vul_id = dr_strdup(probes[i].name HEAPACCT(ACCT_HOT_PATCHING));

        /* For probe api policy_id isn't needed, but it can't be set it to NULL
         * because policy status table (used by drview) set up will crash. */
        temp = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, MAX_POLICY_ID_LENGTH + 1,
                                ACCT_HOT_PATCHING, PROTECTED);
        strncpy(temp, probes[i].name, MAX_POLICY_ID_LENGTH);
        temp[MAX_POLICY_ID_LENGTH] = '\0';
        /* TODO: validate probe def PR 225663 */
        vul->policy_id = temp;

        /* Note: if there is a need (highly doubt it) we can expand the probe
         * api to support versioning; for now just set it to 1. */
        vul->policy_version = 1;

        vul->hotp_dll = NULL;
        if (probes[i].callback_func.type == DR_PROBE_ADDR_LIB_OFFS) {
            if (probes[i].callback_func.lib_offs.library != NULL) {
                /* TODO: validate probe def PR 225663 */
                vul->hotp_dll = dr_strdup(
                    probes[i].callback_func.lib_offs.library HEAPACCT(ACCT_HOT_PATCHING));
            } else {
                probes[i].status = DR_PROBE_STATUS_INVALID_LIB;
                goto dr_probe_parse_error;
            }
        } else if (probes[i].callback_func.type == DR_PROBE_ADDR_EXP_FUNC) {
            /* TODO: NYI - support for exported functions (PR 225654) */
            probes[i].status = DR_PROBE_STATUS_UNSUPPORTED;
            goto dr_probe_parse_error;
        } else if (probes[i].callback_func.type != DR_PROBE_ADDR_VIRTUAL) {
            /* TODO: NYI - support for virtual addresses (PR 225658) */
            probes[i].status = DR_PROBE_STATUS_UNSUPPORTED;
            goto dr_probe_parse_error;
        }

        vul->mode = HOTP_MODE_PROTECT;
        vul->num_sets = 1;
        set = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, hotp_set_t, ACCT_HOT_PATCHING, PROTECTED);
        vul->sets = set;
        vul->info = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_vul_info_t, 1,
                                            ACCT_HOT_PATCHING, PROTECTED, 0);

        /* Note: if probe is inside client dll, then client dll SHOULD be in
         * our module_areas - how to assert on this?
         * Update: I found that neither the client dll nor any dll loaded
         * during client init is in our loaded_module_areas because these dlls
         * are loaded after the vm scan in vm_areas_init() but before dr hooks
         * are inserted - I got pop ups in os_get_module_info() because of
         * this.  PR 225670.
         */
        vul->hotp_dll_base = NULL;
        vul->type = HOTP_TYPE_PROBE;

        set->num_modules = 1;
        module = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_module_t, 1,
                                         ACCT_HOT_PATCHING, PROTECTED, 0);
        set->modules = module;

        if (probes[i].insert_loc.type == DR_PROBE_ADDR_LIB_OFFS) {
            if (probes[i].insert_loc.lib_offs.library != NULL) {
                module->sig.pe_name = dr_strdup(
                    probes[i].insert_loc.lib_offs.library HEAPACCT(ACCT_HOT_PATCHING));
            } else {
                probes[i].status = DR_PROBE_STATUS_INVALID_LIB;
                goto dr_probe_parse_error;
            }
        } else if (probes[i].insert_loc.type == DR_PROBE_ADDR_EXP_FUNC) {
            /* TODO: NYI - support for exported functions (PR 225654) */
            probes[i].status = DR_PROBE_STATUS_UNSUPPORTED;
        } else if (probes[i].insert_loc.type != DR_PROBE_ADDR_VIRTUAL) {
            /* TODO: NYI - support for virtual addresses (PR 225658) */
            probes[i].status = DR_PROBE_STATUS_UNSUPPORTED;
            goto dr_probe_parse_error;
        }

        ppoint = HEAP_ARRAY_ALLOC_MEMSET(GLOBAL_DCONTEXT, hotp_patch_point_t, 1,
                                         ACCT_HOT_PATCHING, PROTECTED, 0);
        module->num_patch_points = 1;
        module->patch_points = ppoint;

        /* The actual patch address will be computed if the module matches.
         * vul_id holds the function name, which will be used to compute the
         * offset.  See hotp_process_image().
         */
        /* TODO: validate probe & callback addr here if possible; PR 225663 */
        ppoint->offset = probes[i].insert_loc.lib_offs.offset;
        ppoint->detector_fn = 0; /* No detector for probes. */
        ppoint->protector_fn = probes[i].callback_func.lib_offs.offset;

        /* Precedence hasn't been implemented yet, however, if it had been,
         * then we don't want gbop hooks to interfere with client probes.
         */
#    define HOTP_PROBE_PRECEDENCE (HOTP_ONLY_GBOP_PRECEDENCE - 1)
        ppoint->precedence = HOTP_PROBE_PRECEDENCE;

        /* id generation should be the last step because parsing of a
         * probe can be aborted before that and we don't want an id being
         * returned for a probe that is rejected.
         */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        vul->id = GENERATE_PROBE_ID();
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

        /* If we parsed a probe definition to this point then it is valid. */
        valid_probes++;
        continue;

    dr_probe_parse_error:
        /* Invalid probes are not kept inside dr, but discarded, so a 0 id
         * should be returned for them
         */
        probes[i].id = 0;
        hotp_free_one_vul(vul);
    }

    /* If there were some invalid probes then free extra memory in the initial
     * table allocation.
     */
    if (valid_probes < num_probes) {
        hotp_vul_t *old_tab = tab;
        if (valid_probes > 0) {
            tab = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, hotp_vul_t, valid_probes,
                                   ACCT_HOT_PATCHING, PROTECTED);
            memcpy(tab, old_tab, sizeof(hotp_vul_t) * valid_probes);
        } else {
            tab = NULL;
        }
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, old_tab, hotp_vul_t, num_probes,
                        ACCT_HOT_PATCHING, PROTECTED);
    }
    hotp_load_hotp_dlls(tab, valid_probes);

    /* Can't load dlls with hotp lock held - can deadlock if app is loading a
     * dll too (see hotp nudge for details).  We solve this by setting up the
     * hotp table in a temp var, doing the load on it and then grabbing the
     * hotp lock and setting the global hotp table.  If we have our own loader
     * (PR 209430) we won't need to do this.
     */
    d_r_write_lock(&hotp_vul_table_lock);

    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    GLOBAL_VUL_TABLE = tab;
    NUM_GLOBAL_VULS = valid_probes;

    if (GLOBAL_VUL_TABLE != NULL) {
        ASSERT(NUM_GLOBAL_VULS > 0);
        /* Policy status table must be initialized after the global
         * vulnerability table is setup, but before module list is iterated
         * over because it uses the former and the latter will set status.
         */
        hotp_init_policy_status_table();
    }
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    d_r_write_unlock(&hotp_vul_table_lock);

    /* Unlike hotp_init(), client init happens after vmareas_init(), i.e.,
     * after module processing, so we have to walk the module list again.  It
     * is ok to do the walk without the hotp lock because
     *  1. that is what is done between hotp_init() and vmareas_init() as no
     *      change can happen to the hotp table at that time (nudges
     *      are nop'ed during dr init).
     *  2. dr_register_probes()'s execution at init time can't overlap with
     *      another dr_register_probes() because
     *      a. a second instance can't be called in dr_client_main() before the first
     *          one returns.
     *      b. if a second one is called via a custom nudge or from a callback
     *          it is nop'ed by this routine (at least for this release - for
     *          next release will have to figure the callback part); btw, nudges
     *          during dr init are nop'ed anyway.
     *  3. dr_register_probes()'s execution at init time also can't overlap
     *      with a liveshield nudge (even if we support them both
     *      simultaneously) because during dr init all nudges are nop'ed.
     *
     * NOTE: for probe/hot patch related nudges after dr init (whether custom
     * nudge, liveshield nudge or internal nudge triggered by calling
     * dr_register_probes() after init), loader walking has to be done with the
     * hotp lock held otherwise two nudges can mess up each other (one common
     * problem would be double injection/removal for hotp_only).  This is done
     * in nudge_action_read_policies() and hotp_nudge_handler() for
     * liveshields.
     *
     * For probes, nudge (custom or internally triggered) isn't supported today
     * - a TODO.  When we do that this routine can't be shared as is for both
     * probe registration at init and probe registration after init.
     */
    if (GLOBAL_VUL_TABLE != NULL) {
        /* TODO: opt: if it is safe move client_init() between hotp_init() &
         * vm_areas_init() then this loader-list-walk can be eliminated.
         * UPDATE: no it can't b/c this can be called post-dr_client_main()!
         */
        /* FIXME: to support calling post-dr_client_main() the actual num_threads needs
         * to be passed (and should do a synchall): does PR 225578 cover this?
         */
        hotp_walk_loader_list(NULL, 0, NULL, true /* probe_init */);
    }
}

/* TODO: currently no status is set, probe status & LS status codes needed to
 * be merged, status code groups have to be defined (invalid, wating to be
 * injected, etc.) so nothing is returned.  PR 225548.
 */
int
dr_get_probe_status(unsigned int id, dr_probe_status_t *status)
{
    unsigned int i;
    bool res = false;

    /* For now, probes are supported iff probe api is explicitly turned on.
     * Also, liveshields shouldn't be on when probe api is on.
     */
    CLIENT_ASSERT(DYNAMO_OPTION(hot_patching) && DYNAMO_OPTION(probe_api) &&
                      !DYNAMO_OPTION(liveshields),
                  "To use Probe API, "
                  "-hot_patching, -probe_api and -no_liveshields options "
                  "should be used.");

    if (!DYNAMO_OPTION(probe_api))
        return res; /* be safe */

    if (status == NULL)
        return res;

    *status = DR_PROBE_STATUS_INVALID_ID;
    d_r_read_lock(&hotp_vul_table_lock);
    for (i = 0; i < NUM_GLOBAL_VULS; i++) {
        if (id == GLOBAL_VUL(i).id) {
            *status = *(GLOBAL_VUL(i).info->inject_status);
            res = true;
            break;
        }
    }

    d_r_read_unlock(&hotp_vul_table_lock);
    return res;
}
#endif /* HOT_PATCHING_INTERFACE */

/* Got hotp_read_policy_defs() working, so this can be used for testing now. */
#ifdef HOT_PATCHING_INTERFACE_UNIT_TESTS
static void
hotp_read_policies(void)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    /* TODO: once this function starts reading from file/memory, the data below
     *       can be used as part of unit-hotpatch.c
     */
    static hotp_patch_point_t pp1 = { (app_rva_t)0x673e, (app_rva_t)0x1010,
                                      (app_rva_t)0x1010, (app_rva_t)0x6741, 0 };
    static hotp_module_t mod1 = {
        { "ci_loop_test.exe", 0, 0x4241c037, 0 }, 1, &pp1, false, NULL
    };
    static hotp_set_t set1 = { 1, &mod1 };
    static hotp_vul_info_t info1 = { 0 }, info2 = { 0 };

    static hotp_patch_point_t pp2 = { (app_rva_t)0x440f, (app_rva_t)&hotp_nimda,
                                      (app_rva_t)&hotp_nimda, (app_rva_t)0, 0 };
    static hotp_module_t mod2 = {
        { "iisrtl.dll", 0x20190, 0x384399bc, 0x21000 }, 1, &pp2, false, NULL
    };
    static hotp_set_t set2 = { 1, &mod2 };

    NUM_GLOBAL_VULS = 2;
    hotp_vul_table = (hotp_vul_t *)heap_alloc(
        GLOBAL_DCONTEXT,
        sizeof(hotp_vul_t) * NUM_GLOBAL_VULS HEAPACCT(ACCT_HOT_PATCHING));

    /* ci_loop_text.exe vulnerability */
    GLOBAL_VUL(0).vul_id = "ci_loop_test-vul";
    GLOBAL_VUL(0).policy_id = "ci_loop_test-policy";
    GLOBAL_VUL(0).hotp_dll = "c:\\cygwin\\home\\bharath\\ci\\hotp_2.5.dll";
    GLOBAL_VUL(0).hotp_dll_hash = NULL;
    GLOBAL_VUL(0).hotp_dll_base = NULL; /* Runtime data! just for now */
    GLOBAL_VUL(0).mode = HOTP_MODE_OFF;
    GLOBAL_VUL(0).num_sets = 1;
    GLOBAL_VUL(0).sets = &set1;
    GLOBAL_VUL(0).info = &info1;

    /* nimda vulnerability */
    GLOBAL_VUL(1).vul_id = "nimda-vul";
    GLOBAL_VUL(1).policy_id = "nimda-policy";
    GLOBAL_VUL(1).hotp_dll = "hotp_2_5.dll";
    GLOBAL_VUL(1).hotp_dll_hash = NULL;
    GLOBAL_VUL(0).hotp_dll_base = NULL; /* Runtime data! just for now */
    GLOBAL_VUL(1).mode = HOTP_MODE_OFF;
    GLOBAL_VUL(1).num_sets = 1;
    GLOBAL_VUL(1).sets = &set2;
    GLOBAL_VUL(1).info = &info2;
}
#endif /* HOT_PATCHING_INTERFACE_UNIT_TESTS */
