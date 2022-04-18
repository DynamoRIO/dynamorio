/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2005-2007 Determina Corp. */

/*
 * drmarker.c
 *
 * shared between core/gui nodemgr, functions to tell if a process is running
 * under dr and pass information out of the running process
 *
 */

#include "configure.h"
#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
#    include "../globals.h"
#    include "os_private.h"
#    include "../nudge.h"
#    include "../module_shared.h"
#else /* NOT_DYNAMORIO_CORE */
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h> /* no longer included in globals_shared.h */
#    include "globals_shared.h"
#endif

#include "ntdll.h"
#include "drmarker.h"

#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
#    define READ_FUNC nt_read_virtual_memory
#    define DR_MARKER_VERSION_CURRENT DR_MARKER_VERSION_2
#else                               /* NOT_DYNAMORIO_CORE */
#    pragma warning(disable : 4054) /* from function pointer to data pointer */
/* complains about size_t* vs SIZE_T* */
/* 'unsigned long *' differs to slightly different base types from 'int *' */
#    pragma warning(disable : 4057)
#    pragma warning(disable : 4100) /* 'envp' : unreferenced formal parameter */
#    ifndef byte
typedef unsigned char byte;
#    endif
#    define PAGE_START(x) (((ptr_uint_t)(x)) & ~((PAGE_SIZE)-1))
#    define PAGE_SIZE (4 * 1024)
#    define READ_FUNC ReadProcessMemory
#endif

#define OP_jmp_byte 0xe9

#ifdef NOT_DYNAMORIO_CORE
/* avoid having to link in ntdll.c */
bool
is_wow64_process(HANDLE hProcess)
{
    /* IsWow64Pocess is only available on XP+ */
    typedef DWORD(WINAPI * IsWow64Process_Type)(HANDLE hProcess, PBOOL isWow64Process);
    static HANDLE kernel32_handle;
    static IsWow64Process_Type IsWow64Process;
    if (kernel32_handle == NULL)
        kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (IsWow64Process == NULL && kernel32_handle != NULL) {
        IsWow64Process =
            (IsWow64Process_Type)GetProcAddress(kernel32_handle, "IsWow64Process");
    }
    if (IsWow64Process == NULL) {
        /* should be NT or 2K */
        return false;
    } else {
        BOOL res;
        if (!IsWow64Process(hProcess, &res))
            return false;
        return CAST_TO_bool(res);
    }
}
#endif

/* For 32-bit build, supports looking for x64 marker (in WOW64 process).
 * For 64-bit build, only supports looking for x64 marker.
 */
