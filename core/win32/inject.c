/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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
#include "../globals.h"            /* for pragma warning's and assert defines */
#include "../module_shared.h"      /* for get_proc_address() */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "ntdll.h"  /* for get/set context etc. */
#include "os_private.h"  /* for load_dynamo */

#ifndef NOT_DYNAMORIO_CORE_PROPER
# include "os_private.h" /* for get_proc_address() */
# define GET_PROC_ADDR get_proc_address
#else
# define GET_PROC_ADDR GetProcAddress
#endif

/* this entry point is hardcoded, FIXME : abstract */
#define DYNAMORIO_ENTRY "dynamo_auto_start"

#ifdef DEBUG
/* for asserts, we import globals.h now (for pragmas) so don't need to 
 * duplicate assert defines, declarations */
extern void display_error(char *msg);
#else
# define display_error(msg) ((void) 0)
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
    addr_getprocaddr = (ptr_uint_t) GET_PROC_ADDR(kern32, "GetProcAddress");
    ASSERT(addr_getprocaddr != 0);
    addr_loadlibrarya = (ptr_uint_t) GET_PROC_ADDR(kern32, "LoadLibraryA");
    ASSERT(addr_loadlibrarya != 0);
# ifdef LOAD_DYNAMO_DEBUGBREAK
    addr_debugbreak = (ptr_uint_t) GET_PROC_ADDR(kern32, "DebugBreak");
    ASSERT(addr_debugbreak != NULL);
# endif
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
inject_into_thread(HANDLE phandle, CONTEXT *cxt, HANDLE thandle,
                   char *dynamo_path)
{
    size_t              nbytes;
    bool                success = false;
    ptr_uint_t          dynamo_entry_esp;
    ptr_uint_t          dynamo_path_esp;
    LPVOID              load_dynamo_code = NULL; /* = base of code allocation */
    ptr_uint_t          addr;
    reg_t               *bufptr;
    char                buf[MAX_PATH*2];
    uint                old_prot;

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
        SYSLOG_INTERNAL_WARNING("Using late inject follow children from early injected process, unsafe LdrLock usage");
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
        memcpy(buf, (char*)load_dynamo, SIZE_OF_LOAD_DYNAMO);
        /* R-X protection is adequate for our non-self modifying code,
         * and we'll update that after we're done with
         * nt_write_virtual_memory() calls */

        /* get allocation, this will be freed by os_heap_free, so make sure
         * is compatible allocation method */
        if (!NT_SUCCESS(nt_remote_allocate_virtual_memory(phandle, &load_dynamo_code, 
                                                          SIZE_OF_LOAD_DYNAMO,
                                                          PAGE_EXECUTE_READWRITE,
                                                          MEMORY_COMMIT))) {
            display_error("Failed to allocate memory for injection code");
            goto error;
        }
        if (!nt_write_virtual_memory(phandle, load_dynamo_code, buf,
                                     SIZE_OF_LOAD_DYNAMO, &nbytes)) {
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
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     buf, nbytes, &nbytes)) {
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
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     buf, nbytes, &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* copy the current context to the app's stack. Only need the
         * control registers, so we use a priv_mcontext_t layout.
         */
        ASSERT(BUFFER_SIZE_BYTES(buf) >= sizeof(priv_mcontext_t));
        bufptr = (reg_t*) buf;
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
        bufptr += PRE_XMM_PADDING/sizeof(*bufptr);
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
            for (i = 0; i < NUM_XMM_SLOTS; i++) {
                for (j = 0; j < IF_X64_ELSE(2,4); j++) {
                    *bufptr++ = CXT_XMM(cxt, i)->reg[j];
                }
                /* FIXME i#437: save ymm fields.  For now we assume we're
                 * not saving and we just skip the upper 128 bits.
                 */
                bufptr += IF_X64_ELSE(2,4);
            }
        } else {
            /* skip xmm slots */
            bufptr += XMM_SLOTS_SIZE/sizeof(*bufptr);
        }
        ASSERT((char *)bufptr - (char *)buf == sizeof(priv_mcontext_t));
        *bufptr++ = (ptr_uint_t)load_dynamo_code;
        *bufptr++ = SIZE_OF_LOAD_DYNAMO;
        nbytes = sizeof(priv_mcontext_t) + 2*sizeof(reg_t);
        cxt->CXT_XSP -= nbytes;
