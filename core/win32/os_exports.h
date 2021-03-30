/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * os_exports.h - Win32 specific exported declarations
 */

#ifndef _OS_EXPORTS_H_
#define _OS_EXPORTS_H_ 1

#include "os_public.h"
#include "../os_shared.h"
#include "arch_exports.h" /* for priv_mcontext_t */
#include "aslr.h"         /* for aslr_context */

/* you can rely on these increasing with later versions */
/* XXX: when updating, also update DR_WINDOWS_VERSION_* in instrument.h,
 * dr_get_os_version() in instrument.c, defines in suite/tests/tools.h,
 * get_windows_version() in suite/tests/tools.c, defines in libutil/mfapi.h,
 * and get_platform() in libutil/utils.c.
 */
#define WINDOWS_VERSION_10_1803 105 /* 10.5 is artificial */
#define WINDOWS_VERSION_10_1709 104 /* 10.4 is artificial */
#define WINDOWS_VERSION_10_1703 103 /* 10.3 is artificial */
#define WINDOWS_VERSION_10_1607 102 /* 10.2 is artificial */
#define WINDOWS_VERSION_10_1511 101 /* 10.1 is artificial */
#define WINDOWS_VERSION_10 100
#define WINDOWS_VERSION_8_1 63
#define WINDOWS_VERSION_8 62
#define WINDOWS_VERSION_7 61
#define WINDOWS_VERSION_VISTA 60
#define WINDOWS_VERSION_2003 52
#define WINDOWS_VERSION_XP 51
#define WINDOWS_VERSION_2000 50
#define WINDOWS_VERSION_NT 40
int
get_os_version(void);
void
get_os_version_ex(int *version OUT, uint *service_pack_major OUT,
                  uint *service_pack_minor OUT, uint *build_number OUT,
                  const char **release_id OUT, const char **edition OUT);

/* TEB offsets
 * we'd like to use offsetof(TEB, field) but that would require
 * everyone to include ntdll.h, and wouldn't work for inline assembly,
 * so we hardcode the fields we need here.  We check vs offsetof() in d_r_os_init().
 */
enum {
#ifdef X64
    EXCEPTION_LIST_TIB_OFFSET = 0x000,
    TOP_STACK_TIB_OFFSET = 0x008,
    BASE_STACK_TIB_OFFSET = 0x010,
    FIBER_DATA_TIB_OFFSET = 0x020,
    SELF_TIB_OFFSET = 0x030,
    PID_TIB_OFFSET = 0x040,
    TID_TIB_OFFSET = 0x048,
    ERRNO_TIB_OFFSET = 0x068,
    WOW64_TIB_OFFSET = 0x100,
    PEB_TIB_OFFSET = 0x060,
    FLS_DATA_TIB_OFFSET = 0x17c8,
    NT_RPC_TIB_OFFSET = 0x1698,
    NLS_CACHE_TIB_OFFSET = 0x17a0,
    STATIC_TLS_TIB_OFFSET = 0x58,
#else
    EXCEPTION_LIST_TIB_OFFSET = 0x00,
    TOP_STACK_TIB_OFFSET = 0x04,
    BASE_STACK_TIB_OFFSET = 0x08,
    FIBER_DATA_TIB_OFFSET = 0x10,
    SELF_TIB_OFFSET = 0x18,
    PID_TIB_OFFSET = 0x20,
    TID_TIB_OFFSET = 0x24,
    ERRNO_TIB_OFFSET = 0x34,
    WOW64_TIB_OFFSET = 0xC0,
    PEB_TIB_OFFSET = 0x30,
    FLS_DATA_TIB_OFFSET = 0xfb4,
    NT_RPC_TIB_OFFSET = 0xf1c,
    NLS_CACHE_TIB_OFFSET = 0xfa0,
    STATIC_TLS_TIB_OFFSET = 0x2c,
#endif
};

#ifdef X64
#    define SEG_TLS SEG_GS
#    define LIB_SEG_TLS SEG_GS /* win32 lib tls */
#else
#    define SEG_TLS SEG_FS     /* x86 and WOW64 */
#    define LIB_SEG_TLS SEG_FS /* win32 lib tls */
#endif

#define DR_REG_SYSNUM REG_EAX

/* even INLINE_FORCED isn't inlining this into d_r_get_thread_id() in debug build (i#655)
 */
#define d_r_get_tls(/*ushort*/ tls_offs) \
    ((void *)IF_X64_ELSE(__readgsqword, __readfsdword)(tls_offs))

static inline void
d_r_set_tls(ushort tls_offs, void *value)
{
    IF_X64_ELSE(__writegsqword, __writefsdword)(tls_offs, (ptr_uint_t)value);
}