static int
read_and_verify_dr_marker_common(HANDLE process, dr_marker_t *marker, bool x64)
{
    /* clang-format mis-parses this function! */
    /* clang-format off */
    byte buf[8]; /* only needs to be 5, but dword pad just in case */
    size_t res;
    void *target = NULL;
#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
    GET_NTDLL(DR_MARKER_HOOKED_FUNCTION, DR_MARKER_HOOKED_FUNCTION_ARGS);
    void *hook_func = (void *)DR_MARKER_HOOKED_FUNCTION;
#else
    if (IF_X64_ELSE(!x64, x64 && !is_wow64_process(NT_CURRENT_PROCESS)))
        return DR_MARKER_ERROR;
    if (x64) {
#    ifndef X64
        uint64 hook_func =
            get_proc_address_64(get_module_handle_64(L_DR_MARKER_HOOKED_DLL),
                                DR_MARKER_HOOKED_FUNCTION_STRING);
        uint64 landing_pad = 0;
        if (hook_func == 0)
            return DR_MARKER_ERROR;
        if (!NT_SUCCESS(
                nt_wow64_read_virtual_memory64(process, hook_func, buf, 5, &res)) ||
            res != 5) {
            return DR_MARKER_ERROR;
        }
        if (buf[0] != OP_jmp_byte)
            return DR_MARKER_NOT_FOUND;

        /* jmp offset + EIP (after jmp = hook_func + size of jmp (5 bytes)) */
        /* for 64-bit, the target is stored in front of the trampoline */
        landing_pad = *(int *)&buf[1] + hook_func + 5 - 8;
        if (!NT_SUCCESS(
                nt_wow64_read_virtual_memory64(process, landing_pad, buf, 8, &res)) ||
            res != 8U)
            return DR_MARKER_ERROR;
        /* trampoline address is stored at the top of the landing pad for 64-bit */
        target = (void *)PAGE_START(*(ptr_int_t *)buf);
    } else {
#    endif /* !X64 */
        void *hook_func = (void *)GetProcAddress(GetModuleHandle(DR_MARKER_HOOKED_DLL),
                                                 DR_MARKER_HOOKED_FUNCTION_STRING);
#endif
        void *landing_pad;
        if (hook_func == NULL)
            return DR_MARKER_ERROR;
        if (!READ_FUNC(process, hook_func, buf, 5, &res) || res != 5)
            return DR_MARKER_ERROR;
        if (buf[0] != OP_jmp_byte)
            return DR_MARKER_NOT_FOUND;

        /* jmp offset + EIP (after jmp = hook_func + size of jmp (5 bytes)) */
        landing_pad = (void *)(*(int *)&buf[1] + (ptr_int_t)hook_func + 5);
        /* for 64-bit, the target is stored in front of the trampoline */
        if (x64)
            landing_pad = (byte *)landing_pad - 8;
        /* see emit_landing_pad_code() for layout of landing pad */
        if (!READ_FUNC(process, landing_pad, buf, (x64 ? 8 : 5), &res) ||
            res != (x64 ? 8U : 5U))
            return DR_MARKER_ERROR;
        if (x64) {
            /* trampoline address is stored at the top of the landing pad for 64-bit */
            target = (void *)PAGE_START(*(ptr_int_t *)buf);
        } else {
            /* jmp offset + EIP (after jmp = landing_pad + size of jmp (5 bytes)) */
            target = (void *)PAGE_START(*(int *)&buf[1] + (ptr_int_t)landing_pad + 5);
        }
#if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
    }
#endif

    if (target == NULL)
        return DR_MARKER_ERROR;
    if (!READ_FUNC(process, target, marker, sizeof(dr_marker_t), &res) ||
        res != sizeof(dr_marker_t)) {
        return DR_MARKER_NOT_FOUND;
    }

    if (dr_marker_verify(process, marker)) {
        return DR_MARKER_FOUND;
    }

    return DR_MARKER_NOT_FOUND; /* probably some other hooker */
/* clang-format on */
}

#ifndef X64
int
read_and_verify_dr_marker_64(HANDLE process, dr_marker_t *marker)
{
    return read_and_verify_dr_marker_common(process, marker, true);
}
#endif

int
read_and_verify_dr_marker(HANDLE process, dr_marker_t *marker)
{
    return read_and_verify_dr_marker_common(process, marker, IF_X64_ELSE(true, false));
}

/* FIXME : in the future we may want to obfuscate so is not a constant? */
/* generated by dropping hand on number keypad and converting to hex */
/* we pass in the process handle to dr_marker_verify and dr_marker_magic_init
 * in case we decide to make the magic numbers process specific (xor w/pid or
 * something similar) */
#define DR_MARKER_MAGIC1 0xB1D2AE58
#define DR_MARKER_MAGIC2 0xCA50C356
#define DR_MARKER_MAGIC3 0x63000089
#define DR_MARKER_MAGIC4 0x3FA898F0

bool
dr_marker_verify(HANDLE process, dr_marker_t *marker)
{
    return (marker->magic1 == DR_MARKER_MAGIC1 && marker->magic2 == DR_MARKER_MAGIC2 &&
            marker->magic3 == DR_MARKER_MAGIC3 && marker->magic4 == DR_MARKER_MAGIC4);
}

#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
/* takes a dr marker with all non magic fields filled in and fills in the
 * magic fields */
