/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include <string.h>

/* Tests instrumenting system calls.  Also tests module_iterator interface and
 * dr_get_proc_address() and dr_mcontext_to_context(). */

#define MINSERT instrlist_meta_preinsert

static app_pc start_pc = NULL;
static app_pc stop_pc = NULL;
static bool monitoring = false;

static void
at_syscall()
{
    if (monitoring) {
        dr_mcontext_t mcontext;
        mcontext.size = sizeof(mcontext);
        mcontext.flags = DR_MC_ALL;
        void *drcontext = dr_get_current_drcontext();
        dr_get_mcontext(drcontext, &mcontext);
        dr_fprintf(STDERR, PFX "\n", mcontext.xax);

        {
            /* Sanity checks for reg_get_value_ex() */
            byte val[sizeof(dr_zmm_t)];
            if (!reg_get_value_ex(DR_REG_XMM0, &mcontext, val) ||
                memcmp(val, &mcontext.simd[0], sizeof(dr_xmm_t)) != 0)
                dr_fprintf(STDERR, "reg_get_value_ex xmm0 mismatch\n");
            if (!reg_get_value_ex(DR_REG_YMM0, &mcontext, val) ||
                memcmp(val, &mcontext.simd[0], sizeof(dr_ymm_t)) != 0)
                dr_fprintf(STDERR, "reg_get_value_ex ymm0 mismatch\n");
            if (!reg_get_value_ex(DR_REG_XBP, &mcontext, val) ||
                *(reg_t *)val != reg_get_value(DR_REG_XBP, &mcontext))
                dr_fprintf(STDERR, "reg_get_value_ex xbp mismatch\n");
        }

#ifdef WINDOWS
        {
            /* Test dr_mcontext_to_context */
            CONTEXT cxt;
            if (!dr_mcontext_to_context(&cxt, &mcontext) ||
                (app_pc)cxt.IF_X64_ELSE(Rip, Eip) != mcontext.pc ||
                (reg_t)cxt.IF_X64_ELSE(Rax, Eax) != mcontext.xax ||
                (reg_t)cxt.IF_X64_ELSE(Rcx, Ecx) != mcontext.xcx ||
                (reg_t)cxt.IF_X64_ELSE(Rdx, Edx) != mcontext.xdx ||
                (reg_t)cxt.IF_X64_ELSE(Rbx, Ebx) != mcontext.xbx ||
                (reg_t)cxt.IF_X64_ELSE(Rsp, Esp) != mcontext.xsp ||
                (reg_t)cxt.IF_X64_ELSE(Rbp, Ebp) != mcontext.xbp ||
                (reg_t)cxt.IF_X64_ELSE(Rsi, Esi) != mcontext.xsi ||
                (reg_t)cxt.IF_X64_ELSE(Rdi, Edi) != mcontext.xdi)
                dr_fprintf(STDERR, "dr_mcontext_to_context failed\n");
        }
#endif
    }
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    app_pc pc = dr_fragment_app_pc(tag);

    if (pc == start_pc) {
        dr_fprintf(STDERR, "starting syscall monitoring\n");
        monitoring = true;
    } else if (pc == stop_pc) {
        dr_fprintf(STDERR, "stopping syscall monitoring\n");
        monitoring = false;
    } else {
        instr_t *instr;
        instr_t *next_instr;

        for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {

            next_instr = instr_get_next(instr);

            /* Insert a callback to at_syscall before every system call */
            if (instr_is_syscall(instr)) {
                dr_insert_clean_call(drcontext, bb, instr, at_syscall, false, 0);
            }
        }
    }
    return DR_EMIT_DEFAULT;
}

#ifdef WINDOWS
#    define TEST_NAME "client.syscall.exe"
#else
#    define TEST_NAME "client.syscall"
#endif

#ifdef UNIX
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    uint64 early_inject;

    /* do some more dr_get_proc_address testing */
    if (strncmp(dr_module_preferred_name(info), "libc.", 5) == 0) {
        module_handle_t lib = info->handle;
        dr_export_info_t fn_info;
        dr_fprintf(STDERR, "found libc\n");
        if (dr_get_proc_address(lib, "malloc") == NULL)
            dr_fprintf(STDERR, "ERROR: can't find malloc in libc\n");
        if (dr_get_proc_address(lib, "free") == NULL)
            dr_fprintf(STDERR, "ERROR: can't find free in libc\n");
        if (dr_get_proc_address(lib, "printf") == NULL)
            dr_fprintf(STDERR, "ERROR: can't find printf in libc\n");

        /* i#884: gettimeofday is indirect code on my system, and calling it
         * will crash unless we wait until libc is fully relocated.
         * dr_get_proc_address() wraps the fault in a try/except and returns
         * NULL.  The _ex variant does not, so we use that to test the lookup.
         */
        if (!dr_get_proc_address_ex(lib, "gettimeofday", &fn_info, sizeof(fn_info)))
            dr_fprintf(STDERR, "ERROR: can't find gettimeofday in libc\n");

        dr_unregister_module_load_event(event_module_load);
    }
}
#endif

DR_EXPORT void
dr_init(client_id_t id)
{
    /* Look up start_monitor() and stop_monitor() in the target app.
     * These functions are dummy markers that tell us when to start
     * and stop printing syscalls.
     */
    /* NOTE - we could use dr_module_lookup_by_name, but we use the iterator instead
     * to test it out. */
    dr_module_iterator_t *iter = dr_module_iterator_start();
    while (dr_module_iterator_hasnext(iter)) {
        module_data_t *data = dr_module_iterator_next(iter);
        if (strcmp(dr_module_preferred_name(data), TEST_NAME) == 0) {
            module_handle_t lib = data->handle;
            start_pc = (app_pc)dr_get_proc_address(lib, "start_monitor");
            stop_pc = (app_pc)dr_get_proc_address(lib, "stop_monitor");
        }
        dr_free_module_data(data);
    }
    dr_module_iterator_stop(iter);

    if (start_pc == NULL || stop_pc == NULL) {
        dr_fprintf(STDERR, "ERROR: did not find start/stop markers\n");
    }

    /* Register the BB hook */
    dr_register_bb_event(bb_event);
#ifdef UNIX /* With early injection, libc won't be loaded until later. */
    dr_register_module_load_event(event_module_load);
#endif
}