/* even INLINE_FORCED isn't inlining this into d_r_get_thread_id() in debug build (i#655)
 */
#define get_own_teb() ((TEB *)d_r_get_tls(SELF_TIB_OFFSET))

/* We need to meet these requirements:
 * + DrMi#1676: cur esp is in [StackLimit..StackBase) at all times on Win8.1.
 * + i#921, i#1102: StackLimit is a valid page at all times.
 * + DrMi#1723: StackLimit must be updated for app guard page hits, even when
 *   in client code.
 * Our solution is:
 * A) Ensure dstack > app xsp.
 * B) StackLimit holds app value at all times, except on thread exit where
 *    loader_pre_client_thread_exit() swaps it.
 * C) Swap StackBase on context switches between priv and app values.
 *
 * If either of these macros is changed, dr_call_on_clean_stack() needs to
 * also be changed to match.
 */
#define SWAP_TEB_STACKLIMIT() (false)
#define SWAP_TEB_STACKBASE() (true)

/* If this changes our persisted caches may all fail.
 * We assert that this matches SYSTEM_BASIC_INFORMATION.AllocationGranularity
 * in get_system_basic_info().
 */
#define OS_ALLOC_GRANULARITY 64 * 1024
#define MAP_FILE_VIEW_ALIGNMENT OS_ALLOC_GRANULARITY

/* Used to flush a thread's stack prior to rest of exit routines.  Caller
 * is required to own the thread_initexit_lock when calling this routine. */
void
os_thread_stack_exit(dcontext_t *dcontext);

void
debugbox_setup_title(void);
int
debugbox(char *msg);
const wchar_t *
debugbox_get_title(void);
int
os_countdown_messagebox(char *message, int time_in_milliseconds);

/* raise an exception in the application context */
void
os_raise_exception(dcontext_t *dcontext, EXCEPTION_RECORD *pexcrec, CONTEXT *pcontext);
int
exception_frame_chain_depth(dcontext_t *dcontext);

/* PR 263338: we have to pad for alignment */
#define CONTEXT_HEAP_SIZE(cxt) (sizeof(cxt) IF_X64(+8) /*heap is 8-aligned already*/)
#define CONTEXT_HEAP_SIZE_OPAQUE (CONTEXT_HEAP_SIZE(CONTEXT))

typedef CONTEXT *os_cxt_ptr_t;
#define osc_empty NULL

static inline bool
is_os_cxt_ptr_null(os_cxt_ptr_t osc)
{
    return osc == NULL;
}

static inline void
set_os_cxt_ptr_null(os_cxt_ptr_t *osc)
{
    *osc = NULL;
}

/* Only one of mc and dmc can be non-NULL. */
bool
os_context_to_mcontext(dr_mcontext_t *dmc, priv_mcontext_t *mc, os_cxt_ptr_t osc);
/* Only one of mc and dmc can be non-NULL. */
bool
mcontext_to_os_context(os_cxt_ptr_t osc, dr_mcontext_t *dmc, priv_mcontext_t *mc);

bool
thread_get_context(thread_record_t *tr, CONTEXT *context);

bool
thread_set_context(thread_record_t *tr, CONTEXT *context);

/* generated routine for taking over native threads */
extern byte *thread_attach_takeover;

void
thread_attach_translate(dcontext_t *dcontext, priv_mcontext_t *mc INOUT,
                        bool restore_memory);

/* Should be passed the full current context, including pc, of a thread
 * set up for takeover but not yet scheduled.
 */
void
thread_attach_exit(dcontext_t *dcontext, priv_mcontext_t *mc);

/* To move a var into one of our special self-protected sections, in
 * addition to declaring a var between START_DATA_SECTION and
 * END_DATA_SECTION you must initialize it to something!
 * cl has no var attributes, only pragmas to indicate current section, so
 * have to ensure that your var goes into proper one of data, bss, and const
 * by manipulating its initialization -- fortunately, any explicit initialization,
 * even to 0, causes cl to put it into data and not bss.
 */
/* Use special C99 operator _Pragma to generate a pragma from a macro */
#if _MSC_VER <= 1200 /* FIXME: __pragma may work w/ vc6: then don't need #if */
#    define ACTUAL_PRAGMA(p) _Pragma(#    p)
#else
#    define ACTUAL_PRAGMA(p) __pragma(p)
#endif
#define START_DATA_SECTION(name, prot) ACTUAL_PRAGMA(data_seg(name))
#define VAR_IN_SECTION(name) /* nothing */
#define END_DATA_SECTION() ACTUAL_PRAGMA(data_seg())
#define START_DO_NOT_OPTIMIZE ACTUAL_PRAGMA(optimize("g", off))
#define END_DO_NOT_OPTIMIZE ACTUAL_PRAGMA(optimize("g", on))

