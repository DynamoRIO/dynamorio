/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* winsysnums.c: analyzes a dll's exported routines, looking for system call
 * numbers or Ki* routines -- typically pointed at a new ntdll.dll
 *
 * now additionally uses drsyms to analyze all symbols and thus locate
 * system call wrappers that are not exported.
 */

/* Uses the DR API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 *
 * To build, use cmake.  Build as 64-bit (no reason to build a 32-bit version
 * as it won't be able to analyze 64-bit dlls, while a 64-bit build can analyze
 * 32-bit dlls).
 * Something like:
 *   % x64
 *   % mkdir ~/dr/git/build_winsysnums
 *   % cd !$
 *   % cmake -DDynamoRIO_DIR=e:/src/dr/git/exports/cmake ../src/clients/standalone
 *   % cmake --build .
 * A copy is now built as part of a regular DR build.
 *
 * To run, you need to put dynamorio.dll, drsyms.dll, and dbghelp.dll into the
 * same directory as winsysnums.exe.  (If you build drsyms statically you don't
 * need to copy it of course.)
 */

#include "dr_api.h"
#include "drsyms.h"
#include <assert.h>
#include <imagehlp.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef UNIX
#    define EXPORT
#else
#    define EXPORT __declspec(dllexport)
#endif

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf) (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

/* global params */
static bool expect_int2e = false;
static bool expect_sysenter = false;
static bool expect_wow = false;
static bool expect_x64 = false;
static bool verbose = false;
static bool list_exports = false;
static bool list_forwards = false;
static bool list_Ki = false;
static bool list_syscalls = false;
static bool list_usercalls = false; /* NtUserCall* */
static bool usercall_imports = false;
static bool ignore_Zw = false;

static const char *const usercall_names[] = {
    "NtUserCallNoParam",       "NtUserCallOneParam",  "NtUserCallHwnd",
    "NtUserCallHwndOpt",       "NtUserCallHwndParam", "NtUserCallHwndLock",
    "NtUserCallHwndParamLock", "NtUserCallTwoParam",
};
/* To handle win10-1607 we have to look for imports from win32u.dll.
 * But, for 32-bit, NoParam instead calls to a local routine that invokes yet
 * another routine that finally does the import.
 */
static const char *const usercall_imp_names[] = {
    "_imp__NtUserCallNoParam", /* For 32-bit we use ALT_NOPARAM */
    /* XXX: x64 win10-1607 is failing to find _imp__NtUserCallOneParam.
     * I bailed on further investigation as we assume the numbers are
     * the same across bitwidths.
     */
    "_imp__NtUserCallOneParam", "_imp__NtUserCallHwnd", "_imp__NtUserCallHwndOpt",
    "_imp__NtUserCallHwndParam", "_imp__NtUserCallHwndLock",
    "_imp__NtUserCallHwndParamLock",
    "_imp__NtUserCallTwoParam", /* For 32-bit we use ALT_TWOPARAM */
};
#define ALT_NOPARAM "Local_NtUserCallNoParam"
#define ALT_TWOPARAM "Local_NtUserCallTwoParam"
#define NUM_USERCALL (sizeof(usercall_names) / sizeof(usercall_names[0]))
static byte *usercall_addr[NUM_USERCALL];

static void
common_print(char *fmt, va_list ap)
{
    vfprintf(stdout, fmt, ap);
    fflush(stdout);
}

static void
verbose_print(char *fmt, ...)
{
    if (verbose) {
        va_list ap;
        va_start(ap, fmt);
        common_print(fmt, ap);
        va_end(ap);
    }
}

static void
print(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    common_print(fmt, ap);
    va_end(ap);
}

/* We expect the win8 x86 sysenter adjacent "inlined" callee to be as simple as
 *     75caeabc 8bd4        mov     edx,esp
 *     75caeabe 0f34        sysenter
 *     75caeac0 c3          ret
 */
#define MAX_INSTRS_SYSENTER_CALLEE 4
/* the max distance from call to the sysenter callee target */
#define MAX_SYSENTER_CALLEE_OFFSET 0x50
#define MAX_INSTRS_BEFORE_SYSCALL 16
#define MAX_INSTRS_IN_FUNCTION 256

/* For searching for usercalls we'll go quite a ways */
#define MAX_BYTES_BEFORE_USERCALL 0x300
#define MAX_SYMBOL_LEN 256

typedef struct {
    int sysnum;
    int num_args;
    int fixup_index; /* WOW dlls only */
} syscall_info_t;

static byte *
get_preferred_base(LOADED_IMAGE *img)
{
    IMAGE_NT_HEADERS *nt;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img->MappedAddress;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return (byte *)(ptr_uint_t)((IMAGE_OPTIONAL_HEADER32 *)&nt->OptionalHeader)
            ->ImageBase;
    } else {
        return (byte *)(ptr_uint_t)((IMAGE_OPTIONAL_HEADER64 *)&nt->OptionalHeader)
            ->ImageBase;
    }
}

