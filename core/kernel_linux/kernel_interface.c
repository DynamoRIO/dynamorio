/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

#include "kernel_interface.h"

#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/string.h>

int
kernel_get_cpu_id(void)
{
    return smp_processor_id();
}

unsigned int
kernel_query_time_seconds(void)
{
    return (unsigned int)ktime_get_real_seconds();
}

#define KERNEL_ENV_MAX 20

typedef struct {
    char name[KERNEL_ENV_NAME_MAX];
    char value[KERNEL_ENV_VALUE_MAX];
} kernel_env_t;

static kernel_env_t env_vars[KERNEL_ENV_MAX];
static int env_count = 0;

const char *
kernel_getenv(const char *name)
{
    for (int i = 0; i < env_count; i++) {
        if (strncmp(name, env_vars[i].name, KERNEL_ENV_NAME_MAX) == 0) {
            return (const char *)env_vars[i].value;
        }
    }
    return NULL;
}