#ifdef DEBUG
void
print_dynamo_regions(void);
#endif

size_t
get_allocation_size(byte *pc, byte **base_pc);
byte *
get_allocation_base(byte *pc);
void
mark_page_as_guard(byte *pc);
bool
unmark_page_as_guard(byte *pc, uint prot);

bool
os_find_free_code_space_in_libs(void **start OUT, void **end OUT);

void
merge_writecopy_pages(app_pc start, app_pc end);

bool
is_pid_me(process_id_t pid);
bool
is_phandle_me(HANDLE phandle);

extern bool intercept_asynch;
extern bool intercept_callbacks;
extern volatile bool init_apc_go_native_pause;
extern volatile bool init_apc_go_native;
extern process_id_t win32_pid;
extern void *peb_ptr; /* not exposing type in case not including ntdll.h */

extern app_pc vsyscall_page_start;
/* pc kernel will claim app is at while in syscall */
extern app_pc vsyscall_after_syscall;
/* pc of the end of the syscall instr itself */
extern app_pc vsyscall_syscall_end_pc;
/* IF_X64(ASSERT_NOT_IMPLEMENTED(false)) -- need to update */
#define VSYSCALL_PAGE_START_BOOTSTRAP_VALUE ((app_pc)(ptr_uint_t)0x7ffe0000)
#define VSYSCALL_BOOTSTRAP_ADDR ((app_pc)(ptr_uint_t)0x7ffe0300)
#define VSYSCALL_AFTER_SYSCALL_BOOTSTRAP_VALUE ((app_pc)(ptr_uint_t)0x7ffe0304)

/* ref case 5217 - for Sygate compatibility have to execute int's out of
 * ntdll.dll. This will hold the target to use (points to int 2e ret 0 in
 * NtYieldExecution). */
extern app_pc int_syscall_address;
/* ref case 5441 - for Sygate compatibility the return address for sysenter
 * system calls needs to be in ntdll.dll for some platforms.  This will point
 * to a ret 0 in ntdll (NtYieldExecution). */
extern app_pc sysenter_ret_address;
/* i#537: sysenter returns to KiFastSystemCallRet from KiFastSystemCall.
 * We do not support XPSP{0,1} wrt showing the skipped ret.
 */
extern app_pc KiFastSystemCallRet_address;

/* For Win10 this is ntdll!Wow64SystemServiceCall, which the call* in
 * each ntdll syscall wrappers targets.  There are also copies in
 * kernelbase, kernel32, user32, and gdi32.
 */
extern app_pc wow64_syscall_call_tgt;

bool
ignorable_system_call(int num, instr_t *gateway, dcontext_t *dcontext_live);
bool
optimizable_system_call(int num);
#ifdef DEBUG
void
check_syscall_numbers(dcontext_t *dcontext);
#endif
bool
is_cb_return_syscall(dcontext_t *dcontext);

#ifdef WINDOWS_PC_SAMPLE
file_t profile_file;
mutex_t profile_dump_lock;

typedef struct _profile_t {
    void *start;
    void *end;
    uint bucket_shift;
    uint *buffer;
    size_t buffer_size; /* in bytes */
    bool enabled;
    dcontext_t *dcontext;
    HANDLE handle;
} profile_t;

/* dcontext optional, allocates global if NULL, otherwise local to that dcontext*/
profile_t *
create_profile(void *start, void *end, uint bucket_shift, dcontext_t *dcontext);

void
free_profile(profile_t *profile);

void
start_profile(profile_t *profile);

void
stop_profile(profile_t *profile);

void
dump_profile(file_t file, profile_t *profile);

void
dump_profile_range(file_t file, profile_t *profile, byte *start, byte *end);

uint
sum_profile_range(profile_t *profile, byte *start, byte *end);

uint
sum_profile(profile_t *profile);

void
reset_profile(profile_t *profile);
#endif

enum {
    /* via -hide_from_query_mem controls what dr does when the app does
     * a query virtual memory call on the dynmorio.dll base */
    HIDE_FROM_QUERY_TYPE_PROTECT = 0x1,   /* to PRIVATE, NO_ACCESS */
    HIDE_FROM_QUERY_BASE_SIZE = 0x2,      /* shift reported allocation a page,
                                           * and expand size to whole dll */
    HIDE_FROM_QUERY_RETURN_INVALID = 0x4, /* return STATUS_INVALID_ADDRESS to
                                           * the app */
};

