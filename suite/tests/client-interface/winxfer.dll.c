/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

/*
 * Tests API interactions with Windows kernel-mediated events.
 */

#include "dr_api.h"
#include "client_tools.h"

#if 0 /* FIXME i#241 */
static void
redirect_xfer(void)
{
    dr_printf("redirected!\n");
}
#endif

static void
kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    dr_fprintf(STDERR, "%s: type %d\n", __FUNCTION__, info->type);
    dr_log(drcontext, DR_LOG_ALL, 2, "%s: %d %p to %p sp=%zx\n", __FUNCTION__, info->type,
           info->source_mcontext == NULL ? 0 : info->source_mcontext->pc, info->target_pc,
           info->target_xsp);
    dr_mcontext_t mc = { sizeof(mc) };
    mc.flags = DR_MC_CONTROL;
    bool ok = dr_get_mcontext(drcontext, &mc);
    ASSERT(ok);
    ASSERT(mc.pc == info->target_pc);
    ASSERT(mc.xsp == info->target_xsp);
    mc.flags = DR_MC_ALL;
    ok = dr_get_mcontext(drcontext, &mc);
    ASSERT(ok);
#if 0 /* FIXME i#241 */
    /* FIXME i#241: test dr_set_mcontext.  It's not easy though: it doesn't make much
     * sense for the Ki dispatchers, there's no NtContinue in SEH64, it's not
     * supported for cbret, and we don't have a test here for NtSetContextThread.  I
     * manually tested by sending to redirect_cbret(), confirmed a print, and then
     * the app crashes.  Making it continue is more work than I want to put in right
     * now.  Since the uses cases are few, we may want to consider disallowing
     * setting the context?
     */
    if (type == DR_XFER_CALLBACK_DISPATCHER) {
        mc.pc = (app_pc)redirect_xfer;
        ok = dr_set_mcontext(drcontext, &mc);
        ASSERT(ok);
    }
#endif
}

static bool
exception_event(void *dcontext, dr_exception_t *excpt)
{
    void *fault_address = (void *)excpt->record->ExceptionInformation[1];
    dr_fprintf(STDERR, "exception %x addr " PFX "\n", excpt->record->ExceptionCode,
               (ptr_uint_t)excpt->record->ExceptionInformation[1]);
    return true;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_kernel_xfer_event(kernel_xfer_event);
    dr_register_exception_event(exception_event);
}
