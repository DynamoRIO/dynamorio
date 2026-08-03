/* **********************************************************
 * Copyright (c) 2010-2026 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

#ifndef _DR_PROJECT_WIDE_DEFINES_H_
#define _DR_PROJECT_WIDE_DEFINES_H_

/* Defines shared across the core, tests, clients, and standalone applications.
 * Due to differing reporting and exiting environments, we cannot easily share
 * assert or logging defines, but we share other cross-domain utilities here.
 */

#ifndef LINUX_KERNEL
#    include <stdint.h>
#endif

/* Alignment helpers.  The alignment must be a power of 2. */
#define ALIGNED(x, alignment) ((((uintptr_t)x) & ((alignment) - 1)) == 0)
#define ALIGN_FORWARD(x, alignment) \
    ((((uintptr_t)x) + ((alignment) - 1)) & (~((uintptr_t)(alignment) - 1)))
#define ALIGN_FORWARD_UINT(x, alignment) \
    ((((unsigned int)x) + ((alignment) - 1)) & (~((alignment) - 1)))
#define ALIGN_BACKWARD(x, alignment) (((uintptr_t)x) & (~((uintptr_t)(alignment) - 1)))
#define PAD(length, alignment) (ALIGN_FORWARD((length), (alignment)) - (length))
#define ALIGN_MOD(addr, size, alignment) \
    ((((uintptr_t)addr) + (size) - 1) & ((alignment) - 1))
#define CROSSES_ALIGNMENT(addr, size, alignment) \
    (ALIGN_MOD(addr, size, alignment) < (size) - 1)
/* number of bytes you need to shift addr forward so that it's !CROSSES_ALIGNMENT */
#define ALIGN_SHIFT_SIZE(addr, size, alignment)            \
    (CROSSES_ALIGNMENT(addr, size, alignment)              \
         ? ((size) - 1 - ALIGN_MOD(addr, size, alignment)) \
         : 0)

/* Buffer helpers. */
#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0
#define NULL_TERMINATE_SIZED_BUFFER(buf, size) (buf)[(size) - 1] = 0
#define BUFFER_ROOM_LEFT_W(wbuf) (BUFFER_SIZE_ELEMENTS(wbuf) - wcslen(wbuf) - 1)
#define BUFFER_ROOM_LEFT(abuf) (BUFFER_SIZE_ELEMENTS(abuf) - strlen(abuf) - 1)

/* Bit test helpers. */
/* Check if all bits in mask are set in var. */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* Check if any bit in mask is set in var.  Use this to check a single bit. */
#define TESTANY(mask, var) (((mask) & (var)) != 0)

#endif /* _DR_PROJECT_WIDE_DEFINES_H_ */
