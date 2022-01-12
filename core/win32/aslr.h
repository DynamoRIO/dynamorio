/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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
 * aslr.h - Types and definitions for ASLR address space layout randomization
 *
 * Plan to implement only for win32.
 */

#ifndef ASLR_H
#define ASLR_H

#include "ntdll.h"

/* ASLR control flags for -aslr */
enum {
    ASLR_DISABLED = 0x00,

    /* application memory areas to randomize */
    ASLR_DLL = 0x01,   /* randomize DLLs mapped as image */
    ASLR_STACK = 0x02, /* case 6287 - padding initial stack from parent */
    /* Note option active in parent applies to its children */

    ASLR_HEAP = 0x04,       /* random initial virtual memory padding */
    ASLR_MAPPED = 0x08,     /* FIXME: NYI case 2491, case 6737 */
    ASLR_EXECUTABLE = 0x10, /* FIXME: NYI case 2491 + case 1948  */

    ASLR_PROCESS_PARAM = 0x20, /* FIXME: NYI case 6840 */
    /* Note option active in parent applies to its children */
    /* controls earliest possible padding from parent, yet interop
     * problem with device drivers assuming ProcessParameters is at a
     * fixed location, separately controlled from ASLR_STACK which
     * gets allocated later
     */

    ASLR_HEAP_FILL = 0x40, /* random padding between virtual allocations */
    ASLR_TEB = 0x80,       /* FIXME: NYI case 2491 */

    /* FIXME: for TEB can reserve the pages after the PEB allocation
     * unit, yet the first threads are still in the PEB allocation
     * unit.  We could fill the original threads space from the parent
     * before any other threads are created, but not that valuable
     * since still providing known writable location */

    /* FIXME: PEB probably CAN'T be moved from user mode at all,
     * definitely can't relocate in running process since can't change protections
     */

    /* range allocation - case 6739 */
    /* A virtual memory range should be allocated to each group,
     * (DLLs, mappings, heap).  Default range allocation within a
     * group is to add new areas bottom up (from a defined starting
     * value + random); after each mapping the size is known and some
     * smaller random padding can be added.
     * FIXME: Reclaiming ranges is not currently controlled,
     *  committed memory issues due to that tracked in case 6729
     */
    ASLR_RANGE_BOTTOM_UP = 0x00100, /* default behaviour, reserved */
    ASLR_RANGE_TOP_DOWN = 0x00200,  /* NYI: FIXME: need size for
                                     * going top down requires an
                                     * extra system call */
    ASLR_RANGE_RANDOM = 0x00400,    /* NYI: FIXME: may cause too
                                     * much fragmentation, best done
                                     * with full vmmap, then we can
                                     * choose a random location
                                     * anywhere in our range */

    ASLR_SHARE_DR_DLL = 0x10000000, /* FIXME: NYI case 8129 */
    /* Note option active in parent applies to its children */
    /* In addition to the current -aslr_dr feature, should eventually
     * share views similar to ASLR_SHARED_CONTENTS */

    /* default in client mode currently is */
    ASLR_CLIENT_DEFAULT = ASLR_DLL | ASLR_STACK | ASLR_HEAP, /* 0x7 */
} aslr_option_t;

/* ASLR control flags for -aslr_cache */
enum {
    /* sharing flags - case 2491 scheme 2.2
     * NYI we may publish section handles in a global
     * namespace to allow sharing of private mappings,
     * and share addresses for compatibility, or both addresses and contents
     * for memory reduction
     */
    ASLR_PROCESS_PRIVATE = 0x1, /* default behaviour, flag RESERVED */

    ASLR_SHARED_PER_USER = 0x2,
    /* NYI: allows per-user sharing so that all instances of a
     * module by 'user' use the same mapping, but other users use
     * private copies.
     *   0) sharing - good - for the general desktop only SYSTEM and Administrator
     *   1) integrity- permission-wise created by 'user'
     *      so other processes may write to it without impact to others
     *   2) disclosure - other users base addresses will be different
     */

