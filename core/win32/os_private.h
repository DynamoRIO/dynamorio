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
 * os_private.h - declarations shared among os-specific files, but not
 * exported to the rest of the code
 */

#ifndef _OS_PRIVATE_H_
#define _OS_PRIVATE_H_ 1

#include "drmarker.h"

/* in os.c *********************************************************/
#define GLOBAL_NT_PREFIX L"\\??\\"
/* FIXME: used for converting Dos to NT paths */

/* thread-local data that's os-private, for modularity and easy sharing across
 * callbacks
 */
typedef struct _os_thread_data_t {
    /* Store stack info at thread startup since an attack or inadvertent write
     * could clobber the teb fields storing this info.  Also, on NT and 2k the
     * stack is freed in process during kernel32!ExitThread (which uses some
     * unused TEB space as the stack to free oringinal stack and exit the
     * the thread) so we mark teb_stack_no_longer_valid when we see the free
     * (which we watch for). */
    byte *stack_base;
    byte *stack_top;
    bool teb_stack_no_longer_valid;
} os_thread_data_t;

/* pc values delimiting dynamo dll image */
extern app_pc dynamo_dll_start;
extern app_pc dynamo_dll_end;

extern dcontext_t *early_inject_load_helper_dcontext;

/* Passed to early injection init by parent.  Sized to work for any bitwidth. */
typedef struct {
    uint64 app_xax;
    uint64 dr_base;
    uint64 ntdll_base;
    uint64 tofree_base;
    uint64 hook_location;
    uint hook_prot;
    bool late_injection;
    char dynamorio_lib_path[MAX_PATH];
} earliest_args_t;

/* Max size is x64 ind jmp (6 bytes) + target (8 bytes).
 * Simpler to always use the same size, esp wrt cross-arch injection.
 * We assume all our early inject target functions are at least
 * this size.  We restore the hook right away in any case.
 */
#define EARLY_INJECT_HOOK_SIZE 14

bool
is_first_thread_in_new_process(HANDLE process_handle, CONTEXT *cxt);

bool
maybe_inject_into_process(dcontext_t *dcontext, HANDLE process_handle,
                          HANDLE thread_handle, CONTEXT *cxt);

bool
translate_context(thread_record_t *trec, CONTEXT *cxt, bool restore_memory);

void
dump_mbi(file_t file, MEMORY_BASIC_INFORMATION *mbi, bool dump_xml);

void
dump_mbi_addr(file_t file, app_pc target, bool dump_xml);

bool
prot_is_writable(uint prot);

bool
prot_is_executable(uint osprot);

uint
get_current_protection(byte *pc);

/* translate platform independent protection bits to native flags */
int
memprot_to_osprot(uint prot);

/* translate native flags to platform independent protection bits */
int
osprot_to_memprot(uint prot);

int
osprot_add_writecopy(uint prot);

int
process_mmap(dcontext_t *dcontext, app_pc pc, size_t size, bool add,
             const char *filepath);

void
check_for_ldrpLoadImportModule(byte *base, uint *ebp);

void
client_thread_target(void *param);

bool
is_new_thread_client_thread(CONTEXT *cxt, OUT byte **dstack);

bool
os_delete_file_w(const wchar_t *file_name, file_t directory_handle);

byte *
os_terminate_wow64_stack(HANDLE thread_handle);

void
os_terminate_wow64_write_args(bool exit_process, HANDLE proc_or_thread_handle,
                              int exit_status);

void
thread_attach_setup(priv_mcontext_t *mc);

void
thread_attach_context_revert(CONTEXT *cxt INOUT);

/* in syscall.c *********************************************************/

/* this points to a windows-version-specific syscall array */
extern int *syscalls;
#define SYSCALL_NOT_PRESENT 0xffffffff
/* this points to a windows-version-specific WOW table index array */
extern int *wow64_index;

/* i#1230: we support the client adding to the end of these, so they are not const
 * (but they're still in .data, so they're read-only once past init)
 */
