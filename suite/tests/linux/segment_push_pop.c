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

/* Tests the application's push and pop of the fs and gs segment registers, which DR
 * virtualizes since it keeps its own TLS and its private libraries' TLS in these
 * segments (i#1833): a push must push the application's selector, and a pop must
 * update the application's selector and base but not DR's.  There is no client, so DR
 * builds its blocks without decoding every instruction.
 */

#include "tools.h"

#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifdef X64
#    include <asm/prctl.h>
#else
#    include <asm/ldt.h>
#endif

/* A 32-bit push of a segment register may write only the low 16 bits of the stack slot,
 * so we compare only those.
 */
#define SELECTOR(val) ((val) & 0xffff)

static void
check(const char *what, unsigned long value, unsigned long expect)
{
    if (value != expect)
        print("%s: 0x%lx, expected 0x%lx\n", what, value, expect);
}

#ifdef X64
/* The selector of the flat user data segment, which any application can load into gs
 * and which differs from the null selector DR keeps there.
 */
#    define USER_DS 0x2b

static unsigned long
get_gs_base(void)
{
    unsigned long base = 0;
    if (syscall(SYS_arch_prctl, ARCH_GET_GS, &base) != 0)
        print("ARCH_GET_GS failed\n");
    return base;
}

static void
test_push_pop(void)
{
    unsigned long orig_gs, orig_fs, pushed, after, popped, pushed_fs;
    unsigned int app_gs = USER_DS;
    unsigned short pushed16;

    __asm__ volatile("mov %%gs, %0" : "=r"(orig_gs));
    __asm__ volatile("mov %%fs, %0" : "=r"(orig_fs));

    /* Push gs while the application's selector differs from DR's, and pop the
     * original one.
     */
    __asm__ volatile("push %%gs\n\t"
                     "mov %3, %%gs\n\t"
                     "push %%gs\n\t"
                     "pop %0\n\t"
                     "pop %%gs\n\t"
                     "mov %%gs, %1\n\t"
                     "push %%gs\n\t"
                     "pop %2\n\t"
                     : "=&r"(pushed), "=&r"(after), "=&r"(popped)
                     : "r"(app_gs)
                     : "memory");
    check("pushed gs", pushed, USER_DS);
    check("gs after pop", after, orig_gs);
    check("pushed gs after pop", popped, orig_gs);
    /* Both selectors give base 0. */
    check("gs base after pop", get_gs_base(), 0);

    /* The 16-bit forms. */
    __asm__ volatile("pushw %%gs\n\t"
                     "mov %2, %%gs\n\t"
                     "pushw %%gs\n\t"
                     "popw %0\n\t"
                     "popw %%gs\n\t"
                     "mov %%gs, %1\n\t"
                     : "=&r"(pushed16), "=&r"(after)
                     : "r"(app_gs)
                     : "memory");
    check("pushed gs (16-bit)", pushed16, USER_DS);
    check("gs after pop (16-bit)", after, orig_gs);

    /* We do not pop fs: loading the null selector clears the base, which holds the
     * application's TLS, on Intel processors.
     */
    __asm__ volatile("push %%fs\n\t"
                     "pop %0\n\t"
                     : "=r"(pushed_fs)
                     :
                     : "memory");
    check("pushed fs", pushed_fs, orig_fs);
}
#else
static unsigned int tls_data[2] = { 0x11223344, 0x55667788 };

/* Returns the selector of a new TLS entry with base "base". */
static unsigned int
new_tls_selector(void *base)
{
    struct user_desc desc;
    memset(&desc, 0, sizeof(desc));
    /* The first TLS entry, which the C library uses, is 6 on a 32-bit kernel and 12
     * for a 32-bit application on a 64-bit kernel.
     */
    desc.entry_number = 7;
    desc.base_addr = (unsigned long)base;
    /* The maximum limit, in units of 4K (the descriptor granularity, whatever the
     * page size), as the C library uses: the whole address space.
     */
    desc.limit = 0xfffff;
    desc.seg_32bit = 1;
    desc.limit_in_pages = 1;
    desc.useable = 1;
    if (syscall(SYS_set_thread_area, &desc) != 0) {
        desc.entry_number = 13;
        if (syscall(SYS_set_thread_area, &desc) != 0)
            print("set_thread_area failed: %d\n", errno);
    }
    return desc.entry_number << 3 | 3;
}

static void
test_push_pop(void)
{
    unsigned int orig_gs, orig_fs, app_gs, pushed, loaded, after, popped, pushed_fs;
    unsigned short pushed16;

    app_gs = new_tls_selector(tls_data);
    __asm__ volatile("mov %%gs, %0" : "=r"(orig_gs));
    __asm__ volatile("mov %%fs, %0" : "=r"(orig_fs));

    /* Push gs while the application's selector differs from the one DR's private
     * libraries use, read through it, and pop the original one.
     */
    __asm__ volatile("push %%gs\n\t"
                     "mov %4, %%gs\n\t"
                     "push %%gs\n\t"
                     "pop %0\n\t"
                     "mov %%gs:4, %1\n\t"
                     "pop %%gs\n\t"
                     "mov %%gs, %2\n\t"
                     "push %%gs\n\t"
                     "pop %3\n\t"
                     : "=&r"(pushed), "=&r"(loaded), "=&r"(after), "=&r"(popped)
                     : "r"(app_gs)
                     : "memory");
    check("pushed gs", SELECTOR(pushed), app_gs);
    check("loaded through gs", loaded, tls_data[1]);
    check("gs after pop", after, orig_gs);
    check("pushed gs after pop", SELECTOR(popped), orig_gs);

    /* The 16-bit forms. */
    __asm__ volatile("pushw %%gs\n\t"
                     "mov %2, %%gs\n\t"
                     "pushw %%gs\n\t"
                     "popw %0\n\t"
                     "popw %%gs\n\t"
                     "mov %%gs, %1\n\t"
                     : "=&r"(pushed16), "=&r"(after)
                     : "r"(app_gs)
                     : "memory");
    check("pushed gs (16-bit)", pushed16, app_gs);
    check("gs after pop (16-bit)", after, orig_gs);

    /* DR keeps its own TLS in fs.  The application's fs holds the null selector. */
    __asm__ volatile("push %%fs\n\t"
                     "pop %0\n\t"
                     "push %%fs\n\t"
                     "pop %%fs\n\t"
                     "mov %%fs, %1\n\t"
                     : "=&r"(pushed_fs), "=&r"(after)
                     :
                     : "memory");
    check("pushed fs", SELECTOR(pushed_fs), orig_fs);
    check("fs after pop", after, orig_fs);
}
#endif

int
main(void)
{
    test_push_pop();
    print("all done\n");
    return 0;
}
