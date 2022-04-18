/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * inject.c - injects dynamo into a new thread
 */

/* FIXME: Unicode support?!?! case 61 */
#include "../globals.h"       /* for pragma warning's and assert defines */
#include "../module_shared.h" /* for d_r_get_proc_address() */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "ntdll.h" /* for get/set context etc. */

#include "instr.h"
#include "instr_create_shared.h"
#include "decode.h"

/* i#1597: to prevent an IAT hooking injected library in drrun or a tool
 * front-end from redirecting kernel32!LoadLibrary and kernel32!GetProcAddress
 * to the inject lib itself, which won't be there in the child, it's best
 * to use DR's d_r_get_proc_address().  We're already linking w/ the files we need.
 */
#include "os_private.h" /* for d_r_get_proc_address() and load_dynamo */
#define GET_PROC_ADDR d_r_get_proc_address

/* this entry point is hardcoded, FIXME : abstract */
#define DYNAMORIO_ENTRY "dynamo_auto_start"

#ifdef DEBUG
/* for asserts, we import globals.h now (for pragmas) so don't need to
 * duplicate assert defines, declarations */
extern void
display_error(char *msg);
#else
#    define display_error(msg) ((void)0)
#endif

/* get_module_handle is unsafe to call at arbitrary point from the core so move
 * all uses in inject.c to separate init function which can be called at a safe
 * point */
static ptr_uint_t addr_getprocaddr;
static ptr_uint_t addr_loadlibrarya;
#ifdef LOAD_DYNAMO_DEBUGBREAK
static ptr_uint_t addr_debugbreak;
#endif
static bool inject_initialized = false;
void
inject_init()
{
    HANDLE kern32 = get_module_handle(L"KERNEL32.DLL");
    ASSERT(kern32 != NULL);
    addr_getprocaddr = (ptr_uint_t)GET_PROC_ADDR(kern32, "GetProcAddress");
    ASSERT(addr_getprocaddr != 0);
    addr_loadlibrarya = (ptr_uint_t)GET_PROC_ADDR(kern32, "LoadLibraryA");
    ASSERT(addr_loadlibrarya != 0);
#ifdef LOAD_DYNAMO_DEBUGBREAK
    addr_debugbreak = (ptr_uint_t)GET_PROC_ADDR(kern32, "DebugBreak");
    ASSERT(addr_debugbreak != NULL);
#endif
    inject_initialized = true;
}

/* change this if load_dynamo changes
 * 128 is more than enough room even with all debugging code in there
 */
#define SIZE_OF_LOAD_DYNAMO 128

/* pass non-NULL for thandle if you want this routine to use
 *   Get/SetThreadContext to get the context -- you must still pass
 *   in a pointer to a cxt
 */
bool
inject_into_thread(HANDLE phandle, CONTEXT *cxt, HANDLE thandle, char *dynamo_path)
{
    size_t nbytes;
    bool success = false;
    ptr_uint_t dynamo_entry_esp;
    ptr_uint_t dynamo_path_esp;
    LPVOID load_dynamo_code = NULL; /* = base of code allocation */
    ptr_uint_t addr;
    reg_t *bufptr;
    char buf[MAX_PATH * 3];
    uint old_prot;

    ASSERT(cxt != NULL);

#ifndef NOT_DYNAMORIO_CORE_PROPER
    /* FIXME - if we were early injected we couldn't call inject_init during
     * startup because kernel32 wasn't loaded yet, so we call it here which
     * isn't safe because it uses app locks. If we want to support a mix
     * of early and late follow children injection we should change load_dynamo
     * to use Nt functions (which we can link) rather then kernel32 functions
     * (which we have to look up).  We could also use module.c code to safely
     * walk the exports of kernel32.dll (we can cache its mod handle when it
     * is loaded). */
    if (!inject_initialized) {
        SYSLOG_INTERNAL_WARNING("Using late inject follow children from early injected "
                                "process, unsafe LdrLock usage");
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        inject_init();
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
#else
    ASSERT(inject_initialized);
#endif

    /* soon we'll start using alternative injection with case 102 - leaving block */
    {
        reg_t app_xsp;
        if (thandle != NULL) {
            /* grab the context of the app's main thread */
            /* we can't use proc_has_feature() so no CONTEXT_DR_STATE */
            cxt->ContextFlags = CONTEXT_DR_STATE_ALLPROC;
            if (!NT_SUCCESS(nt_get_context(thandle, cxt))) {
                display_error("GetThreadContext failed");
                goto error;
            }
        }
        app_xsp = cxt->CXT_XSP;

        /* copy load_dynamo() into the address space of the new process */
        ASSERT(BUFFER_SIZE_BYTES(buf) > SIZE_OF_LOAD_DYNAMO);
        memcpy(buf, (char *)load_dynamo, SIZE_OF_LOAD_DYNAMO);
        /* R-X protection is adequate for our non-self modifying code,
         * and we'll update that after we're done with
         * nt_write_virtual_memory() calls */

        /* get allocation, this will be freed by os_heap_free, so make sure
         * is compatible allocation method */
        if (!NT_SUCCESS(nt_remote_allocate_virtual_memory(
                phandle, &load_dynamo_code, SIZE_OF_LOAD_DYNAMO, PAGE_EXECUTE_READWRITE,
                MEMORY_COMMIT))) {
            display_error("Failed to allocate memory for injection code");
            goto error;
        }
        if (!nt_write_virtual_memory(phandle, load_dynamo_code, buf, SIZE_OF_LOAD_DYNAMO,
                                     NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* Xref PR 252745 & PR 252008 - we can use the app's stack to hold our data
         * even on WOW64 and 64-bit since we're using set context to set xsp. */

        /* copy the DYNAMORIO_ENTRY string to the app's stack */
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s", DYNAMORIO_ENTRY);
        NULL_TERMINATE_BUFFER(buf);
        nbytes = strlen(buf) + 1; // include the trailing '\0'
        /* keep esp at pointer-sized alignment */
        cxt->CXT_XSP -= ALIGN_FORWARD(nbytes, XSP_SZ);
        dynamo_entry_esp = cxt->CXT_XSP;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, buf, nbytes, NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* copy the dynamorio_path string to the app's stack */
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s", dynamo_path);
        NULL_TERMINATE_BUFFER(buf);
        nbytes = strlen(buf) + 1; // include the trailing '\0'
        /* keep esp at pointer-sized byte alignment */
        cxt->CXT_XSP -= ALIGN_FORWARD(nbytes, XSP_SZ);
        dynamo_path_esp = cxt->CXT_XSP;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, buf, nbytes, NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* copy the current context to the app's stack. Only need the
         * control registers, so we use a priv_mcontext_t layout.
         */
        ASSERT(BUFFER_SIZE_BYTES(buf) >= sizeof(priv_mcontext_t));
        bufptr = (reg_t *)buf;
        *bufptr++ = cxt->CXT_XDI;
        *bufptr++ = cxt->CXT_XSI;
        *bufptr++ = cxt->CXT_XBP;
        *bufptr++ = app_xsp;
        *bufptr++ = cxt->CXT_XBX;
        *bufptr++ = cxt->CXT_XDX;
        *bufptr++ = cxt->CXT_XCX;
        *bufptr++ = cxt->CXT_XAX;
#ifdef X64
        *bufptr++ = cxt->R8;
        *bufptr++ = cxt->R9;
        *bufptr++ = cxt->R10;
        *bufptr++ = cxt->R11;
        *bufptr++ = cxt->R12;
        *bufptr++ = cxt->R13;
        *bufptr++ = cxt->R14;
        *bufptr++ = cxt->R15;
#endif
        *bufptr++ = cxt->CXT_XFLAGS;
        *bufptr++ = cxt->CXT_XIP;
        bufptr += PRE_XMM_PADDING / sizeof(*bufptr);
        /* It would be nice to use preserve_xmm_caller_saved(), but we'd need to
         * link proc.c and deal w/ messy dependencies to get it into arch_exports.h,
         * so we do our own check.  We go ahead and put in the xmm slots even
         * if the underlying processor has no xmm support: no harm done.
         */
        if (IF_X64_ELSE(true, is_wow64_process(NT_CURRENT_PROCESS))) {
            /* PR 264138: preserve xmm0-5.  We fill in all slots even though
             * for 32-bit we don't use them (PR 306394).
             */
            int i, j;
            /* For x86, ensure we have ExtendedRegisters space (i#1223) */
            IF_NOT_X64(ASSERT(TEST(CONTEXT_XMM_FLAG, cxt->ContextFlags)));
            /* XXX i#1312: This should be proc_num_simd_sse_avx_registers(). */
            ASSERT(MCXT_SIMD_SLOT_SIZE == ZMM_REG_SIZE);
            for (i = 0; i < MCXT_NUM_SIMD_SLOTS; i++) {
                for (j = 0; j < XMM_REG_SIZE / sizeof(*bufptr); j++) {
                    *bufptr++ = CXT_XMM(cxt, i)->reg[j];
                }
                /* FIXME i#437: save ymm fields.  For now we assume we're
                 * not saving and we just skip the upper 128 bits.
                 */
                bufptr += (ZMM_REG_SIZE - XMM_REG_SIZE) / sizeof(*bufptr);
            }
        } else {
            /* skip xmm slots */
            bufptr += MCXT_TOTAL_SIMD_SLOTS_SIZE / sizeof(*bufptr);
        }
        /* TODO i#1312: the zmm and mask fields need to be copied. */
        bufptr += MCXT_TOTAL_OPMASK_SLOTS_SIZE / sizeof(*bufptr);
        ASSERT((char *)bufptr - (char *)buf == sizeof(priv_mcontext_t));
        *bufptr++ = (ptr_uint_t)load_dynamo_code;
        *bufptr++ = SIZE_OF_LOAD_DYNAMO;
        nbytes = sizeof(priv_mcontext_t) + 2 * sizeof(reg_t);
        cxt->CXT_XSP -= nbytes;
#ifdef X64
        /* We need xsp to be aligned prior to each call, but we can only pad
         * before the context as all later users assume the info they need is
         * at TOS.
         */
        cxt->CXT_XSP = ALIGN_BACKWARD(cxt->CXT_XSP, 16);
#endif
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, buf, nbytes, NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of the DYNAMORIO_ENTRY string on the app's stack */
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, &dynamo_entry_esp,
                                     sizeof(dynamo_entry_esp), &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of GetProcAddress on the app's stack */
        ASSERT(addr_getprocaddr);
        addr = addr_getprocaddr;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, &addr, sizeof(addr),
                                     NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of the dynamorio_path string on the app's stack */
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, &dynamo_path_esp,
                                     sizeof(dynamo_path_esp), &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of LoadLibraryA on the app's stack */
        ASSERT(addr_loadlibrarya);
        addr = addr_loadlibrarya;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, &addr, sizeof(addr),
                                     NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }

#ifdef LOAD_DYNAMO_DEBUGBREAK
        /* push the address of DebugBreak on the app's stack */
        ASSERT(addr_debugbreak);
        addr = addr_debugbreak;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, &addr, sizeof(addr),
                                     NULL)) {
            display_error("WriteMemory failed");
            goto error;
        }
#endif

        /* make the code R-X now */
        if (!nt_remote_protect_virtual_memory(phandle, load_dynamo_code,
                                              SIZE_OF_LOAD_DYNAMO, PAGE_EXECUTE_READ,
                                              &old_prot)) {
            display_error("Failed to make injection code R-X");
            goto error;
        }
        ASSERT(old_prot == PAGE_EXECUTE_READWRITE);

        /* now change Eip to point to the entry point of load_dynamo(), so that
           when we resume, load_dynamo is invoked automatically */
        cxt->CXT_XIP = (ptr_uint_t)load_dynamo_code;
        cxt->CXT_XFLAGS = 0;
        if (thandle != NULL) {
            if (!NT_SUCCESS(nt_set_context(thandle, cxt))) {
                display_error("SetThreadContext failed");
                goto error;
            }
        }

        success = true;
    }
error:
    /* we do not recover any changes in the child's address space */

    return success;
}