#define SYS_CONST /* in .data */
extern int windows_unknown_syscalls[];
extern SYS_CONST int windows_10_1803_x64_syscalls[];
extern SYS_CONST int windows_10_1803_wow64_syscalls[];
extern SYS_CONST int windows_10_1803_x86_syscalls[];
extern SYS_CONST int windows_10_1709_x64_syscalls[];
extern SYS_CONST int windows_10_1709_wow64_syscalls[];
extern SYS_CONST int windows_10_1709_x86_syscalls[];
extern SYS_CONST int windows_10_1703_x64_syscalls[];
extern SYS_CONST int windows_10_1703_wow64_syscalls[];
extern SYS_CONST int windows_10_1703_x86_syscalls[];
extern SYS_CONST int windows_10_1607_x64_syscalls[];
extern SYS_CONST int windows_10_1607_wow64_syscalls[];
extern SYS_CONST int windows_10_1607_x86_syscalls[];
extern SYS_CONST int windows_10_1511_x64_syscalls[];
extern SYS_CONST int windows_10_1511_wow64_syscalls[];
extern SYS_CONST int windows_10_1511_x86_syscalls[];
extern SYS_CONST int windows_10_x64_syscalls[];
extern SYS_CONST int windows_10_wow64_syscalls[];
extern SYS_CONST int windows_10_x86_syscalls[];
extern SYS_CONST int windows_81_x64_syscalls[];
extern SYS_CONST int windows_81_wow64_syscalls[];
extern SYS_CONST int windows_81_x86_syscalls[];
extern SYS_CONST int windows_8_x64_syscalls[];
extern SYS_CONST int windows_8_wow64_syscalls[];
extern SYS_CONST int windows_8_x86_syscalls[];
extern SYS_CONST int windows_7_x64_syscalls[];
extern SYS_CONST int windows_7_syscalls[];
extern SYS_CONST int windows_vista_sp1_x64_syscalls[];
extern SYS_CONST int windows_vista_sp1_syscalls[];
extern SYS_CONST int windows_vista_sp0_x64_syscalls[];
extern SYS_CONST int windows_vista_sp0_syscalls[];
extern SYS_CONST int windows_2003_syscalls[];
extern SYS_CONST int windows_XP_x64_syscalls[];
extern SYS_CONST int windows_XP_wow64_index[]; /* for XP through Win7 */
extern SYS_CONST int windows_XP_syscalls[];
extern SYS_CONST int windows_2000_syscalls[];
extern SYS_CONST int windows_NT_sp3_syscalls[];
extern SYS_CONST int windows_NT_sp0_syscalls[];
extern SYS_CONST int windows_NT_sp4_syscalls[];

/* for x64 this is the # of args */
extern SYS_CONST uint syscall_argsz[];

extern const char *SYS_CONST syscall_names[];

#ifdef DEBUG
void
check_syscall_array_sizes(void);
#endif

bool
windows_version_init(int num_GetContextThread, int num_AllocateVirtualMemory);

enum {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    SYS_##name,
#include "syscallx.h"
#undef SYSCALL
    SYS_MAX,
};

/* the offset from edx of the parameters to a system call, our current
 * (FIXME - also potentially unreliable, since really is a function of os
 * version and processor type) check is by the system entry method,
 * if it's int then offset is 0, if it's sysenter or syscall then offset is 8
 * will also have it default to 0 since I think 2k uses int regardless of
 * proccesor type */
/* FIXME - if we are really paranoid then we should ensure that the offset
 * holds the return values the os would expect (i.e. the ntdll wrapper return
 * address for XP/2003), also if used before we know the syscall method will
 * default to 0! */
#define SYSCALL_PARAM_MAX_OFFSET (2 * XSP_SZ) /* offset for real arguments */
#ifdef X64
/* retaddr, then args */
#    define SYSCALL_PARAM_OFFSET() (XSP_SZ)
#else
/* as done on WinXP, syscalls have extra slots before real params */
/* edx is 4 less than on 2000, plus there's an extra call to provide
 * return address for sysenter, so we have to skip 2 slots:
 */
/* On Win8, wow64 syscalls do not point edx at the params and
 * instead simply use esp and thus must skip the retaddr.
 */
