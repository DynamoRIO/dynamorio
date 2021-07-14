/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2006-2007 Determina Corp. */

/*
 * syscall.c - win32-specific system call handling routines
 */

#include "../globals.h"
#include "../fragment.h"
#include "ntdll.h"
#include "os_private.h"
#include "aslr.h"
#include "instrument.h"
#include "../synch.h"

/* this points to one of the os-version-specific system call # arrays below */
int *syscalls = NULL;
/* this points to one of the os-version-specific wow64 argument conversion arrays */
int *wow64_index = NULL;

/* Ref case 5217 - for Sygate compatibility we indirect int 2e system
 * calls through the int_syscall_address (which after syscalls_init()
 * will point to an int 2e, ret 0 in ntdll.dll. This is, for all intents
 * and purposes, a function pointer that will be set only once early
 * during app init, so we keep it here with the options to leverage their
 * protection. */
app_pc int_syscall_address = NULL;
/* Ref case 5441 - for Sygate compatibility we fake our return address from
 * sysenter system calls (they sometimes verify) to this address which will
 * (by default) point to a ret 0 in ntdll.dll.  This is, for all intents and
 * purposes, a function pointer that will be set only once early during app
 * init, so we keep it here with the options to leverage their protection. */
app_pc sysenter_ret_address = NULL;
/* i#537: sysenter returns to KiFastSystemCallRet from kernel */
app_pc KiFastSystemCallRet_address = NULL;

/* Snapshots are relatively heavyweight, so we do not take them on every memory
 * system call.  On the other hand, if we only did them when we dumped
 * stats, we'd miss large memory allocations that were freed prior
 * to the next stats dump (which can be far between if not much new code
 * is being executed).  Thus, we do them whenever we print stats and on
 * every memory operation larger than this threshold:
 */
#define SNAPSHOT_THRESHOLD (16 * PAGE_SIZE)

/*******************************************************/

/* i#1230: we support a limited number of extra interceptions.
 * We add extra slots to all of the arrays.
 */
#define CLIENT_EXTRA_TRAMPOLINE 12
#define TRAMPOLINE_MAX (SYS_MAX + CLIENT_EXTRA_TRAMPOLINE)
/* no lock needed since only supported during dr_client_main */
static uint syscall_extra_idx;

const char *SYS_CONST syscall_names[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    "Nt" #name,
#include "syscallx.h"
#undef SYSCALL
};

/* i#1598: we try to make progress on unknown versions */
int windows_unknown_syscalls[TRAMPOLINE_MAX];

/* XXX i#2713: With the frequent major win10 updates, adding new tables here is
 * getting tedious and taking up space.  Should we stop adding the win10 updates here
 * and give up on our table of numbers, relying on reading the wrappers (i#1598
 * changed DR to work purely on wrapper-obtained numbers)?  We'd lose robustness vs
 * hooks, and clients like Dr. Memory who have to distinguish win10 versions would
 * have to do their own versioning.  I guess we could still have
 * DR_WINDOWS_VERSION_xx and not have corresponding tables here.  Or we could go the
 * planned Dr. Memory route (DrMi#1848) and store these numbers in a separate file
 * that is updated via a separate standalone utility run once by the user.
 */
SYS_CONST int windows_10_1803_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w15x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1803_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w15w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1803_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w15x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1709_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w14x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1709_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w14w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1709_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w14x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1703_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w13x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1703_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w13w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1703_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w13x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1607_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w12x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1607_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w12w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1607_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w12x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1511_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w11x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1511_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w11w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_1511_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w11x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w10x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w10w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_10_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w10x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_81_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w81x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_81_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w81w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_81_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w81x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_8_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w8x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_8_wow64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w8w64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_8_x86_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w8x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_7_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w7x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_7_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w7x86,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_vista_sp1_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    vista1_x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_vista_sp1_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    vista1,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_vista_sp0_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    vista0_x64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_vista_sp0_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    vista0,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_2003_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w2k3,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_XP_x64_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    xp64,
#include "syscallx.h"
#undef SYSCALL
};
/* This is the index for XP through Win7. */
SYS_CONST int windows_XP_wow64_index[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    wow64,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_XP_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    xp,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_2000_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    w2k,
#include "syscallx.h"
#undef SYSCALL
};
SYS_CONST int windows_NT_sp4_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    ntsp4,
#include "syscallx.h"
#undef SYSCALL
};
/* for SP3 (and maybe SP2 or SP1 -- haven't checked those) */
SYS_CONST int windows_NT_sp3_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    ntsp3,
#include "syscallx.h"
#undef SYSCALL
};
/* for SP0 (and maybe SP2 or SP1 -- haven't checked those) */
SYS_CONST int windows_NT_sp0_syscalls[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    ntsp0,
#include "syscallx.h"
#undef SYSCALL
};

/* for x64 this is the # of args */
SYS_CONST uint syscall_argsz[TRAMPOLINE_MAX] = {
#ifdef X64
#    define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64, \
                    w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,  \
                    w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64,       \
                    w11x86, w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64,     \
                    w13x64, w14x86, w14w64, w14x64, w15x86, w15w64, w15x64)             \
        nargs,
#else
#    define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64, \
                    w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,  \
                    w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64,       \
                    w11x86, w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64,     \
                    w13x64, w14x86, w14w64, w14x64, w15x86, w15w64, w15x64)             \
        arg32,
#endif
#include "syscallx.h"
#undef SYSCALL
};

/* FIXME: currently whether a syscall needs action or not can't be
 * dynamically changed since this flag is used early on by
 * intercept_native_syscall() */
static SYS_CONST int syscall_requires_action[TRAMPOLINE_MAX] = {
#define SYSCALL(name, act, nargs, arg32, ntsp0, ntsp3, ntsp4, w2k, xp, wow64, xp64,     \
                w2k3, vista0, vista0_x64, vista1, vista1_x64, w7x86, w7x64, w8x86,      \
                w8w64, w8x64, w81x86, w81w64, w81x64, w10x86, w10w64, w10x64, w11x86,   \
                w11w64, w11x64, w12x86, w12w64, w12x64, w13x86, w13w64, w13x64, w14x86, \
                w14w64, w14x64, w15x86, w15w64, w15x64)                                 \
    act,
#include "syscallx.h"
#undef SYSCALL
};

/* used to intercept syscalls while native */
static byte *syscall_trampoline_pc[TRAMPOLINE_MAX];
static app_pc syscall_trampoline_skip_pc[TRAMPOLINE_MAX];
static app_pc syscall_trampoline_hook_pc[TRAMPOLINE_MAX];
static app_pc syscall_trampoline_copy_pc[TRAMPOLINE_MAX];

#ifdef GBOP
/* GBOP stack adjustment - currently either always 0 or always 4 for
 * vsyscall calls, but may need to be a more general array in case
 * HOOKED_TRAMPOLINE_HOOK_DEEPER allows different offsets
 * FIXME: case 7127 this can be compressed further, if really only a bitmask
 * see intercept_syscall_wrapper
 */
static byte syscall_trampoline_gbop_fpo_offset[TRAMPOLINE_MAX];
#endif /* GBOP */

/****************************************************************************/

/* System call interception: put any special handling here
 * Arguments come from the pusha right before the call
 * Win32 syscall: int 0x2e, number is in eax, address of start of params
 * on user stack is in edx
 *
 * WinXP uses sysenter instruction and does a call to it since sysenter
 * doesn't store return info -- instead sysexit (called from kernel) grabs
 * continuation pc from edx.  So the callee, same one used by all syscalls,
 * puts esp in edx so that kernel just has to dereference it.
 * Actually, on closer examination, it looks like the kernel sends control
 * directly to 0x7ffe0304, which does a ret to get back to the ret after
 * the call %edx -- since the 0x7ffe0304 ret executes natively we can't tell
 * the difference, but we should be aware of it!  If this is true, why bother
 * filling in edx for sysenter?  Seems like the kernel must be hardcoding it
 * with 0x7ffe0304.
 * FIXME: think about whether want to
 * insert a trampoline (and risk clobbering entry point after the ret)
 * instead of the current method of clobbering the return address
 *
 * Here are some win2000 examples (from ntdll.dll):
NtSetContextThread:
  77F97BFA: B8 BA 00 00 00     mov         eax,0BAh
  77F97BFF: 8D 54 24 04        lea         edx,[esp+4]
  77F97C03: CD 2E              int         2Eh
  77F97C05: C2 08 00           ret         8
this is the only one that does not immediately have a ret, though it
does ret after a jump, some poorly chosen "optimization":
NtContinue:
  77F82872: B8 1C 00 00 00     mov         eax,1Ch
  77F82877: 8D 54 24 04        lea         edx,[esp+4]
  77F8287B: CD 2E              int         2Eh
  77F8287D: E9 82 74 01 00     jmp         77F99D04
  77F99D04: C2 08 00           ret         8
 *
 * WinXP example:
NtOpenKey:
  0x77f7eb23   b8 77 00 00 00       mov    $0x00000077 -> %eax
  0x77f7eb28   ba 00 03 fe 7f       mov    $0x7ffe0300 -> %edx
  0x77f7eb2d   ff d2                call   %edx
  0x7ffe0300   8b d4                mov    %esp -> %edx
  0x7ffe0302   0f 34                sysenter
  0x7ffe0304   c3                   ret    %esp (%esp) -> %esp
  0x77f7eb2f   c2 0c 00             ret    $0x000c %esp (%esp) -> %esp
 */

/* the win32ksys calls are all above 0x1000, only Zw/Nt* are below */
#define MAX_NTOSKRNL_SYSCALL_NUM 0x1000

bool
ignorable_system_call(int num, instr_t *gateway, dcontext_t *dcontext_live)
{
    /* FIXME: this should really be a complete list of ignorable calls,
     * just ntoskrnl ones that we understand, to avoid surprises
     * with added calls?
     */
    /* FIXME: switch to a bit vector?
     * we may want an inverted bit vector instead (inw2k p.123  - lower 12 bits)
     * there are 285 syscalls on xp  - let's say we support 320
     * instead of the 40 ints (160 bytes) and a loop we're using now,
     * we can grab 40 bytes for 320 syscalls and do the bit extraction
     * precomputing from this table will be easy
     */
    /* FIXME : it looks like most file IO/creation syscalls are alertable
     * ref bug 2520, should be added to non-ignorable */
    /* FIXME : we just return false for all system calls, to be safe we should
     * really be checking for known ignoreable system calls rather then the reverse,
     * see syscallx.h for old enumeration. */
    return false;
}

bool
optimizable_system_call(int num)
{
    if (INTERNAL_OPTION(shared_eq_ignore))
        return ignorable_system_call(num, NULL, NULL);
    else {

        int i;

        /* FIXME: switch to a bit vector, just as for the syscalls array? */
        for (i = 0; i < SYS_MAX; i++) {
            if (num == syscalls[i])
                return !syscall_requires_action[i];
        }

        /* If the syscall isn't in the array, DR doesn't care about it. */
        return true;
    }
}

/* The trampoline handler called for ntdll syscall wrappers that we
 * care about, so that we can act on them while native_exec-ing
 */
after_intercept_action_t
syscall_while_native(app_state_at_intercept_t *state)
{
    int sysnum = (int)(ptr_int_t)state->callee_arg;
    /* FIXME : if dr calls through ntdll functions that are hooked by a third
     * party (say Sygate's sysfer.dll) then they could perform syscalls that
     * would get us here.  Most of the time we'll be ok, but if the current
     * thread is under_dyn_hack or native_exec we might try to process the
     * system call or takeover, neither of which is safe.  Currently we avoid
     * calling through nt wrappers that sysfer.dll hooks (doing system call
     * internally instead).  This also applies if we call our own hooks, which
     * we avoid in a similar manner.
     */
    /* Returning AFTER_INTERCEPT_LET_GO will perform the syscall natively,
     * while AFTER_INTERCEPT_LET_GO_ALT_DYN will skip it.  Modify the register
     * arguments to change the returned state, note that the stack will have
     * to be popped once (modify reg_esp) to match up the returns.
     */
    dcontext_t *dcontext = get_thread_private_dcontext();
    IF_X64(ASSERT_TRUNCATE(int, int, (ptr_int_t)state->callee_arg));
    /* N.B.: if any intercepted syscalls are used by DR from ntdll, rather
     * than custom wrappers, then a recursion-avoidance check here would
     * be required to avoid infinite loop on error here!
     */
    STATS_INC(num_syscall_trampolines);
    if (dcontext == NULL) {
        /* unknown thread */
        return AFTER_INTERCEPT_LET_GO; /* do syscall natively */
    } else if (IS_UNDER_DYN_HACK(dcontext->thread_record->under_dynamo_control) ||
               dcontext->thread_record->retakeover) {
        /* this trampoline is our ticket to taking control again prior
         * to the image entry point
         * we often hit this on NtAllocateVirtualMemory from HeapCreate for
         * the next dll init after the cb ret where we lost control
         */
        STATS_INC(num_syscall_trampolines_retakeover);
        LOG(THREAD, LOG_SYSCALLS, 1,
            "syscall_while_native: retakeover in %s after native cb return lost "
            "control\n",
            syscall_names[sysnum]);
        retakeover_after_native(dcontext->thread_record, INTERCEPT_SYSCALL);
        dcontext->thread_record->retakeover = false;
        return AFTER_INTERCEPT_TAKE_OVER; /* syscall under DR */
    } else if (!dcontext->thread_record->under_dynamo_control
               /* xref PR 230836 */
               && !IS_CLIENT_THREAD(dcontext)
               /* i#1318: may get here from privlib at exit, at least until we
                * redirect *everything*.  From privlib we need to keep
                * the syscall native as DR locks may be held.
                */
               && dcontext->whereami == DR_WHERE_APP) {
        /* assumption is that any known native thread is one we control in general,
         * just not right now while in a native_exec_list dll */
        STATS_INC(num_syscall_trampolines_native);
        LOG(THREAD, LOG_SYSCALLS, 1, "NATIVE system call %s\n", syscall_names[sysnum]);
        DOLOG(IF_DGCDIAG_ELSE(1, 2), LOG_SYSCALLS, {
            dump_callstack(*((byte **)state->mc.xsp) /*retaddr*/, (app_pc)state->mc.xbp,
                           THREAD, DUMP_NOT_XML);
        });

#ifdef GBOP
        /* case 7127 - validate GBOP on syscalls that are already hooked for
         * hotp_only on native_exec
         */
        if (DYNAMO_OPTION(gbop) != GBOP_DISABLED) {
            /* FIXME: case 7127: should enforce here GBOP_WHEN_NATIVE_EXEC if we
             * want to apply for -hotp_only but not for native_exec.
             * Today we always validate.
             */

            /* FIXME: case 7127: for -exclude_gbop_list need to check a flag
             * whether this ntdll!Nt* hook has been excluded
             */
            /* state->xsp is the wishful thinking after syscall
             * address, instead of the original one -
             * intercept_syscall_wrapper() keeps the relevant
             * FPO information: 4 on XP SP2+, or 0 earlier
             */
            gbop_validate_and_act(state,
                                  /* adjust ESP */
                                  syscall_trampoline_gbop_fpo_offset[sysnum],
                                  syscall_trampoline_hook_pc[sysnum]);
            /* if the routine at all returns it passed the GBOP checks */

            /* FIXME: case 7127: may want alternative handling
             * and for system calls returning an error of some kind
             * like STATUS_INVALID_ADDRESS or STATUS_BUFFER_OVERFLOW
             * may be a somewhat useful attack handling alternative
             */

            /* FIXME: case 7127 for completeness should be able to add
             * this check to the regular DR syscalls where we'll be at
             * the PC calling sysenter, not necessarily at the start
             * of a function.  Though other than uniform testing it
             * won't serve much else.  There we'll have to match the
             * correct FPO offset at the syscall as well.
             */
        }
#endif /* GBOP */
        /* Notes on handling syscalls for native threads:
         *
         * FIXME: make sure each syscall handler can handle this thread being native,
         * as well as target being native.  E.g., will a native thread terminating
         * itself hit any assertion about not coming back under DR control first?
         * Another example, will GetCxt fail trying to translate a native thread's
         * context?
         * FIXME: what about asynch event while in syscall?  none of ones we
         * intercept are alertable?
         * FIXME: exception during pre-syscall sequence can cause us to miss
         * the go-native trigger!
         *
         * Be careful with cache consistency events -- we assume in general that
         * code executed natively is never mixed with code executed under DR, in
         * both execution and manipulation, and we try to have _all_ DGC-using
         * dlls listed in the native_exec_list.  We do handle write faults from
         * cache consistency in native threads, so we'll have correct behavior,
         * but we don't want a performance hit from in-cache DGC slowing down
         * from-native DGC b/c they share memory and it keeps bouncing from RO to
         * RW -- that's a big reason we're going native in the first place!  For
         * handling app memory-changing syscalls, we don't mark new code as
         * read-only until executed from, so in the common case we should not
         * incur any cost from cache consistency while native.
         */
        /* Invoke normal DR syscall-handling by calling d_r_dispatch() with a
         * linkstub_t marked just like those for fragments ending in syscalls.
         * (We cannot return to the trampoline tail for asynch_take_over() since
         * it will clobber out next_tag and last_exit and will execute the jmp
         * back to the syscall under DR, requiring a more intrusive way of going
         * native afterward.)  Normal handling may skip the syscall or do
         * whatever, but we expect it to not change control flow (we don't
         * intercept those while threads are native) and to come out of the
         * cache and continue on with the next_tag that we set here, which is a
         * special stopping point routine of ours that causes DR to go native @
         * the pc we store in dcontext->native_exec_postsyscall.
         */
        dcontext->next_tag = BACK_TO_NATIVE_AFTER_SYSCALL;
        /* start_pc is the take-over pc that will jmp to the syscall instr, while
         * we need the post-syscall pc, which we stored when generating the trampoline
         */
        ASSERT(syscall_trampoline_skip_pc[sysnum] != NULL);
        dcontext->native_exec_postsyscall = syscall_trampoline_skip_pc[sysnum];
        ASSERT(dcontext->whereami == DR_WHERE_APP);
        dcontext->whereami = DR_WHERE_TRAMPOLINE;
        set_last_exit(dcontext, (linkstub_t *)get_native_exec_syscall_linkstub());
        /* assumption: no special cleanup from tail of trampoline needed */
        transfer_to_dispatch(dcontext, &state->mc, false /*!full_DR_state*/);
        ASSERT_NOT_REACHED();
    }

    /* This routine tries to handle syscalls from DR, but will fail in some
     * cases (if the current thread has certain under_dynamo_control values) --
     * so we use our own custom wrapper rather than go through ntdll when we
     * expect going through wrapper to reach here (FIXME should do this for
     * all system calls). */
    /* i#924: this happens at exit during os_loader_exit(), and at thread init
     * when priv libs call routines we haven't yet redirected.  Best to disable
     * the syslog for clients (we still have the log warning).
     */
    STATS_INC(num_syscall_trampolines_DR);
    LOG(THREAD, LOG_SYSCALLS, 1, "WARNING: syscall_while_native: syscall from DR %s\n",
        syscall_names[sysnum]);
    return AFTER_INTERCEPT_LET_GO; /* do syscall natively */
}

static inline bool
intercept_syscall_for_thin_client(int SYSnum)
{
    if (SYSnum == SYS_CreateThread || SYSnum == SYS_CreateProcess ||
        SYSnum == SYS_CreateProcessEx || SYSnum == SYS_CreateUserProcess ||
        SYSnum == SYS_TerminateThread || /* Case 9079. */
        SYSnum == SYS_ResumeThread ||    /* i#1198: for env var propagation */
        /* case 8866: for -early_inject we must intercept NtMapViewOfSection */
        (DYNAMO_OPTION(early_inject) && SYSnum == SYS_MapViewOfSection)) {
        return true;
    }
    return false;
}

static inline bool
intercept_native_syscall(int SYSnum)
{
    ASSERT(SYSnum < TRAMPOLINE_MAX);
    if ((uint)SYSnum >= SYS_MAX + syscall_extra_idx)
        return false;
    /* Don't hook all syscalls for thin_client. */
    if (DYNAMO_OPTION(thin_client) && !intercept_syscall_for_thin_client(SYSnum))
        return false;

    if (!syscall_requires_action[SYSnum] || syscalls[SYSnum] == SYSCALL_NOT_PRESENT)
        return false;
    /* ignore control transfer system calls:
     * 1) NtCallbackReturn (assume the corresponding cb was native as well,
     *                      else we have big problems!  we could detect
     *                      by stacking up info on native cbs, if nobody ever
     *                      did an int 2b natively...not worth it for now)
     * 2) NtContinue
     * 3) NtCreateThread
     *     Ref case 5295 - Sygate hooks this nt wrapper differently then the
     *     others (@ 2nd instruction).  We only need to hook CreateThread
     *     system call for follow children from native exec threads anyways, so
     *     is easiest to just skip this one and live without that ability.
     * 4) NtWriteVirtualMemory:
     *    Case 9156/9103: we don't hook it to avoid removing
     *    our own GBOP hook, until we actually implement acting on it (case 8321)
     *
     * We do NOT ignore SetContextThread or suspension/resumption, since
     * the target could be in DR!
     */
    if (SYSnum == SYS_CallbackReturn || SYSnum == SYS_Continue ||
        (!DYNAMO_OPTION(native_exec_hook_create_thread) && SYSnum == SYS_CreateThread) ||
        SYSnum == SYS_WriteVirtualMemory)
        return false;
    return true;
}

void
init_syscall_trampolines(void)
{
    int i;
    HMODULE h = (HMODULE)get_ntdll_base();
    ASSERT(DYNAMO_OPTION(native_exec_syscalls));
    for (i = 0; i < TRAMPOLINE_MAX; i++) {
        if (intercept_native_syscall(i)) {
            byte *fpo_adjustment = NULL;
#ifdef GBOP
            fpo_adjustment = &syscall_trampoline_gbop_fpo_offset[i];
#endif

            syscall_trampoline_hook_pc[i] =
                (app_pc)d_r_get_proc_address(h, syscall_names[i]);
            syscall_trampoline_pc[i] =
                /* FIXME: would like to use static references to entry points -- yet,
                 * set of those we care about varies dynamically by platform, and
                 * we cannot include a pointer to a 2003-only Nt* entry point and
                 * avoid a loader link error on 2000, right?
                 * For now just using get_proc_address!
                 */
                intercept_syscall_wrapper(
                    &syscall_trampoline_hook_pc[i], syscall_while_native,
                    (void *)(ptr_int_t)i /* callee arg */,
                    AFTER_INTERCEPT_DYNAMIC_DECISION,
                    /* must store the skip_pc for the new d_r_dispatch()
                     * to know where to go after handling from DR --
                     * this is simpler than having trampoline
                     * pass it in as an arg to syscall_while_native
                     * or trying to decode it.
                     */
                    &syscall_trampoline_skip_pc[i],
                    /* Returns a pointer to a copy of the original
                     * first 5 bytes for removing the trampoline
                     * later. Excepting hook chaining situations
                     * this could just simply be the same as the
                     * returned syscall_trampoline_pc. */
                    &syscall_trampoline_copy_pc[i], fpo_adjustment, syscall_names[i]);
        }
    }
}

void
exit_syscall_trampolines(void)
{
    int i;
    ASSERT(DYNAMO_OPTION(native_exec_syscalls));
    for (i = 0; i < TRAMPOLINE_MAX; i++) {
        if (intercept_native_syscall(i)) {
            if (syscall_trampoline_pc[i] != NULL) {
                ASSERT(syscall_trampoline_copy_pc[i] != NULL &&
                       syscall_trampoline_hook_pc[i] != NULL);
                remove_trampoline(syscall_trampoline_copy_pc[i],
                                  syscall_trampoline_hook_pc[i]);
            } else {
                ASSERT(DYNAMO_OPTION(native_exec_hook_conflict) ==
                       HOOKED_TRAMPOLINE_NO_HOOK);
            }
        }
        DEBUG_DECLARE(else ASSERT(syscall_trampoline_pc[i] == NULL));
    }
}