    ASLR_SHARED_INHERIT = 0x4,
    /* NYI: allow trusted users to create mappings that other users
     * share (kernel deals with ref counting, and expected to be
     * sticky! so low priv process doesn't need to do anything about
     * removing ).
     *    0) sharing better than ASLR_SHARED_PER_USER
     *    1) integrity same
     *
     *    2) disclosure worse, since now plain users know
     *       the randomized mappings for SYSTEM processes
     *       and allows local attacks
     *
     *    3) there is also a more theoretical information disclosure
     *    in case a published executable has 'secrets' that low
     *    privilege users aren't supposed to be able to read or
     *    execute from
     */

    ASLR_SHARED_IN_SESSION = 0x8,
    /* NYI: \Local\ limits sharing for terminal service users to only
     * current session, can be combined with INHERIT to prevent
     * disclosure of mappings of services from remotely logged users.
     * (Although in Vista services will be the only processes in
     * Session 0, on earlier Windows versions still allows desktop
     * user to completely share and inherit mappings).  May apply only
     * to inherit, or could be used to disallow user in multiple sessions...
     */
    /* Note that different security models based on the user
     * privileges may have some processes independently take some but
     * not necessarily all of file 'producer', section 'publisher' and
     * mapping 'subscriber' roles. */

    ASLR_SHARED_CONTENTS = 0x10,
    /* NYI: If not set sharing will be of address only to resolve
     * incompatible applications requiring a DLL to be mapped at the
     * same address, if set sharing also means saving memory by not
     * using private copies.
     */

    ASLR_SHARED_PUBLISHER = 0x20,
    /* Process is allowed and has permissions to publish sections for
     * other users to map contents.  Note may be different from
     * file producer marked as ASLR_SHARED_FILE_PRODUCER.
     */

    ASLR_SHARED_SUBSCRIBER = 0x40,
    /* Process is to take the risk to use published sections, with
     * risk of privilege escalation if sections or backing files are
     * improperly secured, and according to the sharing inheritance
     * flags.
     */

    ASLR_SHARED_ANONYMOUS_CONSUMER = 0x80,
    /* Anonymous section mappings directly from an already produced
     * file if current user doesn't have permission to publish,
     * e.g. direct file consumer.  Useful if consistency and security
     * needs can be satisfied without need for exclusive file access
     * and other shared objects.  Performance hit depends on
     * consistency checks performed.
     *
     * FIXME: we may want to fallback to this option even if we use as
     * default option a publisher and use any of the further options
     * tracked in case 8812
     */

    ASLR_SHARED_FILE_PRODUCER = 0x100,
    /* Publisher may only be opening files created by a different
     * process if not set, or may produce the relocated files from
     * within DR.
     *
     * FIXME: can also queue up request to generate a particular file
     * even if not materializing immediately in that case it will be
     * combined with ASLR_SHARED_WORKLIST modifier.
     */

    ASLR_SHARED_WORKLIST = 0x200,
    /* Process a worklist of modules to optionally produce and/or
     * publish.  FIXME: Note that separate queues for publishing and
     * producing may be needed.
     */

    ASLR_SHARED_INITIALIZE = 0x1000,
    /* Creates object directories if not yet created - needs to be
     * done by first highly privileged process, although multiple ones
     * attempting to do so is OK.  FIXME: TOFILE Security
     * risk for privileged processes if directories and subdirectories
     * can be created by non-privileged process, needs to make sure
     * there is no race in which a low-privileged process creates
     * entries before an older one.  See comments in
     * nt_initialize_shared_directory() about required privileges if a
     * permanent (until reboot) directory is to be created.
     */

    ASLR_SHARED_INITIALIZE_NONPERMANENT = 0x2000,
    /* make ASLR_SHARED_INITIALIZE initialize as a temporary object
     * instead of the default permanent directory, useful for running
     * a user process without proper permissions.  FIXME: may want to
     * force it to use a per-user directory in this case.
     */

