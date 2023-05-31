/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * callback.c - windows-specific callback, APC, and exception handling routines
 */

/* This whole file assumes x86 */
#include "configure.h"
#ifndef X86
#    error X86 must be defined
#endif

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "../monitor.h"
#include "../fcache.h"
#include "../fragment.h"
#include "decode_fast.h"
#include "disassemble.h"
#include "instr_create_shared.h"
#include "ntdll.h"
#include "events.h"
#include "os_private.h"
#include "../moduledb.h"
#include "aslr.h"
#include "../nudge.h" /* for generic_nudge_target() address */
#ifdef RETURN_AFTER_CALL
#    include "../rct.h" /* for rct_ind_branch_target_lookup */
#endif
#include "instrument.h"
#include "../perscache.h"
#include "../translate.h"

#include <windows.h>

/* forward declarations */
static dcontext_t *
callback_setup(app_pc next_pc);
static byte *
insert_image_entry_trampoline(dcontext_t *dcontext);
static void
swap_dcontexts(dcontext_t *done, dcontext_t *dtwo);
static void
asynch_take_over(app_state_at_intercept_t *state);

/* currently we do not intercept top level exceptions */
#ifdef INTERCEPT_TOP_LEVEL_EXCEPTIONS
/* the app's top-level exception handler */
static LPTOP_LEVEL_EXCEPTION_FILTER app_top_handler;
#endif

/* All of our hooks use landing pads to then indirectly target
 * this interception code, which in turn assumes it can directly
 * reach our hook targets in the DR lib.  Thus, we want this
 * interception buffer to not be in vmcode nor vmheap, but near the
 * DR lib: which is simplest with a static array.
 * We write-protect this, so we don't need the ASLR of our heap.
 */
ALIGN_VAR(4096) static byte interception_code_array[INTERCEPTION_CODE_SIZE];

/* interception information
 * if it weren't for syscall trampolines this could be a single page
 * Note: if you add more intercept points, make sure to adjust
 * NUM_INTERCEPT_POINTS below.
 */
static byte *interception_code = NULL;
static byte *interception_cur_pc = NULL;
static byte *ldr_init_pc = NULL;
static byte *callback_pc = NULL;
static byte *apc_pc = NULL;
static byte *exception_pc = NULL;
static byte *raise_exception_pc = NULL;
static byte *after_callback_orig_pc = NULL;
static byte *after_apc_orig_pc = NULL;
static byte *load_dll_pc = NULL;
static byte *unload_dll_pc = NULL;
static byte *image_entry_pc = NULL;
static byte *image_entry_trampoline = NULL;
static byte *syscall_trampolines_start = NULL;
static byte *syscall_trampolines_end = NULL;

/* We rely on the compiler doing the right thing
   so when we dereference an imported function we get its real address
   instead of a stub in our module. The loader does the rest of the magic.
*/
GET_NTDLL(KiUserApcDispatcher,
          (IN PVOID Unknown1, IN PVOID Unknown2, IN PVOID Unknown3, IN PVOID ContextStart,
           IN PVOID ContextBody));
GET_NTDLL(KiUserCallbackDispatcher,
          (IN PVOID Unknown1, IN PVOID Unknown2, IN PVOID Unknown3));
GET_NTDLL(KiUserExceptionDispatcher, (IN PVOID Unknown1, IN PVOID Unknown2));
GET_NTDLL(KiRaiseUserExceptionDispatcher, (void));

/* generated routine for taking over native threads */
byte *thread_attach_takeover;

static byte *
emit_takeover_code(byte *pc);

/* For detach */
volatile bool init_apc_go_native = false;
volatile bool init_apc_go_native_pause = false;

/* overridden by dr_preinjected, or retakeover_after_native() */
static retakeover_point_t interception_point = INTERCEPT_PREINJECT;

/* While emiting the trampoline, the alt. target is unknown for hotp_only. */
#define CURRENTLY_UNKNOWN ((byte *)(ptr_uint_t)0xdeadc0de)

#ifdef DEBUG
#    define INTERCEPT_POINT(point) STRINGIFY(point),
static const char *const retakeover_names[] = { INTERCEPT_ALL_POINTS };
#    undef INTERCEPT_POINT
#endif

/* We keep a list of mappings from intercept points to original app PCs */
typedef struct _intercept_map_elem_t {
    byte *interception_pc;
    app_pc original_app_pc;
    size_t displace_length; /* includes jmp back */
    size_t orig_length;
    bool hook_occludes_instrs; /* i#1632: hook replaced instr(s) of differing length */
    struct _intercept_map_elem_t *next;
} intercept_map_elem_t;

typedef struct _intercept_map_t {
    intercept_map_elem_t *head;
    intercept_map_elem_t *tail;
} intercept_map_t;

static intercept_map_t *intercept_map;

/* i#1632 mask for quick detection of app code pages that may contain intercept hooks. */
ptr_uint_t intercept_occlusion_mask = ~((ptr_uint_t)0);

DECLARE_CXTSWPROT_VAR(static mutex_t map_intercept_pc_lock,
                      INIT_LOCK_FREE(map_intercept_pc_lock));

DECLARE_CXTSWPROT_VAR(static mutex_t emulate_write_lock,
                      INIT_LOCK_FREE(emulate_write_lock));

DECLARE_CXTSWPROT_VAR(static mutex_t exception_stack_lock,
                      INIT_LOCK_FREE(exception_stack_lock));

DECLARE_CXTSWPROT_VAR(static mutex_t intercept_hook_lock,
                      INIT_LOCK_FREE(intercept_hook_lock));

/* Only used for Vista, new threads start directly here instead of going
 * through KiUserApcDispatcher first. Isn't in our lib (though is exported
 * on 2k, xp and vista at least) so we get it dynamically. */
static byte *LdrInitializeThunk = NULL;
/* On vista this is the address the kernel sets (via NtCreateThreadEx, used by all the
 * api routines) as Xip in the context the LdrInitializeThunk NtContinue's to (is eqv.
 * to the unexported kernel32!Base[Process,Thread]StartThunk in pre-Vista).  Fortunately
 * ntdll!RtlUserThreadStart is exported and we cache it here for use in
 * intercept_new_thread().  Note that threads created by the legacy native
 * NtCreateThread don't have to target this address. */
static byte *RtlUserThreadStart = NULL;

#ifndef X64
/* Used to create a clean syscall wrapper on win8 where there's no ind call */
static byte *KiFastSystemCall = NULL;
#endif

/* i#1443: we need to identify threads queued up waiting for DR init.
 * We can't use heap of course so we have to use a max count.
 * We've never seen more than one at a time, but the api.detach_spawn test
 * has 100 of them.
 */
#define MAX_THREADS_WAITING_FOR_DR_INIT 128
/* We assume INVALID_THREAD_ID is 0 (checked in callback_init()). */
/* These need to be neverprot for use w/ new threads.  The risk is small. */
DECLARE_NEVERPROT_VAR(
    static thread_id_t threads_waiting_for_dr_init[MAX_THREADS_WAITING_FOR_DR_INIT],
    { 0 });
/* This is also the next index+1 into the array to write to, incremented atomically. */
DECLARE_NEVERPROT_VAR(static uint threads_waiting_count, 0);

static inline app_pc
get_setcontext_interceptor()
{
    return (app_pc)nt_continue_dynamo_start;
}

/* if tid != self, must hold thread_initexit_lock */
void
set_asynch_interception(thread_id_t tid, bool intercept)
{
    /* Needed to turn on and off asynchronous event interception
     * for non-entire-application-under-dynamo-control situations
     */
    thread_record_t *tr = thread_lookup(tid);
    ASSERT(tr != NULL);
    tr->under_dynamo_control = intercept;
}

static inline bool
intercept_asynch_global()
{
    return (intercept_asynch && !INTERNAL_OPTION(nullcalls));
}

/* if tr is not for calling thread, must hold thread_initexit_lock */
static bool
intercept_asynch_common(thread_record_t *tr, bool intercept_unknown)
{
    if (!intercept_asynch_global())
        return false;
    if (tr == NULL) {
        if (intercept_unknown)
            return true;
        /* caller should have made all attempts to get tr */
        if (control_all_threads) {
            /* we should know about all threads! */
            SYSLOG_INTERNAL_WARNING("Received asynch event for unknown thread " TIDFMT "",
                                    d_r_get_thread_id());
            /* try to make everything run rather than assert -- just do
             * this asynch natively, we probably received it for a thread that's
             * been created but not scheduled?
             */
        }
        return false;
    }
    /* FIXME: under_dynamo_control should be an enum w/ separate
     * values for 1) truly native, 2) under DR but currently native_exec,
     * 3) temporarily native b/c DR lost control (== UNDER_DYN_HACK), and
     * 4) fully under DR
     */
    DOSTATS({
        if (IS_UNDER_DYN_HACK(tr->under_dynamo_control))
            STATS_INC(num_asynch_while_lost);
    });
    return (tr->under_dynamo_control || IS_CLIENT_THREAD(tr->dcontext));
}

/* if tid != self, must hold thread_initexit_lock */
bool
intercept_asynch_for_thread(thread_id_t tid, bool intercept_unknown)
{
    /* Needed to turn on and off asynchronous event interception
     * for non-entire-application-under-dynamo-control situations
     */
    thread_record_t *tr = thread_lookup(tid);
    return intercept_asynch_common(tr, intercept_unknown);
}

bool
intercept_asynch_for_self(bool intercept_unknown)
{
    /* To avoid problems with the all_threads_lock required to look
     * up a thread in the thread table, we first see if it has a
     * dcontext, and if so we get the thread_record_t from there.
     * If not, it probably is a native thread and grabbing the lock
     * should cause no problems as it should not currently be holding
     * any locks.
     */
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (dcontext != NULL)
        return intercept_asynch_common(dcontext->thread_record, intercept_unknown);
    else
        return intercept_asynch_for_thread(d_r_get_thread_id(), intercept_unknown);
}

/***************************************************************************
 * INTERCEPTION CODE FOR TRAMPOLINES INSERTED INTO APPLICATION CODE

interception code either assumes that the app's xsp is valid, or uses
dstack if available, or as a last resort uses d_r_initstack.  when using
d_r_initstack, must make sure all paths exiting handler routine clear the
initstack_mutex once not using the d_r_initstack itself!

We clobber TIB->PID, which is believed to be safe since no user-mode
code will execute there b/c thread is not alertable, and the kernel
shouldn't be reading (and trusting) user mode TIB structures.
FIXME: allocate and use a TIB scratch slot instead

N.B.: this interception code, if encountered by DR, is let run natively,
so make sure DR takes control at the end!

For trying to use the dstack, we have to be careful and check if we're
already on the dstack, which can happen for internal exceptions --
hopefully not for callbacks or apcs, we should assert on that =>
FIXME: add such checks to the cb and apc handlers, and split dstack
check as a separate parameter, once we make cbs and apcs not
assume_xsp (they still do for now since we haven't tested enough to
convince ourselves we never get them while on the dstack)

Unfortunately there's no easy way to check w/o modifying flags, so for
now we assume eflags whenever we do not assume xsp, unless we assume
we're not on the dstack.
Assumption should be ok for Ki*, also for Ldr*.

Alternatives: check later when we're in exception handler, only paths
there are terminate or forge exception.  Thus we can get away w/o
reading anything on stack placed by kernel, but we won't have clean
call stack or anything else for diagnostics, and we'll have clobbered
the real xsp in the mcontext slot, which we use for forging the
exception.

Could perhaps use whereami==DR_WHERE_FCACHE, but could get exception during
clean call or cxt switch when on dstack but prior to whereami change.

Note: the app registers passed to the handler are restored when going back to
      the app, which means any changes made by the handler will be reflected
      in the app state;
      FIXME: change handler prototype to make all registers volatile so that the
      compiler doesn't clobber them; for now it is the user's responsibility.

if (!assume_xsp)
      mov xcx, fs:$PID_TIB_OFFSET  # save xcx
      mov fs:$TLS_DCONTEXT_OFFSET, xcx
      jecxz no_local_stack
 if (!assume_not_on_dstack)
      # need to check if already on dstack
      # assumes eflags!
      mov $DSTACK(xcx), xcx
      cmp xsp, xcx
      jge not_on_dstack
      lea -DYNAMORIO_STACK_SIZE(xcx), xcx
      cmp xsp, xcx
      jl  not_on_dstack
      # record stack method: using dstack/d_r_initstack unmodified
      push xsp
      push $2
      jmp have_stack_now
  not_on_dstack:
      mov fs:$TLS_DCONTEXT_OFFSET, xcx
 endif
      # store app xsp in dcontext & switch to dstack; this will be used to save
      # app xsp on the switched stack, i.e., dstack; not used after that.
      # i#1685: we use the PC slot as it won't affect a new thread that is in the
      # middle of init on the d_r_initstack and came here during client code.
    if TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)
      mov $MCONTEXT_OFFSET(xcx), xcx
    endif
      mov xsp, $PC_OFFSET(xcx)
    if TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)
      mov fs:$TLS_DCONTEXT_OFFSET, xcx
    endif
      mov $DSTACK(xcx), xsp

      # now get the app xsp from the dcontext and put it on the dstack; this
      # will serve as the app xsp cache and will be used to send the correct
      # app xsp to the handler and to restore app xsp at exit
    if TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)
      mov $MCONTEXT_OFFSET(xcx), xcx
    endif
      mov $PC_OFFSET(xcx), xcx
      push xcx

      # need to record stack method, since dcontext could change in handler
      push $1
      jmp have_stack_now
  no_local_stack:
      # use d_r_initstack -- it's a global synch, but should only have no
      # dcontext for initializing thread (where we actually use the app's
      # stack) or exiting thread
      # If we are already on the d_r_initstack, should just continue to use it.
      # need to check if already on d_r_initstack
      # assumes eflags, but we already did on this path for checking dstack
      mov $INITSTACK, xcx
      cmp xsp, xcx
      jge grab_initstack
      lea -DYNAMORIO_STACK_SIZE(xcx), xcx
      cmp xsp, xcx
      jl  grab_initstack
      push xsp
      # record stack method: using dstack/d_r_initstack unmodified
      push $2
      jmp have_stack_now
    grab_initstack:
      mov $1, ecx # upper 32 bits zeroed on x64
     if x64 # can't directly address initstack_mutex or initstack_app_xsp
            # (though we could use rip-relative, nice to not have reachability issues
            # if located far from dynamorio.dll, for general hooks (PR 250294)!)
      # if a new thread we can't easily (w/o syscall) replace tid, so we use peb
      mov xax, fs:$PEB_TIB_OFFSET  # save xax
     endif
    get_lock:
     if x64 # can't directly address initstack_mutex or initstack_app_xsp
      mov $INITSTACK_MUTEX, xax
     endif
      # initstack_mutex.lock_requests is 32-bit
      xchg ecx, IF_X64_ELSE((xax), initstack_mutex)
      jecxz have_lock
      pause  # improve spin-loop perf on P4
      jmp get_lock # no way to sleep or anything, must spin
    have_lock:
      # app xsp is saved in initstack_app_xsp only so that it can be accessed after
      # switching to d_r_initstack; used only to set up the app xsp on the d_r_initstack
     if x64 # we don't need to set initstack_app_xsp, just push the app xsp value
      mov xsp, xcx
      mov d_r_initstack, xax
      xchg xax, xsp
      push xcx
     else
      mov xsp, initstack_app_xsp
      mov d_r_initstack, xsp
      push initstack_app_xsp
     endif
      # need to record stack method, since dcontext could change in handler
      push $0
     if x64
      mov $peb_ptr, xax
      xchg fs:$PEB_TIB_OFFSET, xax  # restore xax and peb ptr
     endif
  have_stack_now:
     if x64
      mov $global_pid, xcx
      xchg fs:$PID_TIB_OFFSET, xcx  # restore xcx and pid
     else
      mov fs:$PID_TIB_OFFSET, xcx  # restore xcx
      mov $global_pid, fs:$PID_TIB_OFFSET  # restore TIB PID
     endif
else
      push xsp # cache app xsp so that it can be used to send the right value
               # to the handler and to restore app xsp safely at exit
      push $3  # recording stack type when using app stack
endif
      # we assume here that we've done two pushes on the stack,
      # which combined w/ the push0 and pushf give us 16-byte alignment
      # for 32-bit and 64-bit prior to push-all-regs
  clean_call_setup:
      # lay out pc, eflags, regs, etc. in app_state_at_intercept_t order
      push $0 # pc slot; unused; could use instead of state->start_pc
      pushf
      pusha (or push all regs for x64)
      push $0 # ASSUMPTION: clearing, not preserving, is good enough
              # FIXME: this won't work at CPL0 if we ever run there!
      popf

      # get the cached app xsp and write it to pusha location,
      # so that the handler gets the correct app xsp
      mov sizeof(priv_mcontext_t)+XSP_SZ(xsp), xax
      mov xax, offsetof(priv_mcontext_t, xsp)(xsp)

if (ENTER_DR_HOOK != NULL)
      call ENTER_DR_HOOK
endif
    if x64
      mov no_cleanup, xax
      push xax
      mov handler_arg, xax
      push xax
    else
      push no_cleanup
      push handler_arg
    endif
      # now we've laid out app_state_at_intercept_t on the stack
      push/mov xsp      # a pointer to the pushed values; this is the argument;
                        # see case 7597.  may be passed in a register.
      call handler
      <clean up args>
      lea 2*XSP_SZ(xsp), lea # pop handler_arg + no_cleanup
if (AFTER_INTERCEPT_DYNAMIC_DECISION)
      cmp xax, AFTER_INTERCEPT_LET_GO
      je let_go
 if (alternate target provided)
      cmp xax, AFTER_INTERCEPT_LET_GO_ALT_DYN
      je let_go_alt
 endif
endif
if (AFTER_INTERCEPT_TAKE_OVER || AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT
      || AFTER_INTERCEPT_DYNAMIC_DECISION)
    if x64
      mov no_cleanup, xax
      push xax
    else
      push no_cleanup # state->start_pc
    endif
      push/mov xsp # app_state_at_intercept_t *
      call asynch_take_over
      # should never reach here
      push $0
      push $-3  # d_r_internal_error will report -3 as line number
      push $0
      call d_r_internal_error
endif
if (AFTER_INTERCEPT_DYNAMIC_DECISION && alternate target provided)
  let_go_alt:
      <complete duplicate of let_go, but ending in a jmp to alternate target>
      <(cannot really share much of let_go cleanup w/o more scratch slots)>
      <(has to be first since original app instrs are placed by caller, not us)>
endif
if (!AFTER_INTERCEPT_TAKE_OVER)
  let_go:
 if (EXIT_DR_HOOK != NULL)
      call EXIT_DR_HOOK
 endif
      # get the xsp passed to the handler, which may have been
      # changed; store it in the xsp cache to restore at exit
      mov offsetof(priv_mcontext_t, xsp)(xsp), xax
      mov xax, sizeof(priv_mcontext_t)+XSP_SZ(xsp)
      popa # or pop all regs on x64
      popf
      lea XSP_SZ(xsp), xsp # clear pc slot
 if (!assume_xsp)
      mov xcx, fs:$PID_TIB_OFFSET  # save xcx
      pop xcx # get back const telling stack used
      pop xsp
      jecxz restore_initstack
      jmp done_restoring
  restore_initstack:
    if x64
      mov &initstack_mutex, xcx
      mov $0, (xcx)
    else
      mov $0, initstack_mutex
    endif
  done_restoring:
    if x64
      mov $global_pid, xcx
      xchg fs:$PID_TIB_OFFSET, xcx  # restore xcx and pid
    else
      mov fs:$PID_TIB_OFFSET, xcx  # restore xcx
      mov $global_pid, fs:$PID_TIB_OFFSET  # restore TIB PID
    endif
 else
      lea XSP_SZ(xsp), xsp       # clear out the stack type
      pop xsp  # handler may have changed xsp; so get it from the xsp cache
 endif
endif (!AFTER_INTERCEPT_TAKE_OVER)
  no_cleanup:
      <original app instructions>

=> handler signature, exported as typedef intercept_function_t:
void handler(app_state_at_intercept_t *args)

if AFTER_INTERCEPT_TAKE_OVER, then asynch_take_over is called.

handler must make sure all paths exiting handler routine clear the
initstack_mutex once not using the d_r_initstack itself!

*/

#define APP instrlist_append

/* common routine separate since used for let go and alternate let go */
static void
insert_let_go_cleanup(dcontext_t *dcontext, byte *pc, instrlist_t *ilist,
                      instr_t *decision, bool assume_xsp, bool assume_not_on_dstack,
                      after_intercept_action_t action_after)
{
    instr_t *first = NULL;
    if (action_after == AFTER_INTERCEPT_DYNAMIC_DECISION) {
        /* placeholder so can find 1st of this path */
        first = instrlist_last(ilist);
    }

    if (EXIT_DR_HOOK != NULL) {
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args.
         */
        IF_DEBUG(bool direct =)
        dr_insert_call_ex((void *)dcontext, ilist, NULL /*append*/,
                          /* we're not in vmcode, so avoid indirect call */
                          pc, (void *)EXIT_DR_HOOK, 0);
        ASSERT(direct);
    }

    /* Get the app xsp passed to the handler from the popa location and store
     * it in the app xsp cache; this is because the handler could have changed
     * the app xsp that was passed to it.  CAUTION: do this before the popa.
     */
    APP(ilist,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                            OPND_CREATE_MEMPTR(REG_XSP, offsetof(priv_mcontext_t, xsp))));
    APP(ilist,
        INSTR_CREATE_mov_st(dcontext,
                            OPND_CREATE_MEMPTR(REG_XSP, sizeof(priv_mcontext_t) + XSP_SZ),
                            opnd_create_reg(REG_XAX)));
    /* now restore everything */
    insert_pop_all_registers(dcontext, NULL, ilist, NULL, XSP_SZ /*see push_all use*/);

    if (action_after == AFTER_INTERCEPT_DYNAMIC_DECISION) {
        /* now that instrs are there, take 1st */
        ASSERT(first != NULL);
        instr_set_target(decision, opnd_create_instr(instr_get_next(first)));
    }

    if (!assume_xsp) {
        instr_t *restore_initstack = INSTR_CREATE_label(dcontext);
        instr_t *done_restoring = INSTR_CREATE_label(dcontext);
        APP(ilist,
            INSTR_CREATE_mov_st(dcontext,
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR),
                                opnd_create_reg(REG_XCX)));
        APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX)));

        /* popa doesn't restore xsp; the handler might have changed it, so
         * restore it from the app xsp cache, which is now the top of stack.
         */
        APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSP)));
        APP(ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(restore_initstack)));
        APP(ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(done_restoring)));
        /* use d_r_initstack to avoid any assumptions about app xsp */
        APP(ilist, restore_initstack);
#ifdef X64
        APP(ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR((ptr_uint_t)&initstack_mutex)));
#endif
        APP(ilist,
            INSTR_CREATE_mov_st(
                dcontext,
                IF_X64_ELSE(OPND_CREATE_MEM32(REG_XCX, 0),
                            OPND_CREATE_ABSMEM((void *)&initstack_mutex, OPSZ_4)),
                OPND_CREATE_INT32(0)));
        APP(ilist, done_restoring);
#ifdef X64
        /* we could perhaps assume the top 32 bits of win32_pid are zero, but
         * xchg works just as well */
        APP(ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR((ptr_uint_t)win32_pid)));
        APP(ilist,
            INSTR_CREATE_xchg(dcontext,
                              opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                        PID_TIB_OFFSET, OPSZ_PTR),
                              opnd_create_reg(REG_XCX)));
#else
        APP(ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR)));
        APP(ilist,
            INSTR_CREATE_mov_st(dcontext,
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR),
                                OPND_CREATE_INTPTR(win32_pid)));
#endif
    } else {
        /* popa doesn't restore xsp; the handler might have changed it, so
         * restore it from the app xsp cache, which is now the top of stack.
         */
        APP(ilist,
            INSTR_CREATE_lea(
                dcontext, opnd_create_reg(REG_XSP),
                opnd_create_base_disp(REG_XSP, REG_NULL, 0, XSP_SZ, OPSZ_0)));
        APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSP)));
    }
}

/* Emits a landing pad (shown below) and returns the address to the first
 * instruction in it.  Also returns the address where displaced app
 * instrs should be copied in displaced_app_loc.
 *
 * The caller must call finalize_landing_pad_code() once finished copying
 * the displaced app code, passing in the changed_prot value it received
 * from this routine.
 *
 *  CAUTION: These landing pad layouts are assumed in intercept_call() and in
 *           read_and_verify_dr_marker(), must_not_be_elided(), and
 *           is_syscall_trampoline().
 *ifndef X64
 *  32-bit landing pad:
 *      jmp tgt_pc          ; 5 bytes, 32-bit relative jump
 *      displaced app instr(s) ; < (JMP_LONG_LENGTH + MAX_INSTR_LENGTH) bytes
 *      jmp after_hook_pc   ; 5 bytes, 32-bit relative jump
 *else
 *  64-bit landing pad:
 *      tgt_pc              ; 8 bytes of absolute address, i.e., tgt_pc
 *      jmp [tgt_pc]        ; 6 bytes, 64-bit absolute indirect jmp
 *      displaced app instr(s) ; < (JMP_LONG_LENGTH + MAX_INSTR_LENGTH) bytes
 *      jmp after_hook_pc   ; 5 bytes, 32-bit relative jump
 *endif
 *
 * Note: For 64-bit landing pad, tgt_pc can be stored at the bottom of the
 * trampoline too.  I chose the top because it helps avoid a minor reachability
 * problem: iff the landing pad is allocated at the topmost part of the
 * reachability region for a given addr_to_hook, then there is a possibility
 * that the return jmp from the landing pad may not reach the instruction after
 * the hook address.  This is because the number of bytes of the hook (5 bytes)
 * and the number of bytes of the instruction(s) clobbered at the hook point
 * might be different.  If the clobbered bytes are more than 5 bytes, then the
 * return jmp from the landing pad won't be able to reach it.  By placing 8
 * bytes above the landing pad, we give it the extra reachability needed.
 * Also, having the tgt_pc at the top of the landing pad makes it easy to see
 * the disassembly of the whole landing pad while debugging, else there will be
 * jmp and garbage after it.
 *
 * This isn't a problem for 32-bit landing pad because in 32-bit everything is
 * reachable.
 *
 * We must put the displaced app instr(s) in the landing pad for x64
 * b/c they may contain rip-rel data refs and those may not reach if
 * in the main trampoline (i#902).
 *
 * See heap.c for details about what landing pads are.
 */
#define JMP_SIZE (IF_X64_ELSE(JMP_ABS_IND64_SIZE, JMP_REL32_SIZE))
static byte *
emit_landing_pad_code(byte *lpad_buf, const byte *tgt_pc, const byte *after_hook_pc,
                      size_t displaced_app_size, byte **displaced_app_loc OUT,
                      bool *changed_prot)
{
    byte *lpad_entry = lpad_buf;
    bool res;
    byte *lpad_start = lpad_buf;
    ASSERT(lpad_buf != NULL);

    res = make_hookable(lpad_buf, LANDING_PAD_SIZE, changed_prot);
    ASSERT(res);

#ifndef X64
    *lpad_buf = JMP_REL32_OPCODE;
    lpad_buf++;
    *((int *)lpad_buf) = (int)(tgt_pc - lpad_buf - 4);
    lpad_buf += 4;
#else
    *((byte **)lpad_buf) = (byte *)tgt_pc; /* save tgt_pc for the rip-rel jmp */
    lpad_buf += sizeof(tgt_pc);
    lpad_entry = lpad_buf; /* entry is after the first 8 bytes */
    *lpad_buf = JMP_ABS_IND64_OPCODE;
    lpad_buf++;
    *lpad_buf = JMP_ABS_MEM_IND64_MODRM;
    lpad_buf++;
    /* rip relative address to 8-bytes, i.e., start of lpad_buf */
    *((int *)lpad_buf) = -(int)(JMP_ABS_IND64_SIZE + sizeof(tgt_pc));
    lpad_buf += 4;
#endif

    /* Leave space for the displaced app code */
    ASSERT(displaced_app_size < MAX_HOOK_DISPLACED_LENGTH);
    ASSERT(displaced_app_loc != NULL);
    *displaced_app_loc = lpad_buf;
    lpad_buf += displaced_app_size;

    /* The return 32-bit relative jump is common to both 32-bit and 64-bit
     * landing pads.  Make sure that the second jmp goes into the right address.
     */
    ASSERT((size_t)(lpad_buf - lpad_start) ==
           JMP_SIZE IF_X64(+sizeof(tgt_pc)) + displaced_app_size);
    *lpad_buf = JMP_REL32_OPCODE;
    lpad_buf++;
    *((int *)lpad_buf) = (int)(after_hook_pc - lpad_buf - 4);
    lpad_buf += 4;

    /* Even though we have the 8 byte space up front for 64-bit, just make sure
     * that the return jmp can reach the instruction after the hook.
     */
    ASSERT(REL32_REACHABLE(lpad_buf, after_hook_pc));

    /* Make sure that the landing pad size match with definitions. */
    ASSERT(lpad_buf - lpad_start <= LANDING_PAD_SIZE);

    /* Return unused space */
    trim_landing_pad(lpad_start, lpad_buf - lpad_start);

    return lpad_entry;
}

static void
finalize_landing_pad_code(byte *lpad_buf, bool changed_prot)
{
    make_unhookable(lpad_buf, LANDING_PAD_SIZE, changed_prot);
}

/* Assumes that ilist contains decoded instrs for [start_pc, start_pc+size).
 * Copies size bytes of the app code at start_pc into buf by encoding
 * the ilist, re-relativizing rip-relative and ctis as it goes along.
 * Also converts short ctis into 32-bit-offset ctis.
 *
 * hotp_only does not support ctis in the middle of the ilist, only at
 * the end, nor size changes in the middle of the ilist: to support
 * that we'd need a relocation table mapping old instruction offsets
 * to the newly emitted ones.
 *
 *      As of today only one cti is allowed in a patch region and that too at
 * the end of it, so the starting location of that cti won't change even if we
 * convert and re-relativize it.  This means hot patch control flow changes into
 * the middle of a patch region won't have to worry about using an offset table.
 *
 *      The current patch region definition doesn't allow ctis to be in the
 * middle of patch regions.  This means we don't have to worry about
 * re-relativizing ctis in the middle of a patch region.  However Alex has an
 * argument about allowing cbrs to be completely inside a patch region as
 * control flow can never reach the following instruction other than fall
 * through, i.e., not from outside.  This is a matter for debate, but one
 * which will need the ilist & creating the relocation table per patch point.
 */
static byte *
copy_app_code(dcontext_t *dcontext, const byte *start_pc, byte *buf, size_t size,
              instrlist_t *ilist)
{
    instr_t *instr;
    byte *buf_nxt;
    DEBUG_DECLARE(byte *buf_start = buf;)
    DEBUG_DECLARE(bool size_change = false;)
    ASSERT(dcontext != NULL && start_pc != NULL && buf != NULL);
    /* Patch region should be at least 5 bytes in length, but no more than 5
     * plus the length of the last instruction in the region.
     */
    ASSERT(size >= 5 &&
           size < (size_t)(5 + instr_length(dcontext, instrlist_last(ilist))));

    /* We have to walk the instr list to lengthen short (8-bit) ctis */
    for (instr = instrlist_first(ilist); instr != NULL; instr = instr_get_next(instr)) {
        /* For short ctis in the loop to jecxz range, the cti conversion
         * will set the target in the raw bits, so the raw bits will be valid.
         * For other short ctis, the conversion will invalidate the raw bits,
         * so a full encoding is enforced.  For other ctis, the raw bits aren't
         * valid for encoding because we are relocating them; so invalidate
         * them explicitly.
         */
        if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
            if (instr_is_cti_short(instr)) {
                DODEBUG({ size_change = true; });
                convert_to_near_rel(dcontext, instr);
            } else
                instr_set_raw_bits_valid(instr, false);
            /* see notes above: hotp_only doesn't support non-final cti */
            ASSERT(!instr_is_cti(instr) || instr == instrlist_last(ilist));
        }
#ifdef X64
        /* If we have reachability issues, instrlist_encode() below
         * will fail.  We try to do an assert here for that case
         * (estimating where the relative offset will be encoded at).
         * PR 250294's heap pad proposal will solve this.
         */
        DOCHECK(1, {
            app_pc target;
            instr_get_rel_addr_target(instr, &target);
            ASSERT_NOT_IMPLEMENTED(
                (!instr_has_rel_addr_reference(instr) || REL32_REACHABLE(buf, target)) &&
                "PR 250294: displaced code too far from rip-rel target");
        });
#endif
    }

    /* now encode and re-relativize x64 rip-relative instructions */
    buf_nxt = instrlist_encode(dcontext, ilist, buf, false /*no instr_t targets*/);
    ASSERT(buf_nxt != NULL);
    ASSERT((buf_nxt - buf) == (ssize_t)size ||
           size_change && (buf_nxt - buf) > (ssize_t)size);
    return buf_nxt;
}

/* N.B.: !assume_xsp && !assume_not_on_dstack implies eflags assumptions!
 * !assume_xsp && assume_not_on_dstack does not assume eflags.
 * Could optimize by having a bool indicating whether to have a callee arg or not,
 * but then the intercept_function_t typedef must be void, or must have two, so we
 * just make every callee take an arg.
 *
 * Currently only hotp_only uses alt_after_tgt_p.  It points at the pointer-sized
 * target that initially has the value alternate_after.  It is NOT intra-cache-line
 * aligned and thus if the caller wants a hot-patchable target it must
 * have another layer of indirection.
 */
static byte *
emit_intercept_code(dcontext_t *dcontext, byte *pc, intercept_function_t callee,
                    void *callee_arg, bool assume_xsp, bool assume_not_on_dstack,
                    after_intercept_action_t action_after, byte *alternate_after,
                    byte **alt_after_tgt_p OUT)
{
    instrlist_t ilist;
    instr_t *inst, *push_start, *push_start2 = NULL;
    instr_t *decision = NULL, *alt_decision = NULL, *alt_after = NULL;
    uint len;
    byte *start_pc, *push_pc, *push_pc2 = NULL;
    app_pc no_cleanup;
    uint stack_offs = 0;
    IF_DEBUG(bool direct;)

    /* AFTER_INTERCEPT_LET_GO_ALT_DYN is used only dynamically to select alternate */
    ASSERT(action_after != AFTER_INTERCEPT_LET_GO_ALT_DYN);

    /* alternate_after provided only when possibly using alternate target */
    ASSERT(alternate_after == NULL || action_after == AFTER_INTERCEPT_DYNAMIC_DECISION ||
           action_after == AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT);

    /* initialize the ilist */
    instrlist_init(&ilist);

    if (!assume_xsp) {
        instr_t *no_local_stack = INSTR_CREATE_label(dcontext);
        instr_t *grab_initstack = INSTR_CREATE_label(dcontext);
        instr_t *get_lock = INSTR_CREATE_label(dcontext);
        instr_t *have_lock = INSTR_CREATE_label(dcontext);
        instr_t *have_stack_now = INSTR_CREATE_label(dcontext);
        APP(&ilist,
            INSTR_CREATE_mov_st(dcontext,
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR),
                                opnd_create_reg(REG_XCX)));
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                opnd_create_tls_slot(os_tls_offset(TLS_DCONTEXT_SLOT))));
        APP(&ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(no_local_stack)));

        if (!assume_not_on_dstack) {
            instr_t *not_on_dstack = INSTR_CREATE_label(dcontext);
            APP(&ilist,
                instr_create_restore_from_dc_via_reg(dcontext, REG_XCX, REG_XCX,
                                                     DSTACK_OFFSET));
            APP(&ilist,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSP),
                                 opnd_create_reg(REG_XCX)));
            APP(&ilist,
                INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(not_on_dstack)));
            APP(&ilist,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XCX),
                                 opnd_create_base_disp(REG_XCX, REG_NULL, 0,
                                                       -(int)DYNAMORIO_STACK_SIZE,
                                                       OPSZ_0)));
            APP(&ilist,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSP),
                                 opnd_create_reg(REG_XCX)));
            APP(&ilist,
                INSTR_CREATE_jcc(dcontext, OP_jl, opnd_create_instr(not_on_dstack)));
            APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSP)));
            APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(2)));
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(have_stack_now)));
            APP(&ilist, not_on_dstack);
            APP(&ilist,
                INSTR_CREATE_mov_ld(
                    dcontext, opnd_create_reg(REG_XCX),
                    opnd_create_tls_slot(os_tls_offset(TLS_DCONTEXT_SLOT))));
        }

        /* Store the app xsp in dcontext and switch to dstack. */
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
            APP(&ilist,
                instr_create_restore_from_dc_via_reg(dcontext, REG_XCX, REG_XCX,
                                                     PROT_OFFS));
        }
        APP(&ilist,
            instr_create_save_to_dc_via_reg(dcontext, REG_XCX, REG_XSP, PC_OFFSET));
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
            APP(&ilist,
                INSTR_CREATE_mov_ld(
                    dcontext, opnd_create_reg(REG_XCX),
                    opnd_create_tls_slot(os_tls_offset(TLS_DCONTEXT_SLOT))));
        }
        APP(&ilist,
            instr_create_restore_from_dc_via_reg(dcontext, REG_XCX, REG_XSP,
                                                 DSTACK_OFFSET));

        /* Get the app xsp from the dcontext and put it on the dstack to serve
         * as the app xsp cache.
         */
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
            APP(&ilist,
                instr_create_restore_from_dc_via_reg(dcontext, REG_XCX, REG_XCX,
                                                     PROT_OFFS));
        }
        APP(&ilist,
            instr_create_restore_from_dc_via_reg(dcontext, REG_XCX, REG_XCX, PC_OFFSET));
        APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XCX)));
        APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(1)));
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(have_stack_now)));

        /* use d_r_initstack to avoid any assumptions about app xsp */
        /* first check if we are already on it */
        APP(&ilist, no_local_stack);
        APP(&ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR((ptr_int_t)d_r_initstack)));
        APP(&ilist,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_reg(REG_XCX)));
        APP(&ilist,
            INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(grab_initstack)));
        APP(&ilist,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XCX),
                             opnd_create_base_disp(REG_XCX, REG_NULL, 0,
                                                   -(int)DYNAMORIO_STACK_SIZE, OPSZ_0)));
        APP(&ilist,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_reg(REG_XCX)));
        APP(&ilist, INSTR_CREATE_jcc(dcontext, OP_jl, opnd_create_instr(grab_initstack)));
        APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSP)));
        APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(2)));
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(have_stack_now)));
        APP(&ilist, grab_initstack);
        APP(&ilist,
            INSTR_CREATE_mov_imm(dcontext,
                                 /* on x64 the upper 32 bits will be zeroed for us */
                                 opnd_create_reg(REG_ECX), OPND_CREATE_INT32(1)));
#ifdef X64
        APP(&ilist,
            INSTR_CREATE_mov_st(dcontext,
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PEB_TIB_OFFSET, OPSZ_PTR),
                                opnd_create_reg(REG_XAX)));
#endif
        APP(&ilist, get_lock);
#ifdef X64
        APP(&ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_INTPTR((ptr_uint_t)&initstack_mutex)));
#endif
        APP(&ilist,
            INSTR_CREATE_xchg(
                dcontext,
                /* initstack_mutex is 32 bits always */
                IF_X64_ELSE(OPND_CREATE_MEM32(REG_XAX, 0),
                            OPND_CREATE_ABSMEM((void *)&initstack_mutex, OPSZ_4)),
                opnd_create_reg(REG_ECX)));
        APP(&ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(have_lock)));
        APP(&ilist, INSTR_CREATE_pause(dcontext));
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(get_lock)));
        APP(&ilist, have_lock);
        APP(&ilist,
            INSTR_CREATE_mov_st(
                dcontext,
                IF_X64_ELSE(opnd_create_reg(REG_XCX),
                            OPND_CREATE_ABSMEM((void *)&initstack_app_xsp, OPSZ_PTR)),
                opnd_create_reg(REG_XSP)));
#ifdef X64
        /* we can do a 64-bit absolute address into xax only */
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                OPND_CREATE_ABSMEM((void *)&d_r_initstack, OPSZ_PTR)));
        APP(&ilist,
            INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XSP),
                              opnd_create_reg(REG_XAX)));
#else
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XSP),
                                OPND_CREATE_ABSMEM((void *)&d_r_initstack, OPSZ_PTR)));
#endif
        APP(&ilist,
            INSTR_CREATE_push(
                dcontext,
                IF_X64_ELSE(opnd_create_reg(REG_XCX),
                            OPND_CREATE_ABSMEM((void *)&initstack_app_xsp, OPSZ_PTR))));
        APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
#ifdef X64
        APP(&ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_INTPTR((ptr_uint_t)peb_ptr)));
        APP(&ilist,
            INSTR_CREATE_xchg(dcontext,
                              opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                        PEB_TIB_OFFSET, OPSZ_PTR),
                              opnd_create_reg(REG_XAX)));
#endif
        APP(&ilist, have_stack_now);
#ifdef X64
        /* we could perhaps assume the top 32 bits of win32_pid are zero, but
         * xchg works just as well */
        APP(&ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR((ptr_uint_t)win32_pid)));
        APP(&ilist,
            INSTR_CREATE_xchg(dcontext,
                              opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                        PID_TIB_OFFSET, OPSZ_PTR),
                              opnd_create_reg(REG_XCX)));
#else
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR)));
        APP(&ilist,
            INSTR_CREATE_mov_st(dcontext,
                                opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                          PID_TIB_OFFSET, OPSZ_PTR),
                                OPND_CREATE_INTPTR(win32_pid)));
#endif       /* X64 */
    } else { /* assume_xsp */
        /* Cache app xsp so that the right value can be passed to the handler
         * and to restore at exit.  Push stack type too: 3 for app stack.
         */
        APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSP)));
        APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(3)));
    }

    /* We assume that if !assume_xsp we've done two pushes on the stack.
     * DR often only cares about stack alignment for xmm saves.
     * However, it sometimes calls ntdll routines; and for client exception
     * handlers that might call random library routines we really care.
     * We assume that the kernel will make sure of the stack alignment,
     * so we use stack_offs to make sure of the stack alignment in the
     * instrumentation.
     */
    stack_offs = insert_push_all_registers(
        dcontext, NULL, &ilist, NULL, XSP_SZ,
        /* pc slot not used: could use instead of state->start_pc */
        /* sign-extended */
        OPND_CREATE_INT32(0), REG_NULL);

    /* clear eflags for callee's usage */
    APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
    APP(&ilist, INSTR_CREATE_RAW_popf(dcontext));

    /* Get the cached app xsp and update the pusha's xsp with it; this is the
     * right app xsp.
     */
    APP(&ilist,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                            OPND_CREATE_MEMPTR(REG_XSP, /* mcxt + stack type */
                                               sizeof(priv_mcontext_t) + XSP_SZ)));
    APP(&ilist,
        INSTR_CREATE_mov_st(dcontext,
                            OPND_CREATE_MEMPTR(REG_XSP, offsetof(priv_mcontext_t, xsp)),
                            opnd_create_reg(REG_XAX)));

    /* FIXME: don't want hooks for trampolines that run natively like
     * LdrLoadDll or image entry, right?
     */
    if (ENTER_DR_HOOK != NULL) {
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args.
         */
        IF_DEBUG(direct =)
        dr_insert_call_ex((void *)dcontext, &ilist, NULL /*append*/,
                          /* we're not in vmcode, so avoid indirect call */
                          pc, (void *)ENTER_DR_HOOK, 0);
        ASSERT(direct);
    }

    /* these are part of app_state_at_intercept_t struct so we have to
     * push them on the stack, rather than pass in registers
     */
    /* will fill in immed with no_cleanup pointer later */
#ifdef X64
    push_start =
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX), OPND_CREATE_INTPTR(0));
    APP(&ilist, push_start);
    APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
    APP(&ilist,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                             OPND_CREATE_INTPTR(callee_arg)));
    APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
#else
    push_start = INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INTPTR(0));
    APP(&ilist, push_start);
    APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INTPTR(callee_arg)));
#endif
    stack_offs += 2 * XSP_SZ;

    /* We pass xsp as a pointer to all the values on the stack; this is the actual
     * argument to the intercept routine.  Fix for case 7597.
     * -- CAUTION -- if app_state_at_intercept_t changes in anyway, this can
     * blow up!  That structure's field's types, order & layout are assumed
     * here.  These two should change only in synch.
     */
    if (parameters_stack_padded()) {
        /* xsp won't have proper value due to stack padding */
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                opnd_create_reg(REG_XSP)));
#ifdef X64
        /* i#331: align the misaligned stack */
#    define STACK_ALIGNMENT 16
        if (!ALIGNED(stack_offs, STACK_ALIGNMENT)) {
            ASSERT(ALIGNED(stack_offs, XSP_SZ));
            APP(&ilist,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, -(int)XSP_SZ, OPSZ_0)));
        }
#endif
    }
    IF_DEBUG(direct =)
    dr_insert_call_ex(dcontext, &ilist, NULL,
                      /* we're not in vmcode, so avoid indirect call */
                      pc, (byte *)callee, 1,
                      parameters_stack_padded() ? opnd_create_reg(REG_XAX)
                                                : opnd_create_reg(REG_XSP));
    ASSERT(direct);
#ifdef X64
    /* i#331, misaligned stack adjustment cleanup */
    if (parameters_stack_padded()) {
        if (!ALIGNED(stack_offs, STACK_ALIGNMENT)) {
            ASSERT(ALIGNED(stack_offs, XSP_SZ));
            APP(&ilist,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, XSP_SZ, OPSZ_0)));
        }
    }
#endif
    /* clean up 2 pushes */
    APP(&ilist,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_XSP),
            opnd_create_base_disp(REG_XSP, REG_NULL, 0, 2 * XSP_SZ, OPSZ_0)));
    if (action_after == AFTER_INTERCEPT_DYNAMIC_DECISION) {
        /* our 32-bit immed will be sign-extended.
         * perhaps we could assume upper bits not set and use eax to save a rex.w.
         */
        APP(&ilist,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XAX),
                             OPND_CREATE_INT32(AFTER_INTERCEPT_LET_GO)));
        /* will fill in later */
        decision = INSTR_CREATE_jcc(dcontext, OP_je, opnd_create_instr(NULL));
        APP(&ilist, decision);
        if (alternate_after != NULL) {
            APP(&ilist,
                INSTR_CREATE_cmp(
                    dcontext, opnd_create_reg(REG_XAX),
                    OPND_CREATE_INT32(AFTER_INTERCEPT_LET_GO_ALT_DYN))); /*sign-extended*/
            /* will fill in later */
            alt_decision = INSTR_CREATE_jcc(dcontext, OP_je, opnd_create_instr(NULL));
            APP(&ilist, alt_decision);
        }
    }

    if (action_after == AFTER_INTERCEPT_TAKE_OVER ||
        action_after == AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT ||
        action_after == AFTER_INTERCEPT_DYNAMIC_DECISION) {
        /* will fill in immed with no_cleanup pointer later */
#ifdef X64
        push_start2 = INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                           OPND_CREATE_INTPTR(0));
        APP(&ilist, push_start2);
        APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
#else
        push_start2 = INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INTPTR(0));
        APP(&ilist, push_start2);
#endif
        APP(&ilist,
            INSTR_CREATE_push_imm(dcontext,
                                  OPND_CREATE_INT32(0 /*don't save dcontext*/)));
        if (parameters_stack_padded()) {
            /* xsp won't have proper value due to stack padding */
            APP(&ilist,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                    opnd_create_reg(REG_XSP)));
#ifdef X64
            /* i#331: align the misaligned stack */
            APP(&ilist,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, -(int)XSP_SZ, OPSZ_0)));
#endif
        }
        IF_DEBUG(direct =)
        dr_insert_call_ex(dcontext, &ilist, NULL,
                          /* we're not in vmcode, so avoid indirect call */
                          pc, (app_pc)asynch_take_over, 1,
                          parameters_stack_padded() ? opnd_create_reg(REG_XAX)
                                                    : opnd_create_reg(REG_XSP));
        ASSERT(direct);
#ifdef INTERNAL
        IF_DEBUG(direct =)
        dr_insert_call_ex(dcontext, &ilist, NULL,
                          /* we're not in vmcode, so avoid indirect call */
                          pc, (app_pc)d_r_internal_error, 3, OPND_CREATE_INTPTR(0),
                          OPND_CREATE_INT32(-3), OPND_CREATE_INTPTR(0));
        ASSERT(direct);
#endif
#ifdef X64
        if (parameters_stack_padded()) {
            /* i#331: misaligned stack adjust cleanup*/
            APP(&ilist,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, XSP_SZ, OPSZ_0)));
        }
#endif
    }

    if (action_after == AFTER_INTERCEPT_LET_GO ||
        action_after == AFTER_INTERCEPT_DYNAMIC_DECISION) {
        if (alternate_after != NULL) {
            byte *encode_pc;
            insert_let_go_cleanup(dcontext, pc, &ilist, alt_decision, assume_xsp,
                                  assume_not_on_dstack, action_after);
            /* alternate after cleanup target */
            /* if alt_after_tgt_p != NULL we always do pointer-sized even if
             * the initial target happens to reach
             */
            /* we assert below we're < PAGE_SIZE for reachability test */
            encode_pc = (alt_after_tgt_p != NULL) ? vmcode_unreachable_pc() : pc;
            IF_DEBUG(direct =)
            insert_reachable_cti(dcontext, &ilist, NULL, encode_pc, alternate_after,
                                 true /*jmp*/, false /*!returns*/, false /*!precise*/,
                                 DR_REG_NULL /*no scratch*/, &alt_after);
            ASSERT(alt_after_tgt_p == NULL || !direct);
        }
        /* the normal let_go target */
        insert_let_go_cleanup(dcontext, pc, &ilist, decision, assume_xsp,
                              assume_not_on_dstack, action_after);
    }

    /* Now encode the instructions, first setting the offset fields. */
    len = 0;
    push_pc = NULL;
    for (inst = instrlist_first(&ilist); inst; inst = instr_get_next(inst)) {
        inst->offset = len;
        len += instr_length(dcontext, inst);
    }
    start_pc = pc;
    for (inst = instrlist_first(&ilist); inst; inst = instr_get_next(inst)) {
        pc = instr_encode(dcontext, inst, pc);
        ASSERT(pc != NULL);
        if (inst == push_start)
            push_pc = (pc - sizeof(ptr_uint_t));
        if (inst == push_start2)
            push_pc2 = (pc - sizeof(ptr_uint_t));
        if (inst == alt_after && alt_after_tgt_p != NULL)
            *alt_after_tgt_p = pc - sizeof(alternate_after);
    }

    /* now can point start_pc arg of callee at beyond-cleanup pc */
    if (action_after == AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT) {
        /* Note the interface here allows any target. Yet as the name
         * suggests it should mainly be used to directly transfer to
         * the now restored trampoline target.
         */
        ASSERT(alternate_after != NULL);
        no_cleanup = alternate_after;
    } else {
        /* callers are supposed to append the original target prefix */
        no_cleanup = pc;
    }

    ASSERT(push_pc != NULL);
    *((ptr_uint_t *)push_pc) = (ptr_uint_t)no_cleanup;
    if (push_pc2 != NULL)
        *((ptr_uint_t *)push_pc2) = (ptr_uint_t)no_cleanup;

    ASSERT((size_t)(pc - start_pc) < PAGE_SIZE &&
           "adjust REL32_REACHABLE for alternate_after");

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}
#undef APP

static void
map_intercept_pc_to_app_pc(byte *interception_pc, app_pc original_app_pc,
                           size_t displace_length, size_t orig_length,
                           bool hook_occludes_instrs)
{
    intercept_map_elem_t *elem =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, intercept_map_elem_t, ACCT_OTHER, UNPROTECTED);

    elem->interception_pc = interception_pc;
    elem->original_app_pc = original_app_pc;
    elem->displace_length = displace_length;
    elem->orig_length = orig_length;
    elem->hook_occludes_instrs = hook_occludes_instrs;
    elem->next = NULL;

    d_r_mutex_lock(&map_intercept_pc_lock);

    if (intercept_map->head == NULL) {
        intercept_map->head = elem;
        intercept_map->tail = elem;
    } else if (hook_occludes_instrs) {    /* i#1632: group hook-occluding intercepts at */
        elem->next = intercept_map->head; /* the head because iteration is frequent.    */
        intercept_map->head = elem;
    } else {
        intercept_map->tail->next = elem;
        intercept_map->tail = elem;
    }

    d_r_mutex_unlock(&map_intercept_pc_lock);
}

static void
unmap_intercept_pc(app_pc original_app_pc)
{
    intercept_map_elem_t *curr, *prev, *next;

    d_r_mutex_lock(&map_intercept_pc_lock);

    prev = NULL;
    curr = intercept_map->head;
    while (curr != NULL) {
        next = curr->next;
        if (curr->original_app_pc == original_app_pc) {
            if (prev != NULL) {
                prev->next = curr->next;
            }
            if (curr == intercept_map->head) {
                intercept_map->head = curr->next;
            }
            if (curr == intercept_map->tail) {
                intercept_map->tail = prev;
            }

            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, curr, intercept_map_elem_t, ACCT_OTHER,
                           UNPROTECTED);
            /* We don't break b/c we allow multiple entries and in fact
             * we have multiple today: one for displaced app code and
             * one for the jmp from interception buffer to landing pad.
             */
        } else
            prev = curr;
        curr = next;
    }

    d_r_mutex_unlock(&map_intercept_pc_lock);
}

static void
free_intercept_list(void)
{
    /* For all regular hooks, un_intercept_call() calls unmap_intercept_pc()
     * and removes the hook's entry.  But syscall wrappers have a target app
     * pc that's unusual.  Rather than store it for each, we just tear
     * down the whole list.
     */
    intercept_map_elem_t *curr;
    d_r_mutex_lock(&map_intercept_pc_lock);
    while (intercept_map->head != NULL) {
        curr = intercept_map->head;
        intercept_map->head = curr->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, curr, intercept_map_elem_t, ACCT_OTHER,
                       UNPROTECTED);
    }
    intercept_map->head = NULL;
    intercept_map->tail = NULL;
    d_r_mutex_unlock(&map_intercept_pc_lock);
}

/* We assume no mangling of code placed in the interception buffer,
 * other than re-relativizing ctis.  As such, we can uniquely correlate
 * interception buffer PCs to their original app PCs.
 * Caller must check that pc is actually in the intercept buffer (or landing
 * pad displaced app code or jmp back).
 */
app_pc
get_app_pc_from_intercept_pc(byte *pc)
{
    intercept_map_elem_t *iter = intercept_map->head;
    while (iter != NULL) {
        byte *start = iter->interception_pc;
        byte *end = start + iter->displace_length;
        if (pc >= start && pc < end) {
            /* include jmp back but map it to instr after displacement */
            if ((size_t)(pc - start) > iter->orig_length)
                return iter->original_app_pc + iter->orig_length;
            else
                return iter->original_app_pc + (pc - start);
        }

        iter = iter->next;
    }

    ASSERT_NOT_REACHED();
    return NULL;
}

/* i#1632: map instrs occluded by an intercept hook to the intercept (as necessary) */
byte *
get_intercept_pc_from_app_pc(app_pc pc, bool occlusions_only, bool exclude_start)
{
    intercept_map_elem_t *iter = intercept_map->head;
    /* hook-occluded instrs are always grouped at the head */
    while (iter != NULL && (!occlusions_only || iter->hook_occludes_instrs)) {
        byte *start = iter->original_app_pc;
        byte *end = start + iter->orig_length;
        if (pc == start) {
            if (exclude_start)
                return NULL;
            else
                return iter->interception_pc;
        } else if (pc > start && pc < end)
            return iter->interception_pc + (pc - start);

        iter = iter->next;
    }

    return NULL;
}

bool
is_intercepted_app_pc(app_pc pc, byte **interception_pc)
{
    intercept_map_elem_t *iter = intercept_map->head;
    while (iter != NULL) {
        /* i#268: respond for any pc not just the first.
         * FIXME: do we handle app targeting middle of hook?
         * I'm assuming here that we would not create another
         * entry for that start and it's ok to not match only start.
         */
        if (pc >= iter->original_app_pc &&
            pc < iter->original_app_pc + iter->orig_length) {
            /* PR 219351: For syscall trampolines, while building bbs we replace
             * the jmp and never execute from the displaced app code in the
             * buffer, so the bb looks normal.  FIXME: should we just not add to
             * the map?  For now, better safe than sorry so
             * get_app_pc_from_intercept_pc will work in case we ever ask about
             * that displaced app code.
             */
            if (is_syscall_trampoline(iter->interception_pc, NULL))
                return false;
            if (interception_pc != NULL)
                *interception_pc = iter->interception_pc + (pc - iter->original_app_pc);

            return true;
        }

        iter = iter->next;
    }

    return false;
}

/* Emits a jmp at pc to resume_pc.  If pc is in the interception buffer,
 * adds a map entry from [xl8_start_pc, return value here) to
 * [app_pc, <same size>).
 */
static byte *
emit_resume_jmp(byte *pc, byte *resume_pc, byte *app_pc, byte *xl8_start_pc)
{
#ifndef X64
    *pc = JMP_REL32_OPCODE;
    pc++;
    *((int *)pc) = (int)(resume_pc - pc - 4);
    pc += 4; /* 4 is the size of the relative offset */
#else
    *pc = JMP_ABS_IND64_OPCODE;
    pc++;
    *pc = JMP_ABS_MEM_IND64_MODRM;
    pc++;
#endif
    /* We explicitly map rather than having instr_set_translation() and
     * dr_fragment_app_pc() special-case this jump: longer linear search
     * in the interception map, but cleaner code.
     */
    if (is_in_interception_buffer(pc) && app_pc != NULL) {
        ASSERT(xl8_start_pc != NULL);
        map_intercept_pc_to_app_pc(xl8_start_pc, app_pc, pc - xl8_start_pc,
                                   pc - xl8_start_pc, false /* not a hook occlusion */);
    }
#ifdef X64
    /* 64-bit abs address is placed after the jmp instr., i.e., rip rel is 0.
     * We can't place it before the jmp as in the case of the landing pad
     * because there is code in the trampoline immediately preceding this jmp.
     */
    *((int *)pc) = 0;
    pc += 4; /* 4 here is the rel offset to the lpad entry */
    *((byte **)pc) = resume_pc;
    pc += sizeof(resume_pc);
#endif
    return pc;
}

/* Redirects code at tgt_pc to jmp to our_pc, which is filled with generated
 * code to call prof_func and then return to the original code.
 * Assumes that the original tgt_pc should be unwritable.
 * The caller is responsible for adding the generated
 * code at our_pc to the dynamo/executable list(s).
 *
 * We assume we're being called either before any threads are created
 * or while all threads are suspended, as our code-overwriting is not atomic!
 * The only fix is to switch from code-overwriting to import-table modifying,
 * which is more complicated, see Richter chap22 for example: and import-table
 * modifying will not allow arbitrary hook placement of course, which we
 * support for probes and hot patches.
 *
 * We guarantee to use a 5-byte jump instruction, even on x64 (PR 250294: we
 * sometimes have to allocate nearby landing pads there.  See PR 245169 for all
 * of the possibilities for x64 hooking, all of which are either very large or
 * have caveats; we decided that allocating several 64K chunks and sticking w/
 * 5-byte jumps was the cleanest).  It is up to the caller to ensure that we
 * aren't crossing a cti target point and that displacing these 5 bytes is safe
 * (though we will take care of re-relativizing the displaced code)).
 *
 * When cti_safe_to_ignore true, we expect to restore the code
 * immediately after hitting our trampoline then we can treat the
 * first 5 bytes as raw.  Otherwise, we may need to PC-relativize or
 * deal with conflicting hookers (case 2525).  Assuming a CTI in the
 * target is a good sign for hookers, we may decide to treat that
 * specially based on DYNAMO_OPTION(hook_conflict) or we can give up
 * and not intercept this call when abort_on_incompatible_hooker is
 * true.
 * FIXME: if we add one more flag we should switch to a single flag enum
 *
 * Currently only hotp_only uses app_code_copy_p and alt_exit_tgt_p.
 * These point at their respective locations.  alt_exit_tgt_p is
 * currently NOT aligned for hot patching.
 *
 * Returns pc after last instruction of emitted interception code,
 * or NULL when abort_on_incompatible_hooker is true and tgt_pc starts with a CTI.
 */
static byte *
intercept_call(byte *our_pc, byte *tgt_pc, intercept_function_t prof_func,
               void *callee_arg, bool assume_xsp, after_intercept_action_t action_after,
               bool abort_on_incompatible_hooker, bool cti_safe_to_ignore,
               byte **app_code_copy_p, byte **alt_exit_tgt_p)
{
    byte *pc, *our_pc_end, *lpad_start, *lpad_pc, *displaced_app_pc;
    size_t size = 0;
    instrlist_t ilist;
    instr_t *instr;
    bool changed_prot, hook_occludes_instrs = false;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool is_hooked = false;
    bool ok;

    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;

    ASSERT(tgt_pc != NULL);
    /* can't detect hookers if ignoring CTIs */
    ASSERT(!abort_on_incompatible_hooker || !cti_safe_to_ignore);

    /* we need 5 bytes for a jump
     * find instr boundary >= 5 bytes after pc
     */
    LOG(GLOBAL, LOG_ASYNCH, 3, "before intercepting:\n");
    instrlist_init(&ilist);
    pc = tgt_pc;
    do {
        app_pc next_pc;
        DOLOG(3, LOG_ASYNCH, { disassemble_with_bytes(dcontext, pc, main_logfile); });
        instr = instr_create(dcontext);
        next_pc = decode_cti(dcontext, pc, instr);
        ASSERT(instr_valid(instr));
        instrlist_append(&ilist, instr);

        hook_occludes_instrs = hook_occludes_instrs || (size > 0 || (next_pc - pc) != 5);

        /* we do not handle control transfer instructions very well here! (case 2525) */
        if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
            /* allow for only a single cti at first instruction,
             *
             * unless CTIs are safe to ignore since never actually
             * re-relativized (case 4086 == once-only so don't execute copy)
             */
            ASSERT(!is_hooked);
            ASSERT(tgt_pc == pc || cti_safe_to_ignore);
            if (!cti_safe_to_ignore) {
                /* we treat this as a sign of a third party hooking before us */
                is_hooked = true;
            }
        }

        pc = next_pc;

        /* some of our trampolines are best effort anyways: LdrLoadDll
         * shouldn't matter much, yet we like to keep it when we can
         */
        if (is_hooked && abort_on_incompatible_hooker) {
            SYSLOG_INTERNAL_WARNING_ONCE(
                "giving up interception: " PFX " already hooked\n", tgt_pc);
            LOG(GLOBAL, LOG_ASYNCH, 1,
                "intercept_call: giving up " PFX " already hooked\n", tgt_pc);
            instrlist_clear(dcontext, &ilist);
            return NULL;
        }

        if (pc == NULL ||
            is_hooked && DYNAMO_OPTION(hook_conflict) == HOOKED_TRAMPOLINE_DIE) {
            FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                              get_application_pid());
        }

        size = (pc - tgt_pc);
    } while (size < 5);

    pc = our_pc;

    if (is_hooked && DYNAMO_OPTION(hook_conflict) == HOOKED_TRAMPOLINE_SQUASH) {
        /* squash over original with expected code, so that both
         * copies we make later (one for actual execution and one for
         * uninterception) have the supposedly original values
         * see use in intercept_syscall_wrapper()
         */
        /* FIXME: it is not easy to get the correct original bytes
         * probably best solution is to read from the original
         * ntdll.dll on disk.  To avoid having to deal with RVA disk
         * to virtual address transformations, it may be even easier
         * to call LdrLoadDll with a different path to a load a
         * pristine copy e.g. \\?C:\WINNT\system32\ntdll.dll
         */
        /* FIXME: even if we detach we don't restore the original
         * values, since what we have here should be good enough
         */
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* Store 1st 5 bytes of original code at start of our code
     * (won't be executed, original code will jump to after it)
     * We do this for convenience of un-intercepting, so we don't have to
     * record offset of the copy in the middle of the interception code
     * CAUTION: storing the exact copy of the 5 bytes from the app image at
     *          the start of the trampoline is assumed in hotp_only for
     *          case 7279 - change only in synch.
     */
    memcpy(pc, tgt_pc, 5);
    pc += 5;

    /* Allocate the landing pad, store its address (4 bytes in 32-bit builds
     * and 8 in 64-bit ones) in the trampoline, just after the original app
     * code, and emit it.
     */
    lpad_start = alloc_landing_pad(tgt_pc);
    memcpy(pc, &lpad_start, sizeof(lpad_start));
    pc += sizeof(lpad_start);

    if (alt_exit_tgt_p != NULL) {
        /* XXX: if we wanted to align for hot-patching we'd do so here
         * and we'd pass the (post-padding) pc here as the alternate_after
         * to emit_intercept_code
         */
    }

    lpad_pc = lpad_start;
    lpad_pc = emit_landing_pad_code(lpad_pc, pc, tgt_pc + size, size, &displaced_app_pc,
                                    &changed_prot);

    pc = emit_intercept_code(dcontext, pc, prof_func, callee_arg, assume_xsp, assume_xsp,
                             action_after,
                             (action_after == AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT)
                                 ? tgt_pc
                                 : ((alt_exit_tgt_p != NULL) ? CURRENTLY_UNKNOWN : NULL),
                             alt_exit_tgt_p);

    /* If we are TAKE_OVER_SINGLE_SHOT then the handler routine has promised to
     * restore the original code and supply the appropriate continuation address.
     * As such there is no need for us to copy the code here as we will never use it.
     * (Note not copying the code also gives us a quick fix for the Vista image entry
     * problem in PR 293452 from not yet handling non-reaching cits in hook displaced
     * code PR 268988). FIXME - not having a displaced copy to decode breaks the
     * redirection deoode_as_bb() (but not other deocde routines) uses to hide the
     * hook from the client (see PR 293465 for other reasons we need a better solution
     * to that problem). */
    if (action_after != AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT) {
        /* Map displaced code to original app PCs */
        map_intercept_pc_to_app_pc(displaced_app_pc, tgt_pc,
                                   size + JMP_LONG_LENGTH /* include jmp back */, size,
                                   hook_occludes_instrs);
        if (hook_occludes_instrs) {
            intercept_occlusion_mask &= (ptr_uint_t)tgt_pc;
            LOG(GLOBAL, LOG_ASYNCH, 4,
                "Intercept hook occludes instructions at " PFX ". "
                "Mask is now " PFX ".\n",
                pc, intercept_occlusion_mask);
        }

        /* Copy original instructions to our version, re-relativizing where necessary */
        if (app_code_copy_p != NULL)
            *app_code_copy_p = displaced_app_pc;
        copy_app_code(dcontext, tgt_pc, displaced_app_pc, size, &ilist);
    } else {
        /* single shot hooks shouldn't need a copy of the app code */
        ASSERT(app_code_copy_p == NULL);
    }

    finalize_landing_pad_code(lpad_start, changed_prot);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    if (is_hooked) {
        if (DYNAMO_OPTION(hook_conflict) == HOOKED_TRAMPOLINE_CHAIN) {
            /* we only have to rerelativize rel32, yet indirect
             * branches can also be used by hookers, in which case we
             * don't need to do anything special when copying as bytes
             */

            /* FIXME: now re-relativize at target location */
            ASSERT_NOT_IMPLEMENTED(false);
            ASSERT_NOT_TESTED();
        }
    }

    /* Must return to the displaced app code in the landing pad */
    pc = emit_resume_jmp(pc, displaced_app_pc, tgt_pc, pc);
    our_pc_end = pc;

    /* Replace original code with jmp to our version (after 5-byte backup) */
    /* copy-on-write will give us a copy of this page */
    ok = make_hookable(tgt_pc, JMP_REL32_SIZE, &changed_prot);
    if (!ok) {
        /* FIXME: we fail to insert our hook but for now it is easier
         * to pretend that we succeeded. */
        /* should really return NULL and have callers handle this better */
        return our_pc_end;
    }
    pc = tgt_pc;
    *pc = JMP_REL32_OPCODE;
    pc++;
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(lpad_pc - pc - 4)));
    *((int *)pc) = (int)(ptr_int_t)(lpad_pc - pc - 4);
    /* make our page unwritable now */
    make_unhookable(tgt_pc, JMP_REL32_SIZE, changed_prot);

    ASSERT(our_pc_end != NULL);
    return our_pc_end;
}

/* Assumes that tgt_pc should be unwritable.  Handles hooks with or without
 * a landing pad.  our_pc is the displaced app code to copy to tgt_pc.
 */
static void
un_intercept_call(byte *our_pc, byte *tgt_pc)
{
    bool changed_prot;
    bool ok;
    byte *lpad_entry;
    /* if intercept_call() has failed we shouldn't be un-intercepting */
    if (our_pc == NULL)
        return;

    lpad_entry = (tgt_pc + JMP_REL32_SIZE) + *((int *)(tgt_pc + 1));

    /* restore 1st 5 bytes of original code */
    ok = make_hookable(tgt_pc, JMP_REL32_SIZE, &changed_prot);
    /* if we were able to hook we can't fail on unhook */
    ASSERT(ok || memcmp(tgt_pc, our_pc, JMP_REL32_SIZE) == 0 /* hook wasn't applied */);
    if (!ok) {
        return;
    }
    ASSERT(memcmp(tgt_pc, our_pc, JMP_REL32_SIZE) != 0 /* hook was applied */);
    memcpy(tgt_pc, our_pc, JMP_REL32_SIZE);
    make_unhookable(tgt_pc, JMP_REL32_SIZE, changed_prot);

    /* Redirect the first jump in the landing pad to the hooked address (which we just
     * restored above) - in case someone has chained with our hook.
     */
    ok = make_hookable(lpad_entry, JMP_SIZE, &changed_prot);
    ASSERT(ok);
    if (ok) {
        /* patch jmp to go back to target */
        /* Note - not a hot_patch, caller must have synchronized already  to make the
         * memcpy restore above safe. */
        /* FIXME: this looks wrong for x64 which uses abs jmp */
        insert_relative_target(lpad_entry + 1, tgt_pc, false /* not a hotpatch */);
        make_unhookable(lpad_entry, JMP_SIZE, changed_prot);
    }

    DOLOG(3, LOG_ASYNCH, {
        byte *pc = tgt_pc;
        LOG(GLOBAL, LOG_ASYNCH, 3, "after un-intercepting:\n");
        do {
            /* Use GLOBAL_DCONTEXT here since we may have already
             * called dynamo_thread_exit()
             */
            pc = disassemble_with_bytes(GLOBAL_DCONTEXT, pc, main_logfile);
        } while (pc < tgt_pc + JMP_REL32_SIZE);
    });

    unmap_intercept_pc((app_pc)tgt_pc);
}

/* Returns the syscall wrapper at nt_wrapper to a pristine (unhooked) state. Currently
 * used for -clean_testalert to block the problematic injection of SpywareDoctor (9288)
 * and similar apps. Returns true if syscall wrapper required cleaning */
/* FIXME - use this for our hook conflict squash policy in intercept_syscall_wrapper as
 * this can handle more complicated hooks. */
/* XXX i#1854: we should try and reduce how fragile we are wrt small
 * changes in syscall wrapper sequences.
 */
static bool
clean_syscall_wrapper(byte *nt_wrapper, int sys_enum)
{
    dcontext_t *dcontext = GLOBAL_DCONTEXT;
    instr_t *instr_new, *instr_old = instr_create(dcontext);
    instrlist_t *ilist = instrlist_create(dcontext);
    app_pc pc = nt_wrapper;
    bool hooked = false;
    int sysnum = syscalls[sys_enum];
    uint arg_bytes = syscall_argsz[sys_enum];

    if (nt_wrapper == NULL || sysnum == SYSCALL_NOT_PRESENT)
        goto exit_clean_syscall_wrapper;

        /* syscall wrapper should look like
         * For NT/2000
         * mov eax, sysnum         {5 bytes}
         * lea edx, [esp+4]        {4 bytes}
         * int 2e                  {2 bytes}
         * ret arg_bytes           {1 byte (0 args) or 3 bytes}
         *
         * For XPsp0/XPsp1/2003sp0
         * mov eax, sysnum         {5 bytes}
         * mov edx, VSYSCALL_ADDR  {5 bytes}
         * call edx                {2 bytes}
         * ret arg_bytes           {1 byte (0 args) or 3 bytes}
         *
         * For XPsp2/2003sp1/Vista
         * mov eax, sysnum         {5 bytes}
         * mov edx, VSYSCALL_ADDR  {5 bytes}
         * call [edx]              {2 bytes}
         * ret arg_bytes           {1 byte (0 args) or 3 bytes}
         *
         * For WOW64 (case 3922), there are two types: if setting ecx to 0, xor is used.
         *   mov eax, sysnum         {5 bytes}
         *   mov ecx, wow_index      {5 bytes}  --OR--   xor ecx,ecx  {2 bytes}
         *   lea edx, [esp+4]        {4 bytes}
         *   call fs:0xc0            {7 bytes}
         * On Win7 WOW64 after the call we have an add:
         *   add esp,0x4             {3 bytes}
         *   ret arg_bytes           {1 byte (0 args) or 3 bytes}
         * On Win8 WOW64 we have no ecx (and no post-syscall add):
         *   777311bc b844000100      mov     eax,10044h
         *   777311c1 64ff15c0000000  call    dword ptr fs:[0C0h]
         *   777311c8 c3              ret
         * Win10 WOW64:
         *   77cda610 b8a3010200      mov     eax,201A3h
         *   77cda615 bab0d5ce77      mov     edx,offset ntdll!Wow64SystemServiceCall
         *   77cda61a ffd2            call    edx
         *   77cda61c c3              ret
         *
         * For win8 sysenter we have a co-located "inlined" callee:
         *   77d7422c b801000000      mov     eax,1
         *   77d74231 e801000000      call    ntdll!NtYieldExecution+0xb (77d74237)
         *   77d74236 c3              ret
         *   77d74237 8bd4            mov     edx,esp
         *   77d74239 0f34            sysenter
         *   77d7423b c3              ret
         * But we instead do the equivalent call to KiFastSystemCall.
         *
         * x64 syscall (PR 215398):
         *   mov r10, rcx          {3 bytes}
         *   mov eax, sysnum       {5 bytes}
         *   syscall               {2 bytes}
         *   ret                   {1 byte}
         *
         * win10-TH2(1511) x64:
         *   4c8bd1          mov     r10,rcx
         *   b843000000      mov     eax,43h
         *   f604250803fe7f01 test    byte ptr [SharedUserData+0x308
         * (00000000`7ffe0308)],1 7503            jne     ntdll!NtContinue+0x15
         * (00007ff9`13185645) 0f05            syscall c3              ret cd2e int 2Eh c3
         * ret
         */

        /* build correct instr list */
#define APP(list, inst) instrlist_append((list), (inst))
#define WIN1511_SHUSRDATA_SYS 0x7ffe0308
#define WIN1511_JNE_OFFS 0x15
#ifdef X64
    APP(ilist,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R10),
                            opnd_create_reg(REG_RCX)));
    APP(ilist,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                             OPND_CREATE_INT32(sysnum)));
    if (get_os_version() >= WINDOWS_VERSION_10_1511) {
        APP(ilist,
            INSTR_CREATE_test(dcontext,
                              OPND_CREATE_MEM8(DR_REG_NULL, WIN1511_SHUSRDATA_SYS),
                              OPND_CREATE_INT8(1)));
        APP(ilist,
            INSTR_CREATE_jcc(dcontext, OP_jne_short,
                             opnd_create_pc(nt_wrapper + WIN1511_JNE_OFFS)));
    }
    APP(ilist, INSTR_CREATE_syscall(dcontext));
    APP(ilist, INSTR_CREATE_ret(dcontext));
#else
    APP(ilist,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                             opnd_create_immed_int(sysnum, OPSZ_4)));
    /* NOTE - the structure of the wrapper depends only on the OS version, not on the
     * syscall method (for ex. using int on XPsp2 just changes the target on the
     * vsyscall page, not the wrapper layout). */
    if (get_os_version() <= WINDOWS_VERSION_2000) {
        APP(ilist,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XDX),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, 4, OPSZ_0)));
        APP(ilist, INSTR_CREATE_int(dcontext, opnd_create_immed_int(0x2e, OPSZ_1)));
    } else if (is_wow64_process(NT_CURRENT_PROCESS)) {
        ASSERT(get_syscall_method() == SYSCALL_METHOD_WOW64);
        if (syscall_uses_wow64_index()) {
            ASSERT(wow64_index != NULL);
            ASSERT(wow64_index[sys_enum] != SYSCALL_NOT_PRESENT);
            if (wow64_index[sys_enum] == 0) {
                APP(ilist,
                    INSTR_CREATE_xor(dcontext, opnd_create_reg(REG_XCX),
                                     opnd_create_reg(REG_XCX)));
            } else {
                APP(ilist,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INT32(wow64_index[sys_enum])));
            }
            APP(ilist,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XDX),
                                 opnd_create_base_disp(REG_XSP, REG_NULL, 0, 4, OPSZ_0)));
        }
        if (get_os_version() >= WINDOWS_VERSION_10) {
            /* create_syscall_instr() won't match the real wrappers */
            APP(ilist,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                     OPND_CREATE_INT32(wow64_syscall_call_tgt)));
            APP(ilist, INSTR_CREATE_call_ind(dcontext, opnd_create_reg(REG_XDX)));
        } else
            APP(ilist, create_syscall_instr(dcontext));
    } else { /* XP or greater */
        if (get_os_version() >= WINDOWS_VERSION_8) {
            /* Win8 does not use ind calls: it calls to a local copy of KiFastSystemCall.
             * We do the next best thing.
             */
            ASSERT(KiFastSystemCall != NULL);
            APP(ilist, INSTR_CREATE_call(dcontext, opnd_create_pc(KiFastSystemCall)));
        } else {
            APP(ilist,
                INSTR_CREATE_mov_imm(
                    dcontext, opnd_create_reg(REG_XDX),
                    OPND_CREATE_INTPTR((ptr_int_t)VSYSCALL_BOOTSTRAP_ADDR)));

            if (use_ki_syscall_routines()) {
                /* call through vsyscall addr to Ki*SystemCall routine */
                APP(ilist,
                    INSTR_CREATE_call_ind(
                        dcontext,
                        opnd_create_base_disp(REG_XDX, REG_NULL, 0, 0, OPSZ_4_short2)));
            } else {
                /* call to vsyscall addr */
                APP(ilist, INSTR_CREATE_call_ind(dcontext, opnd_create_reg(REG_XDX)));
            }
        }
    }
    if (is_wow64_process(NT_CURRENT_PROCESS) && get_os_version() == WINDOWS_VERSION_7) {
        APP(ilist,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP), OPND_CREATE_INT8(4)));
    }

    if (arg_bytes == 0) {
        APP(ilist, INSTR_CREATE_ret(dcontext));
    } else {
        APP(ilist,
            INSTR_CREATE_ret_imm(dcontext, opnd_create_immed_int(arg_bytes, OPSZ_1)));
    }
#endif /* X64 */
#undef APP

    /* we've seen 3 different ways of hooking syscall wrappers :
     * 1) jmp overwriting first 5 bytes (mov eax, sysnum), most common.
     * 2) jmp overwriting second 5 bytes (certain versions of Sygate)
     * 3) overwriting first 8 bytes with push eax (x3) then jmp (Spyware Doctor 9288, A^2
     *      anti-spyware 10414). */

    /* NOTE - we could finish the walk whether hooked or not, but not much point and
     * I don't fully trust are decode routine w/ junk input (if for ex. hook doesn't end
     * on an instr boundary). */
    for (instr_new = instrlist_first(ilist); instr_new != NULL;
         instr_new = instr_get_next(instr_new)) {
        instr_reset(dcontext, instr_old);
        pc = decode(dcontext, pc, instr_old);
        if (!instr_same(instr_new, instr_old) &&
            /* don't consider call to KiFastSystemCall vs inlined sysenter to be a hook */
            !(get_os_version() >= WINDOWS_VERSION_8 &&
              instr_get_opcode(instr_new) == instr_get_opcode(instr_old) &&
              instr_get_opcode(instr_new) == OP_call)) {
            /* We haven't seen hookers where the opcode would match, so in that case
             * seems likely could be our fault (got an immed wrong or something). */
            ASSERT_CURIOSITY(instr_get_opcode(instr_new) != instr_get_opcode(instr_old));
            /* we haven't seen any hook start deeper then the 2nd instruction */
            ASSERT_CURIOSITY(instr_new == instrlist_first(ilist) ||
                             instr_new == instr_get_next(instrlist_first(ilist)));
            hooked = true;
            break;
        }
    }

    LOG(GLOBAL, LOG_SYSCALLS, hooked ? 1U : 2U,
        "Syscall wrapper @ " PFX " syscall_num=0x%03x%s hooked.\n", nt_wrapper, sysnum,
        hooked ? "" : " not");

    if (hooked) {
        bool changed_prot;
        int length = 0, encode_length;
        byte *nxt_pc;
        instr_t *in;

        SYSLOG_INTERNAL_WARNING_ONCE("Cleaning hooked Nt wrapper @" PFX " sysnum=0x%03x",
                                     nt_wrapper, sysnum);
        for (in = instrlist_first(ilist); in != NULL; in = instr_get_next(in))
            length += instr_length(dcontext, in);
        DOLOG(1, LOG_SYSCALLS, {
            LOG(GLOBAL, LOG_SYSCALLS, 1, "Replacing hooked wrapper :\n");
            pc = nt_wrapper;
            /* Note - we may disassemble junk here (if hook doesn't end on instr
             * boundary) but our decode routines should handle it; is debug anyways. */
            while (pc - nt_wrapper < length)
                pc = disassemble_with_bytes(dcontext, pc, GLOBAL);
            LOG(GLOBAL, LOG_SYSCALLS, 1, "With :\n");
            instrlist_disassemble(dcontext, nt_wrapper, ilist, GLOBAL);
        });

        make_hookable(nt_wrapper, length, &changed_prot);
        nxt_pc =
            instrlist_encode(dcontext, ilist, nt_wrapper, false /* no jmp targets */);
        ASSERT(nxt_pc != NULL);
        encode_length = (int)(nxt_pc - nt_wrapper);
        ASSERT(encode_length == length && "clean syscall encoded length mismatch");
        make_unhookable(nt_wrapper, length, changed_prot);

        DOLOG(1, LOG_SYSCALLS, {
            LOG(GLOBAL, LOG_SYSCALLS, 1, "Cleaned wrapper is now :\n");
            pc = nt_wrapper;
            while (pc - nt_wrapper < length)
                pc = disassemble_with_bytes(dcontext, pc, GLOBAL);
        });
    }

exit_clean_syscall_wrapper:
    instr_destroy(dcontext, instr_old);
    instrlist_clear_and_destroy(dcontext, ilist);
    return hooked;
}

/* Inserts a trampoline in a system call wrapper.
 * All uses should end up using dstack -- else watch out for d_r_initstack
 * infinite loop (see comment above).
 * Returns in skip_syscall_pc the native pc for skipping the system call altogether.
 *
 * Since the only safe point is the first instr, and not right at the syscall
 * instr itself (no 5-byte spot there), we have to copy the whole series of app
 * instrs up until the syscall instr into our buffer to be executed prior to the
 * callee.  This means any intercepted syscall from the cache will have that
 * sequence run NATIVELY!  A solution is to set a flag to go back to native
 * after the next syscall, and take over right away, but a little more worrisome
 * than only executing the syscall under DR in terms of potential to miss the
 * re-native trigger.
 *
 * For x64, we still use a 5-byte jump, assuming our main heap is within 2GB of
 * ntdll.dll (xref PR 215395); if not we'll need an auxiliary landing pad
 * trampoline within 2GB (xref PR 250294 where we need to support such
 * trampolines for general hooks).  Also xref PR 245169 on x64 hooking
 * possibilities, none of which is ideal.
 *
 * FIXME: other interception ideas: could do at instr after mov-immed,
 * and arrange own int 2e for win2k, and emulate rest of sequence when
 * handling syscall from handler -- this would eliminate some issues
 * with the pre-syscall sequence copy, but not clear if better overall.
 * Would be nice to have a single shared syscall handler, but since
 * wrappers are stdcall that would be difficult.
 *
 * We allow the callee to execute the syscall itself, and by returning
 * AFTER_INTERCEPT_LET_GO_ALT_DYN, it signals to skip the actual syscall,
 * so we have control returned to the instr after the syscall instr.
 * For AFTER_INTERCEPT_LET_GO or AFTER_INTERCEPT_TAKE_OVER, the syscall
 * instr itself is the next instr to be executed.
 *
 * N.B.: this routine makes assumptions about the exact sequence of instrs in
 * syscall wrappers, in particular that the indirect call to the vsyscall page
 * can be turned into a direct call, which is only safe for XP SP2 if the
 * vsyscall page is not writable, and cannot be made writable, which is what we
 * have observed to be true.
 *
 * XXX i#1854: we should try and reduce how fragile we are wrt small
 * changes in syscall wrapper sequences.
 */

/* Helper function that returns the after-hook pc */
static byte *
syscall_wrapper_ilist(dcontext_t *dcontext, instrlist_t *ilist, /* IN/OUT */
                      byte **ptgt_pc /* IN/OUT */, void *callee_arg,
                      byte *fpo_stack_adjustment, /* OUT OPTIONAL */
                      byte **ret_pc /* OUT */, const char *name)
{
    byte *pc, *after_hook_target = NULL;
    byte *after_mov_immed;
    instr_t *instr, *hook_return_instr = NULL;
    int opcode = OP_UNDECODED;
    int sys_enum = (int)(ptr_uint_t)callee_arg;
    int native_sys_num = syscalls[sys_enum];

    pc = *ptgt_pc;
    /* we need 5 bytes for a jump, and we assume that the first instr
     * (2nd instr for x64, where we skip the 1st) is a 5-byte mov immed!
     */
    instr = instr_create(dcontext);
    pc = decode(dcontext, pc, instr);
    after_mov_immed = pc;
    /* FIXME: handle other hookers gracefully by chaining!
     * Note that moving trampoline point 5 bytes in could help here (see above).
     */
#ifndef X64
    ASSERT(instr_length(dcontext, instr) >= 5);
#endif
    if (fpo_stack_adjustment != NULL)
        *fpo_stack_adjustment = 0; /* for GBOP case 7127 */

    if (instr_is_cti(instr)) {
        /* we only have to rerelativize rel32, yet indirect
         * branches can also be used by hookers, in which case we
         * don't need to do anything special when copying as bytes
         * FIXME: should we still die?
         */

        /* see case 2525 for background discussion */
        if (DYNAMO_OPTION(native_exec_hook_conflict) == HOOKED_TRAMPOLINE_DIE) {
            /* We could still print the message but we don't have to kill the app here */
            FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                              get_application_pid());
        } else if (DYNAMO_OPTION(native_exec_hook_conflict) == HOOKED_TRAMPOLINE_CHAIN) {
            /* We assume 5-byte hookers as well - so only need to relativize in our own
             * copy, and we need to introduce a PUSH in case of a CALL here
             */
            ASSERT(instr_get_opcode(instr) != OP_call_ind);
            if (instr_is_mbr(instr)) {
                /* one can imagine mbr being used on x64 */
                FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                                  get_application_pid());
            }
            if (instr_get_opcode(instr) == OP_call) {
                LOG(GLOBAL, LOG_ASYNCH, 2,
                    "intercept_syscall_wrapper: mangling hooked call at " PFX "\n", pc);
                /* replace the call w/ a push/jmp hoping this will
                 * eventually return to us unless the hooker decides
                 * to squash the system call or execute without going
                 * back here.
                 * FIXME: keep in mind the code on the instrlist is executed natively
                 */
                insert_push_immed_ptrsz(dcontext, (ptr_int_t)pc, ilist, NULL, NULL, NULL);
#ifdef X64
                /* check reachability from new location */
                /* allow interception code to be up to a page: don't bother
                 * to calculate exactly where our jmp will be encoded */
                if (!REL32_REACHABLE(interception_cur_pc,
                                     opnd_get_pc(instr_get_target(instr))) ||
                    !REL32_REACHABLE(interception_cur_pc + PAGE_SIZE,
                                     opnd_get_pc(instr_get_target(instr)))) {
                    FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                                      get_application_pid());
                }
#endif
                instrlist_append(
                    ilist,
                    INSTR_CREATE_jmp(
                        dcontext, opnd_create_pc(opnd_get_pc(instr_get_target(instr)))));
                /* skip original instruction */
                instr_destroy(dcontext, instr);
                /* interp still needs to be updated */
                ASSERT_NOT_IMPLEMENTED(false);
            } else if (instr_get_opcode(instr) == OP_jmp) {
                /* FIXME - no good way to regain control after the hook */
                ASSERT_NOT_IMPLEMENTED(false);
                LOG(GLOBAL, LOG_ASYNCH, 2,
                    "intercept_syscall_wrapper: hooked with jmp " PFX "\n", pc);
                /* just append instruction as is */
                instrlist_append(ilist, instr);
            } else {
                ASSERT_NOT_IMPLEMENTED(false && "unchainable CTI");
                /* FIXME PR 215397: need to re-relativize pc-relative memory reference */
                IF_X64(ASSERT_NOT_IMPLEMENTED(!instr_has_rel_addr_reference(instr)));
                /* just append instruction as is, emit re-relativises if necessary */
                instrlist_append(ilist, instr);
                /* FIXME: if instr's length doesn't match normal 1st instr we'll
                 * get off down below: really shouldn't continue here */
            }
        } else if (DYNAMO_OPTION(native_exec_hook_conflict) == HOOKED_TRAMPOLINE_SQUASH) {
            SYSLOG_INTERNAL_WARNING("intercept_syscall_wrapper: "
                                    "squashing hook in %s @" PFX,
                                    name, pc);
            LOG(GLOBAL, LOG_ASYNCH, 2,
                "intercept_syscall_wrapper: squashing hooked syscall %s %02x at " PFX
                "\n",
                name, native_sys_num, pc);
#ifdef X64
            /* in this case we put our hook at the 1st instr */
            instrlist_append(ilist,
                             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R10),
                                                 opnd_create_reg(REG_RCX)));
#endif
            /* we normally ASSERT that 1st instr is always mov imm -> eax */
            instrlist_append(ilist,
                             INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                                                  OPND_CREATE_INT32(native_sys_num)));
            /* FIXME: even if we detach we don't restore the original
             * values, since what we have here should be good enough
             */
            /* skip original instruction */
            instr_destroy(dcontext, instr);
        } else if (DYNAMO_OPTION(native_exec_hook_conflict) ==
                   HOOKED_TRAMPOLINE_HOOK_DEEPER) {
            /* move our hook one instruction deeper assuming hooker will
             * return to right after the hook, verify that's an
             * instruction boundary */
#ifdef X64
            /* not much room for two hooks before the syscall; we don't support
             * for now */
            ASSERT_NOT_REACHED();
            FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                              get_application_pid());
#else
            ASSERT(instr_length(dcontext, instr) == 5 /* length of normal mov_imm */);
            *ptgt_pc = pc;
            /* skip original instruction */
            instr_destroy(dcontext, instr);
#endif
        } else if (DYNAMO_OPTION(native_exec_hook_conflict) ==
                   HOOKED_TRAMPOLINE_NO_HOOK) {
            SYSLOG_INTERNAL_WARNING("intercept_syscall_wrapper: "
                                    "not hooking %s due to conflict @" PFX,
                                    name, pc);
            LOG(GLOBAL, LOG_ASYNCH, 2,
                "intercept_syscall_wrapper: not hooking syscall %s %02x at " PFX "\n",
                name, native_sys_num, pc);
            instr_destroy(dcontext, instr);
            return NULL;
        } else {
            ASSERT_NOT_REACHED();
            FATAL_USAGE_ERROR(TAMPERED_NTDLL, 2, get_application_name(),
                              get_application_pid());
        }
    } else {
#ifdef X64
        /* first instr is mov rcx -> r10, which we skip to reach the 5-byte mov immed */
        ASSERT(instr_get_opcode(instr) == OP_mov_ld &&
               opnd_is_reg(instr_get_src(instr, 0)) &&
               opnd_get_reg(instr_get_src(instr, 0)) == REG_RCX &&
               opnd_is_reg(instr_get_dst(instr, 0)) &&
               opnd_get_reg(instr_get_dst(instr, 0)) == REG_R10);
        /* we hook after the 1st instr.  will this confuse other hookers who
         * will think there currently is no hook b/c not on 1st instr? */
        *ptgt_pc = pc;
        instr_destroy(dcontext, instr);
        /* now decode the 2nd instr which should be a mov immed */
        DOLOG(3, LOG_ASYNCH, { disassemble_with_bytes(dcontext, pc, main_logfile); });
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);
        ASSERT(instr_length(dcontext, instr) == 5 /* length of normal mov_imm */);
        opcode = instr_get_opcode(instr);
        /* now fall through */
#endif
        /* normally a mov eax, native_sys_num */
        ASSERT(instr_get_opcode(instr) == OP_mov_imm);
        ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) == native_sys_num);
        LOG(GLOBAL, LOG_ASYNCH, 3,
            "intercept_syscall_wrapper: hooked syscall %02x at " PFX "\n", native_sys_num,
            pc);
        /* append instruction (non-CTI) */
        instrlist_append(ilist, instr);
    }

#ifdef X64
    /* 3rd instr: syscall */
    instr = instr_create(dcontext);
    after_hook_target = pc;
    pc = decode(dcontext, pc, instr);
    /* i#1825: win10 TH2 has a test;jne here */
    if (instr_get_opcode(instr) == OP_test) {
        instrlist_append(ilist, instr);
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);
        ASSERT(instr_get_opcode(instr) == OP_jne_short);
        /* Avoid the encoder trying to re-relativize. */
        instr_set_rip_rel_valid(instr, false);
        instrlist_append(ilist, instr);
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);
    }
    *ret_pc = pc;
    ASSERT(instr_get_opcode(instr) == OP_syscall);
    instr_destroy(dcontext, instr);
#else
    if (get_syscall_method() == SYSCALL_METHOD_WOW64 &&
        get_os_version() >= WINDOWS_VERSION_8 &&
        get_os_version() <= WINDOWS_VERSION_8_1) {
        ASSERT(!syscall_uses_wow64_index());
        /* second instr is a call*, what we consider the system call instr */
        after_hook_target = pc;
        instr = instr_create(dcontext);
        *ret_pc = decode(dcontext, pc, instr); /* skip call* to skip syscall */
        ASSERT(instr_get_opcode(instr) == OP_call_ind);
        instr_destroy(dcontext, instr);
        /* XXX: how handle chrome hooks on win8?  (xref i#464) */
    } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
               get_os_version() >= WINDOWS_VERSION_8) {
        /* Second instr is a call to an inlined routine that calls sysenter.
         * We treat this in a similar way to call* to sysenter which is handled
         * down below.
         * XXX: could share a little bit of code but not much.
         */
        after_hook_target = pc;
        instr = instr_create(dcontext);
        *ret_pc = decode(dcontext, pc, instr); /* skip call to skip syscall */
        ASSERT(instr_get_opcode(instr) == OP_call);

        /* replace the call w/ a push */
        instrlist_append(
            ilist,
            INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INTPTR((ptr_int_t)*ret_pc)));

        /* the callee, inlined later in wrapper, or KiFastSystemCall */
        pc = (byte *)opnd_get_pc(instr_get_target(instr));

        /* fourth instr: mov %xsp -> %xdx */
        instr_reset(dcontext, instr); /* re-use call container */
        pc = decode(dcontext, pc, instr);
        instrlist_append(ilist, instr);
        ASSERT(instr_get_opcode(instr) == OP_mov_ld);

        /* fifth instr: sysenter */
        instr = instr_create(dcontext);
        after_hook_target = pc;
        pc = decode(dcontext, pc, instr);
        ASSERT(instr_get_opcode(instr) == OP_sysenter);
        instr_destroy(dcontext, instr);

        /* ignore ret after sysenter, we'll return to ret after call */

    } else {
        /* second instr is either a lea, a mov immed, or an xor */
        DOLOG(3, LOG_ASYNCH, { disassemble_with_bytes(dcontext, pc, main_logfile); });
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);
        instrlist_append(ilist, instr);
        opcode = instr_get_opcode(instr);
    }
    if (after_hook_target != NULL) {
        /* all set */
    } else if (get_syscall_method() == SYSCALL_METHOD_WOW64 &&
               get_os_version() >= WINDOWS_VERSION_10) {
        ASSERT(!syscall_uses_wow64_index());
        ASSERT(opcode == OP_mov_imm);
        /* third instr is a call*, what we consider the system call instr */
        after_hook_target = pc;
        instr = instr_create(dcontext);
        *ret_pc = decode(dcontext, pc, instr); /* skip call* to skip syscall */
        ASSERT(instr_get_opcode(instr) == OP_call_ind);
        instr_destroy(dcontext, instr);
    } else if (get_syscall_method() == SYSCALL_METHOD_WOW64) {
        ASSERT(opcode == OP_xor || opcode == OP_mov_imm);
        /* third instr is a lea */
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);

        if (instr_get_opcode(instr) == OP_jmp_ind) {
            /* Handle chrome hooks (i#464) via targeted handling since these
             * don't look like any other hooks we've seen.  We can generalize if
             * we later find similar-looking hooks elsewhere.
             * They look like this:
             *    ntdll!NtMapViewOfSection:
             *    77aafbe0 b825000000       mov     eax,0x25
             *    77aafbe5 ba28030a00       mov     edx,0xa0328
             *    77aafbea ffe2             jmp     edx
             *    77aafbec c215c0           ret     0xc015
             *    77aafbef 90               nop
             *    77aafbf0 0000             add     [eax],al
             *    77aafbf2 83c404           add     esp,0x4
             *    77aafbf5 c22800           ret     0x28
             * We put in the native instrs in our hook so our stuff
             * operates correctly, and assume the native state change
             * won't affect the chrome hook code.  We resume
             * right after the 1st mov-imm-eax instr.  These are the native
             * instrs for all chrome hooks in ntdll (Nt{,Un}MapViewOfSection),
             * which are put in place from the parent, so they're there when we
             * initialize and aren't affected by -handle_ntdll_modify:
             *    77aafbe5 33c9             xor     ecx,ecx
             *    77aafbe7 8d542404         lea     edx,[esp+0x4]
             */
            instr_t *tmp = instrlist_last(ilist);
            instrlist_remove(ilist, tmp);
            instr_destroy(dcontext, tmp);
            instr_destroy(dcontext, instr);
            ASSERT(syscall_uses_wow64_index()); /* else handled above */
            ASSERT(wow64_index != NULL);
            if (wow64_index[sys_enum] == 0) {
                instrlist_append(ilist,
                                 INSTR_CREATE_xor(dcontext, opnd_create_reg(REG_XCX),
                                                  opnd_create_reg(REG_XCX)));
            } else {
                instrlist_append(
                    ilist,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INT32(wow64_index[sys_enum])));
            }
            instrlist_append(ilist,
                             INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XDX),
                                              opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                                    0x4, OPSZ_lea)));
            after_hook_target = after_mov_immed;
            /* skip chrome hook to skip syscall: target "add esp,0x4" */
#    define CHROME_HOOK_DISTANCE_JMP_TO_SKIP 6
            *ret_pc = pc + CHROME_HOOK_DISTANCE_JMP_TO_SKIP;
            DOCHECK(1, {
                instr = instr_create(dcontext);
                decode(dcontext, *ret_pc, instr);
                ASSERT(instr_get_opcode(instr) == OP_add);
                instr_destroy(dcontext, instr);
            });
        } else {
            ASSERT(instr_get_opcode(instr) == OP_lea);
            instrlist_append(ilist, instr);

            /* fourth instr is a call*, what we consider the system call instr */
            after_hook_target = pc;
            instr = instr_create(dcontext);
            *ret_pc = decode(dcontext, pc, instr); /* skip call* to skip syscall */
            ASSERT(instr_get_opcode(instr) == OP_call_ind);
            instr_destroy(dcontext, instr);
        }
    } else if (opcode == OP_mov_imm) {
        ptr_int_t immed = opnd_get_immed_int(instr_get_src(instr, 0));
        ASSERT(PAGE_START(immed) == (ptr_uint_t)VSYSCALL_PAGE_START_BOOTSTRAP_VALUE);
        ASSERT(get_syscall_method() == SYSCALL_METHOD_SYSENTER);
        ASSERT(get_os_version() >= WINDOWS_VERSION_XP);

        /* third instr is an indirect call */
        instr = instr_create(dcontext);
        pc = decode(dcontext, pc, instr);
        *ret_pc = pc;
        ASSERT(instr_get_opcode(instr) == OP_call_ind);
        if (fpo_stack_adjustment != NULL) {
            /* for GBOP case 7127 */
            *fpo_stack_adjustment = 4;
        }
        /* replace the call w/ a push */
        instrlist_append(
            ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INTPTR((ptr_int_t)pc)));

        /* the callee, either on vsyscall page or at KiFastSystemCall */
        if (opnd_is_reg(instr_get_src(instr, 0)))
            pc = (byte *)immed;
        else /* KiFastSystemCall */
            pc = *((byte **)immed);

        /* fourth instr: mov %xsp -> %xdx */
        instr_reset(dcontext, instr); /* re-use ind call container */
        pc = decode(dcontext, pc, instr);
        instrlist_append(ilist, instr);
        ASSERT(instr_get_opcode(instr) == OP_mov_ld);

        /* fifth instr: sysenter */
        instr = instr_create(dcontext);
        after_hook_target = pc;
        pc = decode(dcontext, pc, instr);
        ASSERT(instr_get_opcode(instr) == OP_sysenter);
        instr_destroy(dcontext, instr);

        /* ignore ret after sysenter, we'll return to ret after call */
    } else {
        ASSERT(opcode == OP_lea);
        /* third instr: int 2e */
        instr = instr_create(dcontext);
        *ret_pc = decode(dcontext, pc, instr);
        ASSERT(instr_get_opcode(instr) == OP_int);
        /* if we hooked deeper, will need to hook over the int too */
        if (pc - *ptgt_pc < 5 /* length of our hook */) {
            /* Need to add an int 2e to the return path since hook clobbered
             * the original one. We use create_syscall_instr(dcontext) for
             * the sygate int fix. FIXME - the pc will now show up as
             * after_do/share_syscall() but should be ok since anyone
             * checking for those on this thread should have already checked
             * for it being native. */
            hook_return_instr = create_syscall_instr(dcontext);
            after_hook_target = *ret_pc;
            ASSERT(DYNAMO_OPTION(native_exec_hook_conflict) ==
                   HOOKED_TRAMPOLINE_HOOK_DEEPER);
        } else {
            /* point after_hook_target to int 2e */
            after_hook_target = pc;
        }
        instr_destroy(dcontext, instr);
    }
#endif
    return after_hook_target;
}

byte *
intercept_syscall_wrapper(byte **ptgt_pc /* IN/OUT */, intercept_function_t prof_func,
                          void *callee_arg, after_intercept_action_t action_after,
                          app_pc *skip_syscall_pc /* OUT */,
                          byte **orig_bytes_pc /* OUT */,
                          byte *fpo_stack_adjustment /* OUT OPTIONAL */, const char *name)
{
    byte *pc, *emit_pc, *ret_pc = NULL, *after_hook_target = NULL, *tgt_pc;
    byte *lpad_start, *lpad_pc, *lpad_resume_pc, *xl8_start_pc;
    instr_t *instr, *hook_return_instr = NULL;
    instrlist_t ilist;
    bool changed_prot;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool ok;
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;

    instrlist_init(&ilist);

    ASSERT(ptgt_pc != NULL && *ptgt_pc != NULL);

    after_hook_target = syscall_wrapper_ilist(dcontext, &ilist, ptgt_pc, callee_arg,
                                              fpo_stack_adjustment, &ret_pc, name);
    if (after_hook_target == NULL)
        return NULL; /* aborted */

    tgt_pc = *ptgt_pc;
    pc = tgt_pc;
    LOG(GLOBAL, LOG_ASYNCH, 3, "%s: before intercepting:\n", __FUNCTION__);
    DOLOG(3, LOG_ASYNCH, { disassemble_with_bytes(dcontext, pc, main_logfile); });

    pc = interception_cur_pc; /* current spot in interception buffer */

    /* copy original 5 bytes to ease unhooking, we won't execute this */
    *orig_bytes_pc = pc;
    memcpy(pc, tgt_pc, 5);
    pc += 5;

    /* i#901: We need a landing pad b/c ntdll may not be reachable from DR.
     * However, we do not support rip-rel instrs in the syscall wrapper, as by
     * keeping the displaced app code in the intercept buffer and not in the
     * landing pad we can use the standard landing pad layout, the existing
     * emit_landing_pad_code(), the existing is_syscall_trampoline(), and other
     * routines, and also keeps the landing pads themselves a constant size and
     * layout (though the ones here do not have all their space used b/c there's
     * no displaced app code).
     */
    lpad_start = alloc_landing_pad(tgt_pc);
    lpad_pc = lpad_start;
    lpad_pc = emit_landing_pad_code(lpad_pc, pc, after_hook_target,
                                    0 /*no displaced code in lpad*/, &lpad_resume_pc,
                                    &changed_prot);
    /* i#1027: map jmp back in landing pad to original app pc.  We do this to
     * have the translation just in case, even though we hide this jmp from the
     * client.  Xref the PR 219351 comment in is_intercepted_app_pc().
     */
    map_intercept_pc_to_app_pc(lpad_resume_pc, after_hook_target, JMP_LONG_LENGTH, 0,
                               false /* not a hook occlusion */);
    finalize_landing_pad_code(lpad_start, changed_prot);

    emit_pc = pc;
    /* we assume that interception buffer is still writable */

    /* we need to enter at copy of pre-syscall sequence, since we need
     * callee to be at app state exactly prior to syscall instr itself.
     * this means this sequence is executed natively even for syscalls
     * in the cache (since interception code is run natively) -- only
     * worry would be stack faults, whose context we might xlate incorrectly
     *
     * N.B.: bb_process_ubr() assumes that the target of the trampoline
     * is the original mov immed!
     */

    /* insert our copy of app instrs leading up to syscall
     * first instr doubles as the clobbered original code for un-intercepting.
     */
    for (instr = instrlist_first(&ilist); instr != NULL; instr = instr_get_next(instr)) {
        pc = instr_encode(dcontext, instr, pc);
        ASSERT(pc != NULL);
    }
    instrlist_clear(dcontext, &ilist);

    pc = emit_intercept_code(
        dcontext, pc, prof_func, callee_arg, false /*do not assume xsp*/,
        false /*not known to not be on dstack: ok to clobber flags*/, action_after,
        ret_pc /* alternate target to skip syscall */, NULL);

    /* Map interception buffer PCs to original app PCs */
    if (is_in_interception_buffer(pc)) {
        map_intercept_pc_to_app_pc(pc, tgt_pc, 10 /* 5 bytes + jmp back */, 5,
                                   false /* not a hook occlusion */);
    }

    /* The normal target, for really doing the system call native, used
     * for letting go normally and for take over.
     * We already did pre-syscall sequence, so we go straight to syscall itself.
     */
    /* have to include syscall instr here if we ended up hooking over it */
    xl8_start_pc = pc;
    if (hook_return_instr != NULL) {
        pc = instr_encode(dcontext, hook_return_instr, pc);
        ASSERT(pc != NULL);
        instr_destroy(dcontext, hook_return_instr);
    }
    pc = emit_resume_jmp(pc, lpad_resume_pc, tgt_pc, xl8_start_pc);

    /* update interception buffer pc */
    interception_cur_pc = pc;

    /* Replace original code with jmp to our version's entrance */
    /* copy-on-write will give us a copy of this page */
    ok = make_hookable(tgt_pc, 5, &changed_prot);
    if (ok) {
        ptr_int_t offset = (lpad_pc - (tgt_pc + 5));
#ifdef X64
        if (!REL32_REACHABLE_OFFS(offset)) {
            ASSERT_NOT_IMPLEMENTED(false && "PR 245169: hook target too far: NYI");
            /* FIXME PR 245169: we need use landing_pad_areas to alloc landing
             * pads to trampolines, as done for PR 250294.
             */
        }
#endif
        pc = tgt_pc;
        *pc = JMP_REL32_OPCODE;
        pc++;
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(offset)));
        *((int *)pc) = (int)offset;
    }
    /* make our page unwritable now */
    make_unhookable(tgt_pc, 5, changed_prot);

    if (skip_syscall_pc != NULL)
        *skip_syscall_pc = ret_pc;

    return emit_pc;
}

/* two convenience routines for intercepting using the code[] buffer
 * after the initialization routine has completed
 *
 * WARNING: only call this when there is only one thread going!
 * This is not thread-safe!
 */
byte *
insert_trampoline(byte *tgt_pc, intercept_function_t prof_func, void *callee_arg,
                  bool assume_xsp, after_intercept_action_t action_after,
                  bool cti_safe_to_ignore)
{
    byte *pc = interception_cur_pc;
    /* make interception code writable, NOTE the interception code may
     * be in vmareas executable list, we make the interception code temporarily
     * writable here without removing or flushing the region, this is ok since
     * we should be single threaded when this function is called and we never
     * overwrite existing interception code */
    DEBUG_DECLARE(bool ok =)
    make_writable(interception_code, INTERCEPTION_CODE_SIZE);
    ASSERT(ok);

    /* FIXME: worry about inserting trampoline across bb boundaries? */
    interception_cur_pc =
        intercept_call(interception_cur_pc, tgt_pc, prof_func, callee_arg, assume_xsp,
                       action_after, false, /* need the trampoline at all costs */
                       cti_safe_to_ignore, NULL, NULL);
    /* FIXME: we assume early intercept_call failures are ok to
     * ignore.  Note we may want to crash instead if trying to sandbox
     * malicious programs that may be able to prevent us from
     * committing memory.
     */

    ASSERT(interception_cur_pc - interception_code < INTERCEPTION_CODE_SIZE);

    /* return interception code to read only state */
    make_unwritable(interception_code, INTERCEPTION_CODE_SIZE);

    return pc;
}

void
remove_trampoline(byte *our_pc, byte *tgt_pc)
{
    un_intercept_call(our_pc, tgt_pc);
}

bool
is_in_interception_buffer(byte *pc)
{
    return (pc >= interception_code && pc < interception_code + INTERCEPTION_CODE_SIZE);
}

bool
is_part_of_interception(byte *pc)
{
    return (is_in_interception_buffer(pc) ||
            vmvector_overlap(landing_pad_areas, pc, pc + 1));
}

bool
is_on_interception_initial_route(byte *pc)
{
    if (vmvector_overlap(landing_pad_areas, pc, pc + 1)) {
        /* Look for the forward jump.  For x64, any ind jmp will do, as reverse
         * jmp is direct.
         */
        if (IF_X64_ELSE(*pc == JMP_ABS_IND64_OPCODE &&
                            *(pc + 1) == JMP_ABS_MEM_IND64_MODRM,
                        *pc == JMP_REL32_OPCODE &&
                            is_in_interception_buffer(PC_RELATIVE_TARGET(pc + 1)))) {

            return true;
        }
    }
    return false;
}

bool
is_syscall_trampoline(byte *pc, byte **tgt)
{
    if (syscall_trampolines_start == NULL)
        return false;
    if (vmvector_overlap(landing_pad_areas, pc, pc + 1)) {
        /* Also count the jmp from landing pad back to syscall instr, which is
         * immediately after the jmp from landing pad to interception buffer (i#1027).
         */
        app_pc syscall;
        if (is_jmp_rel32(pc, pc, &syscall) &&
            is_jmp_rel32(pc - JMP_LONG_LENGTH, NULL, NULL)) {
            dcontext_t *dcontext = get_thread_private_dcontext();
            instr_t instr;
            if (dcontext == NULL)
                dcontext = GLOBAL_DCONTEXT;
            instr_init(dcontext, &instr);
            decode(dcontext, syscall, &instr);
            if (instr_is_syscall(&instr)) {
                /* proceed using the 1st jmp */
                pc -= JMP_LONG_LENGTH;
            }
            instr_free(dcontext, &instr);
        }
#ifdef X64
        /* target is 8 bytes back */
        pc = *(app_pc *)(pc - sizeof(app_pc));
#else
        if (!is_jmp_rel32(pc, pc, &pc))
            return false;
#endif
    }
    if (pc >= syscall_trampolines_start && pc < syscall_trampolines_end) {
        if (tgt != NULL)
            *tgt = pc;
        return true;
    }
    return false;
}

static void
instrument_dispatcher(dcontext_t *dcontext, dr_kernel_xfer_type_t type,
                      app_state_at_intercept_t *state, CONTEXT *interrupted_cxt)
{
    app_pc nohook_pc = dr_fragment_app_pc(state->start_pc);
    state->mc.pc = nohook_pc;
    DWORD orig_flags = 0;
    if (interrupted_cxt != NULL) {
        /* Avoid copying simd fields: this event does not provide them. */
        orig_flags = interrupted_cxt->ContextFlags;
        interrupted_cxt->ContextFlags &=
            ~(CONTEXT_DR_STATE & ~(CONTEXT_INTEGER | CONTEXT_CONTROL));
    }
    if (instrument_kernel_xfer(dcontext, type, interrupted_cxt, NULL, NULL, state->mc.pc,
                               state->mc.xsp, NULL, &state->mc, 0) &&
        state->mc.pc != nohook_pc)
        state->start_pc = state->mc.pc;
    if (interrupted_cxt != NULL)
        interrupted_cxt->ContextFlags = orig_flags;
}

/****************************************************************************
 */
/* TRACK_NTDLL: try to find where kernel re-emerges into user mode when it
 * dives into kernel mode
 */
#if TRACK_NTDLL
static byte *
make_writable_incr(byte *pc)
{
    PBYTE pb = (PBYTE)pc;
    MEMORY_BASIC_INFORMATION mbi;
    DWORD old_prot;
    int res;

    res = query_virtual_memory(pb, &mbi, sizeof(mbi));
    ASSERT(res == sizeof(mbi));

    res = protect_virtual_memory(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_WRITECOPY,
                                 &old_prot);
    ASSERT(res);
    return (byte *)((int)mbi.BaseAddress + (int)mbi.RegionSize);
}

static byte *
make_inaccessible(byte *pc)
{
    PBYTE pb = (PBYTE)pc;
    MEMORY_BASIC_INFORMATION mbi;
    DWORD old_prot;
    int res;

    res = query_virtual_memory(pb, &mbi, sizeof(mbi));
    ASSERT(res == sizeof(mbi));

    res =
        protect_virtual_memory(mbi.BaseAddress, mbi.RegionSize, PAGE_NOACCESS, &old_prot);
    ASSERT(res);
    return (byte *)((int)mbi.BaseAddress + (int)mbi.RegionSize);
}

void
wipe_out_ntdll()
{
    byte *start = (byte *)0x77F81000;
    byte *stop = (byte *)0x77FCD95B;
    byte *pc;

    /* first suspend all other threads */
    thread_record_t **threads;
    int i, num_threads;
    d_r_mutex_lock(&thread_initexit_lock);
    get_list_of_threads(&threads, &num_threads);
    for (i = 0; i < num_threads; i++) {
        if (threads[i]->id != d_r_get_thread_id()) {
            LOG(GLOBAL, LOG_ASYNCH, 1, "Suspending thread " TIDFMT " == " PFX "\n",
                tr->id, tr->handle);
            SuspendThread(threads[i]->handle);
        }
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    global_heap_free(threads,
                     num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));

    LOG(GLOBAL, LOG_ASYNCH, 1, "INVALIDATING ENTIRE NTDLL.DLL!!!\n");
    pc = start;
    while (pc < stop) {
        LOG(GLOBAL, LOG_ASYNCH, 1, "\t" PFX "\n", pc);
#    if 0
        pc = make_inaccessible(pc);
#    else
        pc = make_writable_incr(pc);
#    endif
    }
#    if 1
    for (pc = start; pc < stop; pc++) {
        *pc = 0xcc;
    }
#    endif
}
#endif /* TRACK_NTDLL */
/*
 ****************************************************************************/

/* If we receive an asynch event while we've lost control but before we
 * reach the image entry point or our other retakeover points we should
 * retakeover, to minimize the amount of code run natively -- these should
 * be rare during init and perf hit of repeated flushing and re-walking
 * memory list shouldn't be an issue.
 * Separated from asynch_take_over to not force its callers to do this.
 */
static inline void
asynch_retakeover_if_native()
{
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    ASSERT(tr != NULL);
    if (IS_UNDER_DYN_HACK(tr->under_dynamo_control)) {
        ASSERT(!reached_image_entry_yet());
        /* must do a complete takeover-after-native */
        retakeover_after_native(tr, INTERCEPT_EARLY_ASYNCH);
    }
}

/* This routine is called by a DynamoRIO routine that was invoked natively,
 * i.e., not under DynamoRIO control.
 * This routine takes control using the application state in its arguments,
 * and starts execution under DynamoRIO at start_pc.
 * state->callee_arg is a boolean "save_dcontext":
 * If save_dcontext is true, it saves the cur dcontext on the callback stack
 * of dcontexts and proceeds to execute with a new dcontext.
 * Otherwise, it uses the current dcontext, which has its trace squashed.
 */
static void
asynch_take_over(app_state_at_intercept_t *state)
{
    dcontext_t *dcontext;
    bool save_dcontext = (bool)(ptr_uint_t)state->callee_arg;
    if (save_dcontext) {
        /* save cur dcontext and get a new one */
        dcontext = callback_setup(state->start_pc);
    } else {
        dcontext = get_thread_private_dcontext();
        ASSERT(dcontext->initialized);
        /* case 9347 we want to let go after image entry point */
        if (RUNNING_WITHOUT_CODE_CACHE() &&
            dcontext->next_tag == BACK_TO_NATIVE_AFTER_SYSCALL &&
            state->start_pc == image_entry_pc) {
            ASSERT(dcontext->native_exec_postsyscall == image_entry_pc);
        } else {
            ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
            dcontext->next_tag = state->start_pc;
        }
        /* if we were building a trace, kill it */
        if (is_building_trace(dcontext)) {
            LOG(THREAD, LOG_ASYNCH, 2, "asynch_take_over: squashing old trace\n");
            trace_abort(dcontext);
        }
    }
    ASSERT(os_using_app_state(dcontext));
    LOG(THREAD, LOG_ASYNCH, 2, "asynch_take_over 0x%08x\n", state->start_pc);
    /* may have been inside syscall...now we're in app! */
    set_at_syscall(dcontext, false);
    /* tell d_r_dispatch() why we're coming there */
    if (dcontext->whereami != DR_WHERE_APP) /* new thread, typically: leave it that way */
        dcontext->whereami = DR_WHERE_TRAMPOLINE;
    set_last_exit(dcontext, (linkstub_t *)get_asynch_linkstub());

    transfer_to_dispatch(dcontext, &state->mc, false /*!full_DR_state*/);
    ASSERT_NOT_REACHED();
}

bool
new_thread_is_waiting_for_dr_init(thread_id_t tid, app_pc pc)
{
    uint i;
    /* i#1443c#4: check for a thread that's about to hit our hook */
    if (pc == LdrInitializeThunk || pc == (app_pc)KiUserApcDispatcher)
        return true;
    /* We check until the max to avoid races on threads_waiting_count */
    for (i = 0; i < MAX_THREADS_WAITING_FOR_DR_INIT; i++) {
        if (threads_waiting_for_dr_init[i] == tid)
            return true;
    }
    return false;
}

static void
possible_new_thread_wait_for_dr_init(CONTEXT *cxt)
{
    /* Because of problems with injected threads while we are initializing
     * (case 5167, 5020, 5103 bunch of others) we block here while the main
     * thread finishes initializing. Once dynamo_exited is set is safe to
     * let the thread continue since dynamo_thread_init will imediately
     * return. */
    uint idx;
    /* We allow a client init routine to create client threads: DR is
     * initialized enough by now
     */
    if (is_new_thread_client_thread(cxt, NULL))
        return;

    if (dynamo_initialized || dynamo_exited)
        return;

    /* i#1443: communicate with os_take_over_all_unknown_threads() */
    idx = atomic_add_exchange_int((volatile int *)&threads_waiting_count, 1);
    idx--; /* -1 to get index from count */
    ASSERT(idx < MAX_THREADS_WAITING_FOR_DR_INIT);
    if (idx >= MAX_THREADS_WAITING_FOR_DR_INIT) {
        /* What can we do?  We'll have to risk it and hope this thread is scheduled
         * and initializes before os_take_over_all_unknown_threads() runs.
         */
    } else {
        threads_waiting_for_dr_init[idx] = d_r_get_thread_id();
    }

    while (!dynamo_initialized && !dynamo_exited) {
        STATS_INC(apc_yields_while_initializing);
        os_thread_yield();
    }

    if (idx < MAX_THREADS_WAITING_FOR_DR_INIT) {
        /* os_take_over_all_unknown_threads()'s context check will work from here */
        threads_waiting_for_dr_init[idx] = INVALID_THREAD_ID;
    }
}

/* returns true if intercept function should return immediately and let go,
 * false if intercept function should continue processing and maybe takeover */
static bool
intercept_new_thread(CONTEXT *cxt)
{
    bool is_client = false;
    byte *dstack = NULL;
    priv_mcontext_t mc;
    /* init apc, check init_apc_go_native to sync w/detach */
    if (init_apc_go_native) {
        /* need to wait after checking _go_native to avoid a thread
         * going native too early because of races between setting
         * _go_native and _pause */
        if (init_apc_go_native_pause) {
            /* FIXME : this along with any other logging in this
             * method could potentially be race condition with detach
             * cleanup, though is unlikely */
            LOG(GLOBAL, LOG_ALL, 2, "Thread waiting at init_apc for detach to finish\n");
        }
        while (init_apc_go_native_pause) {
            os_thread_yield();
        }
        /* just return, FIXME : see concerns in detach_helper about
         * getting to native code before the interception_code is
         * freed and getting out of here before the dll is unloaded
         */
#if 0 /* this is not a dynamo controlled thread! */
        SELF_PROTECT_LOCAL(get_thread_private_dcontext(), READONLY);
#endif
        return true /* exit intercept function and let go */;
    }

    /* should keep in sync with changes in intercept_image_entry() for
     * thread initialization
     */

    /* initialize thread now */
    /* i#41/PR 222812: client threads target a certain routine and always
     * directly never via win API (so we don't check THREAT_START_ADDR)
     */
    is_client = is_new_thread_client_thread(cxt, &dstack);
    if (is_client) {
        ASSERT(is_dynamo_address(dstack));
        /* i#2335: We support setup separate from start, and we want
         * to allow a client to create a client thread during init,
         * but we do not support that thread executing until the app
         * has started (b/c we have no signal handlers in place).
         */
        /* i#3973: On unix, there are some race conditions that can
         * occur if the client thread starts executing before
         * dynamo_initialized is set.  Although we are not aware of
         * such a race condition on windows, we are proactively
         * delaying client thread execution until after the app has
         * started (and thus after dynamo_initialized is set.
         */
        wait_for_event(dr_app_started, 0);
    }
    /* FIXME i#2718: we want the app_state_at_intercept_t context, which is
     * the actual code to be run by the thread *now*, and not this CONTEXT which
     * is what will be run later!  We should make sure nobody is relying on
     * the current (broken) context first.
     */
    context_to_mcontext_new_thread(&mc, cxt);
    if (dynamo_thread_init(dstack, &mc, NULL, is_client) != -1) {
        app_pc thunk_xip = (app_pc)cxt->CXT_XIP;
        dcontext_t *dcontext = get_thread_private_dcontext();
        LOG_DECLARE(char sym_buf[MAXIMUM_SYMBOL_LENGTH];)
        bool is_nudge_thread = false;

        if (is_client) {
            /* PR 210591: hide our threads from DllMain by not executing rest
             * of Ldr init code and going straight to target.  our_create_thread()
             * already set up the arg in cxt.
             */
            nt_continue(cxt);
            ASSERT_NOT_REACHED();
        }

        /* Xref case 552, to ameliorate the risk of an attacker
         * leveraging our detach routines etc. against us, we detect
         * an incoming nudge thread here during thread init and set
         * a dcontext flag that the nudge routines can later verify.
         * Attacker could still bypass if can control the start addr
         * of a new thread (FIXME).  We check both Xax and Xip since
         * nodemgr has the ability to target directly or send through
         * kernel32 start thunk (though only start thunk, i.e. xax,
         * is currently used).  If we move to just directly targeted,
         * i.e. xip, would be a lot harder for the attacker since
         * the documented API routines all hardcode that value.
         *
         * The nudge related checks below were moved above thread_policy checks
         * because there is no dependency and because process control nudge for
         * thin_client needs it; part of cases 8884, 8594 & 8888. */
        ASSERT(dcontext != NULL && dcontext->nudge_target == NULL);
        if ((void *)cxt->CXT_XIP == (void *)generic_nudge_target ||
            (void *)cxt->THREAD_START_ADDR == (void *)generic_nudge_target) {
            LOG(THREAD, LOG_ALL, 1, "Thread targeting nudge.\n");
            if (dcontext != NULL) {
                dcontext->nudge_target = (void *)generic_nudge_target;
            }
            is_nudge_thread = true;
        }
        /* FIXME: temporary fix for case 9467 - mute nudges for cygwin apps.
         * Long term fix is to make nudge threads go directly to their targets.
         */
        if (is_nudge_thread && DYNAMO_OPTION(thin_client) && DYNAMO_OPTION(mute_nudge)) {
            TRY_EXCEPT(
                dcontext,
                { /* to prevent crashes when walking the ldr list */
                  PEB *peb = get_own_peb();
                  PEB_LDR_DATA *ldr = peb->LoaderData;
                  LIST_ENTRY *e;
                  LIST_ENTRY *start = &ldr->InLoadOrderModuleList;
                  LDR_MODULE *mod;
                  uint traversed = 0;

                  /* Note: this loader module list walk is racy with the loader;
                   * can't really grab the loader lock here.  Shouldn't be a big
                   * problem as this is a temp fix anyway. */
                  for (e = start->Flink; e != start; e = e->Flink) {
                      mod = (LDR_MODULE *)e;
                      if (wcsstr(mod->BaseDllName.Buffer, L"cygwin1.dll") != NULL) {
                          os_terminate(dcontext, TERMINATE_THREAD | TERMINATE_CLEANUP);
                          ASSERT_NOT_REACHED();
                      }
                      if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
                          SYSLOG_INTERNAL_WARNING("nudge muting: too many modules");
                          break;
                      }
                  }
                },
                { /* do nothing */ });
        }

        /* For thin_client, let go right after we init the thread, i.e., create
         * the dcontext; don't do the thread policy stuff, that requires locks
         * that aren't initialized in this mode!  */
        if (DYNAMO_OPTION(thin_client))
            return true /* exit intercept function and let go */;

            /* In fact the apc_target is ntdll!LdrInitializeThunk
             * (for all threads not only the first one).
             * Note for vista that threads do not start with an apc, but rather
             * directly show up at ntdll!LdrInitializeThunk (which we hook on
             * vista to call this routine).  Note that the thunk will return via
             * an NtContinue to a context on the stack so really we see the same
             * behavior as before except we don't go through the apc dispatcher.
             *
             * For threads created by kernel32!CreateRemoteThread pre vista
             * the cxt->Xip then is kernel32!Base{Process,Thread}StartThunk (not
             * exported), while the cxt->Xax is the user thread procedure and cxt->Xbx
             * is the arg. On vista it's the same except cxt->Xip is set to
             * ntdll!RtlUserThreadStart (which is exported in ntdll.dll) by the
             * kernel.
             *
             * kernel32!BaseProcessStartThunk, or kernel32!BaseThreadStartThunk
             * on all versions I've tested start with
             * 0xed33  xor ebp,ebp
             *
             * Note, of course, that direct NtCreateThread calls
             * can go anywhere they want (including on Vista).  For example toolhelp
             * uses NTDLL!RtlpQueryProcessDebugInformationRemote
             * as the xip so shouldn't count much on this. NtCreateThreadEx threads
             * (vista only) will, however, always have xip=ntdll!RtlUserThreadStart
             * since the kernel sets that.
             */
            /* keep in mind this is a 16-bit match */
#define BASE_THREAD_START_THUNK_USHORT 0xed33

        /* see comments in os.c pre_system_call CreateThread, Xax holds
         * the win32 start address (Nebbett), Xbx holds the argument
         * (observation). Same appears to hold for CreateThreadEx. */
        /* Note that the initial thread won't log here */
        LOG(THREAD_GET, LOG_THREADS, 1,
            "New Thread : Win32 start address " PFX " arg " PFX ", thunk xip=" PFX "\n",
            cxt->THREAD_START_ADDR, cxt->THREAD_START_ARG, cxt->CXT_XIP);
        DOLOG(1, LOG_THREADS, {
            print_symbolic_address((app_pc)cxt->THREAD_START_ADDR, sym_buf,
                                   sizeof(sym_buf), false);
            LOG(THREAD_GET, LOG_THREADS, 1, "Symbol information for start address %s\n",
                sym_buf);
        });
        DOLOG(2, LOG_THREADS, {
            print_symbolic_address((app_pc)cxt->CXT_XIP, sym_buf, sizeof(sym_buf), false);
            LOG(THREAD_GET, LOG_THREADS, 2, "Symbol information for thunk address %s\n",
                sym_buf);
        });

        /* start address should be set at thread initialization */
        if (dcontext->win32_start_addr == (app_pc)cxt->THREAD_START_ARG) {
            /* case 10965/PR 215400: WOW64 & x64 query returns arg for some reason */
#ifndef X64
            ASSERT(is_wow64_process(NT_CURRENT_PROCESS));
#endif
            dcontext->win32_start_addr = (app_pc)cxt->THREAD_START_ADDR;
        }
        ASSERT(dcontext->win32_start_addr == (app_pc)cxt->THREAD_START_ADDR);

#ifdef PROGRAM_SHEPHERDING
        /* We expect target address (xip) to be on our executable list
         * (is usually one of the start thunks). */
        ASSERT_CURIOSITY(executable_vm_area_overlap(thunk_xip, thunk_xip + 2, false));
        /* On vista+ it appears all new threads target RtlUserThreadStart
         * (the kernel sets in in NtCreateThreadEx). Thread created via the legacy
         * NtCreateThread however can target anywhere (such as our internal nudges).
         */
        ASSERT_CURIOSITY(get_os_version() < WINDOWS_VERSION_VISTA || is_nudge_thread ||
                         thunk_xip == RtlUserThreadStart ||
                         /* The security_win32/execept-execution.exe regr test does a
                          * raw create thread using the old NtCreateThread syscall. */
                         check_filter("security-win32.except-execution.exe",
                                      get_short_name(get_application_name())));
        /* check for hooker's shellcode delivered via a remote thread */
        if (TEST(OPTION_ENABLED, DYNAMO_OPTION(thread_policy))) {
            /* Most new threads (and all of the ones that target injected code
             * so far) have xip targeting one of the start thunks.  For these
             * threads the start address we want to apply the policy to is in
             * eax.  However we don't want to apply the policy to the random
             * value in eax if the thread isn't targeting a start thunk (such
             * as injected toolhelp [RtlpQueryProcessDebugInformationRemote]
             * or debugger threads).  For Vista we can check that xip =
             * RtlUserThreadStart, but the kernel32 thunks used pre Vista
             * aren't exported so as a sanity check for those we check if
             * the first few bytes of xip match the kernel32 start thunks.
             * FIXME - this is only a 2 byte comparison (xor ebp,ebp) so a
             * false match is certainly not impossible.  We should try to find
             * a better way to check. Could also check that it's in kernel32
             * etc.
             * FIXME - the deref of cxt->CXT_XIP is potentially unsafe, we
             * assume it's ok since is on the executable list and the thread is
             * about to execute from there.  Should prob. use d_r_safe_read()
             * FIXME - for this to work we're also assuming that the thunks
             * will always be on the executable list.
             */
            if (executable_vm_area_overlap(thunk_xip, thunk_xip + 2, false) &&
                (get_os_version() >= WINDOWS_VERSION_VISTA
                     ? thunk_xip == RtlUserThreadStart
                     : BASE_THREAD_START_THUNK_USHORT == *(ushort *)thunk_xip)) {
                apc_thread_policy_helper((app_pc *)&cxt->THREAD_START_ADDR,
                                         /* target code is in CONTEXT structure */
                                         DYNAMO_OPTION(thread_policy),
                                         THREAD_TARGET_WINDOWS
                                         /* CreateThreadEx target */);
            }
            /* FIXME - threads can directly target new code without going
             * through one of the start thunks (though for our purposes here
             * that's the uncommon case) so we should consider also applying
             * the thread policy to the cxt->CXT_XIP address. Doesn't apply
             * to threads created by NtCreateThreadEx though since they will
             * always go through the RtlUserThreadStart thunk. */
        }
#endif /* PROGRAM_SHEPHERDING */
#ifdef HOT_PATCHING_INTERFACE
        /* For hotp_only, this is where newly created threads should
         * be let go native, i.e., do the thread_policy enforcement.
         */
        if (DYNAMO_OPTION(hotp_only))
            return true /* exit intercept function and let go */;
#endif
    } else {
        ASSERT_NOT_REACHED();
    }
    return false /* continue intercept function and maybe takeover */;
}

/****************************************************************************
 * New Threads
 * On os_versions prior to Vista new threads start KiUserApcDispatcher with an
 * APC to LdrInitializeThunk.  We catch those with our KiUserApcDispatcher
 * hook. On Vista new threads skip the dispatcher and go directly to
 * LdrInitializeThunk (stack is similar to APC, i.e. does an NtContinue to
 * go on) so we need to hook there to catch new threads.
 */

/*
<Vista, note other platforms differ>
ntdll!LdrInitializeThunk:
  77F40229: 8B FF              mov         edi,edi
  77F4022B: 55                 push        ebp
  77F4022C: 8B EC              mov         ebp,esp
  77F4022E: FF 75 0C           push        dword ptr [ebp+0Ch]
  77F40231: FF 75 08           push        dword ptr [ebp+8]
  77F40234: E8 6B 10 00 00     call        ntdll!LdrpInitialize (77F412A4)
  77F40239: 6A 01              push        1
  77F4023B: FF 75 08           push        dword ptr [ebp+8]
  77F4023E: E8 78 2E FF FF     call        ntdll!NtContinue (77F330BB)
  77F40243: 50                 push        eax
  77F40244: E8 EF 49 FF FF     call        ntdll!RtlRaiseStatus (77F34C38)
  77F40249: CC                 int         3
  77F4024A: 90                 nop

 * At interception point esp+4 holds the new threads context (first arg, rcx on 64-bit).
 * FIXME - need code to try and verify this offset during hooking.
 */
#define LDR_INIT_CXT_XSP_OFFSET 0x4

static after_intercept_action_t /* note return value will be ignored */
intercept_ldr_init(app_state_at_intercept_t *state)
{
    CONTEXT *cxt;
#ifdef X64
    cxt = (CONTEXT *)(state->mc.xcx);
#else
    cxt = (*(CONTEXT **)(state->mc.xsp + LDR_INIT_CXT_XSP_OFFSET));
#endif

    /* we only hook this routine on vista+ */
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);

    /* this might be a new thread */
    possible_new_thread_wait_for_dr_init(cxt);

    if (intercept_asynch_for_self(true /*we want unknown threads*/)) {
        if (!is_thread_initialized()) {
            if (intercept_new_thread(cxt))
                return AFTER_INTERCEPT_LET_GO;
            /* We treat this as a kernel xfer, partly b/c of i#2718 where our
             * thread init mcontext is wrong.
             * We pretend it's an APC.
             */
            instrument_dispatcher(get_thread_private_dcontext(), DR_XFER_APC_DISPATCHER,
                                  state, cxt);
        } else {
            /* ntdll!LdrInitializeThunk is only used for initializing new
             * threads so we should never get here unless early injected
             */
            ASSERT(dr_earliest_injected);
        }
        asynch_retakeover_if_native(); /* FIXME - this is unneccesary */
        state->callee_arg = (void *)false /* use cur dcontext */;
        asynch_take_over(state);
    } else {
        /* ntdll!LdrInitializeThunk is only used for initializing new
         * threads so we should never get here */
        ASSERT_NOT_REACHED();
    }

    return AFTER_INTERCEPT_LET_GO;
}

/****************************************************************************
 * APCs
 *
 * Interception routine for an Asynchronous Procedure Call
 * We intercept this point in ntdll:

KiUserApcDispatcher:
  77F9F028: 8D 7C 24 10        lea         edi,[esp+10h]
  77F9F02C: 58                 pop         eax
  77F9F02D: FF D0              call        eax
  77F9F02F: 6A 01              push        1
  77F9F031: 57                 push        edi
  77F9F032: E8 BC 30 FE FF     call        77F820F3 <NtContinue>
  77F9F037: 90                 nop

2003 SP1 looks a little different, w/ SEH code
Vista is similar
KiUserApcDispatcher:
  7c8362c8 8d8424dc020000   lea     eax,[esp+0x2dc]
  7c8362cf 648b0d00000000   mov     ecx,fs:[00000000]
  7c8362d6 baab62837c       mov     edx,0x7c8362ab (KiUserApcExceptionHandler)
  7c8362db 8908             mov     [eax],ecx
  7c8362dd 895004           mov     [eax+0x4],edx
  7c8362e0 64a300000000     mov     fs:[00000000],eax
  7c8362e6 58               pop     eax
  7c8362e7 8d7c240c         lea     edi,[esp+0xc]
  7c8362eb ffd0             call    eax
  7c8362ed 8b8fcc020000     mov     ecx,[edi+0x2cc]
  7c8362f3 64890d00000000   mov     fs:[00000000],ecx
  7c8362fa 6a01             push    0x1
  7c8362fc 57               push    edi
  7c8362fd e88328ffff       call    ntdll!NtContinue (7c828b85)
  7c836302 8bf0             mov     esi,eax
  7c836304 56               push    esi
  7c836305 e88e010000       call    ntdll!RtlRaiseStatus (7c836498)
  7c83630a ebf8             jmp     ntdll!KiUserApcDispatcher+0x3c (7c836304)
  7c83630c c21000           ret     0x10

x64 XP, where they use the PxHome CONTEXT fields:
ntdll!KiUserApcDispatcher:
  00000000`78ef3910 488b0c24        mov     rcx,qword ptr [rsp]
  00000000`78ef3914 488b542408      mov     rdx,qword ptr [rsp+8]
  00000000`78ef3919 4c8b442410      mov     r8,qword ptr [rsp+10h]
  00000000`78ef391e 4c8bcc          mov     r9,rsp
  00000000`78ef3921 ff542418        call    qword ptr [rsp+18h]
  00000000`78ef3925 488bcc          mov     rcx,rsp
  00000000`78ef3928 b201            mov     dl,1
  00000000`78ef392a e861ddffff      call    ntdll!NtContinue (00000000`78ef1690)
  00000000`78ef392f 85c0            test    eax,eax
  00000000`78ef3931 74dd            je      ntdll!KiUserApcDispatcher (00000000`78ef3910)
  00000000`78ef3933 8bf0            mov     esi,eax
  00000000`78ef3935 8bce            mov     ecx,esi
  00000000`78ef3937 e834f90500      call    ntdll!RtlRaiseException+0x10d (0`78f53270)
  00000000`78ef393c cc              int     3
  00000000`78ef393d 90              nop
  00000000`78ef393e ebf7            jmp     ntdll!KiUserApcDispatch+0x27 (0`78ef3937)
  00000000`78ef3940 cc              int     3

On Win10-1703 there's some logic for delegation before the regular lea sequence:
  ntdll!KiUserApcDispatcher:
  778b3fe0 833dc887957700  cmp     dword ptr [ntdll!LdrDelegatedKiUserApcDispatcher
                                              (779587c8)],0
  778b3fe7 740e            je      ntdll!KiUserApcDispatcher+0x17 (778b3ff7)  Branch
  778b3fe9 8b0dc8879577    mov     ecx,dword ptr [ntdll!LdrDelegatedKiUserApcDispatcher
                                                  (779587c8)]
  778b3fef ff15d0919577    call    dword ptr [ntdll!__guard_check_icall_fptr (779591d0)]
  778b3ff5 ffe1            jmp     ecx
  778b3ff7 8d8424dc020000  lea     eax,[esp+2DCh]
  778b3ffe 648b0d00000000  mov     ecx,dword ptr fs:[0]
  778b4005 bac03f8b77      mov     edx,offset ntdll!KiUserApcExceptionHandler (778b3fc0)
  778b400a 8908            mov     dword ptr [eax],ecx
  778b400c 895004          mov     dword ptr [eax+4],edx
  778b400f 64a300000000    mov     dword ptr fs:[00000000h],eax
  778b4015 58              pop     eax
  778b4016 8d7c240c        lea     edi,[esp+0Ch]
  778b401a 8bc8            mov     ecx,eax
  778b401c ff15d0919577    call    dword ptr [ntdll!__guard_check_icall_fptr (779591d0)]
  778b4022 ffd1            call    ecx
  778b4024 8b8fcc020000    mov     ecx,dword ptr [edi+2CCh]
  778b402a 64890d00000000  mov     dword ptr fs:[0],ecx
  778b4031 6a01            push    1
  778b4033 57              push    edi
  778b4034 e807e1ffff      call    ntdll!NtContinue (778b2140)
  778b4039 8bf0            mov     esi,eax
  778b403b 56              push    esi
  778b403c e89f230100      call    ntdll!RtlRaiseStatus (778c63e0)
  778b4041 ebf8            jmp     ntdll!KiUserApcDispatcher+0x5b (778b403b)  Branch

On Win10-1809 the CONTEXT is shifted by 4:
  ntdll!KiUserApcDispatcher:
  77d627b0 833dfc17e17700  cmp     dword ptr [ntdll!LdrDelegatedKiUserApcDispatcher
                                              (77e117fc)],0
  77d627b7 740e            je      ntdll!KiUserApcDispatcher+0x17 (77d627c7)
  77d627b9 8b0dfc17e177    mov     ecx,dword ptr [ntdll!LdrDelegatedKiUserApcDispatcher
                                                  (77e117fc)]
  77d627bf ff15e041e177    call    dword ptr [ntdll!__guard_check_icall_fptr (77e141e0)]
  77d627c5 ffe1            jmp     ecx
  77d627c7 8d8424e0020000  lea     eax,[esp+2E0h]
  77d627ce 648b0d00000000  mov     ecx,dword ptr fs:[0]
  77d627d5 ba9027d677      mov     edx,offset ntdll!KiUserApcExceptionHandler (77d62790)
  77d627da 8908            mov     dword ptr [eax],ecx
  77d627dc 895004          mov     dword ptr [eax+4],edx
  77d627df 64a300000000    mov     dword ptr fs:[00000000h],eax
  77d627e5 8d7c2414        lea     edi,[esp+14h]
  77d627e9 8b742410        mov     esi,dword ptr [esp+10h]
  77d627ed 83e601          and     esi,1
  77d627f0 58              pop     eax
  77d627f1 8bc8            mov     ecx,eax
  77d627f3 ff15e041e177    call    dword ptr [ntdll!__guard_check_icall_fptr (77e141e0)]
  77d627f9 ffd1            call    ecx
  77d627fb 8b8fcc020000    mov     ecx,dword ptr [edi+2CCh]
  77d62801 64890d00000000  mov     dword ptr fs:[0],ecx
  77d62808 56              push    esi
  77d62809 57              push    edi
  77d6280a e8c1e0ffff      call    ntdll!NtContinue (77d608d0)
  77d6280f 8bf0            mov     esi,eax
  77d62811 56              push    esi
  77d62812 e8f9290100      call    ntdll!RtlRaiseStatus (77d75210)
  77d62817 ebf8            jmp     ntdll!KiUserApcDispatcher+0x61 (77d62811)
  77d62819 c21000          ret     10h

XXX case 6395/case 6050: what are KiUserCallbackExceptionHandler and
KiUserApcExceptionHandler, added on 2003 sp1?  We're assuming not
entered from kernel mode despite Ki prefix.  They are not exported
entry points.
Case 10579: KiUserCallbackExceptionHandler is used to pop the kernel
cb stack when an exception will abandon that cb frame.

 * The target APC function pointer is the would-be return address for
 * this routine.  The first argument is in fact the argument of the
 * APC
 *
 * ASSUMPTIONS:
 *    1) *(esp+0x0) == PKNORMAL_ROUTINE ApcRoutine
 *       (IN PVOID NormalContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
 *      call* native target
 *      The next three arguments on the stack are just passed through
 *      to this function, and are the arguments passed by
 *      NtQueueApcThread(thread, ApcRoutine, ApcContext, Argument1, Argument2)
 *
 *      On XP SP2 user mode APCs target kernel32!BaseDispatchAPC,
 *      and the following arguments have been observed to be:
 *    2') *(esp+0x4) == ApcContext
 *        call* Win32 target PAPCFUNC for user mode APCs
 *    FIXME:  need to check the other platforms - it is completely
 *      up to the caller
 *    3') *(esp+0x8) == Argument1
 *        win32_APC_argument
 *    4') *(esp+0xc) == Argument2
 *      on XP SP2 for BaseDispatchAPC, seems to be SXS activation context related
 *    5) *(esp+apc_context_xsp_offs) == CONTEXT
 * For x64, it looks like the CONTEXT is at the top of the stack, and the
 * PxHome fields hold the APC parameters.
 */
static int apc_context_xsp_offs; /* updated by check_apc_context_offset() */
#define APC_TARGET_XSP_OFFS IF_X64_ELSE(0x18, 0)
/* Remember that every path out of here must invoke the DR exit hook.
 * The normal return path will do so as the interception code has an
 * enter and exit hook around the call to this routine.
 */
static after_intercept_action_t /* note return value will be ignored */
intercept_apc(app_state_at_intercept_t *state)
{
    CONTEXT *cxt;
    /* The CONTEXT is laid out on the stack itself.
     * From examining KiUserApcDispatcher, we know it's 16 bytes up.
     * We try to verify that at interception time via check_apc_context_offset().
     */
    cxt = ((CONTEXT *)(state->mc.xsp + apc_context_xsp_offs));

    /* this might be a new thread */
    possible_new_thread_wait_for_dr_init(cxt);

    /* FIXME: should we only intercept apc for non-initialized thread
     * with start/stop interface?
     * (fine to have start/stop interface also call dynamo_thread_init,
     *  2nd call turns into nop)
     */
    if (intercept_asynch_for_self(true /*we want unknown threads*/)) {
        dcontext_t *dcontext;
        DEBUG_DECLARE(app_pc apc_target;)
        if (get_thread_private_dcontext() != NULL)
            SELF_PROTECT_LOCAL(get_thread_private_dcontext(), WRITABLE);
        /* won't be re-protected until d_r_dispatch->fcache */

        RSTATS_INC(num_APCs);

#ifdef DEBUG
        /* retrieve info on this APC call */
        apc_target = *((app_pc *)(state->mc.xsp + APC_TARGET_XSP_OFFS));
        /* FIXME: invalid app parameters would have been caught already, right? */
        ASSERT(apc_target != 0 && cxt != NULL);
        LOG(GLOBAL, LOG_ASYNCH, 2,
            "ASYNCH intercepted apc: thread=" TIDFMT ", apc pc=" PFX ", cont pc=" PFX
            "\n",
            d_r_get_thread_id(), apc_target, cxt->CXT_XIP);
#endif

        /* this is the same check as in dynamorio_init */
        if (!is_thread_initialized()) {
            ASSERT(get_os_version() < WINDOWS_VERSION_VISTA);
            LOG(GLOBAL, LOG_ASYNCH | LOG_THREADS, 2, "APC thread was not initialized!\n");
            LOG(GLOBAL, LOG_ASYNCH, 1,
                "ASYNCH intercepted thread init apc: apc pc=" PFX ", cont pc=" PFX "\n",
                apc_target, cxt->CXT_XIP);
            if (intercept_new_thread(cxt))
                return AFTER_INTERCEPT_LET_GO;
        } else {
            /* should not receive APC while in DR code! */
            ASSERT(get_thread_private_dcontext()->whereami == DR_WHERE_FCACHE);
            LOG(GLOBAL, LOG_ASYNCH | LOG_THREADS, 2,
                "APC thread was already initialized!\n");
            LOG(THREAD_GET, LOG_ASYNCH, 2,
                "ASYNCH intercepted non-init apc: apc pc=" PFX ", cont pc=" PFX "\n",
                apc_target, cxt->CXT_XIP);

#ifdef PROGRAM_SHEPHERDING
            /* check for hooker's shellcode delivered via APC */
            if (TEST(OPTION_ENABLED, DYNAMO_OPTION(apc_policy))) {
                apc_thread_policy_helper((app_pc *)(state->mc.xsp + APC_TARGET_XSP_OFFS),
                                         DYNAMO_OPTION(apc_policy), APC_TARGET_NATIVE
                                         /* NtQueueApcThread, likely from kernel mode */);
                /* case: 9024 test WINDOWS APC as well
                 * FIXME: we may want to attempt to give an exemption
                 * for user mode APCs as long as we can determine
                 * safely that the routine is
                 * kernel32!BaseDispatchAPC.  Then we'd know which
                 * argument is indeed going to be a target for an
                 * indirect call so we can test whether that is
                 * some shellcode that we need to block or allow.
                 */
            }
#endif /* PROGRAM_SHEPHERDING */
        }

        /* Strategy: we want to use the same dcontext for the APC.
         * Since we're not stealing a register or anything, and we're squashing
         * traces, we can rely on the CONTEXT to store the only state we need.
         * We simply change the CONTEXT right now to point to the next app
         * pc to execute, and we're all set.
         */
        dcontext = get_thread_private_dcontext();
        if ((cache_pc)cxt->CXT_XIP == after_do_syscall_addr(dcontext) ||
            (cache_pc)cxt->CXT_XIP == after_shared_syscall_addr(dcontext)) {
            /* to avoid needing to save this dcontext, just have cxt point to
             * app pc for after syscall, stored in asynch_target/esi slot
             * since next_tag holds do/share_syscall address
             */
            LOG(THREAD, LOG_ASYNCH, 2,
                "\tchanging cont pc " PFX " from after do/share syscall to " PFX
                " or " PFX "\n",
                cxt->CXT_XIP, dcontext->asynch_target, get_mcontext(dcontext)->xsi);
            ASSERT(does_syscall_ret_to_callsite());
            if (DYNAMO_OPTION(sygate_int) && get_syscall_method() == SYSCALL_METHOD_INT) {
                /* This should be an int system call and since for sygate
                 * compatility we redirect those with a call to an ntdll.dll
                 * int 2e ret 0 we need to pop the stack once to match app. */
                ASSERT(*(app_pc *)cxt->CXT_XSP == after_do_syscall_code(dcontext) ||
                       *(app_pc *)cxt->CXT_XSP == after_shared_syscall_code(dcontext));
                cxt->CXT_XSP += XSP_SZ; /* pop the stack */
            }
            if (dcontext->asynch_target != 0)
                cxt->CXT_XIP = (ptr_uint_t)dcontext->asynch_target;
            else
                cxt->CXT_XIP = get_mcontext(dcontext)->xsi;
        } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
                   cxt->CXT_XIP == (ptr_uint_t)vsyscall_after_syscall) {
            /* Windows XP/2003: kernel ignores the return address
             * of the caller of the sysenter wrapper code and instead sends control
             * straight to the ret at 0x7ffe0304.  Since a trampoline there is not
             * very transparent, we instead clobber the retaddr to point at the
             * caller of the wrapper.
             * For an APC interrupting a syscall (i.e., a non-init apc), we
             * need to change the retaddr back to its native value for transparency,
             * and also since the storage for it (esi or asynch_target)
             * might get clobbered before the NtContinue restores us back to
             * the syscall.
             * We'll re-fix-up the retaddr to retain control at NtContinue.
             */
            ASSERT(get_os_version() >= WINDOWS_VERSION_XP);
            /* change after syscall ret addr to be app after syscall addr,
             * since asynch/esi slot is going to get clobbered
             */
            /* for the case 5441 Sygate hack, esp will point to
             * sysenter_ret_address while esp+4/8 will point to after_*_syscall
             * we'll need to restore both stack values */
            if (*((cache_pc *)(cxt->CXT_XSP +
                               (DYNAMO_OPTION(sygate_sysenter) ? XSP_SZ : 0))) ==
                after_do_syscall_code(dcontext)) {
                LOG(THREAD, LOG_ASYNCH, 2,
                    "\tcont pc is vsyscall ret, changing ret addr @" PFX " "
                    "from " PFX " to " PFX "\n",
                    cxt->CXT_XSP, *((app_pc *)cxt->CXT_XSP), dcontext->asynch_target);
                if (DYNAMO_OPTION(sygate_sysenter)) {
                    ASSERT(*((app_pc *)cxt->CXT_XSP) == sysenter_ret_address);
                    *((app_pc *)(cxt->CXT_XSP + XSP_SZ)) = dcontext->sysenter_storage;
                }
                *((app_pc *)cxt->CXT_XSP) = dcontext->asynch_target;
            } else if (*((cache_pc *)(cxt->CXT_XSP +
                                      (DYNAMO_OPTION(sygate_sysenter) ? XSP_SZ : 0))) ==
                       after_shared_syscall_code(dcontext)) {
                ASSERT(DYNAMO_OPTION(shared_syscalls));
                /* change after syscall ret addr to be app after syscall addr,
                 * since esi slot is going to get clobbered
                 */
                LOG(THREAD, LOG_ASYNCH, 2,
                    "\tcont pc is vsyscall ret, changing ret addr @" PFX " "
                    "from " PFX " to " PFX "\n",
                    cxt->CXT_XSP, *((app_pc *)cxt->CXT_XSP), get_mcontext(dcontext)->xsi);
                if (DYNAMO_OPTION(sygate_sysenter)) {
                    ASSERT(*((app_pc *)cxt->CXT_XSP) == sysenter_ret_address);
                    *((app_pc *)(cxt->CXT_XSP + XSP_SZ)) = dcontext->sysenter_storage;
                }
                /* change after syscall ret addr to be app after syscall addr,
                 * since esi slot is going to get clobbered
                 */
                *((app_pc *)cxt->CXT_XSP) = (app_pc)get_mcontext(dcontext)->xsi;
            } else {
                /* should only get here w/ non-DR-mangled syscall if was native! */
                ASSERT(IS_UNDER_DYN_HACK(dcontext->thread_record->under_dynamo_control));
            }
        } else if (cxt->CXT_XIP == (ptr_uint_t)nt_continue_dynamo_start) {
            /* NtContinue entered kernel and was interrupted for another APC
             * we have to restore as though NtContinue never happened, this APC will
             * execute its own NtContinue (remember, we're stateless)
             */
            /* asynch_target is zeroed out when handle_sysem_call is done, so
             * a zero value indicates that the syscall was handled in-cache.
             */
            if (dcontext->asynch_target != NULL)
                cxt->CXT_XIP = (ptr_uint_t)dcontext->asynch_target;
            else {
                ASSERT(DYNAMO_OPTION(shared_syscalls));
                cxt->CXT_XIP = (ptr_uint_t)dcontext->next_tag;
            }
            LOG(THREAD, LOG_ASYNCH, 2,
                "\tnew APC interrupted nt_continue_dynamo_start, restoring " PFX
                " as cxt->Xip\n",
                cxt->CXT_XIP);
        } else {
            /* possibilities: for thread init APC I usually see what I think is the
             * thread entry point in kernel32 (very close to image entry point), but
             * I also sometimes see a routine in ntdll @0x77f9e9b9:
             * <ntdll.dll~RtlConvertUiListToApiList+0x2fc,
             *  ~RtlCreateQueryDebugBuffer-0x315>
             */
            LOG(THREAD, LOG_ASYNCH, 2,
                "\tAPC return point " PFX " needs no translation\n", cxt->CXT_XIP);
            /* our internal nudge creates a thread that directly targets
             * generic_nudge_target() */
            ASSERT(!is_dynamo_address((app_pc)cxt->CXT_XIP) ||
                   cxt->CXT_XIP == (ptr_uint_t)generic_nudge_target ||
                   is_new_thread_client_thread(cxt, NULL));
        }

        asynch_retakeover_if_native();
        state->callee_arg = (void *)false /* use cur dcontext */;
        instrument_dispatcher(dcontext, DR_XFER_APC_DISPATCHER, state, cxt);
        asynch_take_over(state);
    } else
        STATS_INC(num_APCs_noasynch);
    return AFTER_INTERCEPT_LET_GO;
}

/* Identifies the offset of the CONTEXT structure on entry to KiUserApcDispatcher
 * and stores it into apc_context_xsp_offs.
 */
static void
check_apc_context_offset(byte *apc_entry)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    instr_t instr;
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;
    instr_init(dcontext, &instr);

    LOG(GLOBAL, LOG_ASYNCH, 3, "check_apc_context_offset\n");
    DOLOG(3, LOG_ASYNCH, { disassemble_with_bytes(dcontext, apc_entry, GLOBAL); });

    decode(dcontext, apc_entry, &instr);

#ifdef X64
    ASSERT(instr_get_opcode(&instr) == OP_mov_ld &&
           opnd_is_reg(instr_get_dst(&instr, 0)) &&
           opnd_get_reg(instr_get_dst(&instr, 0)) == REG_RCX &&
           opnd_is_base_disp(instr_get_src(&instr, 0)) &&
           ((get_os_version() < WINDOWS_VERSION_7 &&
             opnd_get_disp(instr_get_src(&instr, 0)) == 0) ||
            /* on win7x64 the call* tgt is loaded in 1st instr */
            (get_os_version() >= WINDOWS_VERSION_7 &&
             opnd_get_disp(instr_get_src(&instr, 0)) == 0x18)) &&
           opnd_get_base(instr_get_src(&instr, 0)) == REG_XSP &&
           opnd_get_index(instr_get_src(&instr, 0)) == REG_NULL);
    apc_context_xsp_offs = 0;
#else
    int lea_offs = 0x10 /*most common value*/, pushpop_offs = 0;
    app_pc pc = apc_entry;
    /* Skip over the Win10-1703 delegation prefx */
    if (instr_get_opcode(&instr) == OP_cmp &&
        get_os_version() >= WINDOWS_VERSION_10_1703) {
        pc += instr_length(dcontext, &instr);
        do {
            instr_reset(dcontext, &instr);
            pc = decode(dcontext, pc, &instr);
        } while (instr_get_opcode(&instr) != OP_lea && pc - apc_entry < 32);
    }
    /* Look for a small-offs lea, accounting for push/pop in between. */
    while (pc - apc_entry < 96) {
        if (instr_get_opcode(&instr) == OP_lea &&
            opnd_is_base_disp(instr_get_src(&instr, 0)) &&
            opnd_get_base(instr_get_src(&instr, 0)) == DR_REG_XSP &&
            opnd_get_index(instr_get_src(&instr, 0)) == DR_REG_NULL) {
            lea_offs = opnd_get_disp(instr_get_src(&instr, 0));
            /* Skip the large-offs lea 0x2dc */
            if (lea_offs < 0x100)
                break;
        }
        if (instr_get_opcode(&instr) == OP_pop)
            pushpop_offs += XSP_SZ;
        else if (instr_get_opcode(&instr) == OP_push)
            pushpop_offs -= XSP_SZ;
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
    }
    ASSERT(instr_get_opcode(&instr) == OP_lea);
    apc_context_xsp_offs = lea_offs + pushpop_offs;
    LOG(GLOBAL, LOG_ASYNCH, 1, "apc_context_xsp_offs = %d\n", apc_context_xsp_offs);
#endif
    instr_free(dcontext, &instr);
}

/****************************************************************************
 * NtContinue
 *
 * NtContinue is used both by exceptions and APCs (both thread-creation APCs
 * and non-creation APCs, the latter distinguished by always interrupting
 * the app inside a system call, just like a callback).
 * We can avoid needing to save the prev dcontext and restore it here b/c
 * user mode passes a CONTEXT so we know where we're going, and we can simply
 * start interpreting anew at that point.  We aren't stealing a register and
 * we're squashing traces, so we have no baggage that needs to be restored
 * on the other end.  We just have to be careful about shared syscall return
 * addresses and the exception fragment ==> FIXME!  what if get APC while handling
 * exception, then another exception?  will our exception fragment get messed up?
 *
 * We used to need to restore a register, so we worried about non-init APCs and
 * exceptions that re-executed the faulting instruction.  We couldn't tell them apart
 * from init APCs and other exceptions, so we saved the dcontext every time and restored
 * it here.  No more.
 *
NtContinue:
  77F820F3: B8 1C 00 00 00     mov         eax,1Ch
  77F820F8: 8D 54 24 04        lea         edx,[esp+4]
  77F820FC: CD 2E              int         2Eh
 *
 * NtContinue takes CONTEXT *cxt and flag (0=exception, 1=APC?!?)
 * This routine is called by pre_system_call, NOT intercepted from
 * ntdll kernel entry point, as it's user-driven.
 */
void
intercept_nt_continue(CONTEXT *cxt, int flag)
{
    if (intercept_asynch_for_self(false /*no unknown threads*/)) {
        dcontext_t *dcontext = get_thread_private_dcontext();

        LOG(THREAD, LOG_ASYNCH, 2,
            "ASYNCH intercept_nt_continue in thread " TIDFMT ", xip=" PFX "\n",
            d_r_get_thread_id(), cxt->CXT_XIP);

        LOG(THREAD, LOG_ASYNCH, 3, "target context:\n");
        DOLOG(3, LOG_ASYNCH, { dump_context_info(cxt, THREAD, true); });

        get_mcontext(dcontext)->pc = dcontext->next_tag;
        instrument_kernel_xfer(dcontext, DR_XFER_CONTINUE, NULL, NULL,
                               get_mcontext(dcontext), (app_pc)cxt->CXT_XIP, cxt->CXT_XSP,
                               cxt, NULL, 0);

        /* Updates debug register values.
         * FIXME should check dr7 upper bits, and maybe dr6
         * We ignore the potential race condition.
         */
        if (TESTALL(CONTEXT_DEBUG_REGISTERS, cxt->ContextFlags)) {
            if (TESTANY(cxt->Dr7, DEBUG_REGISTERS_FLAG_ENABLE_DR0)) {
                /* Flush only when debug register value changes. */
                if (d_r_debug_register[0] != (app_pc)cxt->Dr0) {
                    d_r_debug_register[0] = (app_pc)cxt->Dr0;
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[0], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                }
            } else {
                /* Disable debug register. */
                if (d_r_debug_register[0] != NULL) {
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[0], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                    d_r_debug_register[0] = NULL;
                }
            }
            if (TESTANY(cxt->Dr7, DEBUG_REGISTERS_FLAG_ENABLE_DR1)) {
                if (d_r_debug_register[1] != (app_pc)cxt->Dr1) {
                    d_r_debug_register[1] = (app_pc)cxt->Dr1;
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[1], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                }
            } else {
                /* Disable debug register. */
                if (d_r_debug_register[1] != NULL) {
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[1], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                    d_r_debug_register[1] = NULL;
                }
            }
            if (TESTANY(cxt->Dr7, DEBUG_REGISTERS_FLAG_ENABLE_DR2)) {
                if (d_r_debug_register[2] != (app_pc)cxt->Dr2) {
                    d_r_debug_register[2] = (app_pc)cxt->Dr2;
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[2], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                }
            } else {
                /* Disable debug register. */
                if (d_r_debug_register[2] != NULL) {
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[2], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                    d_r_debug_register[2] = NULL;
                }
            }
            if (TESTANY(cxt->Dr7, DEBUG_REGISTERS_FLAG_ENABLE_DR3)) {
                if (d_r_debug_register[3] != (app_pc)cxt->Dr3) {
                    d_r_debug_register[3] = (app_pc)cxt->Dr3;
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[3], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                }
            } else {
                /* Disable debug register. */
                if (d_r_debug_register[3] != NULL) {
                    flush_fragments_from_region(
                        dcontext, d_r_debug_register[3], 1 /* size */,
                        false /*don't force synchall*/,
                        NULL /*flush_completion_callback*/, NULL /*user_data*/);
                    d_r_debug_register[3] = NULL;
                }
            }
        }

        if (is_building_trace(dcontext)) {
            LOG(THREAD, LOG_ASYNCH, 2, "intercept_nt_continue: squashing old trace\n");
            trace_abort(dcontext);
        }

        if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
            cxt->CXT_XIP == (ptr_uint_t)vsyscall_after_syscall) {
            /* We need to go back to after shared/do syscall, to do post syscall and other
             * activities, so we restore the special esi pointer from the ret addr
             */
            /* This works w/optimize syscalls w/no changes. Since
             * intercept_nt_continue is called from pre_system_call only, the
             * #ifdefs ensure that the correct location is used for the
             * after-syscall address.
             */
            /* Note that our stateless handling re-uses the same dcontext, and
             * that we assume we can match the dstack for the NtContinue
             * fcache entrance with the fcache return from the return-to syscall
             */
            /* NOTE - the stack mangling must match that of handle_system_call()
             * and shared_syscall as not all routines looking at the stack
             * differentiate. */
            ASSERT(get_os_version() >= WINDOWS_VERSION_XP);
            LOG(THREAD, LOG_ASYNCH, 2,
                "\txip=vsyscall " PFX ", changing ret addr @" PFX " from " PFX " to " PFX
                "\n",
                cxt->CXT_XIP, cxt->CXT_XSP,
                *((app_pc *)(cxt->CXT_XSP +
                             (DYNAMO_OPTION(sygate_sysenter) ? XSP_SZ : 0))),
                after_do_syscall_code(dcontext));
            dcontext->asynch_target = *((app_pc *)cxt->CXT_XSP);
            if (DYNAMO_OPTION(sygate_sysenter)) {
                /* case 5441 Sygate hack, tos in esi/asynch, next stack slot saved
                 * in sysenter_storage. Stack then looks like
                 * esp +0   sysenter_ret_address (ret in ntdll.dll)
                 *     +4/8 after_do/shared_syscall
                 */
                /* get the app ret addr into the proper asynch target slot */
                dcontext->sysenter_storage = *((app_pc *)(cxt->CXT_XSP + XSP_SZ));
                /* now replace the ret addr w/ do syscall */
                *((app_pc *)cxt->CXT_XSP) = sysenter_ret_address;
                *((app_pc *)(cxt->CXT_XSP + XSP_SZ)) =
                    (app_pc)after_do_syscall_code(dcontext);
            } else {
                /* now replace the ret addr w/ do syscall */
                *((app_pc *)cxt->CXT_XSP) = (app_pc)after_do_syscall_code(dcontext);
            }
        } else if (!in_fcache((cache_pc)cxt->CXT_XIP) &&
                   /* FIXME : currently internal nudges (detach on violation
                    * for ex.) create a thread that directly targets the
                    * generic_nudge_target() function. Therefore, we have to check for
                    * it here. */
                   (!is_dynamo_address((cache_pc)cxt->CXT_XIP) ||
                    cxt->CXT_XIP == (ptr_uint_t)generic_nudge_target) &&
                   !in_generated_routine(dcontext, (cache_pc)cxt->CXT_XIP)) {
            /* Going to non-code-cache address, need to make sure get control back
             * Use next_tag slot to hold original Xip
             */
            LOG(THREAD, LOG_ASYNCH, 2,
                "\txip=" PFX " not in fcache, intercepting at " PFX "\n", cxt->CXT_XIP,
                nt_continue_dynamo_start);
            /* we have to use a different slot since next_tag ends up holding
             * the do_syscall entry when entered from d_r_dispatch (we're called
             * from pre_syscall, prior to entering cache)
             */
            dcontext->asynch_target = (app_pc)cxt->CXT_XIP;
            /* Point Xip to allow dynamo to retain control
             * FIXME: w/ stateless handling here, can point at fcache_return
             * like signals do for better performance?
             */
            cxt->CXT_XIP = (ptr_uint_t)nt_continue_dynamo_start;
        } else if (cxt->CXT_XIP == (ptr_uint_t)thread_attach_takeover) {
            /* We set the context of this thread before it was done with its init
             * APC: so we need to undo our takeover changes and take over
             * normally here.
             */
            thread_attach_context_revert(cxt);
            dcontext->asynch_target = (app_pc)cxt->CXT_XIP;
            cxt->CXT_XIP = (ptr_uint_t)nt_continue_dynamo_start;
        } else {
            /* No explanation for this one! */
            SYSLOG_INTERNAL_ERROR(
                "ERROR: intercept_nt_continue: xip=" PFX " not an app pc!", cxt->CXT_XIP);
            ASSERT_NOT_REACHED();
        }
    }
}

/* This routine is called by pre_system_call
 * Assumes caller holds thread_initexit_lock
 * dcontext is the context of the target thread, not this thread
 */
void
intercept_nt_setcontext(dcontext_t *dcontext, CONTEXT *cxt)
{
    /* b/c it needs the registers passed in, pre_system_call does the
     * synch, and b/c post_system_call needs to test the same
     * condition, pre also does the interception check.
     */
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    ASSERT(intercept_asynch_for_thread(dcontext->owning_thread,
                                       false /*no unknown threads*/));
    ASSERT(dcontext != NULL && dcontext->initialized);
    LOG(THREAD, LOG_ASYNCH, 1,
        "ASYNCH intercept_nt_setcontext: thread " TIDFMT " targeting thread " TIDFMT "\n",
        d_r_get_thread_id(), dcontext->owning_thread);
    LOG(THREAD, LOG_ASYNCH, 3, "target context:\n");
    DOLOG(3, LOG_ASYNCH, { dump_context_info(cxt, THREAD, true); });

    if (is_building_trace(dcontext)) {
        LOG(THREAD, LOG_ASYNCH, 2, "intercept_nt_setcontext: squashing old trace\n");
        trace_abort(dcontext);
    }

    get_mcontext(dcontext)->pc = dcontext->next_tag;
    instrument_kernel_xfer(dcontext, DR_XFER_SET_CONTEXT_THREAD, NULL, NULL,
                           get_mcontext(dcontext), (app_pc)cxt->CXT_XIP, cxt->CXT_XSP,
                           cxt, NULL, 0);

    /* Yes, we use the same x86.asm and x86_code.c procedures as
     * NtContinue: nt_continue_dynamo_start and nt_continue_start_setup
     */
    if (!in_fcache((cache_pc)cxt->CXT_XIP) &&
        !in_generated_routine(dcontext, (cache_pc)cxt->CXT_XIP)) {
        /* Going to non-code-cache address, need to make sure get control back
         * Use next_tag slot to hold original Xip
         */
        LOG(THREAD, LOG_ASYNCH, 1,
            "intercept_nt_setcontext: xip=" PFX " not in fcache, intercepting\n",
            cxt->CXT_XIP);
        /* This works w/optimize syscalls w/no changes. Since
         * intercept_nt_setcontext is called from pre_system_call only, the
         * #ifdefs ensure that the correct location is used for the xip.
         */
        /* we have to use a different slot since next_tag ends up holding the do_syscall
         * entry when entered from d_r_dispatch (we're called from pre_syscall,
         * prior to entering cache)
         */
        dcontext->asynch_target = (app_pc)cxt->CXT_XIP;
        /* Point Xip to allow dynamo to retain control
         * FIXME: w/ stateless handling here, can point at fcache_return
         * like signals do for better performance?
         */
        cxt->CXT_XIP = (ptr_uint_t)get_setcontext_interceptor();
    } else {
        LOG(THREAD, LOG_ASYNCH, 1,
            "ERROR: intercept_nt_setcontext: xip=" PFX " in fcache!\n", cxt->CXT_XIP);
        /* This should not happen!  Does this indicate
         * malicious/erroneous application code?
         */
        SYSLOG_INTERNAL_ERROR("intercept_nt_setcontext: targeting fcache!");
        ASSERT_NOT_REACHED();
    }
}

/****************************************************************************
 * EXCEPTIONS
 *
 */

#ifdef INTERCEPT_TOP_LEVEL_EXCEPTIONS
/* top-level exception handler
 * currently we don't need this, so it's not operational
 * to make operational, add this to callback_init:
 *   app_top_handler =
 *     SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) our_top_handler);
 * also need to intercept the app calling SetUnhandledExceptionFilter, so need
 * to investigate whether it turns into syscall RtlUnhandledExceptionFilter
 * or what -- FIXME
 */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    SYSLOG_INTERNAL_INFO("in top level exception handler!");
    if (app_top_handler != NULL)
        return (*app_top_handler)(pExceptionInfo);
    else
        return EXCEPTION_CONTINUE_SEARCH; /* let default action happen */
}
#endif

static void
transfer_to_fcache_return(dcontext_t *dcontext, CONTEXT *cxt, app_pc next_pc,
                          linkstub_t *last_exit)
{
    /* Do not resume execution in cache, go back to d_r_dispatch.
     * Do a direct nt_continue to fcache_return!
     * Note that even if we were in the shared cache, we
     * still go to the private fcache_return for simplicity.
     */
    cxt->CXT_XIP = (ptr_uint_t)fcache_return_routine(dcontext);
#ifdef X64
    /* x64 always uses shared gencode */
    get_local_state_extended()->spill_space.xax = cxt->CXT_XAX;
#else
    get_mcontext(dcontext)->xax = cxt->CXT_XAX;
#endif
    cxt->CXT_XAX = (ptr_uint_t)last_exit;
    /* fcache_return will save rest of state */
    dcontext->next_tag = next_pc;
    LOG(THREAD, LOG_ASYNCH, 2, "\tset next_tag to " PFX ", resuming in fcache_return\n",
        next_pc);
    EXITING_DR();
    nt_continue(cxt);
}

/* Due to lack of parameter space when calling found_modified_code()
 * we use flags.  We also use them for check_for_modified_code() for
 * consistency.
 */
enum {
    MOD_CODE_TAKEOVER = 0x01,
    MOD_CODE_EMULATE_WRITE = 0x02,
    MOD_CODE_APP_CXT = 0x04,
};

/* To allow execution from a writable memory region, we mark it read-only.
 * When we get a write seg fault from that region, we call this routine.
 * It removes the region from the executable list, flushes fragments
 * originating there, marks it writable again, and then calls NtContinue
 * to resume execution of the faulting write.
 * This function does not return!
 */
/* exported since we can't do inline asm anymore and must call from x86.asm */
void
found_modified_code(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                    app_pc target, uint flags, fragment_t *f)
{
    app_pc next_pc = NULL;
    cache_pc instr_cache_pc = (app_pc)pExcptRec->ExceptionAddress;
    app_pc translated_pc;
    if (!TEST(flags, MOD_CODE_TAKEOVER) || TEST(flags, MOD_CODE_APP_CXT)) {
        LOG(THREAD, LOG_ASYNCH, 2, "found_modified_code: native/app " PFX "\n",
            instr_cache_pc);
        /* for !takeover: assumption: native pc -- FIXME: vs
         * thread-noasynch general usage?
         */
        ASSERT(!in_generated_routine(dcontext, instr_cache_pc) &&
               !in_fcache(instr_cache_pc));
        translated_pc = instr_cache_pc;
        instr_cache_pc = NULL;
    } else {
        LOG(THREAD, LOG_ASYNCH, 2, "found_modified_code: translating " PFX "\n",
            instr_cache_pc);
        /* For safe recreation we need to either be couldbelinking or hold the
         * initexit lock (to keep someone from flushing current fragment), the
         * initexit lock is easier
         */
        d_r_mutex_lock(&thread_initexit_lock);
        /* FIXME: currently this will fail for a pc inside a pending-deletion
         * fragment!  == case 3567
         */
        /* We use the passed-in fragment_t pointer.  Perhaps it could have
         * been flushed, but it can't have been freed b/c this thread
         * didn't reach a safe point.
         */
        translated_pc = recreate_app_pc(dcontext, instr_cache_pc, f);
        {
            /* we must translate the full state in case a client changed
             * register values, since we're going back to d_r_dispatch
             */
            recreate_success_t res;
            priv_mcontext_t mcontext;
            context_to_mcontext(&mcontext, cxt);
            res = recreate_app_state(dcontext, &mcontext, true /*memory too*/, f);
            if (res == RECREATE_SUCCESS_STATE) {
                /* cxt came from the kernel, so it should already have ss and cs
                 * initialized. Thus there's no need to get them again.
                 */
                mcontext_to_context(cxt, &mcontext, false /* !set_cur_seg */);
            } else {
                /* Should not happen since this should not be an instr we added! */
                SYSLOG_INTERNAL_WARNING("Unable to fully translate cxt for codemod "
                                        "fault");
                /* we should always at least get pc right */
                ASSERT(res == RECREATE_SUCCESS_PC);
            }
        }
        d_r_mutex_unlock(&thread_initexit_lock);
        LOG(THREAD, LOG_ASYNCH, 2, "\tinto " PFX "\n", translated_pc);
    }
    ASSERT(translated_pc != NULL);
    if (USING_PRETEND_WRITABLE() && is_pretend_writable_address(target)) {
        /* now figure out why is this pretend_writable, here only for debugging */

        /* case 6632: we may want to report even in release build if
         * we've prevented a function patch.  or at least should add a
         * release build statistic to show that.
         */
        DEBUG_DECLARE(bool system_overlap =
                          tamper_resistant_region_overlap(target, target + 1);)
        DEBUG_DECLARE(bool patch_module_overlap =
                          vmvector_overlap(patch_proof_areas, target, target + 1);)

        DEBUG_DECLARE(uint write_size = 0;)
        DODEBUG({
            decode_memory_reference_size(dcontext, (app_pc)pExcptRec->ExceptionAddress,
                                         &write_size);
        });
        SYSLOG_INTERNAL_WARNING_ONCE("app tried to write to pretend-writable "
                                     "code " PFX " %d bytes",
                                     target, write_size);
        LOG(THREAD, LOG_ASYNCH, 2,
            "app tried to write to pretend-writable %s code " PFX " %d bytes\n",
            system_overlap ? "system" : (patch_module_overlap ? "patch module" : "DR"),
            target, write_size);

        DOSTATS({
            if (system_overlap)
                STATS_INC(app_modify_ntdll_writes);
            else if (patch_module_overlap)
                STATS_INC(app_modify_patch_module_writes);
            else
                STATS_INC(app_modify_DR_writes);
        });
        /* if there are more than a handful of writes we're dealing not with a
         * hooking dll but w/ something else.
         * we see 48 ntdll hook writes in case 9149.
         */
        ASSERT_CURIOSITY_ONCE(GLOBAL_STAT(app_modify_DR_writes) < 10);
        ASSERT_CURIOSITY_ONCE(GLOBAL_STAT(app_modify_ntdll_writes) < 50);
        ASSERT_CURIOSITY_ONCE(GLOBAL_STAT(app_modify_patch_module_writes) < 50);

        /* skip the write */
        next_pc = decode_next_pc(dcontext, translated_pc);
        LOG(THREAD, LOG_ASYNCH, 2, "skipping to after write pc " PFX "\n", next_pc);
    } else if (TEST(flags, MOD_CODE_EMULATE_WRITE)) {
        app_pc prot_start = (app_pc)PAGE_START(target);
        uint write_size;
        size_t prot_size;
        priv_mcontext_t mcontext;
        bool ok;
        DEBUG_DECLARE(app_pc result =)
        decode_memory_reference_size(dcontext, translated_pc, &write_size);
        ASSERT(result != NULL);
        /* In current usage, we only use emulation for cases where a sub-page
         * region NOT containing code is being written to -- otherwise there's
         * no real advantage over normal page protection consistency or sandboxing,
         * especially with multiple writes in a row w/ no executions in between.
         * If we do decide to use it for executable regions, must call flush here.
         */
        ASSERT(
            !executable_vm_area_overlap(target, target + write_size, false /*no lock*/));
        SYSLOG_INTERNAL_WARNING_ONCE("app tried to write emulate-write region @" PFX,
                                     target);
        LOG(THREAD, LOG_ASYNCH, 2, "emulating writer @" PFX " writing " PFX "-" PFX "\n",
            translated_pc, target, target + write_size);
        prot_size = (app_pc)PAGE_START(target + write_size) + PAGE_SIZE - prot_start;
        context_to_mcontext(&mcontext, cxt);
        /* can't have two threads in here at once mixing up writability w/ the write */
        d_r_mutex_lock(&emulate_write_lock);
        /* FIXME: this opens up a window where an executable region on the same
         * page will not have code modifications detected
         */
        ok = make_writable(prot_start, prot_size);
        ASSERT_CURIOSITY(ok);
        if (ok) {
            next_pc = d_r_emulate(dcontext, translated_pc, &mcontext);
        } else {
            /* FIXME: case 10550 note that it is possible that this
             * would have failed natively, so we should have executed
             * the call to make a page writable when the app requested
             * it.  Then we wouldn't have to worry about this write
             * failing.  There is a small chance the app wouldn't have
             * even tried to write.
             *
             * It is too late for us to return the proper error from
             * the system call, so we could either skip this
             * instruction, or crash the app.  We do the latter: we'll
             * reexecute app write on a still read only page!
             */
            next_pc = NULL;
        }
        if (next_pc == NULL) {
            /* using some instr our emulate can't handle yet
             * abort and remove page-expanded region from exec list
             */
            d_r_mutex_unlock(&emulate_write_lock);
            LOG(THREAD, LOG_ASYNCH, 1, "emulation of instr @" PFX " failed, bailing\n",
                translated_pc);
            flush_fragments_and_remove_region(dcontext, prot_start, prot_size,
                                              false /* don't own initexit_lock */,
                                              false /* keep futures */);
            /* could re-execute the write in-cache, but could be inside region
             * being flushed, so safest to exit */
            next_pc = translated_pc;
            STATS_INC(num_emulated_write_failures);
        } else {
            LOG(THREAD, LOG_ASYNCH, 1,
                "successfully emulated writer @" PFX " writing " PFX " to " PFX "\n",
                translated_pc, *((int *)target), target);
            make_unwritable(prot_start, prot_size);
            d_r_mutex_unlock(&emulate_write_lock);
            /* will go back to d_r_dispatch for next_pc below */
            STATS_INC(num_emulated_writes);
        }
        ASSERT(next_pc != NULL);
        if (DYNAMO_OPTION(IAT_convert)) {
            /* FIXME: case 85: very crude solution just flush ALL
             * fragments if an IAT hooker shows up to make sure we're
             * executing consistently
             */
            /* we depend on emulate_IAT_writes to get these faults,
             * loader patching up IAT will not trigger these since
             * it is exempted as would also like
             */
            if (vmvector_overlap(IAT_areas, target, target + 1)) {
                LOG(THREAD, LOG_ASYNCH, 1,
                    "IAT hooker at @" PFX " invalidating all caches\n", translated_pc);
                if (!INTERNAL_OPTION(unsafe_IAT_ignore_hooker)) {
                    SYSLOG_INTERNAL_WARNING_ONCE("IAT hooker resulted in whole "
                                                 "cache flush");
                    invalidate_code_cache();
                } else {
                    SYSLOG_INTERNAL_WARNING_ONCE("IAT hooker - ignoring write");
                }
                STATS_INC(num_invalidate_IAT_hooker);
            } else {
                ASSERT_NOT_TESTED();
            }
        }
    } else {
        next_pc =
            handle_modified_code(dcontext, instr_cache_pc, translated_pc, target, f);
    }
    /* if !takeover, re-execute the write no matter what -- the assumption
     * is that the write is native */
    if (!TEST(flags, MOD_CODE_TAKEOVER) || next_pc == NULL) {
        /* now re-execute the write
         * don't try to go through entire exception route by setting up
         * our own exception handler directly in TIB -- not transparent,
         * requires user stack!  just call NtContinue here
         */
        if (next_pc != NULL) {
            cxt->CXT_XIP = (ptr_uint_t)next_pc;
            LOG(THREAD, LOG_ASYNCH, 2, "\tresuming after write instr @ " PFX "\n",
                cxt->CXT_XIP);
        } else
            LOG(THREAD, LOG_ASYNCH, 2, "\tresuming write instr @ " PFX "\n",
                cxt->CXT_XIP);
        EXITING_DR();
        nt_continue(cxt);
    } else {
        /* Cannot resume execution in cache (was flushed), go back to d_r_dispatch
         * via fcache_return
         */
        if (is_building_trace(dcontext)) {
            LOG(THREAD, LOG_ASYNCH, 3, "\tsquashing trace-in-progress\n");
            trace_abort(dcontext);
        }
        transfer_to_fcache_return(dcontext, cxt, next_pc,
                                  (linkstub_t *)get_selfmod_linkstub());
    }
    ASSERT_NOT_REACHED(); /* should never get here */
}

static bool
is_dstack_overflow(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt)
{
    if (pExcptRec->ExceptionCode == EXCEPTION_GUARD_PAGE ||
        pExcptRec->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        /* Both of these seem to put the target in info slot 1. */
        if (pExcptRec->NumberParameters >= 2) {
            app_pc target = (app_pc)pExcptRec->ExceptionInformation[1];
            LOG(THREAD, LOG_ASYNCH, 2, "is_dstack_overflow: target is " PFX "\n", target);
            return is_stack_overflow(dcontext, target);
        }
    }
    return false;
}

/* To allow execution from a writable memory region, we mark it read-only.
 * When we get a seg fault, we call this routine, which determines if it's
 * a write to a region we've marked read-only.  If so, it does not return.
 * In that case, if takeover, we re-execute the faulting instr under our
 * control, while if !takeover, we re-execute the faulting instr natively.
 * If app_cxt, the exception record and context contain app state, not
 * code cache state (happens in some circumstances such as case 7393).
 */
static void
check_for_modified_code(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                        uint flags, fragment_t *f)
{
    /* special case: we expect a seg fault for executable regions
     * that were writable and marked read-only by us
     */
    if (pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        pExcptRec->ExceptionInformation[0] == 1 /* write */) {
        app_pc target = (app_pc)pExcptRec->ExceptionInformation[1];
        bool emulate_write = false;
        uint mod_flags;
        LOG(THREAD, LOG_ASYNCH, 2,
            "check_for_modified_code: exception was write to " PFX "\n", target);
        if (!vmvector_empty(emulate_write_areas)) {
            uint write_size = 0;
            /* FIXME: we duplicate this write size lookup w/ found_modified_code */
            DEBUG_DECLARE(app_pc result =)
            decode_memory_reference_size(dcontext, (app_pc)pExcptRec->ExceptionAddress,
                                         &write_size);
            ASSERT(result != NULL);
            /* only emulate if completely inside -- no quick check for that, good
             * enough to say if overlap w/ emulate areas and no overlap w/ exec areas.
             * o/w we have to flush the written part outside of emulate areas.
             */
            /* FIXME: case 7492: reported target may be in the middle of the write! */
            emulate_write =
                vmvector_overlap(emulate_write_areas, target, target + write_size) &&
                !executable_vm_area_overlap(target, target + write_size,
                                            false /*no lock*/);
        }
        if (was_executable_area_writable(target) || emulate_write ||
            ((DYNAMO_OPTION(handle_DR_modify) == DR_MODIFY_NOP ||
              DYNAMO_OPTION(handle_ntdll_modify) == DR_MODIFY_NOP) &&
             /* FIXME: should pass written-to range and not just single target */
             is_pretend_writable_address(target))) {
            app_pc cur_esp;

            /* not an app exception */
            RSTATS_DEC(num_exceptions);
            DOSTATS({
                if (!TEST(MOD_CODE_TAKEOVER, flags))
                    STATS_INC(num_native_cachecons_faults);
            });
            LOG(THREAD, LOG_ASYNCH, 2,
                "check_for_modified_code: seg fault in exec-writable region @" PFX "\n",
                target);
            /* we're not going to return through either of the usual
             * methods, so we have to free the initstack_mutex, but
             * we need a stack -- so, we use a separate method to avoid
             * stack conflicts, and switch to dstack now.
             */
            GET_STACK_PTR(cur_esp);
            /* prepare flags param for found_modified_code */
            mod_flags = flags;
            if (emulate_write)
                mod_flags |= MOD_CODE_EMULATE_WRITE;
            /* don't switch to base of dstack if already on it b/c we'll end
             * up clobbering the fragment_t wrapper local from parent
             */
            if (is_on_dstack(dcontext, cur_esp)) {
                found_modified_code(dcontext, pExcptRec, cxt, target, mod_flags, f);
            } else {
                call_modcode_alt_stack(dcontext, pExcptRec, cxt, target, mod_flags,
                                       is_on_initstack(cur_esp), f);
            }
            ASSERT_NOT_REACHED();
        }
#ifdef DGC_DIAGNOSTICS
        else {
            /* make all heap RO in attempt to view generation of DGC */
            DOLOG(3, LOG_VMAREAS, {
                /* WARNING: assuming here that app never seg faults on its own */
                char buf[MAXIMUM_SYMBOL_LENGTH];
                app_pc base;
                size_t size;
                bool ok = get_memory_info(target, &base, &size, NULL);
                cache_pc instr_cache_pc = (app_pc)pExcptRec->ExceptionAddress;
                app_pc translated_pc;
                ASSERT(ok);
                LOG(THREAD, LOG_ASYNCH, 1,
                    "got seg fault @" PFX " in non-E region we made RO " PFX "-" PFX "\n",
                    target, base, base + size);
                LOG(THREAD, LOG_ASYNCH, 2, "found_modified_code: translating " PFX "\n",
                    instr_cache_pc);
                /* For safe recreation we need to either be couldbelinking or hold the
                 * initexit lock (to keep someone from flushing current fragment), the
                 * initexit lock is easier
                 */
                d_r_mutex_lock(&thread_initexit_lock);
                translated_pc = recreate_app_pc(dcontext, instr_cache_pc, f);
                ASSERT(translated_pc != NULL);
                d_r_mutex_unlock(&thread_initexit_lock);
                LOG(THREAD, LOG_ASYNCH, 2, "\tinto " PFX "\n", translated_pc);
                print_symbolic_address(translated_pc, buf, sizeof(buf), false);
                LOG(THREAD, LOG_VMAREAS, 1,
                    "non-code written by app pc " PFX " from bb %s:\n", translated_pc,
                    buf);
                DOLOG(1, LOG_VMAREAS,
                      { disassemble_app_bb(dcontext, translated_pc, THREAD); });
                LOG(THREAD, LOG_ASYNCH, 1, "Making " PFX "-" PFX " writable\n", base,
                    base + size);
                ok = make_writable(base, size);
                ASSERT(ok);
                /* now re-execute the write
                 * don't try to go through entire exception route by setting up
                 * our own exception handler directly in TIB -- not transparent,
                 * requires user stack!  just call NtContinue here
                 */
                LOG(THREAD, LOG_ASYNCH, 1,
                    "\tresuming write instr @ " PFX ", esp=" PFX "\n", cxt->CXT_XIP,
                    cxt->CXT_XSP);
                EXITING_DR();
                nt_continue(cxt);
                ASSERT_NOT_REACHED();
            });
        }
#endif
    }
}

/* SEH Definitions */
/* returns current head of exception list
   assumes we haven't installed our own exception handler
   hence we can just read it off TIB
*/
EXCEPTION_REGISTRATION *
get_exception_list()
{
    /* typedef struct _NT_TIB { */
    /*     struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList; */
    return (EXCEPTION_REGISTRATION *)d_r_get_tls(EXCEPTION_LIST_TIB_OFFSET);
}

/* verify exception handler list is consistent */
/* Used as a first level check for handler integrity before throwing an exception. */
/* These checks are best effort and following through a handler may
   still result in subsequent violations of our policies,
   e.g. attacked handler can point to a valid RET that will fail our checks later.
*/
/* returns depth so that can caller can decide whether
    it is worth throwing an exception if anyone would be there to catch it
    1 only the default handler is there
    0 empty shouldn't happen
   -1 when invalid
*/
int
exception_frame_chain_depth(dcontext_t *dcontext)
{
    int depth = 0;
    EXCEPTION_REGISTRATION *pexcrec = get_exception_list();
    app_pc stack_base, stack_top;
    get_stack_bounds(dcontext, &stack_base, &stack_top);

    LOG(THREAD_GET, LOG_ASYNCH, 2, "ASYNCH exception_frame_chain_depth head: " PFX "\n",
        pexcrec);

    for (; (EXCEPTION_REGISTRATION *)PTR_UINT_MINUS_1 != pexcrec;
         pexcrec = pexcrec->prev) {
        if (!ALIGNED(pexcrec, 4)) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "WARNING: ASYNCH invalid chain - not DWORD aligned\n");
            return -1;
        }
        /* each memory location should be readable (don't want to die while checking) */
        /* heavy weight check */
        if (!is_readable_without_exception((app_pc)pexcrec,
                                           sizeof(EXCEPTION_REGISTRATION))) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "ASYNCH exception_frame_chain_depth " PFX " invalid! "
                "possibly under attack\n",
                pexcrec);
            return -1;
        }
        LOG(THREAD_GET, LOG_ASYNCH, 2,
            "ASYNCH exception_frame_chain_depth[%d] " PFX ", handler: " PFX ", prev: " PFX
            "\n",
            depth, pexcrec, pexcrec->handler, pexcrec->prev);
        /* prev address should be higher in memory than current */
        if (pexcrec->prev <= pexcrec) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "WARNING: ASYNCH invalid chain - not strictly up on the stack\n");
            return -1;
        }
        /* check against stack limits */
        if ((stack_base > (app_pc)pexcrec) ||
            (stack_top < (app_pc)pexcrec + sizeof(EXCEPTION_REGISTRATION))) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "WARNING: ASYNCH invalid chain - " PFX " not on 'official' stack " PFX
                "-" PFX "\n",
                pexcrec, stack_base, stack_top);
            return -1;
        }

        /* FIXME: the handler pc should pass the code origins check --
           it is possible that we have failed on it to begin with
           - check_origins_helper() unfortunately has side effects that we may not want..)
           and furthermore we may be coming from there - need to restructure that code
           we want a quick check with no action taken there - something like
           check_thread_vm_area(dcontext, pexcrec->handler, NULL, NULL)
           or maybe a variant of check_origins_helper(dcontext, pexcrec->handler, ...)
        */
        ASSERT_NOT_IMPLEMENTED(true); /* keep going for now  */

        /* make sure we don't get in an infinite loop
           (shouldn't be possible after the prev <= exrec check) */
        if (depth++ > 100) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "ASYNCH frame[%d]: too deep chain, possibly corrupted\n", depth);
            return -1;
        }
    }
    LOG(THREAD_GET, LOG_ASYNCH, 1, "ASYNCH exception_frame_chain_depth depth=%d\n",
        depth);

    return depth; /* FIXME: return length */
}

#ifdef DEBUG
void
dump_context_info(CONTEXT *context, file_t file, bool all)
{
#    define DUMP(r) LOG(file, LOG_ASYNCH, 2, #    r "=" PFX " ", context->r);
#    define DUMPNM(r, nm) LOG(file, LOG_ASYNCH, 2, #    nm "=" PFX " ", context->r);
#    define NEWLINE LOG(file, LOG_ASYNCH, 2, "\n  ");
    DUMP(ContextFlags);
    NEWLINE;

    if (all || context->ContextFlags & CONTEXT_INTEGER) {
        DUMPNM(CXT_XDI, Xdi);
        DUMPNM(CXT_XSI, Xsi);
        DUMPNM(CXT_XBX, Xbx);
        NEWLINE;
        DUMPNM(CXT_XDX, Xdx);
        DUMPNM(CXT_XCX, Xcx);
        DUMPNM(CXT_XAX, Xax);
        NEWLINE;
#    ifdef X64
        DUMPNM(CXT_XBP, Xbp);
        DUMP(R8);
        DUMP(R9);
        NEWLINE;
        DUMP(R10);
        DUMP(R11);
        DUMP(R12);
        NEWLINE;
        DUMP(R13);
        DUMP(R14);
        DUMP(R15);
        NEWLINE;
#    endif
    }

    if (all || context->ContextFlags & CONTEXT_CONTROL) {
#    ifndef X64
        DUMPNM(CXT_XBP, Xbp);
#    endif
        DUMPNM(CXT_XIP, Xip);
        DUMP(SegCs); // MUST BE SANITIZED
        NEWLINE;
        DUMPNM(CXT_XFLAGS, XFlags); // MUST BE SANITIZED
        DUMPNM(CXT_XSP, Xsp);
        DUMP(SegSs);
        NEWLINE;
    }

    if (all || context->ContextFlags & CONTEXT_DEBUG_REGISTERS) {
        DUMP(Dr0);
        DUMP(Dr1);
        DUMP(Dr2);
        DUMP(Dr3);
        NEWLINE;
        DUMP(Dr6);
        DUMP(Dr7);
        NEWLINE;
    }

    /* For PR 264138 */
    /* Even if all, we have to ensure we have the ExtendedRegister fields,
     * which for a dynamically-laid-out context may not exist (i#1223).
     */
    /* XXX i#1312: This will need attention for AVX-512, specifically the different
     * xstate formats supported by the processor, compacted and standard, as well as
     * MPX.
     */
    if ((all && !CONTEXT_DYNAMICALLY_LAID_OUT(context->ContextFlags)) ||
        TESTALL(CONTEXT_XMM_FLAG, context->ContextFlags)) {
        int i, j;
        byte *ymmh_area;
        for (i = 0; i < proc_num_simd_sse_avx_saved(); i++) {
            LOG(file, LOG_ASYNCH, 2, "xmm%d=0x", i);
            /* This would be simpler if we had uint64 fields in dr_xmm_t but
             * that complicates our struct layouts */
            for (j = 0; j < 4; j++) {
                LOG(file, LOG_ASYNCH, 2, "%08x", CXT_XMM(context, i)->u32[j]);
            }
            NEWLINE;
            if (TESTALL(CONTEXT_YMM_FLAG, context->ContextFlags)) {
                ymmh_area = context_ymmh_saved_area(context);
                LOG(file, LOG_ASYNCH, 2, "ymmh%d=0x", i);
                for (j = 0; j < 4; j++) {
                    LOG(file, LOG_ASYNCH, 2, "%08x", YMMH_AREA(ymmh_area, i).u32[j]);
                }
                NEWLINE;
            }
        }
    }

    if (all || context->ContextFlags & CONTEXT_FLOATING_POINT) {
        LOG(THREAD_GET, LOG_ASYNCH, 2, "<floating point area>\n  ");
    }

    if (all || context->ContextFlags & CONTEXT_SEGMENTS) {
        DUMP(SegGs);
        DUMP(SegFs);
        DUMP(SegEs);
        DUMP(SegDs);
    }
    LOG(file, LOG_ASYNCH, 2, "\n");
#    undef DUMP
#    undef DUMPNM
#    undef NEWLINE
}

static const char *
exception_access_violation_type(ptr_uint_t code)
{
    if (code == EXCEPTION_INFORMATION_READ_EXECUTE_FAULT)
        return "read";
    else if (code == EXCEPTION_INFORMATION_WRITE_FAULT)
        return "write";
    else if (code == EXCEPTION_INFORMATION_EXECUTE_FAULT)
        return "execute";
    else
        return "UNKNOWN";
}

static void
dump_exception_info(EXCEPTION_RECORD *exception, CONTEXT *context)
{
    LOG(THREAD_GET, LOG_ASYNCH, 2,
        "\texception code = " PFX ", ExceptionFlags=" PFX "\n\trecord=" PFX
        ", params=%d\n",
        exception->ExceptionCode, exception->ExceptionFlags,
        exception->ExceptionRecord, /* follow if non NULL */
        exception->NumberParameters);
    if (exception->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        LOG(THREAD_GET, LOG_ASYNCH, 2, "\tPC " PFX " tried to %s address " PFX "\n",
            exception->ExceptionAddress,
            exception_access_violation_type(exception->ExceptionInformation[0]),
            exception->ExceptionInformation[1]);
    }
    dump_context_info(context, THREAD_GET, false);
}

static void
dump_exception_frames()
{
    int depth = 0;
    EXCEPTION_REGISTRATION *pexcrec = get_exception_list();

    LOG(THREAD_GET, LOG_ASYNCH, 2,
        "ASYNCH dump_exception_frames SEH frames head: " PFX "\n", pexcrec);

    // 0xFFFFFFFF indicates the end of list
    while ((EXCEPTION_REGISTRATION *)PTR_UINT_MINUS_1 != pexcrec) {
        /* heavy weight check */
        if (!is_readable_without_exception((app_pc)pexcrec,
                                           sizeof(EXCEPTION_REGISTRATION))) {
            LOG(THREAD_GET, LOG_ASYNCH, 1,
                "ASYNCH dump_exception_frames " PFX " invalid! possibly corrupt\n",
                pexcrec);
            return;
        }
        DOLOG(2, LOG_ASYNCH, {
            char symbolbuf[MAXIMUM_SYMBOL_LENGTH];
            print_symbolic_address(pexcrec->handler, symbolbuf, sizeof(symbolbuf), false);
            LOG(THREAD_GET, LOG_ASYNCH, 2,
                "ASYNCH frame[%d]: " PFX "  handler: " PFX " %s, prev: " PFX "\n", depth,
                pexcrec, pexcrec->handler, symbolbuf, pexcrec->prev);
        });

        pexcrec = pexcrec->prev;
        if (depth++ > 100) {
            LOG(THREAD_GET, LOG_ASYNCH, 2,
                "ASYNCH frame[%d]: too deep chain, possibly corrupted\n", depth);
            break;
        }
    }
}

#endif /* DEBUG */

/* Data structure(s) pointed to by Visual C++ extended exception frame
 * WARNING: these are compiler-dependent and we cannot count on any
 * particular exception frame looking like this
 */
typedef struct scopetable_entry_t {
    DWORD previousTryLevel;
    PVOID lpfnFilter;
    PVOID lpfnHandler;
} scopetable_entry_t;

/* The extended exception frame used by Visual C++ */
typedef struct _vc_exception_registration_t {
    EXCEPTION_REGISTRATION exception_base;
    struct scopetable_entry_t *scopetable;
    int trylevel;
    int _ebp;
} vc_exception_registration_t;

#ifdef DEBUG
/* display the extended exception frame used by Visual C++
 * There is at most one EXCEPTION_REGISTRATION record per function,
 * the rest is compiler dependent and we don't want to depend on that...
 */
void
dump_vc_exception_frame(EXCEPTION_REGISTRATION *pexcreg)
{
    vc_exception_registration_t *pVCExcRec = (vc_exception_registration_t *)pexcreg;
    struct scopetable_entry_t *pScopeTableEntry = pVCExcRec->scopetable;
    int i;
    for (i = 0; i <= pVCExcRec->trylevel; i++) {
        LOG(THREAD_GET, LOG_ASYNCH, 2,
            "\t scope[%u] PrevTry: " PFX "  "
            "filter: " PFX "  __except: " PFX "\n",
            i, pScopeTableEntry->previousTryLevel, pScopeTableEntry->lpfnFilter,
            pScopeTableEntry->lpfnHandler);
        pScopeTableEntry++;
    }
}
#endif /* DEBUG */

static void
report_app_exception(dcontext_t *dcontext, uint appfault_flags,
                     EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt, const char *prefix)
{
    report_app_problem(
        dcontext, appfault_flags, pExcptRec->ExceptionAddress, (byte *)cxt->CXT_XBP,
        "\n%s\nCode=0x%08x Flags=0x%08x Param0=" PFX " Param1=" PFX "\n", prefix,
        pExcptRec->ExceptionCode, pExcptRec->ExceptionFlags,
        (pExcptRec->NumberParameters >= 1) ? pExcptRec->ExceptionInformation[0] : 0,
        (pExcptRec->NumberParameters >= 2) ? pExcptRec->ExceptionInformation[1] : 0);
}

void
report_internal_exception(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                          uint dumpcore_flag, const char *prefix, const char *crash_label)
{
    /* WARNING: a fault in DR means that potentially anything could be
     * inconsistent or corrupted!  Do not grab locks or traverse
     * data structures or read memory if you can avoid it!
     */
    /* Note this format string is at its limit.  Do not add anything
     * else without compressing.  xref PR 204171.
     * report_dynamorio_problem has an allocated buffer size that
     * assumes the size here is MAX_PATH+11
     */
    const char *fmt = "%s %s at PC " PFX "\n"
                      "0x%08x 0x%08x " PFX " " PFX " " PFX " " PFX "\n"
                      "Base: " PFX "\n"
                      "Registers: eax=" PFX " ebx=" PFX " ecx=" PFX " edx=" PFX "\n"
                      "\tesi=" PFX " edi=" PFX " esp=" PFX " ebp=" PFX "\n"
#ifdef X64
                      "\tr8 =" PFX " r9 =" PFX " r10=" PFX " r11=" PFX "\n"
                      "\tr12=" PFX " r13=" PFX " r14=" PFX " r15=" PFX "\n"
#endif
                      "\teflags=" PFX;

    /* We used to adjust the address to be offset from the preferred base
     * but I think that's just confusing so I removed it.
     */

    DODEBUG({
        /* also check for a self-protection bug: write fault accessing data section */
        if (pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            pExcptRec->ExceptionInformation[0] == 1 /* write */) {
            app_pc target = (app_pc)pExcptRec->ExceptionInformation[1];
            if (is_in_dynamo_dll(target)) {
                const char *sec = get_data_section_name(target);
                SYSLOG_INTERNAL_CRITICAL("Self-protection bug: %s written to @" PFX,
                                         sec == NULL ? "" : sec, target);
            }
        }
    });

    /* FIXME: we need to test whether the exception is due to
       Guard page violation - code 80000001 (not our own stack guard though)
       Stack overflow - code c00000fd (last guard page touched)

       We may get there because intercept_exception uses the application stack
       before calling asynch_take_over and in case of an invalid exception handler
       this will trickle down to it.

       We can currently get here if trying to read application pages marked as
       GUARD pages, or marked as RESERVEd but not committed - in which case
       we'll receive a general Access violation - code c0000005 and we'd have to
       verify the page flags.  Again we may want to treat differently our own
       guard pages than the application uncommitted pages.
    */

    /* note that the first adjusted_exception_addr is used for
     * eventlog, and the second for forensics, and so both need to be adjusted
     */
    report_dynamorio_problem(
        dcontext, dumpcore_flag, (app_pc)pExcptRec->ExceptionAddress,
        (app_pc)cxt->CXT_XBP, fmt, prefix, crash_label,
        (app_pc)pExcptRec->ExceptionAddress, pExcptRec->ExceptionCode,
        pExcptRec->ExceptionFlags, cxt->CXT_XIP, pExcptRec->ExceptionAddress,
        (pExcptRec->NumberParameters >= 1) ? pExcptRec->ExceptionInformation[0] : 0,
        (pExcptRec->NumberParameters >= 2) ? pExcptRec->ExceptionInformation[1] : 0,
        get_dynamorio_dll_start(), cxt->CXT_XAX, cxt->CXT_XBX, cxt->CXT_XCX, cxt->CXT_XDX,
        cxt->CXT_XSI, cxt->CXT_XDI, cxt->CXT_XSP, cxt->CXT_XBP,
#ifdef X64
        cxt->R8, cxt->R9, cxt->R10, cxt->R11, cxt->R12, cxt->R13, cxt->R14, cxt->R15,
#endif
        cxt->CXT_XFLAGS);
}

void
internal_exception_info(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                        bool dstack_overflow, bool is_client)
{
    report_internal_exception(
        dcontext, pExcptRec, cxt,
        (is_client ? DUMPCORE_CLIENT_EXCEPTION : DUMPCORE_INTERNAL_EXCEPTION) |
            (dstack_overflow ? DUMPCORE_STACK_OVERFLOW : 0),
        /* for clients we need to let them override the label */
        is_client ? exception_label_client : exception_label_core,
        dstack_overflow ? STACK_OVERFLOW_NAME : CRASH_NAME);
}

static void
internal_dynamo_exception(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                          bool is_client)
{
    /* recursive bailout: avoid infinite loop due to fault in fault handling
     * by using DO_ONCE
     * FIXME: any worries about lack of mutex w/ DO_ONCE?
     */
    /* PR 203701: If we've exhausted the dstack, then we'll switch
     * to a separate exception handling stack to make sure we have
     * enough space to report the problem.  One guard page is not
     * always sufficient.
     */
    DO_ONCE({
        if (is_dstack_overflow(dcontext, pExcptRec, cxt) && exception_stack != NULL) {
            d_r_mutex_lock(&exception_stack_lock);
            call_intr_excpt_alt_stack(dcontext, pExcptRec, cxt, exception_stack);
            d_r_mutex_unlock(&exception_stack_lock);
        } else {
            internal_exception_info(dcontext, pExcptRec, cxt, false, is_client);
        }
    });
    os_terminate(dcontext, TERMINATE_PROCESS);
    ASSERT_NOT_REACHED();
}

/* heuristic check whether an exception is due to execution or due to
 * a read from unreadable memory */
static bool
is_execution_exception(EXCEPTION_RECORD *pExcptRec)
{
    app_pc fault_pc = pExcptRec->ExceptionAddress;
    app_pc target = (app_pc)pExcptRec->ExceptionInformation[1];
    bool execution = false;

    ASSERT(pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION);

    if (pExcptRec->ExceptionInformation[0] == EXCEPTION_INFORMATION_EXECUTE_FAULT) {
        /* certainly execution */
        execution = true;
    }

    /* FIXME: case 5879 should know if running on an NX capable
     * machine and whether this information code depends on whether
     * the current application is NX compatible. Verify that if this
     * is not set, it is expected to be just a read fault.  For the
     * time being using the read/execute heuristic all the time
     */

    if (pExcptRec->ExceptionInformation[0] == EXCEPTION_INFORMATION_READ_EXECUTE_FAULT) {
        if (fault_pc == target) {
            /* certainly execution */
            execution = true;
        } else if (fault_pc < target && target < fault_pc + MAX_INSTR_LENGTH) {
            /* near a page boundary crossing a read exception can
             * happen either while reading the instruction (execution)
             * or while the instruction is reading from a nearby
             * location.  Without decoding cannot say which one is it,
             * but most likely it is execution.
             */

            /* unclear whether execution or read */

            /* if we are not crossing a page boundary we can't
             * possibly fail, so should ASSERT on that, and also on
             * is_readable_without_exception() to make sure we don't
             * have a race
             */

            execution = true; /* execution more likely */
            /* FIXME: case 1948 actually has to deal with this more
             * precisely when instructions may cross pages with
             * different permissions */
            ASSERT_NOT_IMPLEMENTED(false);
        } else {
            /* read otherwise */
            execution = false;
        }
    }
    return execution;
}

static void
client_exception_event(dcontext_t *dcontext, CONTEXT *cxt, EXCEPTION_RECORD *pExcptRec,
                       priv_mcontext_t *raw_mcontext, fragment_t *fragment)
{
    /* We cannot use the heap, as clients are allowed to call dr_redirect_execution()
     * and not come back.  So we use the stack, but we separate from
     * intercept_exception() to avoid adding two mcontexts to its stack usage
     * (we have to add one for the pre-translation raw_mcontext).
     * We should only come here for in-fcache faults, so we should have
     * plenty of stack space.
     */
    dr_exception_t einfo;
    dr_mcontext_t xl8_dr_mcontext;
    dr_mcontext_t raw_dr_mcontext;
    fragment_t wrapper;
    bool pass_to_app;
    dr_mcontext_init(&xl8_dr_mcontext);
    dr_mcontext_init(&raw_dr_mcontext);
    einfo.record = pExcptRec;
    context_to_mcontext(dr_mcontext_as_priv_mcontext(&xl8_dr_mcontext), cxt);
    einfo.mcontext = &xl8_dr_mcontext;
    priv_mcontext_to_dr_mcontext(&raw_dr_mcontext, raw_mcontext);
    einfo.raw_mcontext = &raw_dr_mcontext;
    /* i#207 fragment tag and fcache start pc on fault. */
    einfo.fault_fragment_info.tag = NULL;
    einfo.fault_fragment_info.cache_start_pc = NULL;
    if (fragment == NULL)
        fragment = fragment_pclookup(dcontext, einfo.raw_mcontext->pc, &wrapper);
    if (fragment != NULL && !hide_tag_from_client(fragment->tag)) {
        einfo.fault_fragment_info.tag = fragment->tag;
        einfo.fault_fragment_info.cache_start_pc = FCACHE_ENTRY_PC(fragment);
        einfo.fault_fragment_info.is_trace = TEST(FRAG_IS_TRACE, fragment->flags);
        einfo.fault_fragment_info.app_code_consistent =
            !TESTANY(FRAG_WAS_DELETED | FRAG_SELFMOD_SANDBOXED, fragment->flags);
    }

    /* i#249: swap PEB pointers.  We assume that no other Ki-handling code needs
     * the PEB swapped, as our hook code does not swap like fcache enter/return
     * and clean calls do.  We do swap when recreating app state.
     */
    swap_peb_pointer(dcontext, true /*to priv*/);
    /* We allow client to change context */
    pass_to_app = instrument_exception(dcontext, &einfo);
    swap_peb_pointer(dcontext, false /*to app*/);

    if (pass_to_app) {
        CLIENT_ASSERT(einfo.mcontext->flags == DR_MC_ALL,
                      "exception mcontext flags cannot be changed");
        /* cxt came from the kernel, so it should already have ss and cs
         * initialized. Thus there's no need to get them again.
         */
        mcontext_to_context(cxt, dr_mcontext_as_priv_mcontext(einfo.mcontext),
                            true /* !set_cur_seg */);
    } else {
        CLIENT_ASSERT(einfo.raw_mcontext->flags == DR_MC_ALL,
                      "exception mcontext flags cannot be changed");
        /* cxt came from the kernel, so it should already have ss and cs
         * initialized. Thus there's no need to get them again.
         */
        mcontext_to_context(cxt, dr_mcontext_as_priv_mcontext(einfo.raw_mcontext),
                            true /* !set_cur_seg */);
        /* Now re-execute the faulting instr, or go to
         * new context specified by client, skipping
         * app exception handlers.
         */
        EXITING_DR();
        nt_continue(cxt);
    }
}

static void
check_internal_exception(dcontext_t *dcontext, CONTEXT *cxt, EXCEPTION_RECORD *pExcptRec,
                         app_pc forged_exception_addr, priv_mcontext_t *raw_mcontext)
{
    /* even though in_fcache is the much more common path (we hope! :)),
     * it grabs a lock, so we check for DR exceptions first, hoping to
     * avoid livelock due to us crashing while holding the fcache lock
     */
    /* FIXME : we still might pass exceptions that are our fault back to
     * the app (in a client library, global do syscall, client library dgc
     * maybe others?). Also is the on_dstack/on_initstack check too
     * general?  We might take responsibility for app crashes if their esp
     * gets set to random address that happens to match one of our stacks.
     * We could additionally require that the pc is also in ntdll.dll or
     * kernel32.dll (that would cover cases (like bug 3516) where we call
     * out to other dlls) though as it is now it may cover some of the
     * remaining holes (client library for instance).  Is also possible
     * that we could take responsibility for an app exception that occurs
     * in the first few instructions of a location we hooked (since if
     * we didn't takover at the hook, it would execute out of the
     * interception buffer (guard page on stack for instance)).
     */
    /* Note the is_on_[init/d]stack routines count any guard pages as part
     * of the stack */
    bool is_DR_exception = false;
    if ((is_on_dstack(dcontext, (byte *)cxt->CXT_XSP)
         /* PR 302951: clean call arg processing => pass to app/client.
          * Rather than call the risky in_fcache we check whereami. */
         && (dcontext->whereami != DR_WHERE_FCACHE ||
             /* i#263: do not pass to app if fault is in
              * client lib or ntdll called by client
              */
             is_in_client_lib((app_pc)pExcptRec->ExceptionAddress) ||
             is_in_ntdll((app_pc)pExcptRec->ExceptionAddress))) ||
        is_on_initstack((byte *)cxt->CXT_XSP)) {
        is_DR_exception = true;
    }
    /* Is this an exception forged by DR that should be passed on
     * to the app? */
    else if (forged_exception_addr != (app_pc)pExcptRec->ExceptionAddress) {
        if (is_in_dynamo_dll((app_pc)pExcptRec->ExceptionAddress))
            is_DR_exception = true;
        else {
            /* we go ahead and grab locks here to do a negative test for
             * !in_fcache rather than trying to enumerate all non-cache
             * categories, as we'll have to grab a lock anyway to find
             * whether in a separate stub region.  we do this last to
             * reduce the scenarios in which we won't report a crash.
             */
            if (is_dynamo_address((app_pc)pExcptRec->ExceptionAddress) &&
                !in_fcache(pExcptRec->ExceptionAddress)) {
                /* PR 451074: client needs a chance to handle exceptions in its
                 * own gencode.  client_exception_event() won't return if client
                 * wants to re-execute faulting instr.
                 */
                if (CLIENTS_EXIST()) {
                    /* raw_mcontext equals mcontext */
                    context_to_mcontext(raw_mcontext, cxt);
                    client_exception_event(dcontext, cxt, pExcptRec, raw_mcontext, NULL);
                }
                is_DR_exception = true;
            }
        }
    }
    if (is_DR_exception) {
        /* Check if we ended up decoding from unreadable memory due to an
         * app race condition (case 845) or hit an IN_PAGE_ERROR (case 10567) */
        if ((pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
             pExcptRec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
            (pExcptRec->NumberParameters >= 2) &&
            (pExcptRec->ExceptionInformation[0] ==
             EXCEPTION_INFORMATION_READ_EXECUTE_FAULT)) {
            app_pc target_addr = (app_pc)pExcptRec->ExceptionInformation[1];
            ASSERT((pExcptRec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) ||
                   !is_readable_without_exception(target_addr, 4));
            /* for shared fragments, the bb building lock is what prevents
             * another thread from changing the shared last_area before
             * we check it
             * note: for hotp_only or thin_client, this shouldn't trigger,
             *       especially for thin_client because it will crash as
             *       uninitialized vmarea_vectors will be accessed.
             */
            if (!RUNNING_WITHOUT_CODE_CACHE() &&
                check_in_last_thread_vm_area(dcontext, target_addr)) {

                dr_exception_type_t exception_type =
                    (pExcptRec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
                    ? IN_PAGE_ERROR_EXCEPTION
                    : UNREADABLE_MEMORY_EXECUTION_EXCEPTION;

                /* The last decoded application pc should always be in the
                 * thread's last area, yet if code executed by one thread
                 * is unmapped by another we may have let it through
                 * check_thread_vm_area and into decode*()
                 */
                SYSLOG_INTERNAL_ERROR("(decode) exception in last area, "
                                      "%s: dr pc=" PFX ", app pc=" PFX,
                                      (exception_type == IN_PAGE_ERROR_EXCEPTION)
                                          ? "in_page_error"
                                          : "probably app race condition",
                                      (app_pc)pExcptRec->ExceptionAddress, target_addr);
                STATS_INC(num_exceptions_decode);
                if (is_building_trace(dcontext)) {
                    LOG(THREAD, LOG_ASYNCH, 2,
                        "intercept_exception: "
                        "squashing old trace\n");
                    trace_abort(dcontext);
                }
                /* we do get faults when not building a bb: e.g.,
                 * ret_after_call_check does decoding (case 9396) */
                if (dcontext->bb_build_info != NULL) {
                    /* must have been building a bb at the time */
                    bb_build_abort(dcontext, true /*clean vm area*/, true /*unlock*/);
                }
                /* FIXME: if necessary, have a separate dump core mask for
                 * in_page_error */
                /* Let's pass it back to the application - memory is unreadable */
                if (TEST(DUMPCORE_FORGE_UNREAD_EXEC, DYNAMO_OPTION(dumpcore_mask)))
                    os_dump_core("Warning: Racy app execution (decode unreadable)");
                os_forge_exception(target_addr, exception_type);

                /* CHECK: I hope we're not covering up the symptom instead
                 * of fixing the real cause
                 */
                ASSERT_NOT_REACHED();
            }
        }

        internal_dynamo_exception(dcontext, pExcptRec, cxt, false);
        ASSERT_NOT_REACHED();
    }
}

/*
 * Exceptions:
 * Kernel gives control to KiUserExceptionDispatcher.
 * It examines linked list of EXCEPTION_REGISTRATION_RECORDs, which
 * each contain a callback function and a next pointer.
 * Calls each callback function, passing 0 for the exception flags,
 * asking what they want to do.  Callback function corresponds to the filter
 * for the __except block.  Typically a Visual C++ wrapper routine
 * __except_handler3 is used, which expects filter return values of:
 *   EXCEPTION_EXECUTE_HANDLER = execute this handler
 *   NT_CONTINUE_SEARCH = keep walking chain of handlers
 *   NT_CONTINUE_EXECUTION = re-execute faulting instruction
 * Once an accepting filter is found, walks chain of records again
 * by calling __global_unwind2(), which calls the callback functions again,
 * passing 2 (==EH_UNWINDING) as the flag, giving each a chance
 * to clean up.  Accepting handler is responsible for properly setting
 * app state (including stack to be same as frame that contains handler
 * code) and then jumping to the right pc -- __except_handler3 does all this,
 * then calls code corresponding to __except block {} itself.
 *
 * Error during exception handler search -> raises an unhandleable exception
 * Global unwind = handler itself calls RtlUnwind
 * RtlUnwind calls NtContinue to continue execution at faulty instruction or
 *   after handler
 *
KiUserExceptionDispatcher:
  77F9F054: 8B 4C 24 04        mov         ecx,dword ptr [esp+4]
  77F9F058: 8B 1C 24           mov         ebx,dword ptr [esp]
  77F9F05B: 51                 push        ecx
  ...

 * This is entered directly from the kernel, so there is no return address on
 * the stack like there is with NtContinue
 *
 * ASSUMPTIONS:
 *    1) *esp = EXCEPTION_RECORD*
 *    2) *(esp+4) == CONTEXT*
 * For x64:
 *    1) *rsp = CONTEXT
 *    2) *(rsp+sizeof(CONTEXT)) = EXCEPTION_RECORD
 */
/* Remember that every path out of here must invoke the DR exit hook.
 * The normal return path will do so as the interception code has an
 * enter and exit hook around the call to this routine.
 */
static after_intercept_action_t /* note return value will be ignored */
intercept_exception(app_state_at_intercept_t *state)
{
    /* FIXME : if dr is calling through Nt* wrappers that are hooked
     * (say by sygate's sysfer.dll) they may generate and handle exceptions
     * (throw) for which we should backout here.  (We should neither take
     * responsibility nor start interpreting).  Keep in mind we make some
     * syscalls on the app stack (intercept_apc -> init etc.) and that we may
     * also make some before a dcontext is created for the thread.  Right now
     * we avoid calling through sygate's hooks (do our own system calls). */
    /* FIXME - no real scheme to handle us calling SYSFER hooks that trip over
     * a region we made read-only. */

    /* we intercept known threads even if !intercept_asynch_for_self, to
     * handle write faults on regions we made RO
     */

    /* if we have a valid dcontext then we're really valid, but we
     * could also have been just created so we also allow
     * is_thread_known().
     * FIXME: is_thread_known() may be unnecessary */
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (dynamo_exited && d_r_get_num_threads() > 1) {
        /* PR 470957: this is almost certainly a race so just squelch it.
         * We live w/ the risk that it was holding a lock our release-build
         * exit code needs.
         */
        nt_terminate_thread(NT_CURRENT_THREAD, 0);
    }

    if (intercept_asynch_global() &&
        (dcontext != NULL || is_thread_known(d_r_get_thread_id()))) {
        priv_mcontext_t mcontext;
        app_pc forged_exception_addr;
        EXCEPTION_RECORD *pExcptRec;
        CONTEXT *cxt;
        cache_pc faulting_pc;
        byte *fault_xsp;
        /* if !takeover, we handle our-fault write faults, but then let go */
        bool takeover = intercept_asynch_for_self(false /*no unknown threads*/);
        bool thread_is_lost = false; /* temporarily native (UNDER_DYN_HACK) */
        priv_mcontext_t raw_mcontext;
        DEBUG_DECLARE(bool known_source = false;)

        /* grab parameters to native method */
#ifdef X64
        if (get_os_version() >= WINDOWS_VERSION_7) {
            /* XXX: there are 32 bytes worth of extra stuff between
             * CONTEXT and EXCEPTION_RECORD.  Not sure what it is.
             */
            pExcptRec = (EXCEPTION_RECORD *)(state->mc.xsp + sizeof(CONTEXT) + 0x20);
        } else
            pExcptRec = (EXCEPTION_RECORD *)(state->mc.xsp + sizeof(CONTEXT));
        cxt = (CONTEXT *)state->mc.xsp;
#else
        pExcptRec = *((EXCEPTION_RECORD **)(state->mc.xsp));
        cxt = *((CONTEXT **)(state->mc.xsp + XSP_SZ));
#endif
        fault_xsp = (byte *)cxt->CXT_XSP;

        if (dcontext == NULL && !is_safe_read_pc((app_pc)cxt->CXT_XIP) &&
            (dynamo_initialized || global_try_except.try_except_state == NULL)) {
            ASSERT_NOT_TESTED();
            SYSLOG_INTERNAL_CRITICAL("Early thread failure, no dcontext\n");
            /* there is no good reason for this, other than DR error */
            ASSERT(is_dynamo_address((app_pc)pExcptRec->ExceptionAddress));
            pExcptRec->ExceptionFlags = 0xbadDC;
            internal_dynamo_exception(dcontext, pExcptRec, cxt, false);
            ASSERT_NOT_REACHED();
        }

        forged_exception_addr =
            (dcontext == NULL) ? NULL : dcontext->forged_exception_addr;

        /* FIXME : we'd like to retakeover lost-control threads, but we need
         * to correct writable->read_only faults etc. as if for a native thread
         * and the helper routines (below code, check/found/handle modified
         * code) don't support a native thread that we want to retakeover (ref
         * case 6069). So, we just treat the thread as a native_exec thread
         * and wait for a later retakeover point to regain control. */
        if (IS_UNDER_DYN_HACK(takeover)) {
            STATS_INC(num_except_while_lost);
            thread_is_lost = true;
            takeover = false;
        }

        if (dcontext != NULL)
            SELF_PROTECT_LOCAL(dcontext, WRITABLE);
        /* won't be re-protected until d_r_dispatch->fcache */

        RSTATS_INC(num_exceptions);

        if (dcontext != NULL)
            dcontext->forged_exception_addr = NULL;

        LOG(THREAD, LOG_ASYNCH, 1,
            "ASYNCH intercepted exception in %sthread " TIDFMT " at pc " PFX "\n",
            takeover ? "" : "non-asynch ", d_r_get_thread_id(),
            pExcptRec->ExceptionAddress);
        DOLOG(2, LOG_ASYNCH, {
            if (cxt->CXT_XIP != (ptr_uint_t)pExcptRec->ExceptionAddress) {
                LOG(THREAD, LOG_ASYNCH, 2, "\tcxt pc is different: " PFX "\n",
                    cxt->CXT_XIP);
            }
        });

#ifdef HOT_PATCHING_INTERFACE
        /* Recover from a hot patch exception. */
        if (dcontext != NULL && dcontext->whereami == DR_WHERE_HOTPATCH) {
            /* Note: If we use a separate stack for executing hot patches, this
             * assert should be changed.
             */
            ASSERT(is_on_dstack(dcontext, (byte *)cxt->CXT_XSP));
            if (is_on_dstack(dcontext, (byte *)cxt->CXT_XSP)) {
                char excpt_addr[16];
                snprintf(excpt_addr, BUFFER_SIZE_ELEMENTS(excpt_addr), PFX,
                         (byte *)cxt->CXT_XIP);
                NULL_TERMINATE_BUFFER(excpt_addr);

                /* Forensics for this event are dumped in hotp_execute_patch()
                 * because only that has vulnerability specific information.
                 */
                SYSLOG_CUSTOM_NOTIFY(SYSLOG_ERROR, MSG_HOT_PATCH_FAILURE, 3,
                                     "Hot patch exception, continuing.",
                                     get_application_name(), get_application_pid(),
                                     excpt_addr);
                if (TEST(DUMPCORE_HOTP_FAILURE, DYNAMO_OPTION(dumpcore_mask)))
                    os_dump_core("hotp exception");

                /* we don't support filters, so a single pass through
                 * all FINALLY and EXCEPT handlers is sufficient
                 */

                /* The exception interception code did an ENTER so we must EXIT here */
                EXITING_DR();
                DR_LONGJMP(&dcontext->hotp_excpt_state, LONGJMP_EXCEPTION);
            }
            /* Else, if it is on init stack, the control flow below would
             * report an internal error.
             */
        }
#endif

        if (is_safe_read_pc((app_pc)cxt->CXT_XIP) ||
            (dcontext != NULL && dcontext->try_except.try_except_state != NULL) ||
            (!dynamo_initialized && global_try_except.try_except_state != NULL)) {
            /* handle our own TRY/EXCEPT */
            /* similar to hotpatch exceptions above */

            /* XXX: syslog is just too noisy for clients, esp those like
             * Dr. Memory who routinely examine random app memory and
             * use dr_safe_read() and other mechanisms.
             */

            if (TEST(DUMPCORE_TRY_EXCEPT, DYNAMO_OPTION(dumpcore_mask)))
                os_dump_core("try/except fault");

            /* The exception interception code did an ENTER so we must EXIT here */
            EXITING_DR();
            if (is_safe_read_pc((app_pc)cxt->CXT_XIP)) {
                cxt->CXT_XIP = (ptr_uint_t)safe_read_resume_pc();
                nt_continue(cxt);
            } else {
                try_except_context_t *try_cxt = (dcontext != NULL)
                    ? dcontext->try_except.try_except_state
                    : global_try_except.try_except_state;
                ASSERT(try_cxt != NULL);
                DR_LONGJMP(&try_cxt->context, LONGJMP_EXCEPTION);
            }
            ASSERT_NOT_REACHED();
        }
        ASSERT(dcontext != NULL); /* NULL cases handled above */

        /* We dump info after try/except to avoid rank order violation (i#) */
        DOLOG(2, LOG_ASYNCH, {
            dump_exception_info(pExcptRec, cxt);
            dump_exception_frames(); /* check what handlers are installed */
        });
        DOLOG(2, LOG_ASYNCH, {
            /* verify attack handling assumptions on valid frames */
            if (IF_X64_ELSE(is_wow64_process(NT_CURRENT_PROCESS), true) &&
                dcontext != NULL)
                exception_frame_chain_depth(dcontext);
        });

        if (CLIENTS_EXIST() && is_in_client_lib(pExcptRec->ExceptionAddress)) {
            /* i#1354: client might fault touching a code page we made read-only.
             * If so, just re-execute post-page-prot-change (MOD_CODE_APP_CXT), if
             * it's safe to do so (we document these criteria under
             * DR_MEMPROT_PRETEND_WRITE).
             */
            if (pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
                pExcptRec->NumberParameters >= 2 &&
                pExcptRec->ExceptionInformation[0] == EXCEPTION_INFORMATION_WRITE_FAULT &&
                !is_couldbelinking(dcontext) && OWN_NO_LOCKS(dcontext)) {
                /* won't return if it was a made-read-only code page */
                check_for_modified_code(dcontext, pExcptRec, cxt, MOD_CODE_APP_CXT, NULL);
            }
            internal_dynamo_exception(dcontext, pExcptRec, cxt, true);
            os_terminate(dcontext, TERMINATE_PROCESS);
            ASSERT_NOT_REACHED();
        }

        /* If we set a thread's context after it received a fault but
         * before the kernel copied the faulting context to the user
         * mode structures for the handler, we can come here and think
         * it faulted at the pc we set its context to (case 7393).
         * ASSUMPTION: we will not actually fault at any of these addresses.
         * We could set a flag in the dcontext before we setcontext and
         * clear it afterward to reduce the mis-diagnosis chance.
         */
        if ((app_pc)pExcptRec->ExceptionAddress == get_reset_exit_stub(dcontext)) {
            ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
            /* Restore to faulting address, which is an app address.
             * Thus we skip the translation steps below.
             * We must do our own modified code check as well, and since the original
             * cache is gone (so saving the address wouldn't help) we need custom
             * handling along that path.
             */
            ASSERT(!in_fcache(dcontext->next_tag));
            pExcptRec->ExceptionAddress = (PVOID)dcontext->next_tag;
            cxt->CXT_XIP = (ptr_uint_t)dcontext->next_tag;
            STATS_INC(num_reset_setcontext_at_fault);
            SYSLOG_INTERNAL_WARNING("reset SetContext at faulting instruction");
            check_for_modified_code(dcontext, pExcptRec, cxt,
                                    MOD_CODE_TAKEOVER | MOD_CODE_APP_CXT, NULL);
            /* now handle the fault just like RaiseException */
            DODEBUG({ known_source = true; });
        } else if ((app_pc)pExcptRec->ExceptionAddress == get_setcontext_interceptor()) {
            ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
            /* Restore to faulting address, no need to go to our interception routine
             * as natively we'd have the fault and go from there.
             */
            /* FIXME case 7456: we don't have original fault address so we
             * can't properly process a codemod fault!  Need to have SetContext
             * pre-handler store that somewhere.  Here we need new code paths to
             * do the codemod handling but then go to the SetContext target
             * rather than re-execute the write.  NOT IMPLEMENTED!
             */
            ASSERT_NOT_IMPLEMENTED(false && "app SetContext on faulting instr");
            STATS_INC(num_app_setcontext_at_fault);
            pExcptRec->ExceptionAddress = (PVOID)dcontext->asynch_target;
            cxt->CXT_XIP = (ptr_uint_t)dcontext->asynch_target;
            /* now handle the fault just like RaiseException */
            DODEBUG({ known_source = true; });
        }

        check_internal_exception(dcontext, cxt, pExcptRec, forged_exception_addr,
                                 &raw_mcontext);

        /* we do not call trace_abort() here since we may need to
         * translate from a temp private bb (i#376): but all paths
         * that do not return to the faulting instr will call it
         */

        /* FIXME: we may want to distinguish the exception that we
         * have generated from interp from potential races that will
         * result in this really being generated in the fcache. */
        /* FIXME: there is no reason a native_exec thread can't natively
         * generate (and even handle) one of these exceptions. Of course,
         * judging by the need for the filter, doesn't even look like it needs
         * to be native. */
        /* Do not assert when a client is present: it may be using
         * ud2 or something for its own purposes (i#503).  This
         * curiosity is really to find errors in core DR.
         */
        ASSERT_CURIOSITY(dr_bb_hook_exists() || dr_trace_hook_exists() ||
                         pExcptRec->ExceptionCode != STATUS_ILLEGAL_INSTRUCTION ||
                         check_filter("common.decode-bad.exe;common.decode.exe;"
                                      "security-common.decode-bad-stack.exe;"
                                      "security-win32.gbop-test.exe",
                                      get_short_name(get_application_name())));
        ASSERT_CURIOSITY(pExcptRec->ExceptionCode != STATUS_PRIVILEGED_INSTRUCTION ||
                         check_filter("common.decode.exe;common.decode-bad.exe",
                                      get_short_name(get_application_name())));

        /* if !takeover, the thread could be native and not in fcache */
        if (!takeover || in_fcache((void *)(pExcptRec->ExceptionAddress))) {
            recreate_success_t res;
            fragment_t *f = NULL;
            fragment_t wrapper;
            /* cache the fragment since pclookup is expensive for coarse (i#658) */
            f = fragment_pclookup(dcontext, pExcptRec->ExceptionAddress, &wrapper);
            /* special case: we expect a seg fault for executable regions
             * that were writable and marked read-only by us.
             * if it is modified code, routine won't return to us
             * (it takes care of d_r_initstack though).
             * checking for modified code shouldn't be done for thin_client.
             * note: hotp_only plays around with memory protection to smash
             *       hooks, so can't be ignore; vlad: in -client mode we
             *       smash hooks using emulate_write
             */
            if (!DYNAMO_OPTION(thin_client)) {
                check_for_modified_code(dcontext, pExcptRec, cxt,
                                        takeover ? MOD_CODE_TAKEOVER : 0, f);
            }
            if (!takeover) {
                /* -probe_api client should get exception events too */
                if (CLIENTS_EXIST()) {
                    /* raw_mcontext equals mcontext */
                    context_to_mcontext(&raw_mcontext, cxt);
                    client_exception_event(dcontext, cxt, pExcptRec, &raw_mcontext, f);
                }
#ifdef PROGRAM_SHEPHERDING
                /* check for an ASLR execution violation - currently
                 * should only be hit in hotp_only, but could also be
                 * in native_exec threads
                 * FIXME: can we tell hotp from native if we wanted to?
                 */
                /* FIXME: Currently we only track execution faults in
                 * unreadable memory.  If we allow mapping of data,
                 * e.g. !ASLR_RESERVE_AREAS then should track any
                 * other exception, ILLEGAL_INSTRUCTION_EXCEPTION or
                 * PRIVILEGED_INSTRUCTION or anything else with an
                 * ExceptionAddress in that region.  However, can't do
                 * the same if other DLLs can map there,
                 * e.g. !(ASLR_AVOID_AREAS|ASLR_RESERVE_AREAS).
                 */
                if (DYNAMO_OPTION(aslr) != ASLR_DISABLED &&
                    (pExcptRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) &&
                    is_execution_exception(pExcptRec)) {
                    app_pc execution_addr = pExcptRec->ExceptionAddress;
                    if (aslr_is_possible_attack(execution_addr) &&
                        /* ignore if we have just forged_exception_addr */
                        execution_addr != forged_exception_addr) {
                        security_option_t handling_policy = OPTION_BLOCK;

                        LOG(THREAD, LOG_ASYNCH, 1,
                            "Exception at " PFX " is due "
                            "to randomization, under attack!\n",
                            execution_addr);
                        SYSLOG_INTERNAL_ERROR("ASLR: execution attempt " PFX " "
                                              "in preferred DLL range\n",
                                              execution_addr);

                        if (TEST(ASLR_HANDLING, DYNAMO_OPTION(aslr_action)))
                            handling_policy |= OPTION_HANDLING;

                        if (TEST(ASLR_REPORT, DYNAMO_OPTION(aslr_action)))
                            handling_policy |= OPTION_REPORT;

                        /* for reporting purposes copy application
                         * context in our context, we don't need our
                         * priv_mcontext_t at all otherwise
                         */
                        context_to_mcontext(get_mcontext(dcontext), cxt);
                        aslr_report_violation(execution_addr, handling_policy);
                        /* should normally return so we pass the
                         * exception to the application, unless we
                         * want to forcefully handle it
                         */
                        ASSERT(!TEST(OPTION_HANDLING, handling_policy) &&
                               "doesn't return");
                    }
                }
#endif /* PROGRAM_SHEPHERDING */

                /* Note - temporarily lost control threads (UNDER_DYN_HACK) are
                 * whereami == DR_WHERE_FCACHE (FIXME would be more logical to be
                 * DR_WHERE_APP) and !takeover, but unlike the forge case we don't
                 * need to fix them up here. */
                if (!thread_is_lost) {
                    /* xref 8267, can't just check that exception addr matches
                     * forged addr because that can falsely match at 0, so
                     * we base on whereami instead. */
                    if (dcontext->whereami == DR_WHERE_FCACHE) {
                        /* Xref case 8219 - forge exception sets whereami to
                         * DR_WHERE_FCACHE while we perform the RaiseException.  Need
                         * to set the whereami back to DR_WHERE_APP now since not
                         * taking over. */
                        ASSERT_CURIOSITY(pExcptRec->ExceptionAddress ==
                                         forged_exception_addr);
#ifdef PROGRAM_SHEPHERDING
                        /* only known to happen on throw exception */
                        ASSERT_CURIOSITY(DYNAMO_OPTION(throw_exception));
#else
                        ASSERT_CURIOSITY(false && "should not be reached");
#endif
                        dcontext->whereami = DR_WHERE_APP;
                    } else {
                        /* should already be DR_WHERE_APP then */
                        ASSERT_CURIOSITY(dcontext->whereami == DR_WHERE_APP);
                        /* this should not be a forged exception */
                        ASSERT_CURIOSITY(pExcptRec->ExceptionAddress !=
                                             forged_exception_addr || /* 8267 */
                                         forged_exception_addr == NULL);
                    }
                }

                /* wasn't our fault, let it go back to app */
                check_app_stack_limit(dcontext);
                report_app_exception(dcontext, APPFAULT_FAULT, pExcptRec, cxt,
                                     "Exception occurred in native application code.");
#ifdef PROTECT_FROM_APP
                SELF_PROTECT_LOCAL(dcontext, READONLY);
#endif
                return AFTER_INTERCEPT_LET_GO;
            }

            LOG(THREAD, LOG_ASYNCH, 1, "Exception is in code cache\n");
            ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
            DOLOG(2, LOG_ASYNCH, {
                LOG(THREAD, LOG_ASYNCH, 2, "Exception is in this fragment:\n");
                /* We might not find the fragment since if it is shared and
                 * pending deletion for flush, may have already been removed
                 * from the lookup tables */
                if (f != NULL)
                    disassemble_fragment(dcontext, f, false);
                else
                    LOG(THREAD, LOG_ASYNCH, 2, "Fragment not found");
            });
            /* Need to fix EXCEPTION_RECORD's pc and CONTEXT's registers
             * to pretend it was original code, not cache code
             */
            /* remember faulting pc */
            faulting_pc = (cache_pc)pExcptRec->ExceptionAddress;
            if (CLIENTS_EXIST()) {
                /* i#182/PR 449996: we provide the pre-translation context */
                context_to_mcontext(&raw_mcontext, cxt);
            }
            /* For safe recreation we need to either be couldbelinking or hold the
             * initexit lock (to keep someone from flushing current fragment), the
             * initexit lock is easier
             */
            d_r_mutex_lock(&thread_initexit_lock);
            if (cxt->CXT_XIP != (ptr_uint_t)pExcptRec->ExceptionAddress) {
                /* sometimes this happens, certainly cxt can be changed by exception
                 * handlers, but not clear why kernel changes it before getting here.
                 * I saw it pointing to next instr while ExceptionAddress was faulting
                 * instr...who knows, in any case we translate them both, but do
                 * exceptionaddress first since cxt is the one we want for real, we
                 * just want pc for exceptionaddress.
                 */
                app_pc translated_pc;
                if (pExcptRec->ExceptionCode == EXCEPTION_BREAKPOINT &&
                    cxt->CXT_XIP + 1 == (ptr_uint_t)pExcptRec->ExceptionAddress) {
                    /* i#2126 : In case of an int 2d, the exception address is
                     * increased by 1 and we make the same.
                     */
                    translated_pc =
                        recreate_app_pc(dcontext, (cache_pc)cxt->CXT_XIP, f) + 1;
                } else {
                    translated_pc =
                        recreate_app_pc(dcontext, pExcptRec->ExceptionAddress, f);
                }
                ASSERT(translated_pc != NULL);
                LOG(THREAD, LOG_ASYNCH, 2,
                    "Translated ExceptionAddress " PFX " to " PFX "\n",
                    pExcptRec->ExceptionAddress, translated_pc);
                pExcptRec->ExceptionAddress = (PVOID)translated_pc;
            }
            context_to_mcontext(&mcontext, cxt);
            res = recreate_app_state(dcontext, &mcontext, true /*memory too*/, f);
            if (res != RECREATE_SUCCESS_STATE) {
                /* We don't expect to get here: means an exception from an
                 * instruction we added.  FIXME: today we do have some
                 * translation pieces that are NYI: selfmod(PR 267764),
                 * native_exec or windows sysenter (PR 303413), a flushed
                 * fragment (PR 208037), or a hot patch fragment (PR
                 * 213251: we can get here if a hot patched bb excepts
                 * because of app code and the hot patch was applied at the
                 * first instr; this is because the translation_target for
                 * that will be wrong and recreating app state will get
                 * messed up.  However, it will end up with the right
                 * state.  See case 5981.)
                 */
                SYSLOG_INTERNAL_WARNING("Unable to fully translate context for "
                                        "exception in the cache");
                /* we should always at least get pc right */
                ASSERT(res == RECREATE_SUCCESS_PC);
            }
            d_r_mutex_unlock(&thread_initexit_lock);
            if (cxt->CXT_XIP == (ptr_uint_t)pExcptRec->ExceptionAddress)
                pExcptRec->ExceptionAddress = (PVOID)mcontext.pc;
#ifdef X64
            {
                /* PR 520001: the kernel places an extra copy of the fault addr
                 * in the 16-byte-aligned slot just above pExcptRec.  This copy
                 * is used as a sanity check by the SEH64 code, so we must
                 * translate it as well.
                 */
                app_pc *extra_addr =
                    (app_pc *)(((app_pc)pExcptRec) + sizeof(*pExcptRec) + 8);
                ASSERT_CURIOSITY(ALIGNED(extra_addr, 16));
                /* Since I can't find any docs or other refs to this data I'm being
                 * conservative and only replacing if it matches the fault addr
                 */
                if (*extra_addr == (app_pc)cxt->CXT_XIP) {
                    LOG(THREAD, LOG_ASYNCH, 2,
                        "Translated extra addr slot " PFX " to " PFX "\n", *extra_addr,
                        mcontext.pc);
                    *extra_addr = mcontext.pc;
                } else
                    ASSERT_CURIOSITY(false && "extra SEH64 addr not found");
            }
#endif
            LOG(THREAD, LOG_ASYNCH, 2, "Translated cxt->Xip " PFX " to " PFX "\n",
                cxt->CXT_XIP, mcontext.pc);

            /*
             * i#2144 : we check if this is a single step exception
             * where we diverted the address.
             */
            if (pExcptRec->ExceptionCode == EXCEPTION_SINGLE_STEP) {
                instr_t instr;

                instr_init(dcontext, &instr);
                decode(dcontext, faulting_pc, &instr);
                /* Checks that exception was generated by a nop. */
                if (instr_get_opcode(&instr) == OP_nop) {
                    instr_reset(dcontext, &instr);
                    decode(dcontext, pExcptRec->ExceptionAddress, &instr);
                    /* Checks that exception address translate on a popf. */
                    if (instr_get_opcode(&instr) == OP_popf ||
                        instr_get_opcode(&instr) == OP_iret) {
                        /* Will continue after one byte popf or iret. */
                        if (instr_get_opcode(&instr) == OP_popf) {
                            dcontext->next_tag = mcontext.pc + POPF_LENGTH;
                        } else {
                            /* We get the return address which was popped into ecx. */
                            dcontext->next_tag = (app_pc)cxt->CXT_XCX;
                        }
                        /* Deletes fragment starting at single step exception
                         * if it exists.
                         */
                        flush_fragments_from_region(
                            dcontext, dcontext->next_tag, 1 /* size */,
                            false /*don't force synchall*/,
                            NULL /*flush_completion_callback*/, NULL /*user_data*/);
                        /* Sets a field so that build_bb_ilist knows when to stop. */
                        dcontext->single_step_addr = dcontext->next_tag;
                        LOG(THREAD, LOG_ASYNCH, 2,
                            "Caught generated single step exception at " PFX " to " PFX
                            "\n",
                            pExcptRec->ExceptionAddress, dcontext->next_tag);
                        /* Will return to execute instruction
                         * where single step exception will be forged.
                         */
                        dcontext->whereami = DR_WHERE_FCACHE;
                        set_last_exit(dcontext, (linkstub_t *)get_asynch_linkstub());
                        if (instr_get_opcode(&instr) == OP_iret) {
                            /* Emulating the rest of iret which is just a pop into rsp
                             * for x64 and nothing for x86.
                             */
#ifdef X64
                            /* Emulates iret's pop rsp. */
                            if (!dr_safe_read((byte *)mcontext.xsp, XSP_SZ, &mcontext.xsp,
                                              NULL)) {
                                /* FIXME i#2144 : handle if the pop rsp fails */
                                ASSERT_NOT_IMPLEMENTED(false);
                            }
                            /* FIXME i#2144 : handle if pop into ss faults. */
#endif
                        }
                        instr_free(dcontext, &instr);
                        transfer_to_dispatch(dcontext, &mcontext,
                                             false /*!full_DR_state*/);
                        ASSERT_NOT_REACHED();
                    }
                }
                instr_free(dcontext, &instr);
            }

            /* cxt came from the kernel, so it should already have ss and cs
             * initialized. Thus there's no need to get them again.
             */
            mcontext_to_context(cxt, &mcontext, false /* !set_cur_seg */);

            /* PR 306410: if exception while on dstack but going to app,
             * copy SEH frame over to app stack and update handler xsp.
             */
            if (is_on_dstack(dcontext, fault_xsp)) {
                size_t frame_sz = sizeof(CONTEXT) + sizeof(EXCEPTION_RECORD);
                bool frame_copied;
                IF_NOT_X64(frame_sz += XSP_SZ * 2 /* 2 args */);
                ASSERT(!is_on_dstack(dcontext, (byte *)cxt->CXT_XSP));
                frame_copied = safe_write((byte *)cxt->CXT_XSP - frame_sz, frame_sz,
                                          (byte *)state->mc.xsp);
                LOG(THREAD, LOG_ASYNCH, 2,
                    "exception on dstack; copied %d-byte SEH frame from " PFX
                    " to app stack " PFX "\n",
                    frame_sz, state->mc.xsp, cxt->CXT_XSP - frame_sz);
                state->mc.xsp = cxt->CXT_XSP - frame_sz;
#ifndef X64
                /* update pointers */
                *((byte **)state->mc.xsp) = (byte *)state->mc.xsp + 2 * XSP_SZ;
                *((byte **)(state->mc.xsp + XSP_SZ)) =
                    (byte *)state->mc.xsp + 2 * XSP_SZ + sizeof(EXCEPTION_RECORD);
#endif
                /* x64 KiUserExceptionDispatcher does not take any args */
                if (!frame_copied) {
                    SYSLOG_INTERNAL_WARNING("Unable to copy on-dstack app SEH frame "
                                            "to app stack");
                    /* FIXME: terminate app?  forge exception (though that does
                     * a getcontext right here)?  can this be just a guard page? */
                    ASSERT_NOT_REACHED();
                }
            }

            /* we interpret init and other pieces of our own dll */
            if (is_dynamo_address(mcontext.pc)) {
                SYSLOG_INTERNAL_CRITICAL("Exception in cache " PFX " "
                                         "interpreting DR code " PFX,
                                         faulting_pc, mcontext.pc);
                /* to avoid confusion over whether the original DR pc was
                 * native or in the cache we hack the exception flags
                 */
                pExcptRec->ExceptionFlags = 0xbadcad;
                internal_dynamo_exception(dcontext, pExcptRec, cxt, false);
                ASSERT_NOT_REACHED();
            }

            /* Inform client of exceptions */
            if (CLIENTS_EXIST()) {
                client_exception_event(dcontext, cxt, pExcptRec, &raw_mcontext, f);
            }
        } else {
            /* If the exception pc is not in the fcache, then the exception
             * was generated by calling RaiseException, or it's one of the
             * SetContext cases up above.
             * RaiseException calls RtlRaiseException, and the return address
             * of that call site becomes the exception pc
             * RtlRaiseException stores all registers, eflags, and segment
             * registers somewhere (presumably a CONTEXT struct), and then
             * calls NtRaiseException, which does int 0x2e
             * User mode is re-entered in the exception handler with
             * a pc equal to the return address from way up the call stack
             *
             * Now, the question is, how do we make sure the CONTEXT is ok?
             * It's only not ok if we've optimized stuff around the call to
             * RaiseException, right?
             * FIXME: should we simpy not optimize there?  Else we need
             * a special translator that knows to look back there, rather
             * than at the exception pc (== after call to RtlRaiseException)
             *
             * Also, note that exceptions we generate via os_forge_exception
             * end up here as well, they need no translation.
             */
            DOLOG(1, LOG_ASYNCH, {
                if (!known_source) {
                    LOG(THREAD, LOG_ASYNCH, 1,
                        "Exception was generated by call to RaiseException\n");
                }
            });
            /* Inform client of forged exceptions (i#1775) */
            if (CLIENTS_EXIST()) {
                /* raw_mcontext equals mcontext */
                context_to_mcontext(&raw_mcontext, cxt);
                client_exception_event(dcontext, cxt, pExcptRec, &raw_mcontext, NULL);
            }
        }

        /* Fixme : we could do this higher up in the function (before
         * translation) but then wouldn't be able to separate out the case
         * of faulting interpreted dynamo address above. Prob. is nicer
         * to have the final translation available in the dump anyways.*/
        report_app_exception(dcontext, APPFAULT_FAULT, pExcptRec, cxt,
                             "Exception occurred in application code.");
        /* We won't get here for UNDER_DYN_HACK since at the top of the routine
         * we set takeover to false for that case.  FIXME - if we clean up the
         * above if and the check/found/handle modified code paths to handle
         * UNDER_DYN_HACK correctly then we can use this as a retakeover point
         */
        asynch_retakeover_if_native();
        /* We want to squash the current trace (don't want traces
         * following exceptions), asynch_take_over does that for us.
         * We don't save the cur dcontext.
         */
        state->callee_arg = (void *)false /* use cur dcontext */;
        instrument_dispatcher(dcontext, DR_XFER_EXCEPTION_DISPATCHER, state, cxt);
        asynch_take_over(state);
    } else
        STATS_INC(num_exceptions_noasynch);
    return AFTER_INTERCEPT_LET_GO;
}

/* We have yet to ever see this!
 * Disassembly reveals that all it does is call RtlRaiseException,
 * which will dive right back into the kernel and re-appear in
 * KiUserExceptionDispatcher.
 * We intercept this simply for completeness, to run ALL the user mode
 * code.  We do nothing special, just take over.
 *
KiRaiseUserExceptionDispatcher:
  77FA0384: 50                 push        eax
  77FA0385: 55                 push        ebp
  77FA0386: 8B EC              mov         ebp,esp
  77FA0388: 83 EC 50           sub         esp,50h
  77FA038B: 89 44 24 0C        mov         dword ptr [esp+0Ch],eax
  77FA038F: 64 A1 18 00 00 00  mov         eax,fs:[00000018]
  77FA0395: 8B 80 A4 01 00 00  mov         eax,dword ptr [eax+000001A4h]
  77FA039B: 89 04 24           mov         dword ptr [esp],eax
  77FA039E: C7 44 24 04 00 00  mov         dword ptr [esp+4],0
            00 00
  77FA03A6: C7 44 24 08 00 00  mov         dword ptr [esp+8],0
            00 00
  77FA03AE: C7 44 24 10 00 00  mov         dword ptr [esp+10h],0
            00 00
  77FA03B6: 54                 push        esp
  77FA03B7: E8 5C 0B 01 00     call        77FB0F18  <RtlRaiseException>
  77FA03BC: 8B 04 24           mov         eax,dword ptr [esp]
  77FA03BF: 8B E5              mov         esp,ebp
  77FA03C1: 5D                 pop         ebp
  77FA03C2: C3                 ret
 *
 * ASSUMPTIONS: none
 */
/* Remember that every path out of here must invoke the DR exit hook.
 * The normal return path will do so as the interception code has an
 * enter and exit hook around the call to this routine.
 */
static after_intercept_action_t /* note return value will be ignored */
intercept_raise_exception(app_state_at_intercept_t *state)
{
    ASSERT_NOT_TESTED();
    if (intercept_asynch_for_self(false /*no unknown threads*/)) {
        SELF_PROTECT_LOCAL(get_thread_private_dcontext(), WRITABLE);
        /* won't be re-protected until d_r_dispatch->fcache */

        LOG(THREAD_GET, LOG_ASYNCH, 1, "ASYNCH intercept_raise_exception()\n");
        STATS_INC(num_raise_exceptions);

        asynch_retakeover_if_native();
        /* We want to squash the current trace (don't want traces
         * following exceptions), asynch_take_over does that for us.
         * We don't save the cur dcontext.
         */
        state->callee_arg = (void *)false /* use cur dcontext */;
        instrument_dispatcher(get_thread_private_dcontext(), DR_XFER_RAISE_DISPATCHER,
                              state, NULL);
        asynch_take_over(state);
    } else
        STATS_INC(num_raise_exceptions_noasynch);
    return AFTER_INTERCEPT_LET_GO;
}

/* creates an exception record for a forged exception */
/* Access violations on read:
   ff 12 call dword ptr [edx]
        PC 0x00401124 tried to read address 0x00baddaa
   Access violations on execution:
   ff d2 call edx
        PC 0xdeadbeef tried to read address 0xdeadbeef
*/
static void
initialize_exception_record(EXCEPTION_RECORD *rec, app_pc exception_address,
                            uint exception_code)
{
    rec->ExceptionFlags = 0;
    rec->ExceptionRecord = NULL;
    rec->ExceptionAddress = exception_address;
    rec->NumberParameters = 0;
    switch (exception_code) {
    case UNREADABLE_MEMORY_EXECUTION_EXCEPTION:
        rec->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = 0 /* read/execute */;
        rec->ExceptionInformation[1] = (ptr_uint_t)exception_address;
        break;
    case IN_PAGE_ERROR_EXCEPTION:
        rec->ExceptionCode = EXCEPTION_IN_PAGE_ERROR;
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = 0 /* read/execute */;
        rec->ExceptionInformation[1] = (ptr_uint_t)exception_address;
        break;
    case ILLEGAL_INSTRUCTION_EXCEPTION:
        rec->ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    case GUARD_PAGE_EXCEPTION:
        rec->ExceptionCode = STATUS_GUARD_PAGE_VIOLATION;
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = EXCEPTION_EXECUTE_FAULT /* execution tried */;
        rec->ExceptionInformation[1] = (ptr_uint_t)exception_address;
        break;
    case SINGLE_STEP_EXCEPTION:
        rec->ExceptionCode = EXCEPTION_SINGLE_STEP;
        rec->NumberParameters = 0;
        break;
    default: ASSERT_NOT_REACHED();
    }
}

/* forge an exception (much like calling RaiseException)
   1) we fill in the CONTEXT and EXCEPTION_RECORD structure
      with proper context
   2) then pass along to the kernel for delivery
      - switch to application stack
      - call ZwRaiseException(excrec, context, true)
        -- kernel handler (pseudocode in Nebbett p.439)
           (not clear how that pseudocode propagates the context of ZwRaiseException
           see  CONTEXT context = {CONTEXT_FULL | CONTEXT_DEBUG};
           if (valid user stack with enough space)
             copy EXCEPTION_RECORD and CONTEXT to user mode stack
             push &EXCEPTION_RECORD
             push &CONTEXT
        -- will return to user mode in intercept_exception (KiUserExceptionDispatcher)

  An ALTERNATE less transparent step 2) would be:
  push on the user stack the EXCEPTION_POINTERS (pretending to be the kernel)
  and then get to our own intercept_exception to pass this to the application
*/
void
os_forge_exception(app_pc exception_address, dr_exception_type_t exception_type)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    EXCEPTION_RECORD excrec;
    int res;
    /* In order to match the native exception we need a really full context  */
    CONTEXT context;
    context.ContextFlags = CONTEXT_FULL | CONTEXT_FLOATING_POINT |
        IF_X64_ELSE(0 /*doesn't exist*/, CONTEXT_EXTENDED_REGISTERS) |
        CONTEXT_DEBUG_REGISTERS;
    /* keep in mind the above structure is 716 bytes */

    LOG(THREAD, LOG_ASYNCH, 1, "ASYNCH os_forge_exception(type " PFX " addr " PFX ")\n",
        exception_type, exception_address);

    initialize_exception_record(&excrec, exception_address, exception_type);
    dcontext->forged_exception_addr = exception_address;

    /* we first get full context, and then convert it using saved app context */
    /* FIXME Rather than having a thread call get_context() on itself, should we
     * assemble the context data ourself? This would get us around any handle
     * permission problem.
     * FIXME: MSDN says that GetThreadContext for the current thread returns
     * an invalid context: so should use GET_OWN_CONTEXT, if it's sufficient.
     */
    res = nt_get_context(NT_CURRENT_THREAD, &context);
    ASSERT(NT_SUCCESS(res));
    DOLOG(2, LOG_ASYNCH, {
        LOG(THREAD, LOG_ASYNCH, 2, "ASYNCH context before remapping\n");
        dump_exception_info(&excrec, &context);
    });

    /* get application context */
    /* context is initialized via nt_get_context, which should initialize
     * cs and ss, so there is no nead to get them again.
     */
    mcontext_to_context(&context, get_mcontext(dcontext), false /* !set_cur_seg */);
    context.CXT_XIP = (ptr_uint_t)exception_address;

    DOLOG(2, LOG_ASYNCH, {
        LOG(THREAD, LOG_ASYNCH, 2, "\nASYNCH context after remapping\n");
        dump_exception_info(&excrec, &context);
    });

    /* FIXME: App Context issues. */
    /* For some uses of forge_exception we expect registers to differ from
     * native since we forge the exception at the start of the basic block we
     * think will cause the exception (FIXME).  But even when that is not the
     * case the eflags register still sometimes differs from native for unknown
     * reasons, in my tests with the decode_prefixes unit test the resume
     * flags would be set natively, but not under us and the parity flags
     * would be set under us but not natively. FIXME
     */

    /* FIXME : We might want to use nt_raise_exception here instead of
     * os_raise_exception, then we could get rid of that function & issue_last
     * _system_call_from_app.  Also if we call this too early we might
     * not know the syscall method yet, in which case we could screw up the
     * args in os_raise_exception.  Which leads to the next problem that if the
     * syscall fails (can happen if args are bad) then nt_raise_exception will
     * return to us but os_raise_exception will return in to random app memory
     * for xp/2003 and into our global syscall buffer on 2000 (which will
     * prob. fault).  nt_raise_exception also allows the possibility for
     * recovering  (though we currently assert).  As far as os_transparency
     * goes, nt_raise_exception will have the right return addresses,
     * (thought shouldn't matter to the OS).  os_raise_exception has the
     * advantage of going through our cache entering routines before generating
     * the exception so will reach our exception handler with internal dynamo
     * state more similar to the app exception we are trying to forge.  This
     * is especially relevant for self-protection and using nt_raise_exception
     * would entail getting dynamo state into an appropriate configuration for
     * receiving and app exception.
     */
    os_raise_exception(dcontext, &excrec, &context);
    ASSERT_NOT_REACHED();
}

/****************************************************************************
 * CALLBACKS
 *
 * Callbacks: start with kernel entering user mode in KiUserCallbackDispatcher,
 * which this routine intercepts.  They end with an "int 2b" instruction
 * (the ones I see go through 0x77e163c1 in ntdll.dll on win2000), if they
 * come back here they will also hit the int 2b seen below.
 * N.B.: This "int 2b" ending is my theory, I can find no documentation on it!
 * UPDATE: Inside Win2K book confirms that int 2b maps to NtCallbackReturn
 *
KiUserCallbackDispatcher:
  77F9F038: 83 C4 04           add         esp,4
  77F9F03B: 5A                 pop         edx
  77F9F03C: 64 A1 18 00 00 00  mov         eax,fs:[00000018]
  77F9F042: 8B 40 30           mov         eax,dword ptr [eax+30h]
  77F9F045: 8B 40 2C           mov         eax,dword ptr [eax+2Ch]
  77F9F048: FF 14 90           call        dword ptr [eax+edx*4]
  77F9F04B: 33 C9              xor         ecx,ecx
  77F9F04D: 33 D2              xor         edx,edx
  77F9F04F: CD 2B              int         2Bh
  77F9F051: CC                 int         3
  77F9F052: 8B FF              mov         edi,edi
 *
 * 2003 SP1 has an explicit call to ZwCallbackReturn instead of the int2b:
KiUserCallbackDispatcher:
  7c836330 648b0d00000000   mov     ecx,fs:[00000000]
  7c836337 ba1063837c       mov     edx,0x7c836310 (KiUserCallbackExceptionHandler)
  7c83633c 8d442410         lea     eax,[esp+0x10]
  7c836340 894c2410         mov     [esp+0x10],ecx
  7c836344 89542414         mov     [esp+0x14],edx
  7c836348 64a300000000     mov     fs:[00000000],eax
  7c83634e 83c404           add     esp,0x4
  7c836351 5a               pop     edx
  7c836352 64a130000000     mov     eax,fs:[00000030]
  7c836358 8b402c           mov     eax,[eax+0x2c]
  7c83635b ff1490           call    dword ptr [eax+edx*4]
  7c83635e 50               push    eax
  7c83635f 6a00             push    0x0
  7c836361 6a00             push    0x0
  7c836363 e85d27ffff       call    ntdll!ZwCallbackReturn (7c828ac5)
  7c836368 8bf0             mov     esi,eax
  7c83636a 56               push    esi
  7c83636b e828010000       call    ntdll!RtlRaiseStatus (7c836498)
  7c836370 ebf8             jmp  ntdll!KiUserCallbackDispatcher+0x3a (7c83636a)
  7c836372 c20c00           ret     0xc
 *
  x64 xp:
  ntdll!KiUserCallbackDispatch:
  00000000`77ef3160 488b4c2420      mov     rcx,qword ptr [rsp+20h]
  00000000`77ef3165 8b542428        mov     edx,dword ptr [rsp+28h]
  00000000`77ef3169 448b44242c      mov     r8d,dword ptr [rsp+2Ch]
  00000000`77ef316e 65488b042560000000 mov   rax,qword ptr gs:[60h]
  00000000`77ef3177 4c8b4858        mov     r9,qword ptr [rax+58h]
  00000000`77ef317b 43ff14c1        call    qword ptr [r9+r8*8]
  ntdll!KiUserCallbackDispatcherContinue:
  00000000`77ef317f 33c9            xor     ecx,ecx
  00000000`77ef3181 33d2            xor     edx,edx
  00000000`77ef3183 448bc0          mov     r8d,eax
  00000000`77ef3186 e8a5d8ffff      call    ntdll!ZwCallbackReturn (00000000`77ef0a30)
  00000000`77ef318b 8bf0            mov     esi,eax
  00000000`77ef318d 8bce            mov     ecx,esi
  00000000`77ef318f e85cf50500      call    ntdll!RtlRaiseException+0x118 (0`77f526f0)
  00000000`77ef3194 cc              int     3
  00000000`77ef3195 eb01            jmp     ntdll!KiUserCallbackDispatcherContinue+0x15
                                            (00000000`77ef3198)
  00000000`77ef3197 90              nop
  00000000`77ef3198 ebf7            jmp     ntdll!KiUserCallbackDispatcherContinue+0x12
                                           (00000000`77ef3191)
  00000000`77ef319a cc              int     3
 *
 * ASSUMPTIONS:
 *    1) peb->KernelCallbackTable[*(esp+IF_X64_ELSE(0x2c,4))] ==
 *       call* target (used for debug only)
 */
/* Remember that every path out of here must invoke the DR exit hook.
 * The normal return path will do so as the interception code has an
 * enter and exit hook around the call to this routine.
 */
static after_intercept_action_t /* note return value will be ignored */
intercept_callback_start(app_state_at_intercept_t *state)
{
    /* We only hook this in thin_client mode to be able to read the DRmarker. */
    if (DYNAMO_OPTION(thin_client))
        return AFTER_INTERCEPT_LET_GO;

    if (intercept_callbacks && intercept_asynch_for_self(false /*no unknown threads*/)) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        /* should not receive callback while in DR code! */
        if (is_on_dstack(dcontext, (byte *)state->mc.xsp)) {
            CLIENT_ASSERT(false,
                          "Received callback while in tool code!"
                          "Please avoid making alertable syscalls from tool code.");
            /* We assume a callback received on the dstack is an error from a client
             * invoking an alertable syscall.  Safest to let it run natively.
             */
            return AFTER_INTERCEPT_LET_GO;
        }
        SELF_PROTECT_LOCAL(dcontext, WRITABLE);
        /* won't be re-protected until d_r_dispatch->fcache */
        ASSERT(is_thread_initialized());
        ASSERT(dcontext->whereami == DR_WHERE_FCACHE);
        DODEBUG({
            /* get callback target address
             * we want ((fs:0x18):0x30):0x2c => TEB->PEB->KernelCallbackTable
             * on x64 it's the same table ((gs:60):0x58), but the index comes
             * from rsp+0x2c.
             */
            app_pc target = NULL;
            app_pc *cbtable = (app_pc *)get_own_peb()->KernelCallbackTable;
            if (cbtable != NULL) {
                target = cbtable[*(uint *)(state->mc.xsp + IF_X64_ELSE(0x2c, 4))];
                LOG(THREAD_GET, LOG_ASYNCH, 2,
                    "ASYNCH intercepted callback #%d: target=" PFX ", thread=" TIDFMT
                    "\n",
                    GLOBAL_STAT(num_callbacks) + 1, target, d_r_get_thread_id());
                DOLOG(3, LOG_ASYNCH,
                      { dump_mcontext(&state->mc, THREAD_GET, DUMP_NOT_XML); });
            }
        });
        /* FIXME: we should be able to strongly assert that only
         * non-ignorable syscalls should be allowed to get here - all
         * ignorable one's shouldn't be interrupted for callbacks.  We
         * also start syscall_fcache at callback return yet another
         * callback can be delivered.
         */

#if 0  /* disabled b/c not fully flushed out yet, may be useful in future */
        /* attempt to find where thread was suspended for callback by walking
         * stack backward, finding last ret addr before KiUserCallbackDispatcher,
         * and walking forward looking for the syscall that triggered callback.
         * This method is fragile, since ind branches could be in the middle...
         */
        uint *pc = (uint *) state->mc.xbp;
        DOLOG(2, LOG_ASYNCH, {
            dump_callstack(NULL, (app_pc) state->mc.xbp, THREAD_GET, DUMP_NOT_XML);
        });
        while (pc != NULL && is_readable_without_exception((byte *)pc, 8)) {
            if (*(pc+1) == (ptr_uint_t)after_callback_orig_pc ||
                *(pc+1) == (ptr_uint_t)after_apc_orig_pc) {
                uint *stop; /* one past ret for call to syscall wrapper */
                if (*(pc+1) == (ptr_uint_t)after_callback_orig_pc) {
                    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                        "\tafter_callback_orig_pc == "PFX", pointed to @"PFX"\n",
                        after_callback_orig_pc, pc);
                    stop = pc + 29;
                } else {
                    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                        "\tafter_apc_orig_pc == "PFX", pointed to @"PFX"\n",
                        after_apc_orig_pc, pc);
                    stop = pc + 1000;
                }
                while (pc < stop) {
                    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                        "\t*"PFX" = "PFX"\n", pc, *pc);
                    if (*pc != NULL &&
                        is_readable_without_exception(((byte*)(*pc))-5, 5)) {
                        byte *nxt = ((byte*)(*pc))-5;
                        while (nxt < (byte *)(*pc)) {
                            nxt = disassemble_with_bytes(cur_dcontext, nxt,
                                                         cur_dcontext->logfile);
                        }
                    }
                    pc++;
                }
                if (*stop != NULL && is_readable_without_exception(*stop, 4)) {
                    byte *cpc = (byte *) *(pc-1);
                    instr_t instr;
                    instr_init(dcontext, &instr);
                    /* Have to decode previous call...tough to do, how
                     * tell call* from call?
                    e8 f3 cd ff ff       call   $0x77f4386b
                    ff 15 20 11 f4 77    call   0x77f41120
                      can you have call 0x?(?,?,?) such that last 5 bytes
                      look like direct call?
                    ff 14 90             call   (%eax,%edx,4)
                    ff 55 08             call   0x8(%ebp)
                    ff 56 5c             call   0x5c(%esi)
                    ff d3                call   %ebx
                    */
                    cpc -= 5;
                    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                        "\tdecoding "PFX"\n", cpc);
                    cpc = decode_cti(cur_dcontext, cpc, &instr);
                    if (instr_opcode_valid(&instr) &&
                        instr_is_call(&instr) &&
                        cpc == (byte *) *(pc-1)) {
                        LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                            "\tafter instr is "PFX"\n", cpc);
                        cpc = opnd_get_pc(instr_get_target(&instr));
                        LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
                            "\ttarget is "PFX"\n", cpc);
                        /* now if win2k, target will be int 2e, then 3-byte ret
                         * how insert trampoline into 3-byte ret??!?!
                         *   suspend all other threads?
                         * know where it's going -- back to after call, could try for
                         * trampoline there, but what if buffer overflow?
                         * don't want native ret to be executed!
                         */
                    } else {
                        ASSERT(false && "Bad guess of direct near call");
                    }
                }
            }
            pc = (uint *) *pc;
        }
#endif /* if 0: find callback interruption point */

        RSTATS_INC(num_callbacks);

        asynch_retakeover_if_native();
        state->callee_arg = (void *)(ptr_uint_t) true /* save cur dcontext */;
        instrument_dispatcher(dcontext, DR_XFER_CALLBACK_DISPATCHER, state, NULL);
        asynch_take_over(state);
    } else
        STATS_INC(num_callbacks_noasynch);
    return AFTER_INTERCEPT_LET_GO;
}

/* NtCallbackReturn: the other way (other than int 2b) to return from
 * a callback
 *
 * Like NtContinue, NtCallbackReturn is a system call that is entered
 * from user mode, but when control emerges from the kernel it does
 * not resume after the syscall point.
 * Note that sometimes an indirect call (for example, at 77E25A5A in user32.dll)
 * targets NtCallbackReturn
 *
 * Is it possible to get at target pc?
 * I tried looking at various points on the stack:
 *   intercept_callback_return: thread=520, target=0x03f10705
 *   intercept_callback_return: thread=520, target=0x77e25a60
 *   intercept_callback_return: thread=520, target=0x00000000
 *   intercept_callback_return: thread=520, target=0x00000000
 *   intercept_callback_return: thread=520, target=0x00000000
 *   intercept_callback_return: thread=520, target=0x77f9f04b
 *   intercept_callback_return: thread=520, target=0x040bff98
 * First 2 are proc call returns, that 77f9f04b is inside CallbackDispatcher!
 * Sounds very promising but our after callback interception would have caught
 * anything that went there...why is that on stack?  Could be proc call return
 * address for callback call...
 * Final stack value I thought might be pointer to CONTEXT on stack, but
 * no.
 * Resolution: impossible, just like int 2b
 */
/*
NtCallbackReturn:
  77F83103: B8 13 00 00 00     mov         eax,13h
  77F83108: 8D 54 24 04        lea         edx,[esp+4]
  77F8310C: CD 2E              int         2Eh
  77F8310E: E9 FA 57 01 00     jmp         77F9890D
  77F83113: B8 93 00 00 00     mov         eax,93h
  77F83118: 8D 54 24 04        lea         edx,[esp+4]
  77F8311C: CD 2E              int         2Eh
  77F8311E: C2 14 00           ret         14h
*/

/**************************************************
 * Dealing with dcontext stack for callbacks
 * Since the kernel maintains a stack of contexts for callbacks that we
 * cannot read, we must keep our state for when the kernel unwinds the stack
 * and sends control back.  To do that, we have a stack of dcontexts.
 */

/* Called by asynch_take_over to initialize the dcontext structure for
 * the current thread and return a ptr to it.  The parameter "next_pc"
 * indicates where the Dynamo interpreter should begin.
 */
static dcontext_t *
callback_setup(app_pc next_pc)
{
    dcontext_t *old_dcontext;
    dcontext_t *new_dcontext;
    dcontext_t *dc;

    old_dcontext = get_thread_private_dcontext();
    ASSERT(old_dcontext);

    if (!old_dcontext->initialized) {
        /* new threads are created via APC, so they come in here
         * uninitialized -- we could not create new dcontext and use old
         * one, then have to check for that in callback_start_return,
         * instead we simply initialize old one:
         */
        initialize_dynamo_context(old_dcontext);
    }

    /* if we were building a trace, kill it */
    if (is_building_trace(old_dcontext)) {
        LOG(old_dcontext->logfile, LOG_ASYNCH, 2,
            "callback_setup: squashing old trace\n");
        trace_abort(old_dcontext);
    }

    /* kill any outstanding pointers -- once we switch to the new dcontext,
     * they will not be cleared if we delete their target, and we'll have
     * problems when we return to old dcontext and deref them!
     */
    set_last_exit(old_dcontext, (linkstub_t *)get_asynch_linkstub());
#ifdef PROFILE_RDTSC
    old_dcontext->prev_fragment = NULL;
#endif

    /* need to save old dcontext and get new dcontext for callback execution */

    /* must always use same dcontext because fragment code is hardwired
     * for it, so we use stack of dcontexts to save old ones.
     * what we do here: find or create new dcontext for use with callback.
     * initialize new dcontext.  then swap the new and old dcontexts!
     */
    dc = old_dcontext;
    /* go to end of valid (==saved) contexts */
    while (dc->prev_unused != NULL && dc->prev_unused->valid)
        dc = dc->prev_unused;
    if (INTERNAL_OPTION(stress_detach_with_stacked_callbacks) && dc != old_dcontext &&
        dc != old_dcontext->prev_unused && dc != old_dcontext->prev_unused->prev_unused) {
        /* internal stress testing of detach (once app has multiple stacked callbacks) */
        DO_ONCE(detach_internal(););
    }
    if (dc->prev_unused != NULL) {
        new_dcontext = dc->prev_unused;
        ASSERT(!new_dcontext->valid);
    } else {
        /* need to make a new dcontext */
        /* FIXME: how do we know we're not getting a new callback
           while in a system call for allocating more memory?
           This routine should be organized so that we spend minimal time
           setting up a new context and then we should be able to handle a new one.
           We need a per-thread flag/counter saying are we handling a callback stack.
           Not that we can do anything about it when we come here again and the
           flag is set, but at least we can STAT such occurrences.

           Actually that should only happen if we call an alertable system call
           (see end of bug 326 )...we should see if we're calling any.and deal with it,
           otherwise if we're safe with respect to callbacks then remove the
           above comment.
        */
        new_dcontext = create_callback_dcontext(old_dcontext);
        /* stick at end of list */
        dc->prev_unused = new_dcontext;
        new_dcontext->prev_unused = NULL;
    }
    LOG(old_dcontext->logfile, LOG_ASYNCH, 2, "\tsaving prev dcontext @" PFX "\n",
        new_dcontext);

    DOLOG(4, LOG_ASYNCH, {
        LOG(old_dcontext->logfile, LOG_ASYNCH, 4,
            "old dcontext " PFX " w/ next_tag " PFX ":\n", old_dcontext,
            old_dcontext->next_tag);
        dump_mcontext(get_mcontext(old_dcontext), old_dcontext->logfile, DUMP_NOT_XML);
        LOG(old_dcontext->logfile, LOG_ASYNCH, 4,
            "new dcontext " PFX " w/ next_tag " PFX ":\n", new_dcontext,
            new_dcontext->next_tag);
        dump_mcontext(get_mcontext(new_dcontext), old_dcontext->logfile, DUMP_NOT_XML);
    });

    /* i#985: save TEB fields into old context via double swap */
    ASSERT(os_using_app_state(old_dcontext));
    swap_peb_pointer(old_dcontext, true /*to priv*/);
    swap_peb_pointer(old_dcontext, false /*to app*/);

    /* now swap new and old */
    swap_dcontexts(new_dcontext, old_dcontext);
    /* saved and current dcontext should both be valid */
    new_dcontext->valid = true;
    old_dcontext->valid = true;

    /* now prepare to use new dcontext, pointed to by old_dcontext ptr */
    initialize_dynamo_context(old_dcontext);
    old_dcontext->whereami = DR_WHERE_TRAMPOLINE;
    old_dcontext->next_tag = next_pc;
    ASSERT(old_dcontext->next_tag != NULL);
    return old_dcontext;
}

/* Called when a callback has completed execution and is about to return
 * (either at an int 2b, or an NtCallbackReturn system call).
 * Note that we restore the old dcontext now and then execute the rest
 * of the current fragment that's been using the new dcontext, so we
 * rely on the interrupt instruction being the NEXT INSTRUCTION after this call
 * (& call cleanup)!
 * Arguments: pusha right before call -> pretend getting all regs as args
 * Do not need them as args!  You can pass me anything!
 */
void
callback_start_return(priv_mcontext_t *mc)
{
    dcontext_t *cur_dcontext;
    dcontext_t *prev_dcontext;

    if (!intercept_callbacks || !intercept_asynch_for_self(false /*no unknown threads*/))
        return;

    /* both paths here, int 2b and syscall, go back through d_r_dispatch,
     * so self-protection has already been taken care of -- we don't come
     * here straight from cache!
     */

    cur_dcontext = get_thread_private_dcontext();
    ASSERT(cur_dcontext && cur_dcontext->initialized);

    /* if we were building a trace, kill it */
    if (is_building_trace(cur_dcontext)) {
        LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
            "callback_start_return: squashing old trace\n");
        trace_abort(cur_dcontext);
    }

    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
        "ASYNCH callback_start_return(): restoring previous dcontext\n");
    DOLOG(3, LOG_ASYNCH, {
        dump_mcontext(mc, cur_dcontext->logfile, DUMP_NOT_XML);
        if (mc->xbp != 0) {
            dump_callstack(NULL, (app_pc)mc->xbp, cur_dcontext->logfile, DUMP_NOT_XML);
        }
    });

    /* Must always use same dcontext because fragment code is hardwired
     * for it, so we use stack of dcontexts to save old ones.
     * what we do here: find the last dcontext that holds saved values.
     * then swap the new and old dcontexts!
     */
    prev_dcontext = cur_dcontext;
    /* go to end of valid (==saved) contexts */
    while (prev_dcontext->prev_unused != NULL && prev_dcontext->prev_unused->valid)
        prev_dcontext = prev_dcontext->prev_unused;

    if (prev_dcontext == cur_dcontext) {
        /* There's no prior dcontext! */
        /* If this is a callback return, the kernel will send control to a native context!
         * We see this when we take control in the middle of a callback that's
         * in the middle of the init APC, which is used to initialize the main thread.
         */
        thread_record_t *tr = thread_lookup(d_r_get_thread_id());
        /* we may end up losing control, so use this to signal as a hack,
         * if we intercept any asynchs we'll report an error if we see UNDER_DYN_HACK
         */
        tr->under_dynamo_control = UNDER_DYN_HACK;

        /* We assume this is during the init APC.
         * We try to regain control by adding a trampoline at the
         * image entry point -- we ignore race conditions here b/c there's only
         * one thread (not always true [injected], but the races are minimal).
         * FIXME: kernel32's base process start point is actually where
         * control appears, but it's not exported.  It only executes a dozen or
         * so instructions before calling the image entry point, but it would
         * be nice if we had a way to find it -- could get it from the
         * initialization apc context. */
        /* We rely on -native_exec_syscalls to retake control before any app
         * state changes dr needs to see occur. This lets us handle injected
         * threads (or the strange case of a statically linked dll creating a
         * thread in its dllmain). If not using native_exec_syscalls then we
         * can't maintain the executable list, retakeover will have to flush
         * and re-walk the list, and we won't be safe with injected threads.
         */
        if (!DYNAMO_OPTION(native_exec_syscalls)) {
            /* we need to restore changed memory protections because we won't
             * be intercepting system calls to fix things up */
            /* not multi-thread safe */
            ASSERT(check_sole_thread() && d_r_get_num_threads() == 1);
            revert_memory_regions();
        }

        if (INTERNAL_OPTION(hook_image_entry)) {
            /* potentially racy hook (injected threads) */
            insert_image_entry_trampoline(cur_dcontext);
        }

        DODEBUG({
            /* we should never see this after we have reached the image entry point */
            if (reached_image_entry_yet()) {
                /* Nothing we can do -- except try to walk stack frame back and
                 * hope to decode exactly where thread was suspended, but
                 * there's no guarantee we can do that if there were indirect
                 * jmps.
                 */
                SYSLOG_INTERNAL_ERROR("non-process-init callback return with native "
                                      "callback context for %s thread " TIDFMT "",
                                      (tr == NULL) ? "unknown" : "known",
                                      d_r_get_thread_id());
                /* might be injected late, refer to bug 426 for discussion
                 * of instance here where that was the case */
                ASSERT_NOT_REACHED();
            }
        });
        return;
    }

    LOG(cur_dcontext->logfile, LOG_ASYNCH, 2,
        "\trestoring previous dcontext saved @" PFX "\n", prev_dcontext);

    DOLOG(4, LOG_ASYNCH, {
        LOG(cur_dcontext->logfile, LOG_ASYNCH, 4,
            "current dcontext " PFX " w/ next_tag " PFX ":\n", cur_dcontext,
            cur_dcontext->next_tag);
        dump_mcontext(get_mcontext(cur_dcontext), cur_dcontext->logfile, DUMP_NOT_XML);
        LOG(cur_dcontext->logfile, LOG_ASYNCH, 4,
            "prev dcontext " PFX " w/ next_tag " PFX ":\n", prev_dcontext,
            prev_dcontext->next_tag);
        dump_mcontext(get_mcontext(prev_dcontext), cur_dcontext->logfile, DUMP_NOT_XML);
    });

    /* now swap cur and prev
     * N.B.: callback return brings up a tricky dual-dcontext problem, where
     * we need the cur dcontext to restore to native state right before interrupt
     * (after this routine returns), but we need to restore to prev dcontext now
     * since we won't get another chance -- we solve the problem by assuming we
     * never deallocate dstack of prev dcontext and letting the clean call restore
     * from the stack, plus using a special unswapped_scratch slot in dcontext
     * to restore app esp properly to native value.
     * for self-protection, we solve the problem by using a custom fcache_enter
     * routine that restores native state from the prev dcontext.
     */
    swap_dcontexts(prev_dcontext, cur_dcontext);
    /* invalidate prev */
    prev_dcontext->valid = false;

    DOLOG(5, LOG_ASYNCH, {
        LOG(cur_dcontext->logfile, LOG_ASYNCH, 4,
            "after swap, current dcontext " PFX " w/ next_tag " PFX ":\n", cur_dcontext,
            cur_dcontext->next_tag);
        dump_mcontext(get_mcontext(cur_dcontext), cur_dcontext->logfile, DUMP_NOT_XML);
    });

    priv_mcontext_t *cur_mc = get_mcontext(cur_dcontext);
    /* XXX: if the retaddr is in the xsi slot, how do we get back the orig xsi value?
     * Presumably we can't: should we document this?
     */
    cur_mc->pc = POST_SYSCALL_PC(cur_dcontext);
    get_mcontext(prev_dcontext)->pc = prev_dcontext->next_tag;
    /* We don't support changing the target context for cbret. */
    instrument_kernel_xfer(cur_dcontext, DR_XFER_CALLBACK_RETURN, NULL, NULL,
                           get_mcontext(prev_dcontext), cur_mc->pc, cur_mc->xsp, NULL,
                           cur_mc, 0);
}

/* Returns the prev dcontext that was just swapped by callback_start_return */
dcontext_t *
get_prev_swapped_dcontext(dcontext_t *dcontext)
{
    dcontext_t *prev = dcontext;
    /* find first invalid dcontext */
    while (prev->prev_unused != NULL && prev->valid)
        prev = prev->prev_unused;
    return prev;
}

/****************************************************************************
 * MISC
 */

/* finds the pc after the call to the callback routine in
 * KiUserCallbackDispatcher or KiUserApcDispatcher, useful for examining
 * call stacks (the pc after the call shows up on the call stack)
 * For callbacks:
 *   on Win2k Workstation should get: 0x77F9F04F
 *   on Win2K Server: 0x77F9FB83
 * Will also look beyond the call for the 1st cti/int, and if an int2b
 * or a call to NtCallbackReturn, sets cbret to that pc.
 */
byte *
get_pc_after_call(byte *entry, byte **cbret)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    byte *pc = entry;
    byte *after_call = NULL;
    instr_t instr;
    int num_instrs = 0;
    if (dcontext == NULL)
        dcontext = GLOBAL_DCONTEXT;

    /* find call to callback */
    instr_init(dcontext, &instr);
    do {
        instr_reset(dcontext, &instr);
        pc = decode_cti(dcontext, pc, &instr);
        ASSERT(pc != NULL);
        num_instrs++;
        /* win8.1 x86 call* is 13th instr, win10 1703 is 16th */
        ASSERT_CURIOSITY(num_instrs <= 18);
        if (instr_opcode_valid(&instr)) {
            if (instr_is_call_indirect(&instr)) {
                /* i#1599: Win8.1 has an extra call that we have to rule out:
                 * 77ce0c9e ff15d031da77  call  dword ptr [ntdll!__guard_check_icall_fptr]
                 * 77ce0ca4 ffd1          call  ecx
                 */
                opnd_t tgt = instr_get_target(&instr);
                if (opnd_is_base_disp(tgt) && opnd_get_base(tgt) == REG_NULL)
                    continue;
            }
            /* Skip the LdrDelegatedKiUserApcDispatcher, etc. prefixes on 1703 */
            if (get_os_version() >= WINDOWS_VERSION_10_1703 &&
                (instr_get_opcode(&instr) == OP_jmp_ind || instr_is_cbr(&instr)))
                continue;
            break; /* don't expect any other decode_cti instrs */
        }
    } while (true);
    after_call = pc;

    /* find next cti, see if it's an int 2b or a call to ZwCallbackReturn */
    if (cbret != NULL) {
        *cbret = NULL;
        do {
            instr_reset(dcontext, &instr);
            pc = decode_cti(dcontext, pc, &instr);
            ASSERT_CURIOSITY(pc != NULL);
            num_instrs++;
            ASSERT_CURIOSITY(num_instrs <= 32); /* case 3522. */
            if (instr_opcode_valid(&instr)) {
                if (instr_is_interrupt(&instr)) {
                    int num = instr_get_interrupt_number(&instr);
                    if (num == 0x2b) {
                        LOG(THREAD_GET, LOG_ASYNCH, 2,
                            "after dispatcher found int 2b @" PFX "\n", pc);
                        *cbret = pc;
                    }
                } else if (instr_is_call_direct(&instr)) {
                    GET_NTDLL(NtCallbackReturn,
                              (IN PVOID Result OPTIONAL, IN ULONG ResultLength,
                               IN NTSTATUS Status));
                    if (opnd_get_pc(instr_get_target(&instr)) ==
                        (app_pc)NtCallbackReturn) {
                        LOG(THREAD_GET, LOG_ASYNCH, 2,
                            "after dispatcher found call to NtCallbackReturn @" PFX "\n",
                            pc);
                        *cbret = pc;
                    }
                }
            }
        } while (!instr_opcode_valid(&instr));
    }
    instr_free(dcontext, &instr);
    LOG(THREAD_GET, LOG_ASYNCH, 2, "after dispatcher pc is: " PFX "\n", after_call);
    return after_call;
}

/****************************************************************************/
/* It's possible to load w/o going through the LdrLoad routines (by
 * using MapViewOfSectino), so we only use these trampolines for debugging, and
 * also for the AppInit injection in order to regain control at an earlier point
 * than waiting for the main image entry point (and, another thread is sometimes
 * scheduled before the main thread gets to its entry point!)
 * We used to use this hook for our do not load list, but we now do that at
 * MapViewOfSection to avoid needing this hook, as it tends to conflict with
 * 3rd-party hooks (i#1663).
 */
GET_NTDLL(LdrLoadDll,
          (IN PWSTR DllPath OPTIONAL, IN PULONG DllCharacteristics OPTIONAL,
           IN PUNICODE_STRING DllName, OUT PVOID *DllHandle));
GET_NTDLL(LdrUnloadDll, (IN PVOID DllHandle));

/* i#1663: since we rarely need these 2 hooks, and they are the most likely
 * of our hooks to conflict with an app's hooks, we avoid placing them
 * if we don't need them.
 */
static bool
should_intercept_LdrLoadDll(void)
{
#ifdef GBOP
    if (DYNAMO_OPTION(gbop) != GBOP_DISABLED)
        return true;
#endif
    return DYNAMO_OPTION(hook_ldr_dll_routines);
}

static bool
should_intercept_LdrUnloadDll(void)
{
    if (DYNAMO_OPTION(svchost_timeout) > 0 && get_os_version() <= WINDOWS_VERSION_2000)
        return true;
    return DYNAMO_OPTION(hook_ldr_dll_routines);
}

after_intercept_action_t
intercept_load_dll(app_state_at_intercept_t *state)
{
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    /* grab args to original routine */
    wchar_t *path = (wchar_t *)APP_PARAM(&state->mc, 0);
    uint *characteristics = (uint *)APP_PARAM(&state->mc, 1);
    UNICODE_STRING *name = (UNICODE_STRING *)APP_PARAM(&state->mc, 2);
    HMODULE *out_handle = (HMODULE *)APP_PARAM(&state->mc, 3);

    LOG(GLOBAL, LOG_VMAREAS, 1, "intercept_load_dll: %S\n", name->Buffer);
    LOG(GLOBAL, LOG_VMAREAS, 2, "\tpath=%S\n",
        /* win8 LdrLoadDll seems to take small integers instead of paths */
        ((ptr_int_t)path <= (ptr_int_t)PAGE_SIZE) ? L"NULL" : path);
    LOG(GLOBAL, LOG_VMAREAS, 2, "\tcharacteristics=%d\n",
        characteristics ? *characteristics : 0);
    ASSERT(should_intercept_LdrLoadDll());

#ifdef GBOP
    if (DYNAMO_OPTION(gbop) != GBOP_DISABLED) {
        /* FIXME: case 7127: currently doesn't obey
         * -exclude_gbop_list, which should set a flag.
         */
        gbop_validate_and_act(state, 0 /* no ESP offset - at entry point */, load_dll_pc);
        /* if GBOP validation at all returns it accepted the source */
        /* FIXME: case 7127: may want alternative handling */
    }
#endif /* GBOP */

    if (tr == NULL) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "WARNING: native thread in intercept_load_dll\n");
        if (control_all_threads) {
            SYSLOG_INTERNAL_ERROR("LdrLoadDll reached by unexpected %s thread " TIDFMT "",
                                  (tr == NULL) ? "unknown" : "known",
                                  d_r_get_thread_id());
            /* case 9385 tracks an instance */
            ASSERT_CURIOSITY(false);
        }
        return AFTER_INTERCEPT_LET_GO;
    } else if (control_all_threads && IS_UNDER_DYN_HACK(tr->under_dynamo_control)) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        /* trying to open debugbox causes IIS to fail, so don't SYSLOG_INTERNAL */
        LOG(THREAD, LOG_ASYNCH, 1,
            "ERROR: load_dll: we lost control of thread " TIDFMT "\n", tr->id);
        DOLOG(2, LOG_ASYNCH,
              { dump_callstack(NULL, (app_pc)state->mc.xbp, THREAD, DUMP_NOT_XML); });
        retakeover_after_native(tr, INTERCEPT_LOAD_DLL);
        /* we want to take over, but we'll do that anyway on return from this routine
         * since our interception code was told to take over afterward, so we wait.
         * this routine executes natively so even if we were already in control
         * we want to take over so no conditional is needed there.
         */
    } else if (!intercept_asynch_for_self(false /*no unknown threads*/)) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "WARNING: no-asynch thread loading a dll\n");
        return AFTER_INTERCEPT_LET_GO;
    } else {
        /* unnecessary trampoline exit when we were in full control */
        /* LdrLoadDll will be executed as a normal fragment */
    }

    /* we're taking over afterward so we need local writability */
    SELF_PROTECT_LOCAL(get_thread_private_dcontext(), WRITABLE);
    /* won't be re-protected until d_r_dispatch->fcache */

#ifdef DEBUG
    if (get_thread_private_dcontext() != NULL)
        LOG(THREAD_GET, LOG_VMAREAS, 1, "intercept_load_dll: %S\n", name->Buffer);
    DOLOG(3, LOG_VMAREAS, { print_modules(GLOBAL, DUMP_NOT_XML); });
#endif
    return AFTER_INTERCEPT_TAKE_OVER;
}

/* used for log messages in normal operation
   and also needed for the svchost_timeout hack */
after_intercept_action_t
intercept_unload_dll(app_state_at_intercept_t *state)
{
    /* grab arg to original routine */
    HMODULE h = (HMODULE)APP_PARAM(&state->mc, 0);
    static int in_svchost = -1; /* unknown yet */
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    ASSERT(should_intercept_LdrUnloadDll());

    if (tr == NULL) {
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "WARNING: native thread in "
            "intercept_unload_dll\n");
        if (control_all_threads) {
            SYSLOG_INTERNAL_ERROR(
                "LdrUnloadDll reached by unexpected %s thread " TIDFMT "",
                (tr == NULL) ? "unknown" : "known", d_r_get_thread_id());
            /* case 9385 tracks an instance */
            ASSERT_CURIOSITY(false);
        }
        return AFTER_INTERCEPT_LET_GO;
    } else if (!IS_UNDER_DYN_HACK(tr->under_dynamo_control) &&
               !intercept_asynch_for_self(false /*no unknown threads*/)) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "WARNING: no-asynch thread unloading a dll\n");
        return AFTER_INTERCEPT_LET_GO;
    }

    if (in_svchost && DYNAMO_OPTION(svchost_timeout) > 0 &&
        /* case 10509: avoid the timeout on platforms where we haven't seen problems */
        get_os_version() <= WINDOWS_VERSION_2000) {
        /* ENTERING GROSS HACK AREA, case 374 */
#define HACK_EXE_NAME SVCHOST_EXE_NAME "-netsvcs"
#define L_PIN_DLL_NAME L"wzcsvc.dll"
        /* The unload order is wzcsvc.dll, WINSTA.dll, CRYPT32.dll, MSASN1.DLL,
           so waiting on the first one should be sufficient */
        /* We don't want to let this dll be unloaded immediately before it
         * handles some callbacks, so we need to add a time out to the
         * unloading thread.  It looks like we're too late to pin it in memory
         * so our substitute for
         * WinXP::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN) didn't work.
         */

        if (in_svchost < 0) { /* unknown yet */
            /* NOTE: the hack is needed only for the bug in svchost-netsvcs,
               seems to be an issue only on Win2k SP4
               if that grouping changes this may become a problem again.
            */
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            in_svchost =
                strcasecmp(HACK_EXE_NAME, get_short_name(get_application_name())) == 0;
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
            LOG(GLOBAL, LOG_VMAREAS, 3,
                "intercept_unload_dll: svchost_timeout "
                "hack_name=%s app_name=%s in_svchost=%d\n",
                HACK_EXE_NAME, get_short_name(get_application_name()), in_svchost);
        }

        if (in_svchost) {
            extern LDR_MODULE *get_ldr_module_by_pc(app_pc pc); /* in module.c */

            /* FIXME: we're not holding the loader lock here
               we may want a deeper interception point,
               or maybe we should just as well grab the lock */
            LDR_MODULE *mod = get_ldr_module_by_pc((app_pc)h);

            if (mod && (wcscasecmp(L_PIN_DLL_NAME, mod->BaseDllName.Buffer) == 0)) {
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "intercept_unload_dll: "
                    "svchost_timeout found target app_name=%s dll_name=%ls\n",
                    HACK_EXE_NAME, mod->BaseDllName.Buffer);

                SYSLOG_INTERNAL_WARNING("WARNING: svchost timeout in progress");
                /* let the events get delivered */
                os_timeout(dynamo_options.svchost_timeout);

                /* This event can happen any time explorer.exe is restarted so
                 * we stay alert
                 */
            }
        }
    } /* EXITING GROSS HACK */

    /* we're taking over afterward so we need local writability */
    SELF_PROTECT_LOCAL(get_thread_private_dcontext(), WRITABLE);
    /* won't be re-protected until d_r_dispatch->fcache */

    DOLOG(1, LOG_VMAREAS, {
        char buf[MAXIMUM_PATH];
        /* assumption: h is base address! */
        size_t size = get_allocation_size((byte *)h, NULL);
        get_module_name((app_pc)h, buf, sizeof(buf));

        if (buf[0] != '\0') {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "intercept_unload_dll: %s @" PFX " size " PIFX "\n", buf, h, size);
        } else {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "intercept_unload_dll: <unknown> @" PFX " size " PIFX "\n", h, size);
        }
        if (get_thread_private_dcontext() != NULL) {
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "intercept_unload_dll: %s @" PFX " "
                "size " PIFX "\n",
                buf, h, size);
        }
        DOLOG(3, LOG_VMAREAS, { print_modules(GLOBAL, DUMP_NOT_XML); });
    });
    /* we do not flush fragments here b/c this call only decrements
     * the reference count, we wait until the library is actually
     * unloaded in the syscall NtUnmapViewOfSection
     * (there are many unload calls that do not end up unmapping)
     */
    if (control_all_threads && IS_UNDER_DYN_HACK(tr->under_dynamo_control))
        retakeover_after_native(tr, INTERCEPT_UNLOAD_DLL);
    return AFTER_INTERCEPT_TAKE_OVER;
}

/****************************************************************************/

void
retakeover_after_native(thread_record_t *tr, retakeover_point_t where)
{
    ASSERT(IS_UNDER_DYN_HACK(tr->under_dynamo_control) || tr->retakeover ||
           dr_injected_secondary_thread);
    tr->under_dynamo_control = true;

    /* Only one thread needs to do the rest of this, and we don't need to
     * block the rest as A) a thread hitting the hook before we remove it should
     * be a nop and B) the hook removal itself should be thread-safe.
     */
    if (!d_r_mutex_trylock(&intercept_hook_lock))
        return;
    /* Check whether another thread already did this and already unlocked the lock.
     * We can also later re-insert the image entry hook if we lose control on cbret.
     */
    if (image_entry_trampoline == NULL) {
        d_r_mutex_unlock(&intercept_hook_lock);
        return;
    }

    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    interception_point = where;

    if (INTERNAL_OPTION(hook_image_entry) && image_entry_trampoline != NULL) {
        /* remove the image entry trampoline */
        /* ensure we didn't take over and forget to call this routine -- we shouldn't
         * get here via interpreting, only natively, so no fragment should exist
         */
        ASSERT(image_entry_pc != NULL &&
               fragment_lookup(tr->dcontext, image_entry_pc) == NULL);
        /* potentially slightly racy with injected threads */
        remove_image_entry_trampoline();
    }

    STATS_INC(num_retakeover_after_native);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    LOG(GLOBAL, LOG_VMAREAS, 1, "\n*** re-taking-over @%s after losing control ***\n",
        retakeover_names[where]);

    /* we don't need to rescan even if we had injected in secondary
     * thread, since we don't expect anything interesting to have been
     * done by the primary thread: all initialization should have been
     * done by the secondary thread, so it should just go to the entry
     * point.
     */
    if (!DYNAMO_OPTION(native_exec_syscalls)) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        /* ensure we're (still) the only thread! */
        /* xref case 3053 where that is not always the case */
        /* FIXME: we may need to suspend all other threads or grab a lock
         * to make sure other threads aren't blocked before modifying
         * protection that everything we're doing here is still safe
         */
        ASSERT_CURIOSITY(check_sole_thread() && d_r_get_num_threads() == 1);
        DOSTATS({ ASSERT_CURIOSITY(GLOBAL_STAT(num_threads_created) == 1); });
        /* If we weren't watching memory alloc/dealloc while the thread was
         * native we have to redo our exec list completely here.
         * FIXME: To avoid races w/ other threads (which until we have early
         * injection we will continue to see: CTRL_SHUTDOWN and other injected
         * threads (xref case 4181)) it would be best to hold the executable_areas
         * lock across the removal and re-add, but that would require retooling
         * find_executable_vm_areas().  So we don't bother to do it now since
         * native_exec_syscalls is on by default.
         */
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "re-walking executable regions after native execution period\n");
        /* need to re-walk exec areas since may have changed
         * while app was native
         */
        /* first clear the executable list, FIXME : do we need to clear the
         * futurexec list too? */
        flush_fragments_and_remove_region(
            dcontext, UNIVERSAL_REGION_BASE, UNIVERSAL_REGION_SIZE,
            false /* don't own initexit_lock */, true /* remove futures */);

        /* need to clean any existing regions */
        DOLOG(SYMBOLS_LOGLEVEL, LOG_SYMBOLS, { module_cleanup(); });
        modules_reset_list();

#if defined(RCT_IND_BRANCH) || defined(RETURN_AFTER_CALL)
        /* case 9926: we invalidate to avoid stale targets: but
         * (case 10518) modules_reset_list() removed all the rct and rac tables for us;
         * besides, invalidate_{ind_branch,after_call}_target_range doesn't
         * support cross-module ranges
         */
#endif

        find_executable_vm_areas();
        /* N.B.: this is duplicated from callback_interception_init() because
         * after we takeover from native executable_areas is built again to
         * handle the case of the app loading or unloadeing any modules while
         * we didn't have control.  We shouldn't need this with
         * native_exec_syscalls turned on, but need it if not.
         * FIXME: for every special exec region marked during init, must duplicate
         * marking here!  how ensure that?  today we have two such regions.
         */
        add_executable_region(
            interception_code,
            INTERCEPTION_CODE_SIZE _IF_DEBUG("heap mmap callback interception code"));
        landing_pads_to_executable_areas(true /* add */);
        LOG(GLOBAL, LOG_VMAREAS, 1, "after re-walking, executable regions are:\n");
        DOLOG(1, LOG_VMAREAS, { print_executable_areas(GLOBAL); });
    }
    d_r_mutex_unlock(&intercept_hook_lock);
}

void
remove_image_entry_trampoline()
{
    /* we don't assert it's non-NULL b/c we want to support partial native exec modes */
    if (image_entry_trampoline != NULL)
        remove_trampoline(image_entry_trampoline, image_entry_pc);
    image_entry_trampoline = NULL;
    /* FIXME: should set image_entry_trampoline unless we have multiple calls */
}

/* early inject and user32 should reach this, drinject /
 * late inject should never get here, and make sure we have
 * dr_late_injected_primary_thread = true set for that. Assumption: we
 * assume that with our method of injection, the primary thread will
 * not be allowed to execute when the secondary thread is in control,
 * e.g. holds the loader lock.  Any other (tertiary) threads that have
 * reached their APC before us will be left running as 'unknown'.
 * We are also assuming that only the primary thread will
 * ever go through the EXE's image entry.
 */
void
take_over_primary_thread(void)
{
    /* note that if we have initialized this thread we already have the value,
     * but making this more generic in case we move it,
     */
    app_pc win32_start_addr = 0;
    NTSTATUS res = query_win32_start_addr(NT_CURRENT_THREAD, &win32_start_addr);
    ASSERT_CURIOSITY(NT_SUCCESS(res) && "failed to obtain win32 start address");
    if (!NT_SUCCESS(res)) {
        /* assume it was primary if we can't tell */
        win32_start_addr = NULL;
    }

    if ((ptr_uint_t)win32_start_addr < 0x10000 && /* can't be on NULL page */
        win32_start_addr != NULL) {
        /* the value is not reliable if the thread has run and there
         * was a ReplyWaitRecievePort - this would no longer be the
         * correct start address.  We're making an assumption that if
         * that is the case then possibly via user32.dll injection
         * some other DLL that got to run before us may have done this.
         */
        ASSERT_NOT_TESTED();
        win32_start_addr = NULL;
    }
    /* unfortunately the value is also not reliable if Native threads
     * are used as well, and winlogon.exe's parent smss.exe is
     * creating processes directly.  FIXME: pstat however is able to
     * show the value
     */

    /* FIXME: could exempt winlogon.exe by name instead */
    ASSERT_CURIOSITY(win32_start_addr != NULL);

    /* FIXME: while we should be able to always intercept the image
     * entry point, regardless of which threads we really control, we
     * don't need to risk these extra moving parts unless we are
     * certain we are in a secondary thread
     */
    if (win32_start_addr != NULL && win32_start_addr != get_image_entry()) {
        dcontext_t *secondary_dcontext = get_thread_private_dcontext();
        SYSLOG_INTERNAL_WARNING("took over non-primary thread!\n");
        dr_injected_primary_thread = false;
        dr_late_injected_primary_thread = false;
        /* these will be set to to true only when we reach the image entry */

        /* flags the reason for taking over late for use by
         * insert_image_entry_trampoline()
         */
        dr_injected_secondary_thread = true;

        /* Although we don't really control_all_threads we'll leave it
         * set, and we're hoping that we'll soon take over the primary
         * thread since it really has nothing interesting to do before
         * reaching the image entry point, so it is ok to let it execute
         * as unknown thread for a little bit.  (We will still have to
         * furnish it with a dcontext and stack when we take over.)
         */

        /* potentially racy hook (injected threads) */
        insert_image_entry_trampoline(secondary_dcontext);
    } else {
        /* we are in the primary thread */
        dr_injected_primary_thread = true;
    }
}

/* When we are forced to lose control of a thread during an initialization
 * APC, we try to regain control w/ a trampoline at the image entry point
 */
static after_intercept_action_t /* note return value will be ignored */
intercept_image_entry(app_state_at_intercept_t *state)
{
    if (dr_injected_secondary_thread) {
        /* we finally took over the primary thread */
        SYSLOG_INTERNAL_WARNING("image entry point - should be in primary thread\n");

        DOCHECK(1, {
            /* check other threads don't reach image entry point for some reason */
            app_pc win32_start_addr = 0;
            NTSTATUS res = query_win32_start_addr(NT_CURRENT_THREAD, &win32_start_addr);
            ASSERT(NT_SUCCESS(res) && "failed to obtain win32 start address");
            if (win32_start_addr != get_image_entry()) {
                ASSERT(false && "reached by non-primary thread");
                /* FIXME: if this can happen we may want to wait for
                 * the primary and do this out of DODEBUG as well
                 */
            }
        });

        /* if entry point is never reached debug build will complain */

        /* we could have taken over the primary thread already if it
         * has executed its KiUserApcDispatcher routine after the
         * secondary thread has intercepted it, in which case we'd be
         * on a dstack.  Alternatively, we would be on d_r_initstack and
         * have no dcontext and dstack */

        /* we must create a new dcontext to be a 'known' thread */
        /* initialize thread now */
        if (dynamo_thread_init(NULL, NULL, NULL, false) != -1) {
            LOG(THREAD_GET, LOG_ASYNCH, 1, "just initialized primary thread \n");
            /* keep in synch if we do anything else in intercept_new_thread() */
        } else {
            /* Note that it is possible that we were in full control
             * of the primary thread, if it hasn't reached its APC
             * at the time the secondary thread was started
             */
            LOG(THREAD_GET, LOG_ASYNCH, 1, "primary thread was already known\n");
            /* we'll still treat as if it needed to be injected late */

            if (!RUNNING_WITHOUT_CODE_CACHE()) {
                dcontext_t *existing_dcontext = get_thread_private_dcontext();

                /* we MUST flush our image entry point fragment that
                 * would currently use the trampoline.
                 */
                ASSERT(fragment_lookup(existing_dcontext, image_entry_pc) != NULL);
                /* We can lookup at that tag almost for sure,
                 * currently since the indirect call will always have
                 * us start a new bb.  Note we were just there, and
                 * the new bb we'll build won't be executed ever
                 * again.
                 */
                /* note we only flush, but not remove region, since we will not rewalk */
                flush_fragments_in_region_start(existing_dcontext, image_entry_pc, 1,
                                                false /* don't own initexit_lock */,
                                                false /* keep futures */,
                                                false /* exec still valid */,
                                                false /* don't force sychall */
                                                _IF_DGCDIAG(NULL));
                flush_fragments_in_region_finish(existing_dcontext, false);

                ASSERT_NOT_TESTED();
            }
        }

        /* for presys_TerminateThread() need to set after we have become 'known' */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        dr_late_injected_primary_thread = true;
        dr_injected_primary_thread = true;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        /* three protect/unprotect pairs on this path, still rare enough */

        /* in both -client and -thin_client mode */
        if (RUNNING_WITHOUT_CODE_CACHE()) {
            /* nothing more to do here, we just wanted to have a
             * proper dcontext created and set in TLS
             */
            dcontext_t *dcontext = get_thread_private_dcontext();

            /* potentially slightly racy with injected threads */
            remove_image_entry_trampoline();
            /* note that we are not freeing the emitted code, we are
             * just fixing up the original application code
             */

            /* case 9347 - we will incorrectly reach asynch_take_over() */
            /* FIXME: we chould have created the trampoline to 'let
             * go' directly to the now restored application code.
             * AFTER_INTERCEPT_LET_GO_ALT_DYN is closest to what we
             * need - direct native JMP to the image entry, instead of
             * potentially non-pc-relativized execution from our copy,
             * and we only have to make sure we use the d_r_initstack properly.
             *
             * Instead of changing the assembly, we're using a gross
             * hack here - we'll have to rely on asynch_take_over() ->
             * transfer_to_dispatch() -> is_stopping_point(),
             * dispatch_enter_native() to cleanly jump back to the
             * original application code (releasing d_r_initstack etc)
             */
            dcontext->next_tag = BACK_TO_NATIVE_AFTER_SYSCALL;
            /* start_pc is the take-over pc that will jmp to the syscall instr, while
             * we need the post-syscall pc, which we stored when generating the trampoline
             */
            ASSERT(image_entry_pc != NULL);
            dcontext->native_exec_postsyscall = image_entry_pc;

            /* ignored, we are created as AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT */
            return AFTER_INTERCEPT_LET_GO;
        }

        ASSERT(
            IS_UNDER_DYN_HACK(thread_lookup(d_r_get_thread_id())->under_dynamo_control));
    }

    if (dynamo_initialized) {
        thread_record_t *tr = thread_lookup(d_r_get_thread_id());
        /* FIXME: we must unprot .data here and again for retakeover: worth optimizing? */
        set_reached_image_entry();
        if ((tr != NULL && IS_UNDER_DYN_HACK(tr->under_dynamo_control)) ||
            dr_injected_secondary_thread) {
            LOG(THREAD_GET, LOG_ASYNCH, 1, "inside intercept_image_entry\n");
            /* we were native, retakeover */
            retakeover_after_native(tr, INTERCEPT_IMAGE_ENTRY);
#ifdef RETURN_AFTER_CALL
            /* ref case 3565, we need to add the return address to the allowed
             * return after call transitions table */
            ASSERT(tr->dcontext == get_thread_private_dcontext());

            /* Addition to the fix for case 3565; ref case 3896, can't add the
             * return address for -no_ret_after_call; hash table not set up.
             */
            if (DYNAMO_OPTION(ret_after_call)) {
                if (is_readable_without_exception((byte *)state->mc.xsp,
                                                  sizeof(app_pc))) {
                    fragment_add_after_call(tr->dcontext, *(app_pc *)state->mc.xsp);
                } else {
                    ASSERT_NOT_REACHED();
                }
            }
#endif
            /* we want to take over, but we'll do that anyway on return from this routine
             * since our interception code was told to take over afterward, so we wait.
             * this routine executes natively so even if we were already in control
             * we want to take over so no conditional is needed there.
             */
        } else {
            SYSLOG_INTERNAL_ERROR("Image entry interception point reached by unexpected "
                                  "%s thread " TIDFMT "",
                                  (tr == NULL) ? "unknown" : "known",
                                  d_r_get_thread_id());
            ASSERT_NOT_REACHED();
        }
    }
    return AFTER_INTERCEPT_TAKE_OVER;
}

/* WARNING: only call this when there is only one thread going!
 * This is not thread-safe!
 */
static byte *
insert_image_entry_trampoline(dcontext_t *dcontext)
{
    /* Adds a trampoline at the image entry point.
     * We ignore race conditions here b/c we assume there's only one thread.
     * FIXME: kernel32's base process start point is actually where
     * control appears, but it's not exported.  you can find it by seeing
     * what the starting point is using our targeted injection.
     * It only executes a dozen or so instructions before calling the
     * image entry point, but it would be nice if we had a way to find
     * it -- how does loader/whoever know what it is?  how do you get there,
     * if the thread's pc points at image entry point to begin with?
     *
     * (kernel32 call to image entry point for Win2K Server is 0x77e87900
     * (so retaddr on stack is 0x77e87903).  the actual entry point, from
     * examining the ntcontinue finishing up the init apc, is 0x77e878c1.
     * FIXME: how find that programatically?  is it used for every process?
     * Inside Windows 2000 indicates that it is.
     * Promising approach: use QueryThreadInformation with ThreadQuerySetWin32StartAddress
     * -- although that can get clobbered, hopefully will still be set to start
     * address when we read it.
     * Note that will most likely be the image entry point (win32), not the
     * actual entry point (it is the address in eax of the CreateThread system
     * call) as that's how it behaves for a normal CreateThread
     */
    static bool image_entry_hooked = false;
    /* need to set a flag to prevent double injection - e.g. if we
     * have a callback return in a secondary thread
     */
    if (image_entry_hooked) {
        ASSERT_NOT_TESTED();
        LOG(THREAD, LOG_ASYNCH, 1, "WARNING: already hooked!\n");
        ASSERT(dr_injected_secondary_thread);
        return NULL;
    }
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);

    /* note we are thread safe, but we are not worried about a race,
     * but only about getting a callback in a secondary thread after
     * it has already hooked */
    image_entry_hooked = true;
    image_entry_pc = get_image_entry();
    if (dr_injected_secondary_thread) {
        LOG(THREAD, LOG_ASYNCH, 1,
            "WARNING: image entry hook to catch primary thread!\n");
    } else {
        LOG(THREAD, LOG_ASYNCH, 1, "WARNING: callback return with native cb context!\n");
    }

    LOG(THREAD, LOG_ASYNCH, 1, "\tInserting trampoline at image entry point " PFX "\n",
        image_entry_pc);

    image_entry_trampoline =
        insert_trampoline(image_entry_pc, intercept_image_entry, 0, /* no arg */
                          false /* do not assume esp */,
                          /* handler should restore target */
                          AFTER_INTERCEPT_TAKE_OVER_SINGLE_SHOT,
                          true /* single shot, safe to ignore CTI */);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    return image_entry_pc;
}

/****************************************************************************/

/* Note that we don't want to be overwriting ntdll code when another
 * thread is in there, so this init routine should be called prior to
 * creating any other threads...if not using run-entire-program-under-dynamo
 * approach, I'm not sure how to guarantee no race conditions...FIXME
 */

/* for PR 200207 we want KiUserExceptionDispatcher hook early, but we don't
 * want -native_exec_syscalls hooks early since client might scan syscalls
 * to dynamically get their #s.  Plus we want the Ldr hook later to
 * support -no_private_loader for probe API.
 */
void
callback_interception_init_start(void)
{
    byte *pc;
    byte *int2b_after_cb_dispatcher;
    module_handle_t ntdllh = get_ntdll_base();

    intercept_asynch = true;
    intercept_callbacks = true;

    interception_code = interception_code_array;

#ifdef INTERCEPT_TOP_LEVEL_EXCEPTIONS
    app_top_handler =
        SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#endif

    /* Note that we go ahead and assume that the app's esp is valid for
     * most of these interceptions.  This is for efficiency -- the alternative
     * is to use the global d_r_initstack, which imposes a synchronization point,
     * which we don't want for multi-thread frequent events like callbacks.
     * Re: transparency, if the app's esp were not valid, the native
     * routine would crash in a similar, though not necessarily
     * exactly the same, manner as the program will under DynamoRIO.
     */

    /* We place a small struct at the base of the interception code to pass
     * information to outside processes (such as build type, number, etc.).
     * The outside process finds the struct by page aligning the target of our
     * KiUserCallbackDispatcher hook and verifies it by checking certain magic
     * number fields in the struct.  This is also used to determine if dr
     * is running in the process */
    ASSERT(ALIGNED(interception_code, PAGE_SIZE));
    init_dr_marker((dr_marker_t *)interception_code);

    pc = interception_code + sizeof(dr_marker_t);

    /* Order of hooking matters to some degree.  LdrInitializeThunk, then APC
     * dispatcher and then callback dispatcher.
     */
    if (!DYNAMO_OPTION(thin_client)) {
        if (DYNAMO_OPTION(handle_ntdll_modify) != DR_MODIFY_OFF) {
            app_pc ntdll_base = get_ntdll_base();
            size_t ntdll_module_size = get_allocation_size(ntdll_base, NULL);

            /* FIXME: should only add code section(s!), but for now adding
             * whole module
             */
            app_pc ntdll_code_start = ntdll_base;
            app_pc ntdll_code_end = ntdll_base + ntdll_module_size;

            tamper_resistant_region_add(ntdll_code_start, ntdll_code_end);
        }
    }

    intercept_map =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, intercept_map_t, ACCT_OTHER, PROTECTED);
    memset(intercept_map, 0, sizeof(*intercept_map));

    /* we assume callback_interception_init_finish() is called immediately
     * after client init, and that leaving interception_code off exec areas
     * and writable during client init is ok: but now that the buffer is inside
     * our data section, we must mark it +x, before we set up any hooks.
     */
    set_protection(interception_code, INTERCEPTION_CODE_SIZE,
                   MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC);

    /* Note that if we switch to using a non-5-byte-reljmp for our Ki hooks
     * we need to change our drmarker reader. */

    /* LdrInitializeThunk is hooked for thin_client too, so that
     * each thread can have a dcontext (case 8884). */
    if (get_os_version() >= WINDOWS_VERSION_VISTA) {
        LdrInitializeThunk = (byte *)d_r_get_proc_address(ntdllh, "LdrInitializeThunk");
        ASSERT(LdrInitializeThunk != NULL);
        /* initialize this now for use later in intercept_new_thread() */
        RtlUserThreadStart = (byte *)d_r_get_proc_address(ntdllh, "RtlUserThreadStart");
        ASSERT(RtlUserThreadStart != NULL);
        ldr_init_pc = pc;
        pc = intercept_call(pc, (byte *)LdrInitializeThunk, intercept_ldr_init,
                            0,    /* no arg */
                            true, /* FIXME: assume esp only until dstack check
                                   * separated! */
                            AFTER_INTERCEPT_LET_GO, false /* cannot ignore on CTI */,
                            false /* handle CTI */, NULL, NULL);
    }

    /* hook APC dispatcher, also check context offset */
    /* APC dispatcher is hooked for thin_client too, so that
     * each thread can have a dcontext (case 8884).  Needed for handling
     * process control/detach nudges for thin_client and for muting other ones
     * (case 8888).  Also, in future, if we implement inflating to hotp_only
     * mode, then each thread needs to have a dcontext.
     */
    check_apc_context_offset((byte *)KiUserApcDispatcher);
    after_apc_orig_pc = get_pc_after_call((byte *)KiUserApcDispatcher, NULL);
    apc_pc = pc;
    pc = intercept_call(pc, (byte *)KiUserApcDispatcher, intercept_apc, 0, /* no arg */
                        true, /* FIXME: assume esp only until dstack check
                               * separated! */
                        AFTER_INTERCEPT_LET_GO, false /* cannot ignore on CTI */,
                        false /* handle CTI */, NULL, NULL);

    /* The apc hook is how we catch new threads, make sure none sneaked in
     * while we were still initializing. Also the nodemgr etc. use the
     * callback hook to find the drmarker, once it's inserted they might
     * start injecting threads, so be sure to do the apc hook first so we
     * see them. */
    DODEBUG({
        /* case 9423 - just SYSLOG, we can somewhat handle these */
        /* XXX i#1305: while we now take over other threads at init time, we do
         * not yet suspend all other threads for duration of DR init to avoid
         * races.
         */
        if (!check_sole_thread()) {
            SYSLOG_INTERNAL_WARNING("Early threads found");
        }
    });

    callback_pc = pc;
    /* make sure nobody ever comes back to instr after callback call: */
    after_callback_orig_pc =
        get_pc_after_call((byte *)KiUserCallbackDispatcher, &int2b_after_cb_dispatcher);
    /* make sure dispatcher concludes with an int 2b */
    /* In Win 2003 SP1, the dispatcher concludes with a ret.  See case 3522. */
    /* For thin_client we need to hook this to be able to read the DRmarker. */
    ASSERT_CURIOSITY(int2b_after_cb_dispatcher != NULL);
    pc = intercept_call(pc, (byte *)KiUserCallbackDispatcher, intercept_callback_start,
                        0, /* no arg */
                        true /* FIXME: assume esp only until dstack check separated! */,
                        AFTER_INTERCEPT_LET_GO, false /* cannot ignore on CTI */,
                        false /* handle CTI */, NULL, NULL);

    /* We would like to not assume esp for exceptions, since we mark
     * executable regions read-only and if the stack contains code
     * we'd like to be able to catch write faults on the stack,
     * but the kernel just silently kills the process if the user stack
     * is not valid!  So may as well be more efficient and assume esp ourselves.
     * Note: thin_client needs this hook to catch and report core exceptions.
     */
    exception_pc = pc;
    pc = intercept_call(
        pc, (byte *)KiUserExceptionDispatcher, intercept_exception, 0, /* no arg */
        false /* do not assume esp */, AFTER_INTERCEPT_LET_GO,
        false /* cannot ignore on CTI */, false /* handle CTI */, NULL, NULL);

    interception_cur_pc = pc; /* save for callback_interception_init_finish() */

    /* other initialization */
#ifndef X64
    if (get_os_version() >= WINDOWS_VERSION_8) {
        KiFastSystemCall = (byte *)d_r_get_proc_address(ntdllh, "KiFastSystemCall");
        ASSERT(KiFastSystemCall != NULL);
    }
#endif
}

void
callback_interception_init_finish(void)
{
    /* must be called immediately after callback_interception_init_start()
     * as this finishes up initialization
     */
    byte *pc = interception_cur_pc;
    DEBUG_DECLARE(dr_marker_t test_marker);

    if (!DYNAMO_OPTION(thin_client)) {
        raise_exception_pc = pc;
        pc = intercept_call(pc, (byte *)KiRaiseUserExceptionDispatcher,
                            intercept_raise_exception, 0, /* no arg */
                            false /* do not assume esp */, AFTER_INTERCEPT_LET_GO,
                            false /* cannot ignore on CTI */, false /* handle CTI */,
                            NULL, NULL);

        /* WARNING: these two routines are entered from user mode, so the
         * interception code for them ends up being executed under DynamoRIO,
         * and we rely on the list of do-not-inline to allow the actual
         * intercept_{un,}load_dll routine to execute natively.
         * We also count on the interception code to not do anything that won't
         * cause issues when passed through d_r_dispatch().
         * FIXME: a better way to do this?  assume entry point will always
         * be start of fragment, add clean call in mangle?
         */
        if (should_intercept_LdrLoadDll()) {
            load_dll_pc = pc;
            pc = intercept_call(
                pc, (byte *)LdrLoadDll, intercept_load_dll, 0, /* no arg */
                false /* do not assume esp */, AFTER_INTERCEPT_DYNAMIC_DECISION,
                true /* not critical trampoline, can ignore if hooked with CTI */,
                false /* handle CTI */, NULL, NULL);
            if (pc == NULL) {
                /* failed to hook, reset pointer for next routine */
                pc = load_dll_pc;
                load_dll_pc = NULL;
            }
        }
        if (should_intercept_LdrUnloadDll()) {
            unload_dll_pc = pc;
            pc = intercept_call(
                pc, (byte *)LdrUnloadDll, intercept_unload_dll, 0, /* no arg */
                false /* do not assume esp */, AFTER_INTERCEPT_DYNAMIC_DECISION,
                true /* not critical trampoline, can ignore if * hooked with CTI */,
                false /* handle CTI */, NULL, NULL);
            if (pc == NULL) {
                /* failed to hook, reset pointer for next routine */
                pc = unload_dll_pc;
                unload_dll_pc = NULL;
            }
        }
    }

    pc = emit_takeover_code(pc);

    ASSERT(pc - interception_code < INTERCEPTION_CODE_SIZE);
    interception_cur_pc = pc; /* set global pc for future trampoline insertions */

    if (DYNAMO_OPTION(native_exec_syscalls)) {
        syscall_trampolines_start = interception_cur_pc;
        init_syscall_trampolines();
        syscall_trampolines_end = interception_cur_pc;
    }

    if (DYNAMO_OPTION(clean_testalert)) {
        GET_NTDLL(NtTestAlert, (void));
        clean_syscall_wrapper((byte *)NtTestAlert, SYS_TestAlert);
    }

#ifdef PROGRAM_SHEPHERDING
    /* a fragment will be built from this code, but it's not for
     * general execution, so add as dynamo area but not executable area
     * should already be added since it's part of data section
     * FIXME: use generated-code region rather than data section?
     */
    if (!is_dynamo_address(interception_code) ||
        !is_dynamo_address(interception_code + INTERCEPTION_CODE_SIZE -
                           1)) { /* check endpoints */
        /* probably was already added but just to make sure */
        add_dynamo_vm_area(interception_code,
                           interception_code + INTERCEPTION_CODE_SIZE - 1,
                           MEMPROT_READ | MEMPROT_WRITE,
                           true /* from image since static */
                           _IF_DEBUG("intercept_call"));
    }
#endif

    DOLOG(3, LOG_EMIT, {
        dcontext_t *dcontext = get_thread_private_dcontext();
        bool skip8 = false;
        byte *end_asynch_pc = pc;
        if (dcontext == NULL)
            dcontext = GLOBAL_DCONTEXT;
        pc = interception_code + sizeof(dr_marker_t);
        LOG(GLOBAL, LOG_EMIT, 3, "\nCreated these interception points:\n");
        do {
            if (pc == callback_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "KiUserCallbackDispatcher:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == apc_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "KiUserApcDispatcher:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == exception_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "KiUserExceptionDispatcher:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == raise_exception_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "KiRaiseUserExceptionDispatcher:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == load_dll_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "LdrLoadDll:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == unload_dll_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "LdrUnloadDll:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc != NULL && pc == ldr_init_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "LdrInitializeThunk:\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <backup of 1st 5 bytes>\n");
                LOG(GLOBAL, LOG_EMIT, 3, "  <landing pad address>\n");
                pc += 5 + sizeof(byte *);
            } else if (pc == end_asynch_pc) {
                LOG(GLOBAL, LOG_EMIT, 3, "\nSyscall trampolines:\n\n");
            }
            IF_X64({
                /* handle 8 bytes of address at end */
                if (pc + JMP_ABS_IND64_SIZE + sizeof(byte *) <= interception_cur_pc &&
                    *pc == JMP_ABS_IND64_OPCODE && *(pc + 1) == JMP_ABS_MEM_IND64_MODRM &&
                    *(int *)(pc + 2) == 0 /*next pc*/)
                    skip8 = true;
            });
            pc = disassemble_with_bytes(dcontext, pc, main_logfile);
            IF_X64({
                if (skip8) {
                    LOG(GLOBAL, LOG_EMIT, 3, "  <return target address: " PFX ">\n",
                        *(byte **)pc);
                    pc += sizeof(byte *);
                    skip8 = false;
                }
            });
        } while (pc < interception_cur_pc);
        LOG(GLOBAL, LOG_EMIT, 3, "\n");
    });

    /* make unwritable and +x */
    set_protection(interception_code, INTERCEPTION_CODE_SIZE,
                   MEMPROT_READ | MEMPROT_EXEC);

    /* No vm areas except dynamo_areas exists in thin_client mode. */
    if (!DYNAMO_OPTION(thin_client)) {
        /* add interception code to the executable list */
        /* N.B.: we duplicate this call after losing control and re-doing exec
         * regions
         */
        add_executable_region(
            interception_code,
            INTERCEPTION_CODE_SIZE _IF_DEBUG("heap mmap callback interception code"));
        landing_pads_to_executable_areas(true /* add */);
    }

    ASSERT(read_and_verify_dr_marker(NT_CURRENT_PROCESS, &test_marker) ==
           DR_MARKER_FOUND);
}

DEBUG_DECLARE(static bool callback_interception_unintercepted = false;);

/* N.B.: not thread-safe! */
void
callback_interception_unintercept()
{
    /* remove syscall trampolines BEFORE turning off asynch, to avoid
     * thinking we're a native thread! */
    if (DYNAMO_OPTION(native_exec_syscalls)) {
        exit_syscall_trampolines();
        syscall_trampolines_start = NULL;
        syscall_trampolines_end = NULL;
    }

    intercept_asynch = false;
    intercept_callbacks = false;

    LOG(GLOBAL, LOG_ASYNCH | LOG_STATS, 1,
        "Total # of asynchronous events for process:\n");
    LOG(GLOBAL, LOG_ASYNCH | LOG_STATS, 1, "   Callbacks:  %d\n",
        GLOBAL_STAT(num_callbacks));
    LOG(GLOBAL, LOG_ASYNCH | LOG_STATS, 1, "   APCs:       %d\n", GLOBAL_STAT(num_APCs));
    LOG(GLOBAL, LOG_ASYNCH | LOG_STATS, 1, "   Exceptions: %d\n",
        GLOBAL_STAT(num_exceptions));

    un_intercept_call(load_dll_pc, (byte *)LdrLoadDll);
    un_intercept_call(unload_dll_pc, (byte *)LdrUnloadDll);
    un_intercept_call(raise_exception_pc, (byte *)KiRaiseUserExceptionDispatcher);
    un_intercept_call(callback_pc, (byte *)KiUserCallbackDispatcher);
    un_intercept_call(apc_pc, (byte *)KiUserApcDispatcher);
    if (get_os_version() >= WINDOWS_VERSION_VISTA) {
        ASSERT(ldr_init_pc != NULL && LdrInitializeThunk != NULL);
        un_intercept_call(ldr_init_pc, (byte *)LdrInitializeThunk);
    }
    /* remove exception dispatcher last to catch errors in the meantime */
    un_intercept_call(exception_pc, (byte *)KiUserExceptionDispatcher);

    free_intercept_list();

    if (doing_detach) {
        DEBUG_DECLARE(bool ok =)
        make_writable(interception_code, INTERCEPTION_CODE_SIZE);
        ASSERT(ok);
    }
    DODEBUG(callback_interception_unintercepted = true;);
}

void
callback_interception_exit()
{
    ASSERT(callback_interception_unintercepted);
    /* FIXME : we are exiting so no need to flush here right? */
    if (!DYNAMO_OPTION(thin_client)) {
        remove_executable_region(interception_code, INTERCEPTION_CODE_SIZE,
                                 false /*no lock*/);
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, intercept_map, intercept_map_t, ACCT_OTHER,
                   PROTECTED);

    landing_pads_to_executable_areas(false /* remove */);
}

static void
swap_dcontexts(dcontext_t *d1, dcontext_t *d2)
{
    dcontext_t temp;
    /* be careful some fields can't be blindly swapped */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        /* deep swap of upcontext */
        unprotected_context_t uptemp;
        memcpy((void *)&uptemp, (void *)d1->upcontext.separate_upcontext,
               sizeof(unprotected_context_t));
        memcpy((void *)d1->upcontext.separate_upcontext,
               (void *)d2->upcontext.separate_upcontext, sizeof(unprotected_context_t));
        memcpy((void *)d2->upcontext.separate_upcontext, (void *)&uptemp,
               sizeof(unprotected_context_t));
    }
    memcpy((void *)&temp, (void *)d1, sizeof(dcontext_t));
    memcpy((void *)d1, (void *)d2, sizeof(dcontext_t));
    memcpy((void *)d2, (void *)&temp, sizeof(dcontext_t));
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        /* must swap upcontext pointers back since code is hardcoded for main one */
        temp.upcontext.separate_upcontext = d2->upcontext.separate_upcontext;
        d2->upcontext.separate_upcontext = d1->upcontext.separate_upcontext;
        d1->upcontext.separate_upcontext = temp.upcontext.separate_upcontext;
    }
    /* must swap self pointers back so that asm routines work */
    temp.upcontext_ptr = d2->upcontext_ptr;
    d2->upcontext_ptr = d1->upcontext_ptr;
    d1->upcontext_ptr = temp.upcontext_ptr;
    /* swap nonswapped field back */
    temp.nonswapped_scratch = d2->nonswapped_scratch;
    d2->nonswapped_scratch = d1->nonswapped_scratch;
    d1->nonswapped_scratch = temp.nonswapped_scratch;
    /* swap allocated starts back */
    temp.allocated_start = d2->allocated_start;
    d2->allocated_start = d1->allocated_start;
    d1->allocated_start = temp.allocated_start;
    /* swap list pointers back */
    temp.prev_unused = d1->prev_unused;
    d1->prev_unused = d2->prev_unused;
    d2->prev_unused = temp.prev_unused;
}

#ifdef RETURN_AFTER_CALL
/* return EMPTY if we are past the stack bottom - target_pc should NOT be let through
          BOTTOM_REACHED just reached stack bottom, let this through, but start enforcing
          BOTTOM_NOT_REACHED haven't yet reached the stack bottom, let this through
            do not enforce
 */
initial_call_stack_status_t
at_initial_stack_bottom(dcontext_t *dcontext, app_pc target_pc)
{
    /* FIXME: this is quite messy - should figure a better way to keep track
     * of the initial call stack:
     *  - either explicitly save it after all, or maybe only use the depth
     *    instead of matching the address
     */
    LOG(THREAD, LOG_ASYNCH | LOG_STATS, 1,
        "get_initial_stack_bottom: preinjected=%d interception_point=%d "
        "after_callback=" PFX "\n",
        dr_preinjected, interception_point, after_callback_orig_pc);
    /* CHECK: drinject AND follow children go through dynamo_auto_start
       instead of dynamorio_app_take_over which sets dr_preinjected */

    /* we start with an empty stack when explicitly injected  */
    if (!dr_preinjected)
        return INITIAL_STACK_EMPTY;

    if (interception_point == INTERCEPT_IMAGE_ENTRY) {
        /* intercept_image_entry does not execute any DR returns so this is a violation.
         * At image entry trampoline we explicitly added the kernel32 thunk as a
         * RAC target, so even though we didn't see the real stack bottom we've
         * added the bottom frame and so cannot bottom out.
         */
        return INITIAL_STACK_EMPTY;
    }

    if (interception_point == INTERCEPT_LOAD_DLL ||
        interception_point == INTERCEPT_UNLOAD_DLL ||
        interception_point == INTERCEPT_EARLY_ASYNCH ||
        interception_point == INTERCEPT_SYSCALL ||
        interception_point == INTERCEPT_PREINJECT) {
        /* initial APC still has control */
        /* Checking for after_call instruction in KiUserApcDispatcher is problematic --
         * could see it too early if there's a nested APC before the init APC finishes,
         * and if we take control after the end of the init APC but before the image entry
         * trampoline we may never interpret that pc.  Instead we wait until the image
         * entry, fixing those problems, and only opening up a hole by delaying during the
         * kernel32 thunk, which doesn't execute much code.
         */
        if (reached_image_entry_yet()) {
            /* we're past the image entry, so this is a violation, not a bottoming
             * out -- and any issues w/ bottoming out between the image entry
             * and the kernel32 thunk on the way back down should have been
             * handled in the image entry trampoline
             */
            return INITIAL_STACK_EMPTY;
        } else {
            /* FIXME: if we never get to that address we'll never trigger a violation,
             * very unsafe
             */
            return INITIAL_STACK_BOTTOM_NOT_REACHED;
        }
    }

    ASSERT_NOT_REACHED();
    /* safe default */
    return INITIAL_STACK_EMPTY;
}

/* Allow a ret to target an address inside an .xdata section that was
 * the argument to an NtFlushInstructionCache syscall.
 * (xref case 7319)
 */
static bool
at_xdata_rct_exception(dcontext_t *dcontext, app_pc target_pc)
{
    app_pc modbase = get_module_base(target_pc);
    ASSERT(DYNAMO_OPTION(xdata_rct));
    if (modbase != NULL && is_in_xdata_section(modbase, target_pc, NULL, NULL) &&
        was_address_flush_start(dcontext, target_pc)) {
        SYSLOG_INTERNAL_INFO("RCT: .xdata NtFlush-target matched @" PFX, target_pc);
        STATS_INC(ret_after_call_xdata);
        return true;
    }
    return false;
}

/* allow any RCT - though seen a .E when a CALL* from a Kaspersky
 * kernel driver goes to the entry points of the API routines whose
 * exports were hijacked
 */
static bool
at_driver_rct_exception(dcontext_t *dcontext, app_pc source_pc)
{
    ASSERT(DYNAMO_OPTION(driver_rct));
    if (!is_user_address(source_pc) && is_driver_address(source_pc)) {
        SYSLOG_INTERNAL_INFO_ONCE("RCT: kernel driver source @" PFX, source_pc);
        STATS_INC(num_rct_driver_address);
        return true;
    }
    return false;
}

/* Fibers on Win2003 RAC false positive - see case 1543, see also 9726 on Vista */
/* On Win2003 - the initial trampoline needed to initialize the
   function call to the FiberFunc is not registered as an after call
   site when the SwitchToFiber is called for the first time.  This
   piece of code seems to be the same in kernel32.dll on Windows 2000
   and Windows 2003.  So I'll take the risk they won't change this
   instruction to use a different register, and they won't optimize it
   like in our case 1307.  The match will be simply of the first 4
   bytes SEG_FS MOV_EAX 10 00 and only one target location will be
   exempted.

   FIXME: It would be nicer if we could get a pattern on the
   source_fragment (e.g. like at_vbjmp_exception), then we wouldn't
   have to worry about readability of the target address and it is in
   a way safer.

   In terms of multithread safety - it is OK to have multiple threads
   test for the condition, and as long as any of them sets the
   exempted address I don't expect attackers to have any chance here.

   FIXME: We also explicitly check if target_pc is readable, although
   a higher level check should be added to return
   UNREADABLE_MEMORY_EXECUTION_EXCEPTION in that case.

   > u 0x77e65927
   kernel32!ConvertFiberToThread+0x44:  [ with symbols kernel32!BaseFiberStart: ]
   77e65927 64a110000000     mov     eax,fs:[00000010]  ; FIBER_DATA_TIB_OFFSET

   From XP (which doesn't have a problem since we jmp * instead of ret here)
   kernel32!BaseFiberStart:
   7c82ff92 64a110000000     mov     eax,fs:[00000010]
   7c82ff98 ffb0b8000000     push    dword ptr [eax+0xb8]
   7c82ff9e ffb0c4000000     push    dword ptr [eax+0xc4]
   7c82ffa4 e8a3b6fdff       call    kernel32!BaseThreadStart (7c80b64c)
   7c82ffa9 c3               ret

   on Vista routine has been changed with the addition of an SEH frame first and other
   modification (likely since kernel32!BaseThreadStart no longer exists)
   kernel32!BaseFiberStart:
   76cde8ca 6a0c             push    0xc
   76cde8cc 68f8e8cd76       push    0x76cde8f8
   76cde8d1 e85a8f0300       call    kernel32!_SEH_prolog4 (76d17830)
   76cde8d6 64a110000000     mov     eax,fs:[00000010]
   76cde8dc 8b88c4000000     mov     ecx,[eax+0xc4]
   76cde8e2 8b80b8000000     mov     eax,[eax+0xb8]
   76cde8e8 8365fc00         and     dword ptr [ebp-0x4],0x0
   76cde8ec 50               push    eax
   76cde8ed ffd1             call    ecx
   76cde8ef 6a00             push    0x0
   76cde8f1 ff150810cd76 call dword ptr [kernel32!_imp__RtlExitUserThread (76cd1008)]
   76cde8f7 90               nop
   76cde8f8 feff             ???     bh
   76cde8fa ffff             ???

   On 64-bit XP:
   kernel32!BaseFiberStart:
   00000000`77d687c0 65488b0c2520000000 mov   rcx,qword ptr gs:[20h]
   00000000`77d687c9 488b91b8000000  mov     rdx,qword ptr [rcx+0B8h]
   00000000`77d687d0 488b89b0000000  mov     rcx,qword ptr [rcx+0B0h]
   00000000`77d687d7 e9c42e0000      jmp     kernel32!BaseThreadStart (00000000`77d6b6a0)

   On 64-bit Vista:
   Note - There is no kernel32!BaseFiberStart symbol, but this routine takes its
   place and is reached in the same way.
   00000000`772aa1d0 4883ec28        sub     rsp, 28h
   00000000`772aa1d4 65488b042520000000 mov   rax,qword ptr gs:[20h]
   00000000`772aa1dd 488b90b0000000  mov     rdx,qword ptr [rax+0B0h]
   00000000`772aa1e4 488b88b8000000  mov     rcx,qword ptr [rax+0B8h]
   00000000`772aa1eb ffd2            call    rdx
   00000000'772aa1ed 33c9            xor     ecx,ecx
   00000000'772aa1ef ff151b1e0b00    call    qword ptr [kernel32!_imp__RtlExitUserThread]

   Returns true if target_pc is readable and is the known fiber initialization routine.
*/

static bool
at_fiber_init_known_exception(dcontext_t *dcontext, app_pc target_pc)
{
    static app_pc fiber_init_known_pc = 0;
    int os_ver = get_os_version();

    if (os_ver <= WINDOWS_VERSION_XP || target_pc == 0) {
        /* only 2003 and Vista are known to have this problem */
        return false;
    }

    /* check if this is the first time we got to create a fiber,
     * and save the value as the only exception that is allowed to start with
     * this pattern */
    if (fiber_init_known_pc == 0) { /* never seen before */
        /* match first 7 bytes of mov eax/rax/rcx, fs/gs:[FIBER_DATA_TIB_OFFSET] */
        static const byte FIBER_CODE_32[] = { 0x64, 0xa1, 0x10, 0x00, 0x00, 0x00, 0x00 };
        static const byte FIBER_CODE_rcx_64[] = {
            0x65, 0x48, 0x8b, 0x0c, 0x25, 0x20, 0x00
        };
        static const byte FIBER_CODE_rax_64[] = {
            0x65, 0x48, 0x8b, 0x04, 0x25, 0x20, 0x00
        };
        enum { SUB_RSP_LENGTH = 4, FIBER_SEH_LENGTH = 12 };
        byte buf[sizeof(FIBER_CODE_32) + FIBER_SEH_LENGTH]; /* Vista needs extra */
        byte *cur = buf;
        const byte *pattern;
        size_t pattern_size;
        ASSERT(sizeof(FIBER_CODE_32) == sizeof(FIBER_CODE_rcx_64) &&
               sizeof(FIBER_CODE_32) == sizeof(FIBER_CODE_rax_64));

        if (!d_r_safe_read(target_pc, sizeof(buf), &buf))
            return false; /* target not sufficiently readable */

        /* For wow64 we expect to only see 32-bit kernel32 */
        if (IF_X64_ELSE(is_wow64_process(NT_CURRENT_PROCESS), true)) {
            pattern = FIBER_CODE_32;
            pattern_size = sizeof(FIBER_CODE_32);
            if (os_ver >= WINDOWS_VERSION_VISTA) {
                /* we expect some SEH code before the instruction to match
                 * 76cde8ca 6a0c             push    0xc
                 * 76cde8cc 68f8e8cd76       push    0x76cde8f8
                 * 76cde8d1 e85a8f0300       call    kernel32!_SEH_prolog4 (76d17830) */
                if (*cur == 0x6a && *(cur + 2) == 0x68 && *(cur + 7) == 0xe8)
                    cur += FIBER_SEH_LENGTH;
                else
                    return false; /* not a match */
            }
        } else {
            if (os_ver >= WINDOWS_VERSION_VISTA) {
                /* we expect a sub rsp first and to use rax instead of rcx
                 * 00000000`772aa1d0 4883ec28        sub     rsp, 28h
                 */
                if (*cur == 0x48 && *(cur + 1) == 0x83 && *(cur + 2) == 0xec) {
                    cur += SUB_RSP_LENGTH;
                    pattern = FIBER_CODE_rax_64;
                    pattern_size = sizeof(FIBER_CODE_rax_64);
                } else
                    return false; /* not a match */
            } else {
                pattern = FIBER_CODE_rcx_64;
                pattern_size = sizeof(FIBER_CODE_rcx_64);
            }
        }

        if (memcmp(cur, pattern, pattern_size) == 0) {
            /* We have a match! Now ensure target is in kernel32.dll */
            const char *target_module_name =
                os_get_module_name_strdup(target_pc HEAPACCT(ACCT_OTHER));
            if (target_module_name != NULL &&
                check_filter("kernel32.dll", target_module_name)) {
                /* We have a full match! */
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                fiber_init_known_pc = target_pc;
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                SYSLOG_INTERNAL_INFO("RCT: fiber matched @" PFX, fiber_init_known_pc);
            } else {
                ASSERT_CURIOSITY(false && "RCT: false fiber match");
            }
            dr_strfree(target_module_name HEAPACCT(ACCT_OTHER));
        }
    }

    if (fiber_init_known_pc == target_pc && fiber_init_known_pc != 0) {
        return true;
    }
    return false;
}

enum {
    MAX_SEH_TRYLEVEL = 8,
    INSTR_PUSH_IMMED32_LENGTH = 5,
    INSTR_PUSH_IMMED32_OPCODE = 0x68,
};

/* We allow non-standard uses of ret with SEH that we have seen on NT4
 * in kernel32 and ntdll.  An example is a bad ret from 77f65b33 to
 * 77f1d9eb, via a push immed that is used to skip a ret that is part
 * of a handler:
 *
 *   kernel32!TlsFree:
 *   ...
 *   77f1d9df 68ebd9f177       push    0x77f1d9eb
 *   77f1d9e4 ff2548c0f377 jmp dword ptr [KERNEL32!_imp__RtlReleasePebLock (77f3c048)]
 *   77f1d9ea c3               ret
 *   77f1d9eb 0fb645e7         movzx   eax,byte ptr [ebp-0x19]
 *
 *   at top of routine we have:
 *   KERNEL32!TlsFree:
 *   77f1d92d 64a100000000     mov     eax,fs:[00000000]
 *   77f1d933 55               push    ebp
 *   77f1d934 8bec             mov     ebp,esp
 *   77f1d936 6aff             push    0xff
 *   77f1d938 6808d3f377       push    0x77f3d308
 *   77f1d93d 6844b7f377       push    0x77f3b744
 *
 *   and indeed those are on top of the SEH stack:
 *   0:000> !teb
 *   TEB at 7ffde000
 *       ExceptionList:    12fdb4
 *   0:000> dds 12fdb4
 *   0012fdb4  0012fe2c
 *   0012fdb8  77f3b744 KERNEL32!_except_handler3
 *   0012fdbc  77f3d308 KERNEL32!ntdll_NULL_THUNK_DATA+0xebc
 *
 *   and the handler is the instr after the push immed:
 *   0:000> dds 77f3d308
 *   77f3d308  ffffffff
 *   77f3d30c  00000000
 *   77f3d310  77f1d9e4 KERNEL32!TlsFree+0xb7
 *
 * We allow a ret to target an address that is pushed as an immediate
 * immediately prior to the handler at the current trylevel in the scopetable
 */
static bool
at_SEH_rct_exception(dcontext_t *dcontext, app_pc target_pc)
{
    TEB *teb = get_own_teb();
    vc_exception_registration_t *vcex;
    scopetable_entry_t *ste;
    int trylevel;
    app_pc pc, modbase;
    bool result = false;
    /* first, we only allow this in a text section */
    modbase = get_module_base(target_pc);
    if (modbase == NULL || !is_in_code_section(modbase, target_pc, NULL, NULL))
        return false;
    /* now read SEH data structs, being careful not to fault */
    if (!is_readable_without_exception((app_pc)teb->ExceptionList,
                                       /* make sure can read extra fields */
                                       sizeof(vc_exception_registration_t)))
        return false;
    vcex = (vc_exception_registration_t *)teb->ExceptionList;
    trylevel = vcex->trylevel;
    /* sanity check: array offset by -1, don't go too far */
    if (trylevel < -1 || trylevel > MAX_SEH_TRYLEVEL)
        return false;
    /* be even more careful: may not be compiled by VC! */
    if (!is_readable_without_exception((app_pc)vcex->scopetable,
                                       (1 + trylevel) * sizeof(scopetable_entry_t)))
        return false;
    ste = (scopetable_entry_t *)vcex->scopetable;
    /* -1 becomes 0 */
    ste += (trylevel + 1);
    pc = (app_pc)ste->lpfnHandler;
    if (!is_readable_without_exception(pc - INSTR_PUSH_IMMED32_LENGTH,
                                       INSTR_PUSH_IMMED32_LENGTH))
        return false;
    LOG(GLOBAL, LOG_INTERP, 3,
        "RCT: at_SEH_rct_exception: testing " PFX " for push $" PFX "\n",
        pc - INSTR_PUSH_IMMED32_LENGTH, target_pc);
    /* not worth risk of decoding -- we check raw bytes */
    if (*(pc - INSTR_PUSH_IMMED32_LENGTH) == INSTR_PUSH_IMMED32_OPCODE &&
        *((app_pc *)(pc - INSTR_PUSH_IMMED32_LENGTH + 1)) == target_pc) {
        STATS_INC(ret_after_call_SEH);
        SYSLOG_INTERNAL_INFO_ONCE("RCT: SEH matched @" PFX, target_pc);
        ASSERT_CURIOSITY(ste->previousTryLevel == (DWORD)trylevel);
        return true;
    }
    return false;
}

/* Whether we've seen any Borland SEH constructs or not, set by
 * -process_SEH_push in interp.c and used to enable
 * at_Borland_SEH_rct_exemption() which covers the rct exemptions not covered
 * already in interp.c.  We could make this tighter and keep track in which
 * modules we've seen Borland SEH constructs and only allow the additional
 * exemption in those, FIXME prob. overkill. */
bool seen_Borland_SEH = false;

/* The interp.c -process_SEH_push -borland_SEH_rct processing covers 99.9% of
 * the Borland SEH rct problems I've seen.  See notes there for more general
 * explanation of the Borland SEH constructs.  However, I've seen (at least
 * in one spot in one dll) an optimization that isn't covered by watching the
 * frame pushes (you'd think with all the clever, and problematic for us,
 * optimization done, they'd do something about screwing up the rsb predictor
 * on the hot [fall through] path of try finally blocks, oh well).  In what
 * looks like part of the exception handler itself, there appears to be an
 * optimization that rather than pop then push an SEH frame, it modifies the
 * SEH frame directly on the stack (I've only seen it do the top frame, but
 * perhaps it sometimes needs to do a deeper frame in which case this would
 * make more sense).  The x: y: pattern (see interp.c) still holds so we can
 * match on that (even though isn't too much to match on) and use
 * seen_Borland_SEH to limit when we open this hole. (Note that I saw this
 * exemption in spybot, but only with the case 8123 bug, hopefully not missing
 * any other rarely hit corner cases)
 */
static bool
at_Borland_SEH_rct_exemption(dcontext_t *dcontext, app_pc target_pc)
{
    /* See pattern in interp.c -borland_SEH_rct (try except ind branch to y).
     * If we've seen_Borland_SEH already and target_pc is in a module code
     * section and target_pc - JMP_LONG_LENGTH is on the .E/.F table already
     * and is a jmp rel32 whose target is in a code section of the same module
     * then allow.  FIXME - if we can reliably recreate the src cti could also
     * check that it is in a code section of the same module.  FIXME - if we
     * give up the seen_Borland_SEH flag restriction this could cover all try
     * except Borland SEH rct violations (if we feel the above checks are
     * strong enough standalone), however we can't cover the try finally
     * exemptions here unless we can accurately recreate the src cti to have a
     * starting point for pattern matching.  There are some impediments in our
     * system to being able to accurately get the src cti at this point, though
     * if we do that may be preferable to the process_SEH code in interp.c
     * (lazy  rather then upfront processing and less impact to non-Borland
     * apps).  Is also not clear how we could reactively cover the call to a:
     * case in the try finally construct as there is very little to match there
     * (target follows a push imm of an in module addr).  Still I've never
     * actually triggered a call to a (though I've seen it compute the address.
     * FIXME revist handling the Borland SEH execmptions reactively
     * instead of in interp.
     */
    app_pc base, jmp_target, jmp_loc = target_pc - JMP_LONG_LENGTH;
    byte buf[JMP_LONG_LENGTH];

    if (!seen_Borland_SEH ||
        /* see above this routine is only for a certain .E/.F violation */
        (DYNAMO_OPTION(rct_ind_jump) == OPTION_DISABLED &&
         DYNAMO_OPTION(rct_ind_call) == OPTION_DISABLED)) {
        return false;
    }

    base = get_module_base(target_pc);
    if (base != NULL &&
        /* even without rct_analyze_at_load should have processed this
         * module by now (before we hit the exemption code) */
        rct_ind_branch_target_lookup(dcontext, jmp_loc) != NULL &&
        is_in_code_section(base, target_pc, NULL, NULL) &&
        d_r_safe_read(jmp_loc, sizeof(buf), &buf) &&
        is_jmp_rel32(buf, jmp_loc, &jmp_target) &&
        /* perf opt, we use get_allocation_base instead of get_module_base
         * since already checking if matches a known module base (base) */
        get_allocation_base(jmp_target) == base &&
        is_in_code_section(base, jmp_target, NULL, NULL)) {
        /* we have a match */
        return true;
    }
    return false;
}

static bool
at_rct_exempt_module(dcontext_t *dcontext, app_pc target_pc, app_pc source_fragment)
{
    const char *target_module_name;
    const char *source_module_name;
    list_default_or_append_t onlist;
    os_get_module_info_lock();
    os_get_module_name(target_pc, &target_module_name);
    os_get_module_name(source_fragment, &source_module_name);

    LOG(THREAD, LOG_INTERP, 2, "at_rct_exempt_module: target_pc=" PFX " module_name=%s\n",
        target_pc, target_module_name != NULL ? target_module_name : "<none>");

    if (source_module_name != NULL &&
        (!IS_STRING_OPTION_EMPTY(exempt_rct_list) ||
         !IS_STRING_OPTION_EMPTY(exempt_rct_default_list))) {
        /* note check_list_default_and_append will grab string_option_read_lock */
        onlist = check_list_default_and_append(dynamo_options.exempt_rct_default_list,
                                               dynamo_options.exempt_rct_list,
                                               source_module_name);
        if (onlist != LIST_NO_MATCH) {
            LOG(THREAD, LOG_INTERP, 1,
                "at_rct_exempt_module: source_fragment=" PFX " same=%d is_dyngen=%d\n",
                source_fragment, in_same_module(target_pc, source_fragment),
                is_dyngen_code(target_pc));
            if (in_same_module(target_pc, source_fragment) || is_dyngen_code(target_pc)) {
                LOG(THREAD, LOG_INTERP, 1, "RCT: exception in exempt module %s --ok\n",
                    source_module_name);
                STATS_INC(ret_after_call_exempt_exceptions); /* also counted as known */
                os_get_module_info_unlock();
                if (onlist == LIST_ON_APPEND) /* case 9799: not if on default */
                    mark_module_exempted(target_pc);
                return true;
            }
        }
    }

    if (target_module_name != NULL &&
        (!IS_STRING_OPTION_EMPTY(exempt_rct_to_default_list) ||
         !IS_STRING_OPTION_EMPTY(exempt_rct_to_list) ||
         !moduledb_exempt_list_empty(MODULEDB_EXEMPT_RCT))) {
        /* note check_list_default_and_append will grab string_option_read_lock */
        onlist = check_list_default_and_append(dynamo_options.exempt_rct_to_default_list,
                                               dynamo_options.exempt_rct_to_list,
                                               target_module_name);
        if (onlist != LIST_NO_MATCH) {
            LOG(THREAD, LOG_INTERP, 1, "RCT: exception to exempt target module %s --ok\n",
                target_module_name);
            STATS_INC(ret_after_call_exempt_exceptions); /* also counted as known */
            os_get_module_info_unlock();
            if (onlist == LIST_ON_APPEND) /* case 9799: not if on default */
                mark_module_exempted(target_pc);
            return true;
        } else if (!moduledb_exempt_list_empty(MODULEDB_EXEMPT_RCT) &&
                   moduledb_check_exempt_list(MODULEDB_EXEMPT_RCT, target_module_name)) {
            LOG(THREAD, LOG_MODULEDB | LOG_INTERP, 1,
                "RCT: exemption for moduledb exempted target module %s --ok\n",
                target_module_name);
            STATS_INC(num_rct_moduledb_exempt);
            moduledb_report_exemption("Moduledb rct exemption from " PFX " to " PFX
                                      " in %s",
                                      target_pc, source_fragment, target_module_name);
            os_get_module_info_unlock();
            /* case 9799: do not mark as exempted for default-on option */
            return true;
        }
    }

    if (source_module_name != NULL &&
        (!IS_STRING_OPTION_EMPTY(exempt_rct_from_default_list) ||
         !IS_STRING_OPTION_EMPTY(exempt_rct_from_list))) {
        LOG(THREAD, LOG_INTERP, 2,
            "at_rct_exempt_module: source_fragment=" PFX " module_name=%s\n",
            source_fragment, source_module_name != NULL ? source_module_name : "<none>");
        /* note check_list_default_and_append will grab string_option_read_lock */
        if (source_module_name != NULL &&
            (onlist = check_list_default_and_append(
                 dynamo_options.exempt_rct_from_default_list,
                 dynamo_options.exempt_rct_from_list, source_module_name)) !=
                LIST_NO_MATCH) {
            LOG(THREAD, LOG_INTERP, 1,
                "RCT: exception from exempt source module %s --ok\n", source_module_name);
            STATS_INC(ret_after_call_exempt_exceptions); /* also counted as known */
            os_get_module_info_unlock();
            if (onlist == LIST_ON_APPEND) /* case 9799: not if on default */
                mark_module_exempted(target_pc);
            return true;
        }
    }

    os_get_module_info_unlock();
    return false;
}

/* FIXME - this currently used for both .C and .E/.F violations, but almost all
 * the specific exemptions are particular to one or the other.  We should
 * really split this routine. Xref case 8170.
 */
bool
at_known_exception(dcontext_t *dcontext, app_pc target_pc, app_pc source_fragment)
{
    /* check for known exception with fibers on Windows2003 */
    if (DYNAMO_OPTION(fiber_rct) && at_fiber_init_known_exception(dcontext, target_pc)) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on fiber init --ok\n");
        return true;
    }

    /* check for known exception with SEH on Windows NT4 */
    if (DYNAMO_OPTION(seh_rct) && at_SEH_rct_exception(dcontext, target_pc)) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on SEH target --ok\n");
        return true;
    }

    /* check for additional Borland SEH exemptions */
    if (DYNAMO_OPTION(borland_SEH_rct) &&
        at_Borland_SEH_rct_exemption(dcontext, target_pc)) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: at known Borland exception --ok\n");
        STATS_INC(num_borland_SEH_modified);
        return true;
    }

    if (DYNAMO_OPTION(xdata_rct) && at_xdata_rct_exception(dcontext, target_pc)) {
        LOG(THREAD, LOG_INTERP, 1, "RCT: known exception on .xdata target --ok\n");
        return true;
    }

    /* check whether withing an exempt module or targeting DGC from a known module */
    if (DYNAMO_OPTION(exempt_rct) &&
        at_rct_exempt_module(dcontext, target_pc, source_fragment)) {
        DODEBUG({
            const char *name;
            os_get_module_info_lock();
            os_get_module_name(target_pc, &name);
            SYSLOG_INTERNAL_WARNING_ONCE("RCT: target_pc " PFX " exempt in module %s",
                                         target_pc, name == NULL ? "<null>" : name);
            os_get_module_info_unlock();
        });
        return true;
    }

    if (DYNAMO_OPTION(driver_rct) && at_driver_rct_exception(dcontext, source_fragment)) {
        /* FIXME: we need to ensure that we do not form traces, so that the
         * fragile source tag is still from an exit from the driver */
        LOG(THREAD, LOG_INTERP, 1, "RCT: known exception from driver area --ok\n");

        return true;
    }

    return false;
}

#endif /* RETURN_AFTER_CALL */

void
callback_init()
{
    ASSERT(INVALID_THREAD_ID == 0); /* for threads_waiting_for_dr_init[] */
}

void
callback_exit()
{
    DELETE_LOCK(emulate_write_lock);
    DELETE_LOCK(map_intercept_pc_lock);
    DELETE_LOCK(exception_stack_lock);
    DELETE_LOCK(intercept_hook_lock);
}

dr_marker_t *
get_drmarker(void)
{
    return (dr_marker_t *)interception_code;
}

#ifdef HOT_PATCHING_INTERFACE
/* This function provides an interface to hook any instruction in a loaded
 * module.  For now, the consumer is hotp_only.
 */
byte *
hook_text(byte *hook_code_buf, const app_pc image_addr, intercept_function_t hook_func,
          const void *callee_arg, const after_intercept_action_t action_after,
          const bool abort_if_hooked, const bool ignore_cti, byte **app_code_copy_p,
          byte **alt_exit_tgt_p)
{
    byte *res;
    ASSERT(DYNAMO_OPTION(hotp_only));
    ASSERT(hook_code_buf != NULL && image_addr != NULL && hook_func != NULL);

    /* Currently hotp_only is the only user for this.  However if any other
     * module wants to use this, we better find out if they are trying to hook
     * something other than the .text section; note, it will still hook if it
     * isn't the text section.
     */
    ASSERT_CURIOSITY(
        is_in_code_section(get_module_base(image_addr), image_addr, NULL, NULL));

    res = intercept_call(hook_code_buf, image_addr, hook_func, (void *)callee_arg,
                         /* use dr stack now, later on hotp stack - FIXME */
                         false, action_after, abort_if_hooked, ignore_cti,
                         app_code_copy_p, alt_exit_tgt_p);

    /* Hooking can only fail if there was a cti at the patch region.  There
     * better not be any there!
     */
    ASSERT(res != NULL);

    /* If app_code_copy_p isn't null, then *app_code_copy_p can't be null;
     * same for alt_exit_tgt_p if after_action is dynamic decision.
     */
    ASSERT(app_code_copy_p == NULL || *app_code_copy_p != NULL);
    ASSERT(action_after != AFTER_INTERCEPT_DYNAMIC_DECISION || alt_exit_tgt_p == NULL ||
           *app_code_copy_p != NULL);
    return res;
}

/* Just a wrapper to export unhook_text; may evolve in future. */
void
unhook_text(byte *hook_code_buf, app_pc image_addr)
{
    un_intercept_call(hook_code_buf, image_addr);
}

/* Introduced as part of fix for case 9593, which required leaking trampolines. */
void
insert_jmp_at_tramp_entry(dcontext_t *dcontext, byte *trampoline, byte *target)
{
    ASSERT(trampoline != NULL && target != NULL);

    /* Note: first 5 bytes of the trampoline contain the copy of app code which
     * was overwritten with the hook;  so, entry point is 5 bytes after that.
     */
    *(trampoline + 5) = JMP_REL32_OPCODE;
    patch_branch(dr_get_isa_mode(dcontext), trampoline + 5, target,
                 false /* Don't have to hot_patch. */);
}
#endif

#ifdef X86
/* Returns POINTER_MAX on failure.
 * Assumes that cs, ss, ds, and es are flat.
 */
byte *
get_segment_base(uint seg)
{
    if (seg == SEG_TLS)
        return (byte *)get_own_teb();
    else if (seg == SEG_CS || seg == SEG_SS || seg == SEG_DS || seg == SEG_ES)
        return NULL;
    else
        return (byte *)POINTER_MAX;
}

/* i#572: handle opnd_compute_address to return the application
 * segment base value.
 */
byte *
get_app_segment_base(uint seg)
{
    return get_segment_base(seg);
}
#endif

static after_intercept_action_t /* note return value will be ignored */
thread_attach_takeover_callee(app_state_at_intercept_t *state)
{
    /* transfer_to_dispatch() will swap from d_r_initstack to dstack and clear
     * the initstack_mutex.
     */
    thread_attach_setup(&state->mc);
    ASSERT(standalone_library);
    ASSERT_NOT_REACHED(); /* We cannot recover: there's no PC to go back to. */
    return AFTER_INTERCEPT_LET_GO;
}

static byte *
emit_takeover_code(byte *pc)
{
    thread_attach_takeover = pc;
    pc = emit_intercept_code(
        GLOBAL_DCONTEXT, pc, thread_attach_takeover_callee, 0, /* no arg */
        false /* do not assume esp */,
        true /* assume not on dstack, and don't clobber flags */,
        AFTER_INTERCEPT_LET_GO /* won't return anyway */, NULL, NULL);
    return pc;
}