/* FIXME - would be nicer to use instrlist etc. to generate and emit the code
 * (with patch list for the calls), but we'll also likely want to use this for
 * drinject which would mean getting most of the core compiled into that. Prob.
 * should still do it, but writing like this isn't that hard. Another
 * possibility is to export this from a special/standalone build of dr that
 * injector can load, that would also make it easier for injector to find
 * Ldr* addresses. At the very least we should combine all these enums (instr.h
 * os_shared.h, emit_utils.c etc.) in one place.
 *
 * UPDATE: with drdecode (i#617) for use in drinject, we can use DR's
 * IR and should for any future code.
 */
enum {
    PUSHF = 0x9c,
    POPF = 0x9d,
    PUSHA = 0x60,
    POPA = 0x61,
    PUSH_EAX = 0x50,
    POP_EAX = 0x58,
    PUSH_ECX = 0x51,
    POP_ECX = 0x59,
    PUSH_IMM32 = 0x68,
    PUSH_IMM8 = 0x6a,

    JMP_REL8 = 0xeb,
    JMP_REL32 = 0xe9,
    CALL_REL32 = 0xe8,
    CALL_RM32 = 0xff,
    CALL_EAX_RM = 0xd0,
    JMP_FAR_DIRECT = 0xea,

    MOV_RM32_2_REG32 = 0x8b,
    MOV_REG32_2_RM32 = 0x89,
    MOV_ESP_2_EAX_RM = 0xc4,
    MOV_EAX_2_ECX_RM = 0xc8,
    MOV_EAX_2_EDX_RM = 0xd0,
    MOV_EAX_2_EAX_RM = 0xc0,
    MOV_derefEAX_2_EAX_RM = 0x00,
    MOV_deref_disp8_EAX_2_EAX_RM = 0x40,
    MOV_IMM8_2_RM8 = 0xc6,
    MOV_IMM32_2_RM32 = 0xc7,
    MOV_IMM_RM_ABS = 0x05,
    MOV_IMM_XAX = 0xb8,

    ADD_EAX_IMM32 = 0x05,
    AND_RM32_IMM32 = 0x81,

    CMP_EAX_IMM32 = 0x3d,
    JZ_REL8 = 0x74,
    JNZ_REL8 = 0x75,

    REX_W = 0x48,
    REX_B = 0x41,
    REX_R = 0x44,
};

#define DEBUG_LOOP 0

#define ASSERT_ROOM(cur, buf, maxlen) ASSERT(cur + maxlen < buf + sizeof(buf))

#define RAW_INSERT_INT16(pos, value)                           \
    do {                                                       \
        ASSERT(CHECK_TRUNCATE_TYPE_short((ptr_int_t)(value))); \
        *(short *)(pos) = (short)(value);                      \
        (pos) += sizeof(short);                                \
    } while (0)

#define RAW_INSERT_INT32(pos, value)                         \
    do {                                                     \
        ASSERT(CHECK_TRUNCATE_TYPE_int((ptr_int_t)(value))); \
        *(int *)(pos) = (int)(ptr_int_t)(value);             \
        (pos) += sizeof(int);                                \
    } while (0)

#define RAW_INSERT_INT64(pos, value)      \
    do {                                  \
        *(int64 *)(pos) = (int64)(value); \
        (pos) += sizeof(int64);           \
    } while (0)

#define RAW_INSERT_INT8(pos, value)                    \
    do {                                               \
        ASSERT(CHECK_TRUNCATE_TYPE_sbyte((int)value)); \
        *(sbyte *)(pos) = (sbyte)(value);              \
        (pos) += sizeof(sbyte);                        \
    } while (0)

#define RAW_PUSH_INT64(pos, value)                                                 \
    do {                                                                           \
        *(pos)++ = PUSH_IMM32;                                                     \
        RAW_INSERT_INT32(pos, (int)value);                                         \
        /* Push is sign-extended, so we can skip top half if top 33 bits are 0. */ \
        if ((uint64)(value) >= 0x80000000UL) {                                     \
            *(pos)++ = MOV_IMM32_2_RM32;                                           \
            *(pos)++ = 0x44;                                                       \
            *(pos)++ = 0x24;                                                       \
            *(pos)++ = 0x04; /* xsp+4 */                                           \
            RAW_INSERT_INT32(pos, (value) >> 32);                                  \
        }                                                                          \
    } while (0)

#define RAW_PUSH_INT32(pos, value)    \
    do {                              \
        *(pos)++ = PUSH_IMM32;        \
        RAW_INSERT_INT32(pos, value); \
    } while (0)

/* i#142, i#923: 64-bit support now works regardless of where the hook
 * location and the allocated remote_code_buffer are.
 *
 * XXX: this is all really messy: these macros are too limited for
 * inserting general instructions, so for x64 I hacked it by leaving
 * in the pushes and copying from TOS into the register params.
 * I would prefer to throw all this out and replace w/ IR or asm,
 * which would be easy now that we have drinjectlib.
 * Although for cross-arch injection (i#803) we want code for both
 * bitwidths, which actually might be easier w/ the macros for 32-to-64.
 */

/* If reachable is non-0, ensures the resulting allocation is
 * 32-bit-disp-reachable from [reachable, reachable+PAGE_SIZE).
 * For injecting into 64-bit from 32-bit, uses only low addresses.
 */
