/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
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

/*
 * Private memmove for inclusion in libdrmemfuncs for isolation from libc in
 * shared DR but not in static DR.  We need this in addition to memcpy
 * and memset because the compiler will auto-issue a call to "memmove" when
 * it cannot prove there are is no overlap and that memcpy is safe to call.
 *
 * We assume that this is not performance-critical as it is rarely called
 * (in fact it only shows up once in the internal release build and not at
 * all in the external release build) and so we just use simple C code.
 */

#include "../globals.h"

#undef memmove

void *
memmove(void *dst, const void *src, size_t n)
{
    ssize_t i;
    byte *dst_b = (byte *)dst;
    const byte *src_b = (const byte *)src;
    if (dst < src)
        return memcpy(dst, src, n);
    for (i = n - 1; i >= 0; i--) {
        dst_b[i] = src_b[i];
    }
    return dst;
}
