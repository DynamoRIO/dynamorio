/* **********************************************************
 * Copyright (c) 2013-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
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

#include "asm_defines.asm"
START_FILE

#ifdef X64
# define FREE_LIBRARY_NAME FreeLibrary
#else
# define FREE_LIBRARY_NAME FreeLibrary@4
#endif
DECL_EXTERN(FREE_LIBRARY_NAME)
DECL_EXTERN(process_attach)

/* DllMain for win32/pre_inject.c
 * if we start including in multiple dlls will need custom entry names or ifdefs
 */
    /* We want to unload ourselves on process attach, regardless of
     * whether we inject DR or not.
     * One strategy is to return false:
     *   see msdn DllMain docs, return value ignored for all cases other than
     *   DLL_PROCESS_ATTACH, in which case returning false means initialization
     *   failed and the dll is unloaded (exactly what we want!!) if it was loaded
     *   by load library (it terminates process if was loaded as part of process
     *   initialization, but that's not our case)
     * However, that doesn't work when shimeng gets in the way and initializes us,
     * as it ignores the return value.  Furthermore, it causes a "dll init
     * failure" popup on NT in every process.  So instead we construct a call
     * to FreeLibrary that will return to our caller, skipping our freed code.
     * FreeLibrary returns non-zero on success, zero on failure -- so if
     * it fails we'll have the popup on NT and the non-removal w/ shimeng,
     * but on success we'll be returning the proper value and preventing the popup.
     * WARNING: we're freeing our library in the middle of loader code
     * initializing us -- in practice the loader seems to handle it gracefully
     * and never blindly uses a handle to us (at least we see no exceptions),
     * but future loader code could be more fragile.
     * In fact, see i#1522: the win8 loader cannot handle this, and hangs/crashes
     * at process exit when it tries to reference some unloaded data.
     * On win8 we do the best we can for now which is return false.
     */
#define DLL_PROCESS_ATTACH 1 /* from winnt.h */
#ifdef X64
# define DLL_MAIN_STDCALL_NAME DllMain
#else
# define DLL_MAIN_STDCALL_NAME DllMain@12
#endif
        DECLARE_FUNC(DLL_MAIN_STDCALL_NAME)
GLOBAL_LABEL(DLL_MAIN_STDCALL_NAME:)
        cmp      ARG2, DLL_PROCESS_ATTACH
        jne      DllMain_not_pattach
#ifdef X64
        /* push gets us back to 16 byte alignment, ready for next call */
        push     rcx /* preserve arg1, which is caller-saved */
#endif
        CALLC0(process_attach)
        cmp      al, 0
        je       do_not_unload
#ifdef X64
        /* The 32 bytes of shadow space (4 args' worth) are there regardless
         * of the number of args: we have 3, FreeLibrary has 1, so no
         * change to stack.  We only need to preserve arg1.
         */
        pop      rcx
#else
        /* we and FreeLibrary are stdcall, so we have to change the
         * current 3 args + retaddr for our call to 1 arg + retaddr
         * to pretend our caller called FreeLibrary
         */
        /* copy ret addr to 2 slots down */
        mov      eax, [esp]
        mov      [8 + esp], eax
        /* copy our hModule arg 2 slots down to become arg for FreeLibrary */
        mov      eax, [4 + esp]
        mov      [12 + esp], eax
        /* adjust esp and we're off */
        add      esp, 8
#endif
        jmp      FREE_LIBRARY_NAME

do_not_unload:
        /* i#1522: the win8 loader can't handle our self-unload, so we return false
         * instead, which will get us unloaded in all cases except when shimeng
         * is in the way.  We just live w/ remaining in the address space there.
         */
        mov      eax, 0
#ifdef X64
        /* repair the stack and return */
        pop      rcx
        ret
#else
        ret      HEX(0c)
#endif

DllMain_not_pattach:
        mov      eax, 1
#ifdef X64
        ret
#else
        ret      HEX(0c)
#endif
        END_FUNC(DLL_MAIN_STDCALL_NAME)


END_FILE