    ASLR_PERSISTENT = 0x100000, /* NYI: namespace placeholder */
    /* allow persistent copies to remain reusable possibly until
     * reboot, or even longer.  Currently this is the default behavior
     * for produced files.
     */

    /* non transparent option, not recommended */
    ASLR_ALLOW_ORIGINAL_CLOBBER = 0x1000000,
    /* FIXME: as a feature case 9033 - would allow overwriting
     * original files without anyone noticiing.  This would allow one
     * to apply M$ patches without a reboot, and they would take
     * effect for any other service using them.  However, it may
     * prevent the use of M$ or third party tools to appropriately
     * determine that application restarts are necessary.  xref case
     * 8623 about the opposite problem of publishers keeping such
     * handles even after all subscribers have been closed.
     */

    /* non transparent option, not recommended */
    ASLR_RANDOMIZE_EXECUTABLE = 0x2000000,
    /*
     *   executables with relocations can now be randomized
     *   from the parent process (especially on Vista).
     * case 8902 - however shows in taskmgr the name of our mangled file
     *   we need to change our naming scheme to make this appear the same
     * FIXME: to support !ASLR_ALLOW_ORIGINAL_CLOBBER
     * we also need to prevent overwrites of the executablewe by duplicating
     * our handle in the target process, so it gets closed when the child dies.
     *
     */

    /* case 9164: default off, may want if many temporary ASP.NET DLLs get created */
    ASLR_AVOID_NET20_NATIVE_IMAGES = 0x4000000,

    /* default in client mode currently should be 0x192 */
    ASLR_CACHE_DEFAULT = ASLR_SHARED_PER_USER | /*  0x2 */
        ASLR_SHARED_CONTENTS |                  /* 0x10 */
        ASLR_SHARED_ANONYMOUS_CONSUMER |        /* 0x80 */
        ASLR_SHARED_FILE_PRODUCER               /* 0x100 */
} aslr_cache_t;

/* ASLR cache coverage options for -aslr_cache_list */
enum {
    /* almost all controlled by other general ASLR exemptions  */
    ASLR_CACHE_LIST_DEFAULT,
    ASLR_CACHE_LIST_INCLUDE, /* -aslr_cache_list_include */
    ASLR_CACHE_LIST_EXCLUDE  /* -aslr_cache_list_exclude */
    /* (note aslr_cache_list_t values match meaning of allowlist blocklist
     * as in process_control)
     */
} aslr_cache_list_t;

enum {
    /* consistency check flags for use with aslr_validation.  These
     * are enforced by publishers when validating already produced
     * files.  Note that with better security guarantees different
     * levels of caching can be employed.  We currently demand that
     * producers produce all supported metadata even if used only by
     * some publishers, that may have different tradeoffs between
     * security and/or performance objectives.
     */

    ASLR_PERSISTENT_LAX = 0x0,
    /* rely only on file size and matching magic
     * insufficient for small patches,
     * least consistency and no security
     */

    ASLR_PERSISTENT_PARANOID = 0x1,
    /* provides strongest consistency and security: full byte
     * comparison of original DLL and a provided relocated DLL
     * most expensive to evaluate
     */

    ASLR_PERSISTENT_SOURCE_DIGEST = 0x2,
    /* provides source consistency and staleness check: MD5 digest of
     * the source file, assuming reliable and trustworthy producer
     */

    ASLR_PERSISTENT_TARGET_DIGEST = 0x4,
    /* target corruption only: MD5 digest of the produced file,
     * nonmalicious corruption only, otherwise no guarantee of the
     * relationship between source and target */
    /* FIXME: This one may not be very useful, but allows for a better
     * check for corrupt files in case we can't guarantee atomicity,
     * e.g. in case files are to be shared over the network.
     *
     * Note that the combination of target digest and paranoid
     * verification gives us somewhat more information than either
     * one: an attacker trying to bypass an MD5 can always calculate
     * the correct value, while a corrupted file will not have it.
     * Interesting information only if we run processes with different
     * privileged with differing level of validation.
     */