#    define SYSCALL_PARAM_OFFSET()                               \
        ((get_syscall_method() == SYSCALL_METHOD_SYSCALL ||      \
          get_syscall_method() == SYSCALL_METHOD_SYSENTER)       \
             ? SYSCALL_PARAM_MAX_OFFSET                          \
             : ((get_syscall_method() == SYSCALL_METHOD_WOW64 && \
                 !syscall_uses_wow64_index())                    \
                    ? XSP_SZ                                     \
                    : 0))
#endif

static inline reg_t *
sys_param_addr(dcontext_t *dcontext, reg_t *param_base, int num)
{
#ifdef X64
    /* we force-inline get_mcontext() and so don't take it as a param */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    switch (num) {
    /* The first arg was in rcx, but that's clobbered by OP_sysycall, so the wrapper
     * copies it to r10.  We need to use r10 as our own instru sometimes takes
     * advantage of the dead rcx and clobbers it inside the wrapper (i#1901).
     */
    case 0: return &mc->r10;
    case 1: return &mc->xdx;
    case 2: return &mc->r8;
    case 3: return &mc->r9;
    default: return ((reg_t *)param_base) + num;
    }
#else
    return ((reg_t *)param_base) + num;
#endif
}

static inline reg_t
sys_param(dcontext_t *dcontext, reg_t *param_base, int num)
{
    /* sys_param is also called from handle_system_call where dcontext->whereami
     * is not set to DR_WHERE_SYSCALL_HANDLER yet.
     */
    ASSERT(!dcontext->post_syscall);
    return *sys_param_addr(dcontext, param_base, num);
}

static inline reg_t
postsys_param(dcontext_t *dcontext, reg_t *param_base, int num)
{
    ASSERT(dcontext->whereami == DR_WHERE_SYSCALL_HANDLER && dcontext->post_syscall);
#ifdef X64
    switch (num) {
    /* Register params are volatile so we save in dcontext in pre-syscall */
    case 0: return dcontext->sys_param0;
    case 1: return dcontext->sys_param1;
    case 2: return dcontext->sys_param2;
    case 3: return dcontext->sys_param3;
    default: return *sys_param_addr(dcontext, param_base, num);
    }
#else
    return *sys_param_addr(dcontext, param_base, num);
#endif
}

void
syscall_interception_init(void);

void
syscall_interception_exit(void);

void
init_syscall_trampolines(void);

void
exit_syscall_trampolines(void);

bool
os_get_file_size_by_handle(IN HANDLE file_handle, uint64 *end_of_file);
bool
os_set_file_size(IN HANDLE file_handle, uint64 end_of_file);

/* use os_rename_file() for cross-platform uses */
bool
os_rename_file_in_directory(IN HANDLE rootdir, const wchar_t *orig_name,
                            const wchar_t *new_name, bool replace);

/* in callback.c ***************************************************/

/* i#2138: on Win10-x64 extra space is needed for dr_syscall_intercept_natively. */
#define INTERCEPTION_CODE_SIZE IF_X64_ELSE(10 * 4096, 8 * 4096)

/* see notes in intercept_new_thread() about these values */
#define THREAD_START_ADDR IF_X64_ELSE(CXT_XCX, CXT_XAX)
#define THREAD_START_ARG64 CXT_XDX
#define THREAD_START_ARG32 CXT_XBX
#define THREAD_START_ARG IF_X64_ELSE(THREAD_START_ARG64, THREAD_START_ARG32)

void
callback_init(void);

void
callback_exit(void);

dr_marker_t *
get_drmarker(void);

#define UNDER_DYN_HACK 0xab
#define IS_UNDER_DYN_HACK(val) ((byte)(val) == UNDER_DYN_HACK)

byte *
intercept_syscall_wrapper(byte **ptgt_pc /* IN/OUT */, intercept_function_t prof_func,
                          void *callee_arg, after_intercept_action_t action_after,
                          app_pc *skip_syscall_pc /* OUT */,
                          byte **orig_bytes_pc /* OUT */,
                          byte *fpo_stack_adjustment /* OUT OPTIONAL */,
                          const char *name);
byte *
insert_trampoline(byte *tgt_pc, intercept_function_t prof_func, void *callee_arg,
                  bool assume_esp, after_intercept_action_t action_after,
                  bool cti_safe_to_ignore);

void
remove_trampoline(byte *our_pc, byte *tgt_pc);

void
remove_image_entry_trampoline(void);