/* flags for DYNAMO_OPTION(tls_flags) */
enum tls_flags {
    TLS_FLAG_BITMAP_TOP_DOWN = 0x1,  /* when set use last available
                                      * TLS slots, otherwise use first
                                      * just the way TlsAlloc would */
    TLS_FLAG_CACHE_LINE_START = 0x2, /* when set the first should
                                      * start at a cache line,
                                      * otherwise as long as all
                                      * entries should fit order doesn't matter */
    TLS_FLAG_BITMAP_FILL = 0x4,      /* FIXME: NYI: reserve slots
                                      * unused due to alignment,
                                      * should be needed only for
                                      * aligned bottom up xref case
                                      * 6770 SQL 2005
                                      */
};

/* flags for DYNAMO_OPTION(os_aslr) */
enum {
    /* disable completely our ASLR and persistent caches for all modules */
    OS_ASLR_DISABLE_ASLR_ALL = 0x01,   /* disable ASLR DLL,stack,heap */
    OS_ASLR_DISABLE_PCACHE_ALL = 0x02, /* disable pcache generation and use */

    /* note that after section mapping we can read from header whether
     * OS would have randomized base
     */
    OS_ASLR_DISABLE_ASLR_DETECT = 0x10,   /* NYI case 8225 */
    OS_ASLR_DISABLE_PCACHE_DETECT = 0x20, /* NYI case 8225 */
};

enum {
    /* Does not override attack handling options (i.e. kill_thread etc. still
     * do their thing) only detaches if the we were going to kill the
     * process */
    DETACH_UNHANDLED_VIOLATION = 0x01, /* FIXME : separate A, B, C etc.? */
    /* Subset of DETACH_UNHANDLED_VIOLATION, detaches if we see an unsupported
     * module */
    DETACH_UNSUPPORTED_MODULE = 0x02,

    /* Anything below this line is unsafe and will likely fail */
    /* FIXME : this detaches on any internal process terminate, including from
     * a security violation (which we may want to allow to kill the process, as
     * opposed to an internal error in future), in future may also want to
     * further break it up into internal_exception, assertion, etc. */
    DETACH_ON_TERMINATE = 0x010,
    /* safer then w/cleanup, leaves dr memory behind */
    DETACH_ON_TERMINATE_NO_CLEAN = 0x020,

    /* **** FIMXE : following NYI **** */
    /* don't kill faulting thread, make a best guess at its app context
     * note this could turn the kill thread on the faulting thread to a
     * throw exception if we get the context wrong */
    DETACH_ON_TERMINATE_NO_KILL = 0x040,
    /* the following two options try to help prevent hangs when detaching on
     * terminate, but there are hanging scenarios that aren't covered */
    /* tries to avoid deadlocking by proactively freeing some locks (if held)
     * very unsafe */
    DETACH_ON_TERMINATE_NO_LOCKS = 0x080,
    /* tries to detect an infinite loop in the detach synchronization
     * routines and kills the process in that scenario */
    DETACH_ON_TERMINATE_NO_HANG = 0x100,
};

/* mcontext must be valid, including the pc field (native) and app_errno
 * must not be holding any locks */
/* sets detach in motion and never returns */
void
detach_internal_synch(void);
void
detach_internal(void);
bool
detach_handle_callbacks(int num_threads, thread_record_t **threads,
                        bool *cleanup_tpc /* array of size num_threads */);
void
detach_remove_image_entry_hook(int num_threads, thread_record_t **threads);
enum {
    DETACH_NORMAL_TYPE = 0,
    DETACH_BAD_STATE = -1,
    DETACH_BAD_STATE_NO_CLEANUP = -2,
};
void
detach_helper(int detach_type); /* needs to be exported for nudge.c */

void
early_inject_init(void);
bool
earliest_inject_init(byte *arg_ptr);
void
earliest_inject_cleanup(byte *arg_ptr);

wait_status_t
os_wait_handle(HANDLE h, uint64 timeout_ms);

void
os_take_over_mark_thread(thread_id_t tid);

void
os_take_over_unmark_thread(thread_id_t tid);

bool
os_dump_core_live(const char *msg, char *path OUT, size_t path_sz);

