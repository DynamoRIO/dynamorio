/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

/* WARNING i#262: if you use the cmake binary package, ctest is built
 * without a GNU_STACK section, which causes the linux kernel to set
 * the READ_IMPLIES_EXEC personality flag, which is propagated to
 * children and causes all mmaps to be +x, breaking all these tests
 * that check for mmapped memory to be +rw or +r!
 */
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

#ifdef X64
# define PREFERRED_ADDR (char *)0x1000000000
#else
# define PREFERRED_ADDR (char *)0x2000000
#endif
static
void raw_alloc_test(void)
{
    uint prot;
    char *array = PREFERRED_ADDR;
    dr_mem_info_t info;
    bool res;
    dr_fprintf(STDERR, "  testing raw memory alloc...");
    res = dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                           array) != NULL;
    if (!res) {
        dr_fprintf(STDERR, "[error: fail to alloc at "PFX"]\n", array);
        return;
    }
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != (DR_MEMPROT_READ|DR_MEMPROT_WRITE))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rw]\n", prot);
    dr_raw_mem_free(array, PAGE_SIZE);
    dr_query_memory_ex((const byte *)array, &info);
    if (info.prot != DR_MEMPROT_NONE)
        dr_fprintf(STDERR, "[error: prot %d doesn't match none]\n", info.prot);
    dr_fprintf(STDERR, "success\n");
}

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
    raw_alloc_test();
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

    /* Match nop,nop,nop,ret in client's +w code */
    if (instr_get_opcode(instrlist_first(bb)) == OP_nop) {
        instr_t *nxt = instr_get_next(instrlist_first(bb));
        if (nxt != NULL && instr_get_opcode(nxt) == OP_nop) {
            nxt = instr_get_next(nxt);
            if (nxt != NULL && instr_get_opcode(nxt) == OP_nop) {
                nxt = instr_get_next(nxt);
                if (nxt != NULL && instr_get_opcode(nxt) == OP_ret) {
                    uint prot;
                    dr_mem_info_t info;
                    app_pc pc = dr_fragment_app_pc(tag);
#ifdef WINDOWS
                    MEMORY_BASIC_INFORMATION mbi;
#endif
                    dr_fprintf(STDERR, "  testing query pretend-write....");

                    if (!dr_query_memory(pc, NULL, NULL, &prot))
                        dr_fprintf(STDERR, "error: unable to query code prot\n");
                    if (!TESTALL(DR_MEMPROT_WRITE | DR_MEMPROT_PRETEND_WRITE, prot))
                        dr_fprintf(STDERR, "error: not pretend-writable\n");

                    if (!dr_query_memory_ex(pc, &info))
                        dr_fprintf(STDERR, "error: unable to query code prot\n");
                    if (!TESTALL(DR_MEMPROT_WRITE | DR_MEMPROT_PRETEND_WRITE, info.prot))
                        dr_fprintf(STDERR, "error: not pretend-writable\n");

#ifdef WINDOWS
                    if (dr_virtual_query(pc, &mbi, sizeof(mbi)) != sizeof(mbi))
                        dr_fprintf(STDERR, "error: unable to query code prot\n");
                    if (mbi.Protect != PAGE_EXECUTE_READWRITE)
                        dr_fprintf(STDERR, "error: not pretend-writable\n");
#endif
                    dr_fprintf(STDERR, "success\n");
                }
            }
        }
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
