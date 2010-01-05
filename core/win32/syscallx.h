/* **********************************************************
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2007 Determina Corp. */

/*
 * syscallx.h
 *
 * System call support due to the varying system call numbers across platforms.
 * To have one binary work on multiple Windows we can't have one set of constants.
 *
 * Usage:
 * #define SYSCALL(name, act, arg, vista_sp1_x64, vista_sp1, vista_sp0_x64,
 *                 vista_sp0, 2k3, xp64, wow64, xp, 2k, ntsp4, ntsp3, ntsp0)
 * #include "syscallx.h"
 * #undef SYSCALL
 *
 */

/* NOTE - Vista Beta 2 has different syscall numbers than Vista final,
 * the #s here are for for Vista Final (see cvs code attic for Beta
 * 2). 
 */ 

/* We expect x64 2003 and x64 XP to have the same system call numbers but
 * that has not been verified
 */

/* for NT SP4, SP5, SP6, and SP6a 
 * Metasploit's table claims SP4 has additional syscalls, though our
 * investigation disagrees (see case 5616) -- even if so they are appended 
 * and so don't affect the numbering of any of these.
 */

/* Column descriptions:
 * action?    == does DR need to take action when the app issues this system call?
 * nargs      == number of arguments on x64; should we assume this is always arg32/4?
 * arg32      == argument size in bytes on x86
 * wow64      == index into argument conversion routines (see case 3922)
 * all others == system call number for that windows version
 */

/* FIXME: MS sometimes changes argsz between OS versions (see vista case 6853 for
 * some examples) instead of adding an Ex version: if that happens to any of the
 * syscalls we care about we'll have to augment this table.
 */

/* shorter name for the table */
#define NONE SYSCALL_NOT_PRESENT
/* to make some system calls actionable only in DEBUG (because all we do is log) */
#define ACTION_LOG IF_DEBUG_ELSE(true, false)
/*                                                       vista         vista
/*                                                        x64   vista   x64   vista
 *      Name                       action?  nargs arg32   sp1    sp1    sp0    sp0    2003   xp64 wow64     xp  2000 ntsp4 ntsp3 ntsp0 */
