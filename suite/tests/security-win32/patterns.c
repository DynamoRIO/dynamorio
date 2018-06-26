/* **********************************************************
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <process.h> /* for _beginthreadex */

#include "tools.h" /* for print() */

/* patterns.c
 * test for case 7357: ensure code origins patterns disallow other code on
 * page and self-modifying and cross-modified patterns
 * - case 4020 scenarios:
       * other code on the page
       * same thread changes pattern
         this was incorrectly allowed before 4020 fix
       * another thread changes pattern, also allowed before 4020 fix
       * pattern code itself changes pattern, also allowed before 4020 and
         only fixed by 4020 due to luck
 * - -trampoline_displaced_code
 * - -trampoline_dirjmp
 */

/* some code in the data section */

/* case 6615: -trampoline_displaced_code:
00364f8d 8bff             mov     edi,edi
00364f8f 55               push    ebp
00364f90 8bec             mov     ebp,esp
00364f92 e9c5835b7c jmp ntdll!LdrQueryImageFileExecutionOptions+0x5 (7c91d35c)
*/
unsigned char datacode[] = "\x8b\xff"             /* mov     edi,edi */
                           "\x55"                 /* push    ebp */
                           "\x8b\xec"             /* mov     ebp,esp */
                           "\xe9\x00\x00\x00\x00" /* jmp     image_target+5 */
                           /* other code on the page */
                           "\x90"                 /* nop (avoid -trampoline_dirjmp) */
                           "\xe9\x00\x00\x00\x00" /* jmp     maliciousness */
    ;

/* pc after the 1st jmp */
#define DATACODE_POST_JMP ((ptr_int_t)datacode + sizeof(datacode) - 1 - 6)
/* array index of 1st jmp's operand */
#define DATACODE_JMP_OPND_IDX (sizeof(datacode) - 1 - 5 - 1 - 4)
/* pc after the 2nd jmp */
#define DATACODE_POST_2ND_JMP ((ptr_int_t)datacode + sizeof(datacode) - 1)
/* array index of 2nd jmp's operand */
#define DATACODE_2ND_JMP_OPND_IDX (sizeof(datacode) - 1 - 4)

/* another match for -trampoline_displaced_code but that is capable
 * of modifying itself.
 * example of mov immed to abs addr:
      0x24722535   c7 05 78 41 6d 24 b4 mov    $0x151b88b4 -> 0x246d4178
                   88 1b 15
*/
unsigned char datacode2[] =
    "\xc7\x05\x00\x00\x00\x00\x00\x00\x00\x00" /* mov $immed, abs-addr */
    "\xe9\x00\x00\x00\x00"                     /* jmp     image_target2+10 */
    ;

static __declspec(naked) void image_target()
{
    __asm {
            jmp  offset datacode
            pop  ebp
            ret
    }
}

static __declspec(naked) void image_target2()
{
    __asm {
            jmp  offset datacode2
            mov edi,edi
            mov edi,edi
            nop
            ret
    }
}

void
maliciousness()
{
    static int instances = 0;
    print("malicious code executing #%d!\n", ++instances);
}

int WINAPI
run_func(void *arg)
{
    int offs;
    /* have 2nd instr make direct jmp to maliciousness */
    offs = (ptr_int_t)&maliciousness -
        ((ptr_int_t)datacode + 2 /*first instr*/ + 5 /*this new jmp*/);
    datacode[2] = 0xe9;
    *((int *)(&datacode[3])) = offs;
    __asm {
            pusha
            call offset datacode
            popa
    }
}