    ASLR_PERSISTENT_MODIFIED_TIME = 0x8, /* NYI */
    /* FIXME: currently not possible to read the source file time.
     * Otherwise would provide a weak staleness check only, no
     * security: has potential for missing an update that preserves
     * file times.  Also has some possibility of false positives in
     * case say some antivirus program modifies a file or stores
     * information past the end or in a separate stream of the
     * original DLL.
     *
     * Note that FAT32 vs NTFS times differ so we store this in our
     * own signature instead of trying to touch our file to match.
     */

    ASLR_PERSISTENT_NOTOWNER_PARANOIA = 0x10,
    /* FIXME: NYI force paranoia in case file is not securely owned by
     * the current user, e.g. could have never been overwritten by
     * another user.  This allows us to share between users even in
     * case files are produced by others, and skipping the check if we
     * have safe private copies.
     */

    ASLR_PERSISTENT_SHORT_DIGESTS = 0x20,
    /* a qualifer on ASLR_PERSISTENT_SOURCE_DIGEST and
     * ASLR_PERSISTENT_TARGET_DIGEST to use short as defined by
     * aslr_short_digest.  Allows us to set a limit on verification time
     * and working set impact, with acceptable consistency, but with
     * an obvious security risk.
     */

    ASLR_PERSISTENT_PARANOID_TRANSFORM_EXPLICITLY = 0x10000, /* case 8858 */
    /* When set we'll do the comparison by explicitly making a private
     * copy just as a publisher, otherwise we compare in place
     * relocation by relocation.  FIXME: this flag for
     * ASLR_PERSISTENT_PARANOID should be internal, but for now
     * leaving the old implementation for perf comparison, and added
     * late in the game anyways.
     */

    /* FIXME: not recommended in production, should really make INTERNAL */
    ASLR_PERSISTENT_PARANOID_PREFIX = 0x20000,
    /* This flag for ASLR_PERSISTENT_PARANOID similarly to
     * ASLR_PERSISTENT_SHORT_DIGESTS makes it not really so paranoid.
     * Allows to trade security and consistency risk for performance.
     *
     * Cannot be used for security if target files are world writable -
     * planting bad code for Administrator running explorer.exe is a
     * bad enough elevation of privileges.
     *
     * FIXME: if in the future we do on-demand comparison, we may
     * still want to first verify a prefix before we decide that the
     * DLL is usable.
     */
} aslr_consistency_check_t;

/* ASLR control flags for -aslr_action */
enum {
    ASLR_NO_ACTION = 0x0, /* do not track */

    /* Track likely attempts to use preferred addresses */
    ASLR_TRACK_AREAS = 0x1,   /* keep track of would_be regions */
    ASLR_AVOID_AREAS = 0x2,   /* NYI disallow other DLL mappings */
    ASLR_RESERVE_AREAS = 0x4, /* NYI virtually reserve to avoid any allocation */

    /* Reporting exceptions */
    /* intercept execute faults when run native, or RCT violations
     * when targeting unreadable memory.
     *
     * FIXME: if areas are not reserved an RCT violation in a
     * would_be area may also be attributed here.
     */
    ASLR_DETECT_EXECUTE = 0x10,
    /* FIXME: cannot reliably distinguish read from Execute when not
     * enforcing DR security policies on a machine without NX */
    ASLR_DETECT_READ = 0x20,  /* NYI */
    ASLR_DETECT_WRITE = 0x40, /* NYI */

    /* report likely violations */
    ASLR_REPORT = 0x100,
    /* if not set, stays silent, yet detection worthwhile in
     * combination with alternative handling where we'd kill an
     * injected thread */

    /* Alternative attack handling */
    ASLR_HANDLING = 0x1000, /* NYI the default of throw_exception or kill_thread */
    /* if not set an exception is simply passed to the application */