static byte *
allocate_remote_code_buffer(HANDLE phandle, size_t size, byte *reachable)
{
    NTSTATUS res;
    byte *buf = (byte *)NULL;
#ifdef X64
    /* Start at bottom of reachability range and keep trying at higher addresses */
    byte *pc = (byte *)ALIGN_FORWARD(
        REACHABLE_32BIT_START((byte *)reachable, (byte *)reachable + PAGE_SIZE),
        OS_ALLOC_GRANULARITY);
    byte *end_pc =
        (byte *)REACHABLE_32BIT_END((byte *)reachable, (byte *)reachable + PAGE_SIZE);
    /* we can't just pick an address and see if it gets allocated
     * b/c it could be in the middle of an existing reservation
     * (stack, e.g.) and then when we free it we could free the entire
     * reservation (yes this actually happened: i#753)
     * Update: we now reserve+commit so this won't happen, but it means
     * we need to be at an os alloc boundary (64K).
     */
    MEMORY_BASIC_INFORMATION mbi;
    size_t got;
    do {
        /* We do now have remote_query_virtual_memory_maybe64() available, but we
         * do not yet have allocation (win8+ only) or free (would have to make
         * one via switch_modes_and_call()) routines, and using low addresses should
         * always work.  We thus stick with 32-bit pointers here even for 64-bit
         * child processes.
         */
        res = nt_remote_query_virtual_memory(phandle, pc, &mbi, sizeof(mbi), &got);
        if (got != sizeof(mbi)) {
            /* bail and hope a low address works, which it will pre-win8 */
            break;
        }
        if (NT_SUCCESS(res) && mbi.State == MEM_FREE && mbi.RegionSize >= size &&
            /* we're reserving+committing so we need to be at an alloc boundary */
            ALIGNED(pc, OS_ALLOC_GRANULARITY) && pc != NULL) {
            buf = pc; /* we do NOT want mbi.AllocationBase as it may not be reachable */
            break;
        }
        pc += mbi.RegionSize;
    } while (NT_SUCCESS(res) && pc + size < end_pc);
#endif

    /* On Win8, a remote MEM_COMMIT in the dll address region fails with
     * STATUS_CONFLICTING_ADDRESSES.  Yet a local commit works, and a remote
     * reserve+commit works.  Go figure.
     */
    /* See above: we use only low addresses.  To support high we'd need to add
     * allocate and free routines via switch_modes_and_call() (we can use
     * NtWow64AllocateVirtualMemory64 on win8+).
     */
    res = nt_remote_allocate_virtual_memory(phandle, &buf, size, PAGE_EXECUTE_READWRITE,
                                            MEM_RESERVE);
    if (NT_SUCCESS(res)) {
        res = nt_remote_allocate_virtual_memory(phandle, &buf, size,
                                                PAGE_EXECUTE_READWRITE, MEM_COMMIT);
    }

    /* We know buf at low end reaches, but might have gone too high. */
    if (!NT_SUCCESS(res) ||
        (reachable != 0 && !REL32_REACHABLE(buf + size, (byte *)reachable))) {
#ifndef NOT_DYNAMORIO_CORE_PROPER
        SYSLOG_INTERNAL_ERROR("failed to allocate child memory for injection");
#endif
        return NULL;
    }
    return buf;
}

static bool
free_remote_code_buffer(HANDLE phandle, byte *base)
{
    /* There seems to be no such thing as NtWow64FreeVirtualMemory64!
     * allocate_remote_code_buffer() is using low address though, so we're good
     * to use 32-bit pointers even for 64-bit children.
     */
    NTSTATUS res = nt_remote_free_virtual_memory(phandle, base);
    return NT_SUCCESS(res);
}

/* Does not support a 64-bit child of a 32-bit DR. */
static void *
inject_gencode_at_ldr(HANDLE phandle, char *dynamo_path, uint inject_location,
                      void *inject_address, void *hook_location,
                      byte hook_buf[EARLY_INJECT_HOOK_SIZE], void *must_reach)
{
    void *hook_target;
    byte *remote_code_buffer = NULL, *remote_data_buffer;
    /* max usage for local_buf is for writing the dr library name
     * 2*MAX_PATH (unicode) + sizoef(UNICODE_STRING) + 2, round up to
     * 3*MAX_PATH to be safe */
    byte local_buf[3 * MAX_PATH];
    byte *cur_local_pos, *cur_remote_pos, *jmp_fixup1, *jmp_fixup2;
    char *takeover_func = "dynamorio_app_init_and_early_takeover";
    PUNICODE_STRING mod, mod_remote;
    PANSI_STRING func, func_remote;
    int res, i;
    size_t num_bytes_in, num_bytes_out;
    uint old_prot;

    GET_NTDLL(LdrLoadDll,
              (IN PCWSTR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL,
               IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle));
    GET_NTDLL(LdrGetProcedureAddress,
              (IN HANDLE ModuleHandle, IN PANSI_STRING ProcedureName OPTIONAL,
               IN ULONG Ordinal OPTIONAL, OUT FARPROC * ProcedureAddress));
#define GET_PROC_ADDR_BAD_ADDR 0xffbadd11
    GET_NTDLL(NtProtectVirtualMemory,
              (IN HANDLE ProcessHandle, IN OUT PVOID * BaseAddress,
               IN OUT PULONG ProtectSize, IN ULONG NewProtect, OUT PULONG OldProtect));
    GET_NTDLL(NtContinue, (IN PCONTEXT Context, IN BOOLEAN TestAlert));

    /* get buffer for emitted code and data */
    remote_code_buffer = allocate_remote_code_buffer(phandle, 2 * PAGE_SIZE, must_reach);
    if (remote_code_buffer == NULL)
        goto error;
    remote_data_buffer = remote_code_buffer + PAGE_SIZE;

    /* write data */
    /* FIXME the two writes are similar (unicode vs ascii), could combine */
    /* First UNICODE_STRING to library */
    cur_remote_pos = remote_data_buffer;
    cur_local_pos = local_buf;
    ASSERT_ROOM(cur_local_pos, local_buf, sizeof(UNICODE_STRING));
    mod = (PUNICODE_STRING)cur_local_pos;
    memset(mod, 0, sizeof(UNICODE_STRING));
    cur_local_pos += sizeof(UNICODE_STRING);
    mod->Buffer = (wchar_t *)(cur_remote_pos + (cur_local_pos - local_buf));
    ASSERT_ROOM(cur_local_pos, local_buf, 2 * MAX_PATH + 2 /* plus null */);
    res = snwprintf((wchar_t *)cur_local_pos, 2 * MAX_PATH, L"%hs", dynamo_path);
    ASSERT(res > 0);
    if (res > 0) {
        cur_local_pos += (2 * res);
        ASSERT_TRUNCATE(mod->Length, ushort, 2 * res);
        mod->Length = (ushort)(2 * res);
        mod->MaximumLength = (ushort)(2 * res);
    }
    /* ensure NULL termination, just in case */
    *(wchar_t *)cur_local_pos = L'\0';
    cur_local_pos += sizeof(wchar_t);
    /* write to remote process */
    num_bytes_in = cur_local_pos - local_buf;
    if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf, num_bytes_in,
                                 &num_bytes_out) ||
        num_bytes_out != num_bytes_in) {
        goto error;
    }
    mod_remote = (PUNICODE_STRING)cur_remote_pos;
    cur_remote_pos += num_bytes_out;

    /* now write init/takeover func */
    cur_local_pos = local_buf;
    ASSERT_ROOM(cur_local_pos, local_buf, sizeof(ANSI_STRING));
    func = (PANSI_STRING)cur_local_pos;
    memset(func, 0, sizeof(ANSI_STRING));
    cur_local_pos += sizeof(ANSI_STRING);
    func->Buffer = (PCHAR)cur_remote_pos + (cur_local_pos - local_buf);
    ASSERT_ROOM(cur_local_pos, local_buf, strlen(takeover_func) + 1);
    strncpy((char *)cur_local_pos, takeover_func, strlen(takeover_func));
    cur_local_pos += strlen(takeover_func);
    ASSERT_TRUNCATE(func->Length, ushort, strlen(takeover_func));
    func->Length = (ushort)strlen(takeover_func);
    func->MaximumLength = (ushort)strlen(takeover_func);
    *cur_local_pos++ = '\0'; /* ensure NULL termination, just in case */
    /* write to remote_process */
    num_bytes_in = cur_local_pos - local_buf;
    if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf, num_bytes_in,
                                 &num_bytes_out) ||
        num_bytes_out != num_bytes_in) {
        goto error;
    }
    func_remote = (PANSI_STRING)cur_remote_pos;
    cur_remote_pos += num_bytes_out;

    /* now make data page read only */
    res = nt_remote_protect_virtual_memory(phandle, remote_data_buffer, PAGE_SIZE,
                                           PAGE_READONLY, &old_prot);
    ASSERT(res);

#define INSERT_INT(value) RAW_INSERT_INT32(cur_local_pos, value)

#define INSERT_ADDR(value)                            \
    *(ptr_int_t *)cur_local_pos = (ptr_int_t)(value); \
    cur_local_pos += sizeof(ptr_int_t)

#ifdef X64
#    define INSERT_PUSH_ALL_REG()             \
        *cur_local_pos++ = PUSH_EAX;          \
        *cur_local_pos++ = PUSH_ECX;          \
        *cur_local_pos++ = 0x52; /* xdx */    \
        *cur_local_pos++ = 0x53; /* xbx */    \
        *cur_local_pos++ = 0x54; /* xsp */    \
        *cur_local_pos++ = 0x55; /* xbp */    \
        *cur_local_pos++ = 0x56; /* xsi */    \
        *cur_local_pos++ = 0x57; /* xdi */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = PUSH_EAX; /* r8 */ \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = PUSH_ECX; /* r9 */ \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x52; /* r10 */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x53; /* r11 */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x54; /* r12 */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x55; /* r13 */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x56; /* r14 */    \
        *cur_local_pos++ = REX_B;             \
        *cur_local_pos++ = 0x57; /* r15 */
#else
#    define INSERT_PUSH_ALL_REG() *cur_local_pos++ = PUSHA
#endif

