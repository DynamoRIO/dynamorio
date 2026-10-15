/* **********************************************************
 * Copyright (c) 2014-2026 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/*
 * Test of fork with extra threads in the parent at fork time.
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h> /* for wait and mmap */
#include <sys/wait.h>  /* for wait */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tools.h"
#include "condvar.h"
#include "thread.h"

static void *sideline_continue;

static THREAD_FUNC_RETURN_TYPE
thread_function(void *arg)
{
    wait_cond_var(sideline_continue);
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(int argc, char **argv)
{
    pid_t child;
    sideline_continue = create_cond_var();
#define NUM_THREADS 4
    thread_t thread[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        thread[i] = create_thread(thread_function, NULL);
    }
    if (find_dynamo_library())
        print("parent is running under DynamoRIO\n");
    else
        print("parent is running natively\n");
    child = fork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        pid_t result;
        result = waitpid(child, NULL, 0);
        assert(result == child);
        print("child has exited\n");
        signal_cond_var(sideline_continue);
        for (int i = 0; i < NUM_THREADS; ++i) {
            join_thread(thread[i]);
        }
        destroy_cond_var(sideline_continue);
    } else {
        if (find_dynamo_library())
            print("child is running under DynamoRIO\n");
        else
            print("child is running natively\n");
    }
}