#ifdef DEBUG
void
check_syscall_array_sizes()
{
    ASSERT(sizeof(windows_81_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_81_wow64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_81_x86_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_8_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_8_wow64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_8_x86_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_7_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_7_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_vista_sp1_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_vista_sp1_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_vista_sp0_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_vista_sp0_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_2003_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_XP_x64_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_XP_wow64_index) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_2003_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_XP_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_NT_sp4_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_NT_sp3_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_NT_sp0_syscalls) == sizeof(windows_2000_syscalls));
    ASSERT(sizeof(windows_2000_syscalls) / sizeof(windows_2000_syscalls[0]) ==
           sizeof(syscall_requires_action) / sizeof(syscall_requires_action[0]));
    ASSERT(sizeof(windows_2000_syscalls) / sizeof(windows_2000_syscalls[0]) ==
           sizeof(syscall_names) / sizeof(syscall_names[0]));
}

/* verify that syscall numbers match our static lists in an attempt to catch
 * changes to syscall interface across Windows patches and service packs
 */
void
check_syscall_numbers(dcontext_t *dcontext)
{
    int i;
    int sysnum;
    byte *addr;
    module_handle_t h = get_ntdll_base();
    ASSERT(h != NULL && h != INVALID_HANDLE_VALUE);
    LOG(GLOBAL, LOG_SYSCALLS, 4, "check_syscall_numbers: ntdll @ " PFX "\n", h);
    for (i = 0; i < SYS_MAX; i++) {
        if (syscalls[i] == SYSCALL_NOT_PRESENT)
            continue;
        addr = (byte *)d_r_get_proc_address(h, syscall_names[i]);
        ASSERT(addr != NULL);
        LOG(GLOBAL, LOG_SYSCALLS, 4, "\tsyscall 0x%x %s: addr " PFX "\n", i,
            syscall_names[i], addr);
        sysnum = decode_syscall_num(dcontext, addr);
        /* because of Sygate hooks can't assert sysnum is valid here */
        if (sysnum >= 0 && sysnum != syscalls[i]) {
            SYSLOG_INTERNAL_ERROR("syscall %s is really 0x%x not 0x%x\n",
                                  syscall_names[i], sysnum, syscalls[i]);
            syscalls[i] = sysnum;
            /* of course is much too late to fix if we already used via
             * NT_SYSCALL */
        }
    }
}
#endif

/* adjust region to page boundaries, since Windows lets you pass
 * non-aligned values, unlike Linux
 * e.g. a two byte cross-page request will result in a two page region
 */
static inline void
align_page_boundary(dcontext_t *dcontext, app_pc *base /* IN OUT */,
                    size_t *size /* IN OUT */)
{
    if (!ALIGNED(*base, PAGE_SIZE) || !ALIGNED(*size, PAGE_SIZE)) {
        /* need to cover all pages overlapping the region [base, base + size) */
        *size = ALIGN_FORWARD(*base + *size, PAGE_SIZE) - PAGE_START(*base);
        *base = (app_pc)PAGE_START(*base);
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "\talign_page_boundary => base=" PFX " size=" PIFX "\n", *base, *size);
    }
}

/* verifies whether target process is being created, presumably as a
 * child of the current process
 */
bool
is_newly_created_process(HANDLE process_handle)
{
    int remote_ldr_data;
    /* We check based on - trait 3) PEB.Ldr
     *    The Ldr entry is created by the running process itself later */

    /* ATTIC - rejected traits
     * trait 1) it doesn't have any threads created
     * Seems overly expensive to have no easy alternative to
     * NtQuerySystemInformation to tell there are no threads created
     * in the process, should use to verify new process since that
     * should be the rare case
     * FIXME: could at least store the last created pid and a flag indicating if
     * its thread has been created and use that as an auxiliary check
     *
     * May be easier to check the PEB
     * trait 2) PEB.ProcessParameters
     * The process parameters are available only after they have been created,
     * (in fact a good trait that a process without them has just been created,
     * yet they are created at the time the first thread's stack is needed.
     *
     * NOTE - in Vista traits 1 and 2 are no longer valid for this
     * purpose.  NtCreateUserProcess creates the first thread and sets up
     * the process parameters in addition to creating the process.  However this
     * is only used for aslr_stack so doesn't really matter that much. Trait 3
     * (the one we use) should still work anyways (and cover anyone using the legacy
     * native interface NtCreateProcess to create the process).
     */

    DODEBUG({
        /* dead end approach, this code can be removed*/

        /* invalid trait 4: shouldn't have many handles open
         * Attempted using NtQueryInformationProcess
         * ProcessHandleCount which is usually 1 on XP at the time
         * a new process is created, if it holds on all platforms
         *
         * Note unfortunately this cannot be counted on, since handles may
         * be inherited - and processes created by cygwin do inherit a lot
         * of handles.
         */
        ulong remote_process_handle_count;
        NTSTATUS res =
            get_process_handle_count(process_handle, &remote_process_handle_count);
        if (NT_SUCCESS(res)) {
            LOG(GLOBAL, LOG_ALL, 2,
                "is_newly_created_process: process " PIDFMT " has %d handles -> %s\n",
                process_id_from_handle(process_handle), remote_process_handle_count,
                remote_process_handle_count == 1 ? "NEW" : "maybe new");
        }
    });

    remote_ldr_data = get_remote_process_ldr_status(process_handle);
    if (remote_ldr_data >= 0) {
        LOG(GLOBAL, LOG_ALL, 1,
            "is_newly_created_process: process " PIDFMT " PEB->Ldr = %s\n",
            process_id_from_handle(process_handle),
            remote_ldr_data != 0 ? "initialized" : "NULL -> new process");

        return (remote_ldr_data == 0); /* new process */
    } else {
        /* xref case 9800 - can happen if the app handle lacks the rights we
         * need (in which case isn't a new process since the handle used then has
         * full rights). Get handle rights in local since won't be available in an
         * ldmp. */
        DEBUG_DECLARE(ACCESS_MASK rights = nt_get_handle_access_rights(process_handle);)
        ASSERT_CURIOSITY(get_os_version() >= WINDOWS_VERSION_VISTA &&
                         "xref case 9800, is_newly_created_process failure");
    }

    return false;
}

/* Rather than split up get_syscall_method() we have routines like these
 * to query variations
 */
bool
syscall_uses_wow64_index()
{
    ASSERT(get_syscall_method() == SYSCALL_METHOD_WOW64);
    return (get_os_version() < WINDOWS_VERSION_8);
}

bool
syscall_uses_edx_param_base()
{
    return (get_syscall_method() != SYSCALL_METHOD_WOW64 ||
            get_os_version() < WINDOWS_VERSION_8);
}

/* FIXME : For int/syscall we can just subtract 2 from the post syscall pc but for
 * sysenter we do the post-syscall ret native and therefore we've lost the
 * address of the actual syscall, but we are only going to use this for
 * certain ntdll system calls so is almost certainly the ntdll sysenter.  As
 * a hack for now we just use the address of the first system call we saw
 * (which should be ntdll's), this is good enough for detach and prob. good
 * enough for app GetThreadContext (we could just use 0x7ffe0302 but it moved
 * on xp sp2) */
#define SYSCALL_PC(dc)                                                              \
    ((get_syscall_method() == SYSCALL_METHOD_INT ||                                 \
      get_syscall_method() == SYSCALL_METHOD_SYSCALL)                               \
         ? (ASSERT(SYSCALL_LENGTH == INT_LENGTH), POST_SYSCALL_PC(dc) - INT_LENGTH) \
         : (get_syscall_method() == SYSCALL_METHOD_WOW64                            \
                ? (POST_SYSCALL_PC(dc) - CTI_FAR_ABS_LENGTH)                        \
                : get_app_sysenter_addr()))

/* since always coming from d_r_dispatch now, only need to set mcontext */
#define SET_RETURN_VAL(dc, val) get_mcontext(dc)->xax = (reg_t)(val)

/****************************************************************************
 * Thread-handle-to-id table for DrMi#1884.
 *
 * A handle from the app may not have THREAD_QUERY_INFORMATION privileges, so
 * we are forced to maintain a translation table.
 */

static generic_table_t *handle2tid_table;
#define INIT_HTABLE_SIZE_TID 6 /* should remain small */

/* Returns 0 == INVALID_THREAD_ID on failure */
static thread_id_t
handle_to_tid_lookup(HANDLE thread_handle)
{
    thread_id_t tid;
    TABLE_RWLOCK(handle2tid_table, read, lock);
    tid = (thread_id_t)generic_hash_lookup(GLOBAL_DCONTEXT, handle2tid_table,
                                           (ptr_uint_t)thread_handle);
    TABLE_RWLOCK(handle2tid_table, read, unlock);
    return tid;
}

static bool
handle_to_tid_add(HANDLE thread_handle, thread_id_t tid)
{
    TABLE_RWLOCK(handle2tid_table, write, lock);
    generic_hash_add(GLOBAL_DCONTEXT, handle2tid_table, (ptr_uint_t)thread_handle,
                     (void *)tid);
    LOG(GLOBAL, LOG_VMAREAS, 2, "handle_to_tid: thread " PFX " => %d\n", thread_handle,
        tid);
    TABLE_RWLOCK(handle2tid_table, write, unlock);
    return true;
}

static bool
handle_to_tid_remove(HANDLE thread_handle)
{
    bool found = false;
    TABLE_RWLOCK(handle2tid_table, write, lock);
    found =
        generic_hash_remove(GLOBAL_DCONTEXT, handle2tid_table, (ptr_uint_t)thread_handle);
    TABLE_RWLOCK(handle2tid_table, write, unlock);
    return found;
}

static thread_id_t
thread_handle_to_tid(HANDLE thread_handle)
{
    thread_id_t tid = handle_to_tid_lookup(thread_handle);
    if (tid == INVALID_THREAD_ID)
        tid = thread_id_from_handle(thread_handle);
    return tid;
}

static process_id_t
thread_handle_to_pid(HANDLE thread_handle, thread_id_t tid /*optional*/)
{
    if (tid == INVALID_THREAD_ID)
        tid = handle_to_tid_lookup(thread_handle);
    if (tid != INVALID_THREAD_ID) {
        /* Get a handle with more privileges */
        thread_handle = thread_handle_from_id(tid);
        process_id_t pid = process_id_from_thread_handle(thread_handle);
        close_handle(thread_handle);
        return pid;
    }
    return process_id_from_thread_handle(thread_handle);
}

void
syscall_interception_init(void)
{
    handle2tid_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_TID, 80 /* not perf-critical */,
        HASHTABLE_SHARED | HASHTABLE_PERSISTENT, NULL _IF_DEBUG("section-to-file table"));
}

void
syscall_interception_exit(void)
{
    generic_hash_destroy(GLOBAL_DCONTEXT, handle2tid_table);
}

/***************************************************************************
 * PRE SYSTEM CALL
 *
 * FIXME: should we pass mcontext to these routines to avoid
 * the get_mcontext() call and derefs?
 * => now we're forcing the inline of get_mcontext() so should be fine
 */

static reg_t *
pre_system_call_param_base(priv_mcontext_t *mc)
{
#ifdef X64
    reg_t *param_base = (reg_t *)mc->xsp;
#else
    /* On Win8, wow64 syscalls do not point edx at the params and
     * instead simply use esp.
     */
    reg_t *param_base = (reg_t *)(syscall_uses_edx_param_base() ? mc->xdx : mc->xsp);
#endif
    param_base += (SYSCALL_PARAM_OFFSET() / sizeof(reg_t));
    return param_base;
}

/* NtCreateProcess, NtCreateProcessEx */
static void
presys_CreateProcess(dcontext_t *dcontext, reg_t *param_base, bool ex)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE *process_handle = (HANDLE *)sys_param(dcontext, param_base, 0);
    uint access_mask = (uint)sys_param(dcontext, param_base, 1);
    uint attributes = (uint)sys_param(dcontext, param_base, 2);
    uint inherit_from_process = (uint)sys_param(dcontext, param_base, 3);
    BOOLEAN inherit_handles_only = (BOOLEAN)sys_param(dcontext, param_base, 4);
    HANDLE section_handle = (HANDLE)sys_param(dcontext, param_base, 5);
    HANDLE debug_handle = (HANDLE)sys_param(dcontext, param_base, 6);
    HANDLE exception_handle = (HANDLE)sys_param(dcontext, param_base, 7);

    if (ex) {
        /* according to metasploit, others type as HANDLE unknown etc. */
        uint job_member_level = (uint)sys_param(dcontext, param_base, 8);
    }

    /* Case 9173: guard against pid reuse.  Better in post after success
     * check but not a big deal.
     * We don't do this on CreateThread b/c is_newly_created_process() is still
     * true after the first thread (one fix is to store the last created pid and
     * a flag indicating if its thread has been created and use that as an auxiliary
     * check in is_newly_created_process())
     */
    dcontext->aslr_context.last_child_padded = 0;

    DOLOG(1, LOG_SYSCALLS, {
        if (section_handle != 0) {
            app_pc base = (app_pc)get_section_address(section_handle);
            /* we will inject in post_syscall or when the first thread is about
             * to be created */
            LOG(THREAD, LOG_SYSCALLS, IF_DGCDIAG_ELSE(1, 2),
                "syscall: NtCreateProcess section @" PFX "\n", base);
            DOLOG(1, LOG_SYSCALLS, {
                char buf[MAXIMUM_PATH];
                get_module_name(base, buf, sizeof(buf));
                if (buf[0] != '\0') {
                    LOG(THREAD, LOG_SYSCALLS, 2, "\tNtCreateProcess for module %s\n",
                        buf);
                }
            });
        }
    });
}

#ifdef DEBUG
/* NtCreateUserProcess */
static void
presys_CreateUserProcess(dcontext_t *dcontext, reg_t *param_base)
{
    /* New in Vista, here's what I got reverse engineering
     * NtCreateUserProcess (11 args, using windows types)
     *
     * NtCreateUserProcess (
     *   OUT PHANDLE ProcessHandle,
     *   OUT PHANDLE ThreadHandle,
     *   IN ACCESS_MASK ProcDesiredAccess,
     *   IN ACCESS_MASK ThreadDesiredAccess,
     *   IN POBJECT_ATTRIBUTES ProcObjectAttributes,
     *   IN POBJECT_ATTRIBUTES ThreadObjectAttributes,
     *   IN uint? unknown,  [ observed 0x4 ]
     *   IN BOOL CreateSuspended, [ refers to the thread not the process ]
     *   IN PRTL_USER_PROCESS_PARAMETERS Params,
     *   INOUT proc_stuff proc,
     *   INOUT create_proc_thread_info_t *thread [ see ntdll.h ])
     * CreateProcess hardcodes 0x2000000 (== MAXIMUM_ALLOWED) for both
     * ACCESS_MASK arguments.  I've only observed NULL (== default) for the
     * OBJECT_ATTRIBUTES arguments so they are a bit of a guess, but they
     * need to be here somewhere and based on error codes I know they are
     * ptr arguments so seems quite likely esp. given the arg layout.
     *
     * where proc_stuff { \\ speculative - the 64bit differences are odd and imply
     *                    \\ more then just size changes
     *   size_t struct_size, [observed 0x48 (0x58 for 64bit)]  \\ prob. sizeof(proc_stuff)
     *   ptr_uint_t unknown_p2,       \\ OUT
     *   ptr_uint_t unknown_p3,       \\ IN/OUT
     *   OUT HANDLE file_handle, [exe file handle]
     *   OUT HANDLE section_handle, [exe section handle]
     *   uint32 unknown_p6,           \\ OUT
     *   uint32 unknown_p7,           \\ OUT
     *   uint32 unknown_p8,           \\ OUT
     *   uint32 unknown_p9,           \\ OUT
     * #ifndef X64
     *   uint32 unknown_p10,          \\ OUT
     * #endif
     *   OUT PEB *new_proc_peb,
     *   uint32 unknown_p12_p17[6],   \\ OUT
     * #ifndef X64
     *   uint32 unknown_p18,          \\ OUT
     * #endif
     * }
     */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    ACCESS_MASK proc_access_mask = (uint)sys_param(dcontext, param_base, 2);
    ACCESS_MASK thread_access_mask = (uint)sys_param(dcontext, param_base, 3);
    /* might be BOOLEAN instead?  though separate param should zero out rest */
    BOOL create_suspended = (BOOL)sys_param(dcontext, param_base, 7);
    create_proc_thread_info_t *thread_stuff = (void *)sys_param(dcontext, param_base, 10);
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);

    /* might need these in post, note CreateProcess appears to hardcode them */
    ASSERT_CURIOSITY(proc_access_mask == MAXIMUM_ALLOWED);
    ASSERT_CURIOSITY(thread_access_mask == MAXIMUM_ALLOWED);
    ASSERT_CURIOSITY(create_suspended);
    /* FIXME - NYI - if any of the above curiosities don't hold we should
     * change them here and then fixup as needed in post. */

    /* Potentially dangerous deref of app ptr, but is only for debug logging */
    ASSERT(thread_stuff != NULL && thread_stuff->nt_path_to_exe.buffer != NULL);
    LOG(THREAD, LOG_SYSCALLS, 1, "syscall: NtCreateUserProcess presys %.*S\n",
        MIN(MAXIMUM_PATH, thread_stuff->nt_path_to_exe.buffer_size),
        (wchar_t *)thread_stuff->nt_path_to_exe.buffer);

    /* The thread can be resumed inside the kernel so ideally we would
     * insert the DR env vars into the pp param here (i#349).
     * However, no matter what I do, the syscall returns STATUS_INVALID_PARAMETER.
     * I made a complete copy of pp and updated the unicode pointers so it's
     * all contiguous, but still the error.  Perhaps it must be on the app heap?
     * In any case, kernel32!CreateProcess is hardcoding that the thread be
     * suspended (presumably to do its csrss and other inits safely) so we rely
     * on seeing NtResumeThread.
     */
}
#endif

/* NtCreateThread */
static void
presys_CreateThread(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE *thread_handle = (HANDLE *)sys_param(dcontext, param_base, 0);
    uint access_mask = (uint)sys_param(dcontext, param_base, 1);
    uint attributes = (uint)sys_param(dcontext, param_base, 2);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 3);
    uint *client_id = (uint *)sys_param(dcontext, param_base, 4);
    CONTEXT *cxt = (CONTEXT *)sys_param(dcontext, param_base, 5);
    USER_STACK *stack = (USER_STACK *)sys_param(dcontext, param_base, 6);
    BOOLEAN suspended = (BOOLEAN)sys_param(dcontext, param_base, 7);
    DEBUG_DECLARE(process_id_t pid = process_id_from_handle(process_handle);)
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
        "syscall: NtCreateThread pid=" PFX " suspended=%d\n", pid, suspended);
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
        "\tstack: " PFX " " PFX " " PFX " " PFX " " PFX "\n", stack->FixedStackBase,
        stack->FixedStackLimit, stack->ExpandableStackBase, stack->ExpandableStackLimit,
        stack->ExpandableStackBottom);
    /* According to Nebbett, in eax is the win32 start address
     * (stored in ThreadQuerySetWin32StartAddress slot, though that
     * is reused by the os, so might not be the same later) and eax is used
     * by the thread start kernel32 thunk.  It also appears from the thunk
     * that the argument to the thread start function is in ebx */
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
        "\tesp=" PFX ", xip=" PFX "\n\tstart address " PFX " with arg " PFX "\n",
        cxt->CXT_XSP, cxt->CXT_XIP, cxt->CXT_XAX, cxt->CXT_XBX);
    DOLOG(2, LOG_SYSCALLS | LOG_THREADS, {
        char buf[MAXIMUM_PATH];
        print_symbolic_address((app_pc)cxt->CXT_XAX, buf, sizeof(buf), false);
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
            "\tsymbol info for start address : %s\n", buf);
    });
    ASSERT(cxt != NULL);
    /* if not early injecting, we will unsafely modify cxt (for late follow
     * children) FIXME
     * if not injecting at all we won't change cxt.
     */
    maybe_inject_into_process(dcontext, process_handle, thread_handle, cxt);

    if (is_phandle_me(process_handle))
        pre_second_thread();
}

/* NtCreateThreadEx */
static void
presys_CreateThreadEx(dcontext_t *dcontext, reg_t *param_base)
{
    /* New in Vista, here's what I got reverse engineering NtCreateThreadEx
     * (11 args, using windows types)
     *
     * NtCreateThreadEx (
     *   OUT PHANDLE ThreadHandle,
     *   IN ACCESS_MASK DesiredAccess,
     *   IN POBJECT_ATTRIBUTES ObjectAttributes,
     *   IN HANDLE ProcessHandle,
     *   IN LPTHREAD_START_ROUTINE Win32StartAddress,
     *   IN LPVOID StartParameter,
     *   IN BOOL CreateSuspended,
     *   IN uint unknown, [ CreateThread hardcodes to 0 ]
     *   IN SIZE_T StackCommitSize,
     *   IN SIZE_T StackReserveSize,
     *   INOUT create_thread_info_t *thread_info [ see ntdll.h ])
     */
    DEBUG_DECLARE(priv_mcontext_t *mc = get_mcontext(dcontext);)
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 3);
    DEBUG_DECLARE(byte *start_addr = (byte *)sys_param(dcontext, param_base, 4);)
    DEBUG_DECLARE(void *start_parameter = (void *)sys_param(dcontext, param_base, 5);)
    DEBUG_DECLARE(bool create_suspended = (bool)sys_param(dcontext, param_base, 6);)
    DEBUG_DECLARE(process_id_t pid = process_id_from_handle(process_handle);)
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);

    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
        "syscall: NtCreateThread pid=" PFX " suspended=%d\n"
        "\tstart_addr=" PFX " arg=" PFX "\n",
        pid, create_suspended, start_addr, start_parameter);
    DOLOG(2, LOG_SYSCALLS | LOG_THREADS, {
        char buf[MAXIMUM_PATH];
        print_symbolic_address(start_addr, buf, sizeof(buf), false);
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
            "\tsymbol info for start address : %s\n", buf);
    });

    if (is_phandle_me(process_handle))
        pre_second_thread();
}

/* NtCreateWorkerFactory */
static void
presys_CreateWorkerFactory(dcontext_t *dcontext, reg_t *param_base)
{
    /* New in Vista.  10 args:
     * NtCreateWorkerFactory(
     *    __out PHANDLE FactoryHandle,
     *    __in ACCESS_MASK DesiredAccess,
     *    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
     *    __in HANDLE CompletionPortHandle,
     *    __in HANDLE ProcessHandle,
     *    __in PVOID StartRoutine,
     *    __in_opt PVOID StartParameter,
     *    __in_opt ULONG MaxThreadCount,
     *    __in_opt SIZE_T StackReserve,
     *    __in_opt SIZE_T StackCommit)
     */
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 4);
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);

    if (is_phandle_me(process_handle))
        pre_second_thread();
}

/***************************************************************************
 * ENV VAR PROPAGATION
 */

/* There is some overlap w/ handle_execve() in unix/os.c but not
 * quite enough to easily share this.
 */
static const char *const env_to_propagate[] = {
    DYNAMORIO_VAR_RUNUNDER, DYNAMORIO_VAR_OPTIONS,   DYNAMORIO_VAR_AUTOINJECT,
    DYNAMORIO_VAR_LOGDIR,   DYNAMORIO_VAR_CONFIGDIR,
};
static const wchar_t *const wenv_to_propagate[] = {
    L_DYNAMORIO_VAR_RUNUNDER, L_DYNAMORIO_VAR_OPTIONS,   L_DYNAMORIO_VAR_AUTOINJECT,
    L_DYNAMORIO_VAR_LOGDIR,   L_DYNAMORIO_VAR_CONFIGDIR,
};
#define NUM_ENV_TO_PROPAGATE (sizeof(env_to_propagate) / sizeof(env_to_propagate[0]))

