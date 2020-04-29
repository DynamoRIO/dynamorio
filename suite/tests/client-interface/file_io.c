/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

#ifdef UNIX
#    define _GNU_SOURCE
#    include <unistd.h>
#    include <sys/time.h>
#    include <sys/resource.h>
#    include <stdio.h>
#    include <sys/syscall.h>
#    include <errno.h>
struct compat_rlimit {
    unsigned int rlim_cur;
    unsigned int rlim_max;
};
#else
#    include <float.h>
#endif

#if defined(UNIX) && defined(SYS_prlimit64)
int
sys_prlimit(pid_t pid, int resource, const struct rlimit64 *new_limit,
            struct rlimit64 *old_limit)
{
    return syscall(SYS_prlimit64, pid, resource, new_limit, old_limit);
}
#endif

int
main()
{
#ifdef UNIX
    /* test i#357 by trying to close the client's file */
    int i;
    for (i = 3; i < 5000; i++) {
        dup2(0, i);
        close(i);
    }

    /* further tests of i#357 -steal_fds */
    struct rlimit rlimit, new_rlimit;
    if (getrlimit(RLIMIT_NOFILE, &rlimit) != 0) {
        perror("getrlimit failed");
        return 1;
    }

    /* DR should have taken -steal_fds == 96.  To avoid hardcoding the 4096
     * typical max we assume it's just a power of 2.
     */
    if ((rlimit.rlim_max & (rlimit.rlim_max - 1)) == 0) {
        fprintf(stderr,
                "RLIMIT_NOFILE max is %llu but shouldn't be power of 2 "
                "under DR\n",
                /* The size of rlim_max depends on whether __USE_FILE_OFFSET64
                 * is defined. We simply cast it to 8-byte for printing.
                 */
                (unsigned long long)rlimit.rlim_max);
        /* We continue to make it easier to run this app natively. */
    }

    /* setrlimit with lower soft value */
    new_rlimit.rlim_max = rlimit.rlim_max;
    new_rlimit.rlim_cur = rlimit.rlim_cur / 2;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) != 0) {
        fprintf(stderr,
                "Error: fail(%d) to set rlimit for RLIMIT_NOFILE with "
                "lower soft value\n",
                errno);
        return 1;
    }
    /* setrlimit with the same value */
    new_rlimit = rlimit;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) != 0) {
        fprintf(stderr,
                "Error: fail(%d) to set rlimit for RLIMIT_NOFILE "
                "back to the same value\n",
                errno);
        return 1;
    }
    /* setrlimit with higher value */
    new_rlimit.rlim_cur++;
    new_rlimit.rlim_max++;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) == 0 || errno != EPERM) {
        fprintf(stderr,
                "Error: should fail to set rlimit for RLIMIT_NOFILE with higher value\n");
        return 1;
    }
    /* ensure can't raise hard once lower it */
    new_rlimit.rlim_max = rlimit.rlim_max - 1;
    new_rlimit.rlim_cur = rlimit.rlim_cur / 2;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) != 0) {
        fprintf(stderr,
                "Error: fail(%d) to set rlimit for RLIMIT_NOFILE with "
                "lower soft + hard values\n",
                errno);
        return 1;
    }
    new_rlimit.rlim_max = rlimit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) == 0 || errno != EPERM) {
        fprintf(stderr, "Error: should fail to raise hard limit\n");
        return 1;
    }
    /* test invalid values */
    new_rlimit.rlim_max = rlimit.rlim_max;
    new_rlimit.rlim_cur = rlimit.rlim_max + 1;
    if (setrlimit(RLIMIT_NOFILE, &new_rlimit) == 0 || errno != EINVAL) {
        fprintf(stderr, "Error: should fail with EINVAL if max>cur\n");
        return 1;
    }

#    if defined(X86_32)
    /* Same test but w/ compat struct */
    struct compat_rlimit crlimit;
    if (syscall(SYS_getrlimit, RLIMIT_NOFILE, &crlimit) != 0) {
        perror("getrlimit failed");
        return 1;
    }
    if ((crlimit.rlim_max & (crlimit.rlim_max - 1)) == 0) {
        fprintf(stderr, "RLIMIT_NOFILE max is %d but shouldn't be power of 2 under DR\n",
                crlimit.rlim_max);
    }
#    endif

#    ifdef SYS_prlimit64
    /* test sys_prlimit */
    struct rlimit64 rlim64, new_rlim64;
    /* get rlimit */
    if (sys_prlimit(0, RLIMIT_NOFILE, NULL, &rlim64) != 0) {
        fprintf(stderr, "Error: fail(%d) to get prlimit for RLIMIT_NOFILE\n", errno);
        return 1;
    }
    /* set rlimit */
    new_rlim64.rlim_max = rlim64.rlim_max;
    new_rlim64.rlim_cur = rlim64.rlim_cur / 2;
    if (sys_prlimit(0, RLIMIT_NOFILE, &new_rlim64, NULL) != 0) {
        fprintf(stderr,
                "Error: fail(%d) to set prlimit for RLIMIT_NOFILE with "
                "lower soft value\n",
                errno);
        return 1;
    }
    new_rlim64 = rlim64;
    if (sys_prlimit(0, RLIMIT_NOFILE, &new_rlim64, NULL) != 0) {
        fprintf(stderr,
                "Error: fail(%d) to set prlimit for RLIMIT_NOFILE back "
                "to the same value\n",
                errno);
        return 1;
    }
    new_rlim64.rlim_cur++;
    new_rlim64.rlim_max++;
    if (sys_prlimit(0, RLIMIT_NOFILE, &new_rlim64, NULL) == 0) {
        fprintf(stderr,
                "Error: should fail to set prlimit for RLIMIT_NOFILE with higher "
                "value\n");
        return 1;
    }
    new_rlim64 = rlim64;
    rlim64.rlim_cur = 0;
    rlim64.rlim_max = 0;
    if (sys_prlimit(0, RLIMIT_NOFILE, &new_rlim64, &rlim64) != 0 ||
        new_rlim64.rlim_cur != rlim64.rlim_cur ||
        new_rlim64.rlim_max != rlim64.rlim_max) {
        fprintf(stderr, "Error: fail(%d) to set/get rlimit\n", errno);
    }
#    endif

#endif
    /* Now test any floating-point printing at exit time in DR or a
     * client by unmasking div-by-zero, which d_r_vsnprintf_float()
     * relies on being masked (i#1213).
     * On Linux the d_r_vsnprintf_float() code currently doesn't do a
     * divide but we check there nonetheless.
     */
#ifdef WINDOWS
    _control87(_MCW_EM & (~_EM_ZERODIVIDE), _MCW_EM);
#else
    int cw = 0x033; /* finit sets to 0x037 and we remove divide=0x4 */
    __asm("fldcw %0" : : "m"(cw));
#endif
    return 0;
}
