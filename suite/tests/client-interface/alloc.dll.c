/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
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

#define _GNU_SOURCE /* MREMAP_MAYMOVE */

#include "dr_api.h"
#include "client_tools.h"
#ifdef LINUX
#    include <sys/personality.h>
#    include <sys/mman.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>

char *global;
#define SIZE 10
#define VAL 17

static app_pc app_gencode;

/* i#262 add exec if READ_IMPLIES_EXEC is set in personality */
static bool add_exec = false;

static client_id_t client_id;

static void
write_array(char *array)
{
    int i;
    for (i = 0; i < SIZE; i++)
        array[i] = VAL;
}

/* i#262 add exec if READ_IMPLIES_EXEC is set in personality */
static uint
get_os_mem_prot(uint prot)
{
    if (add_exec && TEST(DR_MEMPROT_READ, prot))
        return (prot | DR_MEMPROT_EXEC);
    return prot;
}

/* WARNING i#262: if you use the cmake binary package, ctest is built
 * without a GNU_STACK section, which causes the linux kernel to set
 * the READ_IMPLIES_EXEC personality flag, which is propagated to
 * children and causes all mmaps to be +x, breaking all these tests
 * that check for mmapped memory to be +rw or +r!
 */
static void
global_test(void)
{
    char *array;
    uint prot;
    dr_fprintf(STDERR, "  testing global memory alloc...");
    array = dr_global_alloc(SIZE);
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != get_os_mem_prot(DR_MEMPROT_READ | DR_MEMPROT_WRITE))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rw] ", prot);
    dr_global_free(array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

#ifdef X64
#    define PREFERRED_ADDR (byte *)0x1000000000
#endif

/* Defines the size and alignment of the probe allocation in tests that must probe for
 * a free address (such that their allocation at a preferred address does not fail with
 * a collision). Windows allocations will always be aligned to 64k, so the preferred
 * address must likewise be 64k-aligned--otherwise the resulting allocation will not
 * exactly match the preferred address, causing the test to inadvertently fail. Linux
 * allocations need only be aligned to a page, so better to test unaligned.
 */
#ifdef WINDOWS
#    define HINT_ALLOC_SIZE 0x20000
#    define HINT_OFFSET 0x10000
#else
#    define HINT_ALLOC_SIZE (PAGE_SIZE * 2)
#    define HINT_OFFSET PAGE_SIZE
#endif

#ifdef X64
static void
test_instr_as_immed(void)
{
    void *drcontext = dr_get_current_drcontext();
    instrlist_t *ilist = instrlist_create(drcontext);
    byte *pc;
    instr_t *ins0;
    opnd_t opnd;
    byte *highmem = PREFERRED_ADDR;
    pc = dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC,
                          highmem);
    ASSERT(pc == highmem);

    /* Test push_imm of instr */
    ins0 = INSTR_CREATE_nop(drcontext);
    instrlist_append(ilist, ins0);
    instrlist_insert_push_instr_addr(drcontext, ins0, highmem, ilist, NULL, NULL, NULL);
    instrlist_append(ilist, INSTR_CREATE_pop(drcontext, opnd_create_reg(DR_REG_RAX)));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, highmem, true);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < highmem + PAGE_SIZE);
    pc = ((byte * (*)(void)) highmem)();
    ASSERT(pc == highmem);

    /* Test mov_imm of instr */
    ins0 = INSTR_CREATE_nop(drcontext);
    instrlist_append(ilist, ins0);
    /* Beyond TOS, but a convenient mem dest */
    opnd = opnd_create_base_disp(DR_REG_RSP, DR_REG_NULL, 0, -8, OPSZ_8);
    instrlist_insert_mov_instr_addr(drcontext, ins0, highmem, opnd, ilist, NULL, NULL,
                                    NULL);
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_RAX), opnd));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, highmem, true);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < highmem + PAGE_SIZE);
    pc = ((byte * (*)(void)) highmem)();
    ASSERT(pc == highmem);

    instrlist_clear_and_destroy(drcontext, ilist);
    dr_raw_mem_free(highmem, PAGE_SIZE);
}

