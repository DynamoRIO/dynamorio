/* **********************************************************
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

/* case 8514 testing many sections - only up to 96 are allowed by the
 *       XP SP2 loader
 *
 * case 6772 testing funny flags - though not really excercising the
 * implied change in functionality: should see if they are acted upon
 * - e.g. is discard in memory, is .shared indeed shared between
 * processes, etc.
 *
 *
 * note we don't have too many code sections
 */

#include "tools.h"
#include <windows.h>

/* also preserving alignment test from secalign-fixed.dll.c */

/* Linker requires /driver if specifying /align.
 * Documentation says that "The linker will perform some special
 * optimizations if this option is selected." -- not sure if any other changes.
 */
#pragma comment(linker, "/align:0x2000 /driver")

#pragma comment(linker, "/SECTION:.shared,RWS")
#pragma data_seg(".shared")
int shared1 = 1;
int shared2;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.discard,RWD")
#pragma data_seg(".discard")
int discard1 = 2;
int discard2;
#pragma data_seg()

/* FIXME: case 8677 keeps track of PAGE_NOCACHE problems with ASLR !K*/
#pragma comment(linker, "/SECTION:.nocache,RK")
#pragma data_seg(".nocache")
int nocache1 = 3;
int nocache2;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.nopage,RW!P")
#pragma data_seg(".nopage")
int nopage1 = 4;
int nopage2;
#pragma data_seg()

/*
E Execute The section is executable
R Read Allows read operations on data
W Write Allows write operations on data

S Shared Shares the section among all processes that load the image
D Discardable Marks the section as discardable
K Cacheable Marks the section as not cacheable
P Pageable Marks the section as not pageable

L Preload VxD only; marks the section as preload
X Memory-resident VxD only; marks the section as memory-resident
*/

#pragma comment(linker, "/SECTION:.erw0,ERW")
#pragma data_seg(".erw0")
int erw1 = 5;
int erw2;
#pragma data_seg()

/* FIXME: we can't use _Pragma as we do in core in ACTUAL_PRAGMA since we're
 * not using gcc as a preprocessor here! */
/* Use special C99 operator _Pragma to generate a pragma from a macro */
#define ACTUAL_PRAGMA(p) _Pragma(#p)
#define START_DATA_SECTION(name, prot) ACTUAL_PRAGMA(data_seg(name))
#define VAR_IN_SECTION(name) /* nothing */
#define END_DATA_SECTION() ACTUAL_PRAGMA(data_seg())

