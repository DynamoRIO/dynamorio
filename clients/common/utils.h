/* ***************************************************************************
 * Copyright (c) 2012-2026 Google, Inc.  All rights reserved.
 * ***************************************************************************/

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

#ifndef CLIENTS_COMMON_UTILS_H_
#define CLIENTS_COMMON_UTILS_H_

#include "dr_api.h"
#include "dr_project_wide_defines.h"

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* nothing */
#    define IF_DEBUG(x)    /* nothing */
#endif

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#    define IF_UNIX_ELSE(x, y) y
#else
#    define IF_WINDOWS(x)
#    define IF_UNIX_ELSE(x, y) x
#endif

/* Checks for both debug and release builds: */
#define USAGE_CHECK(x, msg) DR_ASSERT_MSG(x, msg)

static inline generic_func_t
cast_to_func(void *p)
{
#ifdef WINDOWS
#    pragma warning(push)
#    pragma warning(disable : 4055)
#endif
    return (generic_func_t)p;
#ifdef WINDOWS
#    pragma warning(pop)
#endif
}

#endif /* CLIENTS_COMMON_UTILS_H_ */
