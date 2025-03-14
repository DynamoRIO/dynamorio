/* **********************************************************
 * Copyright (c) 2015-2024 Google, Inc.  All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64) || defined(WINDOWS)
#    define WIN32_LEAN_AND_MEAN
#    define UNICODE  // For Windows headers.
#    define _UNICODE // For C headers.
#    define NOMINMAX // Avoid windows.h messing up std::min.
#    include <windows.h>
#else
#    include <sys/time.h>
#endif

namespace dynamorio {
namespace drmemtrace {

// XXX: DR should export this
#define INVALID_THREAD_ID 0
// We avoid collisions with DR's INVALID_PROCESS_ID by using our own name.
#define INVALID_PID -1
// A separate sentinel for an idle core with no software thread.
// XXX i#6703: Export this in scheduler.h as part of its API when we have
// the scheduler insert synthetic headers.
#define IDLE_THREAD_ID -1

// XXX: perhaps we should use a C++-ish stream approach instead
// This cannot be named ERROR as that conflicts with Windows headers.
#define ERRMSG(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)

// XXX: can we share w/ core DR?
#define IS_POWER_OF_2(x) ((x) != 0 && ((x) & ((x)-1)) == 0)

// XXX i#4399: DR should define a DEBUG-only assert.
#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* Nothing. */
#    define IF_DEBUG(x)    /* Nothing. */
#endif

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
#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))

#define NOTIFY(level, ...)                     \
    do {                                       \
        if (op_verbose.get_value() >= (level)) \
            dr_fprintf(STDERR, __VA_ARGS__);   \
    } while (0)

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
compute_log2(int64_t value)
{
    int i;
    for (i = 0; i < 63; i++) {
        if (value == int64_t(1) << i)
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

static inline bool
ends_with(const std::string &str, const std::string &with)
{
    size_t pos = str.rfind(with);
    if (pos == std::string::npos)
        return false;
    return (pos + with.size() == str.size());
}

static inline bool
starts_with(const std::string &str, const std::string &with)
{
    size_t pos = str.find(with);
    if (pos == std::string::npos)
        return false;
    return pos == 0;
}

static inline std::vector<std::string>
split_by(std::string s, const std::string &sep)
{
    size_t pos;
    std::vector<std::string> vec;
    if (s.empty())
        return vec;
    do {
        pos = s.find(sep);
        vec.push_back(s.substr(0, pos));
        s.erase(0, pos + sep.length());
    } while (pos != std::string::npos);
    return vec;
}

// Returns a timestamp with at least microsecond granularity.
// On UNIX this is an absolute timestamp; but on Windows where we had
// trouble with the GetSystemTime* functions not being granular enough
// it's the timestamp counter from the processor.
// (We avoid dr_get_microseconds() because not all targets link
// in the DR library.)
static inline uint64_t
get_microsecond_timestamp()
{
#if defined(_WIN32) || defined(_WIN64) || defined(WINDOWS)
    uint64_t res;
    QueryPerformanceCounter((LARGE_INTEGER *)&res);
    return res;
#else
    struct timeval time;
    if (gettimeofday(&time, nullptr) != 0)
        return 0;
    return time.tv_sec * 1000000 + time.tv_usec;
#endif
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _UTILS_H_ */