static void
dr_marker_magic_init(HANDLE process, dr_marker_t *marker)
{
    marker->magic1 = DR_MARKER_MAGIC1;
    marker->magic2 = DR_MARKER_MAGIC2;
    marker->magic3 = DR_MARKER_MAGIC3;
    marker->magic4 = DR_MARKER_MAGIC4;
}

#    ifdef HOT_PATCHING_INTERFACE
/* FIXME: Including hotpatch.h here breaks the share module because it doesn't
 *        define a whole bunch of types like app_pc.  Once that is fixed, this
 *        extern declaration should be removed and hotpatch.h included; see
 *        case 4999.
 */
extern void *hotp_policy_status_table; /* See FIXME above. */
extern read_write_lock_t *
hotp_get_lock(void); /* See FIXME above. */
#    endif

void
init_dr_marker(dr_marker_t *marker)
{
    /* not calling memset b/c windbg_cmds is large */
#    ifdef DEBUG
    marker->flags = DR_MARKER_DEBUG_BUILD;
#    else
    marker->flags = DR_MARKER_RELEASE_BUILD;
#    endif
#    ifdef PROFILE
    marker->flags = DR_MARKER_PROFILE_BUILD;
#    endif
    /* make sure we set one of the above flags and not more then one */
    ASSERT(TESTANY(DR_MARKER_BUILD_TYPES, marker->flags) &&
           ((DR_MARKER_BUILD_TYPES & marker->flags) &
            ((DR_MARKER_BUILD_TYPES & marker->flags) - 1)) == 0);
    /* TODO : add any additional flags? */
    marker->build_num = BUILD_NUMBER;
    marker->dr_base_addr = get_module_base((app_pc)init_dr_marker);
    marker->dr_generic_nudge_target = (void *)generic_nudge_target;
#    ifdef HOT_PATCHING_INTERFACE
    marker->dr_hotp_policy_status_table = (void *)hotp_policy_status_table;
#    else
    marker->dr_hotp_policy_status_table = NULL;
#    endif
    marker->dr_marker_version = DR_MARKER_VERSION_CURRENT;
    marker->stats = get_dr_stats();
    dr_marker_magic_init(NT_CURRENT_PROCESS, marker);
    marker->windbg_cmds[0] = '\0';
}

#    ifdef HOT_PATCHING_INTERFACE
void *
get_drmarker_hotp_policy_status_table()
{
    dr_marker_t *dr_marker = get_drmarker();

    ASSERT_OWN_READWRITE_LOCK(true, hotp_get_lock());

    if (dr_marker == NULL) /* dr_marker_t has been initialized. */
        return NULL;

    return dr_marker->dr_hotp_policy_status_table;
}

void
set_drmarker_hotp_policy_status_table(void *new_table)
{
    dr_marker_t *dr_marker = get_drmarker();

    ASSERT_OWN_WRITE_LOCK(true, hotp_get_lock());

    /* We don't want to write to the dr_marker_t before it is initialized;  we
     * could get an exception.
     */
    if (dr_marker == NULL) /* Part of fix for case 5367. */
        return;
    /* Ok, dr_marker_t has been initialized. */

    /* It is ok to do this memory protection change here even though this can
     * be any arbitrary time due to the nature of nudge.  This is because
     * once initialized, the dr_marker_t isn't touched by any one except the hot
     * patch nudge.
     *
     * TODO: In future other parts of the core may need to change the dr_marker_t.
     *       It might be a good idea to introduce a lock for the dr_marker_t and
     *       generic accessor functions.
     */
    make_writable((byte *)dr_marker, INTERCEPTION_CODE_SIZE);
    dr_marker->dr_hotp_policy_status_table = new_table;
    make_unwritable((byte *)dr_marker, INTERCEPTION_CODE_SIZE);
}
#    endif /* HOT_PATCHING_INTERFACE */

#endif /* !NOT_DYNAMORIO_CORE && !NOT_DYNAMORIO_CORE_PROPER */