/* Read env var from remote process:
 * - return true on read successfully or until end of reading
 * - skip DR env vars
 * Handles both 32-bit and 64-bit remote processes.
 */
static uint64
get_process_env_var(HANDLE phandle, uint64 env_ptr, wchar_t *buf, size_t toread)
{
    int i;
    size_t got;
    bool keep_env;
    while (true) {
        keep_env = true;
        ASSERT(toread <= (size_t)PAGE_SIZE);
        /* if an env var is too long we're ok: DR vars will fit, and if longer we'll
         * handle rest next call.
         */
        if (!read_remote_memory_maybe64(phandle, env_ptr, buf, toread, &got)) {
            /* may have crossed page boundary and the next page is inaccessible */
            uint64 start = env_ptr;
            if (PAGE_START64(start) != PAGE_START64(start + toread)) {
                ASSERT((size_t)(ALIGN_FORWARD(start, PAGE_SIZE) - start) <= toread);
                toread = (size_t)(ALIGN_FORWARD(start, PAGE_SIZE) - start);
                if (!read_remote_memory_maybe64(phandle, env_ptr, buf, toread, &got))
                    return 0;
            } else
                return 0;
            continue;
        }
        buf[got / sizeof(buf[0]) - 1] = '\0';
        if (buf[0] == '\0')
            return env_ptr;
        for (i = 0; i < NUM_ENV_TO_PROPAGATE; i++) {
            /* if conflict between env and cfg, we use cfg */
            if (wcsncmp(wenv_to_propagate[i], buf, wcslen(wenv_to_propagate[i])) == 0) {
                keep_env = false;
            }
        }
        if (keep_env)
            return env_ptr;
        env_ptr += (wcslen(buf) + 1) * sizeof(wchar_t);
    }
    return 0;
}

/* called at presys-ResumeThread to append DR env vars in the target process PEB */
static bool
add_dr_env_vars(dcontext_t *dcontext, HANDLE phandle, uint64 env_ptr, bool peb_is_32)
{
    union {
        uint64 base64;
        uint base32;
    } env_base;
    uint64 env, cur;
    size_t tot_sz = 0, app_sz, sz;
    size_t got;
    wchar_t *new_env = NULL;
    wchar_t buf[MAX_OPTIONS_STRING];
    bool need_var[NUM_ENV_TO_PROPAGATE];
    size_t sz_var[NUM_ENV_TO_PROPAGATE];
    NTSTATUS res;
    uint old_prot = PAGE_NOACCESS;
    int i, num_propagate = 0;

    for (i = 0; i < NUM_ENV_TO_PROPAGATE; i++) {
        if (get_config_val(env_to_propagate[i]) == NULL)
            need_var[i] = false;
        else {
            need_var[i] = true;
            num_propagate++;
        }
    }
    if (num_propagate == 0) {
        LOG(THREAD, LOG_SYSCALLS, 2, "%s: no DR env vars to propagate\n", __FUNCTION__);
        return true; /* nothing to do */
    }

    ASSERT(env_ptr != 0);
    if (!read_remote_memory_maybe64(phandle, env_ptr, &env_base, sizeof(env_base), NULL))
        goto add_dr_env_failure;
    env = peb_is_32 ? env_base.base32 : env_base.base64;
    if (env != 0) {
        /* compute size of current env block, and check for existing DR vars */
        cur = env;
        while (true) {
            /* for simplicity we do a syscall for each var */
            cur = get_process_env_var(phandle, cur, buf, sizeof(buf));
            if (cur == 0)
                return false;
            if (buf[0] == '\0')
                break;
            tot_sz += wcslen(buf) + 1;
            cur += (wcslen(buf) + 1) * sizeof(wchar_t);
        }
        tot_sz++; /* final 0 marking end */
        /* from here on out, all *sz vars are total bytes, not wchar_t elements */
        tot_sz *= sizeof(wchar_t);
    }
    app_sz = tot_sz;
    LOG(THREAD, LOG_SYSCALLS, 2, "%s: orig app env vars at 0x%I64x-0x%I64x\n",
        __FUNCTION__, env, env + app_sz / sizeof(wchar_t));

    /* calculate size needed for adding DR env vars.
     * for each var, we truncate if too big for buf.
     */
    for (i = 0; i < NUM_ENV_TO_PROPAGATE; i++) {
        if (need_var[i]) {
            sz_var[i] = wcslen(wenv_to_propagate[i]) +
                strlen(get_config_val(env_to_propagate[i])) + 2 /*=+0*/;
            if (sz_var[i] > BUFFER_SIZE_ELEMENTS(buf)) {
                SYSLOG_INTERNAL(SYSLOG_WARNING, "truncating DR env var for child");
                sz_var[i] = BUFFER_SIZE_ELEMENTS(buf);
            }
            sz_var[i] *= sizeof(wchar_t);
            tot_sz += sz_var[i];
        }
    }
    /* Allocate a new env block and copy over the old.
     * We're fine being limited to low addresses for parent32 child64
     * (NtWow64AllocateVirtualMemory64 is win8+ only).
     * That means we can also use the regular write, protect, and free calls below
     * for the new block (but not the original PEB addresses).
     */
    res = nt_remote_allocate_virtual_memory(phandle, &new_env, tot_sz, PAGE_READWRITE,
                                            MEM_COMMIT);
    if (!NT_SUCCESS(res)) {
        LOG(THREAD, LOG_SYSCALLS, 2, "%s: failed to allocate new env " PIFX "\n",
            __FUNCTION__, res);
        goto add_dr_env_failure;
    }
    LOG(THREAD, LOG_SYSCALLS, 2, "%s: new app env vars allocated at " PFX "-" PFX "\n",
        __FUNCTION__, new_env, new_env + tot_sz / sizeof(wchar_t));
    cur = env;
    sz = 0;
    while (true) {
        /* for simplicity we do a syscall for each var */
        size_t towrite = 0;
        cur = get_process_env_var(phandle, cur, buf, sizeof(buf));
        if (cur == 0)
            goto add_dr_env_failure;
        if (buf[0] == '\0')
            break;
        towrite = (wcslen(buf) + 1);
        res = nt_raw_write_virtual_memory(phandle, new_env + sz / sizeof(wchar_t), buf,
                                          towrite * sizeof(wchar_t), &got);
        if (!NT_SUCCESS(res)) {
            LOG(THREAD, LOG_SYSCALLS, 2,
                "%s copy: got status " PFX ", wrote " PIFX " vs requested " PIFX "\n",
                __FUNCTION__, res, got, towrite);
            goto add_dr_env_failure;
        }
        sz += towrite * sizeof(wchar_t);
        cur += towrite * sizeof(wchar_t);
    }
    ASSERT(sz == app_sz - sizeof(wchar_t) /* before final 0 */);

    /* add DR env vars at the end.
     * XXX: is alphabetical sorting relied upon?  adding to end is working.
     */
    for (i = 0; i < NUM_ENV_TO_PROPAGATE; i++) {
        if (need_var[i]) {
            _snwprintf(buf, BUFFER_SIZE_ELEMENTS(buf), L"%s=%S", wenv_to_propagate[i],
                       get_config_val(env_to_propagate[i]));
            NULL_TERMINATE_BUFFER(buf);
            if (!nt_write_virtual_memory(phandle, new_env + sz / sizeof(wchar_t), buf,
                                         sz_var[i], NULL))
                goto add_dr_env_failure;
            LOG(THREAD, LOG_SYSCALLS, 2, "%s: wrote DR env var |%S| to 0x%I64x\n",
                __FUNCTION__, buf, new_env + sz / sizeof(wchar_t));
            sz += sz_var[i];
        }
    }
    ASSERT(sz == tot_sz - sizeof(wchar_t) /* before final 0 */);
    /* write final 0 */
    buf[0] = 0;
    if (!nt_write_virtual_memory(phandle, new_env + sz / sizeof(wchar_t), buf,
                                 sizeof(wchar_t), NULL))
        goto add_dr_env_failure;

    /* install new env */
    if (!remote_protect_virtual_memory_maybe64(phandle, PAGE_START64(env_ptr), PAGE_SIZE,
                                               PAGE_READWRITE, &old_prot)) {
        LOG(THREAD, LOG_SYSCALLS, 1, "%s: failed to mark 0x%I64x writable\n",
            __FUNCTION__, PAGE_START64(env_ptr));
        goto add_dr_env_failure;
    }
    union {
        uint64 ptr64;
        uint ptr32;
    } new_env_remote;
    new_env_remote.ptr64 = (uint64)new_env;
    new_env_remote.ptr32 = (uint)(ptr_uint_t)new_env;
    if (!write_remote_memory_maybe64(phandle, env_ptr, &new_env_remote, peb_is_32 ? 4 : 8,
                                     NULL))
        goto add_dr_env_failure;
    if (!remote_protect_virtual_memory_maybe64(phandle, PAGE_START64(env_ptr), PAGE_SIZE,
                                               old_prot, &old_prot)) {
        LOG(THREAD, LOG_SYSCALLS, 1, "%s: failed to restore 0x%I64x to " PIFX "\n",
            __FUNCTION__, env_ptr, old_prot);
        /* not a fatal error */
    }
    /* XXX: free the original? on Vista+ it's part of the pp alloc and
     * is on the app heap so we can't.  we could query and see if it's
     * a separate alloc.  for now we just leave it be.
     */
    LOG(THREAD, LOG_SYSCALLS, 2, "%s: installed new env " PFX " at 0x%I64x\n",
        __FUNCTION__, new_env, env_ptr);
    return true;

add_dr_env_failure:
    if (new_env != NULL) {
        if (!NT_SUCCESS(nt_remote_free_virtual_memory(phandle, new_env))) {
            LOG(THREAD, LOG_SYSCALLS, 2, "%s: unable to free new env " PFX "\n",
                __FUNCTION__, new_env);
        }
        if (old_prot != PAGE_NOACCESS) {
            if (!remote_protect_virtual_memory_maybe64(phandle, PAGE_START64(env_ptr),
                                                       PAGE_SIZE, old_prot, &old_prot)) {
                LOG(THREAD, LOG_SYSCALLS, 1,
                    "%s: failed to restore 0x%I64x to " PIFX "\n", __FUNCTION__, env_ptr,
                    old_prot);
            }
        }
    }
    return false;
}

/* If unable to find info, return false (i.e., assume it might be the
 * first thread).  Retrieves context from thread handle.
 */
static bool
not_first_thread_in_new_process(dcontext_t *dcontext, HANDLE process_handle,
                                HANDLE thread_handle)
{
#ifndef X64
    bool peb_is_32 = is_32bit_process(process_handle);
    if (!peb_is_32) {
        /* XXX: We need to use CONTEXT_64 and thread_get_context_64 for parent32,child64.
         * We only need this for pre-Vista, so just xp64, where we are not willing
         * to put much effort: for now we bail (we never supported cross-arch
         * injection in the past in any case).
         */
        REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                    get_application_pid(),
                                    "32-bit parent's 64-bit child not supported on XP");
    }
#endif
    DWORD cxt_flags = CONTEXT_DR_STATE;
    size_t bufsz = nt_get_context_size(cxt_flags);
    char *buf = (char *)heap_alloc(dcontext, bufsz HEAPACCT(ACCT_THREAD_MGT));
    CONTEXT *cxt = nt_initialize_context(buf, bufsz, cxt_flags);
    bool res = false;
    if (NT_SUCCESS(nt_get_context(thread_handle, cxt)))
        res = !is_first_thread_in_new_process(process_handle, cxt);
    heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
    return res;
}

/* The caller should already have checked should_inject_into_process().
 * The child thread should be suspended.
 * This routine directly invokes REPORT_FATAL_ERROR_AND_EXIT on errors.
 */
static void
propagate_options_via_env_vars(dcontext_t *dcontext, HANDLE process_handle,
                               HANDLE thread_handle)
{
    /* For -follow_children we propagate env vars (current
     * DYNAMORIO_RUNUNDER, DYNAMORIO_OPTIONS, DYNAMORIO_AUTOINJECT, and
     * DYNAMORIO_LOGDIR) to the child to support a simple run-all-children
     * model without requiring setting up config files for children.
     */
    uint64 peb;
    bool peb_is_32 = is_32bit_process(process_handle)
        // If x64 client targeting WOW64 we need to
        // target x64 PEB.
        IF_X64(&&!DYNAMO_OPTION(inject_x64));
    size_t sz_read;
    union {
        uint64 ptr_64;
        uint ptr_32;
    } params_ptr;
    if (process_handle == INVALID_HANDLE_VALUE) {
        REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                    get_application_pid(),
                                    "Option propagation failed to acquire child handle");
        return; /* Not reached. */
    }
    /* We have to write to the 32-bit env block for a 32-bit target process. */
#ifdef X64
    if (peb_is_32)
        peb = get_peb32(process_handle, thread_handle);
    else
#endif
        peb = get_peb_maybe64(process_handle);
    if (peb == 0) {
        REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                    get_application_pid(),
                                    "Option propagation failed to find PEB");
        close_handle(process_handle); /* Not reached. */
        return;                       /* Not reached. */
    }
    if (!read_remote_memory_maybe64(
            process_handle,
            peb +
                (peb_is_32 ? X86_PROCESS_PARAM_PEB_OFFSET : X64_PROCESS_PARAM_PEB_OFFSET),
            &params_ptr, sizeof(params_ptr), &sz_read) ||
        sz_read != sizeof(params_ptr) ||
        (peb_is_32 ? (params_ptr.ptr_32 == 0) : (params_ptr.ptr_64 == 0))) {
        REPORT_FATAL_ERROR_AND_EXIT(
            FOLLOW_CHILD_FAILED, 3, get_application_name(), get_application_pid(),
            "Option propagation failed to find ProcessParameters");
    }
    uint64 params_base = peb_is_32 ? params_ptr.ptr_32 : params_ptr.ptr_64;
    uint64 env_ptr;
    if (IF_X64(!) peb_is_32)
        env_ptr = params_base + offsetof(RTL_USER_PROCESS_PARAMETERS, Environment);
    else {
        env_ptr = params_base +
            offsetof(IF_X64_ELSE(RTL_USER_PROCESS_PARAMETERS_32,
                                 RTL_USER_PROCESS_PARAMETERS_64),
                     Environment);
    }
    LOG(THREAD, LOG_SYSCALLS, 2,
        "inserting DR env vars to child &pp->Environment=0x%I64x\n", env_ptr);
    if (!add_dr_env_vars(dcontext, process_handle, env_ptr, peb_is_32)) {
        REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                    get_application_pid(),
                                    "Option propagation failed to add DR env vars");
    }
}

/* NtResumeThread */
static void
presys_ResumeThread(dcontext_t *dcontext, reg_t *param_base)
{
    HANDLE thread_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    thread_id_t tid = thread_handle_to_tid(thread_handle);
    process_id_t pid = thread_handle_to_pid(thread_handle, tid);
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
        "syscall: NtResumeThread pid=%d tid=%d\n", pid, tid);
    if (get_os_version() < WINDOWS_VERSION_VISTA && DYNAMO_OPTION(follow_children) &&
        pid != POINTER_MAX && !is_pid_me(pid)) {
        /* For Vista+ we propagate in postsys_CreateUserProcess.  Waiting until here
         * requires not_first_thread_in_new_process() which currently does not
         * support cross-arch, so we only propagate here for pre-Vista.
         *
         * It's possible the app is explicitly resuming a thread in another
         * process and this has nothing to do with a new process: but our env
         * var insertion should be innocuous in that case.
         *
         * For pre-Vista, the initial thread is always suspended, and is either
         * resumed inside kernel32!CreateProcessW or by the app, so we should
         * always see a resume.
         */
        HANDLE process_handle = process_handle_from_id(pid);
        if (process_handle == INVALID_HANDLE_VALUE) {
            REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                        get_application_pid(),
                                        "Option propagation failed to acquire handle");
            return; /* Not reached. */
        }
        if (!should_inject_into_process(dcontext, process_handle, NULL, NULL)) {
            LOG(THREAD, LOG_SYSCALLS, 1,
                "Not injecting so not setting DR env vars in pid=" PIFX "\n", pid);
            return;
        }
        if (not_first_thread_in_new_process(dcontext, process_handle, thread_handle)) {
            LOG(THREAD, LOG_SYSCALLS, 1,
                "Not first thread so not setting DR env vars in pid=" PIFX "\n", pid);
            return;
        }
        propagate_options_via_env_vars(dcontext, process_handle, thread_handle);
        close_handle(process_handle);
    }
}

