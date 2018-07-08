/* **********************************************************
 * Copyright (c) 2012-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* Tests protection of DynamoRIO's stack
 * This test when run natively will fail with an error message
 */

#include "tools.h"

#ifdef UNIX
#    include <unistd.h>
#    include <signal.h>
#    include <ucontext.h>
#    include <errno.h>
#endif

#include <setjmp.h>

#define VERBOSE 0

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

/****************************************************************************/

/* the data sections to try to write to */
static const char *const datasec_names[] = { ".data", ".fspdata", ".cspdata",
                                             ".nspdata" };
#define DATASEC_NUM (sizeof(datasec_names) / sizeof(datasec_names[0]))

typedef unsigned char byte;

#ifdef UNIX
#    error NYI
#else
bool
get_named_section_bounds(byte *module_base, const char *name, byte **start /*OUT*/,
                         byte **end /*OUT*/)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    uint i;
    bool prev_sec_same_chars = false;
    assert(module_base != NULL);
    dos = (IMAGE_DOS_HEADER *)module_base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    if (nt == NULL || nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    assert(start != NULL || end != NULL);
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec->Name != NULL && strncmp(sec->Name, name, strlen(name)) == 0) {
            if (start != NULL)
                *start = module_base + sec->VirtualAddress;
            if (end != NULL)
                *end = module_base + sec->VirtualAddress +
                    ALIGN_FORWARD(sec->Misc.VirtualSize, PAGE_SIZE);
            return true;
        }
        sec++;
    }
    return false;
}

byte *
get_DR_base()
{
    /* read DR marker
     * just hardcode the offsets for now
     */
    static const uint DR_BASE_OFFSET = IF_X64_ELSE(0x20, 0x1c);
    return get_drmarker_field(DR_BASE_OFFSET);
}
#endif /* WINDOWS */

/****************************************************************************/

jmp_buf mark;
int where; /* 0 = normal, 1 = segfault longjmp */

#ifdef UNIX
static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
#    if VERBOSE
        print("Got seg fault\n");
#    endif
        longjmp(mark, 1);
    }
    exit(-1);
}
#else
/* sort of a hack to avoid the MessageBox of the unhandled exception spoiling
 * our batch runs
 */
#    include <windows.h>
/* top-level exception handler */
static LONG
our_top_handler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
#    if VERBOSE
        print("Got segfault\n");
#    endif
        longjmp(mark, 1);
    }
#    if VERBOSE
    print("Exception occurred, process about to die silently\n");
#    endif
    return EXCEPTION_EXECUTE_HANDLER; /* => global unwind and silent death */
}
#endif

int
main()
{
    uint i;
    uint writes = 0;
    byte *DR_base;
    INIT();

#ifdef UNIX
    intercept_signal(SIGSEGV, (handler_3_t)signal_handler, false);
#else
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)our_top_handler);
#endif

    where = setjmp(mark);
    if (where == 0) {
        DR_base = get_DR_base();
#if VERBOSE
        print("DR base is " PFX "\n", DR_base);
#endif
        for (i = 0; i < DATASEC_NUM; i++) {
            byte *start, *end, *pc;
            bool found = get_named_section_bounds(DR_base,
                                                  /*FIXME use drmarker to find*/
                                                  datasec_names[i], &start, &end);
            assert(found);
#if VERBOSE
            print("data section %s: " PFX "-" PFX "\n", datasec_names[i], start, end);
#endif
            print("about to write to every page in %s\n", datasec_names[i]);
            for (pc = start; pc < end; pc += PAGE_SIZE) {
                /* try to write to every single page */
                where = setjmp(mark);
                if (where == 0) {
                    uint old = *((uint *)pc);
                    *((uint *)pc) = 0x0badbad0;
                    /* restore in same bb so we don't crash DR */
                    *((uint *)pc) = old;
                    /* if the section is protected we shouldn't get here */
                    writes++;
#if VERBOSE
                    print("successfully wrote to " PFX " in %s!\n", pc, datasec_names[i]);
#endif
                }
            }
#if VERBOSE
            print("successfully wrote to %d pages!\n", writes);
#endif
            if (writes > 0)
                print("successfully wrote to %s\n", datasec_names[i]);
        }
    } else {
        print("no DR or no data sections found: are you running natively?\n");
    }

    print("all done\n");
}