SYSCALL(Continue,                     true,     2, 0x08, 0x040, 0x037, 0x040, 0x037, 0x022, 0x040,    0, 0x020, 0x1c, 0x13, 0x13, 0x13)
SYSCALL(CallbackReturn,               true,     3, 0x0c, 0x002, 0x02b, 0x002, 0x02b, 0x016, 0x002,    0, 0x014, 0x13, 0x0b, 0x0b, 0x0b)
SYSCALL(SetContextThread,             true,     2, 0x08, 0x149, 0x121, 0x14f, 0x125, 0x0dd, 0x0f6,    0, 0x0d5, 0xba, 0x99, 0x99, 0x98)
SYSCALL(GetContextThread,             true,     2, 0x08, 0x0c7, 0x097, 0x0c9, 0x097, 0x059, 0x09d,    0, 0x055, 0x49, 0x3c, 0x3c, 0x3c)
SYSCALL(CreateProcess,                true,     8, 0x20, 0x0a0, 0x048, 0x0a2, 0x048, 0x031, 0x082,    0, 0x02f, 0x29, 0x1f, 0x1f, 0x1f)
SYSCALL(CreateProcessEx,              true,     9, 0x24, 0x04a, 0x049, 0x04a, 0x049, 0x032, 0x04a,    0, 0x030, NONE, NONE, NONE, NONE)
SYSCALL(CreateUserProcess,            true,    11, 0x2c, 0x0aa, 0x17f, 0x0ac, 0x185,  NONE,  NONE, NONE,  NONE, NONE, NONE, NONE, NONE)
SYSCALL(TerminateProcess,             true,     2, 0x08, 0x029, 0x14e, 0x029, 0x152, 0x10a, 0x029,    0, 0x101, 0xe0, 0xbb, 0xbb, 0xba)
SYSCALL(CreateThread,                 true,     8, 0x20, 0x04b, 0x04e, 0x04b, 0x04e, 0x037, 0x04b,    0, 0x035, 0x2e, 0x24, 0x24, 0x24)
SYSCALL(CreateThreadEx,               true,    11, 0x2c, 0x0a5, 0x17e, 0x0a7, 0x184,  NONE,  NONE, NONE,  NONE, NONE, NONE, NONE, NONE)
SYSCALL(TerminateThread,              true,     2, 0x08, 0x050, 0x14f, 0x050, 0x153, 0x10b, 0x050,    0, 0x102, 0xe1, 0xbc, 0xbc, 0xbb)
SYSCALL(SuspendThread,                true,     2, 0x08, 0x172, 0x14b, 0x179, 0x14f, 0x107, 0x118, 0x07, 0x0fe, 0xdd, 0xb9, 0xb9, 0xb8)
SYSCALL(AllocateVirtualMemory,        true,     6, 0x18, 0x015, 0x012, 0x015, 0x012, 0x012, 0x015,    0, 0x011, 0x10, 0x0a, 0x0a, 0x0a)
SYSCALL(FreeVirtualMemory,            true,     4, 0x10, 0x01b, 0x093, 0x01b, 0x093, 0x057, 0x01b,    0, 0x053, 0x47, 0x3a, 0x3a, 0x3a)
SYSCALL(ProtectVirtualMemory,         true,     5, 0x14, 0x04d, 0x0d2, 0x04d, 0x0d2, 0x08f, 0x04d,    0, 0x089, 0x77, 0x60, 0x60, 0x60)
SYSCALL(QueryVirtualMemory,           true,     6, 0x18, 0x020, 0x0fd, 0x020, 0x0fd, 0x0ba, 0x020,    0, 0x0b2, 0x9c, 0x81, 0x81, 0x81)
SYSCALL(WriteVirtualMemory,           true,     5, 0x14, 0x037, 0x166, 0x037, 0x16a, 0x11f, 0x037,    0, 0x115, 0xf0, 0xcb, 0xcb, 0xc9)
SYSCALL(MapViewOfSection,             true,    10, 0x28, 0x025, 0x0b1, 0x025, 0x0b1, 0x071, 0x025,    0, 0x06c, 0x5d, 0x49, 0x49, 0x49)
SYSCALL(UnmapViewOfSection,           true,     2, 0x08, 0x027, 0x15c, 0x027, 0x160, 0x115, 0x027,    0, 0x10b, 0xe7, 0xc2, 0xc2, 0xc1)
SYSCALL(FlushInstructionCache,        true,     3, 0x0c, 0x0bf, 0x08d, 0x0c1, 0x08d, 0x052, 0x098, 0x0c, 0x04e, 0x42, 0x36, 0x36, 0x36)
SYSCALL(FreeUserPhysicalPages,        true,     3, 0x0c, 0x0c4, 0x092, 0x0c6, 0x092, 0x056, 0x09c,    0, 0x052, 0x46, NONE, NONE, NONE)
SYSCALL(MapUserPhysicalPages,         true,     3, 0x0c, 0x0e4, 0x0af, 0x0e7, 0x0af, 0x06f, 0x0b2, 0x0a, 0x06a, 0x5b, NONE, NONE, NONE)
    /* FIXME: processing needed only for
     * ASLR_SHARED, but can't be made dynamic */
SYSCALL(OpenSection,                  true,     3, 0x0c, 0x034, 0x0c5, 0x034, 0x0c5, 0x083, 0x034,    0, 0x07d, 0x6c, 0x56, 0x56, 0x56)
SYSCALL(CreateSection,                true,     7, 0x1c, 0x047, 0x04b, 0x047, 0x04b, 0x034, 0x047,    0, 0x032, 0x2b, 0x21, 0x21, 0x21)
SYSCALL(Close,                        true,     1, 0x04, 0x00c, 0x030, 0x00c, 0x02f, 0x01b, 0x00c,    0, 0x019, 0x18, 0x0f, 0x0f, 0x0f)
SYSCALL(DuplicateObject,              true,     7, 0x1c, 0x039, 0x081, 0x039, 0x081, 0x047, 0x039,    0, 0x044, 0x3a, 0x2f, 0x2f, 0x2f)
#ifdef DEBUG                                                                                                    
    /* FIXME: move this stuff to an strace-like
     * client, not needed for core DynamoRIO (at
     * least not that we know of)
     */