/* NtTerminateProcess */
static bool /* returns whether to execute syscall */
presys_TerminateProcess(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    NTSTATUS exit_status = (NTSTATUS)sys_param(dcontext, param_base, 1);
    LOG(THREAD, LOG_SYSCALLS, 1,
        "syscall: NtTerminateProcess handle=" PFX " pid=%d exit=%d\n", process_handle,
        process_id_from_handle((process_handle == 0) ? NT_CURRENT_PROCESS
                                                     : process_handle),
        exit_status);
    if (process_handle == 0) {
        NTSTATUS return_val;
        thread_record_t **threads;
        int num_threads;
        priv_mcontext_t mcontext;
        DEBUG_DECLARE(bool ok;)
        /* this thread won't be terminated! */
        LOG(THREAD, LOG_SYSCALLS, 2, "terminating all other threads, not this one\n");
        copy_mcontext(mc, &mcontext);
        mc->pc = SYSCALL_PC(dcontext);

        /* make sure client nudges are finished */
        wait_for_outstanding_nudges();

        /* FIXME : issues with cleaning up here what if syscall fails */
        DEBUG_DECLARE(ok =)
        synch_with_all_threads(THREAD_SYNCH_SUSPENDED_AND_CLEANED, &threads, &num_threads,
                               /* Case 6821: while we're ok to be detached, we're
                                * not ok to be reset since we won't have the
                                * last_exit flag set for coming back here (plus
                                * our kstats get off since we didn't yet enter
                                * the cache)
                                */
                               THREAD_SYNCH_VALID_MCONTEXT_NO_XFER,
                               /* if we fail to suspend a thread (e.g., privilege
                                * problems) ignore it. FIXME: retry instead? */
                               /* XXX i#2345: add THREAD_SYNCH_SKIP_CLIENT_THREAD
                                * to synch all application threads only.
                                */
                               THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
        ASSERT(ok);
        ASSERT(threads == NULL && num_threads == 0); /* We asked for CLEANED */
        copy_mcontext(&mcontext, mc);

        /* we hold the initexit lock at this point, but we cannot release
         * it, b/c a new thread waiting on it could start initializing and
         * then we'd issue the syscall and kill it while it's holding our
         * lock, causing a deadlock when the subsequent process-terminating
         * syscall comes in! (==case 4243) So, we hold the lock to issue
         * the syscall, safest to do syscall right here rather than going
         * back to handle_system_call()
         */
        /* XXX i#2346: instead of NtTerminateProcess syscall, which terminates all
         * threads, we should use synch-all to terminate app threads only and
         * delay client sideline threads termination.
         */
        return_val = nt_terminate_process_for_app(process_handle, exit_status);
        SET_RETURN_VAL(dcontext, return_val);
        LOG(THREAD, LOG_SYSCALLS, 2,
            "\tNtTerminateProcess(" PFX ", " PFX ") => " PIFX " on behalf of app\n",
            process_handle, exit_status, return_val);

        end_synch_with_all_threads(threads, num_threads, false /*no resume*/);

        return false; /* do not execute syscall -- we already did it */
    } else if (is_phandle_me((process_handle == 0) ? NT_CURRENT_PROCESS
                                                   : process_handle)) {
        /* case 10338: we don't synchall here for faster shutdown, but we have
         * to try and not crash any other threads.  FIXME: if it's rare to get here
         * w/ > 1 thread perhaps we should do the synchall.
         */
        LOG(THREAD, LOG_SYSCALLS, 2, "\tterminating process w/ %d running thread(s)\n",
            d_r_get_num_threads());
        KSTOP(pre_syscall);
        KSTOP(num_exits_dir_syscall);
        if (is_thread_currently_native(dcontext->thread_record)) {
            /* Avoid hooks on syscalls made while cleaning up: such as
             * private libraries making system lib calls
             */
            dynamo_thread_under_dynamo(dcontext);
        }
        /* FIXME: what if syscall returns w/ STATUS_PROCESS_IS_TERMINATING? */
        os_terminate_wow64_write_args(true /*process*/, process_handle, exit_status);
        cleanup_and_terminate(dcontext, syscalls[SYS_TerminateProcess],
                              /* r10, which will go to rcx in cleanup_and_terminate
                               * and back to r10 in global_do_syscall_syscall (i#1901).
                               */
                              IF_X64_ELSE(mc->r10, mc->xdx), mc->xdx,
                              true /* entire process */, 0, 0);
    }
    return true;
}

/* NtTerminateThread */
static void
presys_TerminateThread(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    /* NtTerminateThread(IN HANDLE ThreadHandle OPTIONAL, IN NTSTATUS ExitStatus) */
    HANDLE thread_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    NTSTATUS exit_status = (NTSTATUS)sys_param(dcontext, param_base, 1);
    /* need to determine which thread is being terminated
     * it's harder than you'd think -- we can get its handle but
     * the handle may have been duplicated, no way to test
     * equivalence, we have to get the thread id
     */
    thread_id_t tid;
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    ASSERT(tr != NULL);
    if (thread_handle == 0)
        thread_handle = NT_CURRENT_THREAD;
    tid = thread_handle_to_tid(thread_handle);
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 1,
        "syscall: NtTerminateThread " PFX " => tid=%d\n", thread_handle, tid);

    if (tid == 0xFFFFFFFF) {
        /* probably invalid handle, do nothing for now */
        /* FIXME: case 2573 about adding ASSERT_CURIOSITY replacing the ASSERT we had */
    } else if (tid != tr->id) {
        priv_mcontext_t mcontext;
        DEBUG_DECLARE(thread_synch_result_t synch_res;)

        copy_mcontext(mc, &mcontext);
        mc->pc = SYSCALL_PC(dcontext);

        /* Fixme : issues with cleaning up here, what if syscall fails */
        DEBUG_DECLARE(synch_res =)
        synch_with_thread(tid, true, false, THREAD_SYNCH_VALID_MCONTEXT,
                          THREAD_SYNCH_SUSPENDED_AND_CLEANED,
                          /* if we fail to suspend a thread (e.g., privilege
                           * problems) ignore it. FIXME: retry instead? */
                          THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
        ASSERT(synch_res == THREAD_SYNCH_RESULT_SUCCESS ||
               /* App could be calling on already exited thread (xref 8125)
                * or thread could have exited while we were synching.
                * FIXME - check is racy since for dr purposes the thread is
                * considered exited just before it is signaled, but is ok
                * for an assert. */
               is_thread_exited(thread_handle) == THREAD_EXITED ||
               !is_pid_me(thread_handle_to_pid(thread_handle, tid)));
        copy_mcontext(&mcontext, mc);
    } else {
        /* case 9347 - racy early thread, yet primary is not yet 'known' */
        /* we should evaluate dr_late_injected_primary_thread before
         * d_r_get_num_threads()
         */
        bool secondary = dr_injected_secondary_thread && !dr_late_injected_primary_thread;

        bool exitproc = !secondary && (is_last_app_thread() && !dynamo_exited);
        /* this should really be check_sole_thread() */
        /* FIXME: case 9461 - we may not control all threads,
         * the syscall may fail and may not be allowed to kill last thread
         */

        if (secondary) {
            SYSLOG_INTERNAL_WARNING("secondary thread terminating, primary not ready\n");
            ASSERT(!exitproc);
            ASSERT(!check_sole_thread());
        }
        ASSERT(!exitproc || check_sole_thread());

        KSTOP(pre_syscall);
        KSTOP(num_exits_dir_syscall);
        os_terminate_wow64_write_args(false /*thread*/, thread_handle, exit_status);
        cleanup_and_terminate(dcontext, syscalls[SYS_TerminateThread],
                              /* r10, which will go to rcx in cleanup_and_terminate
                               * and back to r10 in global_do_syscall_syscall (i#1901).
                               */
                              IF_X64_ELSE(mc->r10, mc->xdx), mc->xdx, exitproc, 0, 0);
    }
}

/* NtSetContextThread */
static bool
presys_SetContextThread(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE thread_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    CONTEXT *cxt = (CONTEXT *)sys_param(dcontext, param_base, 1);
    thread_id_t tid = thread_handle_to_tid(thread_handle);
    bool intercept = true;
    bool execute_syscall = true;
    /* FIXME : we are going to read and write to cxt, which may be unsafe */
    ASSERT(tid != 0xFFFFFFFF);
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
        "syscall: NtSetContextThread handle=" PFX " tid=%d cxt->Xip=" PFX " flags=" PFX
        "\n",
        thread_handle, tid, cxt->CXT_XIP, cxt->ContextFlags);
    if (d_r_get_thread_id() == tid) {
        /* Simple case when called on own thread. */
        /* FIXME i#2249 : we should handle these flags. */
        ASSERT_NOT_IMPLEMENTED(!TEST(CONTEXT_CONTROL, cxt->ContextFlags) &&
                               !TEST(CONTEXT_DEBUG_REGISTERS, cxt->ContextFlags));
        return execute_syscall;
    }
    d_r_mutex_lock(&thread_initexit_lock); /* need lock to lookup thread */
    if (intercept_asynch_for_thread(tid, false /*no unknown threads*/)) {
        priv_mcontext_t mcontext;
        thread_record_t *tr = thread_lookup(tid);
        CONTEXT *my_cxt;
        NTSTATUS res;
        const thread_synch_state_t desired_state = THREAD_SYNCH_VALID_MCONTEXT;
        DEBUG_DECLARE(thread_synch_result_t synch_res;)
        ASSERT(tr != NULL);
        SELF_PROTECT_LOCAL(tr->dcontext, WRITABLE);
        /* now ensure target thread is at a safe point when it gets reset */
        copy_mcontext(mc, &mcontext);
        mc->pc = SYSCALL_PC(dcontext);

        DEBUG_DECLARE(synch_res =)
        synch_with_thread(tid, true, true, desired_state,
                          THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT,
                          /* if we fail to suspend a thread (e.g., privilege
                           * problems) ignore it. FIXME: retry instead? */
                          THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
        ASSERT(synch_res == THREAD_SYNCH_RESULT_SUCCESS);
        copy_mcontext(&mcontext, mc);
        if (!TESTALL(CONTEXT_CONTROL /*2 bits so ALL*/, cxt->ContextFlags)) {
            /* app didn't request pc so we'd better get it now.
             * FIXME: this isn't transparent as we have to clobber
             * fields in the app cxt: should restore in post-syscall.
             */
            DWORD cxt_flags = CONTEXT_DR_STATE;
            size_t bufsz = nt_get_context_size(cxt_flags);
            char *buf = (char *)heap_alloc(dcontext, bufsz HEAPACCT(ACCT_THREAD_MGT));
            CONTEXT *alt_cxt = nt_initialize_context(buf, bufsz, cxt_flags);
            STATS_INC(num_app_setcontext_no_control);
            if (thread_get_context(tr, alt_cxt) &&
                translate_context(tr, alt_cxt, true /*set memory*/)) {
                LOG(THREAD, LOG_SYSCALLS, 2, "no CONTROL flag on original cxt:\n");
                DOLOG(3, LOG_SYSCALLS, { dump_context_info(cxt, THREAD, true); });
                cxt->ContextFlags |= CONTEXT_CONTROL;
                cxt->CXT_XIP = alt_cxt->CXT_XIP;
                cxt->CXT_XFLAGS = alt_cxt->CXT_XFLAGS;
                cxt->CXT_XSP = alt_cxt->CXT_XSP;
                cxt->CXT_XBP = alt_cxt->CXT_XBP;
                IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* Rbp not part of CONTROL */
                cxt->SegCs = alt_cxt->SegCs;
                cxt->SegSs = alt_cxt->SegSs;
                LOG(THREAD, LOG_SYSCALLS, 3, "changed cxt:\n");
                DOLOG(3, LOG_SYSCALLS, { dump_context_info(cxt, THREAD, true); });
                /* don't care about other regs -- if app didn't
                 * specify CONTEXT_INTEGER that's fine
                 */
            } else {
                /* just don't intercept: could crash us in middle of mangled
                 * sequence once we start translating there and treating them
                 * as safe spots, but for now will be ok.
                 */
                intercept = false;
                ASSERT_NOT_REACHED();
            }
            heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
        }
        if (intercept) {
            /* modify the being-set cxt so that we retain control */
            intercept_nt_setcontext(tr->dcontext, cxt);
            LOG(THREAD, LOG_SYSCALLS, 3, "final cxt passed to syscall:\n");
            DOLOG(3, LOG_SYSCALLS, { dump_context_info(cxt, THREAD, true); });
        }
        /* nt_continue_dynamo_start path assumes target is !couldbelinking
         * all synch_with_thread synch points should be, we check here
         */
        ASSERT(!is_couldbelinking(tr->dcontext));
        if (TEST(THREAD_SET_CONTEXT, nt_get_handle_access_rights(thread_handle))) {
            /* Case 10101: a thread waiting at check_wait_at_safe_spot can't
             * be directly setcontext-ed so we explicitly do the context
             * set request here and skip the system call.
             * A waiting thread does NtContinue and so bypasses permission issues,
             * so we explicitly check for setcontext permission.
             * We have to make a copy since the app could de-allocate or modify
             * cxt before a waiting thread examines it.
             */
            DEBUG_DECLARE(bool ok;)
#ifdef X64
            /* PR 263338: we need to align to 16 on x64.  Heap is 8-byte aligned. */
            byte *cxt_alloc;
#endif
            my_cxt = global_heap_alloc(CONTEXT_HEAP_SIZE(*my_cxt) HEAPACCT(ACCT_OTHER));
#ifdef X64
            cxt_alloc = (byte *)cxt;
            if (!ALIGNED(my_cxt, 16)) {
                ASSERT(ALIGNED(my_cxt, 8));
                my_cxt = (CONTEXT *)(((app_pc)my_cxt) + 8);
            }
            ASSERT(ALIGNED(my_cxt, 16));
#endif
            *my_cxt = *cxt;
            /* my_cxt is freed by set_synched_thread_context() or target thread */
            DEBUG_DECLARE(ok =)
            set_synched_thread_context(
                tr, NULL, (void *)my_cxt, CONTEXT_HEAP_SIZE(*my_cxt),
                desired_state _IF_X64(cxt_alloc) _IF_WINDOWS(&res));
            /* We just tested permissions, but could be bad handle, etc.
             * FIXME: if so and thread was waiting we have transparency violation
             */
            ASSERT_CURIOSITY(ok);
            SET_RETURN_VAL(tr->dcontext, res);
            /* must wake up thread so it can go to nt_continue_dynamo_start */
            nt_thread_resume(tr->handle, NULL);
            execute_syscall = false;
        } else {
            /* we expect the system call to fail */
            DODEBUG({ tr->dcontext->expect_last_syscall_to_fail = true; });
        }
        SELF_PROTECT_LOCAL(tr->dcontext, READONLY);
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    return execute_syscall;
}

/* NtSetInformationProcess */
static bool
presys_SetInformationProcess(dcontext_t *dcontext, reg_t *param_base)
{
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    PROCESSINFOCLASS class = (PROCESSINFOCLASS)sys_param(dcontext, param_base, 1);
    void *info = (void *)sys_param(dcontext, param_base, 2);
    ULONG info_len = (ULONG)sys_param(dcontext, param_base, 3);
    LOG(THREAD, LOG_SYSCALLS, 2, "NtSetInformationProcess %p %d %p %d\n", process_handle,
        class, info, info_len);
    if (!should_swap_teb_static_tls())
        return true;
    if (class != ProcessTlsInformation)
        return true;
    if (!is_phandle_me(process_handle)) {
        SYSLOG_INTERNAL_WARNING_ONCE("ProcessTlsInformation on another process");
        return true;
    }
    LOG(THREAD, LOG_SYSCALLS, 2, "ProcessTlsInformation: pausing all other threads\n");
    thread_record_t **threads;
    int num_threads, i;
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER,
                                &threads, &num_threads, THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                /* Ignore failures to suspend: best-effort. */
                                THREAD_SYNCH_SUSPEND_FAILURE_IGNORE)) {
        SYSLOG_INTERNAL_WARNING_ONCE("Failed to suspend for ProcessTlsInformation");
        return true;
    }
    /* Ensure each thread has the app TEB.ThreadLocalStoragePointer value. */
    bool *swapped =
        (bool *)global_heap_alloc(num_threads * sizeof(bool) HEAPACCT(ACCT_THREAD_MGT));
    for (i = 0; i < num_threads; i++) {
        if (!os_using_app_state(threads[i]->dcontext)) {
            swapped[i] = true;
            os_swap_context(threads[i]->dcontext, true /*to app*/, DR_STATE_TEB_MISC);
        } else
            swapped[i] = false;
    }
    /* We're holding the initexit lock so we can't safely enter the fcache for
     * a regular app syscall.  We instead emulate the syscall ourselves.
     * We assume it's not alertable and no callback will come in.
     */
    NTSTATUS return_val =
        nt_set_information_process_for_app(process_handle, class, info, info_len);
    SET_RETURN_VAL(dcontext, return_val);
    LOG(THREAD, LOG_SYSCALLS, 2,
        "\tNtSetInformationProcess(%p, %d, %p, %d) => %d on behalf of app\n",
        process_handle, class, info, info_len, return_val);
    /* Swap the TEB fields back to DR. */
    for (i = 0; i < num_threads; i++) {
        if (swapped[i])
            os_swap_context(threads[i]->dcontext, false /*to priv*/, DR_STATE_TEB_MISC);
    }
    global_heap_free(swapped, num_threads * sizeof(bool) HEAPACCT(ACCT_THREAD_MGT));
    end_synch_with_all_threads(threads, num_threads, true /*resume*/);
    return false;
}

/* Assumes mc is app state prior to system call.
 * Returns true iff system call is a callback return that does transfer control
 * (xref case 10579).
 */
bool
is_cb_return_syscall(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    if (mc->xax == (reg_t)syscalls[SYS_CallbackReturn]) {
        reg_t *param_base = pre_system_call_param_base(mc);
        if ((NTSTATUS)sys_param(dcontext, param_base, 2) != STATUS_CALLBACK_POP_STACK)
            return true;
    }
    return false;
}

/* NtCallbackReturn */
static void
presys_CallbackReturn(dcontext_t *dcontext, reg_t *param_base)
{
    /* args are:
     *   IN PVOID Result OPTIONAL, IN ULONG ResultLength, IN NTSTATUS Status
     * same args go to int 2b (my theory anyway), where they are passed in
     * eax, ecx, and edx.  if KiUserCallbackDispatcher returns, it leaves
     * eax w/ result value of callback, and zeros out ecx and edx, then int 2b.
     * people doing the int 2b in user32 set ecx and edx to what they want, then
     * call a routine that simply pulls first arg into eax and then does int 2b.
     */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    NTSTATUS status = (NTSTATUS)sys_param(dcontext, param_base, 2);
    if (status == STATUS_CALLBACK_POP_STACK) {
        /* case 10579: this status code instructs the kernel to only
         * pop the stack and not transfer control there */
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtCallbackReturn STATUS_CALLBACK_POP_STACK\n");
    } else {
        /* NtCallbackReturn returns from callback via a syscall, and it
         * requires us to restore the prev dcontext immediately prior
         * to the syscall (want to use current dcontext in prior instructions
         * in shared_syscall).
         * N.B.: this means that the return from the call to pre_system_call
         * uses a different dcontext than the setup for the call!
         * the popa and popf will be ok -- old dstack is still in esp, isn't
         * restored, isn't deleted by swapping to new dcontext.
         * The problem is the restore of the app's esp -- so we fix that by
         * having the clean call to pre_system_call store and restore app's esp
         * from a special nonswapped dcontext slot.
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtCallbackReturn\n");
        callback_start_return(mc);
    }
}

static void
check_for_stack_free(dcontext_t *dcontext, byte *base, size_t size)
{
    /* Ref case 5518 - on some versions of windows the thread stack is freed
     * in process.  So we watch here for the free to keep from removing again
     * at thread exit. */
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(dcontext == get_thread_private_dcontext());
    if (base == ostd->stack_base) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1, "Thread's os stack is being freed\n");
        ASSERT(base + size == ostd->stack_top);
        /* only seen the in process free on 2k and NT */
        ASSERT_CURIOSITY(get_os_version() <= WINDOWS_VERSION_2000);
        /* When we've seen it happen (in kernel32!ExitThread), ExitThread uses
         * a chunk of the TEB as the stack while freeing and calling
         * NtTerminate. */
        ASSERT_CURIOSITY((byte *)get_mcontext(dcontext)->xsp >= (byte *)get_own_teb() &&
                         (byte *)get_mcontext(dcontext)->xsp <
                             ((byte *)get_own_teb()) + PAGE_SIZE);
        /* FIXME - Instead of saying the teb stack is no longer valid, we could
         * instead change the bounds to be the TEB region.  Other users could
         * then always we assert we have something valid set. Is slightly
         * greater dependence on observed behavior though. */
        ostd->teb_stack_no_longer_valid = true;
        ostd->stack_base = NULL;
        ostd->stack_top = NULL;
    }
}

/* NtAllocateVirtualMemory */
static bool
presys_AllocateVirtualMemory(dcontext_t *dcontext, reg_t *param_base, int sysnum)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    void **pbase = (void **)sys_param(dcontext, param_base, 1);
    /* XXX i#899: NtWow64AllocateVirtualMemory64 has an extra arg after ZeroBits but
     * it's ignored in wow64!whNtWow64AllocateVirtualMemory64.  We should keep an eye
     * out: maybe a future service pack or win9 will use it.
     */
    int arg_shift = (sysnum == syscalls[SYS_Wow64AllocateVirtualMemory64] ? 1 : 0);
    size_t *psize = (size_t *)sys_param(dcontext, param_base, 3 + arg_shift);
    uint type = (uint)sys_param(dcontext, param_base, 4 + arg_shift);
    uint prot = (uint)sys_param(dcontext, param_base, 5 + arg_shift);
    app_pc base;
    if (is_phandle_me(process_handle) && TEST(MEM_COMMIT, type) &&
        /* Any overlap when asking for MEM_RESERVE (even when combined w/ MEM_COMMIT)
         * will fail anyway, so we only have to worry about overlap on plain MEM_COMMIT
         */
        !TEST(MEM_RESERVE, type)) {
        /* i#1175: NtAllocateVirtualMemory can modify prot on existing pages */
        size_t size;
        if (d_r_safe_read(pbase, sizeof(base), &base) &&
            d_r_safe_read(psize, sizeof(size), &size) && base != NULL &&
            !app_memory_pre_alloc(dcontext, base, size, osprot_to_memprot(prot),
                                  false /*!hint*/, true /*update*/, false /*!image*/)) {
            SET_RETURN_VAL(dcontext, STATUS_CONFLICTING_ADDRESSES);
            return false; /* do not execute system call */
        }
    }
#ifdef PROGRAM_SHEPHERDING
    if (is_phandle_me(process_handle) && TEST(MEM_COMMIT, type) &&
        TESTALL(PAGE_EXECUTE_READWRITE, prot)) {
        /* executable_if_alloc policy says we only add a region to the future
         * list if it is committed rwx with no prior reservation.
         * - if a base is passed and MEM_RESERVE is not set, there must be a prior
         *   reservation
         * - if a base is passed and MEM_RESERVE is set, do a query to see if
         *   reservation existed before
         * - if no base is passed, there was no reservation
         */
        /* unfortunately no way to avoid syscall to check readability
         * (unless have try...except)
         */
        if (d_r_safe_read(pbase, sizeof(base), &base)) {
            dcontext->alloc_no_reserve =
                (base == NULL ||
                 (TEST(MEM_RESERVE, type) && !get_memory_info(base, NULL, NULL, NULL)));
            /* FIXME: can one MEM_RESERVE an address previously
             * MEM_RESERVEd - at least on XP that's not allowed */
        }
    } else if (TEST(ASLR_STACK, DYNAMO_OPTION(aslr)) && !is_phandle_me(process_handle) &&
               TEST(MEM_RESERVE, type) && is_newly_created_process(process_handle)) {
        /* pre-processing of remote NtAllocateVirtualMemory reservation */
        /* Case 9173: ignore allocations with a requested base.  These may come
         * after we've inserted our pad (is_newly_created_process() isn't
         * perfect), but may also come before, and we do not want to cause
         * interop issues.  We could instead try to adjust our pad to not cause
         * their alloc to fail, but may end up eliminating any security
         * advantage anyway.
         */
        if (d_r_safe_read(pbase, sizeof(base), &base)) {
            if (base == NULL) {
                /* FIXME: make the above check stronger */
                ASSERT_CURIOSITY(prot == PAGE_READWRITE);
                /* this is just a reservation, so can be anything */

                /* currently not following child flags, so maybe is almost always */
                /* NOTE - on vista we should only ever get here if someone is using
                 * the legacy NtCreateProcess native api (vs NtCreateUserProcess) or
                 * the app is injecting memory into a new process before it's started
                 * initializing itself. */
                ASSERT_CURIOSITY(get_os_version() < WINDOWS_VERSION_VISTA);
                aslr_maybe_pad_stack(dcontext, process_handle);
            } else {
                DODEBUG({
                    if (process_id_from_handle(process_handle) !=
                        dcontext->aslr_context.last_child_padded) {
                        SYSLOG_INTERNAL_WARNING_ONCE("aslr stack: allowing alloc prior "
                                                     "to pad");
                    }
                });
            }
        }
    }
#endif /* PROGRAM_SHEPHERDING */
    return true;
}

/* NtAllocateVirtualMemoryEx */
static void
presys_AllocateVirtualMemoryEx(dcontext_t *dcontext, reg_t *param_base)
{
    /*
        FIXME i#3090: The parameters for NtAllocateVirtualMemoryEx are undocumented.
    */
    ASSERT_CURIOSITY("unimplemented pre handler for NtAllocateVirtualMemoryEx");
}

/* NtFreeVirtualMemory */
static void
presys_FreeVirtualMemory(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    void **pbase = (void **)sys_param(dcontext, param_base, 1);
    size_t *psize = (size_t *)sys_param(dcontext, param_base, 2);
    uint type = (uint)sys_param(dcontext, param_base, 3);
    app_pc base;
    size_t size;

    /* check for common argument problems, apps tend to screw this call
     * up a lot (who cares about a memory leak, esp. at process exit) */
    /* ref case 3536, 545, 4046 */
    if (!d_r_safe_read(pbase, sizeof(base), &base) || base == NULL ||
        !d_r_safe_read(psize, sizeof(size), &size) ||
        !(type == MEM_RELEASE || type == MEM_DECOMMIT)) {
        /* we expect the system call to fail */
        DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
        return;
    }

    if (!is_phandle_me(process_handle)) {
        IPC_ALERT("ERROR: FreeVirtualMemory %s " PFX " " PIFX " on another process",
                  type == MEM_DECOMMIT ? "MEM_DECOMMIT" : "MEM_RELEASE", base, size);
        return;
    }

    if ((type == MEM_DECOMMIT && size == 0) || (type == MEM_RELEASE)) {
        app_pc real_base;
        /* whole region being freed, we must look up size, ignore psize
         * msdn and Nebbet claim that you need *psize == 0 for MEM_RELEASE
         * but that doesn't seem to be true on all platforms */

        /* 2K+: if base is anywhere on the first page of region this succeeds,
         * and doesn't otherwise.
         * NT: base must be the actual base.
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "syscall: NtFreeVirtualMemory type=%s region base=" PFX " size=" PIFX "\n",
            type == MEM_DECOMMIT ? "MEM_DECOMMIT" : "MEM_RELEASE", base, size);

        size = get_allocation_size(base, &real_base);
        ASSERT(ALIGNED(real_base, PAGE_SIZE));
        /* if region has been already been freed */
        if (((app_pc)ALIGN_BACKWARD(base, PAGE_SIZE) != real_base) ||
            (get_os_version() == WINDOWS_VERSION_NT && base != real_base)) {
            /* we expect the system call to fail
             * with (NTSTATUS) 0xc000009f -
             * "Virtual memory cannot be freed as base address is not
             * the base of the region and a region size of zero was
             * specified"
             */
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "syscall: NtFreeVirtualMemory base=" PFX ", size=" PIFX " invalid base\n",
                base, size);
            DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
            return;
        }
        /* make sure we use correct region base address, */
        /* otherwise we'll free an extra page */
        base = real_base;
        ASSERT(real_base != NULL && "already freed");
    }

    DODEBUG({
        /* FIXME: this shouldn't be DODEBUG since we need to handle syscall failure */
        if (type == MEM_DECOMMIT && size != 0) {
            size_t real_size = get_allocation_size(base, NULL);
            if ((app_pc)ALIGN_BACKWARD(base, PAGE_SIZE) + real_size < base + size) {
                /* we expect the system call to fail with
                 * (NTSTATUS) 0xc000001a - "Virtual memory cannot be freed."
                 */
                DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "syscall: NtFreeVirtualMemory base=" PFX ", size=" PIFX
                    " too large should fail \n",
                    base, size);
                return;
            }
        }
    });

    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: NtFreeVirtualMemory base=" PFX " size=" PIFX "\n", base, size);
    DOLOG(1, LOG_SYSCALLS | LOG_VMAREAS, {
        char buf[MAXIMUM_PATH];
        get_module_name(base, buf, sizeof(buf));
        if (buf[0] != '\0') {
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "\tNtFreeVirtualMemory called on module %s\n", buf);
            ASSERT_CURIOSITY(false && "NtFreeVirtualMemory called on module");
            /* should switch to PE name and then can do this at loglevel 0 */
        }
    });
    DOLOG(1, LOG_MEMSTATS, {
        /* snapshots are heavyweight, so do rarely */
        if (size > SNAPSHOT_THRESHOLD)
            mem_stats_snapshot();
    });

    align_page_boundary(dcontext, &base, &size);
    ASSERT_BUG_NUM(4511, ALIGNED(base, PAGE_SIZE) && ALIGNED(size, PAGE_SIZE));
    /* ref case 5518 - we need to keep track if the thread stack is freed */
    if (type == MEM_RELEASE) {
        check_for_stack_free(dcontext, base, size);
    }
    if (type == MEM_RELEASE && TEST(ASLR_HEAP_FILL, DYNAMO_OPTION(aslr))) {
        /* We free our allocation before the application
         * reservation is released.  Not a critical failure if the
         * application free fails but we have freed our pad.  Also
         * avoids fragmentation if a racy allocation.
         */
        aslr_pre_process_free_virtual_memory(dcontext, base, size);
        /* note we handle the untracked stack free in
         * os_thread_stack_exit() */
    }

    app_memory_deallocation(dcontext, base, size,
                            false /* don't own thread_initexit_lock */,
                            false /* not image */);
}

