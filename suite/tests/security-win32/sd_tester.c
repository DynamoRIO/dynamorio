/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
 * Copyright (c) 2004 VMware, Inc.  All rights reserved.
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

/*
 *
 *  Copyright (c) 2004 Determina, Inc
 *  All rights reserved
 *
 *  based on sd_tester.c for security evaluation
 *
 *  Copyright (c) 2002-2003 By Next Generation Security S.L.
 *  All rights  reserved
 *
 */

#include "tools.h"

typedef unsigned char *(*func_TYPE)();

#define OPC_NOP 0x90
#define NUM_NOPS 1000

char data_section[4096]; // For .data tests

unsigned char shellcode[] = "\xc3";

void
usage(char *prog)
{
    print("%s [stack | heap | newheap | crtheap | virtual | virtual_x | .data]\n", prog);
}

int
buffer_test(char *location)
{
    char stack_buffer[4096];
    char *ptr = NULL;
    func_TYPE func;

    if (strcmp(location, "stack") == 0) {
        ptr = stack_buffer;
    }

    if (strcmp(location, "heap") == 0) {
        ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4000);
    }

    if (strcmp(location, "newheap") == 0) {
        ptr = HeapAlloc(HeapCreate(0, 8000, 16000), HEAP_ZERO_MEMORY, 4000);
    }

    if (strcmp(location, "virtual") == 0) {
        ptr = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    if (strcmp(location, "virtual_x") == 0) {
        ptr = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }

    if (strcmp(location, "crtheap") == 0) {
        ptr = malloc(4096);
    }

    if (strcmp(location, ".data") == 0) {
        ptr = data_section;
    }

    if (ptr == NULL) {
        usage("use");
        return (-1);
    }

    memset(ptr, OPC_NOP, NUM_NOPS);
    memcpy(ptr + NUM_NOPS, shellcode, sizeof(shellcode));

    func = (func_TYPE)ptr;

    VERBOSE_PRINT("ptr: %#x\n", ptr);
    print("Executing %s shellcode...\n", location);
    (*func)();
    print("success!\n");

    return (0);
}

int
main(int argc, char **argv)
{
    INIT();

    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        usage(argv[0]);
        return (-1);
    }

    if (argc == 1) {
        print("full suite run\n");
        buffer_test("stack");
        buffer_test("heap");
        buffer_test("newheap");
#ifndef X64
        /* disabling b/c messes up the print: "success" comes out as "Ðuccess" */
        buffer_test("crtheap");
#endif
        buffer_test("virtual");
        buffer_test("virtual_x");
        buffer_test(".data");
        /* TODO:  .text, TEB */
    }
}
