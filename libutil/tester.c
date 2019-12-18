/* **********************************************************
 * Copyright (c) 2005-2007 VMware, Inc.  All rights reserved.
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

#include <stdio.h>

#include "share.h"
#include "utils.h"
#include "hotp-test.h"

#define OUTFILE L"tester.out"

#define ATTACK_NONE 0
#define ATTACK_STACK 1
#define ATTACK_HEAP 2

int attack_status = ATTACK_NONE;

DWORD
WINAPI
heap_attack(LPVOID param);

DWORD
WINAPI
stack_attack(LPVOID param);

DWORD
WINAPI
ls_attack(LPVOID param);

enum { stack = 1, heap, liveshield };

/* disable type case warning for attacks below. */
#pragma warning(disable : 4055)

void
usage()
{
    fprintf(
        stderr,
        " usage: tester [initial_sleep_ms] [attack] [num_attacks]\n"
        "\n"
        " tester will then do:\n"
        "  (1) [initial_sleep_ms] sleep before doing anything\n"
        "  (2) if [attack]/[num_attacks] params are not set, exits\n"
        "  (3) [attack=1,2 or 3] (executes 1=stack, 2=heap or 3=liveshield attack \n"
        "      (1) liveshield attacks execute test_reg and test_control_flow hotpatches\n"
        "          (equivalent of araktest liveshield buttons 1 and 2)\n"
        "      (2) write to \"tester.out.xx\" two characters:\n"
        "          -- 0 or 1 according to whether test_reg was patched\n"
        "          -- 0 or 1 according to whether test_control_flow was patched\n"
        "  (4) repeat step (3) [num_attacks] times\n"
        "\n"
        " To run under DR setup appropriate registry settings for tester.exe\n");
}

int
main(int argc, char **argv)
{
    int timeout = 5000;
    /*char output[MAX_PATH];*/
    if (argc == 2 && (_stricmp(argv[1], "-h") == 0 || _stricmp(argv[1], "-help") == 0)) {
        usage();
        exit(0);
    }

    if (argc >= 2)
        timeout = atoi(argv[1]);

    Sleep(timeout);

    if (argc >= 3) {
        int i, count = 1;
        int attack = atoi(argv[2]);
        if (attack == 0) {
            fprintf(stderr, "attack=%d not expected\n", attack);
            usage();
            exit(-1);
        }

        if (argc >= 4)
            count = atoi(argv[3]);

        for (i = 0; i < count; i++) {
            HANDLE athr;
            fprintf(stderr, "loop %d\n", i);
            if (attack == 1)
                athr = CreateThread(NULL, 0, stack_attack, NULL, 0, NULL);
            else if (attack == 2)
                athr = CreateThread(NULL, 0, heap_attack, NULL, 0, NULL);
            else if (attack == 3)
                athr = CreateThread(NULL, 0, ls_attack, (void *)i, 0, NULL);
            else {
                fprintf(stderr, "attack=%d can take 1,2 or 3\n", attack);
                usage();
                exit(-1);
            }

            if (athr == NULL) {
                fprintf(stderr, "tc %d\n", GetLastError());
                exit(-1);
            }
            WaitForSingleObject(athr, INFINITE);
            CloseHandle(athr);
        }
    }

    return 0;
}