/* NtProtectVirtualMemory */
static bool /* returns whether to execute syscall */
presys_ProtectVirtualMemory(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    void **pbase = (void **)sys_param(dcontext, param_base, 1);
    size_t *psize = (size_t *)sys_param(dcontext, param_base, 2);
    uint prot = (uint)sys_param(dcontext, param_base, 3);
    uint *oldprot = (uint *)sys_param(dcontext, param_base, 4);
    app_pc base;
    size_t size;
    uint old_memprot = MEMPROT_NONE;    /* for SUBSET_APP_MEM_PROT_CHANGE
                                         * or PRETEND_APP_MEM_PROT_CHANGE */
    uint subset_memprot = MEMPROT_NONE; /* for SUBSET_APP_MEM_PROT_CHANGE */

    if (!d_r_safe_read(pbase, sizeof(base), &base) ||
        !d_r_safe_read(psize, sizeof(size), &size)) {
        /* we expect the system call to fail */
        DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
        return true;
    }

    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: NtProtectVirtualMemory process=" PFX " base=" PFX " size=" PIFX
        " prot=%s 0x%x\n",
        process_handle, base, size, prot_string(prot), prot);
    if (is_phandle_me(process_handle)) {
        uint res;
        /* go to page boundaries, since windows lets you pass non-aligned
         * values, unlike Linux
         */
        /* FIXME: use align_page_boundary(dcontext, &base, &size) instead */
        if (!ALIGNED(base, PAGE_SIZE) || !ALIGNED(base + size, PAGE_SIZE)) {
            /* need to cover all pages between base and base + size */
            size = ALIGN_FORWARD(base + size, PAGE_SIZE) - PAGE_START(base);
            base = (app_pc)PAGE_START(base);
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "\tpage boundaries => base=" PFX " size=" PIFX "\n", base, size);
        }
        DOLOG(1, LOG_SYSCALLS | LOG_VMAREAS, {
            char module_name[MAX_MODNAME_INTERNAL];
            if (os_get_module_name_buf(base, module_name,
                                       BUFFER_SIZE_ELEMENTS(module_name)) > 0) {
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "\tNtProtectVirtualMemory called on module %s\n", module_name);
            }
        });
#ifdef DGC_DIAGNOSTICS
        DOLOG(1, LOG_VMAREAS, {
            dump_callstack(POST_SYSCALL_PC(dcontext), (app_pc)mc->xbp, THREAD,
                           DUMP_NOT_XML);
        });
#endif
        res =
            app_memory_protection_change(dcontext, base, size, osprot_to_memprot(prot),
                                         &subset_memprot, &old_memprot, false /*!image*/);
        if (res != DO_APP_MEM_PROT_CHANGE) {
            /* from experimentation it seems to return
             * STATUS_CONFLICTING_ADDRESSES
             * rather than STATUS_NOT_COMMITTED for invalid memory
             */
            if (res == FAIL_APP_MEM_PROT_CHANGE) {
                SET_RETURN_VAL(dcontext, STATUS_CONFLICTING_ADDRESSES);
            } else if (res == PRETEND_APP_MEM_PROT_CHANGE ||
                       res == SUBSET_APP_MEM_PROT_CHANGE) {
                /*
                 * FIXME: is alternative of letting it go through and undoing in
                 * post-handler simpler and safer (here we have to emulate kernel
                 * behavior), if we remove +w flag to avoid other-thread issues?
                 */
                uint pretend_oldprot;
                uint old_osprot = PAGE_NOACCESS;
                SET_RETURN_VAL(dcontext, STATUS_SUCCESS);

                if (res == SUBSET_APP_MEM_PROT_CHANGE) {
                    uint subset_osprot = osprot_replace_memprot(prot, subset_memprot);

                    /* we explicitly make our system call.  Although in this
                     * case we could change the application arguments as
                     * well, in general it is not nice to the application
                     * to change IN arguments.
                     */
                    bool ok = nt_remote_protect_virtual_memory(
                        process_handle, base, size, subset_osprot, &old_osprot);
                    /* using app's handle in case has different rights than cur thread */
                    ASSERT_CURIOSITY(process_handle == NT_CURRENT_PROCESS);
                    ASSERT_CURIOSITY(ok);
                    /* we'll keep going anyways as if it would have worked */
                } else {
                    ASSERT_NOT_TESTED();
                    ASSERT(res == PRETEND_APP_MEM_PROT_CHANGE);
                    /* pretend it worked but don't execute system call */
                    old_osprot = get_current_protection(base);
                }

                /* Today we base on the current actual flags
                 * (old_osprot), and preserve WRITECOPY and other
                 * unlikely original flags.
                 *
                 * We should be using our value for what the correct
                 * view of the application memory should be.  case
                 * 10437 we should be able to transparently carry the
                 * original protection flags across multiple calls to
                 * NtProtectVirtualMemory.
                 */
                pretend_oldprot = osprot_replace_memprot(old_osprot, old_memprot);

                /* have to set OUT vars properly */
                /* size and base were already aligned up above */
                ASSERT(ALIGNED(size, PAGE_SIZE));
                ASSERT(ALIGNED(base, PAGE_SIZE));
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "skipping NtProtectVirtualMemory, returning base=" PFX ", size=" PIFX
                    ", oldprot=%s 0x%x\n",
                    base, size, prot_string(pretend_oldprot), pretend_oldprot);

                /* FIXME: we really should be _probing_ these writes
                 * to make sure not targeting DR addresses when
                 * PROTECT_FROM_APP
                 */
                safe_write(oldprot, sizeof(pretend_oldprot), &pretend_oldprot);
                safe_write(pbase, sizeof(base), &base);
                safe_write(psize, sizeof(size), &size);
            } else {
                ASSERT_NOT_REACHED();
            }

            return false; /* do not execute system call */
        } else {
            /* FIXME i#143: we still need to tweak the returned oldprot (in
             * post-syscall) for writable areas we've made read-only
             */
            /* FIXME: ASSERT here that have not modified size unless using, e.g.
             * fix_unsafe_hooker
             */
        }
    } else {
        /* FIXME: should we try to alert any dynamo running the other process?
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "WARNING: ProtectVirtualMemory called on process " PFX " %d\n",
            process_handle, process_id_from_handle(process_handle));
        /* this actually happens (e.g., in calc.exe's winhlp popups)
         * so don't die here with IPC_ALERT
         */
    }
    return true;
}

/* NtMapViewOfSection */
static bool
presys_MapViewOfSection(dcontext_t *dcontext, reg_t *param_base)
{
    bool execute = true;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE section_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    /* trying to make sure we're tracking properly all section
     * handles
     *
     * Unfortunately SHELL32!SHChangeRegistration_Create seems
     * to be using sections to communicate with explorer.exe
     * and sends a message via sending a duplicate section
     * handle, and likely receives a message back in a
     * similarly duplicated handle from the other process.
     * Hard to match that particular call so cannot keep a
     * CURIOSITY here.
     *
     * Note we also wouldn't like some global handle being used by
     * different threads as well, or any other unusually nested
     * use of NtCreateSection/NtOpenSection before NtMapViewOfSection.
     *
     * For non-image sections accessed via OpenSection rather than CreateSection,
     * we do NOT have the file name here, but we can get it once we have a mapping
     * via MemorySectionName: plus we don't care about non-images.  But, we don't
     * have a test for image here, so we leave this LOG note.
     */
    const char *file = section_to_file_lookup(section_handle);
    if (file != NULL) {
        /* we should be able to block loads even in unknown threads */
        if (DYNAMO_OPTION(enable_block_mod_load) &&
            (!IS_STRING_OPTION_EMPTY(block_mod_load_list) ||
             !IS_STRING_OPTION_EMPTY(block_mod_load_list_default))) {
            const char *short_name = get_short_name(file);
            string_option_read_lock();
            if ((!IS_STRING_OPTION_EMPTY(block_mod_load_list) &&
                 check_filter(DYNAMO_OPTION(block_mod_load_list), short_name)) ||
                (!IS_STRING_OPTION_EMPTY(block_mod_load_list_default) &&
                 check_filter(DYNAMO_OPTION(block_mod_load_list_default), short_name))) {
                string_option_read_unlock();
                /* Modify args so call fails, stdcall so caller shouldn't care
                 * about the args being modified. FIXME - alt. we could just
                 * do the stdcall ret here (for non-takeover need to supply
                 * a dr location with a ret 4 instruction at hook time and
                 * return alt_dyn here, for takeover need to modify the
                 * interception code or pass a flag to asynch_takeover/dispath
                 * to modify the app state) */
                LOG(GLOBAL, LOG_ALL, 1, "Blocking load of module %s\n", file);
                SYSLOG_INTERNAL_WARNING_ONCE("Blocking load of module %s", file);
                execute = false;
                SET_RETURN_VAL(dcontext, STATUS_ACCESS_DENIED);
                /* With failure we shouldn't have to set any of the out vals */
            } else {
                string_option_read_unlock();
            }
        }
        dr_strfree(file HEAPACCT(ACCT_VMAREAS));
    } else if (section_handle != dcontext->aslr_context.randomized_section_handle) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "syscall: NtMapViewOfSection unusual section mapping\n");
    }

    if (TESTANY(ASLR_DLL | ASLR_MAPPED, DYNAMO_OPTION(aslr))) {
        aslr_pre_process_mapview(dcontext);
    }
    return execute;
}

static void
presys_MapViewOfSectionEx(dcontext_t *dcontext, reg_t *param_base)
{
    /*
        FIXME i#3090: The parameters for NtMapViewOfSectionEx are undocumented.
    */
    ASSERT_CURIOSITY("unimplemented pre handler for NtMapViewOfSectionEx");
}

/* NtUnmapViewOfSection{,Ex} */
static void
presys_UnmapViewOfSection(dcontext_t *dcontext, reg_t *param_base, int sysnum)
{
    /* This is what actually removes a dll from memory */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    app_pc base = (app_pc)sys_param(dcontext, param_base, 1);
    app_pc real_base;
    size_t size = get_allocation_size(base, &real_base);
    MEMORY_BASIC_INFORMATION mbi;
    if (sysnum == syscalls[SYS_UnmapViewOfSectionEx]) {
        ptr_int_t arg3 = (ptr_int_t)sys_param(dcontext, param_base, 2);
        /* FIXME i#899: new Win8 syscall w/ 3rd arg that's 0 by default.
         * We want to know when we see non-zero so we have some code to study.
         */
        ASSERT_CURIOSITY(arg3 == 0 && "i#899: unknown new param");
    }
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: NtUnmapViewOfSection " PFX " size=" PIFX "\n", base, size);

    if (!is_phandle_me(process_handle)) {
        IPC_ALERT("ERROR: UnmapViewOfSection on another process");
        return;
    }

    /* check for args we expect to fail, ref case 545, 3697, on east coast
     * xp server shell32 dllmain process attach calls kernel32
     * CreateActCtxW which ends up calling this with an unaligned pointer
     * into private memory (which is suspicously just a few bytes under
     * the base address of a recently freed mapped region) */
    /* Don't worry about the query_virtual_memory cost, we are already
     * doing a ton of them for the get_allocation_size and process_mmap
     * calls */
    if (query_virtual_memory(base, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        (mbi.Type != MEM_IMAGE && mbi.Type != MEM_MAPPED)) {
        DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
        return;
    }
    /* people don't always call with the actual base address (see east
     * coast xp server (sp1) whose uxtheme.dll CThemeSignature::
     * CalculateHash always calls this with base+0x130, is hardcoded in
     * the assembly).  OS doesn't seem to care as the syscall still
     * succeeds. */
    if (base != mbi.AllocationBase) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "syscall: NtUnmapViewOfSection real base is " PFX "\n", mbi.AllocationBase);
        base = mbi.AllocationBase;
    }

    DOLOG(1, LOG_MEMSTATS, {
        /* snapshots are heavyweight, so do rarely */
        if (size > SNAPSHOT_THRESHOLD)
            mem_stats_snapshot();
    });
    RSTATS_INC(num_app_munmaps);

    /* we have to mark before any policy processing gets started */

    /* FIXME: we could also allow MEM_MAPPED areas here, since .B
     * policies may in fact allow such to be executable areas, but
     * since we can keep track of only one, focusing on MEM_IMAGE only
     */
    if (DYNAMO_OPTION(unloaded_target_exception) && mbi.Type == MEM_IMAGE) {
        mark_unload_start(base, size);
    }

    if (TESTANY(ASLR_DLL | ASLR_MAPPED, DYNAMO_OPTION(aslr))) {
        aslr_pre_process_unmapview(dcontext, base, size);
    }
    process_mmap(dcontext, base, size, false /*unmap*/, NULL);
}

/* NtFlushInstructionCache */
static void
presys_FlushInstructionCache(dcontext_t *dcontext, reg_t *param_base)
{
    /* This syscall is from the days when Windows ran on multiple
     * architectures, but many apps still use it
     */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    app_pc base = (app_pc)sys_param(dcontext, param_base, 1);
    size_t size = (size_t)sys_param(dcontext, param_base, 2);
#ifdef PROGRAM_SHEPHERDING
    uint prot;
#endif
    /* base can be NULL, in which case size is meaningless
     * loader calls w/ NULL & 0 on rebasing -- means entire icache?
     */
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: NtFlushInstructionCache " PFX " size=" PIFX "\n", base, size);
    if (base == NULL)
        return;
    if (is_phandle_me(process_handle)) {
#ifdef DGC_DIAGNOSTICS
        DOLOG(1, LOG_VMAREAS, {
            dump_callstack(POST_SYSCALL_PC(dcontext), (app_pc)mc->xbp, THREAD,
                           DUMP_NOT_XML);
        });
#endif
#ifdef PROGRAM_SHEPHERDING
        prot = osprot_to_memprot(get_current_protection(base));
        app_memory_flush(dcontext, base, size, prot);
#endif
    } else {
        /* FIXME: should we try to alert any dynamo running the other process?
         * no reason to ASSERT here, not critical like alloc/dealloc in other process
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "WARNING: NtFlushInstructionCache on another process\n");
    }
}

/* NtCreateSection */
static void
presys_CreateSection(dcontext_t *dcontext, reg_t *param_base)
{
    /* a section is an object that can be mmapped */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE *section_handle = (HANDLE *)sys_param(dcontext, param_base, 0);
    uint access_mask = (uint)sys_param(dcontext, param_base, 1);
    POBJECT_ATTRIBUTES obj = (POBJECT_ATTRIBUTES)sys_param(dcontext, param_base, 2);
    void *size = (void *)sys_param(dcontext, param_base, 3);
    uint protect = (uint)sys_param(dcontext, param_base, 4);
    uint attributes = (uint)sys_param(dcontext, param_base, 5);
    HANDLE file_handle = (HANDLE)sys_param(dcontext, param_base, 6);
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "syscall: NtCreateSection protect 0x%x, attributes 0x%x, file " PIFX "\n",
        protect, attributes, file_handle);

    DODEBUG({
        if (obj != NULL && obj->ObjectName != NULL) {
            DEBUG_DECLARE(char buf[MAXIMUM_PATH];)
            /* convert name from unicode to ansi */
            wchar_t *name = obj->ObjectName->Buffer;
            _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%S", name);
            NULL_TERMINATE_BUFFER(buf);
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "syscall: NtCreateSection %s\n",
                buf);
        } else {
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "syscall: NtCreateSection\n");
        }
    });
}

/* NtClose */
static void
presys_Close(dcontext_t *dcontext, reg_t *param_base)
{
    HANDLE handle = (HANDLE)sys_param(dcontext, param_base, 0);
    if (DYNAMO_OPTION(track_module_filenames)) {
        if (section_to_file_remove(handle)) {
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "syscall: NtClose of section handle " PFX "\n", handle);
        }
    }
    if (handle_to_tid_remove(handle)) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "syscall: NtClose of thread handle " PFX "\n", handle);
    }
}

#ifdef DEBUG
/* NtOpenFile */
static void
presys_OpenFile(dcontext_t *dcontext, reg_t *param_base)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE *file_handle = (HANDLE *)sys_param(dcontext, param_base, 0);
    uint access_mask = (uint)sys_param(dcontext, param_base, 1);
    POBJECT_ATTRIBUTES obj = (POBJECT_ATTRIBUTES)sys_param(dcontext, param_base, 2);
    void *status = (void *)sys_param(dcontext, param_base, 3);
    uint share = (uint)sys_param(dcontext, param_base, 4);
    uint options = (uint)sys_param(dcontext, param_base, 5);
    if (obj != NULL) {
        /* convert name from unicode to ansi */
        char buf[MAXIMUM_PATH];
        wchar_t *name = obj->ObjectName->Buffer;
        /* not always null-terminated */
        _snprintf(buf,
                  MIN(obj->ObjectName->Length / sizeof(obj->ObjectName->Buffer[0]),
                      BUFFER_SIZE_ELEMENTS(buf)),
                  "%S", name);
        NULL_TERMINATE_BUFFER(buf);
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "syscall: NtOpenFile %s\n", buf);
    } else {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "syscall: NtOpenFile\n");
    }
}
#endif

int
os_normalized_sysnum(int num_raw, instr_t *gateway, dcontext_t *dcontext_live)
{
    return num_raw;
}

/* WARNING: flush_fragments_and_remove_region assumes that pre and post system
 * call handlers do not examine or modify fcache or its fragments in any
 * way except for calling flush_fragments_and_remove_region!
 */
bool
pre_system_call(dcontext_t *dcontext)
{
    bool execute_syscall = true;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    int sysnum = (int)mc->xax;
    reg_t *param_base = pre_system_call_param_base(mc);
    dr_where_am_i_t old_whereami = dcontext->whereami;
    dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;
    /* XXX i#49: mc->rax's top bits are non-zero in 32-bit mode for
     * reasons we do not yet understand.
     * For now we disable the assert for mixed-mode.
     */
    IF_X64(
        ASSERT(is_wow64_process(NT_CURRENT_PROCESS) || CHECK_TRUNCATE_TYPE_int(mc->xax)));
    DODEBUG(dcontext->expect_last_syscall_to_fail = false;);

    KSTART(pre_syscall);
    RSTATS_INC(pre_syscall);
    DOSTATS({
        if (ignorable_system_call(sysnum, NULL, dcontext))
            STATS_INC(pre_syscall_ignorable);
    });
    LOG(THREAD, LOG_SYSCALLS, 2, "system call: sysnum = " PFX ", param_base = " PFX "\n",
        sysnum, param_base);

#ifdef DEBUG
    DOLOG(2, LOG_SYSCALLS, { dump_mcontext(mc, THREAD, false /*not xml*/); });
    /* we can't pass other than a numeric literal anymore */
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 0: " PFX "\n",
        sys_param(dcontext, param_base, 0));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 1: " PFX "\n",
        sys_param(dcontext, param_base, 1));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 2: " PFX "\n",
        sys_param(dcontext, param_base, 2));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 3: " PFX "\n",
        sys_param(dcontext, param_base, 3));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 4: " PFX "\n",
        sys_param(dcontext, param_base, 4));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 5: " PFX "\n",
        sys_param(dcontext, param_base, 5));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 6: " PFX "\n",
        sys_param(dcontext, param_base, 6));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 7: " PFX "\n",
        sys_param(dcontext, param_base, 7));
    LOG(THREAD, LOG_SYSCALLS, 3, "\tparam 8: " PFX "\n",
        sys_param(dcontext, param_base, 8));
    DOLOG(3, LOG_SYSCALLS, {
        /* ebp isn't in mcontext right now, so pass ebp */
        dump_callstack(POST_SYSCALL_PC(dcontext), (app_pc)mc->xbp, THREAD, DUMP_NOT_XML);
    });
#endif

    /* save key register values for post_system_call (they get clobbered
     * in syscall itself)
     * FIXME: our new stateless asynch handling means that these values
     * are wrong when we finally return to an interrupted syscall, so post-processing
     * looks at the wrong system call!
     * Fortunately it always looks at NtContinue, and we haven't yet implemented
     * NtContinue failure
     * We need fields analogous to asynch_target: asynch_sys_num and
     * asynch_param_base.  Unlike callbacks only one outstanding return-to point
     * can exist.  Let's do this when we go and make our syscall failure handling
     * more robust.  (This is case 1501)
     */
    dcontext->sys_num = sysnum;
    dcontext->sys_param_base = param_base;
#ifdef X64
    /* save params that are in registers */
    dcontext->sys_param0 = sys_param(dcontext, param_base, 0);
    dcontext->sys_param1 = sys_param(dcontext, param_base, 1);
    dcontext->sys_param2 = sys_param(dcontext, param_base, 2);
    dcontext->sys_param3 = sys_param(dcontext, param_base, 3);
#endif

    if (sysnum == syscalls[SYS_Continue]) {
        CONTEXT *cxt = (CONTEXT *)sys_param(dcontext, param_base, 0);
        /* FIXME : we are going to read and write to cxt, which may be unsafe */
        int flag = (int)sys_param(dcontext, param_base, 1);
        LOG(THREAD, LOG_SYSCALLS | LOG_ASYNCH, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtContinue cxt->Xip=" PFX " flag=" PFX "\n", cxt->CXT_XIP, flag);
        intercept_nt_continue(cxt, flag);
    } else if (sysnum == syscalls[SYS_CallbackReturn]) {
        presys_CallbackReturn(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_SetContextThread]) {
        execute_syscall = presys_SetContextThread(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_CreateProcess]) {
        presys_CreateProcess(dcontext, param_base, false /*!Ex*/);
    } else if (sysnum == syscalls[SYS_CreateProcessEx]) {
        presys_CreateProcess(dcontext, param_base, true /*Ex*/);
    }
#ifdef DEBUG
    else if (sysnum == syscalls[SYS_CreateUserProcess]) {
        presys_CreateUserProcess(dcontext, param_base);
    }
#endif
    else if (sysnum == syscalls[SYS_CreateThread]) {
        presys_CreateThread(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_CreateThreadEx]) {
        presys_CreateThreadEx(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_CreateWorkerFactory]) {
        presys_CreateWorkerFactory(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_SuspendThread]) {
        HANDLE thread_handle = (HANDLE)sys_param(dcontext, param_base, 0);
        thread_id_t tid = thread_handle_to_tid(thread_handle);
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtSuspendThread tid=%d\n", tid);
        if (SELF_PROTECT_ON_CXT_SWITCH) {
            /* This thread must make it back out of the cache for post-syscall
             * processing, regardless of what locks target thread holds at
             * suspension point, so we have to turn off our cxt switch hooks
             * (see case 4942)
             */
            dcontext->ignore_enterexit = true;
        }
    } else if (sysnum == syscalls[SYS_ResumeThread]) {
        presys_ResumeThread(dcontext, param_base);
    }
#ifdef DEBUG
    else if (sysnum == syscalls[SYS_AlertResumeThread]) {
        HANDLE thread_handle = (HANDLE)sys_param(dcontext, param_base, 0);
        thread_id_t tid = thread_handle_to_tid(thread_handle);
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtAlertResumeThread tid=%d\n", tid);
    }
#endif
    else if (sysnum == syscalls[SYS_TerminateProcess]) {
        execute_syscall = presys_TerminateProcess(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_TerminateThread]) {
        presys_TerminateThread(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_SetInformationProcess]) {
        execute_syscall = presys_SetInformationProcess(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_AllocateVirtualMemory] ||
               /* i#899: new win8 syscall w/ similar params to NtAllocateVirtualMemory */
               sysnum == syscalls[SYS_Wow64AllocateVirtualMemory64]) {
        execute_syscall = presys_AllocateVirtualMemory(dcontext, param_base, sysnum);
    } else if (sysnum == syscalls[SYS_AllocateVirtualMemoryEx]) {
        presys_AllocateVirtualMemoryEx(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_FreeVirtualMemory]) {
        KSTART(pre_syscall_free);
        presys_FreeVirtualMemory(dcontext, param_base);
        KSTOP(pre_syscall_free);
    } else if (sysnum == syscalls[SYS_ProtectVirtualMemory]) {
        KSTART(pre_syscall_protect);
        execute_syscall = presys_ProtectVirtualMemory(dcontext, param_base);
        KSTOP(pre_syscall_protect);
    } else if (sysnum == syscalls[SYS_WriteVirtualMemory]) {
        /* FIXME NYI: case 8321: need to watch for cache consistency
         * FIXME case 9103: note that we don't hook this for native_exec yet
         */
    } else if (sysnum == syscalls[SYS_MapViewOfSection]) {
        execute_syscall = presys_MapViewOfSection(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_MapViewOfSectionEx]) {
        presys_MapViewOfSectionEx(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_UnmapViewOfSection] ||
               sysnum == syscalls[SYS_UnmapViewOfSectionEx]) {
        KSTART(pre_syscall_unmap);
        presys_UnmapViewOfSection(dcontext, param_base, sysnum);
        KSTOP(pre_syscall_unmap);
    } else if (sysnum == syscalls[SYS_FlushInstructionCache]) {
        presys_FlushInstructionCache(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_CreateSection]) {
        presys_CreateSection(dcontext, param_base);
    } else if (sysnum == syscalls[SYS_Close]) {
        presys_Close(dcontext, param_base);
    }
#ifdef DEBUG
    /* FIXME: move this stuff to an strace-like client, not needed
     * for core DynamoRIO (at least not that we know of)
     */
    else if (sysnum == syscalls[SYS_OpenFile]) {
        presys_OpenFile(dcontext, param_base);
    }
#endif
    /* Address Windowing Extensions (win2k only):
     * swap pieces of memory in and out of virtual address space
     * => we must intercept when virtual addresses could point to something new
     */
    else if (sysnum == syscalls[SYS_FreeUserPhysicalPages]) {
        HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 0);
        uint *num_pages = (uint *)sys_param(dcontext, param_base, 1);
        uint *page_frame_nums = (uint *)sys_param(dcontext, param_base, 2);
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, IF_DGCDIAG_ELSE(1, 2),
            "syscall: NtFreeUserPhysicalPages %d pages\n", num_pages);
        /* FIXME: need to know base if currently mapped, must
         * record every mapping to do so
         */
        SYSLOG_INTERNAL_WARNING_ONCE(PRODUCT_NAME " is using un-supported "
                                                  "Address Windowing Extensions");
    } else if (sysnum == syscalls[SYS_MapUserPhysicalPages]) {
        app_pc base = (app_pc)sys_param(dcontext, param_base, 0);
        uint *pnum_pages = (uint *)sys_param(dcontext, param_base, 1);
        uint *page_frame_nums = (uint *)sys_param(dcontext, param_base, 2);
        uint num_pages;
        if (d_r_safe_read(pnum_pages, sizeof(num_pages), &num_pages)) {
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, IF_DGCDIAG_ELSE(1, 2),
                "syscall: NtMapUserPhysicalPages " PFX " pages=%d\n", base, num_pages);
            base = proc_get_containing_page(base);
            app_memory_deallocation(dcontext, base, num_pages * PAGE_SIZE,
                                    false /* don't own thread_initexit_lock */,
                                    false /* not image */);
        } else {
            DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
            goto exit_pre_system_call;
        }
    } else if (sysnum == syscalls[SYS_SetInformationVirtualMemory]) {
        /* XXX i#899: new Win8 syscall.  So far we've observed calls from
         * KERNELBASE!PrefetchVirtualMemory and we see that
         * KERNELBASE!SetProcessValidCallTargets calls the syscall for CFG security
         * feature purposes, neither of which should concern us, so we ignore it for
         * now.
         */
    } else if (sysnum == syscalls[SYS_RaiseException]) {
        check_app_stack_limit(dcontext);
        /* FIXME i#1691: detect whether we're inside SEH handling already, in which
         * case this process is about to die by this secondary exception and
         * we want to do a normal exit and give the client a chance to clean up.
         */
    }