    /* ThreatIDs can be normalized */
    ASLR_NORMALIZE_ID = 0x10000, /* NYI use bytes at current mapping
                                  * instead of would_be address */
    /* as long as there are no relocations should maintain ThreatID */
} aslr_action_t;

/* testing and stress testing range flags for -aslr_internal */
enum {
    ASLR_INTERNAL_DEFAULT = 0x0000, /* normal handling */

    /* stress test option to verify proper dealing with address conflicts */
    /* doesn't increment base, so most requests will overlap */
    ASLR_INTERNAL_SAME_STRESS = 0x1000,

    /* testing option - actually not choosing base, FIXME: remove soon */
    ASLR_INTERNAL_RANGE_NONE = 0x2000,

    ASLR_INTERNAL_SHARED_NONUNIQUE = 0x800000,
    /* stress test naming conflicts */

    ASLR_INTERNAL_SHARED_APPFILE = 0x1000000,
    /* stress test - use application file to test sections */

    ASLR_INTERNAL_SHARED_AND_PRIVATE = 0x2000000,
    /* stress test - use our files but still randomize privately as well */

    /* Note that -exempt_aslr_list '*' can also be used as a stress
     * option, instead of another flag here.
     */
} aslr_internal_option_t;

typedef struct {
    bool sys_aslr_clobbered;
    /* mark syscalls modified by ASLR, and need additional handling */

    /* ASLR_SHARED_CONTENTS needs to preserve some context across
     * NtCreateSection and NtMapViewOfSection system calls
     * xref case 9028 about using a more robust scheme that doesn't depend
     * on these being consecutive: FIXME: add to section2file table?
     */
    HANDLE randomized_section_handle; /* for shared randomization */
    app_pc original_section_base;     /* for detecting attacks */
    uint original_section_timestamp;  /* for hotpatching */
    uint original_section_checksum;   /* for hotpatching */
    HANDLE original_image_section_handle;
    /* used for !ASLR_ALLOW_ORIGINAL_CLOBBER to pass information from
     * NtCreateSection to NtMapViewOfSection or NtCreateProcess* */

#ifdef DEBUG
    /* with i#138's section2file table we only use this for debugging */
    HANDLE last_app_section_handle; /* flagging unusual section handle uses */
#endif

    /* Case 9173: only pad each child once.  Rather than record every child seen
     * (which has problems w/ pid reuse, as well as unbounded growth, as we
     * won't see child death), we only record the previous one, leaving a corner
     * case with alternate memory allocations to multiple pre-thread children
     * causing us to pad multiply; likewise with separate threads each
     * allocating in the same child.  We'll live with both risks.
     */
    process_id_t last_child_padded; /* pid */
} aslr_syscall_context_t;

#define ASLR_INVALID_SECTION_BASE ((app_pc)PTR_UINT_MINUS_1)

/* names should look like kernel32.dll-12349783 */
#define MAX_PUBLISHED_SECTION_NAME 64

void
aslr_init(void);
void
aslr_exit(void);
void
aslr_thread_init(dcontext_t *dcontext);
void
aslr_thread_exit(dcontext_t *dcontext);

void
aslr_free_last_section_file_name(dcontext_t *dcontext);

void
aslr_pre_process_mapview(dcontext_t *dcontext);
void
aslr_post_process_mapview(dcontext_t *dcontext);

void
aslr_pre_process_unmapview(dcontext_t *dcontext, app_pc base, size_t size);
reg_t
aslr_post_process_unmapview(dcontext_t *dcontext);

void
aslr_post_process_allocate_virtual_memory(dcontext_t *dcontext, app_pc base, size_t size);
void
aslr_pre_process_free_virtual_memory(dcontext_t *dcontext, app_pc base, size_t size);

/* FIXME: wrap in aslr_post_process_create_or_open_section */
bool
aslr_post_process_create_section(IN HANDLE old_app_section_handle, IN HANDLE file_handle,
                                 OUT HANDLE *new_relocated_handle);