#ifdef X64
#    define INSERT_POP_ALL_REG()                                            \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5f; /* r15 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5e; /* r14 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5d; /* r13 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5c; /* r12 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5b; /* r11 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = 0x5a; /* r10 */                                  \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = POP_ECX; /* r9 */                                \
        *cur_local_pos++ = REX_B;                                           \
        *cur_local_pos++ = POP_EAX; /* r8 */                                \
        *cur_local_pos++ = 0x5f;    /* xdi */                               \
        *cur_local_pos++ = 0x5e;    /* xsi */                               \
        *cur_local_pos++ = 0x5d;    /* xbp */                               \
        *cur_local_pos++ = 0x5b;    /* xsp slot but popped into dead xbx */ \
        *cur_local_pos++ = 0x5b;    /* xbx */                               \
        *cur_local_pos++ = 0x5a;    /* xdx */                               \
        *cur_local_pos++ = POP_ECX;                                         \
        *cur_local_pos++ = POP_EAX
#else
#    define INSERT_POP_ALL_REG() *cur_local_pos++ = POPA
#endif

#define PUSH_IMMEDIATE(value) RAW_PUSH_INT32(cur_local_pos, value)

#define PUSH_SHORT_IMMEDIATE(value) \
    *cur_local_pos++ = PUSH_IMM8;   \
    *cur_local_pos++ = value

#ifdef X64
#    define PUSH_PTRSZ_IMMEDIATE(value) RAW_PUSH_INT64(cur_local_pos, value)
#else
#    define PUSH_PTRSZ_IMMEDIATE(value) PUSH_IMMEDIATE(value)
#endif

#define MOV_ESP_TO_EAX()                 \
    IF_X64(*cur_local_pos++ = REX_W;)    \
    *cur_local_pos++ = MOV_RM32_2_REG32; \
    *cur_local_pos++ = MOV_ESP_2_EAX_RM

#ifdef X64
    /* mov rax -> rcx */
#    define MOV_EAX_TO_PARAM_0()             \
        *cur_local_pos++ = REX_W;            \
        *cur_local_pos++ = MOV_RM32_2_REG32; \
        *cur_local_pos++ = MOV_EAX_2_ECX_RM
    /* mov rax -> rdx */
#    define MOV_EAX_TO_PARAM_1()             \
        *cur_local_pos++ = REX_W;            \
        *cur_local_pos++ = MOV_RM32_2_REG32; \
        *cur_local_pos++ = MOV_EAX_2_EDX_RM
    /* mov rax -> r8 */
#    define MOV_EAX_TO_PARAM_2()             \
        *cur_local_pos++ = REX_R | REX_W;    \
        *cur_local_pos++ = MOV_RM32_2_REG32; \
        *cur_local_pos++ = MOV_EAX_2_EAX_RM
    /* mov rax -> r9 */
#    define MOV_EAX_TO_PARAM_3()             \
        *cur_local_pos++ = REX_R | REX_W;    \
        *cur_local_pos++ = MOV_RM32_2_REG32; \
        *cur_local_pos++ = MOV_EAX_2_ECX_RM
    /* mov (rsp) -> rcx */
#    define MOV_TOS_TO_PARAM_0()  \
        *cur_local_pos++ = REX_W; \
        *cur_local_pos++ = 0x8b;  \
        *cur_local_pos++ = 0x0c;  \
        *cur_local_pos++ = 0x24
    /* mov (rsp) -> rdx */
#    define MOV_TOS_TO_PARAM_1()  \
        *cur_local_pos++ = REX_W; \
        *cur_local_pos++ = 0x8b;  \
        *cur_local_pos++ = 0x14;  \
        *cur_local_pos++ = 0x24
    /* mov (rsp) -> r8 */
#    define MOV_TOS_TO_PARAM_2()          \
        *cur_local_pos++ = REX_R | REX_W; \
        *cur_local_pos++ = 0x8b;          \
        *cur_local_pos++ = 0x04;          \
        *cur_local_pos++ = 0x24
    /* mov (rsp) -> r9 */
#    define MOV_TOS_TO_PARAM_3()          \
        *cur_local_pos++ = REX_R | REX_W; \
        *cur_local_pos++ = 0x8b;          \
        *cur_local_pos++ = 0x0c;          \
        *cur_local_pos++ = 0x24
#endif /* X64 */

/* FIXME - all values are small use imm8 version */
#define ADD_TO_EAX(value)             \
    IF_X64(*cur_local_pos++ = REX_W;) \
    *cur_local_pos++ = ADD_EAX_IMM32; \
    INSERT_INT(value)

#define ADD_IMM8_TO_ESP(value)        \
    IF_X64(*cur_local_pos++ = REX_W;) \
    *cur_local_pos++ = 0x83;          \
    *cur_local_pos++ = 0xc4;          \
    *cur_local_pos++ = (byte)(value);

#define CMP_TO_EAX(value)             \
    IF_X64(*cur_local_pos++ = REX_W;) \
    *cur_local_pos++ = CMP_EAX_IMM32; \
    INSERT_INT(value)

#define INSERT_REL32_ADDRESS(target)                                             \
    IF_X64(ASSERT_NOT_IMPLEMENTED(REL32_REACHABLE(                               \
        ((cur_local_pos - local_buf) + 4) + cur_remote_pos, (byte *)(target)))); \
    INSERT_INT((int)(ptr_int_t)((byte *)target -                                 \
                                (((cur_local_pos - local_buf) + 4) + cur_remote_pos)))

#ifdef X64
    /* for reachability, go through eax, which should be dead */
#    define CALL(target_func)           \
        *cur_local_pos++ = REX_W;       \
        *cur_local_pos++ = MOV_IMM_XAX; \
        INSERT_ADDR(target_func);       \
        *cur_local_pos++ = CALL_RM32;   \
        *cur_local_pos++ = CALL_EAX_RM;
#else
#    define CALL(target_func)          \
        *cur_local_pos++ = CALL_REL32; \
        INSERT_REL32_ADDRESS(target_func)
#endif /* X64 */

/* ecx will hold OldProtection afterwards */
/* for x64 we need the 4 stack slots anyway so we do the pushes */
/* on x64, up to caller to have rsp aligned to 16 prior to calling this macro */
#define PROT_IN_ECX 0xbad5bad /* doesn't match a PAGE_* define */
#define CHANGE_PROTECTION(start, size, new_protection)                              \
    *cur_local_pos++ = PUSH_EAX; /* OldProtect slot */                              \
    MOV_ESP_TO_EAX();            /* get &OldProtect */                              \
    PUSH_PTRSZ_IMMEDIATE(ALIGN_FORWARD(start + size, PAGE_SIZE) -                   \
                         ALIGN_BACKWARD(start, PAGE_SIZE)); /* ProtectSize */       \
    PUSH_PTRSZ_IMMEDIATE(ALIGN_BACKWARD(start, PAGE_SIZE)); /* BaseAddress */       \
    *cur_local_pos++ = PUSH_EAX;                            /* arg 5 &OldProtect */ \
    if (new_protection == PROT_IN_ECX) {                                            \
        *cur_local_pos++ = PUSH_ECX; /* arg 4 NewProtect */                         \
    } else {                                                                        \
        PUSH_IMMEDIATE(new_protection); /* arg 4 NewProtect */                      \
    }                                                                               \
    IF_X64(MOV_TOS_TO_PARAM_3());                                                   \
    ADD_TO_EAX(-(int)XSP_SZ);    /* get &ProtectSize */                             \
    *cur_local_pos++ = PUSH_EAX; /* arg 3 &ProtectSize */                           \
    IF_X64(MOV_EAX_TO_PARAM_2());                                                   \
    ADD_TO_EAX(-(int)XSP_SZ);    /* get &BaseAddress */                             \
    *cur_local_pos++ = PUSH_EAX; /* arg 2 &BaseAddress */                           \
    IF_X64(MOV_EAX_TO_PARAM_1());                                                   \
    PUSH_IMMEDIATE((int)(ptr_int_t)NT_CURRENT_PROCESS); /* arg ProcessHandle */     \
    IF_X64(MOV_TOS_TO_PARAM_0());                                                   \
    CALL(NtProtectVirtualMemory); /* 8 pushes => still aligned to 16 */             \
    /* no error checking, can't really do anything about it, FIXME */               \
    /* stdcall so just the three slots we made for the ptr arguments                \
     * left on the stack for 32-bit */                                              \
    IF_X64(ADD_IMM8_TO_ESP(5 * XSP_SZ)); /* clean up 5 slots */                     \
    *cur_local_pos++ = POP_ECX;          /* pop BaseAddress */                      \
    *cur_local_pos++ = POP_ECX;          /* pop ProtectSize */                      \
    *cur_local_pos++ = POP_ECX           /* pop OldProtect into ecx */

    /* write code */
    /* xref case 3821, first call to a possibly hooked routine should be
     * more then 5 bytes into the page, which is satisfied (though is not
     * clear if any hookers would manage to get in first). */
    cur_remote_pos = remote_code_buffer;
    cur_local_pos = local_buf;
    hook_target = cur_remote_pos;
    /* for inject_location INJECT_LOCATION_Ldr* we stick the address used
     * at the start of the code for the child's use */
    if (INJECT_LOCATION_IS_LDR(inject_location)) {
        INSERT_ADDR(inject_address);
        hook_target = cur_remote_pos + sizeof(ptr_int_t); /* skip the address */
    }

#if DEBUG_LOOP
    *cur_local_pos++ = JMP_REL8;
    *cur_local_pos++ = 0xfe;