/* returns false on failure */
static bool
decode_function(void *dcontext, byte *entry)
{
    byte *pc, *pre_pc;
    int num_instr = 0;
    bool found_ret = false;
    instr_t *instr;
    if (entry == NULL)
        return false;
    instr = instr_create(dcontext);
    pc = entry;
    while (true) {
        instr_reset(dcontext, instr);
        pre_pc = pc;
        pc = decode(dcontext, pc, instr);
        instr_set_translation(instr, pre_pc);
        dr_print_instr(dcontext, STDOUT, instr, "");
        if (instr_is_return(instr)) {
            found_ret = true;
            break;
        }
        num_instr++;
        if (num_instr > MAX_INSTRS_IN_FUNCTION) {
            print("ERROR: hit max instr limit %d\n", MAX_INSTRS_IN_FUNCTION);
            break;
        }
    }
    instr_destroy(dcontext, instr);
    return found_ret;
}

static void
check_Ki(const char *name)
{
    /* FIXME: eventually we should automatically analyze these, but
     * not worth the time at this point.  Once we have automatic
     * analysis code, should put it into DR debug build init too!  For
     * now we issue manual instructions about verifying our
     * assumptions, and look for unknown Ki routines.
     */
    if (strcmp(name, "KiUserApcDispatcher") == 0) {
        print("verify that:\n"
              "\t1) *esp == call* target (not relied on)\n"
              "\t2) *(esp+16) == CONTEXT\n");
    } else if (strcmp(name, "KiUserExceptionDispatcher") == 0) {
        print("verify that:\n"
              "\t1) *esp = EXCEPTION_RECORD*\n"
              "\t2) *(esp+4) == CONTEXT*\n");
    } else if (strcmp(name, "KiRaiseUserExceptionDispatcher") == 0) {
        print("we've never seen this guy invoked\n");
    } else if (strcmp(name, "KiUserCallbackDispatcher") == 0) {
        print("verify that:\n"
              "\t1) peb->KernelCallbackTable[*(esp+4)] == call* target "
              "(not relied on)\n");
    } else if (strcmp(name, "KiFastSystemCall") == 0) {
        print("should be simply \"mov esp,edx; sysenter; ret\"\n");
    } else if (strcmp(name, "KiFastSystemCallRet") == 0) {
        print("should be simply \"ret\"\n");
    } else if (strcmp(name, "KiIntSystemCall") == 0) {
        print("should be simply \"lea 0x8(esp),edx; int 2e; ret\"\n");
    } else {
        print("WARNING!  UNKNOWN Ki ROUTINE!\n");
    }
}

static void
process_ret(instr_t *instr, syscall_info_t *info)
{
    assert(instr_is_return(instr));
    if (opnd_is_immed_int(instr_get_src(instr, 0)))
        info->num_args = (int)opnd_get_immed_int(instr_get_src(instr, 0));
    else
        info->num_args = 0;
}

/* returns whether found a syscall
 * - found_eax: whether the caller has seen "mov imm => %eax"
 * - found_edx: whether the caller has seen "mov $0x7ffe0300 => %edx",
 *              xref the comment below about "mov $0x7ffe0300 => %edx".
 */