bool
aslr_post_process_create_or_open_section(dcontext_t *dcontext, bool is_create,
                                         IN HANDLE file_handle /* OPTIONAL */,
                                         OUT HANDLE *sysarg_section_handle);
bool
aslr_is_handle_KnownDlls(HANDLE directory_handle);

bool
aslr_recreate_known_dll_file(OBJECT_ATTRIBUTES *obj_attr, OUT HANDLE *recreated_file);

void
aslr_maybe_pad_stack(dcontext_t *dcontext, HANDLE process_handle);

void
aslr_force_dynamorio_rebase(HANDLE process_handle);

/* deterministic (and reversible) timestamp transformation */
static inline uint
aslr_timestamp_transformation(uint old_timestamp)
{
    return old_timestamp + 1;
}

#ifdef GBOP
/* Generically Bypassable Overflow Protection in user mode
 *
 * For reference see
 * P. Szor, "Virus Research and Defense", Chapter 13, 13.3.1.1 and 13.3.4.2,
 *   (skipping 13.2.6 on program shepherding and 13.3.4.1 ASLR)
 * or "Bypassing 3rd Party Windows Buffer Overflow Protection"
 *     http://www.phrack.org/show.php?p=62&a=5 on how to break one
 *
 * FIXME: For interoperability purposes need to identify a
 * compatibility mode that doesn't do more than competitor's desktop
 * suite offerings, so their BOP functionality can be turned off.  Yet
 * to avoid reversing them (although legally allowed for
 * interoperability purposes), for now we'll do the best they may be
 * doing.
 */

/* GBOP control flags for the -gbop option */
enum {
    GBOP_DISABLED = 0x0,
    /* source is identified as [TOS] for the first level,
     * and if non-zero gbop_frames a stack backtrace is attempted.
     *
     * Note that providing FPO information (requires product updates
     * for new versions) would allow deeper hooking and reliable
     * backtraces.
     */

    /* source memory properties, note these are ORed together,
     * at least one of the following set needs to be enabled */
    GBOP_IS_EXECUTABLE = 0x1, /* using our own tracking definitions */
    GBOP_IS_X = 0x2,          /* allowing all ..X pages, cf -executable_if_x */
    GBOP_IS_IMAGE = 0x4,      /* allowing all MEM_IMAGE pages,
                               * c.f. -executable_if_image */
    /* FIXME: another realistic policy would be to allow RWX as
     * long as it is in an image, but not otherwise
     */
    /* FIXME: add GBOP_IS_NOT_W to allow as long as not writable */

    GBOP_IS_NOT_STACK = 0x8,
    /* If set, allows returns to anything but the current stack.  Case 8085. */

    /* see also GBOP_IS_FUTURE_EXEC = 0x04000 */

    /* source instruction type.  Checks if instruction according to
     * [TOS] is of valid type to have targeted the hook location.
     * Note the allowed type checks are ORed. Evaluated only if source
     * memory protection is satisfied.  See also stronger validation
     * in GBOP_EMULATE_SOURCE.
     */
    GBOP_CHECK_INSTR_TYPE = 0x10, /* no source instruction type restriction if not set */
    GBOP_IS_CALL = 0x20,          /* verify source is at all a CALL instruction */
    GBOP_IS_JMP = 0x40,           /* FIXME: not needed - app JMP won't be seen on TOS */
    GBOP_IS_HOTPATCH_JMP = 0x80,  /* our JMP in case we hotpatched purported source */
    /* FIXME: we can't find the source if it was a JMP or JMP*, unless
     * the caller explicitly wants to fool us with one.
     *
     * Note that in case of tail recursion elimination [TOS] is really
     * of the caller of the previous function, should check that there
     * are no internal XREFs to a hooked location, but we don't really
     * care.
     */