#ifdef X64
        /* We need xsp to be aligned prior to each call, but we can only pad
         * before the context as all later users assume the info they need is
         * at TOS.
         */
        cxt->CXT_XSP = ALIGN_BACKWARD(cxt->CXT_XSP, 16);
#endif
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP,
                                     buf, nbytes, &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of the DYNAMORIO_ENTRY string on the app's stack */
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     &dynamo_entry_esp, sizeof(dynamo_entry_esp),
                                     &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of GetProcAddress on the app's stack */
        ASSERT(addr_getprocaddr);
        addr = addr_getprocaddr;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     &addr, sizeof(addr), &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of the dynamorio_path string on the app's stack */
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     &dynamo_path_esp, sizeof(dynamo_path_esp),
                                     &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

        /* push the address of LoadLibraryA on the app's stack */
        ASSERT(addr_loadlibrarya);
        addr = addr_loadlibrarya;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     &addr, sizeof(addr), &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }

#ifdef LOAD_DYNAMO_DEBUGBREAK
        /* push the address of DebugBreak on the app's stack */
        ASSERT(addr_debugbreak);
        addr = addr_debugbreak;
        cxt->CXT_XSP -= XSP_SZ;
        if (!nt_write_virtual_memory(phandle, (LPVOID)cxt->CXT_XSP, 
                                     &addr, sizeof(addr), &nbytes)) {
            display_error("WriteMemory failed");
            goto error;
        }
#endif

        /* make the code R-X now */
        if (!nt_remote_protect_virtual_memory(phandle, load_dynamo_code, 
                                              SIZE_OF_LOAD_DYNAMO,
                                              PAGE_EXECUTE_READ, &old_prot)) {
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
 * os_shared.h, emit_utils.c etc.) in one place. */
enum {
    PUSHF                 = 0x9c,
    POPF                  = 0x9d,
    PUSHA                 = 0x60,
    POPA                  = 0x61,
    PUSH_EAX              = 0x50,
    POP_EAX               = 0x58,
    PUSH_ECX              = 0x51,
    POP_ECX               = 0x59,
    PUSH_IMM32            = 0x68,
    PUSH_IMM8             = 0x6a,

    JMP_REL8              = 0xeb,
    JMP_REL32             = 0xe9,
    CALL_REL32            = 0xe8,
    CALL_RM32             = 0xff,
    CALL_EAX_RM           = 0xd0,

    MOV_RM32_2_REG32      = 0x8b,
    MOV_ESP_2_EAX_RM      = 0xc4,
    MOV_EAX_2_ECX_RM      = 0xc8,
    MOV_EAX_2_EDX_RM      = 0xd0,
    MOV_EAX_2_EAX_RM      = 0xc0,
    MOV_derefEAX_2_EAX_RM = 0x00,
    MOV_IMM8_2_RM8        = 0xc6,
    MOV_IMM32_2_RM32      = 0xc7,
    MOV_IMM_RM_ABS        = 0x05,

    ADD_EAX_IMM32         = 0x05,

    CMP_EAX_IMM32         = 0x3d,
    JZ_REL8               = 0x74,
    JNZ_REL8              = 0x75,

#ifdef X64
    REX_W                 = 0x48,
    REX_B                 = 0x41,
    REX_R                 = 0x44,
#endif
};

#define DEBUG_LOOP 0

#define ASSERT_ROOM(cur, buf, maxlen) \
    ASSERT(cur + maxlen < buf + sizeof(buf))

/* i#142: 64-bit support only works if the hook location and the
 * allocated remote_code_buffer are in lower 2GB of the address space.
 * we ensure the latter, and ntdll seems to always be there, at least
 * on Vista and Win7.  just haven't bothered to write full
 * 64-bit-reachable code: partly b/c of next complaint:
 *
 * XXX: this is all really messy: these macros are too limited for
 * inserting general instructions, so for x64 I hacked it by leaving
 * in the pushes and copying from TOS into the register params.
 * I would prefer to throw all this out and replace w/ IR or asm
 * (and deal w/ linking into drinject).
 */
#ifdef X64
# define CHECK_S64(val) \
    ASSERT_NOT_IMPLEMENTED(CHECK_TRUNCATE_TYPE_int((ptr_int_t)(val)))
# define CHECK_U64(val) \
    ASSERT_NOT_IMPLEMENTED(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)(val)))
#else
# define CHECK_S64(val) /* nothing */
# define CHECK_U64(val) /* nothing */
#endif

