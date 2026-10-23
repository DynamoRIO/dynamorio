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

#ifndef _DYNAMORIO_IOCTL_H_
#define _DYNAMORIO_IOCTL_H_

/* Interface between the DynamoRIO kernel module and its user-space control utility.
 * This header is shared by both sides.
 */

#include <linux/ioctl.h>

#define DYNAMORIO_DEVICE_NAME "dynamorio"

/* Registering the misc device creates this node when the module is loaded. */
#define DYNAMORIO_DEVICE_PATH "/dev/" DYNAMORIO_DEVICE_NAME

/* 0xDF is not listed in the kernel's ioctl-number table:
 * https://docs.kernel.org/userspace-api/ioctl/ioctl-number.html
 * Ioctls go to the opened device, so another device using the same magic normally
 * does not interfere.  A distinct magic is meant to help catch wrong-device calls.
 */
#define DYNAMORIO_IOCTL_MAGIC 0xDF

struct dynamorio_init_args {
    /* User pointer to the NUL-terminated options string. */
    const char *options;
};

/* _IOW means user space writes the argument and the kernel reads it. */
#define DYNAMORIO_IOCTL_INIT _IOW(DYNAMORIO_IOCTL_MAGIC, 0, struct dynamorio_init_args)

#endif /* _DYNAMORIO_IOCTL_H_ */