#endif

    /* save current state */
    INSERT_PUSH_ALL_REG();
    *cur_local_pos++ = PUSHF;

    /* restore trampoline, first make writable */
    CHANGE_PROTECTION(hook_location, EARLY_INJECT_HOOK_SIZE, PAGE_EXECUTE_READWRITE);
    /* put target in xax to ensure we can reach it */
    IF_X64(*cur_local_pos++ = REX_W);
    *cur_local_pos++ = MOV_IMM_XAX;
    INSERT_ADDR(hook_location);
    for (i = 0; i < EARLY_INJECT_HOOK_SIZE / 4; i++) {
        /* restore bytes 4*i..4*i+3 of hook */
        *cur_local_pos++ = MOV_IMM32_2_RM32;
        *cur_local_pos++ = MOV_deref_disp8_EAX_2_EAX_RM;
        *cur_local_pos++ = (byte)i * 4;
        INSERT_INT(*((int *)hook_buf + i));
    }
    for (i = i * 4; i < EARLY_INJECT_HOOK_SIZE; i++) {
        /* restore byte i of hook */
        *cur_local_pos++ = MOV_IMM8_2_RM8;
        *cur_local_pos++ = MOV_deref_disp8_EAX_2_EAX_RM;
        *cur_local_pos++ = (byte)i;
        *cur_local_pos++ = hook_buf[i];
    }
    /* hook restored, restore protection */
    CHANGE_PROTECTION(hook_location, EARLY_INJECT_HOOK_SIZE, PROT_IN_ECX);

    if (inject_location == INJECT_LOCATION_KiUserException) {
        /* Making the first page of the image unreadable triggers an exception
         * to early to use the loader, might try pointing the import table ptr
         * to bad memory instead TOTRY, whatever we do should fixup here */
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* call LdrLoadDll to load dr library */
    *cur_local_pos++ = PUSH_EAX; /* need slot for OUT hmodule*/
    MOV_ESP_TO_EAX();
    IF_X64(*cur_local_pos++ = PUSH_EAX); /* extra slot to align to 16 for call */
    *cur_local_pos++ = PUSH_EAX;         /* arg 4 OUT *hmodule */
    IF_X64(MOV_EAX_TO_PARAM_3());
    /* XXX: these push-ptrsz, mov-tos sequences are inefficient, but simpler
     * for cross-platform
     */
    PUSH_PTRSZ_IMMEDIATE((ptr_int_t)mod_remote); /* our library name */
    IF_X64(MOV_TOS_TO_PARAM_2());
    PUSH_SHORT_IMMEDIATE(0x0); /* Flags OPTIONAL */
    IF_X64(MOV_TOS_TO_PARAM_1());
    PUSH_SHORT_IMMEDIATE(0x0); /* PathToFile OPTIONAL */
    IF_X64(MOV_TOS_TO_PARAM_0());
    CALL(LdrLoadDll);                    /* see signature at declaration above */
    IF_X64(ADD_IMM8_TO_ESP(5 * XSP_SZ)); /* clean up 5 slots */

    /* stdcall so removed args so top of stack is now the slot containing the
     * returned handle.  Use LdrGetProcedureAddress to get the address of the
     * dr init and takeover function. Is ok to call even if LdrLoadDll failed,
     * so we check for errors afterwards. */
    *cur_local_pos++ = POP_ECX;  /* dr module handle */
    *cur_local_pos++ = PUSH_ECX; /* need slot for out ProcedureAddress */
    MOV_ESP_TO_EAX();
    IF_X64(*cur_local_pos++ = PUSH_EAX); /* extra slot to align to 16 for call */
    *cur_local_pos++ = PUSH_EAX;         /* arg 4 OUT *ProcedureAddress */
    IF_X64(MOV_EAX_TO_PARAM_3());
    PUSH_SHORT_IMMEDIATE(0x0); /* Ordinal OPTIONAL */
    IF_X64(MOV_TOS_TO_PARAM_2());
    PUSH_PTRSZ_IMMEDIATE((ptr_int_t)func_remote); /* func name */
    IF_X64(MOV_TOS_TO_PARAM_1());
    *cur_local_pos++ = PUSH_ECX; /* module handle */
    IF_X64(MOV_TOS_TO_PARAM_0());
    /* for x64, aligned at LdrLoadDll - 5 - 1 + 6 => aligned here */
    CALL(LdrGetProcedureAddress);        /* see signature at declaration above */
    IF_X64(ADD_IMM8_TO_ESP(5 * XSP_SZ)); /* clean up 5 slots */

    /* Top of stack is now the dr init and takeover function (stdcall removed
     * args). Check for errors and bail (FIXME debug build report somehow?) */
    CMP_TO_EAX(STATUS_SUCCESS);
    *cur_local_pos++ = POP_EAX;   /* dr init_and_takeover function */
    *cur_local_pos++ = JNZ_REL8;  /* FIXME - should check >= 0 instead? */
    jmp_fixup1 = cur_local_pos++; /* jmp to after call below */
    /* Xref case 8373, LdrGetProcedureAdderss sometimes returns an
     * address of 0xffbadd11 even though it returned STATUS_SUCCESS */
    CMP_TO_EAX((int)GET_PROC_ADDR_BAD_ADDR);
    *cur_local_pos++ = JZ_REL8;                          /* JZ == JE */
    jmp_fixup2 = cur_local_pos++;                        /* jmp to after call below */
    IF_X64(ADD_IMM8_TO_ESP(-2 * (int)XSP_SZ));           /* need 4 slots total */
    PUSH_PTRSZ_IMMEDIATE((ptr_int_t)remote_code_buffer); /* arg to takeover func */
    IF_X64(MOV_TOS_TO_PARAM_1());
    PUSH_IMMEDIATE(inject_location); /* arg to takeover func */
    IF_X64(MOV_TOS_TO_PARAM_0());
    /* for x64, 2 pushes => aligned to 16 */
    *cur_local_pos++ = CALL_RM32; /* call EAX */
    *cur_local_pos++ = CALL_EAX_RM;
#ifdef X64
    IF_X64(ADD_IMM8_TO_ESP(4 * XSP_SZ)); /* clean up 4 slots */
#else
    *cur_local_pos++ = POP_ECX; /* cdecl so pop arg */
    *cur_local_pos++ = POP_ECX; /* cdecl so pop arg */
#endif
    /* Now patch the jnz above (if error) to go to here */
    ASSERT_TRUNCATE(*jmp_fixup1, byte, cur_local_pos - (jmp_fixup1 + 1));
    *jmp_fixup1 = (byte)(cur_local_pos - (jmp_fixup1 + 1)); /* target of jnz */
    ASSERT_TRUNCATE(*jmp_fixup2, byte, cur_local_pos - (jmp_fixup2 + 1));
    *jmp_fixup2 = (byte)(cur_local_pos - (jmp_fixup2 + 1)); /* target of jz */
    *cur_local_pos++ = POPF;
    INSERT_POP_ALL_REG();
    if (inject_location != INJECT_LOCATION_KiUserException) {
        /* jmp back to the hook location to resume execution */
#ifdef X64
        /* ind jmp w/ target rip-rel right after (thus 0 disp) */
        *cur_local_pos++ = 0xff;
        *cur_local_pos++ = 0x25;
        INSERT_INT(0);
        INSERT_ADDR(hook_location);
#else
        *cur_local_pos++ = JMP_REL32;
        INSERT_REL32_ADDRESS(hook_location);
#endif
    } else {
        /* we triggered the exception, so do an NtContinue back */
        /* see callback.c, esp+4 holds CONTEXT ** */
        *cur_local_pos++ = POP_EAX;  /* EXCEPTION_RECORD ** */
        *cur_local_pos++ = POP_EAX;  /* CONTEXT ** */
        PUSH_SHORT_IMMEDIATE(FALSE); /* arg 2 TestAlert */
        IF_X64(MOV_TOS_TO_PARAM_1());
        *cur_local_pos++ = MOV_RM32_2_REG32;
        *cur_local_pos++ = MOV_derefEAX_2_EAX_RM; /* CONTEXT * -> EAX */
        *cur_local_pos++ = PUSH_EAX;              /* push CONTEXT * (arg 1) */
        IF_X64(MOV_EAX_TO_PARAM_0());
        IF_X64(ADD_IMM8_TO_ESP(-4 * (int)XSP_SZ)); /* 4 slots */
        CALL(NtContinue);
        /* should never get here, will be zeroed memory so will crash if
         * we do happen to get here, good enough reporting */
    }

    /* Our emitted code above is much less then the sizeof local_buf,
     * but we'll add a check here (after the fact so not robust if really
     * overflowed) that we didn't even come close (someon adding large amounts
     * of code should hit this. FIXME - do better? */
    ASSERT_ROOM(cur_local_pos, local_buf, MAX_PATH);
    num_bytes_in = cur_local_pos - local_buf;
    if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf, num_bytes_in,
                                 &num_bytes_out) ||
        num_bytes_out != num_bytes_in) {
        goto error;
    }
    cur_remote_pos += num_bytes_out;
    /* now make code page rx */
    res = nt_remote_protect_virtual_memory(phandle, remote_code_buffer, PAGE_SIZE,
                                           PAGE_EXECUTE_READ, &old_prot);
    ASSERT(res);

