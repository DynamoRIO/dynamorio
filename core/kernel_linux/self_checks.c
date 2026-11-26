/* **********************************************************
 * Copyright (c) 2026 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* On-demand self-checks for the DynamoRIO Linux kernel module, run by the self-check
 * ioctl.  This file is always compiled so that the build systems share one source
 * list; KERNEL_MODULE_SELF_CHECKS selects whether it contains any self-checks.
 */

#include "globals.h"
#include "dr_interface.h"
#include "kernel_interface.h"

#ifdef KERNEL_MODULE_SELF_CHECKS

typedef enum {
    SELF_CHECK_PASS,
    SELF_CHECK_FAIL,
    SELF_CHECK_SKIP,
} self_check_result_t;

/* Fails the enclosing self-check if |cond| is false.  Unlike ASSERT, this reports the
 * failure and returns rather than terminating, which in the kernel is a panic.
 */
#    define CHECK(cond)                                                          \
        do {                                                                     \
            if (!(cond)) {                                                       \
                print_file(STDERR, "Self-check failed at %s:%d: %s\n", __func__, \
                           __LINE__, #cond);                                     \
                return SELF_CHECK_FAIL;                                          \
            }                                                                    \
        } while (0)

typedef struct {
    const char *name;
    self_check_result_t (*func)(void);
} self_check_t;

/* Verifies that segment accesses through os_tls_offset() and C accesses through the
 * CPU-local state pointer reach the same memory, in both directions.
 */
static self_check_result_t
check_tls_segment_mapping(void)
{
    local_state_extended_t *ls = get_local_state_extended();
    /* Lowest and highest pointer-sized slots that generated code accesses:
     * the first register spill slot and the last IBL hashtable pointer.
     */
    const ushort first = TLS_REG0_SLOT;
    const ushort last = TLS_TABLE_SLOT(IBL_BRANCH_TYPE_END - 1);
    ptr_uint_t *first_slot = (ptr_uint_t *)((byte *)ls + first);
    ptr_uint_t *last_slot = (ptr_uint_t *)((byte *)ls + last);
    const ptr_uint_t sentinel1 = 0xdecafbadcafef00d;
    const ptr_uint_t sentinel2 = 0x0123456789abcdef;

    /* The session is live. Restore each value through the same access path that
     * wrote it before reporting a failure.
     */
    const ptr_uint_t saved_first = *first_slot;
    *first_slot = sentinel1;
    ptr_uint_t observed_first = (ptr_uint_t)d_r_get_tls(os_tls_offset(first));
    *first_slot = saved_first;
    CHECK(observed_first == sentinel1);

    const int last_offset = os_tls_offset(last);
    void *saved_last = d_r_get_tls(last_offset);
    d_r_set_tls(last_offset, (void *)sentinel2);
    ptr_uint_t observed_last = *last_slot;
    d_r_set_tls(last_offset, saved_last);
    CHECK(observed_last == sentinel2);
    return SELF_CHECK_PASS;
}

/* Verifies that a safe read copies readable memory, and that it fails without faulting
 * on an unreadable address in both the user and the kernel range.
 */
static self_check_result_t
check_safe_read(void)
{
    const ptr_uint_t src = 0x0123456789abcdef;
    ptr_uint_t dst = 0;
    size_t bytes_read = 0;

    bool ok = safe_read_ex(&src, sizeof(src), &dst, &bytes_read);
    CHECK(ok && bytes_read == sizeof(src) && dst == src);

    dst = 0;
    ok = d_r_safe_read(&src, sizeof(src), &dst);
    CHECK(ok && dst == src);

    /* Address 0 is in the user range and is not mapped. */
    bytes_read = sizeof(dst);
    ok = safe_read_ex(NULL, sizeof(dst), &dst, &bytes_read);
    CHECK(!ok && bytes_read == 0);

    /* The last page of the address space is in the kernel range and is never mapped:
     * its addresses are the ERR_PTR() values.
     */
    bytes_read = sizeof(dst);
    ok = safe_read_ex((void *)ALIGN_BACKWARD(PTR_UINT_MINUS_1, PAGE_SIZE), sizeof(dst),
                      &dst, &bytes_read);
    CHECK(!ok && bytes_read == 0);
    return SELF_CHECK_PASS;
}

/* Verifies that DR's code cache can reach the whole kernel image with a 32-bit
 * relative offset, which the heap placement in kernel_module_init() guarantees.
 */
static self_check_result_t
check_kernel_image_reachable_from_vmcode(void)
{
#    if defined(X86) && defined(X64)
    CHECK(rel32_reachable_from_vmcode(get_application_base()));
    /* get_application_end() is exclusive. */
    CHECK(rel32_reachable_from_vmcode(get_application_end() - 1));
    return SELF_CHECK_PASS;
#    else
    /* rel32 reachability is only used on x86-64. */
    return SELF_CHECK_SKIP;
#    endif
}

/* For non-user addresses, query_memory_ex() describes mapped and free pages.
 * get_memory_info() succeeds only for mapped pages.  Both reject user addresses.
 */
static self_check_result_t
check_memory_query(void)
{
    /* Module data is readable and writable.  Both queries describe the same page. */
    static int data;
    byte *data_addr = (byte *)&data + 1; /* Exercise rounding down to the page base. */
    dr_mem_info_t info;
    byte *base = NULL;
    size_t size = 0;
    uint prot = 0;
    CHECK(!is_user_address(data_addr));
    CHECK(query_memory_ex(data_addr, &info));
    CHECK(info.type == DR_MEMTYPE_DATA && !info.is_shared);
    CHECK(info.base_pc == (byte *)ALIGN_BACKWARD(data_addr, PAGE_SIZE));
    CHECK(info.size == PAGE_SIZE);
    CHECK(TESTALL(MEMPROT_READ | MEMPROT_WRITE, info.prot));
    CHECK(get_memory_info(data_addr, &base, &size, &prot));
    CHECK(base == info.base_pc && size == info.size && prot == info.prot);

    /* Module code is readable and executable.  All wrapper outputs are optional. */
    byte *code_addr = (byte *)check_memory_query;
    CHECK(!is_user_address(code_addr));
    CHECK(query_memory_ex(code_addr, &info));
    CHECK(info.type == DR_MEMTYPE_DATA);
    CHECK(info.base_pc == (byte *)ALIGN_BACKWARD(code_addr, PAGE_SIZE));
    CHECK(info.size == PAGE_SIZE);
    CHECK(TESTALL(MEMPROT_READ | MEMPROT_EXEC, info.prot));
    CHECK(get_memory_info(code_addr, NULL, NULL, NULL));

    /* The last kernel page is unmapped. */
    byte *last_page = (byte *)ALIGN_BACKWARD(PTR_UINT_MINUS_1, PAGE_SIZE);
    CHECK(!is_user_address(last_page));
    CHECK(query_memory_ex(last_page, &info));
    CHECK(info.type == DR_MEMTYPE_FREE && info.prot == MEMPROT_NONE);
    CHECK(info.base_pc == last_page && info.size == PAGE_SIZE);
    CHECK(!get_memory_info(last_page, NULL, NULL, NULL));

    /* Clearing the top bit moves data_addr into the gap but preserves its page table
     * indices.  The query must report that address as free.
     */
    byte *gap_addr = (byte *)((ptr_uint_t)data_addr & ~((ptr_uint_t)1 << 63));
    CHECK(!is_user_address(gap_addr));
    CHECK(query_memory_ex(gap_addr, &info));
    CHECK(info.type == DR_MEMTYPE_FREE && info.prot == MEMPROT_NONE);
    CHECK(info.base_pc == (byte *)ALIGN_BACKWARD(gap_addr, PAGE_SIZE));
    CHECK(info.size == PAGE_SIZE);
    CHECK(!get_memory_info(gap_addr, NULL, NULL, NULL));

    /* Address zero belongs to user space. */
    CHECK(is_user_address(NULL));
    CHECK(!query_memory_ex(NULL, &info) && info.type == DR_MEMTYPE_ERROR);
    CHECK(!get_memory_info(NULL, NULL, NULL, NULL));

    return SELF_CHECK_PASS;
}

static int
cmp_uint(const void *a, const void *b)
{
    uint lhs = *(const uint *)a;
    uint rhs = *(const uint *)b;
    return (lhs > rhs) - (lhs < rhs);
}

/* Verifies that code executes under DR between dr_app_start() and dr_app_stop() and
 * produces the same result as native execution.  Uses the kernel's sort() so that
 * vmlinux code (outside is_in_dynamo_dll()) and indirect comparator calls are exercised.
 */
static self_check_result_t
check_run_under_dr(void)
{
    enum { count = 64 };
    uint native_array[count];
    uint under_dr_array[count];

    for (size_t i = 0; i < count; i++) {
        native_array[i] = kernel_get_random_u32();
    }
    memcpy(under_dr_array, native_array, sizeof(native_array));

    kernel_sort(native_array, count, sizeof(native_array[0]), cmp_uint);
    for (size_t i = 1; i < count; i++) {
        CHECK(native_array[i - 1] <= native_array[i]);
    }

    CHECK(!dr_app_running_under_dynamorio());
    unsigned long flags = kernel_irq_save();
    dr_app_start();
    bool under_dr = dr_app_running_under_dynamorio();
    kernel_sort(under_dr_array, count, sizeof(under_dr_array[0]), cmp_uint);
    dr_app_stop();
    kernel_irq_restore(flags);
    CHECK(!dr_app_running_under_dynamorio());
    CHECK(under_dr);

    for (size_t i = 0; i < count; i++) {
        CHECK(under_dr_array[i] == native_array[i]);
    }

    return SELF_CHECK_PASS;
}

static const self_check_t self_checks[] = {
    { "tls_segment_mapping", check_tls_segment_mapping },
    { "safe_read", check_safe_read },
    { "kernel_image_reachable_from_vmcode", check_kernel_image_reachable_from_vmcode },
    { "memory_query", check_memory_query },
    { "run_under_dr", check_run_under_dr },
};

int
dynamorio_run_self_checks(void)
{
    int num_passed = 0;
    int num_failed = 0;
    int num_skipped = 0;
    for (size_t i = 0; i < BUFFER_SIZE_ELEMENTS(self_checks); i++) {
        const self_check_t *check = &self_checks[i];
        self_check_result_t result = check->func();
        const char *result_string = "PASS";
        if (result == SELF_CHECK_PASS) {
            num_passed++;
        } else if (result == SELF_CHECK_FAIL) {
            result_string = "FAIL";
            num_failed++;
        } else if (result == SELF_CHECK_SKIP) {
            result_string = "SKIP";
            num_skipped++;
        }
        print_file(STDERR, "Self-check %s: %s\n", check->name, result_string);
    }
    if (num_failed == 0) {
        print_file(STDERR, "All self-checks passed (%d passed, %d skipped)\n", num_passed,
                   num_skipped);
    } else {
        print_file(STDERR, "Self-checks failed (%d passed, %d failed, %d skipped)\n",
                   num_passed, num_failed, num_skipped);
    }
    return num_failed;
}

#else

int
dynamorio_run_self_checks(void)
{
    /* The caller maps this to an errno. */
    return -1;
}

#endif /* KERNEL_MODULE_SELF_CHECKS */
