/* **********************************************************
 * Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
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

char *global;
#define SIZE 10
#define VAL 17

static
void write_array(char *array)
{
    int i;
    for (i=0; i<SIZE; i++)
        array[i] = VAL;
}

static
void global_test(void)
{
    char *array;
    uint prot;
    dr_fprintf(STDERR, "  testing global memory alloc...");
    array = dr_global_alloc(SIZE);
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != (DR_MEMPROT_READ|DR_MEMPROT_WRITE))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rw] ", prot);
    dr_global_free(array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

/* FIXME: 32-bit apps on my 64-bit linux machine end up with +x for
 * all of these regardless of what's allocated up front or what's
 * mprotected: DR is doing the right thing, the OS is adding it.
 */
static
void nonheap_test(void)
{
    uint prot;
    char *array =
        dr_nonheap_alloc(SIZE, DR_MEMPROT_READ|DR_MEMPROT_WRITE|DR_MEMPROT_EXEC);
    dr_fprintf(STDERR, "  testing nonheap memory alloc...");
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != (DR_MEMPROT_READ|DR_MEMPROT_WRITE|DR_MEMPROT_EXEC))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rwx] ", prot);
    dr_memory_protect(array, SIZE, DR_MEMPROT_NONE);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != DR_MEMPROT_NONE)
        dr_fprintf(STDERR, "[error: prot %d doesn't match r] ", prot);
    dr_memory_protect(array, SIZE, DR_MEMPROT_READ);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != DR_MEMPROT_READ)
        dr_fprintf(STDERR, "[error: prot %d doesn't match r] ", prot);
    if (dr_safe_write(array, 1, (const void *) &prot, NULL))
        dr_fprintf(STDERR, "[error: should not be writable] ");
    dr_nonheap_free(array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

static
void local_test(void *drcontext)
{
    char *array;
    dr_fprintf(STDERR, "  testing local memory alloc....");
    array = dr_thread_alloc(drcontext, SIZE);
    write_array(array);
    dr_thread_free(drcontext, array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

static
void thread_init_event(void *drcontext)
{
    static bool tested = false;
    if (!tested) {
        dr_fprintf(STDERR, "thread initialization:\n");
        local_test(drcontext);
        global_test();
        tested = true;
    }
}

static
void inline_alloc_test(void)
{
    dr_fprintf(STDERR, "code cache:\n");
    local_test(dr_get_current_drcontext());
    global_test();
    nonheap_test();
}

#define MINSERT instrlist_meta_preinsert

static
dr_emit_flags_t bb_event(void* drcontext, void *tag, instrlist_t* bb, bool for_trace, bool translating)
{
    static bool inserted = false;
    if (!inserted) {
        instr_t* instr = instrlist_first(bb);

        dr_prepare_for_call(drcontext, bb, instr);

        MINSERT(bb, instr, INSTR_CREATE_call
                (drcontext, opnd_create_pc((void*)inline_alloc_test)));
        
        dr_cleanup_after_call(drcontext, bb, instr, 0);

        inserted = true;
    }

    /* store, since we're not deterministic */
    return DR_EMIT_STORE_TRANSLATIONS;
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    global_test();
    nonheap_test();

    dr_register_bb_event(bb_event);
    dr_register_thread_init_event(thread_init_event);
}