/* Early injection. */
/* FIXME - like inject_into_thread we assume esp, but we could allocate our
 * own stack in the child and swap to that for transparency. */
bool
inject_into_new_process(HANDLE phandle, char *dynamo_path, bool map,
                        uint inject_location, void *inject_address)
{
    void *hook_target = NULL, *hook_location = NULL;
    uint old_prot; 
    size_t num_bytes_out;
    byte hook_buf[5];

    /* Possible child hook points */
    GET_NTDLL(KiUserApcDispatcher, (IN PVOID Unknown1, 
                                    IN PVOID Unknown2, 
                                    IN PVOID Unknown3, 
                                    IN PVOID ContextStart, 
                                    IN PVOID ContextBody));
    GET_NTDLL(KiUserExceptionDispatcher, (IN PVOID Unknown1, 
                                          IN PVOID Unknown2));

    /* Only ones that work, though I have hopes for KiUserException if can
     * find a better spot to trigger the exception, or we should implement
     * KiUserApc map requirement. */
    ASSERT_NOT_IMPLEMENTED(INJECT_LOCATION_IS_LDR(inject_location));
    switch(inject_location) {
    case INJECT_LOCATION_LdrLoadDll:
    case INJECT_LOCATION_LdrpLoadDll:
    case INJECT_LOCATION_LdrCustom:
    case INJECT_LOCATION_LdrpLoadImportModule:
    case INJECT_LOCATION_LdrDefault:
        /* caller provides the ldr address to use */
        ASSERT(inject_address != NULL);
        hook_location = inject_address;
        if (hook_location == NULL) {
            goto error;
        }
        break;
    case INJECT_LOCATION_KiUserApc:
        hook_location = (void *)KiUserApcDispatcher;
        ASSERT(map);
        break;
    case INJECT_LOCATION_KiUserException:
        hook_location = (void *)KiUserExceptionDispatcher;
        break;
    default:
        ASSERT_NOT_REACHED();
        goto error;
    }

    /* read in code at hook */
    if (!nt_read_virtual_memory(phandle, hook_location, hook_buf,
                                sizeof(hook_buf), &num_bytes_out) ||
        num_bytes_out != sizeof(hook_buf)) {
        goto error;
    }

    if (map) {
        hook_target = NULL; /* for compiler */
        /* NYI see case 102, plan is to remote map in our dll, link and rebase if
         * necessary and hook to a routine in our dll */
        ASSERT_NOT_IMPLEMENTED(false);
    } else {
        byte *remote_code_buffer = NULL, *remote_data_buffer;
        /* max usage for local_buf is for writing the dr library name
         * 2*MAX_PATH (unicode) + sizoef(UNICODE_STRING) + 2, round up to
         * 3*MAX_PATH to be safe */
        byte local_buf[3*MAX_PATH];
        byte *cur_local_pos, *cur_remote_pos, *jmp_fixup1, *jmp_fixup2;
        char *takeover_func = "dynamorio_app_init_and_early_takeover";
        PUNICODE_STRING mod, mod_remote;
        PANSI_STRING func, func_remote;
        int res;
        size_t num_bytes_in;

        GET_NTDLL(LdrLoadDll, (IN PCWSTR PathToFile OPTIONAL,
                               IN PULONG Flags OPTIONAL,
                               IN PUNICODE_STRING ModuleFileName,
                               OUT PHANDLE ModuleHandle));
        GET_NTDLL(LdrGetProcedureAddress, (IN HANDLE ModuleHandle,
                                           IN PANSI_STRING ProcedureName OPTIONAL,
                                           IN ULONG Ordinal OPTIONAL,
                                           OUT FARPROC *ProcedureAddress));
#define GET_PROC_ADDR_BAD_ADDR 0xffbadd11
        GET_NTDLL(NtProtectVirtualMemory, (IN HANDLE ProcessHandle,
                                           IN OUT PVOID *BaseAddress,
                                           IN OUT PULONG ProtectSize,
                                           IN ULONG NewProtect,
                                           OUT PULONG OldProtect));
        GET_NTDLL(NtContinue, (IN PCONTEXT Context,
                               IN BOOLEAN TestAlert));

        /* get buffer for emitted code and data */
#ifdef X64
        /* we require lower-2GB so pass a hint */
        remote_code_buffer = (byte *)(ptr_int_t) 0x30000;
        res = nt_remote_allocate_virtual_memory(phandle, &remote_code_buffer,
                                                2*PAGE_SIZE, PAGE_READWRITE,
                                                MEM_COMMIT);
        if (res == STATUS_CONFLICTING_ADDRESSES) {
            /* try again w/ no hint: often ends up in low GB, no guarantee though */
            remote_code_buffer = NULL;
        } else if (!NT_SUCCESS(res))
            goto error;
#endif
        if (remote_code_buffer == NULL &&
            !NT_SUCCESS(nt_remote_allocate_virtual_memory(phandle, &remote_code_buffer,
                                                          2*PAGE_SIZE, PAGE_READWRITE,
                                                          MEM_COMMIT))) {
            goto error;
        }
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
        ASSERT_ROOM(cur_local_pos, local_buf, 2*MAX_PATH+2 /* plus null */);
        res = snwprintf((wchar_t *)cur_local_pos, 2*MAX_PATH, L"%hs", dynamo_path);
        ASSERT(res > 0);
        if (res > 0) {
            cur_local_pos += (2*res);
            ASSERT_TRUNCATE(mod->Length, ushort, 2*res);
            mod->Length = (ushort)(2*res);
            mod->MaximumLength = (ushort)(2*res);
        }
        /* ensure NULL termination, just in case */
        *(wchar_t *)cur_local_pos = L'\0';
        cur_local_pos += sizeof(wchar_t);
        /* write to remote process */
        num_bytes_in = cur_local_pos - local_buf;
        if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf,
                                     num_bytes_in, &num_bytes_out) ||
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
        func->Buffer = (PCHAR) cur_remote_pos + (cur_local_pos - local_buf);
        ASSERT_ROOM(cur_local_pos, local_buf, strlen(takeover_func)+1);
        strncpy((char *)cur_local_pos, takeover_func, strlen(takeover_func));
        cur_local_pos += strlen(takeover_func);
        ASSERT_TRUNCATE(func->Length, ushort, strlen(takeover_func));
        func->Length = (ushort)strlen(takeover_func);
        func->MaximumLength = (ushort)strlen(takeover_func);
        *cur_local_pos++ = '\0'; /* ensure NULL termination, just in case */
        /* write to remote_process */
        num_bytes_in = cur_local_pos - local_buf;
        if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf,
                                     num_bytes_in, &num_bytes_out) ||
            num_bytes_out != num_bytes_in) {
            goto error;
        }
        func_remote = (PANSI_STRING)cur_remote_pos;
        cur_remote_pos += num_bytes_out;
        
        /* now make data page read only */
        res = nt_remote_protect_virtual_memory(phandle, remote_data_buffer, 
                                               PAGE_SIZE, PAGE_READONLY,
                                               &old_prot);
        ASSERT(res);
        