static void
reachability_test(void)
{
    void *drcontext = dr_get_current_drcontext();
    instrlist_t *ilist = instrlist_create(drcontext);
    byte *gencode = (byte *)dr_nonheap_alloc(
        PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC);
    byte *pc;
    int res;
    byte *highmem = PREFERRED_ADDR;
    pc = dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC,
                          highmem);
    ASSERT(pc == highmem);

    dr_fprintf(STDERR, "  reachability test...");

    /* Test auto-magically turning rip-rel that won't reach but targets xax
     * into absmem.
     */
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),
                                         opnd_create_rel_addr(highmem, OPSZ_4)));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, gencode, false);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < gencode + PAGE_SIZE);
    *(int *)highmem = 0x12345678;
    res = ((int (*)(void))gencode)();
    ASSERT(res == 0x12345678);

    /* Test auto-magically turning a reachable absmem into a rip-rel. */
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_ECX),
                                         opnd_create_abs_addr(highmem + 0x800, OPSZ_4)));
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),
                                         opnd_create_reg(DR_REG_ECX)));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, highmem, false);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < highmem + PAGE_SIZE);
    *(int *)(highmem + 0x800) = 0x12345678;
    res = ((int (*)(void))highmem)();
    ASSERT(res == 0x12345678);

    dr_raw_mem_free(highmem, PAGE_SIZE);

    /* Test targeting upper 2GB of low 4GB (this will fail with default options
     * of -vm_size 2G and a low vm_base b/c there's no room there).
     */
    highmem =
        dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC,
                         (byte *)(ptr_uint_t)0xabcd0000);
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_ECX),
                                         opnd_create_abs_addr(highmem, OPSZ_4)));
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),
                                         opnd_create_reg(DR_REG_ECX)));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, gencode, false);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < gencode + PAGE_SIZE);
    *(int *)highmem = 0x12345678;
    res = ((int (*)(void))gencode)();
    ASSERT(res == 0x12345678);
    dr_raw_mem_free(highmem, PAGE_SIZE);

    /* Test targeting lower 2GB of low 4GB */
    highmem =
        dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC,
                         (byte *)0x143d0000);
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_ECX),
                                         opnd_create_abs_addr(highmem, OPSZ_4)));
    instrlist_append(ilist,
                     INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),
                                         opnd_create_reg(DR_REG_ECX)));
    instrlist_append(ilist, INSTR_CREATE_ret(drcontext));
    pc = instrlist_encode(drcontext, ilist, gencode, false);
    instrlist_clear(drcontext, ilist);
    ASSERT(pc < gencode + PAGE_SIZE);
    *(int *)highmem = 0x12345678;
    res = ((int (*)(void))gencode)();
    ASSERT(res == 0x12345678);
    dr_raw_mem_free(highmem, PAGE_SIZE);

    instrlist_clear_and_destroy(drcontext, ilist);
    dr_nonheap_free(gencode, PAGE_SIZE);

    test_instr_as_immed();

    dr_fprintf(STDERR, "success\n");
}
#endif

static void
raw_alloc_test(void)
{
    uint prot;
    char *array, *preferred;
    dr_mem_info_t info;
    bool res;
    dr_fprintf(STDERR, "  testing raw memory alloc...");

    /* Find a free region of memory without inadvertently "preloading" it.
     * First probe by allocating 2x the platform allocation alignment unit.
     */
    array = dr_raw_mem_alloc(HINT_ALLOC_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    /* Then select the second half as the preferred address for the allocation test. */
    preferred = (void *)((ptr_uint_t)array + HINT_OFFSET);
    /* Free the probe allocation. */
    dr_raw_mem_free(array, HINT_ALLOC_SIZE);
    array = preferred;

    /* Now `array` is guaranteed to be available. */
    res = dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, array) != NULL;
    if (!res) {
        dr_fprintf(STDERR, "[error: fail to alloc at " PFX "]\n", array);
        return;
    }
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != get_os_mem_prot(DR_MEMPROT_READ | DR_MEMPROT_WRITE))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rw]\n", prot);
    dr_raw_mem_free(array, PAGE_SIZE);
    dr_query_memory_ex((const byte *)array, &info);
    if (info.prot != DR_MEMPROT_NONE)
        dr_fprintf(STDERR, "[error: prot %d doesn't match none]\n", info.prot);
    dr_fprintf(STDERR, "success\n");
}