exit_pre_system_call:
    dcontext->whereami = old_whereami;
    KSTOP(pre_syscall);
    return execute_syscall;
}

/***************************************************************************
 * POST SYSTEM CALL
 */

/* NtCreateUserProcess */
static void
postsys_CreateUserProcess(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* See notes in presys_CreateUserProcess for information on signature
     * of NtCreateUserProcess. */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE *proc_handle_ptr = (HANDLE *)postsys_param(dcontext, param_base, 0);
    HANDLE *thread_handle_ptr = (HANDLE *)postsys_param(dcontext, param_base, 1);
    BOOL create_suspended = (BOOL)postsys_param(dcontext, param_base, 7);
    HANDLE proc_handle, thread_handle;
    /* FIXME should have type for this */
    DEBUG_DECLARE(
        create_proc_thread_info_t *thread_stuff =
            (create_proc_thread_info_t *)postsys_param(dcontext, param_base, 10);)
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);

    LOG(THREAD, LOG_SYSCALLS, 1, "syscall: NtCreateUserProcess => " PIFX "\n", mc->xax);
    DOLOG(1, LOG_SYSCALLS, {
        if (success) {
            PCLIENT_ID client_id;
            ASSERT(thread_stuff != NULL && thread_stuff->client_id.buffer != NULL);
            /* Potentially dangerous deref of app ptr,
             * but is only for debug logging */
            client_id = (PCLIENT_ID)thread_stuff->client_id.buffer;
            LOG(THREAD, LOG_SYSCALLS, 1,
                "syscall: NtCreateUserProcess created process " PIFX " "
                "with main thread " PIFX "\n",
                client_id->UniqueProcess, client_id->UniqueThread);
        }
    });

    /* Even though syscall succeeded we use safe_read to be sure */
    if (!success || !d_r_safe_read(proc_handle_ptr, sizeof(proc_handle), &proc_handle) ||
        !d_r_safe_read(thread_handle_ptr, sizeof(thread_handle), &thread_handle))
        return;

    /* Case 9173: guard against pid reuse */
    dcontext->aslr_context.last_child_padded = 0;

    ACCESS_MASK rights = nt_get_handle_access_rights(proc_handle);
    if (!TESTALL(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                     PROCESS_QUERY_INFORMATION,
                 rights)) {
        LOG(THREAD, LOG_SYSCALLS, 1,
            "syscall: NtCreateUserProcess unable to get sufficient rights"
            " to follow children\n");
        /* This happens for Vista protected processes (drm). xref 8485 */
        /* FIXME - could check against executable file name from
         * thread_stuff to see if this was a process we're configured to
         * protect. */
        /* XXX: Should we make this a fatal release build error? */
        SYSLOG_INTERNAL_WARNING("Insufficient permissions to examine "
                                "child process\n");
    }
    if (!create_suspended) {
        /* For Vista+ NtCreateUserProcess has suspend as a
         * param and ideally we should replace the env pre-NtCreateUserProcess,
         * but we have yet to get that to work, so for now we rely on
         * Vista+ process creation going through the kernel32 routines,
         * which do hardcode the thread as being suspended.
         * TODO: We should change the parameter to ensure the thread is suspended.
         */
        LOG(THREAD, LOG_SYSCALLS, 1,
            "syscall: NtCreateUserProcess first thread not suspended "
            "can't safely follow children.\n");
        REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                    get_application_pid(),
                                    "Child thread not created suspended");
    }
    DWORD cxt_flags = CONTEXT_DR_STATE;
    size_t bufsz = nt_get_context_size(cxt_flags);
    char *buf = (char *)heap_alloc(dcontext, bufsz HEAPACCT(ACCT_THREAD_MGT));
    CONTEXT *context;
    CONTEXT *cxt = NULL;
    int res;
    /* Since this syscall is vista+ only, whether a wow64 process
     * has no bearing (xref i#381)
     */
    ASSERT(get_os_version() >= WINDOWS_VERSION_VISTA);
    if (!DYNAMO_OPTION(early_inject)) {
        /* If no early injection we have to do thread injection, and
         * on Vista+ we don't see the NtCreateThread so we do it here.  PR 215423.
         */
        context = nt_initialize_context(buf, bufsz, cxt_flags);
        res = nt_get_context(thread_handle, context);
        if (NT_SUCCESS(res))
            cxt = context;
        else {
            /* FIXME i#49: cross-arch injection can end up here w/
             * STATUS_INVALID_PARAMETER.  Need to use proper platform's
             * CONTEXT for target.
             */
            DODEBUG({
                if (is_wow64_process(NT_CURRENT_PROCESS) &&
                    !is_wow64_process(proc_handle)) {
                    SYSLOG_INTERNAL_WARNING_ONCE(
                        "Injecting from 32-bit into 64-bit "
                        "is not supported for -no_early_inject.");
                }
            });
            LOG(THREAD, LOG_SYSCALLS, 1,
                "syscall: NtCreateUserProcess: WARNING: failed to get cxt of "
                "thread (" PIFX ") so can't follow children on WOW64.\n",
                res);
            REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                        get_application_pid(),
                                        "Failed to get context of child thread");
        }
    }
    ASSERT(cxt != NULL || DYNAMO_OPTION(early_inject)); /* Else, exited above. */
    /* Do the actual injection. */
    if (!maybe_inject_into_process(dcontext, proc_handle, thread_handle, cxt)) {
        heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
        return;
    }
    propagate_options_via_env_vars(dcontext, proc_handle, thread_handle);
    if (cxt != NULL) {
        /* injection routine is assuming doesn't have to install cxt */
        res = nt_set_context(thread_handle, cxt);
        if (!NT_SUCCESS(res)) {
            LOG(THREAD, LOG_SYSCALLS, 1,
                "syscall: NtCreateUserProcess: WARNING: failed to set cxt of "
                "thread (" PIFX ") so can't follow children on WOW64.\n",
                res);
            REPORT_FATAL_ERROR_AND_EXIT(FOLLOW_CHILD_FAILED, 3, get_application_name(),
                                        get_application_pid(),
                                        "Failed to set context of child thread");
        }
    }
    heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
}

/* NtGetContextThread */
static void
postsys_GetContextThread(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE thread_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    CONTEXT *cxt = (CONTEXT *)postsys_param(dcontext, param_base, 1);
    thread_record_t *trec;
    thread_id_t tid = thread_handle_to_tid(thread_handle);
    CONTEXT *alt_cxt;
    CONTEXT *xlate_cxt;
    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 1,
        "syscall: NtGetContextThread handle=" PFX " (tid=%d) flags=0x%x"
        " cxt->Xip=" PFX " => " PIFX "\n",
        thread_handle, tid, cxt->ContextFlags, cxt->CXT_XIP, mc->xax);
    if (!success)
        return;

    DWORD cxt_flags = CONTEXT_DR_STATE;
    size_t bufsz = nt_get_context_size(cxt_flags);
    char *buf = (char *)heap_alloc(dcontext, bufsz HEAPACCT(ACCT_THREAD_MGT));

    /* FIXME : we are going to read/write the context argument which is
     * potentially unsafe, since success it must have been readable when
     * at the os call, but there could always be multi-thread races */

    /* so trec remains valid, we are !could_be_linking */
    d_r_mutex_lock(&thread_initexit_lock);
    trec = thread_lookup(tid);
    if (trec == NULL) {
        /* this can occur if the target thread hasn't been scheduled yet
         * and therefore we haven't initialized it yet, (scheduled for
         * fixing), OR if the thread is in another process (FIXME : IPC)
         * for either case we do nothing for now
         */
        DODEBUG({
            process_id_t pid = thread_handle_to_pid(thread_handle, tid);
            if (!is_pid_me(pid)) {
                IPC_ALERT("Warning: NtGetContextThread called on thread "
                          "tid=" PFX " in different process, pid=" PFX,
                          tid, pid);
            } else {
                SYSLOG_INTERNAL_WARNING_ONCE("Warning: NtGetContextThread "
                                             "called on unknown thread " PFX,
                                             tid);
            }
        });
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
            "NtGetContextThread on unknown thread " TIDFMT "\n", tid);
    } else {
        /* FIXME : the following routine (and the routines it calls
         * namely recreate_app_state) require that trec thread be
         * suspended at a consistent spot, but we could have that the
         * trec thread is not suspended (get_thread_context doesn't
         * require it!), should we check the suspend count?
         */
        bool translate = true;
        xlate_cxt = cxt;
        if (!TESTALL(CONTEXT_DR_STATE, cxt->ContextFlags)) {
            LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
                "NtGetContextThread: app didn't ask for enough, querying ourselves\n");
            STATS_INC(num_app_getcontext_no_control);
            /* we need esp and eip, plus all regs + xmm, to translate the machine state.
             * no further permissions are needed to acquire them so we
             * get our own context w/ them.
             */
            alt_cxt = nt_initialize_context(buf, bufsz, cxt_flags);
            /* if asking for own context, thread_get_context() will point at
             * dynamorio_syscall_* and we'll fail to translate so we special-case
             */
            if (tid == d_r_get_thread_id()) {
                /* only fields that DR might change are propagated to cxt below,
                 * so set set_cur_seg to false.
                 */
                mcontext_to_context(alt_cxt, mc, false /* !set_cur_seg */);
                alt_cxt->CXT_XIP = (ptr_uint_t)dcontext->asynch_target;
                translate = false;
            } else if (!thread_get_context(trec, alt_cxt)) {
                ASSERT_NOT_REACHED();
                /* FIXME: just don't translate -- right now won't hurt us since
                 * we don't translate other than the pc anyway.
                 */
                d_r_mutex_unlock(&thread_initexit_lock);
                heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
                return;
            }
            xlate_cxt = alt_cxt;
        }

        SELF_PROTECT_LOCAL(trec->dcontext, WRITABLE);
        /* PR 214962: since we are not relocating the target thread, we do NOT
         * want to restore memory.  This is no less transparent, because
         * this thread could read the target thread's memory at any time anyway.
         */
        if (translate &&
            !translate_context(trec, xlate_cxt, false /*leave memory alone*/)) {
            /* FIXME: can get here native if GetThreadContext on
             * an un-suspended thread, but then API says result is
             * undefined so just pass anything reasonable
             * PLUS, need to handle unknown (unscheduled yet) thread --
             * passing native should be fine
             */
            SYSLOG_INTERNAL_WARNING("NtGetContextThread called for thread not in "
                                    "translatable spot");
            LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 1,
                "ERROR: NtGetContextThread called for thread not in translatable spot\n");
        } else if (xlate_cxt != cxt) {
            /* copy the fields we may have changed that app requested */
            ASSERT(!TESTALL(CONTEXT_DR_STATE, cxt->ContextFlags));
            if (TESTALL(CONTEXT_CONTROL /*2 bits so ALL*/, cxt->ContextFlags)) {
                cxt->CXT_XIP = xlate_cxt->CXT_XIP;
                cxt->CXT_XFLAGS = xlate_cxt->CXT_XFLAGS;
                cxt->CXT_XSP = xlate_cxt->CXT_XSP;
#ifndef X64
                cxt->CXT_XBP = xlate_cxt->CXT_XBP;
#endif
            }
            if (TESTALL(CONTEXT_INTEGER /*2 bits so ALL*/, cxt->ContextFlags)) {
                cxt->CXT_XAX = xlate_cxt->CXT_XAX;
                cxt->CXT_XBX = xlate_cxt->CXT_XBX;
                cxt->CXT_XCX = xlate_cxt->CXT_XCX;
                cxt->CXT_XDX = xlate_cxt->CXT_XDX;
                cxt->CXT_XSI = xlate_cxt->CXT_XSI;
                cxt->CXT_XDI = xlate_cxt->CXT_XDI;
#ifdef X64
                cxt->CXT_XBP = xlate_cxt->CXT_XBP;
                cxt->R8 = xlate_cxt->R8;
                cxt->R9 = xlate_cxt->R9;
                cxt->R10 = xlate_cxt->R10;
                cxt->R11 = xlate_cxt->R11;
                cxt->R12 = xlate_cxt->R12;
                cxt->R13 = xlate_cxt->R13;
                cxt->R14 = xlate_cxt->R14;
                cxt->R15 = xlate_cxt->R15;
#endif
            }
            if (TESTALL(CONTEXT_XMM_FLAG, cxt->ContextFlags) &&
                preserve_xmm_caller_saved()) {
                /* PR 264138 */
                memcpy(CXT_XMM(cxt, 0), CXT_XMM(xlate_cxt, 0),
                       MCXT_TOTAL_SIMD_SLOTS_SIZE);
            }
            if (TESTALL(CONTEXT_YMM_FLAG, cxt->ContextFlags) &&
                preserve_xmm_caller_saved()) {
                byte *ymmh_area = context_ymmh_saved_area(cxt);
                ASSERT(ymmh_area != NULL);
                memcpy(ymmh_area, context_ymmh_saved_area(xlate_cxt),
                       MCXT_YMMH_SLOTS_SIZE);
            }
        }
        SELF_PROTECT_LOCAL(trec->dcontext, READONLY);
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
}

/* NtSuspendThread */
static void
postsys_SuspendThread(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE thread_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    /* ignoring 2nd argument (OUT PULONG PreviousSuspendCount OPTIONAL) */
    thread_id_t tid = thread_handle_to_tid(thread_handle);
    process_id_t pid;

    LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 1,
        "syscall: NtSuspendThread tid=%d => " PIFX "\n", tid, mc->xax);
    if (SELF_PROTECT_ON_CXT_SWITCH) {
        /* No matter what, restore ignore to default value */
        dcontext->ignore_enterexit = false;
    }
    /* if we suspended ourselves then skip synchronization,
     * already resumed, FIXME : what if someone else resumes the thread
     * while we are trying to synch with it */
    if (!success || tid == d_r_get_thread_id())
        return;

    pid = thread_handle_to_pid(thread_handle, tid);
    if (!is_pid_me(pid)) {
        /* (FIXME : IPC) */
        IPC_ALERT("Warning: SuspendThread called on thread in "
                  "different process, pid=" PFX,
                  pid);
        return;
    }

    /* as optimization check if at good spot already before resuming for
     * synch, use trylocks in case suspended thread is holding any locks */
    if (d_r_mutex_trylock(&thread_initexit_lock)) {
        if (!mutex_testlock(&all_threads_lock)) {
            DWORD cxt_flags = CONTEXT_DR_STATE;
            size_t bufsz = nt_get_context_size(cxt_flags);
            char *buf = (char *)heap_alloc(dcontext, bufsz HEAPACCT(ACCT_THREAD_MGT));
            CONTEXT *cxt = nt_initialize_context(buf, bufsz, cxt_flags);
            thread_record_t *tr;
            /* know thread isn't holding any of the locks we will need */
            LOG(THREAD, LOG_SYNCH, 2,
                "SuspendThread got necessary locks to test if thread " TIDFMT
                " suspended at good spot without resuming\n",
                tid);
            tr = thread_lookup(tid);
            if (tr == NULL) {
                /* Could be unknown thread, a thread just starting up or
                 * a thread that is in the process of exiting.
                 * synch_with_thread will take care of the last case at
                 * least so we fall through to that. */
            } else if (thread_get_context(tr, cxt)) {
                priv_mcontext_t mc_thread;
                context_to_mcontext(&mc_thread, cxt);
                SELF_PROTECT_LOCAL(tr->dcontext, WRITABLE);
                if (at_safe_spot(tr, &mc_thread, THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT)) {
                    /* suspended at good spot, skip synch */
                    d_r_mutex_unlock(&thread_initexit_lock);
                    LOG(THREAD, LOG_SYNCH, 2,
                        "SuspendThread suspended thread " TIDFMT " at good place\n", tid);
                    SELF_PROTECT_LOCAL(tr->dcontext, READONLY);
                    heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
                    return;
                }
                SELF_PROTECT_LOCAL(tr->dcontext, READONLY);
            }
            heap_free(dcontext, buf, bufsz HEAPACCT(ACCT_THREAD_MGT));
        } else {
            LOG(THREAD, LOG_SYNCH, 2,
                "SuspendThread couldn't get all_threads_lock to test if thread " TIDFMT
                " at good spot without resuming\n",
                tid);
        }
        d_r_mutex_unlock(&thread_initexit_lock);
    } else {
        LOG(THREAD, LOG_SYNCH, 2,
            "SuspendThread couldn't get thread_initexit_lock to test if thread " TIDFMT
            " at good spot without resuming\n",
            tid);
    }
    LOG(THREAD, LOG_SYNCH, 2,
        "SuspendThread resuming suspended thread " TIDFMT " for synch routine\n", tid);

    /* resume for synch */
    nt_thread_resume(thread_handle, NULL);

    /* do synch */
    {
        priv_mcontext_t mcontext;
        thread_synch_result_t synch_res;
        copy_mcontext(mc, &mcontext);
        mc->pc = POST_SYSCALL_PC(dcontext);

        /* we hold the initexit lock for case 9489, see comment below in failure
         * to synch path for details why */
        if (DYNAMO_OPTION(suspend_on_synch_failure_for_app_suspend))
            d_r_mutex_lock(&thread_initexit_lock);
        synch_res = synch_with_thread(
            tid, true /* block */,
            /* initexit lock status */
            DYNAMO_OPTION(suspend_on_synch_failure_for_app_suspend),
            THREAD_SYNCH_VALID_MCONTEXT, THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT,
            /* if we fail to suspend a thread (e.g., privilege
             * problems) ignore it. FIXME: retry instead? */
            THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
        if (synch_res != THREAD_SYNCH_RESULT_SUCCESS) {
            /* xref case 9488 - we failed to synch, could be we exceeded our loop count
             * for some reason, we lack GetContext permission (or the apps handle has
             * suspend and ours doesn't somehow), or could be an unknown thread. FIXME -
             * we suspend the thread so the app doesn't get screwed up (it expects a
             * suspended thread) at the risk of possibly deadlocking DR if it holds
             * one of our locks etc. */
            /* If the thread is unknown everything might be ok, could be a thread that's
             * almost exited (should be fine though app might get slightly screwy result
             * if it calls get context, e.g. an eip in our dll) or a new thread that
             * hasn't yet initialized (see case 9489, should also be fine since we hold
             * the initexit lock so the thread can't have gone anywhere since the
             * synch_with_thread checks). NOTE - SetEvent appears to do the sensible
             * thing when an auto-reset event that has a suspended thread waiting on it
             * is signaled (the new thread could be waiting on the initexit lock), i.e.
             * leave the event signaled for someone else to grab. */
            /* Full ASSERT if thread is known (always bad to fail then), curiosity
             * instead if thread is unknown (since expected to be ok). */
            ASSERT(thread_lookup(tid) == NULL); /* i.e. thread not known */
            /* The suspend.c unit test can hit this regularly on (via suspend new thread)
             * though we expect it to be unusual in normal applications. Same thing with
             * detach_test.exe and threadinjection.exe. */
            ASSERT_CURIOSITY_ONCE((thread_lookup(tid) != NULL || /* thread known */
                                   EXEMPT_TEST("win32.suspend.exe;runall.detach_test.exe;"
                                               "win32.threadinjection.exe")) &&
                                  "app suspending unknown thread");
            if (DYNAMO_OPTION(suspend_on_synch_failure_for_app_suspend)) {
                /* thread may already be exited in which case this will fail */
                DEBUG_DECLARE(bool res =) nt_thread_suspend(thread_handle, NULL);
                ASSERT(res || is_thread_exited(thread_handle) == THREAD_EXITED);
            }
        }
        if (DYNAMO_OPTION(suspend_on_synch_failure_for_app_suspend))
            d_r_mutex_unlock(&thread_initexit_lock);

        /* FIXME - if the thread exited we should prob. change the return value to
         * the app to a failure value. Only an assert_curiosity for now to see if any
         * apps suspend threads while the threads are exiting and if so what they expect
         * to happen. */
        ASSERT_CURIOSITY(is_thread_exited(thread_handle) == THREAD_NOT_EXITED);

        copy_mcontext(&mcontext, mc);
    }
}

/* NtQueryInformationThread */
static void
postsys_QueryInformationThread(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    THREADINFOCLASS class = (THREADINFOCLASS)postsys_param(dcontext, param_base, 1);
    if (success && class == ThreadAmILastThread) {
        HANDLE thread_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        thread_id_t tid = thread_handle_to_tid(thread_handle);
        process_id_t pid = thread_handle_to_pid(thread_handle, tid);
        if (pid != POINTER_MAX && is_pid_me(pid) && get_num_client_threads() > 0 &&
            is_last_app_thread()) {
            PVOID info = (PVOID)postsys_param(dcontext, param_base, 2);
            ULONG info_sz = (ULONG)postsys_param(dcontext, param_base, 3);
            BOOL pretend_val = TRUE;
            LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, IF_DGCDIAG_ELSE(1, 2),
                "syscall: NtQueryInformationThread ThreadAmILastThread fooling\n");
            ASSERT_CURIOSITY(info_sz == sizeof(BOOL));
            if (info_sz == sizeof(BOOL))
                safe_write(info, info_sz, &pretend_val);
        }
    }
}

