/* **********************************************************
 * Copyright (c) 2017-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test sigaction without signals. */
/* XXX: We should also test non-RT sigaction. */

#include "tools.h" /* for print() */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef ANDROID
/* Android does not fully support RT (Bionic only uses non-RT): there is
 * some weird behavior when invoking RT syscalls (like bogus memory not
 * causing a failure).  Bionic also registers for 8 signals up front.
 * If we add non-RT tests we could likely run some of those but with caveats
 * for the 8 signals.
 */
#    error Android is not supported
#endif

#define SIGMAX 64
#define SIG1 SIGALRM /* an arbitrary signal that is not SIGKILL or SIGSTOP */
#define SIGSETSIZE 8 /* size of sigset mask */

typedef void (*sighandler_t)(int);
typedef void (*restorer_t)(void);

typedef struct {
    unsigned long sig[(SIGSETSIZE + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
} kernel_sigset_t;

/* This structure has no gaps. It can be compared with memcmp.
 * The same type is independently defined in core/unix/signal_private.h.
 */
struct kernel_sigaction_t {
    sighandler_t handler;
    unsigned long flags;
    restorer_t restorer;
    kernel_sigset_t mask;
};

#ifdef X86
#    define SA_IA32_ABI 0x02000000U
#    define SA_X32_ABI 0x01000000U
#endif

#define SIGACTSZ sizeof(struct kernel_sigaction_t)

/* This is a margin used by the test program to detect writes outside
 * the area that should be written to.
 */
#define MARGIN 8

/* POSIX sigdelset: delete signal signum from set. */
void
kernel_sigdelset(kernel_sigset_t *set, int signum)
{
    size_t b = sizeof(long) * 8;
    set->sig[(signum - 1) / b] &= ~(1L << ((signum - 1) % b));
}

/* The "real" sigaction. */
int
sys_sigaction(int signum, unsigned char *act, unsigned char *oldact, size_t sigsetsize)
{
    int ret = syscall(SYS_rt_sigaction, signum, act, oldact, sigsetsize);
    /* We see a 1 return value for some 32-bit-on-64-bit-kernel test_edge() cases. */
    assert(ret == 0 || ret == -1 || ret == 1);
    return ret == 0 ? 0 : -errno;
}

/* The simulated sigaction, for comparison.
 * The simulated sigaction does not detect memory protection; instead we specify
 * the memory protection with additional parameters.
 */
int
sim_sigaction(int signum, unsigned char *act, unsigned char *oldact, size_t sigsetsize,
              int prot_act, int prot_oldact)
{
    static unsigned char sigactions[SIGMAX][SIGACTSZ];
    struct kernel_sigaction_t tmp_act, tmp_oldact;

    if (sigsetsize != SIGSETSIZE)
        return -EINVAL;

    /* This may seem surprising, but it's what Linux does:
     * it checks the protection of "act" before it checks the signal number!
     */
    if (act != NULL && (prot_act == PROT_NONE || (prot_act & PROT_READ) == 0))
        return -EFAULT;

    if (!(1 <= signum && signum <= SIGMAX) ||
        (act != NULL && (signum == SIGKILL || signum == SIGSTOP)))
        return -EINVAL;

    if (oldact != NULL)
        memcpy(&tmp_oldact, sigactions[signum - 1], SIGACTSZ);

    if (act != NULL) {
        memcpy(&tmp_act, act, SIGACTSZ);
        kernel_sigdelset(&tmp_act.mask, SIGKILL);
        kernel_sigdelset(&tmp_act.mask, SIGSTOP);
#ifdef X86
        tmp_act.flags &= ~(SA_IA32_ABI | SA_X32_ABI);
#endif
        memcpy(sigactions[signum - 1], &tmp_act, SIGACTSZ);
    }

    /* This may seem surprising, but it's what Linux does:
     * it checks the protection of "oldact" after changing the signal action!
     */
    if (oldact != NULL && (prot_oldact == PROT_NONE || (prot_oldact & PROT_WRITE) == 0))
        return -EFAULT;

    if (oldact != NULL)
        memcpy(oldact, &tmp_oldact, SIGACTSZ);

    return 0;
}

void *
mmap_nofail(size_t length, int prot)
{
    void *ret = mmap(NULL, length, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ret != MAP_FAILED);
    return ret;
}

void
mprotect_nofail(void *addr, size_t len, int prot)
{
    int ret = mprotect(addr, len, prot);
    assert(ret == 0);
}

void
memrand(void *s, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        ((unsigned char *)s)[i] = (unsigned char)rand();
}

/****************************************************************************
 * Test sigaction with memory that is readable and writable.
 */

unsigned char test_rw_sys[(SIGACTSZ + MARGIN) * 2]; /* memory for "real" sigaction */
unsigned char test_rw_sim[(SIGACTSZ + MARGIN) * 2]; /* memory for simulated sigaction */

unsigned char *
init_test_rw(void)
{
    return test_rw_sys + MARGIN;
}

void
test_rw(int signum, unsigned char *act, unsigned char *oldact, size_t sigsetsize,
        bool enforce_cmp)
{
    int prot_rw = PROT_READ | PROT_WRITE;
    int ret_sys, ret_sim;

    memrand(test_rw_sys, sizeof(test_rw_sys));
    memcpy(test_rw_sim, test_rw_sys, sizeof(test_rw_sys));
    ret_sys = sys_sigaction(signum, act, oldact, sigsetsize);
    ret_sim =
        sim_sigaction(signum, act == NULL ? NULL : test_rw_sim + (act - test_rw_sys),
                      oldact == NULL ? NULL : test_rw_sim + (oldact - test_rw_sys),
                      sigsetsize, prot_rw, prot_rw);
    assert(ret_sys == ret_sim);
    assert(memcmp(test_rw_sys, test_rw_sim, sizeof(test_rw_sys)) == 0 || !enforce_cmp);
}

void
tests_rw(void)
{
    unsigned char *base = init_test_rw();
    int i;

    /* Read the initial handlers.  They are not always all 0 for some cases of
     * embedding DR into frameworks that link in pthreads or something so we
     * suspend the memcmp assert.
     */
    for (i = 1; i <= SIGMAX; i++)
        test_rw(i, NULL, base, SIGSETSIZE, false /*not init yet*/);

    /* Try each value of sigsetsize. */
    for (i = -1; i < SIGSETSIZE * 2 + 2; i++) {
        test_rw(SIG1, NULL, NULL, i, true);
        test_rw(SIG1, base, NULL, i, true);
        test_rw(SIG1, NULL, base, i, true);
        test_rw(SIG1, base, base, i, true);
    }

    /* Try each value of signum. */
    for (i = 0; i < SIGMAX + 2; i++) {
        test_rw(i, NULL, NULL, SIGSETSIZE, true);
        test_rw(i, base, NULL, SIGSETSIZE, true);
        test_rw(i, NULL, base, SIGSETSIZE, true);
        test_rw(i, base, base, SIGSETSIZE, true);
    }

    /* Try some random values. */
    for (i = 0; i < 1000; i++) {
        test_rw(rand() % (SIGMAX + 2), rand() % 2 == 0 ? NULL : base + rand() % SIGACTSZ,
                rand() % 2 == 0 ? NULL : base + rand() % SIGACTSZ,
                SIGSETSIZE + (rand() % 10 == 0 ? 1 : 0), true);
    }
}

/****************************************************************************
 * Test sigaction with memory with different protection.
 */

/* Write-only memory is not available under Linux. */
int test_prots[4] = { -1, PROT_NONE, PROT_READ, PROT_READ | PROT_WRITE };
unsigned char *test_prot_mem1; /* for "act" */
unsigned char *test_prot_mem2; /* for "oldact" */

int
init_test_prot(void)
{
    test_prot_mem1 = mmap_nofail(SIGACTSZ + 2 * MARGIN, PROT_READ | PROT_WRITE);
    test_prot_mem2 = mmap_nofail(SIGACTSZ + 2 * MARGIN, PROT_READ | PROT_WRITE);
    return sizeof(test_prots) / sizeof(*test_prots);
}

/* The protection is specified by t1 and t2, referring to test_prots.
 * The value 0 means a null pointer so there is no memory to protect.
 */
void
test_prot(int signum, int t1, int t2, size_t sigsetsize)
{
    unsigned char sim_new[SIGACTSZ + 2 * MARGIN];
    unsigned char sim_old[SIGACTSZ + 2 * MARGIN];
    const size_t sim_array_size = sizeof(sim_new);
    int prot1 = test_prots[t1];
    int prot2 = test_prots[t2];
    int ret_sys, ret_sim;

    memrand(sim_new, sim_array_size);
    memcpy(test_prot_mem1, sim_new, sim_array_size);
    memrand(sim_old, sim_array_size);
    memcpy(test_prot_mem2, sim_old, sim_array_size);

    if (t1 != 0)
        mprotect_nofail(test_prot_mem1, sim_array_size, prot1);
    if (t2 != 0)
        mprotect_nofail(test_prot_mem2, sim_array_size, prot2);
    ret_sys = sys_sigaction(signum, t1 == 0 ? NULL : test_prot_mem1 + MARGIN,
                            t2 == 0 ? NULL : test_prot_mem2 + MARGIN, sigsetsize);
    if (t1 != 0)
        mprotect_nofail(test_prot_mem1, sim_array_size, PROT_READ | PROT_WRITE);
    if (t2 != 0)
        mprotect_nofail(test_prot_mem2, sim_array_size, PROT_READ | PROT_WRITE);

    ret_sim = sim_sigaction(signum, t1 == 0 ? NULL : sim_new + MARGIN,
                            t2 == 0 ? NULL : sim_old + MARGIN, sigsetsize, prot1, prot2);

    assert(ret_sys == ret_sim ||
           /* 32-bit on a 64-bit kernel returns -ENXIO for invalid oact (i#1984) */
           (ret_sys == -ENXIO && ret_sim == -EFAULT));
    assert(memcmp(test_prot_mem1, sim_new, sim_array_size) == 0 &&
           memcmp(test_prot_mem2, sim_old, sim_array_size) == 0);
}

void
tests_prot(void)
{
    int n = init_test_prot();
    int i, j;

    /* Try each combination of protection. */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++)
            test_prot(SIG1, i, j, SIGSETSIZE);
    }

    /* Try some random values. */
    for (i = 0; i < 1000; i++) {
        test_prot(rand() % (SIGMAX + 2), rand() % n, rand() % n,
                  SIGSETSIZE + (rand() % 10 == 0 ? 1 : 0));
    }
}

