/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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

#include <stdio.h>

#if defined(MACOS) || defined(WINDOWS)
#    error Linux x86 only
#endif

#ifdef __i386__
#    include <linux/unistd.h>
#    include <asm/ldt.h>
#    include <sys/mman.h>
#    include <unistd.h>
#    include <sys/syscall.h>
#    include <errno.h>
#else
#    include <asm/prctl.h>
#    include <sys/prctl.h>
#endif

int
arch_prctl(int code, unsigned long addr);

static int
test_func()
{
    return (42);
}

int
main()
{
#ifdef __i386__
    void *seg = mmap(NULL, getpagesize(), PROT_WRITE | PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    struct user_desc u_info;
    int val;

    ((void **)seg)[0x10 / sizeof(void *)] = &test_func;
    u_info.entry_number = 7;
    u_info.base_addr = (unsigned long)seg;
    u_info.limit = getpagesize();
    u_info.seg_32bit = 1;
    u_info.contents = MODIFY_LDT_CONTENTS_DATA;
    u_info.read_exec_only = 0;
    u_info.limit_in_pages = 0;
    u_info.seg_not_present = 0;
    u_info.useable = 1;
    val = (7 << 3 | 0 << 2 | 3);

    /* If a 32 bit program run on a 64 bit system, the first free gdt slot is 12
     * instead of 6 for 32 bit system.
     */
    if (syscall(SYS_set_thread_area, &u_info) < 0 && errno == EINVAL) {
        u_info.entry_number = 13;
        val = (13 << 3 | 0 << 2 | 3);
        syscall(SYS_set_thread_area, &u_info);
    }

    /* FIXME i#1833: the following code with gs doesn't run properly with DynamoRIO
     * when it will, enable the following code for gs segment test.
     */
#    if ENABLE_ONCE_1833_IS_FIXED
    __asm__ volatile("push    %%gs\n"
                     "mov     %0, %%gs\n"
                     "call    *%%gs:0x10\n"
                     "mov     $0x10, %%eax\n"
                     "call    *%%gs:(%%eax)\n"
                     "pop     %%gs\n"
                     :
                     : "m"(val)
                     : "eax");
#    endif
    __asm__ volatile("mov     %0, %%fs\n"
                     "call    *%%fs:0x10\n"
                     "mov     $0x10, %%eax\n"
                     "call    *%%fs:(%%eax)\n"
                     :
                     : "m"(val)
                     : "eax");
#else
    void (*funcs[10])();
    void *old_fs;

    funcs[0x10 / sizeof(void *)] = (void *)&test_func;

    arch_prctl(ARCH_GET_FS, (unsigned long)&old_fs);
    arch_prctl(ARCH_SET_FS, (unsigned long)funcs);
    __asm__ volatile("call    *%fs:0x10\n"
                     "mov     $0x10, %rax\n"
                     "call    *%fs:(%rax)\n");
    arch_prctl(ARCH_SET_FS, (unsigned long)old_fs);

    /* FIXME i#1833: Actually only fs is test because gs is used by DynamoRIO
     * and made it segfault, fs have to be restored because it's used by the kernel
     * (for example to store the canary).
     * When the segfault is fixed enable the following code to add
     * the test for gs.
     */
#    if ENABLE_ONCE_1833_IS_FIXED
    arch_prctl(ARCH_SET_GS, (unsigned long)funcs);
    __asm__ volatile("call    *%gs:0x10\n"
                     "mov     $0x10, %rax\n"
                     "call    *%gs:(%rax)\n");
#    endif

#endif
    return 0;
}
