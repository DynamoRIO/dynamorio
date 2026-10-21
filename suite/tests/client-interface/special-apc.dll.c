/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
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
#include <xmmintrin.h>

typedef BOOL(WINAPI *queue_apc_t)(PAPCFUNC, HANDLE, ULONG_PTR, ULONG);
static queue_apc_t queue_apc;
static app_pc interrupt_client;
static volatile LONG callbacks;
static bool checked;

static void CALLBACK
apc(ULONG_PTR nested)
{
    _mm_setcsr(0x1f80);
    InterlockedIncrement(&callbacks);
    if (nested)
        ASSERT(queue_apc(apc, GetCurrentThread(), 0, 1));
}

static void
interrupt_runtime_stack(void)
{
    // A clean call runs on the runtime stack, with live client frames below the APC.
    volatile ptr_uint_t canary = (ptr_uint_t)0x12345678;
    uint saved_mxcsr = _mm_getcsr();
    _mm_setcsr(0x7f80);
    ASSERT(!dr_using_app_state(dr_get_current_drcontext()));
    for (int i = 0; i < 32; ++i) {
        ASSERT(queue_apc(apc, GetCurrentThread(), 1, 1));
        ASSERT(callbacks == 2 * (i + 1));
        ASSERT(canary == (ptr_uint_t)0x12345678);
        ASSERT(_mm_getcsr() == 0x7f80);
    }
    _mm_setcsr(saved_mxcsr);
    checked = true;
}

static dr_emit_flags_t
event_bb(void *dc, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (dr_fragment_app_pc(tag) == interrupt_client)
        dr_insert_clean_call(dc, bb, instrlist_first(bb), interrupt_runtime_stack, false,
                             0);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    ASSERT(checked && callbacks == 64);
    dr_fprintf(STDERR, "nested special APCs preserved runtime frames\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    module_data_t *app = dr_get_main_module();
    queue_apc =
        (queue_apc_t)GetProcAddress(GetModuleHandle("kernelbase.dll"), "QueueUserAPC2");
    if (queue_apc == NULL) {
        dr_free_module_data(app);
        dr_fprintf(STDERR, "special APCs unavailable\n");
        return;
    }
    interrupt_client = (app_pc)dr_get_proc_address(app->handle, "interrupt_client");
    ASSERT(interrupt_client != NULL);
    dr_free_module_data(app);
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
}