/* NtOpenThread */
static void
postsys_OpenThread(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    if (success) {
        HANDLE *handle = (HANDLE *)postsys_param(dcontext, param_base, 0);
        CLIENT_ID *cid = (CLIENT_ID *)postsys_param(dcontext, param_base, 3);
        LOG(THREAD, LOG_SYSCALLS | LOG_THREADS, 2,
            "syscall: NtOpenThread " PFX "=>" PFX " " PFX "=>" TIDFMT "\n", handle,
            *handle, cid, cid->UniqueThread);
        handle_to_tid_add(*handle, (thread_id_t)cid->UniqueThread);
    }
}

/* NtAllocateVirtualMemory */
static void
postsys_AllocateVirtualMemory(dcontext_t *dcontext, reg_t *param_base, bool success,
                              int sysnum)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    /* XXX i#899: for NtWow64AllocateVirtualMemory64, the base and
     * size may be 64-bit values?  But, when allocating in wow64
     * child, the address should be in low 2GB, as only ntdll64 is up
     * high.  If the extra arg were before ZeroBits, it could be a pointer
     * to the high bits of the base addr, like NtWow64ReadVirtualMemory64(),
     * but that doesn't seem to be the case.
     */
    void **pbase = (void **)postsys_param(dcontext, param_base, 1);
    uint zerobits = (uint)postsys_param(dcontext, param_base, 2);
    /* XXX i#899: NtWow64AllocateVirtualMemory64 has an extra arg after ZeroBits but
     * it's ignored in wow64!whNtWow64AllocateVirtualMemory64.  We should keep an eye
     * out: maybe a future service pack or win9 will use it.
     */
    int arg_shift = (sysnum == syscalls[SYS_Wow64AllocateVirtualMemory64] ? 1 : 0);
    size_t *psize = (size_t *)postsys_param(dcontext, param_base, 3 + arg_shift);
    uint type = (uint)postsys_param(dcontext, param_base, 4 + arg_shift);
    uint prot = (uint)postsys_param(dcontext, param_base, 5 + arg_shift);
    app_pc base;
    size_t size;
    if (!success) {
        /* FIXME i#148: should try to recover from any prot change -- though today we
         * don't even do so on NtProtectVirtualMemory failing.
         */
        return;
    }
    if (!d_r_safe_read(pbase, sizeof(base), &base) ||
        !d_r_safe_read(psize, sizeof(size), &size)) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "syscall: NtAllocateVirtualMemory: failed to read params " PFX " " PFX "\n",
            pbase, psize);
        return;
    }
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, prot_is_executable(prot) ? 1U : 2U,
        "syscall: NtAllocateVirtualMemory%s%s%s@" PFX " sz=" PIFX
        " prot=%s 0x%x => 0x%x\n",
        is_phandle_me(process_handle) ? "" : " IPC",
        TEST(MEM_RESERVE, type) ? " reserve" : "",
        TEST(MEM_COMMIT, type) ? " commit  " : " ", base, size, prot_string(prot), prot,
        mc->xax);
    DOLOG(1, LOG_MEMSTATS, {
        /* snapshots are heavyweight, so do rarely */
        if (size > SNAPSHOT_THRESHOLD && is_phandle_me(process_handle))
            mem_stats_snapshot();
    });

    if (TEST(ASLR_HEAP_FILL, DYNAMO_OPTION(aslr)) && is_phandle_me(process_handle)) {
        /* We allocate our padding after the application region is
         * successfully reserved.  FIXME: assuming that one cannot
         * pass MEM_RESERVE|MEM_COMMIT on an already reserved
         * region.  Yet note one can MEM_COMMIT a region that has
         * been committed already.  Note that it is OK to pass
         * MEM_COMMIT with original base set to NULL, and then the
         * allocation will act as MEM_RESERVE|MEM_COMMIT!  One
         * can't pass MEM_COMMIT that with non-zero base on a
         * region that hasn't been reserved before.  We want to
         * make sure we pad only an amount corresponding to the
         * new reservations.  (Currently we only pad immediately
         * after an allocation but that may change.)
         */

        /* FIXME: case 6287 we should TEST(MEM_RESERVE, type) if
         * allocation has just been reserved, or if pre_syscall
         * base was NULL for a MEM_COMMIT.  Currently a pad is
         * reserved only in case immediate region has not been
         * reserved, so we're ok to attempt to pad even
         * a MEM_COMMIT with an existing reservation
         */
        aslr_post_process_allocate_virtual_memory(dcontext, base, size);
    }

    if (!TEST(MEM_COMMIT, type)) {
        /* MEM_RESERVE only: protection bits are meaningless, we do nothing
         * MEM_RESET: we do not need to flush on a reset, since whatever is there
         * cannot be changed without writing to it!
         * the subsequent commit to the already committed region will work fine.
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "not committing, so ignorable\n");
        return;
    }
    if (is_phandle_me(process_handle)) {
#ifdef DGC_DIAGNOSTICS
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, TEST(MEM_COMMIT, type) ? 1U : 2U,
            "syscall: NtAllocateVirtualMemory%s%s@" PFX " sz=" PIFX
            " prot=%s 0x%x => 0x%x\n",
            TEST(MEM_RESERVE, type) ? " reserve" : "",
            TEST(MEM_COMMIT, type) ? " commit  " : " ", base, size, prot_string(prot),
            prot, mc->xax);
        DOLOG(1, LOG_VMAREAS, {
            dump_callstack(POST_SYSCALL_PC(dcontext), (app_pc)mc->xbp, THREAD,
                           DUMP_NOT_XML);
        });
#endif
        app_memory_allocation(dcontext, base, size, osprot_to_memprot(prot),
                              false /*not image*/
                              _IF_DEBUG("NtAllocateVirtualMemory"));
#ifdef DGC_DIAGNOSTICS
        DOLOG(3, LOG_VMAREAS, {
            /* make all heap RO in attempt to view generation of DGC */
            if (!is_address_on_stack(dcontext, base) && prot_is_writable(prot)) {
                /* new thread stack: reserve big region, commit 2 pages, then mark
                 * 1 page as PAGE_GUARD.  strangely thread gets resumed sometimes
                 * before see PAGE_GUARD prot, so instead of tracking that we have
                 * a hack to guess if this is a thread stack:
                 */
                IF_X64(ASSERT_NOT_IMPLEMENTED(false));
                if (size == 0x2000 && ((ptr_uint_t)base & 0xf0000000) == 0x00000000 &&
                    prot == PAGE_READWRITE) {
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "Guessing " PFX "-" PFX " is thread stack\n", base, base + size);
                } else {
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "Making " PFX "-" PFX " 0x%x unwritable\n", base, base + size,
                        prot);
                    make_unwritable(base, size);
                }
            }
        });
#endif
    } else {
        /* FIXME: should we try to alert any dynamo running the other process?
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "WARNING: NtAllocateVirtualMemory for process " PFX " %d\n", process_handle,
            process_id_from_handle(process_handle));
        DODEBUG({
            if (prot_is_executable(prot)) {
                IPC_ALERT("NtAllocateVirtualMemory for process " PFX " %d prot=%s",
                          process_handle, process_id_from_handle(process_handle),
                          prot_string(prot));
            }
        });

        /* This actually happens in calc's help defn popup!
         * FIXME: we need IPC!  Plus need to queue up msgs to child dynamo,
         * for calc it did NtCreateProcess, NtAllocateVirtualMemory, then
         * the NtCreateThread that triggers our fork injection!
         * don't die with IPC_ALERT
         */
    }
}

/* NtAllocateVirtualMemoryEx */
static void
postsys_AllocateVirtualMemoryEx(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /*
        FIXME i#3090: The parameters for NtAllocateVirtualMemoryEx are undocumented.
    */
    ASSERT_CURIOSITY("unimplemented post handler for NtAllocateVirtualMemoryEx");
}

/* NtQueryVirtualMemory */
static void
postsys_QueryVirtualMemory(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* we intercept this for transparency wrt the executable regions
     * that we mark as read-only
     */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    app_pc base = (app_pc)postsys_param(dcontext, param_base, 1);
    uint class = (uint)postsys_param(dcontext, param_base, 2);
    MEMORY_BASIC_INFORMATION *mbi =
        (MEMORY_BASIC_INFORMATION *)postsys_param(dcontext, param_base, 3);
    size_t infolen = (size_t)postsys_param(dcontext, param_base, 4);
    size_t *returnlen = (size_t *)postsys_param(dcontext, param_base, 5);
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, IF_DGCDIAG_ELSE(1, 2),
        "syscall: NtQueryVirtualMemory base=" PFX " => " PIFX "\n", base, mc->xax);
    if (!success)
        return;
    /* FIXME : since success we assume that all argument dereferences are
     * safe though there could always be multi-thread races */
    if (is_phandle_me(process_handle)) {
        if (class == MemoryBasicInformation) {
            /* see if asking about an executable area we made read-only */
            if (is_pretend_or_executable_writable(base)) {
                /* pretend area is writable */
                uint flags = mbi->Protect & ~PAGE_PROTECTION_QUALIFIERS;
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "WARNING: Query to now-readonly executable area, pretending "
                    "writable\n");
                if (flags == PAGE_READONLY) {
                    mbi->Protect &= ~PAGE_READONLY;
                    mbi->Protect |= PAGE_READWRITE;
                } else if (flags == PAGE_EXECUTE_READ) {
                    mbi->Protect &= ~PAGE_EXECUTE_READ;
                    mbi->Protect |= PAGE_EXECUTE_READWRITE;
                } else {
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "ERROR: Query to now-readonly executable area w/ bad flags %s\n",
                        prot_string(mbi->Protect));
                    SYSLOG_INTERNAL_INFO("ERROR: Query to now-readonly executable area "
                                         "w/ bad flags");
                }
            } else if (is_dynamo_address(base)) {
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "WARNING: QueryVM to DR memory " PFX "\n", base);
                if (base == dynamo_dll_start && mbi != NULL &&
                    DYNAMO_OPTION(hide_from_query) != 0) {
                    /* pretend area is un-allocated */
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "WARNING: QueryVM to DR DLL " PFX ", pretending not a dll\n",
                        base);
                    if (TEST(HIDE_FROM_QUERY_TYPE_PROTECT,
                             DYNAMO_OPTION(hide_from_query))) {
                        mbi->Type = MEM_PRIVATE; /* not image! */
                        mbi->Protect = PAGE_NOACCESS;
                    }
                    /* now do an off-by-1 to fool any calls to GetModuleFileName
                     * (it doesn't turn into a syscall)
                     * FIXME: app could still use a snapshot to get list
                     * of modules, but that is covered by -hide
                     */
                    if (TEST(HIDE_FROM_QUERY_BASE_SIZE, DYNAMO_OPTION(hide_from_query))) {
                        mbi->AllocationBase = ((app_pc)mbi->AllocationBase) + PAGE_SIZE;
                        mbi->BaseAddress = ((app_pc)mbi->BaseAddress) + PAGE_SIZE;
                        /* skip over the other regions in our dll -- ok to
                         * be PAGE_SIZE off, better to be beyond than
                         * return too small and have caller incrementing
                         * only and ignoring bases!
                         */
                        mbi->RegionSize = (dynamo_dll_end - dynamo_dll_start);
                    }
                    /* note that returning STATUS_INVALID_ADDRESS is too
                     * extreme of a solution, so this is off by default */
                    if (TEST(HIDE_FROM_QUERY_RETURN_INVALID,
                             DYNAMO_OPTION(hide_from_query))) {
                        /* FIXME: SET_RETURN_VAL bug 5068 had return val as 0
                         * Need to re-test this with this actual return val
                         */
                        SET_RETURN_VAL(dcontext, STATUS_INVALID_ADDRESS);
                    }
                }
            }
        } else if (class == MemorySectionName) {
            /* This does work on image sections on later Windows */
            if (is_dynamo_address(base)) {
                /* Apps should be fine with this failing.  This is the failure
                 * status for an address that does not contain a mapped file.
                 */
                SET_RETURN_VAL(dcontext, STATUS_INVALID_ADDRESS);
            }
        }
    } else {
        IPC_ALERT("Warning: QueryVirtualMemory on another process");
    }
}

static void
postsys_create_or_open_section(dcontext_t *dcontext, HANDLE *unsafe_section_handle,
                               HANDLE file_handle, bool non_image)
{
    HANDLE section_handle = INVALID_HANDLE_VALUE;
    if (DYNAMO_OPTION(track_module_filenames) &&
        d_r_safe_read(unsafe_section_handle, sizeof(section_handle), &section_handle)) {
        /* Case 1272: keep file name around to use for module identification */
        FILE_NAME_INFORMATION name_info; /* note: large struct */
        wchar_t buf[MAXIMUM_PATH];
        wchar_t *fname = name_info.FileName;
        /* For i#138 we want the full path so we ignore the short name
         * returned by get_file_short_name
         */
        if (file_handle != INVALID_HANDLE_VALUE &&
            get_file_short_name(file_handle, &name_info) != NULL) {
            bool have_name = false;
            if (convert_NT_to_Dos_path(buf, name_info.FileName,
                                       BUFFER_SIZE_ELEMENTS(buf))) {
                fname = buf;
                have_name = true;
            } else if (get_os_version() <= WINDOWS_VERSION_2000 && !non_image) {
                /* It's normal for NtQueryInformationFile to return a relative path.
                 * For non-images, or for XP+ for all sections, we can get the
                 * absolute path at map time: but for images (or if we don't know
                 * whether image, e.g. for OpenSection) on NT/2K we map in the file
                 * as a non-image to find the name.  Kind of expensive, but it's only
                 * for legacy platforms, and option-controlled.
                 */
                size_t size = 0;
                byte *pc = os_map_file(file_handle, &size, 0, NULL, MEMPROT_READ,
                                       0 /*not cow or image*/);
                if (pc == NULL) {
                    /* We don't know what perms the file was opened with.  Sometimes
                     * we can only map +x so try that.
                     */
                    pc = os_map_file(file_handle, &size, 0, NULL, MEMPROT_EXEC,
                                     0 /*not cow or image*/);
                }
                if (pc != NULL) {
                    NTSTATUS res = get_mapped_file_name(pc, buf, BUFFER_SIZE_BYTES(buf));
                    if (NT_SUCCESS(res)) {
                        have_name = convert_NT_to_Dos_path(
                            name_info.FileName, buf,
                            BUFFER_SIZE_ELEMENTS(name_info.FileName));
                    }
                    os_unmap_file(pc, size);
                }
            }
            if (!have_name) {
                /* i#1180: we get non-drive absolute DOS paths here which naturally
                 * convert_NT_to_Dos_path can't handle (e.g.,
                 * "\Windows\Globalization\Sorting\SortDefault.nls").
                 * We expect to get an NT path at map time on XP+, so we only
                 * warn for 2K- images.
                 */
                DODEBUG({
                    if (get_os_version() <= WINDOWS_VERSION_2000 && !non_image) {
                        STATS_INC(map_unknown_Dos_name);
                        SYSLOG_INTERNAL_WARNING_ONCE("unknown mapfile Dos name");
                    }
                });
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "\t%s: pre-map, unable to convert NT to Dos path for \"%S\"\n",
                    __FUNCTION__, fname);
            }
            section_to_file_add_wide(section_handle, fname);
        } else {
            /* We assume that we'll have the file_handle for image sections:
             * either we'll see a CreateSection w/ a file, or we'll see OpenSection
             * on a KnownDlls path w/ RootDirectory set.  So this is likely a
             * non-image section, whose backing file we'll query at map time.
             */
            DODEBUG(name_info.FileName[0] = '\0';);
        }
#ifdef DEBUG
        dcontext->aslr_context.last_app_section_handle = section_handle;
#endif
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "\tNt{Create,Open}Section: sec handle " PIFX ", file " PFX " => \"%S\"\n",
            section_handle, file_handle, fname);
    }
}

/* NtCreateSection */
static void
postsys_CreateSection(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* a section is an object that can be mmapped */
    HANDLE *unsafe_section_handle = (HANDLE *)postsys_param(dcontext, param_base, 0);
    uint access_mask = (uint)postsys_param(dcontext, param_base, 1);
    POBJECT_ATTRIBUTES obj = (POBJECT_ATTRIBUTES)postsys_param(dcontext, param_base, 2);
    void *size = (void *)postsys_param(dcontext, param_base, 3);
    uint protect = (uint)postsys_param(dcontext, param_base, 4);
    uint attributes = (uint)postsys_param(dcontext, param_base, 5);
    HANDLE file_handle = (HANDLE)postsys_param(dcontext, param_base, 6);
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "syscall: NtCreateSection protect 0x%x, attributes 0x%x\n", protect, attributes);
    if (!success)
        return;

    postsys_create_or_open_section(dcontext, unsafe_section_handle, file_handle,
                                   !TEST(SEC_IMAGE, attributes));

    if (TEST(ASLR_DLL, DYNAMO_OPTION(aslr))) {
        if (TEST(SEC_IMAGE, attributes)) {
            if (aslr_post_process_create_or_open_section(dcontext, true, /* create */
                                                         file_handle,
                                                         unsafe_section_handle)) {
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "syscall: ASLR: NtCreateSection replaced with new section " PIFX "\n",
                    *unsafe_section_handle);
            } else {
                /* leaving as is */
            }
        } else {
            /* ignoring SEC_COMMIT mappings - since SEC_COMMIT is
             * default it doesn't need to be set */
        }
    }
}

/* NtOpenSection */
static void
postsys_OpenSection(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* a section is an object that can be mmapped, here opened by object name */
    HANDLE *unsafe_section_handle = (HANDLE *)postsys_param(dcontext, param_base, 0);
    uint access_mask = (uint)postsys_param(dcontext, param_base, 1);
    POBJECT_ATTRIBUTES obj_attr =
        (POBJECT_ATTRIBUTES)postsys_param(dcontext, param_base, 2);
    HANDLE new_file_handle = INVALID_HANDLE_VALUE;
    if (!success) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "syscall: NtOpenSection, failed, access 0x%x\n", access_mask);
        return;
    }

    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "syscall: NtOpenSection opened sh " PIFX ", access_mask 0x%x, obj_attr " PFX "\n",
        *unsafe_section_handle, access_mask, obj_attr);

    /* If we only wanted short names for -track_module_filenames,
     * could we use obj_attr->ObjectName->Buffer and not
     * call aslr_recreate_known_dll_file() at all?
     */
    if ((DYNAMO_OPTION(track_module_filenames) ||
         (TEST(ASLR_DLL, DYNAMO_OPTION(aslr)) &&
          TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)))) &&
        obj_attr != NULL) {
        /* need to identify KnownDlls here */
        /* FIXME: NtOpenSection doesn't give us section attributes,
         * and we can't even query them - the only reasonable solution is to
         * match the directory handle
         *
         * FIXME: case 9032 about possibly duplicating the handle if
         * that is any faster than any other syscalls we're making here
         */
        /* FIXME: we could restrict the check to potential DLLs
         * based on access_mask, although most users use
         * SECTION_ALL_ACCESS */
        HANDLE root_directory = NULL;
        bool ok = d_r_safe_read(&obj_attr->RootDirectory, sizeof(root_directory),
                                &root_directory);
        if (ok && root_directory != NULL && aslr_is_handle_KnownDlls(root_directory)) {

            if (aslr_recreate_known_dll_file(obj_attr, &new_file_handle)) {
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "syscall: NtOpenSection: recreated file handle " PIFX "\n",
                    new_file_handle);
            } else {
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "syscall: NtOpenSection: unable to recreate file handle\n");
            }

            if (TEST(ASLR_DLL, DYNAMO_OPTION(aslr)) &&
                TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache))) {
                if (aslr_post_process_create_or_open_section(dcontext, false, /* open */
                                                             /* recreated file */
                                                             new_file_handle,
                                                             unsafe_section_handle)) {
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                        "syscall: ASLR: NtOpenSection replaced with new section " PIFX
                        "\n",
                        *unsafe_section_handle);
                } else {
                    /* leaving as is */
                }
            }
            /* If we're not replacing the section (i.e., not doing ASLR_DLL),
             * we need new_file_handle for postsys_create_or_open_section so we do
             * not close here
             */
        } else {
            /* nothing */
        }
    }
    if (DYNAMO_OPTION(track_module_filenames)) {
        postsys_create_or_open_section(dcontext, unsafe_section_handle, new_file_handle,
                                       false /*don't know*/);
    }
    if (new_file_handle != INVALID_HANDLE_VALUE)
        close_handle(new_file_handle);
}

/* NtMapViewOfSection */
static void
postsys_MapViewOfSection(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* This is what actually allocates a dll into memory */
    priv_mcontext_t *mc = get_mcontext(dcontext);
    HANDLE section_handle;       /* 0 */
    HANDLE process_handle;       /* 1 */
    void **pbase_unsafe;         /* 2 */
    uint zerobits;               /* 3 */
    size_t commit_size;          /* 4 */
    LARGE_INTEGER *section_offs; /* 5 */
    size_t *view_size;           /* 6 */
    uint inherit_disposition;    /* 7 */
    uint type;                   /* 8 */
    uint prot;                   /* 9 */

    size_t size; /* safe dereferenced values */
    app_pc base;

    /* only process if we acted on this call in aslr_pre_process_mapview */
    if (dcontext->aslr_context.sys_aslr_clobbered) {
        aslr_post_process_mapview(dcontext);
        /* preceding call sets mcontext.xax so re-evaluate */
        success = NT_SUCCESS(mc->xax);
        /* reevaluate all system call OUT arguments, since they
         * may have changed in aslr_post_process_mapview()!
         */

        /* FIXME: registers may not necessarily match state of
         * mangled system call, but we assume only state->mc.xax matters
         */
    }

    if (!success) {
        prot = (uint)postsys_param(dcontext, param_base, 9);
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "syscall: failed NtMapViewOfSection prot=%s => " PFX "\n", prot_string(prot),
            mc->xax);

        return;
    }

    section_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    process_handle = (HANDLE)postsys_param(dcontext, param_base, 1);
    pbase_unsafe = (void *)postsys_param(dcontext, param_base, 2);
    zerobits = (uint)postsys_param(dcontext, param_base, 3);
    commit_size = (size_t)postsys_param(dcontext, param_base, 4);
    section_offs = (LARGE_INTEGER *)postsys_param(dcontext, param_base, 5);
    view_size = (size_t *)postsys_param(dcontext, param_base, 6);
    inherit_disposition = (uint)postsys_param(dcontext, param_base, 7);
    type = (uint)postsys_param(dcontext, param_base, 8);
    prot = (uint)postsys_param(dcontext, param_base, 9);

    /* we assume that since syscall succeeded these dereferences are safe
     * FIXME : could always be multi-thread races though */
    size = *view_size; /* ignore commit_size? */
    base = *((app_pc *)pbase_unsafe);

    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: NtMapViewOfSection " PFX " size=" PIFX " prot=%s => " PFX "\n", base,
        size, prot_string(prot), mc->xax);

    if (is_phandle_me(process_handle)) {
        /* Check if we are looking for LdrpLoadImportModule address */
        if (dcontext == early_inject_load_helper_dcontext) {
            check_for_ldrpLoadImportModule(base, (uint *)mc->xbp);
        }
        DOLOG(1, LOG_MEMSTATS, {
            /* snapshots are heavyweight, so do rarely */
            if (size > SNAPSHOT_THRESHOLD)
                mem_stats_snapshot();
        });
#ifdef DGC_DIAGNOSTICS
        DOLOG(1, LOG_VMAREAS, {
            dump_callstack(POST_SYSCALL_PC(dcontext), (app_pc)mc->xbp, THREAD,
                           DUMP_NOT_XML);
        });
#endif
        RSTATS_INC(num_app_mmaps);
        if (!DYNAMO_OPTION(thin_client)) {
            const char *file = NULL;
            DEBUG_DECLARE(const char *reason = "";)
            if (DYNAMO_OPTION(track_module_filenames)) {
                bool unknown = true;
                /* get_mapped_file_name always gives an absolute path, so it's
                 * preferable to using our section_to_file table.  but,
                 * get_mapped_file_name only works on image sections on XP+.
                 * we go ahead and use it on all sections here, even though
                 * we don't use the names of non-image sections, to avoid
                 * warnings below (where we don't know whether image or not).
                 */
                wchar_t buf[MAXIMUM_PATH];
                /* FIXME: should we heap alloc to avoid these huge buffers */
                wchar_t buf2[MAXIMUM_PATH];
                NTSTATUS res = get_mapped_file_name(base, buf, BUFFER_SIZE_BYTES(buf));
                if (NT_SUCCESS(res)) {
                    if (convert_NT_to_Dos_path(buf2, buf, BUFFER_SIZE_ELEMENTS(buf2))) {
                        file = dr_wstrdup(buf2 HEAPACCT(ACCT_VMAREAS));
                    } else {
                        file = dr_wstrdup(buf HEAPACCT(ACCT_VMAREAS));
                        STATS_INC(map_unknown_Dos_name);
                        SYSLOG_INTERNAL_WARNING_ONCE("unknown mapfile Dos name");
                        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                            "\t%s: WARNING: unable to convert NT to Dos path for "
                            "\"%S\"\n",
                            __FUNCTION__, buf2);
                    }
                    /* may as well update the table: if already there this is a nop */
                    section_to_file_add(section_handle, file);
                    unknown = false;
                } else if (res == STATUS_FILE_INVALID) {
                    /* An anonymous section backed by the pagefile.  Should we
                     * verify that its CreateSection was passed NULL for a file?
                     * You can see some of these just starting up calc.  They
                     * have names like
                     * "\BaseNamedObjects\CiceroSharedMemDefaultS-1-5-21-1262752155-"
                     * "650278929-1212509807-100"
                     */
                    unknown = false;
                    DODEBUG(reason = " (pagefile-backed)";);
                } else {
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                        "\tget_mapped_file_name failed error=" PFX "\n", res);
                }
                if (file == NULL) {
                    file = section_to_file_lookup(section_handle);
                    if (file != NULL)
                        unknown = false;
                }
                if (unknown) {
                    /* Since we have a process-wide handle map and we watch
                     * close and duplicate, we should only mess up when handles
                     * are passed via IPC.
                     */
                    STATS_INC(map_section_mismatch);
                    SYSLOG_INTERNAL_WARNING_ONCE("unknown mapped section " PIFX,
                                                 section_handle);
                }
            }
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "\tNtMapViewOfSection: sec handle " PIFX " == file \"%s\"%s\n",
                section_handle, file == NULL ? "<null>" : file, reason);
            process_mmap(dcontext, base, size, true /*map*/, file);
            if (file != NULL)
                dr_strfree(file HEAPACCT(ACCT_VMAREAS));
        }
    } else {
        IPC_ALERT("WARNING: MapViewOfSection on another process");
    }
}