void
callback_start_return(priv_mcontext_t *mc);

void
intercept_nt_continue(CONTEXT *cxt, int flag);

void
intercept_nt_setcontext(dcontext_t *dcontext, CONTEXT *cxt);

/* methods of taking over */
#define INTERCEPT_POINT(point) point,
#define INTERCEPT_ALL_POINTS                      \
    /* when dr_preinjected=false, not used */     \
    INTERCEPT_POINT(INTERCEPT_EXPLICIT_INJECT)    \
    /* otherwise we are in one of these */        \
    INTERCEPT_POINT(INTERCEPT_PREINJECT)          \
    INTERCEPT_POINT(INTERCEPT_IMAGE_ENTRY)        \
    INTERCEPT_POINT(INTERCEPT_LOAD_DLL)           \
    INTERCEPT_POINT(INTERCEPT_UNLOAD_DLL)         \
    /* asynch prior to image entry */             \
    INTERCEPT_POINT(INTERCEPT_EARLY_ASYNCH)       \
    /* syscall trampoline prior to image entry */ \
    INTERCEPT_POINT(INTERCEPT_SYSCALL)
typedef enum { INTERCEPT_ALL_POINTS } retakeover_point_t;
#undef INTERCEPT_POINT

void
retakeover_after_native(thread_record_t *tr, retakeover_point_t where);

bool
new_thread_is_waiting_for_dr_init(thread_id_t tid, app_pc pc);

void
context_to_mcontext(priv_mcontext_t *mcontext, CONTEXT *cxt);

void
context_to_mcontext_new_thread(priv_mcontext_t *mcontext, CONTEXT *cxt);

void
mcontext_to_context(CONTEXT *cxt, priv_mcontext_t *mcontext, bool set_cur_seg);

#ifdef DEBUG
void
dump_context_info(CONTEXT *context, file_t file, bool all);
#endif

/* PR 264138: we need to preserve xmm0-5 for x64 and wow64.
 * These flags must be used for any CONTEXT that is being used
 * to set a priv_mcontext_t for executing by DR, or if the CONTEXT
 * will be passed to nt_set_context() and the thread in question
 * will execute DR code in between.
 * Although winnt.h mentions CONTEXT_MMX_REGISTERS, there is no such
 * constant: they must mean CONTEXT_FLOATING_POINT.
 * We allow non-core inject.c to use this by not using
 * preserve_xmm_caller_saved() and just ignoring underlying SSE support: so we
 * have some duplication of logic, but it's messy to get preserve_xmm_caller_saved()
 * into arch_exports.h as NT_CURRENT_PROCESS is not defined yet, and
 * non-core modules don't link with proc.c.
 * Since this affects only what we request from the kernel, asking
 * for floating point w/o underlying sse support is not a problem.
 */
#ifdef CONTEXT_XSTATE
#    undef CONTEXT_XSTATE /* defined in VS2008+ */
#endif
/* i#437:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/hh134240(v=vs.85).aspx
 * Win 7 SP1 is the first version of Windows supporting the AVX API.
 * The value for CONTEXT_XSTATE is different between Win 7 and Win 7 SP1.
 * A single MACRO is not enough to set the CONTEXT_XSTATE across different
 * Windows, so we use a global variable instead and set the value at runtime.
 */
extern uint context_xstate;
#define CONTEXT_XSTATE context_xstate
#define CONTEXT_XMM_FLAG IF_X64_ELSE(CONTEXT_FLOATING_POINT, CONTEXT_EXTENDED_REGISTERS)
#define CONTEXT_YMM_FLAG CONTEXT_XSTATE
#define CONTEXT_PRESERVE_XMM IF_X64_ELSE(true, is_wow64_process(NT_CURRENT_PROCESS))
/* AVX is supported only if both hardware and os support it, and this proc
 * check looks at both (i#1278)
 */
#define CONTEXT_PRESERVE_YMM (proc_avx_enabled())
#define CONTEXT_DR_STATE_NO_YMM \
    (CONTEXT_INTEGER | CONTEXT_CONTROL | (CONTEXT_PRESERVE_XMM ? CONTEXT_XMM_FLAG : 0U))