#define INSERT_INT(value)         \
  *(int *)cur_local_pos = value;  \
  cur_local_pos += sizeof(int)

#define INSERT_ADDR(value)              \
  *(ptr_int_t *)cur_local_pos = (ptr_int_t)(value);       \
  cur_local_pos += sizeof(ptr_int_t)

#ifdef X64
# define INSERT_PUSH_ALL_REG()     \
  *cur_local_pos++ = PUSH_EAX;  \
  *cur_local_pos++ = PUSH_ECX;  \
  *cur_local_pos++ = 0x52; /* xdx */ \
  *cur_local_pos++ = 0x53; /* xbx */ \
  *cur_local_pos++ = 0x54; /* xsp */ \
  *cur_local_pos++ = 0x55; /* xbp */ \
  *cur_local_pos++ = 0x56; /* xsi */ \
  *cur_local_pos++ = 0x57; /* xdi */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = PUSH_EAX; /* r8 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = PUSH_ECX; /* r9 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x52; /* r10 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x53; /* r11 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x54; /* r12 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x55; /* r13 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x56; /* r14 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x57; /* r15 */
#else
# define INSERT_PUSH_ALL_REG()     \
  *cur_local_pos++ = PUSHA
#endif

#ifdef X64
# define INSERT_POP_ALL_REG()     \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5f; /* r15 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5e; /* r14 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5d; /* r13 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5c; /* r12 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5b; /* r11 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = 0x5a; /* r10 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = POP_ECX; /* r9 */ \
  *cur_local_pos++ = REX_B;  \
  *cur_local_pos++ = POP_EAX; /* r8 */ \
  *cur_local_pos++ = 0x5f; /* xdi */ \
  *cur_local_pos++ = 0x5e; /* xsi */ \
  *cur_local_pos++ = 0x5d; /* xbp */ \
  *cur_local_pos++ = 0x5b; /* xsp slot but popped into dead xbx */ \
  *cur_local_pos++ = 0x5b; /* xbx */ \
  *cur_local_pos++ = 0x5a; /* xdx */ \
  *cur_local_pos++ = POP_ECX;  \
  *cur_local_pos++ = POP_EAX