static bool
process_syscall_instr(void *dcontext, instr_t *instr, bool found_eax, bool found_edx)
{
    /* ASSUMPTION: a mov imm of 0x7ffe0300 into edx followed by an
     * indirect call via edx is a system call on XP and later
     * On XP SP1 it's call *edx, while on XP SP2 it's call *(edx)
     * For wow it's a call through fs.
     * FIXME - core exports various is_*_syscall routines (such as
     * instr_is_wow64_syscall()) which we could use here instead of
     * duplicating if they were more flexible about when they could
     * be called (instr_is_wow64_syscall() for ex. asserts if not
     * in a wow process).
     */
    if (/* int 2e or x64 or win8 sysenter */
        (instr_is_syscall(instr) && found_eax &&
         (expect_int2e || expect_x64 || expect_sysenter)) ||
        /* sysenter case */
        (expect_sysenter && found_edx && found_eax && instr_is_call_indirect(instr) &&
         /* XP SP{0,1}, 2003 SP0: call *edx */
         ((opnd_is_reg(instr_get_target(instr)) &&
           opnd_get_reg(instr_get_target(instr)) == REG_EDX) ||
          /* XP SP2, 2003 SP1: call *(edx) */
          (opnd_is_base_disp(instr_get_target(instr)) &&
           opnd_get_base(instr_get_target(instr)) == REG_EDX &&
           opnd_get_index(instr_get_target(instr)) == REG_NULL &&
           opnd_get_disp(instr_get_target(instr)) == 0))) ||
        /* wow case
         * we don't require found_ecx b/c win8 does not use ecx
         */
        (expect_wow && found_eax && instr_is_call_indirect(instr) &&
         ((opnd_is_far_base_disp(instr_get_target(instr)) &&
           opnd_get_base(instr_get_target(instr)) == REG_NULL &&
           opnd_get_index(instr_get_target(instr)) == REG_NULL &&
           opnd_get_segment(instr_get_target(instr)) == SEG_FS) ||
          /* win10 has imm in edx and a near call */
          found_edx)))
        return true;
    return false;
}

/* returns whether found a syscall
 * - found_eax: whether the caller has seen "mov imm => %eax"
 * - found_edx: whether the caller has seen "mov $0x7ffe0300 => %edx",
 *              xref the comment in process_syscall_instr.
 */
static bool
process_syscall_call(void *dcontext, byte *next_pc, instr_t *call, bool found_eax,
                     bool found_edx)
{
    int num_instr;
    byte *pc;
    instr_t instr;
    bool found_syscall = false;

    assert(instr_get_opcode(call) == OP_call && opnd_is_pc(instr_get_target(call)));
    pc = opnd_get_pc(instr_get_target(call));
    if (pc > next_pc + MAX_SYSENTER_CALLEE_OFFSET ||
        pc <= next_pc /* assuming the call won't go backward */)
        return false;
    /* handle win8 x86 which has sysenter callee adjacent-"inlined"
     *     ntdll!NtYieldExecution:
     *     77d7422c b801000000  mov     eax,1
     *     77d74231 e801000000  call    ntdll!NtYieldExecution+0xb (77d74237)
     *     77d74236 c3          ret
     *     77d74237 8bd4        mov     edx,esp
     *     77d74239 0f34        sysenter
     *     77d7423b c3          ret
     *
     * or DrMem-i#1366-c#2
     *     USER32!NtUserCreateWindowStation:
     *     75caea7a b841110000  mov     eax,0x1141
     *     75caea7f e838000000  call    user32!...+0xd (75caeabc)
     *     75caea84 c22000      ret     0x20
     *     ...
     *     USER32!GetWindowStationName:
     *     75caea8c 8bff        mov     edi,edi
     *     75caea8e 55          push    ebp
     *     ...
     *     75caeabc 8bd4        mov     edx,esp
     *     75caeabe 0f34        sysenter
     *     75caeac0 c3          ret
     */
    /* We expect the win8 x86 sysenter adjacent "inlined" callee to be as simple as
     *     75caeabc 8bd4        mov     edx,esp
     *     75caeabe 0f34        sysenter
     *     75caeac0 c3          ret
     */
    instr_init(dcontext, &instr);
    num_instr = 0;
    do {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        if (verbose)
            dr_print_instr(dcontext, STDOUT, &instr, "");
        if (pc == NULL || !instr_valid(&instr))
            break;
        if (instr_is_syscall(&instr) || instr_is_call_indirect(&instr)) {
            found_syscall = process_syscall_instr(dcontext, &instr, found_eax, found_edx);
            break;
        } else if (instr_is_cti(&instr)) {
            break;
        }
        num_instr++;
    } while (num_instr <= MAX_INSTRS_SYSENTER_CALLEE);
    instr_free(dcontext, &instr);
    return found_syscall;
}