#define CONTEXT_DR_STATE \
    (CONTEXT_DR_STATE_NO_YMM | (CONTEXT_PRESERVE_YMM ? CONTEXT_YMM_FLAG : 0U))
/* FIXME i#444: including CONTEXT_YMM_FLAG blindly results in STATUS_NOT_SUPPORTED in
 * inject_into_thread()'s NtGetContextThread so for now we remove it:
 */
#define CONTEXT_DR_STATE_ALLPROC                            \
    (CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_XMM_FLAG | \
     0 /*CONTEXT_YMM_FLAG: see above*/)

#define XSTATE_HEADER_SIZE 0x40 /* 512 bits */
#define YMMH_AREA(ymmh_area, i) (((dr_xmm_t *)ymmh_area)[i])
#define CONTEXT_DYNAMICALLY_LAID_OUT(flags) (TESTALL(CONTEXT_XSTATE, flags))

enum {
    EXCEPTION_INFORMATION_READ_EXECUTE_FAULT = 0,
    /* On non-NX capable machines Read and Execute faults are not
     * differentiated */
    EXCEPTION_INFORMATION_WRITE_FAULT = 1,
    EXCEPTION_INFORMATION_EXECUTE_FAULT = 8, /* case 5879 */
    /* Only on NX enabled machines Execute faults are differentiated */
};

#ifndef X64
/* x64 CONTEXT, for use from WOW64 32-bit code */

/* XXX: is there a better way to identify pre-7.0 SDK?  It has XSAVE_FORMAT
 * under _AMD64_ so we have to supply it here.  To identify, it doesn't
 * have XSAVE_ALIGN.  Neither does 8.0 SDK, but we rule that out via
 * XSTATE_AVX, which is only defined in 8.0.
 */
#    if !defined(XSAVE_ALIGN) && !defined(XSTATE_AVX)
typedef struct DECLSPEC_ALIGN(16) _M128A {
    ULONGLONG Low;
    LONGLONG High;
} M128A, *PM128A;

typedef struct _XMM_SAVE_AREA32 {
    WORD ControlWord;
    WORD StatusWord;
    BYTE TagWord;
    BYTE Reserved1;
    WORD ErrorOpcode;
    DWORD ErrorOffset;
    WORD ErrorSelector;
    WORD Reserved2;
    DWORD DataOffset;
    WORD DataSelector;
    WORD Reserved3;
    DWORD MxCsr;
    DWORD MxCsr_Mask;
    M128A FloatRegisters[8];
    M128A XmmRegisters[16];
    BYTE Reserved4[96];
} XMM_SAVE_AREA32, *PXMM_SAVE_AREA32;
#    else
typedef XSAVE_FORMAT XMM_SAVE_AREA32, *PXMM_SAVE_AREA32;
#    endif

