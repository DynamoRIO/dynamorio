/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
 * ********************************************************** */

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

/*
 * os_asm_defines.asm - OS-specific assembly defines
 */

#ifndef _OS_ASM_DEFINES_ASM_
#define _OS_ASM_DEFINES_ASM_ 1

/* XXX: keep in sync with TLS_MAGIC_OFFSET, etc. in os.c.
 * We have checks in os_tls_init() and privload_mod_tls_init().
 */
#if defined(UNIX) && defined(X86)
# ifdef X64
#  ifdef HASHTABLE_STATISTICS
#   define TLS_MAGIC_OFFSET_ASM  104
#   define TLS_SELF_OFFSET_ASM    96
#  else
#   define TLS_MAGIC_OFFSET_ASM   96
#   define TLS_SELF_OFFSET_ASM    88
#  endif
#  define TLS_APP_SELF_OFFSET_ASM 16
# else
#  ifdef HASHTABLE_STATISTICS
#   define TLS_MAGIC_OFFSET_ASM   52
#   define TLS_SELF_OFFSET_ASM    48
#  else
#   define TLS_MAGIC_OFFSET_ASM   48
#   define TLS_SELF_OFFSET_ASM    44
#  endif
#  define TLS_APP_SELF_OFFSET_ASM  8
# endif
#endif

#endif /* _OS_ASM_DEFINES_ASM_ */
