/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * memfuncs.asm: Contains our custom memcpy and memset routines.
 *
 * The core certainly needs its own memcpy and memset for libc isolation as
 * described below, when a shared library and used for early injection.
 * We also use these to avoid glibc-versioned imports in our auxiliary tools
 * such as drrun (i#1504).  However, when DR is built as a static library, these
 * functions can conflict with the application's own versions (even when
 * marking as weak): i#3348.  Since a static-library DR will not be taking control
 * early, and since these functions should always be re-entrant, we simply
 * do not include them in libdynamorio_static and it imports from libc.
 * We accomplish that by separating these into their own library.
 * There is a downside to using libc routines: they can be intercepted,
 * such as by sanitizer tools.  There are other interoperability issues
 * with such tools, though, so we live with the potential problem of
 * the app overriding DR's memory function as the lesser of two evils,
 * assuming symbol conflicts are more prevalent and serious.
 * XXX i#3348: We can try to rename DR's internal uses of functions like
 * these to avoid interception.
 */

#include "../asm_defines.asm"
#include "x86_asm_defines.asm" /* PUSHGPR, POPGPR, etc. */
START_FILE

/***************************************************************************/
#ifdef UNIX

/* i#46: Implement private memcpy and memset for libc isolation.  With early
 * injection we need libdynamorio.so to have zero dependencies so we can
 * more easily bootstrap-load our own library.  Even with late injection, if we
 * import memcpy and memset from libc in the normal way, the application can
 * override those definitions and intercept them.  In particular, this occurs when
 * running an app that links in a sanitizer tool's runtime.
 *
 * Since we already need a reasonably efficient assembly memcpy implementation
 * for safe_read, we go ahead and reuse the code (via the REP_STRING_OP macros)
 * for these private memcpy and memset functions.
 * XXX: See comment on REP_STRING_OP about maybe using SSE instrs.  It's more
 * viable for memcpy and memset than for safe_read_asm.
 */

/* Private memcpy.
 */
        DECLARE_FUNC(memcpy)
        /* i#3315: We mark as weak to avoid app conflicts in our static libs drinjectlib
         * and drfrontendlib (we omit completely from static core DR).  There is no
         * weak support in nasm on Mac so we live with potential conflicts there.
         */
        WEAK(memcpy)
GLOBAL_LABEL(memcpy:)
        ARGS_TO_XDI_XSI_XDX()           /* dst=xdi, src=xsi, n=xdx */
        mov    REG_XAX, REG_XDI         /* Save dst for return. */
        /* Copy xdx bytes, align on src. */
        REP_STRING_OP(memcpy, REG_XSI, movs)
        RESTORE_XDI_XSI()
        ret                             /* Return original dst. */
        END_FUNC(memcpy)

/* Private memset.
 */
        DECLARE_FUNC(memset)
        /* See the discussion above on marking weak. */
        WEAK(memset)
GLOBAL_LABEL(memset:)
        ARGS_TO_XDI_XSI_XDX()           /* dst=xdi, val=xsi, n=xdx */
        push    REG_XDI                 /* Save dst for return. */
        test    esi, esi                /* Usually val is zero. */
        jnz     make_val_word_size
        xor     eax, eax
do_memset:
        /* Set xdx bytes, align on dst. */
        REP_STRING_OP(memset, REG_XDI, stos)
        pop     REG_XAX                 /* Return original dst. */
        RESTORE_XDI_XSI()
        ret

        /* Create pointer-sized value in XAX using multiply. */
make_val_word_size:
        and     esi, HEX(ff)
# ifdef X64
        mov     rax, HEX(0101010101010101)
# else
        mov     eax, HEX(01010101)
# endif
        /* Use two-operand imul to avoid clobbering XDX. */
        imul    REG_XAX, REG_XSI
        jmp     do_memset
        END_FUNC(memset)


# ifndef MACOS /* XXX: attribute alias issue, plus using nasm */
/* gcc emits calls to these *_chk variants in release builds when the size of
 * dst is known at compile time.  In C, the caller is responsible for cleaning
 * up arguments on the stack, so we alias these *_chk routines to the non-chk
 * routines and rely on the caller to clean up the extra dst_len arg.
 */
.global __memcpy_chk
.hidden __memcpy_chk
WEAK(__memcpy_chk)
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
WEAK(__memset_chk)
.set __memset_chk,memset
# endif

#endif /* UNIX */


END_FILE