static void
nonheap_test(void)
{
    uint prot;
    char *array =
        dr_nonheap_alloc(SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC);
    dr_fprintf(STDERR, "  testing nonheap memory alloc...");
    write_array(array);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != get_os_mem_prot((DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC)))
        dr_fprintf(STDERR, "[error: prot %d doesn't match rwx] ", prot);
    dr_memory_protect(array, SIZE, DR_MEMPROT_NONE);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != get_os_mem_prot(DR_MEMPROT_NONE))
        dr_fprintf(STDERR, "[error: prot %d doesn't match none] ", prot);
    dr_memory_protect(array, SIZE, DR_MEMPROT_READ);
    dr_query_memory((const byte *)array, NULL, NULL, &prot);
    if (prot != get_os_mem_prot(DR_MEMPROT_READ))
        dr_fprintf(STDERR, "[error: prot %d doesn't match r] ", prot);
    if (dr_safe_write(array, 1, (const void *)&prot, NULL))
        dr_fprintf(STDERR, "[error: should not be writable] ");
    dr_nonheap_free(array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

static bool
reachable_from_client(void *addr)
{
    ssize_t diff = (byte *)addr - dr_get_client_base(client_id);
    return (diff <= INT_MAX && diff >= INT_MIN);
}

static void
custom_test(void)
{
    void *drcontext = dr_get_current_drcontext();
    void *array, *preferred;
    size_t size;
    uint prot;

    dr_fprintf(STDERR, "  testing custom memory alloc....");

    /* test global */
    array = dr_custom_alloc(NULL, 0, SIZE, 0, NULL);
    write_array(array);
    dr_custom_free(NULL, 0, array, SIZE);

    array = dr_custom_alloc(NULL, DR_ALLOC_CACHE_REACHABLE, SIZE, 0, NULL);
    ASSERT(reachable_from_client(array));
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_CACHE_REACHABLE, array, SIZE);

    /* test thread-local */
    array = dr_custom_alloc(drcontext, DR_ALLOC_THREAD_PRIVATE, SIZE, 0, NULL);
    write_array(array);
    dr_custom_free(drcontext, DR_ALLOC_THREAD_PRIVATE, array, SIZE);

    array = dr_custom_alloc(drcontext, DR_ALLOC_THREAD_PRIVATE | DR_ALLOC_CACHE_REACHABLE,
                            SIZE, 0, NULL);
    ASSERT(reachable_from_client(array));
    write_array(array);
    dr_custom_free(drcontext, DR_ALLOC_THREAD_PRIVATE | DR_ALLOC_CACHE_REACHABLE, array,
                   SIZE);

    /* test non-heap */
    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP, array, PAGE_SIZE);

    /* Find a free region of memory without inadvertently "preloading" it.
     * First probe by allocating 2x the platform allocation alignment unit.
     */
    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR, HINT_ALLOC_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    /* Then select the second half as the preferred address for the allocation test. */
    preferred = (void *)((ptr_uint_t)array + HINT_OFFSET);
    /* Free the probe allocation. */
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR, array, HINT_ALLOC_SIZE);

    /* Now `preferred` is guaranteed to be available. */
    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_FIXED_LOCATION, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, preferred);
    ASSERT(array == preferred);
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_FIXED_LOCATION, array, PAGE_SIZE);

    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_CACHE_REACHABLE, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    ASSERT(reachable_from_client(array));
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_CACHE_REACHABLE, array, PAGE_SIZE);

    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_LOW_2GB, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