#else
# define INSERT_POP_ALL_REG()     \
  *cur_local_pos++ = POPA
#endif

#define PUSH_IMMEDIATE(value)     \
  *cur_local_pos++ = PUSH_IMM32;  \
  INSERT_INT(value)

#define PUSH_SHORT_IMMEDIATE(value)     \
  *cur_local_pos++ = PUSH_IMM8;         \
  *cur_local_pos++ = value

#define MOV_ESP_TO_EAX()                \
  IF_X64(*cur_local_pos++ = REX_W;)     \
  *cur_local_pos++ = MOV_RM32_2_REG32;  \
  *cur_local_pos++ = MOV_ESP_2_EAX_RM

#ifdef X64
/* mov rax -> rcx */
# define MOV_EAX_TO_PARAM_0()           \
  *cur_local_pos++ = REX_W;             \
  *cur_local_pos++ = MOV_RM32_2_REG32;  \
  *cur_local_pos++ = MOV_EAX_2_ECX_RM
/* mov rax -> rdx */
# define MOV_EAX_TO_PARAM_1()           \
  *cur_local_pos++ = REX_W;             \
  *cur_local_pos++ = MOV_RM32_2_REG32;  \
  *cur_local_pos++ = MOV_EAX_2_EDX_RM
/* mov rax -> r8 */
# define MOV_EAX_TO_PARAM_2()           \
  *cur_local_pos++ = REX_R|REX_W;       \
  *cur_local_pos++ = MOV_RM32_2_REG32;  \
  *cur_local_pos++ = MOV_EAX_2_EAX_RM
/* mov rax -> r9 */
# define MOV_EAX_TO_PARAM_3()           \
  *cur_local_pos++ = REX_R|REX_W;       \
  *cur_local_pos++ = MOV_RM32_2_REG32;  \
  *cur_local_pos++ = MOV_EAX_2_ECX_RM
/* mov (rsp) -> rcx */
# define MOV_TOS_TO_PARAM_0()           \
  *cur_local_pos++ = REX_W;             \
  *cur_local_pos++ = 0x8b;              \
  *cur_local_pos++ = 0x0c;              \
  *cur_local_pos++ = 0x24
/* mov (rsp) -> rdx */
# define MOV_TOS_TO_PARAM_1()           \
  *cur_local_pos++ = REX_W;             \
  *cur_local_pos++ = 0x8b;              \
  *cur_local_pos++ = 0x14;              \
  *cur_local_pos++ = 0x24
/* mov (rsp) -> r8 */
# define MOV_TOS_TO_PARAM_2()           \
  *cur_local_pos++ = REX_R|REX_W;       \
  *cur_local_pos++ = 0x8b;              \
  *cur_local_pos++ = 0x04;              \
  *cur_local_pos++ = 0x24
/* mov (rsp) -> r9 */
# define MOV_TOS_TO_PARAM_3()           \
  *cur_local_pos++ = REX_R|REX_W;       \
  *cur_local_pos++ = 0x8b;              \
  *cur_local_pos++ = 0x0c;              \
  *cur_local_pos++ = 0x24
#endif /* X64 */

/* FIXME - all values are small use imm8 version */
#define ADD_TO_EAX(value)               \
  IF_X64(*cur_local_pos++ = REX_W;)     \
  *cur_local_pos++ = ADD_EAX_IMM32;     \
  INSERT_INT(value)

#define ADD_IMM8_TO_ESP(value)          \
  IF_X64(*cur_local_pos++ = REX_W;)     \
  *cur_local_pos++ = 0x83;              \
  *cur_local_pos++ = 0xc4;              \
  *cur_local_pos++ = (byte)(value);

#define CMP_TO_EAX(value)               \
  IF_X64(*cur_local_pos++ = REX_W;)     \
  *cur_local_pos++ = CMP_EAX_IMM32;     \
  INSERT_INT(value)