typedef struct DECLSPEC_ALIGN(16) _CONTEXT_64 {

    //
    // Register parameter home addresses.
    //
    // N.B. These fields are for convience - they could be used to extend the
    //      context record in the future.
    //

    DWORD64 P1Home;
    DWORD64 P2Home;
    DWORD64 P3Home;
    DWORD64 P4Home;
    DWORD64 P5Home;
    DWORD64 P6Home;

    //
    // Control flags.
    //

    DWORD ContextFlags;
    DWORD MxCsr;

    //
    // Segment Registers and processor flags.
    //

    WORD SegCs;
    WORD SegDs;
    WORD SegEs;
    WORD SegFs;
    WORD SegGs;
    WORD SegSs;
    DWORD EFlags;

    //
    // Debug registers
    //

    DWORD64 Dr0;
    DWORD64 Dr1;
    DWORD64 Dr2;
    DWORD64 Dr3;
    DWORD64 Dr6;
    DWORD64 Dr7;

    //
    // Integer registers.
    //

    DWORD64 Rax;
    DWORD64 Rcx;
    DWORD64 Rdx;
    DWORD64 Rbx;
    DWORD64 Rsp;
    DWORD64 Rbp;
    DWORD64 Rsi;
    DWORD64 Rdi;
    DWORD64 R8;
    DWORD64 R9;
    DWORD64 R10;
    DWORD64 R11;
    DWORD64 R12;
    DWORD64 R13;
    DWORD64 R14;
    DWORD64 R15;

    //
    // Program counter.
    //

    DWORD64 Rip;

    //
    // Floating point state.
    //

    union {
        XMM_SAVE_AREA32 FltSave;
        struct {
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0;
            M128A Xmm1;
            M128A Xmm2;
            M128A Xmm3;
            M128A Xmm4;
            M128A Xmm5;
            M128A Xmm6;
            M128A Xmm7;
            M128A Xmm8;
            M128A Xmm9;
            M128A Xmm10;
            M128A Xmm11;
            M128A Xmm12;
            M128A Xmm13;
            M128A Xmm14;
            M128A Xmm15;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;

    //
    // Vector registers.
    //

    M128A VectorRegister[26];
    DWORD64 VectorControl;

    //
    // Special debug control registers.
    //

    DWORD64 DebugControl;
    DWORD64 LastBranchToRip;
    DWORD64 LastBranchFromRip;
    DWORD64 LastExceptionToRip;
    DWORD64 LastExceptionFromRip;
} CONTEXT_64, *PCONTEXT_64;
#endif

/* in module_shared.c */
#ifndef X64
size_t
nt_get_context64_size(void);

bool
thread_get_context_64(HANDLE thread, CONTEXT_64 *cxt64);

bool
thread_set_context_64(HANDLE thread, CONTEXT_64 *cxt64);
#endif

bool
should_inject_into_process(dcontext_t *dcontext, HANDLE process_handle,
                           int *rununder_mask, /* OPTIONAL OUT */
                           inject_setting_mask_t *inject_settings /* OPTIONAL OUT */);

/* in inject.c (also used by dynamo library) ***********************/

/* Note : inject_init calls get_module_handle and therefore should be called
 * during dynamo initialization to avoid race conditions on the loader lock */
void
inject_init(void); /* must be called prior to inject_into_thread(void) */

bool
inject_into_thread(HANDLE phandle, CONTEXT *cxt, HANDLE thandle, char *dynamo_path);

/* inject_location values come from the INJECT_LOCATION_* enum in os_shared.h. */
bool
inject_into_new_process(HANDLE phandle, HANDLE thandle, char *dynamo_path, bool map,
                        uint inject_location, void *inject_address);

/* in <arch.s> (x86.asm for us) ************************************/

void
internal_dynamo_start(void);

void
cleanup_after_interp(void);

void
callback_dynamo_start(void);

void
nt_continue_dynamo_start(void);

/* x86.asm custom routine used only for check_for_modified_code() */
void
call_modcode_alt_stack(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                       app_pc target, uint flags, bool using_initstack,
                       fragment_t *fragment);

/* x86.asm routine used for injection */
void
load_dynamo(void);

/* in eventlog.c ***************************************************/

void
eventlog_init(void);

void
eventlog_fast_exit(void);
void
eventlog_slow_exit(void);

/* in module.c *****************************************************/

#ifdef X64
#    ifndef __IMAGE_UNWIND_INFO
#        define __IMAGE_UNWIND_INFO
#        pragma warning(push)
#        pragma warning(disable : 4214) /* allow byte-sized bitfields */
/* These definitions are needed to parse exception handlers to add to the RCT
 * table as part of PR 250395.  These definitions aren't found in any header
 * files.  These seem to be coming directly from internal sources.  I saw a
 * definition for RUNTIME_FUNCTION in winternal.h which was identical to
 * IMAGE_RUNTIME_FUNCTION_ENTRY, which under a big comment block saying that it
 * is for internal windows use only and might change from release to release, so
 * using externally visible ones and declaring those that aren't available.
 *
 * These are based on the Microsoft specifications and suggested
 * C defines at http://msdn2.microsoft.com/en-us/library/ssa62fwe(VS.80).aspx
 * Since these are not in any header files we use our own style conventions.
 */
typedef enum _unwind_opcode_t {
    UWOP_PUSH_NONVOL = 0, /* info == register number */
    UWOP_ALLOC_LARGE,     /* no info, alloc size in next 2 slots */
    UWOP_ALLOC_SMALL,     /* info == size of allocation / 8 - 1 */
    UWOP_SET_FPREG,       /* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
    UWOP_SAVE_NONVOL,     /* info == register number, offset in next slot */
    UWOP_SAVE_NONVOL_FAR, /* info == register number, offset in next 2 slots */
    UWOP_SAVE_XMM128,     /* info == XMM reg number, offset in next slot */
    UWOP_SAVE_XMM128_FAR, /* info == XMM reg number, offset in next 2 slots */
    UWOP_PUSH_MACHFRAME   /* info == 0: no error-code, 1: error-code */
} unwind_opcode_t;

typedef union _unwind_code_t {
    struct {
        byte CodeOffset;
        byte UnwindOp : 4;
        byte OpInfo : 4;
    };
    USHORT FrameOffset;
} unwind_code_t;

#        ifndef UNW_FLAG_EHANDLER
#            define UNW_FLAG_EHANDLER 0x01
#            define UNW_FLAG_UHANDLER 0x02
#            define UNW_FLAG_CHAININFO 0x04
#        endif

typedef struct _unwind_info_t {
    byte Version : 3;
    byte Flags : 5;
    byte SizeOfProlog;
    byte CountOfCodes;
    byte FrameRegister : 4;
    byte FrameOffset : 4;
    unwind_code_t UnwindCode[1];
#        if 0 /* variable-length tail of struct */
    /* MSDN uses "((CountOfCodes + 1) & ~1)" which is just align-forward-2,
     * used b/c the UnwindCode array must always have an even capacity */
    unwind_code_t MoreUnwindCode[ALIGN_FORWARD(CountOfCodes, 2) - 1];
    union {
        OPTIONAL ULONG ExceptionHandler;
        OPTIONAL ULONG FunctionEntry;
    };
    OPTIONAL ULONG ExceptionData[];
#        endif
} unwind_info_t;

/* Address of field that's after the unwind code data */
#        define UNWIND_INFO_PTR_ADDR(info) \
            ((void *)&(info)->UnwindCode[ALIGN_FORWARD((info)->CountOfCodes, 2)])

/* Field that's after the unwind code data, treated as an RVA */
#        define UNWIND_INFO_PTR_RVA(info) (*(uint *)UNWIND_INFO_PTR_ADDR(info))

/* ExceptionData field (2nd one after the unwind code data) */
#        define UNWIND_INFO_DATA_ADDR(info) (((uint *)UNWIND_INFO_PTR_ADDR(info)) + 1)
#        define UNWIND_INFO_DATA_RVA(info) (*(((uint *)UNWIND_INFO_PTR_ADDR(info)) + 1))

/* ExceptionData takes this form.  It is inlined according to my observation
 * but it may instead be pointed at by an RVA.
 */
typedef struct _scope_table_t {
    ULONG Count;
    struct {
        ULONG BeginAddress;
        ULONG EndAddress;
        ULONG HandlerAddress;
        ULONG JumpTarget;
    } ScopeRecord[1];
} scope_table_t;

#        pragma warning(pop)
#    endif /* __IMAGE_UNWIND_INFO */
#endif     /* X64 */

#define RVA_TO_VA(base, rva) ((ptr_uint_t)(base) + (ptr_uint_t)(rva))

bool
is_readable_pe_base(app_pc base);

bool
get_module_info_pe(const app_pc module_base, uint *checksum, uint *timestamp,
                   size_t *image_size, char **pe_name, size_t *code_size);

/* Use get_module_short_name for arbitrary pcs -- only call this if
 * you KNOW this is the base addr of a non-executable module, as it
 * bypasses some safety checks in get_module_short_name to avoid 4
 * system calls.
 */
char *
get_dll_short_name(app_pc base_addr);

/* Use get_module_short_name in general */
const char *
get_module_short_name_uncached(dcontext_t *dcontext, app_pc base,
                               bool at_map HEAPACCT(which_heap_t which));

app_pc
get_module_entry(app_pc module_base);

uint
get_module_characteristics(app_pc module_base);

bool
module_has_cor20_header(app_pc module_base);

bool
module_is_32bit(app_pc module_base);

bool
module_is_64bit(app_pc module_base);

#ifdef DEBUG

enum { SYMBOLS_LOGLEVEL = 1 };

int
loaded_modules_exports(void);

int
remove_module_info(app_pc start, size_t size);

int
add_module_info(app_pc base_addr, size_t size);

void
module_cleanup(void);

void
module_info_exit(void);

#endif /* DEBUG */

bool
module_file_relocatable(app_pc module_base);

bool
module_rebase(app_pc module_base, size_t module_size, ssize_t relocation_delta,
              bool protect_incrementally);
bool
module_dump_pe_file(HANDLE new_file, app_pc module_base, size_t module_size);

bool
module_contents_compare(app_pc original_module_base, app_pc suspect_module_base,
                        size_t matching_module_size, bool relocated,
                        ssize_t relocation_delta, size_t validation_section_prefix);

bool
module_make_writable(app_pc module_base, size_t module_size);

bool
aslr_compare_header(app_pc original_module_base, size_t original_header_len,
                    app_pc suspect_module_base);

bool
get_executable_segment(app_pc module_base, app_pc *sec_start /* OPTIONAL OUT */,
                       app_pc *sec_end /* OPTIONAL OUT */,
                       app_pc *sec_end_nopad /* OPTIONAL OUT */);

/* Returns a dr_strdup-ed string which caller must dr_strfree w/ ACCT_VMAREAS */
const char *
section_to_file_lookup(HANDLE section_handle);

bool
section_to_file_add_wide(HANDLE section_handle, const wchar_t *file_path);

bool
section_to_file_add(HANDLE section_handle, const char *file_path);

bool
section_to_file_remove(HANDLE section_handle);

bool
module_get_tls_info(app_pc module_base, OUT void ***callbacks, OUT int **index,
                    OUT byte **data_start, OUT byte **data_end);

/* in aslr.c */
const wchar_t *
get_file_short_name(IN HANDLE file_handle, IN OUT FILE_NAME_INFORMATION *name_info);

void
aslr_set_last_section_file_name(dcontext_t *dcontext, const wchar_t *short_name);

/* in os.c */
bool
safe_write(void *base, size_t size, const void *in_buf);

/* Note that we should keep an eye for potential additional qualifier
 * flags.  Alternatively we may simply mask off ~0xff to allow for any
 * future flags added here.
 */
#define PAGE_PROTECTION_QUALIFIERS (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)

char *
prot_string(uint prot);

/* FIXME: should we try to alert any dynamo running the other process?
 * Refer new instances to Case 68
 */
#define IPC_ALERT(...) SYSLOG_INTERNAL_WARNING_ONCE("IPC ALERT " __VA_ARGS__)

const PSID
get_process_primary_SID(void);

bool
convert_NT_to_Dos_path(OUT wchar_t *buf, IN const wchar_t *fname,
                       IN size_t buf_len /*# elements*/);

size_t
nt_get_context_size(DWORD flags);

size_t
nt_get_max_context_size(void);

CONTEXT *
nt_initialize_context(char *buf, size_t buf_len, DWORD flags);

byte *
context_ymmh_saved_area(CONTEXT *cxt);

#ifndef NOT_DYNAMORIO_CORE_PROPER /* b/c of global_heap_* */
/* always null-terminates when it returns non-NULL */
wchar_t *
convert_to_NT_file_path_wide(OUT wchar_t *fixedbuf, IN const wchar_t *fname,
                             IN size_t fixedbuf_len /*# elements*/,
                             OUT size_t *allocbuf_sz /*#bytes*/);

void
convert_to_NT_file_path_wide_free(IN wchar_t *alloc, IN size_t alloc_buf_sz /*#bytes*/);
#endif

/* always null-terminates when it returns true */
bool
convert_to_NT_file_path(OUT wchar_t *buf, IN const char *fname,
                        IN size_t buf_len /*# elements*/);

/* in loader.c */

/* early injection bootstrapping */
bool
privload_bootstrap_dynamorio_imports(byte *dr_base, byte *ntdll_base);

bool
bootstrap_protect_virtual_memory(void *base, size_t size, uint prot, uint *old_prot);

/* in ntdll.c, set via arg from parent for earliest inj */
void
set_ntdll_base(app_pc base);

/* in diagnost.c */
byte *
get_system_processes(OUT uint *info_bytes_needed);

#endif /* _OS_PRIVATE_H_ */