#undef INSERT_INT
#undef PUSH_IMMEDIATE
#undef PUSH_SHORT_IMMEDIATE
#undef MOV_ESP_TO_EAX
#undef ADD_TO_EAX
#undef INSERT_REL32_ADDRESS
#undef CALL
#undef PROT_IN_ECX
#undef CHANGE_PROTECTION

    return hook_target;

error:
    return NULL;
}

/* make gencode easier to read */
#define APP instrlist_append
#define GDC GLOBAL_DCONTEXT

#define SWITCH_MODE_DATA_SIZE 4 /* size of 32 bit stack ptr */

#ifdef X64
/* This function is necessary b/c the original logic push the hook location on
 * the stack and jump to dynamorio. Dynamorio start translating the first
 * return address and control transfer to it. It then run in translated
 * mode and when it unwinds the stack at some point it will jump to hook
 * location(which is pushed on the stack). If the dynamorio is 64 bit the
 * first return address it will see will be 64 bit and hence when it finds
 * the 32 bit address on the stack it will treat it as a 64 bit address.
 * Instead of pushing the hook location on the stack we are pushing the
 * location of the sequece of code which does a mode switch and jump to
 * the hook location.
 */
/* This function genearates the code for mode switch after returning
 * from dynamorio. local_code_buf is the parent process buf which will
 * temporarily hold the generated instructions. mode_switch_buf is the
 * location where the actual switch_code will be stored in the target
 * process, mode_switch_buf_sz is maximum size for switch code, and
 * mode_switch_data is the address where the app stack pointer is stored.
 */
static size_t
generate_switch_mode_jmp_to_hook(HANDLE phandle, byte *local_code_buf,
                                 byte *mode_switch_buf, byte *hook_location,
                                 size_t mode_switch_buf_sz, byte *mode_switch_data)
{
    /* Switch to 32 bit mode
     * Restore the stack
     * Jump to the hook location
     */
    byte *pc;
    instrlist_t ilist;
    size_t num_bytes_out, sz;
    uint target;
    instr_t *jmp = INSTR_CREATE_jmp(GDC, opnd_create_pc((app_pc)hook_location));
    instr_t *restore_esp =
        INSTR_CREATE_mov_ld(GDC, opnd_create_reg(REG_ESP),
                            OPND_CREATE_MEM32(REG_NULL, (int)(size_t)mode_switch_data));
    const byte *eax_saved_offset = (byte *)((size_t)mode_switch_data) + 4;
    // Restoring eax back, which is storing routine address that needs to be passed to
    // RtlUserStartThread
    instr_t *restore_eax =
        INSTR_CREATE_mov_ld(GDC, opnd_create_reg(REG_EAX),
                            OPND_CREATE_MEM32(REG_NULL, (int)(size_t)eax_saved_offset));

    instr_set_x86_mode(jmp, true);
    instr_set_x86_mode(restore_esp, true);
    instr_set_x86_mode(restore_eax, true);
    instrlist_init(&ilist);
    /* We patch the 0 with the correct target location in this function */
    APP(&ilist, INSTR_CREATE_push_imm(GDC, OPND_CREATE_INT32(0)));
    APP(&ilist,
        INSTR_CREATE_mov_st(GDC, OPND_CREATE_MEM16(REG_RSP, 4),
                            OPND_CREATE_INT16((ushort)CS32_SELECTOR)));
    APP(&ilist,
        INSTR_CREATE_jmp_far_ind(GDC,
                                 opnd_create_base_disp(REG_RSP, REG_NULL, 0, 0, OPSZ_6)));
    APP(&ilist, restore_esp);
    APP(&ilist, restore_eax);
    APP(&ilist, jmp);

    pc = instrlist_encode_to_copy(GDC, &ilist, local_code_buf, mode_switch_buf,
                                  local_code_buf + mode_switch_buf_sz,
                                  true /*has instr targets*/);
    ASSERT(pc != NULL && pc < local_code_buf + mode_switch_buf_sz);

    /* Calculate the offset of first instruction after switching
     * to x86 mode
     */
    sz = (size_t)(pc - local_code_buf - instr_length(GDC, jmp) -
                  instr_length(GDC, restore_esp) - instr_length(GDC, restore_eax));
    instrlist_clear(GDC, &ilist);
    /* For x86 code the address must be 32 bit */
    ASSERT_TRUNCATE(target, uint, (size_t)mode_switch_buf);
    target = (uint)(size_t)((byte *)mode_switch_buf + sz);
    /* Patch the operand of push with target of jmp far indirect.
     * 1 is the size of the opcode of push instruction.
     */
    *(uint *)(local_code_buf + 1) = target;

    /* FIXME: Need to free this page after jumping to the hook location b/c
     * after that it is no longer necessary
     */

    sz = (size_t)(pc - local_code_buf);
    /* copy local buffer to child process */
    if (!write_remote_memory_maybe64(phandle, (uint64)mode_switch_buf, local_code_buf,
                                     pc - local_code_buf, &num_bytes_out) ||
        num_bytes_out != sz) {
        return false;
    }
    return sz;
}
#endif