int
main(int argc, char *argv[])
{
    int offs, tid;
    unsigned long hThread;

    INIT();

    print("testing hook pattern\n");
    /* make it executable so that natively it works on NX */
    protect_mem(datacode, sizeof(datacode), ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    protect_mem(datacode2, sizeof(datacode), ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    /* ensure our same-page test is relevant */
    assert((((ptr_int_t)datacode) & ~(PAGE_SIZE - 1)) ==
           (((ptr_int_t)&datacode[sizeof(datacode) - 1]) & ~(PAGE_SIZE - 1)));

    /****************************************************************************/
    /* datacode */

    /* we need to set the 1st jmp so we'll match the pattern */
    offs = ((ptr_int_t)&image_target + 5 /*skip jmp*/) - DATACODE_POST_JMP;
    /* make direct jmp go to image_target */
    *((int *)(&datacode[DATACODE_JMP_OPND_IDX])) = offs;
    __asm {
            pusha
            call offset datacode
            popa
    }

    print("testing non-pattern-match on same page\n");
    offs = (ptr_int_t)&maliciousness - DATACODE_POST_2ND_JMP;
    /* make 2nd direct jmp go to maliciousness */
    *((int *)(&datacode[DATACODE_2ND_JMP_OPND_IDX])) = offs;
    offs = DATACODE_POST_JMP;
    __asm {
            pusha
            call dword ptr offs
            popa
    }

    print("testing non-pattern-match in same region\n");
    /* have 2nd instr make direct jmp to maliciousness */
    offs = (ptr_int_t)&maliciousness -
        ((ptr_int_t)datacode + 2 /*first instr*/ + 5 /*this new jmp*/);
    datacode[2] = 0xe9;
    *((int *)(&datacode[3])) = offs;
    __asm {
            pusha
            call offset datacode
            popa
    }

    /* put the code back */
    datacode[2] = 0x55;
    datacode[3] = 0x8b;
    datacode[4] = 0xec;
    datacode[5] = 0xe9;
    offs = ((ptr_int_t)&image_target + 5 /*skip jmp*/) - DATACODE_POST_JMP;
    print("testing hook pattern again\n");
    /* make direct jmp go to image_target */
    *((int *)(&datacode[DATACODE_JMP_OPND_IDX])) = offs;
    __asm {
            pusha
            call offset datacode
            popa
    }

    print("testing non-pattern-match in same region by another thread\n");
    /* now have another thread do the same thing */
    hThread = _beginthreadex(NULL, 0, run_func, NULL, 0, &tid);
    WaitForSingleObject((HANDLE)hThread, INFINITE);

    print("testing different pattern match in same region\n");
    /* now change to have 1st instr make direct jmp to maliciousness */
    offs = (ptr_int_t)&maliciousness - ((ptr_int_t)datacode + 5);
    datacode[0] = 0xe9;
    *((int *)(&datacode[1])) = offs;
    __asm {
            pusha
            call offset datacode
            popa
    }

    /****************************************************************************/
    /* datacode2 */

    /* for -detect_mode we may have added datacode2 -- so force removal of it */
    protect_mem(datacode2, sizeof(datacode), ALLOW_READ | ALLOW_WRITE);
    /* but we have to make sure it works on nx */
    protect_mem(datacode2, sizeof(datacode), ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);

    print("testing pattern match that modifies itself to be a non-match\n");
    /* would be allowed w/ last_area 4020 impl but shared->private check deletes
     * shared area and we end up getting lucky.
     */
    /* MUST be just after change 1st instr of datacode to jmp to maliciousness */
    offs = ((ptr_int_t)&image_target2 + 10 /*skip length of pre-jmp instrs*/) -
        ((ptr_int_t)datacode2 + sizeof(datacode2) - 1);
    /* make direct jmp go to image_target */
    *((int *)(&datacode2[sizeof(datacode2) - 5])) = offs;
    /* make the mov modify the jmp to go to the jmp at the start of
     * datacode (I would put another jmp at end of datacode2 but we'll
     * just elide and allow!) which will go to maliciousness
     */
    offs = (ptr_int_t)(&datacode) - ((ptr_int_t)datacode2 + sizeof(datacode2) - 1);
    /* immed comes last */
    *((int *)(&datacode2[6])) = offs;
    /* target of mov comes before immed */
    *((int *)(&datacode2[2])) = (ptr_int_t)&datacode2[11];
    __asm {
            pusha
            call offset datacode2
            popa
    }

    print("finished\n");
    return 0;
}