SYSCALL(AlertResumeThread,       ACTION_LOG,    2, 0x08, 0x06a, 0x00d, 0x06a, 0x00d, 0x00d, 0x069, 0x07, 0x00c, 0x0b, 0x06, 0x06, 0x06)
SYSCALL(OpenFile,                ACTION_LOG,    6, 0x18, 0x030, 0x0ba, 0x030, 0x0ba, 0x07a, 0x030,    0, 0x074, 0x64, 0x4f, 0x4f, 0x4f)
#endif                                                                 
    /* These ones are here for DR's own use */
SYSCALL(ResumeThread,            ACTION_LOG,    2, 0x08, 0x04f, 0x11a, 0x04f, 0x119, 0x0d6, 0x04f, 0x07, 0x0ce, 0xb5, 0x96, 0x96, 0x95)
SYSCALL(TestAlert,                    false,    0,    0, 0x175, 0x150, 0x17c, 0x154, 0x10c, 0x11b, 0x02, 0x103, 0xe2, 0xbd, 0xbd, 0xbc)
SYSCALL(RaiseException,               false,    3, 0x0c, 0x126, 0x100, 0x12b, 0x100, 0x0bd, 0x0e1,    0, 0x0b5, 0x9f, 0x84, 0x84, 0x84)
SYSCALL(CreateFile,                   false,   11, 0x2c, 0x052, 0x03c, 0x052, 0x03c, 0x027, 0x052,    0, 0x025, 0x20, 0x17, 0x17, 0x17)
SYSCALL(CreateKey,                    false,    7, 0x1c, 0x01a, 0x040, 0x01a, 0x040, 0x02b, 0x01a,    0, 0x029, 0x23, 0x19, 0x19, 0x19)
SYSCALL(OpenKey,                      false,    3, 0x0c, 0x00f, 0x0bd, 0x00f, 0x0bd, 0x07d, 0x00f,    0, 0x077, 0x67, 0x51, 0x51, 0x51)
SYSCALL(SetInformationFile,           false,    5, 0x14, 0x024, 0x12d, 0x024, 0x131, 0x0e9, 0x024,    0, 0x0e0, 0xc2, 0xa1, 0xa1, 0xa0)
SYSCALL(SetValueKey,                  false,    6, 0x18, 0x05d, 0x144, 0x05d, 0x148, 0x100, 0x05d,    0, 0x0f7, 0xd7, 0xb3, 0xb3, 0xb2)

#undef NONE
#undef ACTION_LOG



/* Attic - there is little point in continuing to update the syscall numbers
 * below since they are only used for ignorable system calls which is
 * terminally broken anyways. */
#if 0
    /* we don't intercept these syscalls for correctness or security policies,
     * but we need to come back to a non-cache point for them since they
     * are alertable and callbacks can be delivered during the syscall.
     * this list came from this filter of the syscall names:
     *   grep 'Alert|Wait' | grep -v CreateWaitablePort
     *   (NtContinue is already up above)
     *   plus grep 'Alert|Wait' in ntdll.h (ZwDelayExecution)
     */
    /* FIXME - don't think all of these are in fact alertable, plus many file
     * io syscalls are alertable depending on the options passed when the file
     * handle was created.  There also may other alertable system calls we
     * don't know about and since we don't even use ignore syscalls (with no
     * plan to bring it back) having them in our arrays doesn't serve much
     * purpose. */
    /* FIXME: NT4.0 (only) has two more exported *Wait* routines,
     * NtSetHighWaitLowThread and NtSetLowWaitHighThread, that each
     * have a gap in the system call numbering, but in the ntdll dump
     * they look like int 2b and 2c, resp.  Now, Inside Win2K lists int 2c as
     * "KiSetLowWaitHighThread" (though 2b is what we expect, even on NT,
     * KiCallbackReturn).  Nebbett has more pieces of the story:
     * "three of the four entry points purporting to refer to this system
     * service actually invoke a different routine", int 2c/2b, which does not
     * do what this call is supposed to do -- only NTOSKRNL!NtSet* does the
     * right thing.  Thus, we don't bother to intercept, as we will never
     * see those system calls, right?
     */