/* returns false on failure */
static bool
decode_syscall_num(void *dcontext, byte *entry, syscall_info_t *info, LOADED_IMAGE *img)
{
    /* FIXME: would like to fail gracefully rather than have a DR assertion
     * on non-code! => use DEBUG=0 INTERNAL=1 DR build!
     */
    bool found_syscall = false, found_eax = false, found_edx = false, found_ecx = false;
    bool found_ret = false;
    byte *pc, *pre_pc;
    int num_instr = 0;
    instr_t *instr;
    byte *preferred = get_preferred_base(img);
    if (entry == NULL)
        return false;
    info->num_args = -1; /* if find sysnum but not args */
    info->sysnum = -1;
    info->fixup_index = -1;
    instr = instr_create(dcontext);
    pc = entry;
    /* FIXME - we don't support decoding 64bit instructions in 32bit mode, but I want
     * this to work on 32bit machines.  Hack fix based on the wrapper pattern, we skip
     * the first instruction (mov r10, rcx) here, the rest should decode ok.
     * Xref PR 236203. */
    if (expect_x64 && *pc == 0x4c && *(pc + 1) == 0x8b && *(pc + 2) == 0xd1)
        pc += 3;
    while (true) {
        instr_reset(dcontext, instr);
        pre_pc = pc;
        pc = decode(dcontext, pc, instr);
        if (verbose) {
            instr_set_translation(instr, pre_pc);
            dr_print_instr(dcontext, STDOUT, instr, "");
        }
        if (pc == NULL || !instr_valid(instr))
            break;
        if (instr_is_syscall(instr) || instr_is_call_indirect(instr)) {
            /* If we see a syscall instr or an indirect call which is not syscall,
             * we assume this is not a syscall wrapper.
             */
            found_syscall = process_syscall_instr(dcontext, instr, found_eax, found_edx);
            if (!found_syscall)
                break; /* assume not a syscall wrapper, give up gracefully */
        } else if (instr_is_return(instr)) {
            /* we must break on return to avoid case like win8 x86
             * which has sysenter callee adjacent-"inlined"
             *     ntdll!NtYieldExecution:
             *     77d7422c b801000000  mov     eax,1
             *     77d74231 e801000000  call    ntdll!NtYieldExecution+0xb (77d74237)
             *     77d74236 c3          ret
             *     77d74237 8bd4        mov     edx,esp
             *     77d74239 0f34        sysenter
             *     77d7423b c3          ret
             */
            if (!found_ret) {
                process_ret(instr, info);
                found_ret = true;
            }
            break;
        } else if (instr_get_opcode(instr) == OP_call) {
            found_syscall =
                process_syscall_call(dcontext, pc, instr, found_eax, found_edx);
            /* If we see a call and it is not a sysenter callee,
             * we assume this is not a syscall wrapper.
             */
            if (!found_syscall)
                break; /* assume not a syscall wrapper, give up gracefully */
        } else if (instr_is_cti(instr)) {
            /* We expect only ctis like ret or ret imm, syscall, and call, which are
             * handled above. Give up gracefully if we hit any other cti.
             * XXX: what about jmp to shared ret (seen in the past on some syscalls)?
             */
            /* Update: win10 TH2 1511 x64 has a cti:
             *   ntdll!NtContinue:
             *   00007ff9`13185630 4c8bd1          mov     r10,rcx
             *   00007ff9`13185633 b843000000      mov     eax,43h
             *   00007ff9`13185638 f604250803fe7f01 test byte ptr [SharedUserData+0x308
             *                                                     (00000000`7ffe0308)],1
             *   00007ff9`13185640 7503            jne     ntdll!NtContinue+0x15
             *                                             (00007ff9`13185645)
             *   00007ff9`13185642 0f05            syscall
             *   00007ff9`13185644 c3              ret
             *   00007ff9`13185645 cd2e            int     2Eh
             *   00007ff9`13185647 c3              ret
             */
            if (expect_x64 && instr_is_cbr(instr) &&
                opnd_get_pc(instr_get_target(instr)) == pc + 3 /*syscall;ret*/) {
                /* keep going */
            } else
                break;
        } else if ((!found_eax || !found_edx || !found_ecx) &&
                   instr_get_opcode(instr) == OP_mov_imm &&
                   opnd_is_reg(instr_get_dst(instr, 0))) {
            if (!found_eax && opnd_get_reg(instr_get_dst(instr, 0)) == REG_EAX) {
                info->sysnum = (int)opnd_get_immed_int(instr_get_src(instr, 0));
                found_eax = true;
            } else if (!found_edx && opnd_get_reg(instr_get_dst(instr, 0)) == REG_EDX) {
                uint imm = (uint)opnd_get_immed_int(instr_get_src(instr, 0));
                if (imm == 0x7ffe0300 ||
                    /* On Win10 the immed is ntdll!Wow64SystemServiceCall */
                    (expect_wow && imm > (ptr_uint_t)preferred &&
                     imm < (ptr_uint_t)preferred + img->SizeOfImage))
                    found_edx = true;
            } else if (!found_ecx && opnd_get_reg(instr_get_dst(instr, 0)) == REG_ECX) {
                found_ecx = true;
                info->fixup_index = (int)opnd_get_immed_int(instr_get_src(instr, 0));
            }
        } else if (instr_get_opcode(instr) == OP_xor &&
                   opnd_is_reg(instr_get_src(instr, 0)) &&
                   opnd_get_reg(instr_get_src(instr, 0)) == REG_ECX &&
                   opnd_is_reg(instr_get_dst(instr, 0)) &&
                   opnd_get_reg(instr_get_dst(instr, 0)) == REG_ECX) {
            /* xor to 0 */
            found_ecx = true;
            info->fixup_index = 0;
        }
        num_instr++;
        if (num_instr > MAX_INSTRS_BEFORE_SYSCALL) /* wrappers should be short! */
            break; /* avoid weird cases like NPXEMULATORTABLE */
    }
    instr_destroy(dcontext, instr);
    return found_syscall;
}

