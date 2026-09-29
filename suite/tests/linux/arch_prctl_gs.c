/* **********************************************************
 * Copyright (c) 2026 Chroniton PBC.  All rights reserved.
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
 * * Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Tests the application's arch_prctl(ARCH_SET_GS), which DR performs itself since it
 * keeps its own TLS in the gs segment on x86-64 (i#1833): the base the application
 * sets, and the bases the kernel rejects.
 */

#include "tools.h"

#include <asm/prctl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#    define MAP_FIXED_NOREPLACE 0x100000
#endif

static uint64_t gs_data[2] = { 0x1122334455667788ULL, 0x0123456789abcdefULL };

static uint64_t
get_gs(void)
{
    uint64_t base = 0;
    if (syscall(SYS_arch_prctl, ARCH_GET_GS, &base) != 0)
        print("ARCH_GET_GS failed\n");
    return base;
}

/* Sets the gs base to "base" and checks that that succeeds if "valid" and otherwise
 * fails with EPERM and leaves the base alone, as the kernel does.
 */
static void
check_set_gs(uint64_t base, int valid)
{
    uint64_t before = get_gs();
    errno = 0;
    long res = syscall(SYS_arch_prctl, ARCH_SET_GS, base);
    uint64_t after = get_gs();
    if (valid ? (res != 0 || after != base)
              : (res != -1 || errno != EPERM || after != before)) {
        print("ARCH_SET_GS 0x%lx: %ld, errno %d, base 0x%lx\n", base, res, errno, after);
    }
}

int
main(int argc, char **argv)
{
    uint64_t orig_gs = get_gs();
    uint64_t val;

    /* gs references use the base the application set. */
    if (syscall(SYS_arch_prctl, ARCH_SET_GS, (uint64_t)gs_data) != 0)
        print("ARCH_SET_GS failed\n");
    __asm__ __volatile__("mov %%gs:8, %0" : "=r"(val));
    if (get_gs() != (uint64_t)gs_data || val != gs_data[1])
        print("gs base 0x%lx, gs:8 0x%lx\n", get_gs(), val);

    /* The kernel takes the code as an int, ignoring the upper bits of the register. */
    if (syscall(SYS_arch_prctl, (1L << 32) | ARCH_SET_GS, orig_gs) != 0 ||
        get_gs() != orig_gs)
        print("ARCH_SET_GS with upper bits failed\n");

    /* The kernel rejects bases at or above TASK_SIZE_MAX, which is (1 << 47) - 4096
     * with 4-level page tables and (1 << 56) - 4096 with 5-level page tables.  We find
     * out which the kernel uses without arch_prctl: only with 5-level page tables can
     * we map memory at 1 << 47.
     */
    void *hi = mmap((void *)(1UL << 47), 4096, PROT_READ,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    int five_level = hi == (void *)(1UL << 47);
    if (hi != MAP_FAILED)
        munmap(hi, 4096);
    check_set_gs((1UL << 47) - 8192, 1);
    /* The first base between the two limits: DR asks the kernel. */
    check_set_gs((1UL << 47) - 4096, five_level);
    /* The others use what DR learned. */
    check_set_gs(1UL << 47, five_level);
    check_set_gs((1UL << 56) - 8192, five_level);
    check_set_gs((1UL << 56) - 4096, 0);
    check_set_gs(0xffff880000000000ULL, 0);

    if (syscall(SYS_arch_prctl, ARCH_SET_GS, orig_gs) != 0)
        print("ARCH_SET_GS failed\n");
    print("all done\n");
    return 0;
}
