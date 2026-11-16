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

#include <linux/errno.h>

#ifdef KERNEL_MODULE_SELF_CHECKS

/* Fails the enclosing self-check if |cond| is false.  Unlike ASSERT, this reports the
 * failure and returns rather than terminating, which in the kernel is a panic.
 */
#    define EXPECT(cond)                                                         \
        do {                                                                     \
            if (!(cond)) {                                                       \
                print_file(STDERR, "Self-check failed at %s:%d: %s\n", __func__, \
                           __LINE__, #cond);                                     \
                return false;                                                    \
            }                                                                    \
        } while (0)

typedef struct {
    const char *name;
    /* Returns whether the self-check passed. */
    bool (*func)(void);
} self_check_t;

/* Terminated by an entry with a NULL func. */
static const self_check_t self_checks[] = {
    { NULL, NULL },
};

int
dynamorio_run_self_checks(void)
{
    int num_failed = 0;
    for (const self_check_t *check = self_checks; check->func != NULL; check++) {
        bool passed = check->func();
        print_file(STDERR, "Self-check %s: %s\n", check->name, passed ? "PASS" : "FAIL");
        if (!passed) {
            num_failed++;
        }
    }
    return num_failed;
}

#else

int
dynamorio_run_self_checks(void)
{
    return -EOPNOTSUPP;
}

#endif /* KERNEL_MODULE_SELF_CHECKS */