static void
process_syscall_wrapper(void *dcontext, byte *addr, const char *string, const char *type,
                        LOADED_IMAGE *img)
{
    syscall_info_t sysinfo;
    if (ignore_Zw && string[0] == 'Z' && string[1] == 'w')
        return;
    if (decode_syscall_num(dcontext, addr, &sysinfo, img)) {
        if (sysinfo.sysnum == -1) {
            /* we expect this sometimes */
            if (strcmp(string, "KiFastSystemCall") != 0 &&
                strcmp(string, "KiIntSystemCall") != 0) {
                print("ERROR: unknown syscall #: %s\n", string);
            }
        } else {
            /* be sure to print all digits b/c win8 now uses the top 16 bits for wow64 */
            if (expect_wow) {
                print("syscall # 0x%08x %-6s %2d args fixup 0x%02x = %s\n",
                      sysinfo.sysnum, type, sysinfo.num_args, sysinfo.fixup_index,
                      string);
            } else if (expect_x64) {
                print("syscall # 0x%08x %-6s = %s\n", sysinfo.sysnum, type, string);
            } else {
                print("syscall # 0x%08x %-6s %2d args = %s\n", sysinfo.sysnum, type,
                      sysinfo.num_args, string);
            }
        }
    }
}

static void
look_for_usercall(void *dcontext, byte *entry, const char *sym, LOADED_IMAGE *img,
                  const char *modpath)
{
    bool found_push_imm = false;
    int imm = 0;
    app_pc pc, pre_pc;
    instr_t *instr;
    if (entry == NULL)
        return;
    instr = instr_create(dcontext);
    pc = entry;
    while (true) {
        instr_reset(dcontext, instr);
        pre_pc = pc;
        pc = decode(dcontext, pc, instr);
        if (verbose) {
            instr_set_translation(instr, pre_pc);
            dr_print_instr(dcontext, STDOUT, instr, "");
        }
        if (pc == NULL || !instr_valid(instr))
            break;
        /* If there are multiple push-immeds we want the outer one
         * as the code is the last param.
         */
        if (!found_push_imm && instr_get_opcode(instr) == OP_push_imm) {
            found_push_imm = true;
            imm = (int)opnd_get_immed_int(instr_get_src(instr, 0));
        } else if (instr_is_call_direct(instr) && found_push_imm) {
            /* We don't rule out usercall_imports due to Local_NtUserCallNoParam */
            app_pc tgt = opnd_get_pc(instr_get_target(instr));
            bool found = false;
            int i;
            for (i = 0; i < NUM_USERCALL; i++) {
                if (tgt == usercall_addr[i]) {
                    dr_printf("Call #0x%02x to %s at %s+0x%x\n", imm, usercall_names[i],
                              sym, pre_pc - entry);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            found_push_imm = false;
        } else if (usercall_imports && instr_is_call_indirect(instr) && found_push_imm &&
                   opnd_is_abs_addr(instr_get_target(instr))) {
            app_pc tgt = opnd_get_addr(instr_get_target(instr));
            bool found = false;
            int i;
            for (i = 0; i < NUM_USERCALL; i++) {
                if (tgt == usercall_addr[i]) {
                    dr_printf("Call #0x%02x to %s at %s+0x%x\n", imm, usercall_names[i],
                              sym, pre_pc - entry);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            found_push_imm = false;
        } else if (instr_is_return(instr))
            break;
        else if (instr_is_call(instr))
            found_push_imm = false;
        if (pc - entry > MAX_BYTES_BEFORE_USERCALL)
            break;
    }
    instr_destroy(dcontext, instr);
}

typedef struct _search_data_t {
    void *dcontext;
    LOADED_IMAGE *img;
    const char *modpath;
} search_data_t;

/* not only do we have NtUser*, NtWow64*, etc., but also user32!UserContectToServer,
 * so we go through all symbols
 */
#define SYM_PATTERN "*"

static bool
search_syms_cb(const char *name, size_t modoffs, void *data)
{
    search_data_t *sd = (search_data_t *)data;
    byte *addr =
        ImageRvaToVa(sd->img->FileHeader, sd->img->MappedAddress, (ULONG)modoffs, NULL);
    verbose_print("Found symbol \"%s\" at offs " PIFX " => " PFX "\n", name, modoffs,
                  addr);
    if (list_usercalls)
        look_for_usercall(sd->dcontext, addr, name, sd->img, sd->modpath);
    else
        process_syscall_wrapper(sd->dcontext, addr, name, "pdb", sd->img);
    return true; /* keep iterating */
}

static void
process_symbols(void *dcontext, char *dllname, LOADED_IMAGE *img)
{
    /* We have to specify the module via "modname!symname".
     * We must use the same modname as in full_path.
     */
    char fullpath[MAX_PATH];
#define MAX_SYM_WITH_MOD_LEN 256
    char sym_with_mod[MAX_SYM_WITH_MOD_LEN];
    int len;
    drsym_error_t symres;
    char *fname = NULL, *c;
    search_data_t sd;

    if (drsym_init(NULL) != DRSYM_SUCCESS) {
        print("WARNING: unable to initialize symbol engine\n");
        return;
    }

    if (dllname == NULL)
        return;
    fname = dllname;
    for (c = dllname; *c != '\0'; c++) {
        if (*c == '/' || *c == '\\')
            fname = c + 1;
    }
    assert(fname != NULL && "unable to get fname for module");
    if (fname == NULL)
        return;
    /* now get rid of extension */
    for (; c > fname && *c != '.'; c--)
        ; /* nothing */

    assert(c > fname && "file has no extension");
    assert(c - fname < BUFFER_SIZE_ELEMENTS(sym_with_mod) && "sizes way off");
    len = dr_snprintf(sym_with_mod, BUFFER_SIZE_ELEMENTS(sym_with_mod), "%.*s!%s",
                      c - fname, fname, SYM_PATTERN);
    assert(len > 0 && "error printing modname!symname");
    NULL_TERMINATE_BUFFER(sym_with_mod);

    len = GetFullPathName(dllname, BUFFER_SIZE_ELEMENTS(fullpath), fullpath, NULL);
    assert(len > 0);
    NULL_TERMINATE_BUFFER(dllname);

    if (list_usercalls) {
        int i;
        byte *preferred = get_preferred_base(img);
        for (i = 0; i < NUM_USERCALL; i++) {
            size_t offs;
            /* We have to look for the __imp first, b/c win10-1607 does have
             * a NoParam wrapper.
             */
            const char *imp_name = usercall_imp_names[i];
            if (i == 0 && !expect_x64)
                imp_name = ALT_NOPARAM;
            else if (i == NUM_USERCALL - 1 && !expect_x64)
                imp_name = ALT_TWOPARAM;
            symres = drsym_lookup_symbol(fullpath, imp_name, &offs, 0);
            if (symres == DRSYM_SUCCESS) {
                usercall_imports = true;
                if (strstr(imp_name, "_imp__") == imp_name) {
                    /* Not relocated, so use preferred */
                    usercall_addr[i] = preferred + (ULONG)offs;
                } else {
                    usercall_addr[i] = ImageRvaToVa(img->FileHeader, img->MappedAddress,
                                                    (ULONG)offs, NULL);
                }
                verbose_print("%s = %d +0x%x == " PFX "\n", imp_name, symres, offs,
                              usercall_addr[i]);
            } else {
                symres = drsym_lookup_symbol(fullpath, usercall_names[i], &offs, 0);
                if (symres == DRSYM_SUCCESS) {
                    usercall_addr[i] = ImageRvaToVa(img->FileHeader, img->MappedAddress,
                                                    (ULONG)offs, NULL);
                    verbose_print("%s = %d +0x%x == " PFX "\n", usercall_names[i], symres,
                                  offs, usercall_addr[i]);
                } else {
                    dr_printf("Error locating usercall %s: aborting\n",
                              usercall_names[i]);
                    return;
                }
            }
        }
    }

    sd.dcontext = dcontext;
    sd.img = img;
    sd.modpath = fullpath;
    verbose_print("Searching \"%s\" for \"%s\"\n", fullpath, sym_with_mod);
    symres = drsym_search_symbols(fullpath, sym_with_mod, true, search_syms_cb, &sd);
    if (symres != DRSYM_SUCCESS)
        print("Error %d searching \"%s\" for \"%s\"\n", symres, fullpath, sym_with_mod);
    drsym_exit();
}

static void
process_exports(void *dcontext, char *dllname, LOADED_IMAGE *img)
{
    IMAGE_EXPORT_DIRECTORY *dir;
    IMAGE_SECTION_HEADER *sec;
    DWORD *name, *code;
    WORD *ordinal;
    const char *string;
    ULONG size;
    uint i;
    byte *addr, *start_exports, *end_exports;

    verbose_print("Processing exports of \"%s\"\n", dllname);
    dir = (IMAGE_EXPORT_DIRECTORY *)ImageDirectoryEntryToData(
        img->MappedAddress, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size);
    verbose_print("mapped at " PFX " (preferred " PFX "), exports 0x%08x, size 0x%x\n",
                  img->MappedAddress, get_preferred_base(img), dir, size);
    start_exports = (byte *)dir;
    end_exports = start_exports + size;
    verbose_print(
        "name=%s, ord base=0x%08x, names=%d 0x%08x\n",
        (char *)ImageRvaToVa(img->FileHeader, img->MappedAddress, dir->Name, NULL),
        dir->Base, dir->NumberOfNames, dir->AddressOfNames);

    /* don't limit functions to lie in .text --
     * for ntdll, some exported routines have their code after .text, inside
     * ECODE section!
     */
    sec = img->Sections;
    for (i = 0; i < img->NumberOfSections; i++) {
        verbose_print(
            "Section %d %s: 0x%x + 0x%x == 0x%08x through 0x%08x\n", i, sec->Name,
            sec->VirtualAddress, sec->SizeOfRawData,
            ImageRvaToVa(img->FileHeader, img->MappedAddress, sec->VirtualAddress, NULL),
            (ptr_uint_t)ImageRvaToVa(img->FileHeader, img->MappedAddress,
                                     sec->VirtualAddress, NULL) +
                sec->SizeOfRawData);
        sec++;
    }

    name = (DWORD *)ImageRvaToVa(img->FileHeader, img->MappedAddress, dir->AddressOfNames,
                                 NULL);
    code = (DWORD *)ImageRvaToVa(img->FileHeader, img->MappedAddress,
                                 dir->AddressOfFunctions, NULL);
    ordinal = (WORD *)ImageRvaToVa(img->FileHeader, img->MappedAddress,
                                   dir->AddressOfNameOrdinals, NULL);
    verbose_print("names: from 0x%08x to 0x%08x\n",
                  ImageRvaToVa(img->FileHeader, img->MappedAddress, name[0], NULL),
                  ImageRvaToVa(img->FileHeader, img->MappedAddress,
                               name[dir->NumberOfNames - 1], NULL));

    for (i = 0; i < dir->NumberOfNames; i++) {
        string = (char *)ImageRvaToVa(img->FileHeader, img->MappedAddress, name[i], NULL);
        /* ordinal is biased (dir->Base), but don't add base when using as index */
        assert(dir->NumberOfFunctions > ordinal[i]);
        /* I don't understand why have to do RVA to VA here, when dumpbin /exports
         * seems to give the same offsets but by simply adding them to base we
         * get the appropriate code location -- but that doesn't work here...
         */
        addr = ImageRvaToVa(img->FileHeader, img->MappedAddress, code[ordinal[i]], NULL);
        verbose_print("name=%s 0x%08x, ord=%d, code=0x%x -> 0x%08x\n", string, string,
                      ordinal[i], code[ordinal[i]], addr);
        if (list_exports) {
            print("ord %3d offs 0x%08x %s\n", ordinal[i], addr - img->MappedAddress,
                  string);
        }
        if (list_Ki && string[0] == 'K' && string[1] == 'i') {
            print("\n==================================================\n");
            print("%s\n\n", string);
            check_Ki(string);
            print("\ndisassembly:\n");
            decode_function(dcontext, addr);
            print("==================================================\n");
        }
        /* forwarded export points inside exports section */
        if (addr >= start_exports && addr < end_exports) {
            if (list_forwards || verbose) {
                /* I've had issues w/ forwards before, so avoid printing crap */
                if (addr[0] > 0 && addr[0] < 127)
                    print("%s is forwarded to %.128s\n", string, addr);
                else
                    print("ERROR identifying forwarded entry for %s\n", string);
            }
        } else if (list_syscalls) {
            process_syscall_wrapper(dcontext, addr, string, "export", img);
        }
    }
}

static void
load_and_analyze(void *dcontext, char *dllname)
{
    LOADED_IMAGE img;
    BOOL res;

    res = MapAndLoad(dllname, NULL, &img, FALSE, TRUE);
    if (!res) {
        print("Error loading %s\n", dllname);
        return;
    }
    verbose_print("mapped at " PFX " (preferred " PFX ")\n", img.MappedAddress,
                  get_preferred_base(&img));
    if (!list_usercalls)
        process_exports(dcontext, dllname, &img);
    if (list_syscalls || list_usercalls)
        process_symbols(dcontext, dllname, &img);
    UnMapAndLoad(&img);
}

static void
usage(char *pgm)
{
    print("Usage: %s [-syscalls <-sysenter | -int2e | -wow | -x64> [-ignore_Zw]] | "
          "-Ki | -exports | -forwards | -usercalls [-x64] | -v] <dll>\n",
          pgm);
    exit(-1);
}

int
main(int argc, char *argv[])
{
    void *dcontext = dr_standalone_init();
    int res;
    char *dll;
    bool forced = false;

    dr_set_isa_mode(dcontext, DR_ISA_IA32, NULL);

    for (res = 1; res < argc; res++) {
        if (strcmp(argv[res], "-sysenter") == 0) {
            expect_sysenter = true;
            forced = true;
        } else if (strcmp(argv[res], "-int2e") == 0) {
            expect_int2e = true;
            forced = true;
        } else if (strcmp(argv[res], "-wow") == 0) {
            expect_wow = true;
            forced = true;
        } else if (strcmp(argv[res], "-x64") == 0) {
            expect_x64 = true;
#ifdef X64
            dr_set_isa_mode(dcontext, DR_ISA_AMD64, NULL);
#else
            /* For 32-bit builds we hack a fix for -syscalls (see
             * decode_syscall_num()) but -Ki won't work.
             */
#endif
            forced = true;
        } else if (strcmp(argv[res], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[res], "-exports") == 0) {
            list_exports = true;
            list_forwards = true; /* implied */
        } else if (strcmp(argv[res], "-forwards") == 0) {
            list_forwards = true;
        } else if (strcmp(argv[res], "-Ki") == 0) {
            list_Ki = true;
        } else if (strcmp(argv[res], "-syscalls") == 0) {
            list_syscalls = true;
        } else if (strcmp(argv[res], "-ignore_Zw") == 0) {
            ignore_Zw = true;
        } else if (strcmp(argv[res], "-usercalls") == 0) {
            list_usercalls = true;
        } else if (argv[res][0] == '-') {
            usage(argv[0]);
        } else {
            break;
        }
    }
    if (res >= argc ||
        (!list_syscalls && !list_Ki && !list_forwards && !verbose && !list_usercalls)) {
        usage(argv[0]);
    }
    dll = argv[res];

    if (!forced && list_syscalls) {
        usage(argv[0]);
    }

    load_and_analyze(dcontext, dll);
    dr_standalone_exit();
    return 0;
}