static uint64
inject_gencode_mapped_helper(HANDLE phandle, char *dynamo_path, uint64 hook_location,
                             byte hook_buf[EARLY_INJECT_HOOK_SIZE], byte *map,
                             void *must_reach, bool x86_code, bool late_injection,
                             uint old_hook_prot)
{
    uint64 remote_code_buf = 0, remote_data;
    byte *local_code_buf = NULL;
    uint64 pc;
    uint64 hook_code_buf = 0;
    const size_t remote_alloc_sz = 2 * PAGE_SIZE; /* one code, one data */
    const size_t code_alloc_sz = PAGE_SIZE;
    size_t hook_code_sz = PAGE_SIZE;
    uint64 switch_code_location = hook_location;
#ifdef X64
    byte *mode_switch_buf = NULL;
    byte *mode_switch_data = NULL;
    size_t switch_code_sz = PAGE_SIZE;
    size_t switch_data_sz = SWITCH_MODE_DATA_SIZE;
    if (x86_code && DYNAMO_OPTION(inject_x64)) {
        switch_data_sz += 4; // we need space for ESP and EAX
    }
#endif
    size_t num_bytes_out;
    uint old_prot;
    earliest_args_t args;
    int i;
    bool target_64 = !x86_code IF_X64(|| DYNAMO_OPTION(inject_x64));

    /* Generate code and data. */
    /* We only support low-address remote allocations. */
    IF_NOT_X64(ASSERT(!target_64 || must_reach == NULL));
    remote_code_buf =
        (uint64)allocate_remote_code_buffer(phandle, remote_alloc_sz, must_reach);
    if (remote_code_buf == 0)
        goto error;

    /* we can't use heap_mmap() in drinjectlib */
    local_code_buf = allocate_remote_code_buffer(NT_CURRENT_PROCESS, code_alloc_sz, NULL);

    hook_code_buf = remote_code_buf;
    remote_data = remote_code_buf + code_alloc_sz;
    ASSERT(sizeof(args) < PAGE_SIZE);

#ifdef X64
    if (x86_code && DYNAMO_OPTION(inject_x64)) {
        mode_switch_buf = (byte *)remote_code_buf;
        switch_code_location = (uint64)mode_switch_buf;
        mode_switch_data = (byte *)remote_data;
        remote_data += switch_data_sz;
        switch_code_sz = generate_switch_mode_jmp_to_hook(
            phandle, local_code_buf, mode_switch_buf, (byte *)hook_location,
            switch_code_sz, mode_switch_data);
        if (!switch_code_sz || switch_code_sz == PAGE_SIZE)
            goto error;
        hook_code_sz -= switch_code_sz;
        hook_code_buf += switch_code_sz;
    }
#endif

    /* see below on why it's easier to point at args in memory */
    args.dr_base = (uint64)map;
    args.ntdll_base = find_remote_dll_base(phandle, target_64, "ntdll.dll");
    if (args.ntdll_base == 0)
        goto error;
    args.tofree_base = remote_code_buf;
    args.hook_location = hook_location;
    args.hook_prot = old_hook_prot;
    args.late_injection = late_injection;
    strncpy(args.dynamorio_lib_path, dynamo_path,
            BUFFER_SIZE_ELEMENTS(args.dynamorio_lib_path));
    NULL_TERMINATE_BUFFER(args.dynamorio_lib_path);
    if (!write_remote_memory_maybe64(phandle, remote_data, &args, sizeof(args),
                                     &num_bytes_out) ||
        num_bytes_out != sizeof(args)) {
        goto error;
    }

    /* We would prefer to use IR to generate our instructions, but we need to support
     * creating 64-bit code from 32-bit DR.  XXX i#1684: Once we have multi-arch
     * cross-bitwidth IR support from a single build, switch this back to using IR.
     */
    byte *cur_local_pos = local_code_buf;
#ifdef X64
    if (x86_code && DYNAMO_OPTION(inject_x64)) {
        /* Mode Switch from 32 bit to 64 bit.
         * Forward align stack.
         */
        const byte *eax_saved_offset = mode_switch_data + 4;
        // mov dword ptr[mode_switch_data] , esp
        *cur_local_pos++ = MOV_REG32_2_RM32;
        *cur_local_pos++ = 0x24;
        *cur_local_pos++ = 0x25;
        RAW_INSERT_INT32(cur_local_pos, mode_switch_data);

        /* XXX: eax register is getting clobbered somehow in injection process,
         * and we don't know how/where yet. Thus we need to restore it now,
         * before calling RtlUserStartThread
         */
        // mov dword ptr[mode_switch_data+4], eax
        *cur_local_pos++ = MOV_REG32_2_RM32;
        *cur_local_pos++ = MOV_IMM_RM_ABS;
        RAW_INSERT_INT32(cur_local_pos, eax_saved_offset);

        /* Far jmp to next instr. */
        const int far_jmp_len = 7;
        byte *pre_jmp = cur_local_pos;
        uint64 cur_remote_pos_tmp =
            remote_code_buf + (cur_local_pos - local_code_buf + switch_code_sz);

        *cur_local_pos++ = JMP_FAR_DIRECT;
        RAW_INSERT_INT32(cur_local_pos, cur_remote_pos_tmp + far_jmp_len);
        RAW_INSERT_INT16(cur_local_pos, CS64_SELECTOR);
        ASSERT(cur_local_pos == pre_jmp + far_jmp_len);

        /* Align stack. */
        // and    rsp,0xfffffffffffffff0
        *cur_local_pos++ = 0x83;
        *cur_local_pos++ = 0xe4;
        *cur_local_pos++ = 0xf0;
    }
#endif
    /* Save xax, which we clobber below.  It is live for INJECT_LOCATION_ThreadStart.
     * We write it into earliest_args_t.app_xax, and in dynamorio_earliest_init_takeover
     * we use the saved value to update the PUSHGRP pushed xax.
     */
    if (target_64)
        *cur_local_pos++ = REX_W;
    *cur_local_pos++ = MOV_REG32_2_RM32;
    *cur_local_pos++ = MOV_IMM_RM_ABS;
    uint64 cur_remote_pos = remote_code_buf + (cur_local_pos - local_code_buf);
    RAW_INSERT_INT32(cur_local_pos,
                     target_64 ? (remote_data - (cur_remote_pos + sizeof(int)))
                               : remote_data);
    /* Restore hook rather than trying to pass contents to C code
     * (we leave hooked page writable for this and C code restores).
     */
    if (target_64)
        *cur_local_pos++ = REX_W;
    *cur_local_pos++ = MOV_IMM_XAX;
    if (target_64)
        RAW_INSERT_INT64(cur_local_pos, hook_location);
    else
        RAW_INSERT_INT32(cur_local_pos, hook_location);

    for (i = 0; i < EARLY_INJECT_HOOK_SIZE / 4; i++) {
        /* Restore bytes 4*i..4*i+3 of the hook. */
        *cur_local_pos++ = MOV_IMM32_2_RM32;
        *cur_local_pos++ = MOV_deref_disp8_EAX_2_EAX_RM;
        RAW_INSERT_INT8(cur_local_pos, i * 4);
        RAW_INSERT_INT32(cur_local_pos, *((int *)hook_buf + i));
    }
    for (i = i * 4; i < EARLY_INJECT_HOOK_SIZE; i++) {
        /* Restore byte i of the hook. */
        *cur_local_pos++ = MOV_IMM8_2_RM8;
        *cur_local_pos++ = MOV_deref_disp8_EAX_2_EAX_RM;
        RAW_INSERT_INT8(cur_local_pos, i);
        RAW_INSERT_INT8(cur_local_pos, (sbyte)hook_buf[i]);
    }

    /* Call DR earliest-takeover routine w/ retaddr pointing at hooked
     * location.  DR will free remote_code_buf.
     * If we passed regular args to a C routine, we'd clobber the args to
     * the routine we hooked.  We would then need to return here to restore,
     * it would be more complicated to free remote_code_buf, and we'd want
     * dr_insert_call() in drdecodelib, etc.  So we instead only touch
     * xax here and we target an asm routine in DR that will preserve the
     * other regs, enabling returning to the hooked routine w/ the
     * original state (except xax which is scratch and xbx which kernel
     * isn't counting on of course).
     * We pass our args in memory pointed at by xax stored in the 2nd page.
     */
    if (target_64)
        *cur_local_pos++ = REX_W;
    *cur_local_pos++ = MOV_IMM_XAX;
    if (target_64)
        RAW_INSERT_INT64(cur_local_pos, remote_data);
    else
        RAW_INSERT_INT32(cur_local_pos, remote_data);
    /* We can't use dr_insert_call() b/c it's not avail in drdecode for drinject,
     * and its main value is passing params and we can't use regular param regs.
     * we don't even want the 4 stack slots for x64 here b/c we don't want to
     * clean them up.
     */
    if (target_64)
        RAW_PUSH_INT64(cur_local_pos, switch_code_location);
    else
        RAW_PUSH_INT32(cur_local_pos, switch_code_location);
    pc =
        get_remote_proc_address(phandle, (uint64)map, "dynamorio_earliest_init_takeover");
    if (pc == 0)
        goto error;
    if (REL32_REACHABLE((int64)pc, (int64)hook_code_buf) &&
        /* over-estimate to be sure: we assert below we're < PAGE_SIZE */
        REL32_REACHABLE((int64)pc, (int64)remote_code_buf + PAGE_SIZE)) {
        *cur_local_pos++ = JMP_REL32;
        cur_remote_pos = remote_code_buf + (cur_local_pos - local_code_buf);
        RAW_INSERT_INT32(cur_local_pos,
                         (int64)pc - (int64)(cur_remote_pos + sizeof(int)));
    } else {
        /* Indirect through an inlined target. */
        *cur_local_pos++ = JMP_ABS_IND64_OPCODE;
        *cur_local_pos++ = JMP_ABS_MEM_IND64_MODRM;
        cur_remote_pos = remote_code_buf + (cur_local_pos - local_code_buf);
        RAW_INSERT_INT32(cur_local_pos, target_64 ? 0 : cur_remote_pos + sizeof(int));
        if (target_64)
            RAW_INSERT_INT64(cur_local_pos, pc);
        else
            RAW_INSERT_INT32(cur_local_pos, pc);
    }
    ASSERT(cur_local_pos - local_code_buf <= (ssize_t)hook_code_sz);

    /* copy local buffer to child process */
    if (!write_remote_memory_maybe64(phandle, hook_code_buf, local_code_buf,
                                     cur_local_pos - local_code_buf, &num_bytes_out) ||
        num_bytes_out != (size_t)(cur_local_pos - local_code_buf)) {
        goto error;
    }

    if (!remote_protect_virtual_memory_maybe64(phandle, remote_code_buf, remote_alloc_sz,
                                               PAGE_EXECUTE_READWRITE, &old_prot)) {
        ASSERT_NOT_REACHED();
        goto error;
    }

    free_remote_code_buffer(NT_CURRENT_PROCESS, local_code_buf);
    return hook_code_buf;

error:
    if (local_code_buf != NULL)
        free_remote_code_buffer(NT_CURRENT_PROCESS, local_code_buf);
    if (remote_code_buf != 0)
        free_remote_code_buffer(phandle, (byte *)(ptr_int_t)remote_code_buf);
    return 0;
}

/* i#234: earliest injection so we see every single user-mode instruction
 * Supports a 64-bit child of a 32-bit DR.
 * XXX i#625: not supporting rebasing: assuming no conflict w/ executable.
 */
static uint64
inject_gencode_mapped(HANDLE phandle, char *dynamo_path, uint64 hook_location,
                      byte hook_buf[EARLY_INJECT_HOOK_SIZE], void *must_reach,
                      bool x86_code, bool late_injection, uint old_hook_prot)
{
    bool success = false;
    NTSTATUS res;
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE section = INVALID_HANDLE_VALUE;
    byte *map = NULL;
    size_t view_size = 0;
    wchar_t dllpath[MAX_PATH];
    uint64 ret = 0;

    /* map DR dll into child
     *
     * FIXME i#625: check memory in child for conflict w/ DR from executable
     * (PEB->ImageBaseAddress doesn't seem to be set by kernel so how
     * locate executable easily?) and fall back to late injection.
     * Eventually we'll have to support rebasing from parent, or from
     * contains-no-relocation code in DR.
     */
    if (!convert_to_NT_file_path(dllpath, dynamo_path, BUFFER_SIZE_ELEMENTS(dllpath)))
        goto done;
    NULL_TERMINATE_BUFFER(dllpath);
    res = nt_create_module_file(&file, dllpath, NULL, FILE_EXECUTE | FILE_READ_DATA,
                                FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, 0);
    if (!NT_SUCCESS(res))
        goto done;

    res = nt_create_section(&section, SECTION_ALL_ACCESS, NULL, /* full file size */
                            PAGE_EXECUTE_WRITECOPY, SEC_IMAGE, file,
                            /* XXX: do we need security options to put in other process?*/
                            NULL /* unnamed */, 0, NULL, NULL);
    if (!NT_SUCCESS(res))
        goto done;

    /* For 32-into-64, there's no NtWow64 version so we rely on this simply mapping
     * into the low 2G.
     */
    res = nt_raw_MapViewOfSection(section, phandle, &map, 0, 0 /* not page-file-backed */,
                                  NULL, (PSIZE_T)&view_size, ViewUnmap,
                                  0 /* no special top-down or anything */,
                                  PAGE_EXECUTE_WRITECOPY);
    if (!NT_SUCCESS(res))
        goto done;

    ret =
        inject_gencode_mapped_helper(phandle, dynamo_path, hook_location, hook_buf, map,
                                     must_reach, x86_code, late_injection, old_hook_prot);
done:
    if (ret == 0) {
        close_handle(file);
        close_handle(section);
    }
    return ret;
}