    /* source instruction validation, if correct instruction type,
     * check further whether it could have targeted our hook or it is
     * used as chaff to bypass the previous simple validations.
     */
    GBOP_EMULATE_SOURCE = 0x100, /* NYI */
    /* We do know that this was the previous instruction and there is
     * no reason it should have lost its state in normal operations.
     * All registers can be restored to original expected state when
     * hooking at entry point, only ESP on say call [esp+8] is the
     * hardest where should revert ESP a little bit to get back to
     * original state.  Of course call [esp-4] is impossible to
     * recover since it will be overwritten, but not expected in real
     * code.
     *
     * Needs to support CALL intrasegment to hook, CALL *eax or CALL
     * *[IAT] intrasegment to hook, and CALL PLT -> JMP *[IAT] to hook.
     * Other custom jump tables may get more messy.
     */

    /* target instruction check for simple ret2libc */
    GBOP_IS_RET_TO_ENTRY = 0x00200, /* NYI */

    /* FIXME: See Szor's idea of checking whether PC=[ESP-4] as a
     * check for RET_LIBC attack.  Is at all that safe to do - would
     * there be a valid program doing PUSH target, RET targeting the
     * exported routines, or simply remnants from a stack frame where
     * the entry point is pushed (e.g. register spill).  Attacker
     * could have used a RET 4 instr - then the API entry point is at
     * [ESP-8] and can no longer be validated
     */

    /* FIXME: GBOP_WHEN_NATIVE_EXEC = 0x01000, NYI */
    /* should this be applied when running in native exec mode - need
     * to tell apart native_exec from hotp_only .NET, Java, maybe VB.
     * Today -gbop is not by default, and recommended to use only for
     * hotp_only.  This flag would be useful only when running in a
     * future mix of -hotp_only and DR in the same process the two
     * modes may need to differentiated.
     */

    GBOP_IS_DGC = 0x02000,
    /* If set, allows returns to heap (not stack) if a known vm has been loaded.
     * native_exec today runs all vms/dgc natively; gbop for dgc involves
     * doing all the same bookkeepping to identify vms and uses -hotp_only
     * (which uses native_exec), so they are the same.  The bookkeepping may
     * have to be split up if native_exec's definition changes.  See case 8087.
     * The main difference between just native_exec & gbop is that the former
     * is used only to run a dll/dgc natively, not when the control is in dr,
     * whereas in gbop it is used to avoid false positives for dgc.
     */

    GBOP_IS_FUTURE_EXEC = 0x04000,
    /* using our tracking definition for allowing a region as a
     * futureexec
     */

    GBOP_DIAGNOSE_SOURCE = 0x10000, /* NYI */
    /* all enabled checks are evaluated to allow diagnosing all
     * failures, and evaluate any disabled OR checks to provide
     * recommendations to workaround a false positive based on a
     * single run without staging.
     */

    /* FIXME: we may want to further control whether the hooked
     * locations for other purposes do a GBOP check, or leave it only
     * to the 'extra' hooks from gbop_include_list */

    /* default in client mode currently should be 0x6037 */
    GBOP_CLIENT_DEFAULT = GBOP_IS_DGC | GBOP_IS_FUTURE_EXEC | /* 0x06000 */
        GBOP_CHECK_INSTR_TYPE | GBOP_IS_CALL |                /* 0x30 */
        GBOP_IS_EXECUTABLE | GBOP_IS_X | GBOP_IS_IMAGE        /*  0x7 */
};

extern bool gbop_vm_loaded;

typedef struct {
    const char *mod_name;
    const char *func_name;
} gbop_hook_desc_t;

const gbop_hook_desc_t *
gbop_get_hook(uint hook_index);
uint
gbop_get_num_hooks(void);

bool
gbop_exclude_filter(const gbop_hook_desc_t *gbop_hook);

bool
gbop_check_valid_caller(app_pc reg_ebp, app_pc reg_esp, app_pc cur_pc,
                        app_pc *violating_source_addr /* OUT */);
void
gbop_validate_and_act(app_state_at_intercept_t *state, byte fpo_adjustment,
                      app_pc hooked_target);
#endif /* GBOP */

#endif /* ASLR_H */