/*                                                       vista  vista
 *      Name                       action?  nargs arg32   sp1    sp0    2003   xp64 wow64     xp  2000 ntsp4 ntsp3 ntsp0 */
SYSCALL(AlertThread,                  false,    1, 0x04, 0x00e, 0x00e, 0x00e, 0x06a, 0x03, 0x00d, 0x0c, 0x07, 0x07, 0x07)
SYSCALL(DelayExecution,               false,    2, 0x08, 0x076, 0x076, 0x03d, 0x031, 0x06, 0x03b, 0x32, 0x27, 0x27, 0x27)
SYSCALL(ReplyWaitReceivePort,         false,    4, 0x10, 0x10f, 0x10e, 0x0cb, 0x008,    0, 0x0c3, 0xab, 0x90, 0x90, 0x8f)
SYSCALL(ReplyWaitReceivePortEx,       false,    5, 0x14, 0x110, 0x10f, 0x0cc, 0x028,    0, 0x0c4, 0xac, NONE, NONE, NONE)
SYSCALL(ReplyWaitReplyPort,           false,    2, 0x08, 0x111, 0x110, 0x0cd, 0x0e8,    0, 0x0c5, 0xad, 0x91, 0x91, 0x90)
SYSCALL(ReplyWaitSendChannel,         false,    3, 0x0c,  NONE,  NONE,  NONE,  NONE, NONE,  NONE, 0xf4, 0xcf, 0xd0, 0xce)
SYSCALL(RequestWaitReplyPort,         false,    3, 0x0c, 0x114, 0x113, 0x0d0, 0x01f,    0, 0x0c8, 0xb0, 0x93, 0x93, 0x92)
SYSCALL(SendWaitReplyChannel,         false,    4, 0x10,  NONE,  NONE,  NONE,  NONE, NONE,  NONE, 0xf5, 0xd0, 0xd1, 0xcf)
SYSCALL(SetHighWaitLowEventPair,      false,    1, 0x04, 0x12b, 0x12f, 0x0e7, 0x0fe, 0x03, 0x0de, 0xc1, 0x9f, 0x9f, 0x9e)
SYSCALL(SetLowWaitHighEventPair,      false,    1, 0x04, 0x138, 0x13c, 0x0f4, 0x107, 0x03, 0x0eb, 0xcc, 0xaa, 0xaa, 0xa9)
SYSCALL(SignalAndWaitForSingleObject, false,    4, 0x10, 0x147, 0x14b, 0x103, 0x114, 0x13, 0x0fa, 0xda, 0xb6, 0xb6, 0xb5)
SYSCALL(WaitForDebugEvent,            false,    4, 0x10, 0x15e, 0x162, 0x117, 0x124,    0, 0x10d, NONE, NONE, NONE, NONE)
SYSCALL(WaitForKeyedEvent,            false,    4, 0x10, 0x16b, 0x16f, 0x124, 0x125, 0x15, 0x11a, NONE, NONE, NONE, NONE)
SYSCALL(WaitForMultipleObjects,       false,    5, 0x14, 0x15f, 0x163, 0x118, 0x058, 0x1d, 0x10e, 0xe9, 0xc4, 0xc4, 0xc3)
SYSCALL(WaitForSingleObject,          false,    3, 0x0c, 0x160, 0x164, 0x119, 0x001, 0x0d, 0x10f, 0xea, 0xc5, 0xc5, 0xc4)
SYSCALL(WaitHighEventPair,            false,    1, 0x04, 0x161, 0x165, 0x11a, 0x126, 0x03, 0x110, 0xeb, 0xc6, 0xc6, 0xc5)
SYSCALL(WaitLowEventPair,             false,    1, 0x04, 0x162, 0x166, 0x11b, 0x127, 0x03, 0x111, 0xec, 0xc7, 0xc7, 0xc6)
#endif /* if 0 */