#define INSERT_REL32_ADDRESS(target)    \
  IF_X64(ASSERT_NOT_IMPLEMENTED(REL32_REACHABLE( \
    ((cur_local_pos - local_buf)+4)+cur_remote_pos, (byte *)(target)))); \
  INSERT_INT((int)(ptr_int_t)((byte *)target - \
                              (((cur_local_pos - local_buf)+4)+cur_remote_pos)))

#define CALL(target_func)               \
  *cur_local_pos++ = CALL_REL32;        \
  INSERT_REL32_ADDRESS(target_func)

/* ecx will hold OldProtection afterwards */
/* for x64 we need the 4 stack slots anyway so we do the pushes */
#define PROT_IN_ECX 0xbad15bad /* doesn't match a PAGE_* define */
#define CHANGE_PROTECTION(start, size, new_protection)                \
  *cur_local_pos++ = PUSH_EAX; /* OldProtect slot */                  \
  MOV_ESP_TO_EAX(); /* get &OldProtect */                             \
  CHECK_S64((ptr_int_t)ALIGN_FORWARD(start+size, PAGE_SIZE) -         \
    (ptr_int_t)ALIGN_BACKWARD(start, PAGE_SIZE)); \
  PUSH_IMMEDIATE((int)(ALIGN_FORWARD(start+size, PAGE_SIZE) -         \
                 ALIGN_BACKWARD(start, PAGE_SIZE))); /* ProtectSize */ \
  CHECK_U64(ALIGN_BACKWARD(start, PAGE_SIZE));                        \
  PUSH_IMMEDIATE((int)ALIGN_BACKWARD(start, PAGE_SIZE)); /* BaseAddress */ \
  *cur_local_pos++ = PUSH_EAX; /* arg 5 &OldProtect */                \
  if (new_protection == PROT_IN_ECX) {                                \
      *cur_local_pos++ = PUSH_ECX; /* arg 4 NewProtect */             \
  } else {                                                            \
      PUSH_IMMEDIATE(new_protection); /* arg 4 NewProtect */          \
  }                                                                   \
  IF_X64(MOV_TOS_TO_PARAM_3());                                       \
  ADD_TO_EAX(-(int)XSP_SZ); /* get &ProtectSize */                         \
  *cur_local_pos++ = PUSH_EAX; /* arg 3 &ProtectSize */               \
  IF_X64(MOV_EAX_TO_PARAM_2());                                       \
  ADD_TO_EAX(-(int)XSP_SZ); /* get &BaseAddress */                         \
  *cur_local_pos++ = PUSH_EAX; /* arg 2 &BaseAddress */               \
  IF_X64(MOV_EAX_TO_PARAM_1());                                       \
  PUSH_IMMEDIATE((int)(ptr_int_t)NT_CURRENT_PROCESS); /* arg ProcessHandle */ \
  IF_X64(MOV_TOS_TO_PARAM_0());                                       \
  CALL(NtProtectVirtualMemory);                                       \
  /* no error checking, can't really do anything about it, FIXME */   \
  /* stdcall so just the three slots we made for the ptr arguments    \
   * left on the stack for 32-bit */                                  \
  IF_X64(ADD_IMM8_TO_ESP(5*XSP_SZ)); /* clean up 5 slots */           \
  *cur_local_pos++ = POP_ECX; /* pop BaseAddress */                   \
  *cur_local_pos++ = POP_ECX; /* pop ProtectSize */                   \
  *cur_local_pos++ = POP_ECX /* pop OldProtect into ecx */


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
            hook_target = cur_remote_pos + sizeof(ptr_int_t);  /* skip the address */
        }

#if DEBUG_LOOP
        *cur_local_pos++ = JMP_REL8;
        *cur_local_pos++ = 0xfe;