#define SECTION_SET(id)                   \
    START_DATA_SECTION(".rx" #id, "rx")   \
    int rx##id;                           \
    END_DATA_SECTION()                    \
    START_DATA_SECTION(".rwx" #id, "rwx") \
    int rwx##id;                          \
    END_DATA_SECTION()                    \
    START_DATA_SECTION(".r" #id, "r")     \
    int r##id;                            \
    END_DATA_SECTION()

/* FIXME: plan was to just use SECTION_SET(1) ...
 * but for now doing manually
 */

/* add alignment - has to be smaller than the /ALIGN option above */
#pragma comment(linker, "/SECTION:.awer5,WER,ALIGN:0x1000")
#pragma data_seg(".awer5")
int awer5 = 5;
#pragma data_seg()

/* ================= */
/* iterate through ER and ERW (though with high alignment already separate allocations) */
#pragma comment(linker, "/SECTION:.cer1,ER")
#pragma code_seg(".cer1")
int cer1 = 5;
/* this is supposed to crash if ever run since not writable */
void
funcer1()
{
    cer1 = 1;
}
#pragma code_seg()

/* does it matter what is what? */
#pragma comment(linker, "/SECTION:.cwer1,WER")
#pragma data_seg(".cwer1")
int cwer1 = 5;
/* forcing a relocation */
void
funccwer1()
{
    cwer1 = 1;
}
#pragma data_seg()

#pragma comment(linker, "/SECTION:.cer2,ER")
#pragma code_seg(".cer2")
int cer2 = 5;
void
funcer2()
{
    cer2 = 2;
}
#pragma code_seg()

#pragma comment(linker, "/SECTION:.cer3,ER")
#pragma code_seg(".cer3")
int cer3 = 5;
void
funcer3()
{
    cer3 = 3;
}
#pragma code_seg()
/* FIXME: could add more code sections, but to avoid triggering the
 * curiosity in add_rct_module() not adding many code sections */

#pragma bss_seg(".bss1")
int bss1;
int bss2;
#pragma bss_seg()

#pragma const_seg(".rdata1")
const char hello1[] = "hello world";
const char hello2[] = "hello world";

const char hello3[] = "hello new world";
#pragma const_seg()

/* iterate through ER and ERW (though with high alignment already separate allocations) */
#pragma comment(linker, "/SECTION:.er1,ER")
#pragma data_seg(".er1")
int er1 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer1,WER")
#pragma data_seg(".wer1")
int wer1 = 5;
#pragma data_seg()

/* FIXME: just doing manually in emacs for now
 *  (query-replace "er1" "er2" nil nil nil)
 */
#pragma comment(linker, "/SECTION:.er2,ER")
#pragma data_seg(".er2")
int er2 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer2,WER")
#pragma data_seg(".wer2")
int wer2 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er3,ER")
#pragma data_seg(".er3")
int er3 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer3,WER")
#pragma data_seg(".wer3")
int wer3 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er4,ER")
#pragma data_seg(".er4")
int er4 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer4,WER")
#pragma data_seg(".wer4")
int wer4 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er5,ER")
#pragma data_seg(".er5")
int er5 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer5,WER")
#pragma data_seg(".wer5")
int wer5 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er6,ER")
#pragma data_seg(".er6")
int er6 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer6,WER")
#pragma data_seg(".wer6")
int wer6 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er7,ER")
#pragma data_seg(".er7")
int er7 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer7,WER")
#pragma data_seg(".wer7")
int wer7 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er8,ER")
#pragma data_seg(".er8")
int er8 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer8,WER")
#pragma data_seg(".wer8")
int wer8 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er9,ER")
#pragma data_seg(".er9")
int er9 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer9,WER")
#pragma data_seg(".wer9")
int wer9 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er10,ER")
#pragma data_seg(".er10")
int er10 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer10,WER")
#pragma data_seg(".wer10")
int wer10 = 5;
#pragma data_seg()

/* now replacing er with er20 */

/* --- */
#pragma comment(linker, "/SECTION:.er201,ER")
#pragma data_seg(".er201")
int er201 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer201,WER")
#pragma data_seg(".wer201")
int wer201 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.er202,ER")
#pragma data_seg(".er202")
int er202 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer202,WER")
#pragma data_seg(".wer202")
int wer202 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er203,ER")
#pragma data_seg(".er203")
int er203 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer203,WER")
#pragma data_seg(".wer203")
int wer203 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er204,ER")
#pragma data_seg(".er204")
int er204 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer204,WER")
#pragma data_seg(".wer204")
int wer204 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er205,ER")
#pragma data_seg(".er205")
int er205 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer205,WER")
#pragma data_seg(".wer205")
int wer205 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er206,ER")
#pragma data_seg(".er206")
int er206 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer206,WER")
#pragma data_seg(".wer206")
int wer206 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er207,ER")
#pragma data_seg(".er207")
int er207 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer207,WER")
#pragma data_seg(".wer207")
int wer207 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er208,ER")
#pragma data_seg(".er208")
int er208 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer208,WER")
#pragma data_seg(".wer208")
int wer208 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er209,ER")
#pragma data_seg(".er209")
int er209 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer209,WER")
#pragma data_seg(".wer209")
int wer209 = 5;
#pragma data_seg()

/* now replacing er20 with er30 */

/* --- */
#pragma comment(linker, "/SECTION:.er301,ER")
#pragma data_seg(".er301")
int er301 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer301,WER")
#pragma data_seg(".wer301")
int wer301 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.er302,ER")
#pragma data_seg(".er302")
int er302 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer302,WER")
#pragma data_seg(".wer302")
int wer302 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er303,ER")
#pragma data_seg(".er303")
int er303 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer303,WER")
#pragma data_seg(".wer303")
int wer303 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er304,ER")
#pragma data_seg(".er304")
int er304 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer304,WER")
#pragma data_seg(".wer304")
int wer304 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er305,ER")
#pragma data_seg(".er305")
int er305 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer305,WER")
#pragma data_seg(".wer305")
int wer305 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er306,ER")
#pragma data_seg(".er306")
int er306 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer306,WER")
#pragma data_seg(".wer306")
int wer306 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er307,ER")
#pragma data_seg(".er307")
int er307 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer307,WER")
#pragma data_seg(".wer307")
int wer307 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er308,ER")
#pragma data_seg(".er308")
int er308 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer308,WER")
#pragma data_seg(".wer308")
int wer308 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er309,ER")
#pragma data_seg(".er309")
int er309 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer309,WER")
#pragma data_seg(".wer309")
int wer309 = 5;
#pragma data_seg()

/* now replacing er20 with er40 */

/* --- */
#pragma comment(linker, "/SECTION:.er401,ER")
#pragma data_seg(".er401")
int er401 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer401,WER")
#pragma data_seg(".wer401")
int wer401 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.er402,ER")
#pragma data_seg(".er402")
int er402 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer402,WER")
#pragma data_seg(".wer402")
int wer402 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er403,ER")
#pragma data_seg(".er403")
int er403 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer403,WER")
#pragma data_seg(".wer403")
int wer403 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er404,ER")
#pragma data_seg(".er404")
int er404 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer404,WER")
#pragma data_seg(".wer404")
int wer404 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er405,ER")
#pragma data_seg(".er405")
int er405 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer405,WER")
#pragma data_seg(".wer405")
int wer405 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er406,ER")
#pragma data_seg(".er406")
int er406 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer406,WER")
#pragma data_seg(".wer406")
int wer406 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er407,ER")
#pragma data_seg(".er407")
int er407 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer407,WER")
#pragma data_seg(".wer407")
int wer407 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er408,ER")
#pragma data_seg(".er408")
int er408 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer408,WER")
#pragma data_seg(".wer408")
int wer408 = 5;
#pragma data_seg()

/* --- */
#pragma comment(linker, "/SECTION:.er409,ER")
#pragma data_seg(".er409")
int er409 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer409,WER")
#pragma data_seg(".wer409")
int wer409 = 5;
#pragma data_seg()

/* now replacing er20 with er50 */
/* --- */
#pragma comment(linker, "/SECTION:.er501,ER")
#pragma data_seg(".er501")
int er501 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.wer501,WER")
#pragma data_seg(".wer501")
int wer501 = 5;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.er502,ER")
#pragma data_seg(".er502")
int er502 = 5;
#pragma data_seg()

#ifndef X64 /* leaving this in goes over the xp x64 loader limit so removing */
#    pragma comment(linker, "/SECTION:.wer502,WER")
#    pragma data_seg(".wer502")
int wer502 = 5;
#    pragma data_seg()
#endif

#if 0 /* leaving this in goes over the WOW64 xp loader limit so removing */
/* --- */
#    pragma comment(linker, "/SECTION:.er503,ER")
#    pragma data_seg(".er503")
int er503 = 5;
#    pragma data_seg()
#endif

/* 96 if we stop adding .er50x here */

/* although dumpbin has no problems with 107 sections */
/* The windows XP SP2 loader still maintains this limit
 * ---------------------------
 * section-max.exe - Bad Image
 * ---------------------------
 * The application or DLL i:\vlk\trees\tot\suite\tests\security-win32\section-max.dll.dll
 * is not a valid Windows image. Please check this against your installation diskette.
 * ---------------------------
 * OK
 * ---------------------------
 * > error loading library section-max.dll.dll
 *
 */

int __declspec(dllexport) make_a_lib(int arg)
{
    shared2 = 101;
    return shared1 + discard1 + nocache1 + nopage1 + erw1;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: print("in section max dll\n"); break;
    }
    return TRUE;
}