#ifdef X64
    ASSERT((ptr_uint_t)array < 0x80000000);
#endif
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_LOW_2GB, array, PAGE_SIZE);

    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR, array, PAGE_SIZE);

    array = dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP, PAGE_SIZE,
                            DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC, NULL);
    ASSERT(dr_query_memory((byte *)array, NULL, &size, &prot) && size == PAGE_SIZE &&
           prot == (DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC));
    write_array(array);
    dr_custom_free(NULL, DR_ALLOC_NON_HEAP, array, PAGE_SIZE);

    dr_fprintf(STDERR, "success\n");
}

#ifdef WINDOWS
static void
custom_windows_test(void)
{
    void *array;
    MEMORY_BASIC_INFORMATION mbi;
    bool ok;

    dr_fprintf(STDERR, "  testing custom windows alloc....");

    array =
        dr_custom_alloc(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR | DR_ALLOC_RESERVE_ONLY,
                        PAGE_SIZE * 2, DR_MEMPROT_NONE, NULL);
    if (array == NULL)
        dr_fprintf(STDERR, "error: unable to reserve\n");
    if (dr_virtual_query(array, &mbi, sizeof(mbi)) != sizeof(mbi))
        dr_fprintf(STDERR, "error: unable to query prot\n");
    /* 0 is sometimes returned (see VirtualQuery docs) */
    if (mbi.Protect != PAGE_NOACCESS && mbi.Protect != 0)
        dr_fprintf(STDERR, "error: wrong reserve prot %x\n", mbi.Protect);
    if (mbi.State != MEM_RESERVE)
        dr_fprintf(STDERR, "error: memory wasn't reserved\n");

    array = dr_custom_alloc(NULL,
                            DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR | DR_ALLOC_COMMIT_ONLY |
                                DR_ALLOC_FIXED_LOCATION,
                            PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, array);
    if (array == NULL)
        dr_fprintf(STDERR, "error: unable to commit\n");
    if (dr_virtual_query(array, &mbi, sizeof(mbi)) != sizeof(mbi))
        dr_fprintf(STDERR, "error: unable to query prot\n");
    if (mbi.Protect != PAGE_READWRITE)
        dr_fprintf(STDERR, "error: wrong commit prot %x\n", mbi.Protect);
    if (mbi.State != MEM_COMMIT || mbi.RegionSize != PAGE_SIZE)
        dr_fprintf(STDERR, "error: memory wasn't committed\n");

    write_array(array);

    ok = dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR | DR_ALLOC_COMMIT_ONLY,
                        array, PAGE_SIZE);
    if (!ok)
        dr_fprintf(STDERR, "error: failed to de-commit\n");
    if (dr_virtual_query(array, &mbi, sizeof(mbi)) != sizeof(mbi))
        dr_fprintf(STDERR, "error: unable to query prot\n");
    /* 0 is sometimes returned (see VirtualQuery docs) */
    if (mbi.Protect != PAGE_NOACCESS && mbi.Protect != 0)
        dr_fprintf(STDERR, "error: wrong decommit prot %x\n", mbi.Protect);
    if (mbi.State != MEM_RESERVE)
        dr_fprintf(STDERR, "error: memory wasn't de-committed %x\n", mbi.State);

    ok = dr_custom_free(NULL, DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR | DR_ALLOC_RESERVE_ONLY,
                        array, PAGE_SIZE * 2);
    if (!ok)
        dr_fprintf(STDERR, "error: failed to un-reserve\n");
    if (dr_virtual_query(array, &mbi, sizeof(mbi)) != sizeof(mbi))
        dr_fprintf(STDERR, "error: unable to query prot\n");
    /* 0 is sometimes returned (see VirtualQuery docs) */
    if (mbi.Protect != PAGE_NOACCESS && mbi.Protect != 0)
        dr_fprintf(STDERR, "error: wrong unreserve prot %x\n", mbi.Protect);
    if (mbi.State != MEM_FREE)
        dr_fprintf(STDERR, "error: memory wasn't un-reserved\n");

    dr_fprintf(STDERR, "success\n");
}
#endif