#endif

        /* save current state */
        INSERT_PUSH_ALL_REG();
        *cur_local_pos++ = PUSHF;

        /* restore trampoline, first make writable */
        CHANGE_PROTECTION(hook_location, 5, PAGE_EXECUTE_READWRITE);
        *cur_local_pos++ = MOV_IMM32_2_RM32; /* restore first 4 bytes of hook */
        *cur_local_pos++ = MOV_IMM_RM_ABS;
        CHECK_U64(hook_location);
        CHECK_U64(cur_local_pos+8 - local_buf + cur_remote_pos);
        INSERT_INT((int)(ptr_int_t)hook_location 
                   /* rip-rel for x64 */
                   IF_X64(- (int)(ptr_int_t)(cur_local_pos+8 - local_buf +
                                             cur_remote_pos)));
        INSERT_INT(*(int *)hook_buf);
        *cur_local_pos++ = MOV_IMM8_2_RM8; /* restore 5th byte of the hook */
        *cur_local_pos++ = MOV_IMM_RM_ABS;
        CHECK_U64((ptr_int_t)hook_location+4);
        CHECK_U64(cur_local_pos+5 - local_buf + cur_remote_pos);
        INSERT_INT((int)(ptr_int_t)hook_location+4
                   /* rip-rel for x64 */
                   IF_X64(- (int)(ptr_int_t)(cur_local_pos+5 - local_buf +
                                             cur_remote_pos)));
        *cur_local_pos++ = hook_buf[4];
        /* hook restored, restore protection */
        CHANGE_PROTECTION(hook_location, 5, PROT_IN_ECX);

        if (inject_location == INJECT_LOCATION_KiUserException) {
            /* Making the first page of the image unreadable triggers an exception
             * to early to use the loader, might try pointing the import table ptr
             * to bad memory instead TOTRY, whatever we do should fixup here */
            ASSERT_NOT_IMPLEMENTED(false);
        }
        
        /* call LdrLoadDll to load dr library */
        *cur_local_pos++ = PUSH_EAX; /* need slot for OUT hmodule*/
        MOV_ESP_TO_EAX();
        *cur_local_pos++ = PUSH_EAX; /* arg 4 OUT *hmodule */
        IF_X64(MOV_EAX_TO_PARAM_3());
        CHECK_U64(mod_remote);
        PUSH_IMMEDIATE((int)(ptr_int_t)mod_remote); /* our library name */
        IF_X64(MOV_TOS_TO_PARAM_2());
        PUSH_SHORT_IMMEDIATE(0x0); /* Flags OPTIONAL */
        IF_X64(MOV_TOS_TO_PARAM_1());
        PUSH_SHORT_IMMEDIATE(0x0); /* PathToFile OPTIONAL */
        IF_X64(MOV_TOS_TO_PARAM_0());
        CALL(LdrLoadDll); /* see signature at decleration above */
        IF_X64(ADD_IMM8_TO_ESP(4*XSP_SZ)); /* clean up 4 slots */

        /* stdcall so removed args so top of stack is now the slot containing the
         * returned handle.  Use LdrGetProcedureAddress to get the address of the
         * dr init and takeover function. Is ok to call even if LdrLoadDll failed,
         * so we check for errors afterwards. */
        *cur_local_pos++ = POP_ECX; /* dr module handle */
        *cur_local_pos++ = PUSH_ECX; /* need slot for out ProcedureAddress */
        MOV_ESP_TO_EAX();
        *cur_local_pos++ = PUSH_EAX; /* arg 4 OUT *ProcedureAddress */
        IF_X64(MOV_EAX_TO_PARAM_3());
        PUSH_SHORT_IMMEDIATE(0x0); /* Ordinal OPTIONAL */
        IF_X64(MOV_TOS_TO_PARAM_2());
        CHECK_U64(func_remote);
        PUSH_IMMEDIATE((int)(ptr_int_t)func_remote); /* func name */
        IF_X64(MOV_TOS_TO_PARAM_1());
        *cur_local_pos++ = PUSH_ECX; /* module handle */
        IF_X64(MOV_TOS_TO_PARAM_0());
        CALL(LdrGetProcedureAddress); /* see signature at decleration above */
        IF_X64(ADD_IMM8_TO_ESP(4*XSP_SZ)); /* clean up 4 slots */

        /* Top of stack is now the dr init and takeover function (stdcall removed
         * args). Check for errors and bail (FIXME debug build report somehow?) */
        CMP_TO_EAX(STATUS_SUCCESS);
        *cur_local_pos++ = POP_EAX; /* dr init_and_takeover function */
        *cur_local_pos++ = JNZ_REL8; /* FIXME - should check >= 0 instead? */
        jmp_fixup1 = cur_local_pos++; /* jmp to after call below */
        /* Xref case 8373, LdrGetProcedureAdderss sometimes returns an
         * address of 0xffbadd11 even though it returned STATUS_SUCCESS */
        CMP_TO_EAX(GET_PROC_ADDR_BAD_ADDR);
        *cur_local_pos++ = JZ_REL8; /* JZ == JE */
        jmp_fixup2 = cur_local_pos++; /* jmp to after call below */
        IF_X64(ADD_IMM8_TO_ESP(-2*(int)XSP_SZ)); /* need 4 slots total */
        CHECK_U64(remote_code_buffer);
        PUSH_IMMEDIATE((int)(ptr_int_t)remote_code_buffer); /* arg to takeover func */
        IF_X64(MOV_TOS_TO_PARAM_1());
        PUSH_IMMEDIATE(inject_location); /* arg to takeover func */
        IF_X64(MOV_TOS_TO_PARAM_0());
        *cur_local_pos++ = CALL_RM32; /* call EAX */
        *cur_local_pos++ = CALL_EAX_RM;
