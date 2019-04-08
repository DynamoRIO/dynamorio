/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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
#include "client_tools.h"

static void **expected_fault_address;

#if defined(UNIX)
#    include <signal.h>

static dr_signal_action_t
signal_event(void *dcontext, dr_siginfo_t *info)
{
    if (info->sig == SIGABRT) {
        /* do nothing */
    } else if (info->sig == SIGSEGV && info->access_address == *expected_fault_address) {
        dr_fprintf(STDERR, "dr handler ok\n");
    } else {
        dr_fprintf(STDERR,
                   "dr handler got signal %d with addr " PFX
                   ", but expected signal %d with addr " PFX "\n",
                   info->sig, (ptr_uint_t)info->access_address, SIGSEGV,
                   (ptr_uint_t)*expected_fault_address);
    }
    return DR_SIGNAL_DELIVER;
}
#elif defined(WINDOWS)

static bool
exception_event(void *dcontext, dr_exception_t *excpt)
{
    void *fault_address = (void *)excpt->record->ExceptionInformation[1];
    if (excpt->record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        fault_address == *expected_fault_address) {
        dr_fprintf(STDERR, "dr handler ok\n");
    } else {
        dr_fprintf(STDERR,
                   "dr handler got exception %x with addr " PFX
                   ", but expected exception %x with addr " PFX "\n",
                   excpt->record->ExceptionCode,
                   (ptr_uint_t)excpt->record->ExceptionInformation[1],
                   EXCEPTION_ACCESS_VIOLATION, (ptr_uint_t)*expected_fault_address);
    }
    return true;
}
#endif

static void
exit_event(void)
{
    dr_fprintf(STDERR, "dr exit handler aborting\n");
    /* we want to also test end-to-end w/ core dump being generated but
     * that's hard to do in a test suite so we abort here: but that can
     * mask errors.
     */
    dr_abort();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    module_data_t *module;

    dr_register_exit_event(exit_event);
#if defined(UNIX)
    dr_register_signal_event(signal_event);
#elif defined(WINDOWS)
    dr_register_exception_event(exception_event);
#endif

    module = dr_get_main_module();
    ASSERT(module != NULL);
    expected_fault_address =
        (void **)dr_get_proc_address(module->handle, "expected_fault_address");
    ASSERT(expected_fault_address != NULL);
    dr_free_module_data(module);
}