#ifdef UNIX
static void
custom_unix_test(void)
{
    void *array;
    bool ok;

    /* "linux" is replaced by "1" in .template so we use "Linux" */
    dr_fprintf(STDERR, "  testing custom Linux alloc....");

    array = dr_raw_mem_alloc(PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (array == NULL)
        dr_fprintf(STDERR, "error: unable to mmap\n");
    write_array(array);

#    ifdef LINUX
    array = dr_raw_mremap(array, PAGE_SIZE, PAGE_SIZE * 2, MREMAP_MAYMOVE, NULL);
    if ((ptr_int_t)array <= 0 && (ptr_int_t)array >= -PAGE_SIZE)
        dr_fprintf(STDERR, "error: unable to mremap\n");
    write_array(array);
#    endif

    ok = dr_raw_mem_free(array, PAGE_SIZE * 2);
    if (!ok)
        dr_fprintf(STDERR, "error: failed to munmap\n");

#    ifdef LINUX
    array = dr_raw_brk(0);
    if (array == NULL)
        dr_fprintf(STDERR, "error: unable to query brk\n");
#    endif

    dr_fprintf(STDERR, "success\n");
}
#endif

static void
memory_iteration_test(void)
{
    dr_mem_info_t info;
    byte *pc = NULL;
    while (true) {
        bool res = dr_query_memory_ex(pc, &info);
        if (!res) {
            ASSERT(
                info.type ==
                DR_MEMTYPE_ERROR IF_WINDOWS(|| info.type == DR_MEMTYPE_ERROR_WINKERNEL));
            if (info.type == DR_MEMTYPE_ERROR)
                dr_fprintf(STDERR, "error: memory iteration failed\n");
            break;
        }
        ASSERT(info.type !=
               DR_MEMTYPE_ERROR IF_WINDOWS(&&info.type != DR_MEMTYPE_ERROR_WINKERNEL));
        if (POINTER_OVERFLOW_ON_ADD(pc, info.size))
            break;
        pc += info.size;
    }
}

static void
alignment_test(void)
{
    dr_fprintf(STDERR, "  testing alignment....");
    /* The standard alignment guarantee is 16-byte for 64-bit, 8-byte for 32-bit: */
#define EXPECT_ALIGN IF_X64_ELSE(16, 8)
    /* Alloc many times since half of the new allocations will align accidentally,
     * and if there are free list entries it could be more than half.
     */
#define NUM_TRIES 8
    /* We use a bit pattern that doesn't match DR's 0xab, 0xbc, and 0xcd fills. */
#define PATTERN 0x77
#define DR_PATTERN 0xab
    void *mem[NUM_TRIES];
    /* See if DR is using a known fill pattern (we have to pass -checklevel 3
     * to get the pattern for client/privlib allocs).
     */
    mem[0] = malloc(4);
    bool filled = ((byte *)mem[0])[0] == DR_PATTERN;
    free(mem[0]);
    /* Try several sizes since DR's bucket sizes can make one particular bucket
     * over-align more often than others.
     */
    for (size_t sz = 4; sz < 64; sz += 4) {
        for (int i = 0; i < NUM_TRIES; ++i) {
            mem[i] = malloc(sz);
            ASSERT(ALIGNED(mem[i], EXPECT_ALIGN));
            size_t smaller_sz = sz / 2;
            size_t larger_sz = sz * 2 + 2;
            mem[i] = realloc(mem[i], smaller_sz);
            ASSERT(ALIGNED(mem[i], EXPECT_ALIGN));
            /* Ensure the values get preserved. */
            memset(mem[i], PATTERN, smaller_sz);
            mem[i] = realloc(mem[i], larger_sz);
            ASSERT(ALIGNED(mem[i], EXPECT_ALIGN));
            for (size_t j = 0; j < smaller_sz; ++j)
                ASSERT(((byte *)mem[i])[j] == PATTERN);
            if (filled) {
                /* Make sure we copied the right size and no more. */
                for (size_t j = smaller_sz; j < larger_sz; ++j) {
                    ASSERT(((byte *)mem[i])[j] == DR_PATTERN);
                }
            }
        }
        for (int i = 0; i < NUM_TRIES; ++i)
            free(mem[i]);
    }
    dr_fprintf(STDERR, "success\n");
}

#ifdef UNIX
static void
calloc_test(void)
{
    /* using the trigger from i#1115 */
    char *array = (char *)calloc(100000, 16);
    if (array[100000 * 16 - 1] != 0)
        dr_fprintf(STDERR, "error: calloc not zeroing\n");
}
#endif

static void
local_test(void *drcontext)
{
    char *array;
    dr_fprintf(STDERR, "  testing local memory alloc....");
    array = dr_thread_alloc(drcontext, SIZE);
    write_array(array);
    dr_thread_free(drcontext, array, SIZE);
    dr_fprintf(STDERR, "success\n");
}

static void
thread_init_event(void *drcontext)
{
    static bool tested = false;
    if (!tested) {
        dr_fprintf(STDERR, "thread initialization:\n");
        local_test(drcontext);
        global_test();
        tested = true;
    }
}

static void
inline_alloc_test(void)
{
    dr_fprintf(STDERR, "code cache:\n");
    local_test(dr_get_current_drcontext());
    global_test();
    nonheap_test();
    raw_alloc_test();
#ifdef UNIX
    calloc_test();
#endif
#ifdef X64
    reachability_test();
#endif
#if defined(LINUX) && defined(X86_64)
    /* i#4335: Test allocation of more than 2.8GB in unreachable heap */
    for (int i = 0; i != 50; ++i) {
        if (malloc(100000000) == NULL)
            dr_fprintf(STDERR, "Failed to allocate\n");
    }
#endif
}

#define MINSERT instrlist_meta_preinsert

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    static bool inserted = false;
    if (!inserted) {
        instr_t *instr = instrlist_first(bb);

        dr_prepare_for_call(drcontext, bb, instr);

        MINSERT(bb, instr,
                INSTR_CREATE_call(drcontext, opnd_create_pc((void *)inline_alloc_test)));

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

                    /* Store the pc so we can write to it once in a safe place,
                     * at the next syscall, to test i#143c#4.
                     */
                    if (app_gencode == NULL)
                        app_gencode = pc;

                    dr_fprintf(STDERR, "success\n");
                }
            }
        }
    }

    /* store, since we're not deterministic */
    return DR_EMIT_STORE_TRANSLATIONS;
}