#ifdef X64
        IF_X64(ADD_IMM8_TO_ESP(4*XSP_SZ)); /* clean up 4 slots */
#else
        *cur_local_pos++ = POP_ECX; /* cdecl so pop arg */
        *cur_local_pos++ = POP_ECX; /* cdecl so pop arg */
#endif
        /* Now patch the jnz above (if error) to go to here */
        ASSERT_TRUNCATE(*jmp_fixup1, byte, cur_local_pos - (jmp_fixup1+1));
        *jmp_fixup1 = (byte)(cur_local_pos - (jmp_fixup1+1)); /* target of jnz */
        ASSERT_TRUNCATE(*jmp_fixup2, byte, cur_local_pos - (jmp_fixup2+1));
        *jmp_fixup2 = (byte)(cur_local_pos - (jmp_fixup2+1)); /* target of jz */
        *cur_local_pos++ = POPF;
        INSERT_POP_ALL_REG();
        if (inject_location != INJECT_LOCATION_KiUserException) {
            /* jmp back to the hook location to resume execution */
            *cur_local_pos++ = JMP_REL32;
            INSERT_REL32_ADDRESS(hook_location);
        } else {
            /* we triggered the exception, so do an NtContinue back */
            /* see callback.c, esp+4 holds CONTEXT ** */
            *cur_local_pos++ = POP_EAX;  /* EXCEPTION_RECORD ** */
            *cur_local_pos++ = POP_EAX;  /* CONTEXT ** */
            PUSH_SHORT_IMMEDIATE(FALSE); /* arg 2 TestAlert */
            IF_X64(MOV_TOS_TO_PARAM_1());
            *cur_local_pos++ = MOV_RM32_2_REG32;
            *cur_local_pos++ = MOV_derefEAX_2_EAX_RM; /* CONTEXT * -> EAX */
            *cur_local_pos++ = PUSH_EAX; /* push CONTEXT * (arg 1) */
            IF_X64(MOV_EAX_TO_PARAM_0());
            IF_X64(ADD_IMM8_TO_ESP(-4*(int)XSP_SZ)); /* 4 slots */
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
        if (!nt_write_virtual_memory(phandle, cur_remote_pos, local_buf,
                                     num_bytes_in, &num_bytes_out) ||
            num_bytes_out != num_bytes_in) {
            goto error;
        }
        cur_remote_pos += num_bytes_out;
        /* now make code page rx */
        res = nt_remote_protect_virtual_memory(phandle, remote_code_buffer, 
                                               PAGE_SIZE, PAGE_EXECUTE_READ,
                                               &old_prot);
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
    }

    /* place hook */
    ASSERT(sizeof(hook_buf) == 5); /* standard 5 byte jmp rel32 hook */
    hook_buf[0] = JMP_REL32;
    IF_X64(ASSERT_NOT_IMPLEMENTED(REL32_REACHABLE
                                  ((byte *)hook_location + 5, (byte*)hook_target)));
    *(int *)(&hook_buf[1]) = (int)((byte *)hook_target - ((byte *)hook_location + 5));
    if (!nt_remote_protect_virtual_memory(phandle, hook_location,
                                          sizeof(hook_buf),
                                          PAGE_EXECUTE_READWRITE, &old_prot)) {
        goto error;
    }
    if (!nt_write_virtual_memory(phandle, hook_location, hook_buf,
                                 sizeof(hook_buf), &num_bytes_out) ||
        num_bytes_out != sizeof(hook_buf)) {
        goto error;
    }
    if (!nt_remote_protect_virtual_memory(phandle, hook_location,
                                          sizeof(hook_buf),
                                          old_prot, &old_prot)) {
        goto error;
    }

    return true;

    error:
    /* we do not recover any changes in the child's address space */
    return false;
}