/****************************************************************************
 * Test sigaction with some memory protection edge cases.
 */

/* Starting at test_edge_base we map two blocks of memory, each of test_edge_size.
 * The blocks are given different memory protection and we call sigaction with an
 * argument at the border between them.
 */
unsigned char *test_edge_base;
unsigned char *test_edge_middle;
size_t test_edge_size;

void
init_test_edge(void)
{
    size_t pagesize = sysconf(_SC_PAGESIZE);
    test_edge_size = (SIGACTSZ + (pagesize - 1)) & ~(pagesize - 1);
    test_edge_base = mmap_nofail(test_edge_size * 2, PROT_NONE);
    test_edge_middle = test_edge_base + test_edge_size;
}

void
test_edge(int prot1, int prot2, unsigned char *act, unsigned char *oldact, int ret)
{
    int ret_sys, ret_sim;

    assert(act == NULL || oldact == NULL);

    mprotect_nofail(test_edge_base, test_edge_size, prot1);
    mprotect_nofail(test_edge_base + test_edge_size, test_edge_size, prot2);

    ret_sys = sys_sigaction(SIG1, act, oldact, SIGSETSIZE);
    /* Check that the real syscall gave the expected return value. */
    assert(ret_sys == ret);
    /* If the real syscall succeeded, run the simulated syscall and compare. */
    if (ret == 0) {
        unsigned char tmp[SIGACTSZ];
        int prot = PROT_READ | PROT_WRITE;
        ret_sim = sim_sigaction(SIG1, act, tmp, SIGSETSIZE, prot, prot);
        assert(ret_sim == ret);
        assert(oldact == NULL || memcmp(oldact, tmp, SIGACTSZ) == 0);
    }
}

void
tests_edge(void)
{
    unsigned char *middle;
    int err = -EFAULT;

    init_test_edge();

    middle = test_edge_middle;

    test_edge(PROT_NONE, PROT_READ, middle, NULL, 0);
    test_edge(PROT_NONE, PROT_READ, middle - 1, NULL, err);
    test_edge(PROT_READ, PROT_NONE, middle - SIGACTSZ, NULL, 0);
    test_edge(PROT_READ, PROT_NONE, middle - SIGACTSZ + 1, NULL, err);

    test_edge(PROT_NONE, PROT_WRITE, NULL, middle, 0);
    test_edge(PROT_NONE, PROT_WRITE, NULL, middle - 1, err);
    test_edge(PROT_WRITE, PROT_NONE, NULL, middle - SIGACTSZ, 0);
    test_edge(PROT_WRITE, PROT_NONE, NULL, middle - SIGACTSZ + 1, err);
}

/****************************************************************************
 * Main function.
 */

int
main()
{
    tests_rw();
    tests_prot();
    tests_edge();

    print("all done\n");
    return 0;
}
