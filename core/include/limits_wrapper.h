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

#ifndef _LIMITS_WRAPPER_H_
#define _LIMITS_WRAPPER_H_

#include "configure.h"

#ifdef LINUX_KERNEL
/* The Linux kernel does not have a standard limits.h for basic C types,
 * but it defines some integer limits in <linux/kernel.h>. We include that
 * and manually define the standard C char/byte limits here.
 */
#    include <linux/kernel.h>

/* Number of bits in a `char'.  */
#    define CHAR_BIT 8

/* Minimum and maximum values a `signed char' can hold.  */
#    define SCHAR_MIN (-128)
#    define SCHAR_MAX 127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0.)  */
#    define UCHAR_MAX 255

/* Minimum and maximum values a `char' can hold.  */
#    ifdef __CHAR_UNSIGNED__
#        define CHAR_MIN 0
#        define CHAR_MAX UCHAR_MAX
#    else
#        define CHAR_MIN SCHAR_MIN
#        define CHAR_MAX SCHAR_MAX
#    endif
#else
#    include <limits.h>
#endif

#endif /* _LIMITS_WRAPPER_H_ */
