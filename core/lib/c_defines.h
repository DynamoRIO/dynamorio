/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/*
 * c_defines.h: non-C++ defines separated out from globals_shared.h to work around
 * clang-format bugs with defines of C++ keywords "inline", "true", "false", "__try".
 */
/* clang-format off */ /* (work around clang-format bugs) */

#ifndef _C_DEFINES_H_
#define _C_DEFINES_H_ 1

/* DR_API EXPORT TOFILE dr_defines.h */
/* DR_API EXPORT BEGIN */
#ifndef __cplusplus
#    ifdef WINDOWS
#        define inline __inline
#    else
#        define inline __inline__
#    endif
#endif
/* DR_API EXPORT END */
/* DR_API EXPORT VERBATIM */
#ifndef __cplusplus
#    ifndef DR_DO_NOT_DEFINE_bool
#        ifdef DR__Bool_EXISTS
/* prefer _Bool as it avoids truncation casting non-zero to zero */
typedef _Bool bool;
#        else
/* char-sized for compatibility with C++ */
typedef char bool;
#        endif
#    endif
#    ifndef true
#        define true (1)
#    endif
#    ifndef false
#        define false (0)
#    endif
#endif
/* DR_API EXPORT END */

#if !defined(__cplusplus) && !defined(NOT_DYNAMORIO_CORE_PROPER) && \
    !defined(NOT_DYNAMORIO_CORE)
#    define __try __try_forbidden_construct /* see case 4461 */
#endif

#endif /* _C_DEFINES_H_ */