/*
123:
124:  void test(void(*f)(int),  int i)
125:
126:  {
004012A0 55                   push        ebp
004012A1 8B EC                mov         ebp,esp
004012A3 83 EC 40             sub         esp,40h
004012A6 53                   push        ebx
004012A7 56                   push        esi
004012A8 57                   push        edi
004012A9 8D 7D C0             lea         edi,[ebp-40h]
004012AC B9 10 00 00 00       mov         ecx,10h
004012B1 B8 CC CC CC CC       mov         eax,0CCCCCCCCh
004012B6 F3 AB                rep stos    dword ptr [edi] ****GET RID OF THIS CRAP ****
127:      (f)(i);
004012B8 8B F4                mov         esi,esp
004012BA 8B 45 0C             mov         eax,dword ptr [ebp+0Ch]
004012BD 50                   push        eax
004012BE FF 55 08             call        dword ptr [ebp+8]
004012C1 83 C4 04             add         esp,4
004012C4 3B F4                cmp         esi,esp
004012C6 E8 55 10 00 00       call        _chkesp (00402320) ***GET RID OF THIS B'COZ ABS
ADDR **** 128:      return; 129:  } 004012CB 5F                   pop         edi 004012CC
5E                   pop         esi 004012CD 5B                   pop         ebx
004012CE 83 C4 40             add         esp,40h
004012D1 3B EC                cmp         ebp,esp
004012D3 E8 48 10 00 00       call        _chkesp (00402320) ***GET RID OF THIS B'COZ ABS
ADDR **** 004012D8 8B E5                mov         esp,ebp 004012DA 5D pop         ebp
004012DB C3                   ret


*/
unsigned char test_compiled_with_debug[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x40, 0x53, 0x56, 0x57, 0x8D, 0x7D, 0xC0, 0xB9, 0x10,
    0x00, 0x00, 0x00, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC,
    /* 0xF3, 0xAB, */
    0x8B, 0xF4, 0x8B, 0x45, 0x0C, 0x50, 0xFF, 0x55, 0x08, 0x83, 0xC4, 0x04, 0x3B, 0xF4,
    /* 0xE8, 0xC5, 0x0E, 0x00, 0x00, */
    0x5F, 0x5E, 0x5B, 0x83, 0xC4, 0x40, 0x3B, 0xEC,
    /* 0xE8, 0xB8, 0x0E, 0x00, 0x00, */
    0x8B, 0xE5, 0x5D, 0xC3, 0x00, 0x00, 0x00, 0x00
};

void
test(void (*f)(int), int i)
{
    (f)(i);
    return;
}

/*
123:
124:  void test(void(*f)(int),  int i)
125:
126:  {
004012A0 55                   push        ebp
004012A1 8B EC                mov         ebp,esp
127:      (f)(i);
004012BA 8B 45 0C             mov         eax,dword ptr [ebp+0Ch]
004012BD 50                   push        eax
004012BE FF 55 08             call        dword ptr [ebp+8]
128:      return;
129:  }
004012D8 8B E5                mov         esp,ebp
004012DA 5D                   pop         ebp
004012DB C3                   ret


*/
unsigned char sendfunc[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x50, 0xFF, 0x55,
                             0x08, 0x8B, 0xE5, 0x5D, 0xC3, 0x00, 0x00, 0x00, 0x00 };

void
set_attack(int i)
{
    attack_status = i;
}

void
fool_opt_compiler(unsigned char *foo)
{
    foo[0] = 1;
}

DWORD
WINAPI
stack_attack(LPVOID param)
{
    unsigned char myfunc[1024];
    int i;

    /* use the param to make compiler happy */
    if (param == NULL)
        i = 0;

    if (sizeof(sendfunc) >= sizeof(myfunc))
        MessageBox(NULL, L"ERROR", L"Memory allocation problem", MB_OK);

    for (i = 0; i < sizeof(sendfunc); i++)
        myfunc[i] = sendfunc[i];

    ((void (*)(void (*)(int), int))((void *)myfunc))(set_attack, ATTACK_STACK);
    fool_opt_compiler(myfunc);
    return ERROR_SUCCESS;
}

DWORD
WINAPI
heap_attack(LPVOID param)
{
    unsigned char *myfunc;
    int i;

    /* use the param to make compiler happy */
    if (param == NULL)
        i = 0;

    myfunc = (unsigned char *)malloc(sizeof(sendfunc));

    if (myfunc == NULL)
        MessageBox(NULL, L"ERROR", L"Memory allocation problem", MB_OK);

    for (i = 0; i < sizeof(sendfunc); i++)
        myfunc[i] = sendfunc[i];

    ((void (*)(void (*)(int), int))((void *)myfunc))(set_attack, ATTACK_HEAP);
    free(myfunc);
    return ERROR_SUCCESS;
}

DWORD
WINAPI
ls_attack(LPVOID param)
{
    char output[MAX_PATH];
    WCHAR filename[MAX_PATH];

    _snwprintf(filename, MAX_PATH, L"%s.%d", OUTFILE, (int)param);
    delete_file_rename_in_use(filename);

    _snprintf(output, MAX_PATH, "%d%d\n", hotp_test_reg(), hotp_test_control_flow());
    write_file_contents(filename, output, TRUE);

    return ERROR_SUCCESS;
}