/* NtMapViewOfSectionEx */
static void
postsys_MapViewOfSectionEx(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /*
        FIXME i#3090: The parameters for NtMapViewOfSectionEx are undocumented.
    */
    ASSERT_CURIOSITY("unimplemented post handler for NtMapViewOfSectionEx");
}

/* NtUnmapViewOfSection{,Ex} */
static void
postsys_UnmapViewOfSection(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    /* This is what actually removes a dll from memory */
    HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
#ifdef DEBUG
    if (dcontext->expect_last_syscall_to_fail) {
        ASSERT(!success);
    } else {
        /* FIXME : try to recover if the syscall fails, could re-walk this
         * region but that gets us in trouble with the stateful policies */
        ASSERT_CURIOSITY(success || !is_phandle_me(process_handle));
    }
#endif
    /* note that if expected this to fail we wouldn't have really registered,
     * but we don't keep track in release builds
     */
    if (DYNAMO_OPTION(unloaded_target_exception) && is_phandle_me(process_handle)) {
        app_pc base = (app_pc)postsys_param(dcontext, param_base, 1);
        /* We always mark end of unmap no matter what the original
         * section really was.  FIXME: Note we can't get the real_base
         * of the allocation, unless we keep it in dcontext from
         * presys_UnmapViewOfSection, but we don't really need it in
         * release build.  We don't care about success or !success
         * either.  Note that this means that if a MEM_MAPPED UnMap
         * ends before an overlapping MEM_IMAGE UnMap, we will mark
         * end too early.
         */
        mark_unload_end(base);
    }
}

/* NtDuplicateObject */
static void
postsys_DuplicateObject(dcontext_t *dcontext, reg_t *param_base, bool success)
{
    if (DYNAMO_OPTION(track_module_filenames) && success) {
        HANDLE src_process = (HANDLE)postsys_param(dcontext, param_base, 0);
        HANDLE tgt_process = (HANDLE)postsys_param(dcontext, param_base, 2);
        if (is_phandle_me(src_process) && is_phandle_me(tgt_process)) {
            HANDLE src = (HANDLE)postsys_param(dcontext, param_base, 1);
            HANDLE *dst = (HANDLE *)postsys_param(dcontext, param_base, 3);
            const char *file = section_to_file_lookup(src);
            if (file != NULL) {
                HANDLE dup;
                if (d_r_safe_read(dst, sizeof(dup), &dup)) {
                    /* Should already be converted to Dos path */
                    section_to_file_add(dup, file);
                    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
                        "syscall: NtDuplicateObject section handle " PFX " => " PFX "\n",
                        src, dup);
                } else /* shouldn't happen: syscall succeeded; must be race */
                    ASSERT_NOT_REACHED();
                dr_strfree(file HEAPACCT(ACCT_VMAREAS));
            }
        } else {
            IPC_ALERT("WARNING: handle via IPC may mess up section-to-handle mapping");
        }
    }
}

/* i#537: sysenter returns to KiFastSystemCallRet from kernel, and returns to DR
 * from there. We restore the correct app return target and re-execute
 * KiFastSystemCallRet to make sure client see the code at KiFastSystemCallRet.
 */
static void
restore_for_KiFastSystemCallRet(dcontext_t *dcontext)
{
    reg_t adjust_esp;
    ASSERT(get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
           KiFastSystemCallRet_address != NULL);
    /* We don't want to do this adjustment until after the final syscall
     * in any invoke-another sequence (i#1210)
     */
    if (instrument_invoke_another_syscall(dcontext))
        return;
    /* If this thread is native, don't disrupt the return-to-native */
    if (!dcontext->thread_record->under_dynamo_control)
        return;
    adjust_esp = get_mcontext(dcontext)->xsp - XSP_SZ;
    *(app_pc *)adjust_esp = dcontext->asynch_target;
    get_mcontext(dcontext)->xsp = adjust_esp;
    dcontext->asynch_target = KiFastSystemCallRet_address;
}

/* NOTE : no locks can be grabbed on the path to SuspendThread handling code */
void
post_system_call(dcontext_t *dcontext)
{
    /* registers have been clobbered, so grab key values that were
     * saved in pre_system_call
     */
    int sysnum = dcontext->sys_num;
    reg_t *param_base = dcontext->sys_param_base;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool success = NT_SUCCESS(mc->xax);
    dr_where_am_i_t old_whereami = dcontext->whereami;
    KSTART(post_syscall);
    dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;
    DODEBUG({ dcontext->post_syscall = true; });

    LOG(THREAD, LOG_SYSCALLS, 2,
        "post syscall: sysnum=" PFX ", params @" PFX ", result=" PFX "\n", sysnum,
        param_base, mc->xax);

    if (sysnum == syscalls[SYS_GetContextThread]) {
        postsys_GetContextThread(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_SuspendThread]) {
        postsys_SuspendThread(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_SetContextThread]) {
        HANDLE thread_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        thread_id_t tid = thread_handle_to_tid(thread_handle);
        ASSERT(tid != 0xFFFFFFFF);
        /* FIXME : we modified the passed in context, we should restore it
         * to app state (same for SYS_Continue though is more difficult there)
         */
        if (tid != d_r_get_thread_id()) {
            d_r_mutex_lock(&thread_initexit_lock); /* need lock to lookup thread */
            if (intercept_asynch_for_thread(tid, false /*no unknown threads*/)) {
                /* Case 10101: we shouldn't get here since we now skip the system call,
                 * unless it should fail for permission issues */
                ASSERT(dcontext->expect_last_syscall_to_fail);
                /* must wake up thread so it can go to nt_continue_dynamo_start */
                nt_thread_resume(thread_handle, NULL);
            }
            d_r_mutex_unlock(&thread_initexit_lock); /* need lock to lookup thread */
        }
    } else if (sysnum == syscalls[SYS_OpenThread]) {
        postsys_OpenThread(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_QueryInformationThread]) {
        postsys_QueryInformationThread(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_AllocateVirtualMemory] ||
               /* i#899: new win8 syscall w/ similar params to NtAllocateVirtualMemory */
               sysnum == syscalls[SYS_Wow64AllocateVirtualMemory64]) {
        KSTART(post_syscall_alloc);
        postsys_AllocateVirtualMemory(dcontext, param_base, success, sysnum);
        KSTOP(post_syscall_alloc);
    } else if (sysnum == syscalls[SYS_AllocateVirtualMemoryEx]) {
        postsys_AllocateVirtualMemoryEx(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_QueryVirtualMemory]) {
        postsys_QueryVirtualMemory(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_CreateSection]) {
        postsys_CreateSection(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_OpenSection]) {
        postsys_OpenSection(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_MapViewOfSection]) {
        KSTART(post_syscall_map);
        postsys_MapViewOfSection(dcontext, param_base, success);
        KSTOP(post_syscall_map);
    } else if (sysnum == syscalls[SYS_MapViewOfSectionEx]) {
        postsys_MapViewOfSectionEx(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_CreateProcess]) {
        HANDLE *process_handle = (HANDLE *)postsys_param(dcontext, param_base, 0);
        uint access_mask = (uint)postsys_param(dcontext, param_base, 1);
        uint attributes = (uint)postsys_param(dcontext, param_base, 2);
        uint inherit_from = (uint)postsys_param(dcontext, param_base, 3);
        BOOLEAN inherit = (BOOLEAN)postsys_param(dcontext, param_base, 4);
        HANDLE section_handle = (HANDLE)postsys_param(dcontext, param_base, 5);
        HANDLE debug_handle = (HANDLE)postsys_param(dcontext, param_base, 6);
        HANDLE exception_handle = (HANDLE)postsys_param(dcontext, param_base, 7);
        HANDLE proc_handle;

        DOLOG(1, LOG_SYSCALLS, {
            app_pc base = (app_pc)get_section_address(section_handle);

            LOG(THREAD, LOG_SYSCALLS, IF_DGCDIAG_ELSE(1, 2),
                "syscall post: NtCreateProcess section @" PFX "\n", base);
        });
        if (success && d_r_safe_read(process_handle, sizeof(proc_handle), &proc_handle))
            maybe_inject_into_process(dcontext, proc_handle, NULL, NULL);
    } else if (sysnum == syscalls[SYS_CreateProcessEx]) {
        HANDLE *process_handle = (HANDLE *)postsys_param(dcontext, param_base, 0);
        uint access_mask = (uint)postsys_param(dcontext, param_base, 1);
        uint attributes = (uint)postsys_param(dcontext, param_base, 2);
        uint inherit_from = (uint)postsys_param(dcontext, param_base, 3);
        BOOLEAN inherit = (BOOLEAN)postsys_param(dcontext, param_base, 4);
        HANDLE section_handle = (HANDLE)postsys_param(dcontext, param_base, 5);
        HANDLE debug_handle = (HANDLE)postsys_param(dcontext, param_base, 6);
        HANDLE exception_handle = (HANDLE)postsys_param(dcontext, param_base, 7);
        HANDLE proc_handle;

        /* according to metasploit, others type as HANDLE unknown etc. */
        uint job_member_level = (uint)postsys_param(dcontext, param_base, 8);

        DOLOG(1, LOG_SYSCALLS, {
            if (section_handle != 0) {
                app_pc base = (app_pc)get_section_address(section_handle);
                LOG(THREAD, LOG_SYSCALLS, IF_DGCDIAG_ELSE(1, 2),
                    "syscall: NtCreateProcessEx section @" PFX "\n", base);
            }
        });
        if (success && d_r_safe_read(process_handle, sizeof(proc_handle), &proc_handle))
            maybe_inject_into_process(dcontext, proc_handle, NULL, NULL);
    } else if (sysnum == syscalls[SYS_CreateUserProcess]) {
        postsys_CreateUserProcess(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_UnmapViewOfSection] ||
               sysnum == syscalls[SYS_UnmapViewOfSectionEx]) {
        postsys_UnmapViewOfSection(dcontext, param_base, success);
    } else if (sysnum == syscalls[SYS_DuplicateObject]) {
        postsys_DuplicateObject(dcontext, param_base, success);
#ifdef DEBUG
        /* Check to see if any system calls for which we did non-reversible
         * processing in pre_system_call() failed. FIXME : handle failure
         * cases as needed */
        /* FIXME : because of our stateless apc handling we can't check
         * SYS_Continue for success (all syscalls interrupted by an APC will
         * look like a continue at post)
         */
    } else if (sysnum == syscalls[SYS_CallbackReturn]) {
        /* should never get here, also ref case 4121, except for
         * STATUS_CALLBACK_POP_STACK (case 10579) */
        ASSERT_CURIOSITY((NTSTATUS)postsys_param(dcontext, param_base, 2) ==
                         STATUS_CALLBACK_POP_STACK);
        /* FIXME: should provide a routine to swap the dcontexts back so we can
         * handle any future cases like case 10579 */
    } else if (sysnum == syscalls[SYS_TerminateProcess]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        NTSTATUS exit_status = (NTSTATUS)postsys_param(dcontext, param_base, 1);
        /* FIXME : no way to recover if syscall fails and handle is 0 or us */
        /* Don't allow success && handle == us since we should never get here
         * in that case */
        ASSERT((process_handle == 0 && success) || !is_phandle_me(process_handle));
    } else if (sysnum == syscalls[SYS_TerminateThread]) {
        HANDLE thread_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        ASSERT(thread_handle != 0); /* 0 => current thread */
        if (thread_handle != 0) {
            thread_id_t tid = thread_handle_to_tid(thread_handle);
            process_id_t pid = thread_handle_to_pid(thread_handle, tid);
            ASSERT(tid != d_r_get_thread_id()); /* not current thread */
            /* FIXME : if is thread in this process and syscall fails then
             * no way to recover since we already cleaned up the thread */
            /* Don't allow success && handle == us since we should never get
             * here in that case */
            ASSERT(success || tid == 0xFFFFFFFF /* prob. bad/incorrect type handle */ ||
                   is_thread_exited(thread_handle) == THREAD_EXITED || !is_pid_me(pid));
            if (success && !is_pid_me(pid)) {
                IPC_ALERT("Warning: NtTerminateThread on thread tid=" PFX " "
                          "in other process pid=" PFX,
                          tid, pid);
            }
        }
    } else if (sysnum == syscalls[SYS_CreateThread]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 3);
        CONTEXT *cxt = (CONTEXT *)postsys_param(dcontext, param_base, 5);
        /* FIXME : we are going to read cxt, this is potentially unsafe */
        if (is_first_thread_in_new_process(process_handle, cxt)) {
            /* we might have tried to inject into the process with this
             * new thread, assert curiosity to see if this ever fails */
            ASSERT_CURIOSITY(success);
        }
    } else if (sysnum == syscalls[SYS_FreeVirtualMemory]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        void **pbase = (void **)postsys_param(dcontext, param_base, 1);
        size_t *psize = (size_t *)postsys_param(dcontext, param_base, 2);
        uint type = (uint)postsys_param(dcontext, param_base, 3);
        if (dcontext->expect_last_syscall_to_fail) {
            ASSERT(!success);
        } else {
            /* FIXME i#148: try to recover if the syscall fails, could re-walk this
             * region but that gets us in trouble with the stateful policies */
            ASSERT_CURIOSITY_ONCE(success || !is_phandle_me(process_handle));
            ;
        }
    } else if (sysnum == syscalls[SYS_ProtectVirtualMemory]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        if (dcontext->expect_last_syscall_to_fail) {
            ASSERT(!success);
        } else {
            /* FIXME : try to recover if the syscall fails, could re-walk this
             * region but that gets us in trouble with the stateful policies */
            ASSERT_CURIOSITY(success || !is_phandle_me(process_handle));
        }
    } else if (sysnum == syscalls[SYS_FlushInstructionCache]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        /* Even if this system call fails, doesn't affect our correctness, but
         * lets see if this ever fails, slight false negative risk if it does */
        ASSERT_CURIOSITY(success || !is_phandle_me(process_handle));
    } else if (sysnum == syscalls[SYS_MapUserPhysicalPages]) {
        HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
        /* Even if this system call fails, doesn't affect our correctness, but
         * lets see if this ever fails, slight false negative risk if it does */
        if (dcontext->expect_last_syscall_to_fail) {
            ASSERT(!success);
        } else {
            ASSERT_CURIOSITY(success || !is_phandle_me(process_handle));
        }
#endif /* DEBUG */
    }

    /* The instrument_post_syscall should be called after DR finishes all
     * its operations. Xref to i#1.
     */
    /* i#202: ignore native syscalls in early_inject_init() */
    if (dynamo_initialized)
        instrument_post_syscall(dcontext, sysnum);

    /* i#537 restore app stack for KiFastSystemCallRet
     * this could be in handle_post_system_call@dispatch.c, but seems better
     * here since it is windows specific.
     */
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        KiFastSystemCallRet_address != NULL)
        restore_for_KiFastSystemCallRet(dcontext);

    /* stats lock grabbing ok here, any synch with suspended threads taken
     * care of already */
    RSTATS_INC(post_syscall);
    DOSTATS({
        if (ignorable_system_call(sysnum, NULL, dcontext))
            STATS_INC(post_syscall_ignorable);
    });
    dcontext->whereami = old_whereami;
    DODEBUG({ dcontext->post_syscall = false; });
    KSTOP(post_syscall);
}

/***************************************************************************
 * SYSTEM CALL API
 */

DR_API
reg_t
dr_syscall_get_param(void *drcontext, int param_num)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    /* if we supported this from post-syscall we would need to
     * get dcontext->sys_param_base() and call postsys_param() -- but
     * then it would be confusing vs client checking its set param
     */
    reg_t *param_base = pre_system_call_param_base(mc);
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall,
                  "dr_syscall_get_param() can only be called from pre-syscall event");
    return sys_param(dcontext, param_base, param_num);
}

DR_API
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    reg_t *param_base;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_param() can only be called from a syscall event");
    if (dcontext->client_data->in_pre_syscall)
        param_base = pre_system_call_param_base(mc);
    else
        param_base = dcontext->sys_param_base;
    *sys_param_addr(dcontext, param_base, param_num) = new_value;
}

DR_API
reg_t
dr_syscall_get_result(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_get_result() can only be called from post-syscall event");
    return get_mcontext(dcontext)->xax;
}

DR_API
bool
dr_syscall_get_result_ex(void *drcontext, dr_syscall_result_info_t *info INOUT)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "only call dr_syscall_get_result_ex() from post-syscall event");
    CLIENT_ASSERT(info != NULL, "invalid parameter");
    CLIENT_ASSERT(info->size == sizeof(*info), "invalid dr_syscall_result_info_t size");
    if (info->size != sizeof(*info))
        return false;
    info->value = dr_syscall_get_result(drcontext);
    /* We document not to rely on this for non-ntoskrnl syscalls */
    info->succeeded = NT_SUCCESS(info->value);
    if (info->use_high)
        info->high = 0;
    if (info->use_errno)
        info->errno_value = (uint)info->value;
    return true;
}

DR_API
void
dr_syscall_set_result(void *drcontext, reg_t value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_result() can only be called from a syscall event");
    SET_RETURN_VAL(dcontext, value);
}

DR_API
bool
dr_syscall_set_result_ex(void *drcontext, dr_syscall_result_info_t *info)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "only call dr_syscall_set_result_ex() from a syscall event");
    CLIENT_ASSERT(info != NULL, "invalid parameter");
    CLIENT_ASSERT(info->size == sizeof(*info), "invalid dr_syscall_result_info_t size");
    if (info->size != sizeof(*info))
        return false;
    if (info->use_high)
        return false; /* not supported */
    /* we ignore info->succeeded */
    if (info->use_errno)
        SET_RETURN_VAL(dcontext, info->errno_value);
    else
        SET_RETURN_VAL(dcontext, info->value);
    return true;
}

DR_API
void
dr_syscall_set_sysnum(void *drcontext, int new_num)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_sysnum() can only be called from a syscall event");
    mc->xax = new_num;
}

DR_API
void
dr_syscall_invoke_another(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_invoke_another() can only be called from post-syscall "
                  "event");
    LOG(THREAD, LOG_SYSCALLS, 2, "invoking additional syscall on client request\n");
    /* Dispatch checks this flag immediately upon return from handle_post_system_call()
     * and if set it invokes handle_system_call().
     */
    dcontext->client_data->invoke_another_syscall = true;
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* Since we're not regaining control immediately after sysenter,
         * need to push regain-control retaddr on stack, and then copy esp to edx.
         */
        mc->xsp -= XSP_SZ;
        /* put the post-call-to-vsyscall address, currently in asynch_target, back
         * on stack, and set asynch_target back to post-sysenter pc (will be put
         * into next_tag back in handle_post_system_call())
         */
        *((app_pc *)mc->xsp) = dcontext->asynch_target;
        dcontext->asynch_target = vsyscall_syscall_end_pc;
        mc->xdx = mc->xsp;
    } else if (get_syscall_method() == SYSCALL_METHOD_WOW64) {
        if (get_os_version() == WINDOWS_VERSION_7) {
            /* emulate win7's add 4,esp after the call* in the syscall wrapper */
            mc->xsp += XSP_SZ;
        }
        if (syscall_uses_edx_param_base()) {
            /* perform: lea edx,[esp+0x4] */
            mc->xdx = mc->xsp + XSP_SZ;
        }
    } else if (get_syscall_method() == SYSCALL_METHOD_INT) {
        if (syscall_uses_edx_param_base()) {
            /* perform: lea edx,[esp+0x4] */
            mc->xdx = mc->xsp + XSP_SZ;
        }
    }
#ifdef X64
    else if (get_syscall_method() == SYSCALL_METHOD_SYSCALL) {
        /* sys_param_addr() is already using r10 */
    }
#endif
}

DR_API
bool
dr_syscall_intercept_natively(const char *name, int sysnum, int num_args, int wow64_idx)
{
    uint i, idx;
    if (syscall_extra_idx >= CLIENT_EXTRA_TRAMPOLINE)
        return false;
    if (dynamo_initialized)
        return false;
    /* see whether we already intercept it */
    for (i = 0; i < SYS_MAX + syscall_extra_idx; i++) {
        if (intercept_native_syscall(i) && strcmp(syscall_names[i], name) == 0)
            return true;
    }
    if (d_r_get_proc_address(get_ntdll_base(), name) == NULL)
        return false;
    /* no lock needed since only supported during dr_client_main */
    idx = SYS_MAX + syscall_extra_idx;
    syscall_names[idx] = name;
    syscalls[idx] = sysnum;
    syscall_argsz[idx] = num_args * 4;
    if (wow64_index != NULL)
        wow64_index[idx] = wow64_idx;
    syscall_requires_action[idx] = true;
    syscall_extra_idx++;
    /* some syscalls we just don't support intercepting */
    if (!intercept_native_syscall(idx)) {
        LOG(GLOBAL, LOG_SYSCALLS, 2, "%s: %s is not interceptable!\n", __FUNCTION__,
            name);
        syscall_extra_idx--;
        return false;
    }
    LOG(GLOBAL, LOG_SYSCALLS, 2, "%s: intercepting %s as index %d\n", __FUNCTION__, name,
        idx);
    return true;
}