/* in module.c */
app_pc
get_module_preferred_base_safe(app_pc pc);
app_pc
get_module_preferred_base(app_pc pc);
ssize_t
get_module_preferred_base_delta(app_pc pc);
bool
in_same_module(app_pc target, app_pc source);
void
print_modules_safe(file_t f, bool dump_xml);
void
print_modules_ldrlist_and_ourlist(file_t f, bool dump_xml, bool conservative);
/* FIXME: rename this to get_module_path, cf. get_module_short_name() */
void
get_module_name(app_pc, char *buf, int max_chars);
bool
is_module_patch_region(dcontext_t *dcontext, app_pc start, app_pc end, bool conservative);
bool
is_IAT(app_pc start, app_pc end, bool page_align, app_pc *iat_start, app_pc *iat_end);
bool
is_in_IAT(app_pc addr);
bool
get_IAT_section_bounds(app_pc module_base, app_pc *iat_start, app_pc *iat_end);
bool
os_module_store_IAT_code(app_pc addr);
bool
os_module_cmp_IAT_code(app_pc addr);
bool
os_module_free_IAT_code(app_pc addr);
void
print_module_section_info(file_t file, app_pc addr);
/* here rather than os_shared.h since this is win32-specific */
/* Returns true if addr is in a xdata section and if so returns in sec_start and sec_end
 * the bounds of the section containing addr (MERGED with adjacent xdata sections).
 * sec_start and sec_end are optional. */
bool
is_in_xdata_section(app_pc module_base, app_pc addr, app_pc *start_pc, app_pc *end_pc);
thread_id_t
get_loader_lock_owner(void);
bool
module_pc_section_lookup(app_pc module_base, app_pc pc,
                         IMAGE_SECTION_HEADER *section_out);
bool
os_module_has_dynamic_base(app_pc module_base);

/* in callback.c */
dcontext_t *
get_prev_swapped_dcontext(dcontext_t *dcontext);
void
callback_interception_init_start(void);
void
callback_interception_init_finish(void);
void
callback_interception_unintercept(void);
void
callback_interception_exit(void);
void
set_asynch_interception(thread_id_t tid, bool intercept);
bool
intercept_asynch_for_thread(thread_id_t tid, bool intercept_unknown);
bool
intercept_asynch_for_self(bool intercept_unknown);

bool
is_in_interception_buffer(byte *pc);

bool
is_part_of_interception(byte *pc);

bool
is_on_interception_initial_route(byte *pc);

bool
is_syscall_trampoline(byte *pc, byte **tgt);

static inline bool
could_be_hook_occluded_pc(app_pc pc)
{
    extern uint intercept_occlusion_mask;
    return ((((uint)IF_X64((ptr_uint_t)) pc) & intercept_occlusion_mask) ==
            intercept_occlusion_mask);
}

byte *
get_intercept_pc_from_app_pc(app_pc pc, bool occlusions_only, bool exclude_start);

app_pc
get_app_pc_from_intercept_pc(byte *pc);

static inline app_pc
get_app_pc_from_intercept_pc_if_necessary(app_pc pc)
{
    if (is_part_of_interception(pc))
        return get_app_pc_from_intercept_pc(pc);
    return pc;
}

bool
is_intercepted_app_pc(app_pc pc, byte **interception_pc);

/* in inject_shared.c */
#include "inject_shared.h"

/* in ntdll.c, exported through here */
bool
syscalls_init(void);
void
syscalls_init_options_read(void);
int
get_last_error(void);
void
set_last_error(int error);
HANDLE
get_stderr_handle(void);
HANDLE
get_stdout_handle(void);
HANDLE
get_stdin_handle(void);
/* used in certain asserts in arch/interp.c otherwise should be in os_private.h */
bool
use_ki_syscall_routines(void);

wchar_t *
get_application_cmdline(void);
const char *
get_application_short_unqualified_name(void);

/* in syscall.c */
bool
syscall_uses_wow64_index();

bool
syscall_uses_edx_param_base();

/* in loader.c */
/* Handles a private-library FLS callback called from interpreted app code */
bool
private_lib_handle_cb(dcontext_t *dcontext, app_pc pc);
/* our copy of the PEB for isolation (i#249) */
PEB *
get_private_peb(void);
/* these 2 do not cover SWAP_TEB_STACK{LIMIT,BASE}() which are separate */
bool
should_swap_peb_pointer(void);
/* Does not cover TEB.ThreadLocalStoragePointer (use should_swap_teb_static_tls()). */
bool
should_swap_teb_nonstack_fields(void);
bool
should_swap_teb_static_tls(void);
void
loader_pre_client_thread_exit(dcontext_t *dcontext);
bool
is_using_app_peb(dcontext_t *dcontext);
void
swap_peb_pointer(dcontext_t *dcontext, bool to_priv);
/* Meant for use on detach only: restore app values and does not update
 * or swap private values.  Up to caller to synchronize w/ other thread.
 */
void
restore_peb_pointer_for_thread(dcontext_t *dcontext);
void
check_app_stack_limit(dcontext_t *dcontext);

#endif /* _OS_EXPORTS_H_ */
