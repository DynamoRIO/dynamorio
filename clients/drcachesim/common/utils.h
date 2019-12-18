/* **********************************************************
 * Copyright (c) 2015-2019 Google, Inc.  All rights reserved.
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

/* utils.h: utilities for cache simulator */

#ifndef _UTILS_H_
#define _UTILS_H_ 1

#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>

// XXX: perhaps we should use a C++-ish stream approach instead
// This cannot be named ERROR as that conflicts with Windows headers.
#define ERRMSG(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)

// XXX: can we share w/ core DR?
#define IS_POWER_OF_2(x) ((x) != 0 && ((x) & ((x)-1)) == 0)

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)

#if defined(X64) || defined(ARM)
#    define IF_REL_ADDRS(x) x
#else
#    define IF_REL_ADDRS(x)
#endif

#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((ptr_uint_t)(alignment)-1)))

#define BOOLS_MATCH(b1, b2) (!!(b1) == !!(b2))

#ifdef WINDOWS
/* Use special C99 operator _Pragma to generate a pragma from a macro */
#    if _MSC_VER <= 1200
#        define ACTUAL_PRAGMA(p) _Pragma(#        p)
#    else
#        define ACTUAL_PRAGMA(p) __pragma(p)
#    endif
/* Usage: if planning to typedef, that must be done separately, as MSVC will
 * not take _pragma after typedef.
 */
#    define START_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(push, 1))
#    define END_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(pop))
#else
#    define START_PACKED_STRUCTURE /* nothing */
#    define END_PACKED_STRUCTURE __attribute__((__packed__))
#endif

/* TODO(i#2924): Remove this and others like it once we stop supporting VS2013. */
#if defined(WINDOWS) && _MSC_VER < 1900
#    define CONSTEXPR const /* 'constexpr' not supported */
#else
#    define CONSTEXPR constexpr
#endif

#ifndef __has_cpp_attribute
#    define __has_cpp_attribute(x) 0 // Compatibility with non-clang compilers.
#endif
// We annotate to support building with -Wimplicit-fallthrough.
#if __has_cpp_attribute(clang::fallthrough)
#    define ANNOTATE_FALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#    define ANNOTATE_FALLTHROUGH [[gnu::fallthrough]]
#else
#    define ANNOTATE_FALLTHROUGH
#endif

#ifdef WINDOWS
#    define DIRSEP "\\"
#    define ALT_DIRSEP "/"
#    define IF_WINDOWS(x) x
#    define IF_UNIX(x)
#else
#    define DIRSEP "/"
#    define ALT_DIRSEP ""
#    define IF_WINDOWS(x)
#    define IF_UNIX(x) x
#endif

static inline int
compute_log2(int value)
{
    int i;
    for (i = 0; i < 31; i++) {
        if (value == 1 << i)
            return i;
    }
    // returns -1 if value is not a power of 2.
    return -1;
}

template <typename T>
std::string
to_hex_string(T integer)
{
    std::stringstream sstream;
    sstream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex
            << integer;
    return sstream.str();
}

#endif /* _UTILS_H_ */