/* Early injection. */
/* XXX: Like inject_into_thread we assume esp, but we could allocate our
 * own stack in the child and swap to that for transparency.
 */
bool
inject_into_new_process(HANDLE phandle, HANDLE thandle, char *dynamo_path, bool map,
                        uint inject_location, void *inject_address)
{
    /* To handle a 64-bit child of a 32-bit DR we use "uint64" for remote addresses. */
    uint64 hook_target = 0;
    uint64 hook_location = 0;
    uint old_prot;
    size_t num_bytes_out;
    byte hook_buf[EARLY_INJECT_HOOK_SIZE];
    bool x86_code = false;
    bool late_injection = false;
    uint64 image_entry = 0;
    union {
        /* Ensure we're not using too much stack via a union. */
        CONTEXT cxt;
#ifndef X64
        CONTEXT_64 cxt64;
#endif
    } cxt;

    /* Possible child hook points */
    GET_NTDLL(KiUserApcDispatcher,
              (IN PVOID Unknown1, IN PVOID Unknown2, IN PVOID Unknown3,
               IN PVOID ContextStart, IN PVOID ContextBody));
    GET_NTDLL(KiUserExceptionDispatcher, (IN PVOID Unknown1, IN PVOID Unknown2));

    switch (inject_location) {
    case INJECT_LOCATION_LdrLoadDll:
    case INJECT_LOCATION_LdrpLoadDll:
    case INJECT_LOCATION_LdrCustom:
    case INJECT_LOCATION_LdrpLoadImportModule:
    case INJECT_LOCATION_LdrDefault:
        /* caller provides the ldr address to use */
        ASSERT(inject_address != NULL);
        hook_location = (uint64)inject_address;
        if (hook_location == 0) {
            goto error;
        }
        break;
    case INJECT_LOCATION_KiUserApc: {
        /* FIXME i#234 NYI: for wow64 need to hook ntdll64 NtMapViewOfSection */
#ifdef NOT_DYNAMORIO_CORE_PROPER
        PEB *peb = get_own_peb();
        if (peb->OSMajorVersion >= 6) {
#else
        if (get_os_version() >= WINDOWS_VERSION_VISTA) {
#endif
            /* LdrInitializeThunk isn't in our ntdll.lib but it is
             * exported on 2K+
             */
            HANDLE ntdll_base = get_module_handle(L"ntdll.dll");
            ASSERT(ntdll_base != NULL);
            hook_location = (uint64)GET_PROC_ADDR(ntdll_base, "LdrInitializeThunk");
            ASSERT(hook_location != 0);
        } else
            hook_location = (uint64)KiUserApcDispatcher;
        ASSERT(map);
        break;
    }
    case INJECT_LOCATION_KiUserException:
        hook_location = (uint64)KiUserExceptionDispatcher;
        break;
    case INJECT_LOCATION_ImageEntry:
        hook_location = get_remote_process_entry(phandle, &x86_code);
        late_injection = true;
        break;
    case INJECT_LOCATION_ThreadStart:
        late_injection = true;
        /* Try to get the actual thread context if possible.
         * We next try looking in the remote ntdll for RtlUserThreadStart.
         * If we can't find the thread start, we fall back to the image entry, which
         * is not many instructions later.  We also need to call this first to set
         * "x86_code":
         */
        image_entry = get_remote_process_entry(phandle, &x86_code);
        if (thandle != NULL) {
            /* We can get the context for same-bitwidth, or (below) for parent32,
             * child64.  For parent64, child32, a regular query gives us
             * ntdll64!RtlUserThreadStart, which our gencode can't reach and which
             * is not actually executed: we'd need a reverse switch_modes_and_call?
             * For now we rely on the get_remote_proc_address() and assume that's
             * the thread start for parent64, child32.
             */
            if (IF_X64(!) is_32bit_process(phandle)) {
                cxt.cxt.ContextFlags = CONTEXT_CONTROL;
                if (NT_SUCCESS(nt_get_context(thandle, &cxt.cxt)))
                    hook_location = cxt.cxt.CXT_XIP;
            }
#ifndef X64
            else {
                cxt.cxt64.ContextFlags = CONTEXT_CONTROL;
                if (thread_get_context_64(thandle, &cxt.cxt64))
                    hook_location = cxt.cxt64.Rip;
            }
#endif
        }
        if (hook_location == 0) {
            bool target_64 = !x86_code IF_X64(|| DYNAMO_OPTION(inject_x64));
            uint64 ntdll_base = find_remote_dll_base(phandle, target_64, "ntdll.dll");
            uint64 thread_start =
                get_remote_proc_address(phandle, ntdll_base, "RtlUserThreadStart");
            if (thread_start != 0)
                hook_location = thread_start;
        }
        if (hook_location == 0) {
            /* Fall back to the image entry which is just a few instructions later. */
            hook_location = image_entry;
        }
        break;
    default: ASSERT_NOT_REACHED(); goto error;
    }

    /* read in code at hook */
    if (!read_remote_memory_maybe64(phandle, hook_location, hook_buf, sizeof(hook_buf),
                                    &num_bytes_out) ||
        num_bytes_out != sizeof(hook_buf)) {
        goto error;
    }
    /* Even if skipping we have to mark writable since gencode writes to it. */
    if (!remote_protect_virtual_memory_maybe64(phandle, hook_location, sizeof(hook_buf),
                                               PAGE_EXECUTE_READWRITE, &old_prot)) {
        goto error;
    }

    /* Win8 wow64 has ntdll up high but it reserves all the reachable addresses,
     * so we cannot use a relative jump to reach our code.  Rather than have
     * different hooks for different situations, we just always do an indirect
     * jump for x64.  Plus we always save the max size we need for that jump.
     * We assume there's no other thread this early (already assuming that
     * anyway) and that we restore the hook before we do anything; plus, the
     * routines we're hooking are big enough that we won't clobber anything
     * else.  Thus, we pass NULL instead of hook_location for must_reach.
     */
    if (map) {
        hook_target = inject_gencode_mapped(phandle, dynamo_path, hook_location, hook_buf,
                                            NULL, x86_code, late_injection, old_prot);
    } else {
        /* No support for 32-to-64. */
        hook_target = (uint64)inject_gencode_at_ldr(
            phandle, dynamo_path, inject_location, inject_address,
            (void *)(ptr_int_t)hook_location, hook_buf, NULL);
    }
    if (hook_target == 0)
        goto error;

    bool skip_hook = false;
    if (inject_location == INJECT_LOCATION_ThreadStart && hook_location != image_entry &&
        thandle != NULL) {
        /* XXX i#803: Having a hook at the thread start seems to cause strange
         * instability.  We instead set the thread context, like thread injection
         * does.  We should better understand the problems.
         * If we successfully set the context, we skip the hook.  The gencode
         * will still write the original instructions on top (a nop).
         */
        if (IF_X64_ELSE(true, is_32bit_process(phandle))) {
            cxt.cxt.ContextFlags = CONTEXT_CONTROL;
            if (NT_SUCCESS(nt_get_context(thandle, &cxt.cxt))) {
                cxt.cxt.CXT_XIP = (ptr_uint_t)hook_target;
                if (NT_SUCCESS(nt_set_context(thandle, &cxt.cxt)))
                    skip_hook = true;
            }
        }
#ifndef X64
        else {
            cxt.cxt64.ContextFlags = CONTEXT_CONTROL;
            if (thread_get_context_64(thandle, &cxt.cxt64)) {
                cxt.cxt64.Rip = hook_target;
                if (thread_set_context_64(thandle, &cxt.cxt64)) {
                    skip_hook = true;
                }
            }
        }
#endif
    }
    if (!skip_hook) {
        /* Place hook */
        if (REL32_REACHABLE((int64)hook_location + 5, (int64)hook_target)) {
            hook_buf[0] = JMP_REL32;
            *(int *)(&hook_buf[1]) =
                (int)((int64)hook_target - ((int64)hook_location + 5));
        } else {
            hook_buf[0] = JMP_ABS_IND64_OPCODE;
            hook_buf[1] = JMP_ABS_MEM_IND64_MODRM;
            *(int *)(&hook_buf[2]) = 0; /* rip-rel to following address */
            *(uint64 *)(&hook_buf[6]) = hook_target;
        }
    }
    if (!write_remote_memory_maybe64(phandle, hook_location, hook_buf, sizeof(hook_buf),
                                     &num_bytes_out) ||
        num_bytes_out != sizeof(hook_buf)) {
        goto error;
    }
    if (!map) {
        /* For map we restore the hook from gencode to avoid having to pass
         * the displaced code around.  But, we can't invoke lib routines easily,
         * so we can't mark +w from gencode easily: so we just leave it +w
         * and restore to +rx in dynamorio_earliest_init_takeover_C().
         */
        if (!remote_protect_virtual_memory_maybe64(
                phandle, hook_location, sizeof(hook_buf), old_prot, &old_prot)) {
            goto error;
        }
    }

    return true;

error:
    /* we do not recover any changes in the child's address space */
    return false;
}
