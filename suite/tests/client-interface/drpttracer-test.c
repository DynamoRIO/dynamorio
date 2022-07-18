/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* based on suite/tests/pthreads/pthreads.c */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>
#include <signal.h>

volatile double pi = 0.0;  /* Approximation to pi (shared) */
pthread_mutex_t pi_lock;   /* Lock for above */
volatile double intervals; /* How many intervals? */

void *
process(void *arg)
{
    char *id = (char *)arg;
    register double width, localsum;
    register int i;
    register int iproc;

    iproc = (*((char *)arg) - '0');

    /* Set width */
    width = 1.0 / intervals;

    /* Do the local computations */
    localsum = 0;
    for (i = iproc; i < intervals; i += 2) {
        register double x = (i + 0.5) * width;
        localsum += 4.0 / (1.0 + x * x);
    }
    localsum *= width;

    /* Lock pi for update, update it, and unlock */
    pthread_mutex_lock(&pi_lock);
    pi += localsum;
    pthread_mutex_unlock(&pi_lock);

    return (NULL);
}

int
main(int argc, char **argv)
{
    pthread_t thread0, thread1;
    void *retval;
    char table[2] = { 'A', 'B' };
#ifdef X86
    char ch;
    /* Test xlat for drutil_insert_get_mem_addr,
     * we do not bother to run this test on Windows side.
     */
    __asm("mov %1, %%" IF_X64_ELSE("rbx", "ebx") "\n\t"
                                                 "mov $0x1, %%eax\n\t"
                                                 "xlat\n\t"
                                                 "movb %%al, %0\n\t"
          : "=m"(ch)
          : "g"(&table)
          : "%eax", "%ebx");
    printf("%c\n", ch);
    /* XXX: should come up w/ some clever way to ensure
     * this gets the right address: for now just making sure
     * it doesn't crash.
     */
#else
    printf("%c\n", table[1]);
#endif

#ifdef X86
    /* Test xsave for drutil_opnd_mem_size_in_bytes. We're assuming that
     * xsave support is available and enabled, which should be the case
     * on all machines we're running on.
     * Ideally we'd run whatever cpuid invocations are needed to figure out the exact
     * size but 16K is more than enough for the foreseeable future: it's 576 bytes
     * with SSE and ~2688 for AVX-512.
     */
    char ALIGN_VAR(64) buffer[16 * 1024];
    __asm("xor %%edx, %%edx\n\t"
          "or $-1, %%eax\n\t"
          "xsave %0\n\t"
          : "=m"(buffer)
          :
          : "eax", "edx", "memory");
#endif

    /* Test rep string expansions. */
    char buf1[1024];
    char buf2[1024];
#ifdef X86_64
    __asm("lea %[buf1], %%rdi\n\t"
          "lea %[buf2], %%rsi\n\t"
          "mov %[count], %%ecx\n\t"
          "rep movsq\n\t"
          :
          : [buf1] "m"(buf1), [buf2] "m"(buf2), [count] "i"(sizeof(buf1))
          : "ecx", "rdi", "rsi", "memory");
#elif defined(X86_32)
    __asm("lea %[buf1], %%edi\n\t"
          "lea %[buf2], %%esi\n\t"
          "mov %[count], %%ecx\n\t"
          "rep movsd\n\t"
          :
          : [buf1] "m"(buf1), [buf2] "m"(buf2), [count] "i"(sizeof(buf1))
          : "ecx", "edi", "esi", "memory");
#endif

    intervals = 10;
    /* Initialize the lock on pi */
    pthread_mutex_init(&pi_lock, NULL);

    /* Make the two threads */
    if (pthread_create(&thread0, NULL, process, (void *)"0") ||
        pthread_create(&thread1, NULL, process, (void *)"1")) {
        printf("%s: cannot make thread\n", argv[0]);
        exit(1);
    }

    /* Join (collapse) the two threads */
    if (pthread_join(thread0, &retval) || pthread_join(thread1, &retval)) {
        printf("%s: thread join failed\n", argv[0]);
        exit(1);
    }

    void *hmod;
    hmod = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
    if (hmod != NULL)
        dlclose(hmod);
    else
        printf("module load failed: %s\n", dlerror());

    /* Print the result */
    printf("Estimation of pi is %16.15f\n", pi);

    /* Let's raise a signal */
    raise(SIGUSR1);
    return 0;
}