/* i#143c#4: test DR handling a fault from a client writing to a
 * pretend-writable page, which can only be done when nolinking and
 * when holding no locks.  Thus, we store the pc in the bb event above
 * and wait for the next syscall.
 */
static bool
filter_syscall_event(void *drcontext, int sysnum)
{
    return true;
}

static bool
pre_syscall_event(void *drcontext, int sysnum)
{
    if (app_gencode != NULL) {
        *app_gencode = 90;
        app_gencode = NULL;
        dr_fprintf(STDERR, "wrote to app code page successfully\n");
    }
    return true;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    client_id = id;

    dr_fprintf(STDERR, "thank you for testing the client interface\n");

#ifdef LINUX
    /* i#262: check if READ_IMPLIES_EXEC in personality. If true,
     * we expect the readable memory also has exec right.
     */
    int res = personality(0xffffffff);
    if (res == -1)
        fprintf(stderr, "Error: fail to get personality\n");
    else if (TEST(READ_IMPLIES_EXEC, res))
        add_exec = true;
#endif

    global_test();
    nonheap_test();
    custom_test();
#ifdef WINDOWS
    custom_windows_test();
#else
    custom_unix_test();
#endif
    memory_iteration_test();
    alignment_test();

    dr_register_bb_event(bb_event);
    dr_register_thread_init_event(thread_init_event);
    dr_register_filter_syscall_event(filter_syscall_event);
    dr_register_pre_syscall_event(pre_syscall_event);
}
