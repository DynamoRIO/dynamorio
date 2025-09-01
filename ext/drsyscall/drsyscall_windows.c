/* **********************************************************
 * Copyright (c) 2011-2025 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"
#include <string.h> /* for strcmp */
#include <stddef.h> /* offsetof */

#include "../drmf/wininc/ndk_extypes.h"
#include "../drmf/wininc/ndk_psfuncs.h"
#include "../drmf/wininc/ndk_mmtypes.h"
#include "../drmf/wininc/afd_shared.h"
#include "../drmf/wininc/msafdlib.h"
#include "../drmf/wininc/winioctl.h"
#include "../drmf/wininc/tcpioctl.h"
#include "../drmf/wininc/iptypes_undocumented.h"
#include "../drmf/wininc/ntalpctyp.h"
#include "../drmf/wininc/wdm.h"
#include "../drmf/wininc/ntddk.h"
#include "../drmf/wininc/ntifs.h"
#include "../drmf/wininc/tls.h"
#include "../drmf/wininc/ntpsapi.h"

static app_pc ntdll_base;
dr_os_version_info_t win_ver = {
    sizeof(win_ver),
};
static bool syscall_numbers_unknown;

/***************************************************************************
 * WIN32K.SYS SYSTEM CALL NUMBERS
 */

/* For non-exported syscall wrappers we have tables of numbers */

#define NONE -1

#define IMM32 USER32
#define GDI32 USER32
#define KERNEL32 USER32
#define NTDLL USER32

static const char *const sysnum_names[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
#    n,
#include "drsyscall_numx.h"
#undef USER32
};
#define NUM_SYSNUM_NAMES (sizeof(sysnum_names) / sizeof(sysnum_names[0]))

static const int win10_1803_x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w15x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1803_wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w15wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1803_x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w15x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1709_x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w14x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1709_wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w14wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1709_x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w14x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1703_x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w13x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1703_wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w13wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1703_x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w13x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1607_x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w12x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1607_wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w12wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1607_x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w12x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1511_x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w11x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1511_wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w11wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10_1511_x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w11x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w10x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w10wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win10x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w10x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win81x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w81x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win81wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w81wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win81x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w81x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win8x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w8x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win8wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w8wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win8x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w8x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win7x64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w7x64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win7wow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w7wow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win7x86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w7x86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int vistax64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    vx64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int vistawow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    vwow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int vistax86_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    vx86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int winXPx64_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    xp64,
#include "drsyscall_numx.h"
#undef USER32
};

static const int winXPwow_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    xpwow,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win2003_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w2k3,
#include "drsyscall_numx.h"
#undef USER32
};

static const int winXP_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    xpx86,
#include "drsyscall_numx.h"
#undef USER32
};

static const int win2K_sysnums[] = {
#define USER32(n, w2K, xpx86, w2k3, xpwow, xp64, vx86, vwow, vx64, w7x86, w7wow, w7x64, \
               w8x86, w8wow, w8x64, w81x86, w81wow, w81x64, w10x86, w10wow, w10x64,     \
               w11x86, w11wow, w11x64, w12x86, w12wow, w12x64, w13x86, w13wow, w13x64,  \
               w14x86, w14wow, w14x64, w15x86, w15wow, w15x64)                          \
    w2K,
#include "drsyscall_numx.h"
#undef USER32
};

#undef IMM32
#undef GDI32
#undef KERNEL32
#undef NTDLL

/***************************************************************************
 * NAME TO NUMBER
 */

/* Table that maps syscall names to numbers.  We need to store primary + secondary,
 * and we need to allocate the Zw forms, so we can't avoid a heap-allocated payload.
 */
#define NAME2NUM_TABLE_HASH_BITS 13 /* 1.5K of them, x2 for no-prefix entries + Zw */
static hashtable_t name2num_table;

typedef struct _name2num_entry_t {
    char *name;
    bool name_allocated;
    drsys_sysnum_t num;
} name2num_entry_t;

static void
name2num_entry_free(void *p)
{
    name2num_entry_t *e = (name2num_entry_t *)p;
    if (e->name_allocated)
        global_free(e->name, strlen(e->name) + 1 /*null*/, HEAPSTAT_MISC);
    global_free(e, sizeof(*e), HEAPSTAT_MISC);
}

void
name2num_entry_add(void *drcontext, const char *name, drsys_sysnum_t num, bool dup_Zw,
                   bool dup_name)
{
    name2num_entry_t *e = global_alloc(sizeof(*e), HEAPSTAT_MISC);
    bool ok;
    if (dup_Zw && name[0] == 'N' && name[1] == 't') {
        size_t len = strlen(name) + 1 /*null*/;
        e->name = global_alloc(len, HEAPSTAT_MISC);
        dr_snprintf(e->name, len, "Zw%s", name + 2 /*skip "Nt"*/);
        e->name[len - 1] = '\0';
        e->name_allocated = true;
    } else if (dup_name) {
        e->name = drmem_strdup(name, HEAPSTAT_MISC);
        e->name_allocated = true;
    } else {
        e->name = (char *)name;
        e->name_allocated = false;
    }
    e->num = num;
    LOG(drcontext, SYSCALL_VERBOSE + 1,
        "name2num: adding %s => " SYSNUM_FMT "." SYSNUM_FMT "\n", e->name, num.number,
        num.secondary);
    ok = hashtable_add(&name2num_table, (void *)e->name, (void *)e);
    if (!ok) {
        /* With auto-generated files on a new OS, we may have had a shift from
         * NtUserCallOneParam.FOO to NtUserFoo (this has happened before:
         * WindowFromDC, GetKeyboardLayout).  So we downgrade this to just a warning.
         */
        if (strcmp(e->name, "GetThreadDesktop") != 0 /*i#487*/ &&
            strstr(e->name, "PREPAREFORLOGOFF") == NULL /* NoParam vs OneParam */)
            WARN("WARNING: duplicate entry added to name2num_table: %s\n", e->name);
        name2num_entry_free((void *)e);
    }
}

void
name2num_record(void *drcontext, const char *name, int num, bool dup_name)
{
    const char *skip_prefix = NULL;
    drsys_sysnum_t sysnum = { num, 0 };

    /* Support adding usercalls from a sysnum file. */
    if (strstr(name, "NtUserCall") == name && strchr(name, '.') != NULL) {
        wingdi_add_usercall(drcontext, name, num);
        return;
    }

    name2num_entry_add(drcontext, name, sysnum, false /*no Zw*/, dup_name);

    /* we also add the version without the prefix, so e.g. alloc.c
     * can pass in "UserConnectToServer" without having the
     * optional_prefix param in sysnum_from_name()
     */
    if (strstr(name, "NtUser") == name)
        skip_prefix = name + strlen("NtUser");
    else if (strstr(name, "NtGdi") == name)
        skip_prefix = name + strlen("NtGdi");
    /* We could re-arrange the add_syscall_entry() below and look up the
     * entry to check SYSINFO_REQUIRES_PREFIX, but since GetThreadDesktop
     * is the only one for now, we rely on having GetThreadDesktop before
     * NtUserGetThreadDesktop to avoid the wrong # in the table (i#1418).
     */
    if (skip_prefix != NULL)
        name2num_entry_add(drcontext, skip_prefix, sysnum, false /*no Zw*/, dup_name);
}

/***************************************************************************
 * SYSTEM CALLS FOR WINDOWS
 */

/* We need a hashtable to map system call # to index in table, since syscall #s
 * vary by Windows version.
 */
#define SYSTABLE_HASH_BITS 12 /* has ntoskrnl and win32k.sys */
hashtable_t systable;

/* We need separate hashtable to map syscalls with secondary components in table. */
#define SECONDARY_SYSTABLE_HASH_BITS 10 /*has ntoskrnl and user32 secondary syscalls*/
hashtable_t secondary_systable;

/* Syscalls that need special processing.  The address of each is kept
 * in the syscall_info_t entry so we don't need separate lookup.
 */
drsys_sysnum_t sysnum_CreateThread = { -1, 0 };
drsys_sysnum_t sysnum_CreateThreadEx = { -1, 0 };
drsys_sysnum_t sysnum_CreateUserProcess = { -1, 0 };
drsys_sysnum_t sysnum_DeviceIoControlFile = { -1, 0 };
drsys_sysnum_t sysnum_QueryInformationThread = { -1, 0 };
drsys_sysnum_t sysnum_QuerySystemInformation = { -1, 0 };
drsys_sysnum_t sysnum_QuerySystemInformationWow64 = { -1, 0 };
drsys_sysnum_t sysnum_QuerySystemInformationEx = { -1, 0 };
drsys_sysnum_t sysnum_SetSystemInformation = { -1, 0 };
drsys_sysnum_t sysnum_SetInformationProcess = { -1, 0 };
drsys_sysnum_t sysnum_SetInformationFile = { -1, 0 };
drsys_sysnum_t sysnum_PowerInformation = { -1, 0 };
drsys_sysnum_t sysnum_QueryVirtualMemory = { -1, 0 };
drsys_sysnum_t sysnum_FsControlFile = { -1, 0 };
drsys_sysnum_t sysnum_TraceControl = { -1, 0 };

/* The tables are large, so we separate them into their own files: */
extern syscall_info_t syscall_ntdll_info[];
extern size_t
num_ntdll_syscalls(void);
/* win32k.sys and other non-ntoskrnl syscalls are in syscall_wingdi.c */
extern syscall_info_t syscall_kernel32_info[];
extern size_t
num_kernel32_syscalls(void);
extern syscall_info_t syscall_user32_info[];
extern size_t
num_user32_syscalls(void);
extern syscall_info_t syscall_gdi32_info[];
extern size_t
num_gdi32_syscalls(void);

/* The initial set of entries in drsyscall_numx for which we check the ntdll wrappers
 * to ensure our table is correct.
 */
#define NUM_SPOT_CHECKS 4

/* Takes in any Nt syscall wrapper entry point.
 * Will accept other entry points (e.g., we call it for gdi32!GetFontData)
 * and return -1 for them: up to caller to assert if that shouldn't happen.
 */
static int
syscall_num_from_wrapper(void *drcontext, byte *entry)
{
    /* Presumably the cross-module cost here doesn't matter vs all the
     * calls into DR: if so we should inline the DR calls and maybe
     * have our own copy here (like we used to).
     */
    return drmgr_decode_sysnum_from_wrapper(entry);
}

bool
syscall_num_from_name(void *drcontext, const module_data_t *info, const char *name,
                      const char *optional_prefix, bool sym_lookup,
                      drsys_sysnum_t *num_out DR_PARAM_OUT)
{
    app_pc entry = (app_pc)dr_get_proc_address(info->handle, name);
    int num = -1;
    ASSERT(num_out != NULL, "invalid param");
    if (entry != NULL) {
        /* look for partial map (i#730) */
        if (entry >= info->end) /* XXX: syscall_num will decode a few instrs in */
            return -1;
        num = syscall_num_from_wrapper(drcontext, entry);
    }
    if (entry == NULL && sym_lookup && drsys_ops.lookup_internal_symbol != NULL) {
        /* i#388: for those that aren't exported, if we have symbols, find the
         * sysnum that way.
         */
        /* drsym_init() was called already in utils_init() */
        entry = (*drsys_ops.lookup_internal_symbol)(info, name);
        if (entry != NULL)
            num = syscall_num_from_wrapper(drcontext, entry);
        if (num == -1 && optional_prefix != NULL &&
            strstr(name, optional_prefix) == name) {
            const char *skip_prefix = name + strlen(optional_prefix);
            entry = (*drsys_ops.lookup_internal_symbol)(info, skip_prefix);
            if (entry != NULL)
                num = syscall_num_from_wrapper(drcontext, entry);
        }
    }
    /* Work around DRi#3453: drmgr_decode_sysnum_from_wrapper () should check for
     * "return 1".
     */
    if (num == 1 && strstr(name, "NtUser") == name)
        num = -1;
    if (num == -1)
        return false;
    num_out->number = num;
    num_out->secondary = 0;
    return true;
}

bool
os_syscall_get_num(const char *name, drsys_sysnum_t *num DR_PARAM_OUT)
{
    name2num_entry_t *e =
        (name2num_entry_t *)hashtable_lookup(&name2num_table, (void *)name);
    ASSERT(num != NULL, "invalid param");
    if (e != NULL) {
        *num = e->num;
        return true;
    }
    return false;
}

#ifdef DEBUG
static void
check_syscall_entry(void *drcontext, const module_data_t *info, syscall_info_t *syslist,
                    const char *optional_prefix)
{
    /* i#1521: windows version-specific entry feature */
    if (syslist->num.number != 0 && win_ver.version < syslist->num.number)
        return;
    if (syslist->num.secondary != 0 && win_ver.version > syslist->num.secondary)
        return;
    if (TEST(SYSINFO_REQUIRES_PREFIX, syslist->flags))
        optional_prefix = NULL;
    if (info != NULL) {
        drsys_sysnum_t num_from_wrapper;
        bool ok = syscall_num_from_name(drcontext, info, syslist->name, optional_prefix,
                                        drsys_ops.verify_sysnums, &num_from_wrapper);
        if (ok && !drsys_sysnums_equal(&syslist->num, &num_from_wrapper)) {
            WARN("WARNING: sysnum table " PIFX " != wrapper " PIFX " for %s\n",
                 syslist->num.number, num_from_wrapper.number, syslist->name);
            ASSERT(false, "sysnum table does not match wrapper");
        }
    }
}
#endif

static bool
get_primary_syscall_num(void *drcontext, const module_data_t *info,
                        syscall_info_t *syslist DR_PARAM_OUT, const char *optional_prefix)
{
    bool ok = false;
    /* Windows version-specific entry feature */
    if (syslist->num.number != 0 && win_ver.version < syslist->num.number)
        return ok;
    if (syslist->num.secondary != 0 && win_ver.version > syslist->num.secondary)
        return ok;
    if (TEST(SYSINFO_REQUIRES_PREFIX, syslist->flags))
        optional_prefix = NULL;
    /* i#388: we try our name2num table first.  We need it anyway for wrappers that
     * are not exported or when we don't have symbol info.  The table has both
     * win32k.sys entries (mostly not exported) and ntoskrnl entries (to handle hook
     * conflicts: i#1686).
     */
    ok = os_syscall_get_num(syslist->name, &syslist->num);
    if (!ok && info != NULL) {
        LOG(drcontext, SYSCALL_VERBOSE,
            "looking at wrapper b/c %s not in name2num_table\n", syslist->name);
        ok = syscall_num_from_name(drcontext, info, syslist->name, optional_prefix,
                                   /* it's a perf hit to do one-at-a-time symbol
                                    * lookup for hundreds of syscalls, so we rely
                                    * on our tables unless asked.
                                    * XXX: a single Nt* regex would probably
                                    * be performant enough
                                    */
                                   drsys_ops.verify_sysnums, &syslist->num);
    }
    DOLOG(SYSCALL_VERBOSE, {
        if (!ok) {
            LOG(drcontext, SYSCALL_VERBOSE, "WARNING: could not find system call %s\n",
                syslist->name);
        }
    });

    return ok;
}

/* user should set is_secondary flag to add syscall in secondary hashtable */
static bool
add_syscall_entry(void *drcontext, const module_data_t *info, syscall_info_t *syslist,
                  const char *optional_prefix, bool add_name2num, bool is_secondary)
{
    IF_DEBUG(bool ok;)
    bool result = false;
    if (is_secondary) {
        dr_recurlock_lock(systable_lock);
        IF_DEBUG(ok =)
        hashtable_add(&secondary_systable, (void *)&syslist->num, (void *)syslist);
    } else {
        result = get_primary_syscall_num(drcontext, info, syslist, optional_prefix);
        if (!result)
            return false;
        dr_recurlock_lock(systable_lock);
        IF_DEBUG(ok =)
        hashtable_add(&systable, (void *)&syslist->num, (void *)syslist);
    }
    dr_recurlock_unlock(systable_lock);
    LOG(drcontext, (info != NULL && info->start == ntdll_base) ? 2 : SYSCALL_VERBOSE,
        "system call %-35s = %3d.%d (0x%04x.%x)\n", syslist->name, syslist->num.number,
        syslist->num.secondary, syslist->num.number, syslist->num.secondary);
    /* We do have a dup with GetThreadDesktop on many platforms */
    ASSERT(ok || strcmp(syslist->name, "GetThreadDesktop") == 0 ||
               (strstr(syslist->name, "NtUserCall") == syslist->name &&
                syscall_numbers_unknown),
           "no dups in sys num to call table");
    /* When SYSINFO_SECONDARY_TABLE flag is set, num_out
     * is a pointer to secondary table. So we shouldn't
     * rewrite them here.
     */
    if (syslist->num_out != NULL && !TEST(SYSINFO_SECONDARY_TABLE, syslist->flags))
        *syslist->num_out = syslist->num;
    if (add_name2num) {
        /* Add the Nt variant only if a secondary, which our numx.h table doesn't have */
        if (is_secondary) {
            name2num_entry_add(drcontext, syslist->name, syslist->num, false /*no Zw*/,
                               false);
        }
        /* Add the Zw variant */
        name2num_entry_add(drcontext, syslist->name, syslist->num, true /*dup Zw*/,
                           false);
    }
    return true;
}

/* The routine adds secondary syscall entries in the separate hashtable.
 * User should provide callback routine to add user32 syscall in the hashtable.
 */
static void
secondary_syscall_setup(void *drcontext, const module_data_t *info,
                        syscall_info_t *syslist, drsys_get_secnum_cb_t cb)
{
    uint entry_index;
    uint second_entry_num = 0;
    bool is_ntoskrnl = false;
    IF_DEBUG(bool ok;)
    const char *skip_primary;

    syscall_info_t *syscall_info_second = (syscall_info_t *)syslist->num_out;
    if (cb == NULL)
        is_ntoskrnl = true;

    for (entry_index = 0;
         syscall_info_second[entry_index].num.number != SECONDARY_TABLE_ENTRY_MAX_NUMBER;
         entry_index++) {
        if (syscall_info_second[entry_index].num.number == SECONDARY_TABLE_SKIP_ENTRY)
            continue;
        if (cb != NULL) {
            second_entry_num =
                cb(drcontext, syscall_info_second[entry_index].name, syslist->num.number);
            if (second_entry_num == -1) {
                LOG(drcontext, SYSCALL_VERBOSE,
                    "can't resolve secondary number for %s syscall\n",
                    syscall_info_second[entry_index].name);
                continue;
            }
        } else {
            second_entry_num = entry_index;
        }

        syscall_info_second[entry_index].num.secondary = second_entry_num;
        /* already have primary num */
        syscall_info_second[entry_index].num.number = syslist->num.number;
        IF_DEBUG(ok =)
        add_syscall_entry(drcontext, info, &syscall_info_second[entry_index], NULL,
                          is_ntoskrnl, /* add ntoskrnl syscalls into name2num table */
                          true /*add syscall in secondary hashtable*/);
        ASSERT(ok, "failed to add new syscall in the secondary table");
    }

    entry_index++; /* base entry placed after SECONDARY_TABLE_ENTRY_MAX_NUMBER */

    syscall_info_second[entry_index].num.secondary = BASE_ENTRY_INDEX;
    /* already have primary num */
    syscall_info_second[entry_index].num.number = syslist->num.number;
    /* add base entry */
    IF_DEBUG(ok =)
    add_syscall_entry(drcontext, info, &syscall_info_second[entry_index], NULL,
                      is_ntoskrnl, /* add ntoskrnl syscalls into name2num table */
                      true /*add syscall in secondary hashtable*/);
    ASSERT(ok, "failed to add base entry syscall in the secondary table");
}

drmf_status_t
drsyscall_os_init(void *drcontext)
{
    drmf_status_t res = DRMF_SUCCESS, subres;
    uint i;
    bool ok;
    module_data_t *data;
    bool nums_from_file = false;
    const int *sysnums = NULL; /* array of primary syscall numbers */
    /* XXX i#945: we expect the #s and args of 64-bit windows syscall match
     * wow64, but we have not verified there's no number shifting or arg shifting
     * in the wow64 marshaling layer.
     * XXX i#772: on win8, wow64 does add some upper bits, which we
     * want to honor so that our stateless number-to-name and
     * name-to-number match real numbers.
     */
    bool wow64 = IF_X64_ELSE(true, dr_is_wow64());
    if (!dr_get_os_version(&win_ver)) {
        ASSERT(false, "unable to get version");
        /* guess at latest win10 */
        win_ver.version = DR_WINDOWS_VERSION_10_1803;
        win_ver.service_pack_major = 0;
        win_ver.service_pack_minor = 0;
    }
    switch (win_ver.version) {
    case DR_WINDOWS_VERSION_10_1803:
        sysnums = IF_X64_ELSE(win10_1803_x64_sysnums,
                              wow64 ? win10_1803_wow_sysnums : win10_1803_x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_10_1709:
        sysnums = IF_X64_ELSE(win10_1709_x64_sysnums,
                              wow64 ? win10_1709_wow_sysnums : win10_1709_x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_10_1703:
        sysnums = IF_X64_ELSE(win10_1703_x64_sysnums,
                              wow64 ? win10_1703_wow_sysnums : win10_1703_x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_10_1607:
        sysnums = IF_X64_ELSE(win10_1607_x64_sysnums,
                              wow64 ? win10_1607_wow_sysnums : win10_1607_x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_10_1511:
        sysnums = IF_X64_ELSE(win10_1511_x64_sysnums,
                              wow64 ? win10_1511_wow_sysnums : win10_1511_x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_10:
        sysnums =
            IF_X64_ELSE(win10x64_sysnums, wow64 ? win10wow_sysnums : win10x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_8_1:
        sysnums =
            IF_X64_ELSE(win81x64_sysnums, wow64 ? win81wow_sysnums : win81x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_8:
        sysnums = IF_X64_ELSE(win8x64_sysnums, wow64 ? win8wow_sysnums : win8x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_7:
        sysnums = IF_X64_ELSE(win7x64_sysnums, wow64 ? win7wow_sysnums : win7x86_sysnums);
        break;
    case DR_WINDOWS_VERSION_VISTA:
        sysnums =
            IF_X64_ELSE(vistax64_sysnums, wow64 ? vistawow_sysnums : vistax86_sysnums);
        break;
    case DR_WINDOWS_VERSION_2003:
        sysnums =
            IF_X64_ELSE(winXPx64_sysnums, wow64 ? winXPwow_sysnums : win2003_sysnums);
        break;
    case DR_WINDOWS_VERSION_XP:
        ASSERT(!wow64, "should be 2003 if wow64");
        sysnums = winXP_sysnums;
        break;
    case DR_WINDOWS_VERSION_2000: sysnums = win2K_sysnums; break;
    case DR_WINDOWS_VERSION_NT:
    default:
        /* Unsupported but we try to continue; we'll return
         * DRMF_WARNING_UNSUPPORTED_KERNEL below.
         */
        sysnums = NULL;
        break;
    }

    data = dr_lookup_module_by_name("ntdll.dll");
    ASSERT(data != NULL, "cannot find ntdll.dll");
    if (data == NULL)
        return DRMF_ERROR;
    ntdll_base = data->start;

    /* Set up hashtable for name2num translation at init time.
     * Case-insensitive primarily for NtUserCallOneParam.*.
     */
    hashtable_init_ex(&name2num_table, NAME2NUM_TABLE_HASH_BITS, HASH_STRING_NOCASE,
                      false /*!strdup*/, true /*synch*/, name2num_entry_free, NULL, NULL);
    if (sysnums != NULL && drsys_ops.skip_internal_tables)
        sysnums = NULL;
    if (sysnums != NULL) {
        /* Check whether these match by spot-checking a few (we want to check
         * multiple in case some are hooked or in case an update ends up with
         * some being identical).  We do not want to take the time to check them all.
         */
        for (i = 0; i < NUM_SPOT_CHECKS; i++) {
            drsys_sysnum_t num_from_wrapper, num_from_table;
            bool ok = syscall_num_from_name(drcontext, data, sysnum_names[i], NULL,
                                            false /*exported*/, &num_from_wrapper);
            if (ok && num_from_wrapper.number != sysnums[i]) {
                LOG(drcontext, 1, "Syscall mismatch for %s: wrapper %d vs table %d\n",
                    sysnum_names[i], num_from_wrapper.number, sysnums[i]);
                ELOG(0,
                     "Syscall mismatch detected.  "
                     "Running on unknown kernel version!\n");
                sysnums = NULL;
                break;
            } else if (!ok) {
                WARN("WARNING: failed to spot-check %s\n", sysnum_names[i]);
            }
        }
    }
    if (sysnums != NULL) {
        for (i = NUM_SPOT_CHECKS; i < NUM_SYSNUM_NAMES; i++) {
            if (sysnums[i] != NONE)
                name2num_record(drcontext, sysnum_names[i], sysnums[i], false);
        }
    }

    if (sysnums == NULL) {
        /* i#1908: we support loading numbers from a file */
        if (drsys_ops.sysnum_file == NULL)
            res = DRMF_WARNING_UNSUPPORTED_KERNEL;
        else {
            res = read_sysnum_file(drcontext, drsys_ops.sysnum_file, data);
            if (res != DRMF_SUCCESS) {
                if (dr_file_exists(drsys_ops.sysnum_file)) {
                    NOTIFY_ERROR("%s does not contain an entry for this kernel." NL,
                                 drsys_ops.sysnum_file);
                }
            } else
                nums_from_file = true;
        }
        if (res != DRMF_SUCCESS) {
            /* We'll keep going, relying on wrapper decoding and unknown
             * syscall heuristics.  Unless symbols are available, we expect
             * false positives in graphical apps.  We tell the caller via
             * this return value in case he wants to abort.
             */
            res = DRMF_WARNING_UNSUPPORTED_KERNEL;
            syscall_numbers_unknown = true;
        }
    }

    hashtable_init_ex(&systable, SYSTABLE_HASH_BITS, HASH_INTPTR, false /*!strdup*/,
                      false /*!synch*/, NULL, sysnum_hash, sysnum_cmp);
    /* i#1549: We init additional table for syscalls with secondary components */
    hashtable_init_ex(&secondary_systable, SECONDARY_SYSTABLE_HASH_BITS, HASH_INTPTR,
                      false /*!strdup*/, false /*!synch*/, NULL, sysnum_hash, sysnum_cmp);

    /* Add all entries at process init time, to support drsys_name_to_syscall()
     * for secondary win32k.sys and drsys_number_to_syscall() in dr_init.
     * If the numbers are not known, however, such queries will fail, and
     * we delay adding win32k.sys until module load time.
     */
    for (i = 0; i < num_ntdll_syscalls(); i++) {
        /* check whether syscall has additional entries */
        ok =
            add_syscall_entry(drcontext, data, &syscall_ntdll_info[i], NULL, true, false);
        if (TEST(SYSINFO_SECONDARY_TABLE, syscall_ntdll_info[i].flags) && ok)
            secondary_syscall_setup(drcontext, data, &syscall_ntdll_info[i], NULL);
        DODEBUG({ check_syscall_entry(drcontext, data, &syscall_ntdll_info[i], NULL); });
    }
    if (!syscall_numbers_unknown) {
        for (i = 0; i < num_kernel32_syscalls(); i++) {
            add_syscall_entry(drcontext, NULL, &syscall_kernel32_info[i], NULL,
                              false /*already added*/, false);
        }
    }

    /* wingdi_init will return _UNSUPPORTED_KERNEL if we pass true for 4th param */
    subres = drsyscall_wingdi_init(drcontext, ntdll_base, &win_ver,
                                   !syscall_numbers_unknown && !nums_from_file);
    if (subres != DRMF_SUCCESS) {
        ASSERT(false, "wingdi_init unexpectedly failed");
        res = subres;
    }

    if (!syscall_numbers_unknown) {
        for (i = 0; i < num_user32_syscalls(); i++) {
            /* We ignore SYSINFO_IMM32_DLL here.  We check vs dlls in
             * drsyscall_os_module_load().
             */
            ok = add_syscall_entry(drcontext, NULL, &syscall_user32_info[i], "NtUser",
                                   false /*already added*/, false);
            if (TEST(SYSINFO_SECONDARY_TABLE, syscall_user32_info[i].flags) && ok) {
                secondary_syscall_setup(drcontext, data, &syscall_user32_info[i],
                                        wingdi_get_secondary_syscall_num);
            }
        }
        for (i = 0; i < num_gdi32_syscalls(); i++) {
            add_syscall_entry(drcontext, NULL, &syscall_gdi32_info[i], "NtGdi",
                              false /*already added*/, false);
        }
    }

    dr_free_module_data(data);

    return res;
}

void
drsyscall_os_exit(void)
{
    hashtable_delete(&systable);
    hashtable_delete(&secondary_systable);
    hashtable_delete(&name2num_table);
    drsyscall_wingdi_exit();
}

void
drsyscall_os_thread_init(void *drcontext)
{
    drsyscall_wingdi_thread_init(drcontext);
}

void
drsyscall_os_thread_exit(void *drcontext)
{
    drsyscall_wingdi_thread_exit(drcontext);
}

void
drsyscall_os_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    uint i;
    const char *modname = dr_module_preferred_name(info);
    if (modname == NULL)
        return;

    /* We've already added to the tables at process init time.
     * Here we just check vs the wrapper numbers for other than ntdll
     * (ntdll module was available at process init).
     */
    if (stri_eq(modname, "kernel32.dll") ||
        (win_ver.version >= DR_WINDOWS_VERSION_10_1607 &&
         stri_eq(modname, "win32u.dll"))) {
        for (i = 0; i < num_kernel32_syscalls(); i++) {
            if (syscall_numbers_unknown) {
                add_syscall_entry(drcontext, info, &syscall_kernel32_info[i], NULL, true,
                                  false);
            }
            DODEBUG({
                check_syscall_entry(drcontext, info, &syscall_kernel32_info[i], NULL);
            });
        }
    }
    if (stri_eq(modname, "user32.dll") ||
        (win_ver.version >= DR_WINDOWS_VERSION_10_1607 &&
         stri_eq(modname, "win32u.dll"))) {
        for (i = 0; i < num_user32_syscalls(); i++) {
            if (syscall_numbers_unknown) {
                add_syscall_entry(drcontext, info, &syscall_user32_info[i], "NtUser",
                                  true, false);
                if (TEST(SYSINFO_SECONDARY_TABLE, syscall_user32_info[i].flags)) {
                    secondary_syscall_setup(drcontext, info, &syscall_user32_info[i],
                                            wingdi_get_secondary_syscall_num);
                }
            }
            DODEBUG({
                if (!TEST(SYSINFO_IMM32_DLL, syscall_user32_info[i].flags)) {
                    check_syscall_entry(drcontext, info, &syscall_user32_info[i],
                                        "NtUser");
                }
            });
        }
    }
    if (stri_eq(modname, "imm32.dll") ||
        (win_ver.version >= DR_WINDOWS_VERSION_10_1607 &&
         stri_eq(modname, "win32u.dll"))) {
        DODEBUG({
            for (i = 0; i < num_user32_syscalls(); i++) {
                if (TEST(SYSINFO_IMM32_DLL, syscall_user32_info[i].flags)) {
                    check_syscall_entry(drcontext, info, &syscall_user32_info[i],
                                        "NtUser");
                }
            }
        });
    }
    if (stri_eq(modname, "gdi32.dll") ||
        (win_ver.version >= DR_WINDOWS_VERSION_10_1607 &&
         stri_eq(modname, "win32u.dll"))) {
        for (i = 0; i < num_gdi32_syscalls(); i++) {
            if (syscall_numbers_unknown) {
                add_syscall_entry(drcontext, info, &syscall_gdi32_info[i], "NtGdi", true,
                                  false);
            }
            DODEBUG({
                check_syscall_entry(drcontext, info, &syscall_gdi32_info[i], "NtGdi");
            });
        }
    }
}

/* Though DR's new syscall events provide parameter value access,
 * we need the address of all parameters passed on the stack
 */
static reg_t *
get_sysparam_base(cls_syscall_t *pt)
{
    reg_t *base = (reg_t *)pt->param_base;
    if (is_using_sysenter())
        base += 2;
    else if (IF_X64_ELSE(true,
                         win_ver.version >= DR_WINDOWS_VERSION_8 && is_using_wow64()))
        base += 1; /* retaddr */
    return base;
}

static app_pc
get_sysparam_addr(cls_syscall_t *pt, uint ord)
{
    return (app_pc)(((reg_t *)get_sysparam_base(pt)) + ord);
}

/* Either sets arg->reg to DR_REG_NULL and sets arg->start_addr, or sets arg->reg
 * to non-DR_REG_NULL
 */
void
drsyscall_os_get_sysparam_location(cls_syscall_t *pt, uint argnum, drsys_arg_t *arg)
{
    /* We store the sysparam base so we can answer queries about
     * syscall parameter addresses in post-syscall, where xdx (base
     * for 32-bit) is often clobbered.
     */
#ifdef X64
    arg->reg = DR_REG_NULL;
    switch (argnum) {
    case 0:
        /* The first arg was in rcx, but that's clobbered by OP_sysycall, so the
         * wrapper copies it to r10.  We need to use r10 in case someone (incl
         * our own instru) takes advantage of the dead rcx and clobbers it
         * inside the wrapper (DRi#1901).
         */
        arg->reg = DR_REG_R10;
        break;
    case 1: arg->reg = DR_REG_RDX; break;
    case 2: arg->reg = DR_REG_R8; break;
    case 3: arg->reg = DR_REG_R9; break;
    }
    if (pt->pre)
        pt->param_base = arg->mc->xsp; /* x64 never uses xdx */
    if (arg->reg == DR_REG_NULL) {
        arg->start_addr = get_sysparam_addr(pt, argnum);
    } else {
        arg->start_addr = NULL;
    }
#else
    if (pt->pre) {
        if (win_ver.version >= DR_WINDOWS_VERSION_8 && dr_is_wow64())
            pt->param_base = arg->mc->xsp; /* right on stack */
        else
            pt->param_base = arg->mc->xdx; /* xdx points at args on stack */
    }
    arg->reg = DR_REG_NULL;
    arg->start_addr = get_sysparam_addr(pt, argnum);
#endif
}

bool
os_syscall_ret_small_write_last(syscall_info_t *info, ptr_int_t res)
{
    /* i#486, i#932: syscalls that return the capacity needed in an OUT
     * param will still write to it when returning STATUS_BUFFER_TOO_SMALL
     */
    if (!TEST(SYSINFO_RET_SMALL_WRITE_LAST, info->flags))
        return false;
    if (info->return_type == DRSYS_TYPE_NTSTATUS) {
        return (res == STATUS_BUFFER_TOO_SMALL ||
                res == STATUS_BUFFER_OVERFLOW || /* warning, not error, value */
                res == STATUS_INFO_LENGTH_MISMATCH);
    }
    /* i#1246: it seems weird for a bool return value to do this, b/c what happens
     * if the OUT param for the size is bogus?  There's no other return status.
     * On a bogus param addr we'll raise UNADDR -- maybe not so bad?
     * What else can we do?  Rely on options.is_byte_addressable, or query mem
     * prot -- but then we need the arg value, and drsys_syscall_succeeded() is
     * supposed to not rely on that.
     */
    if (info->return_type == SYSARG_TYPE_BOOL32 || info->return_type == SYSARG_TYPE_BOOL8)
        return (!res);
    return false;
}

/* Returns true if successful, yet we should skip automated table output
 * params as we need custom output handling.
 */
bool
os_syscall_succeeded_custom(drsys_sysnum_t sysnum, syscall_info_t *info,
                            cls_syscall_t *pt)
{
    if (drsys_sysnums_equal(&sysnum, &sysnum_QueryVirtualMemory)) {
        /* i#1547: NtQueryVirtualMemory.MemoryWorkingSetList writes the
         * 1st field of MEMORY_WORKING_SET_LIST when it returns
         * STATUS_INFO_LENGTH_MISMATCH if the size is big enough.
         */
        if (pt->mc.xax == STATUS_INFO_LENGTH_MISMATCH &&
            pt->sysarg[2] == MemoryWorkingSetList && pt->sysarg[4] >= sizeof(ULONG_PTR))
            return true;
    }
    return false;
}

bool
os_syscall_succeeded(drsys_sysnum_t sysnum, syscall_info_t *info, cls_syscall_t *pt)
{
    /* If any output is written, we have to consider the syscall to have succeeded.
     * Else the client may not bother to iterate args post-syscall, and we ourselves
     * do not iterate over table entries.
     */
    bool success;
    ptr_int_t res = (ptr_int_t)pt->mc.xax;
    if (wingdi_syscall_succeeded(sysnum, info, res, &success))
        return success;
    if (os_syscall_succeeded_custom(sysnum, info, pt))
        return true;
    /* if info==NULL we assume specially handled and we don't need to look it up */
    if (info != NULL) {
        if (os_syscall_ret_small_write_last(info, res))
            return true;
        if (TEST(SYSINFO_RET_ZERO_FAIL, info->flags) ||
            info->return_type == SYSARG_TYPE_BOOL32 ||
            info->return_type == SYSARG_TYPE_BOOL8 ||
            info->return_type == DRSYS_TYPE_HANDLE ||
            info->return_type == DRSYS_TYPE_POINTER)
            return (res != 0);
        /* For DRSYS_TYPE_HANDLE, while -1 is INVALID_HANDLE_VALUE, it is
         * also NT_CURRENT_PROCESS, so we rely on any syscalls that return
         * INVALID_HANDLE_VALUE for failure to use SYSINFO_RET_MINUS1_FAIL.
         */
        if (TEST(SYSINFO_RET_MINUS1_FAIL, info->flags))
            return (res != -1);
        if (info->return_type != DRSYS_TYPE_NTSTATUS) {
            /* We don't really know, so safest to assume it succeeded */
            return true;
        }
    }
    /* We fell through on NTSTATUS, or we don't know and we guess it's NTSTATUS */
    if (res == STATUS_BUFFER_OVERFLOW) {
        /* Data is filled in so consider success (i#358) */
        return true;
    }
    return NT_SUCCESS(res);
}

/***************************************************************************
 * SYSTEM CALL TYPE
 */

DR_EXPORT
drmf_status_t
drsys_syscall_type(drsys_syscall_t *syscall, drsys_syscall_type_t *type DR_PARAM_OUT)
{
    syscall_info_t *sysinfo = (syscall_info_t *)syscall;
    if (syscall == NULL || type == NULL)
        return DRMF_ERROR_INVALID_PARAMETER;
    /* We have usercalls which are not in a single table. So we also check
     * that syscall names start with "NtUser" to determine their type.
     */
    if ((sysinfo >= &syscall_user32_info[0] &&
         sysinfo <= &syscall_user32_info[num_user32_syscalls() - 1]) ||
        (strstr(sysinfo->name, "NtUser") == sysinfo->name))
        *type = DRSYS_SYSCALL_TYPE_USER;
    else if (sysinfo >= &syscall_gdi32_info[0] &&
             sysinfo <= &syscall_gdi32_info[num_gdi32_syscalls() - 1])
        *type = DRSYS_SYSCALL_TYPE_GRAPHICS;
    else
        *type = DRSYS_SYSCALL_TYPE_KERNEL;
    return DRMF_SUCCESS;
}

/***************************************************************************
 * SHADOW PER-ARG-TYPE HANDLING
 */

static bool
handle_port_message_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                           app_pc start, uint size)
{
    /* variable-length */
    PORT_MESSAGE pm;
    if (TEST(SYSARG_WRITE, arg_info->flags) && ii->arg->pre &&
        !TEST(SYSARG_READ, arg_info->flags)) {
        /* Struct is passed in uninit w/ max-len buffer after it.
         * XXX i#415: There is some ambiguity over the max, hence we choose
         * the lower estimation to avoid false positives.
         * (We'll still use sizeof(PORT_MESSAGE) + PORT_MAXIMUM_MESSAGE_LENGTH
         *  in the ASSERTs below)
         * We'll re-do the addressability check at the post- hook as part
         * of handling SYSARG_WRITE in any case.
         */
        size = PORT_MAXIMUM_MESSAGE_LENGTH;
    } else if (safe_read(start, sizeof(pm), &pm)) {
        if (pm.u1.s1.DataLength > 0 ||
            /* i#865: sometimes data has 0 length */
            (pm.u1.s1.DataLength == 0 && pm.u1.s1.TotalLength > 0))
            size = pm.u1.s1.TotalLength;
        else
            size = pm.u1.Length;
        if (size > sizeof(PORT_MESSAGE) + PORT_MAXIMUM_MESSAGE_LENGTH) {
            DO_ONCE({ WARN("WARNING: PORT_MESSAGE size larger than known max\n"); });
        }
        /* See above: I've seen 0x15c and 0x130.  Anything too large, though,
         * may indicate an error in our syscall param types, so we want a
         * full stop assert.
         */
        ASSERT(size <= 2 * (sizeof(PORT_MESSAGE) + PORT_MAXIMUM_MESSAGE_LENGTH),
               "PORT_MESSAGE size much larger than expected");
        /* For optional PORT_MESSAGE args I've seen valid pointers to structs
         * filled with 0's
         */
        ASSERT(size == 0 || (ssize_t)size >= sizeof(pm), "PORT_MESSAGE size too small");
        LOG(ii->arg->drcontext, 2, "total size of PORT_MESSAGE arg %d is %d\n",
            arg_info->param, size);
    } else {
        /* can't read real size, so report presumed-unaddr w/ struct size */
        ASSERT(size == sizeof(PORT_MESSAGE), "invalid PORT_MESSAGE sysarg size");
        /* XXX: should we mark arg->valid as false?  though start addr
         * is known: it's just size.  Could change meaning of valid as it's
         * not really used for memargs right now.
         */
    }

    if (!report_memarg(ii, arg_info, start, size, NULL))
        return true;
    return true;
}

static bool
handle_context_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info, app_pc start,
                      uint size)
{
#if !defined(X86)
    ASSERT_NOT_IMPLEMENTED();
#endif
    /* The 'cxt' pointer will only be used for retrieving pointers
     * for the CONTEXT fields, hence we can do without safe_read.
     */
    const CONTEXT *cxt = (CONTEXT *)start;
    DWORD context_flags;
    if (!report_memarg(ii, arg_info, (app_pc)&cxt->ContextFlags,
                       sizeof(cxt->ContextFlags), "CONTEXT.ContextFlags"))
        return true;
    if (!safe_read((void *)&cxt->ContextFlags, sizeof(context_flags), &context_flags)) {
        /* if safe_read fails due to CONTEXT being unaddr, the preceding
         * report_memarg should have raised the error, and there's
         * no point in trying to further check the CONTEXT
         */
        return true;
    }
    if (TESTALL(CONTEXT_DEBUG_REGISTERS, context_flags)) {
#define CONTEXT_NUM_DEBUG_REGS 6
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Dr0,
                           CONTEXT_NUM_DEBUG_REGS * sizeof(DWORD), "CONTEXT.DrX"))
            return true;
    }
    /* Segment registers are 16-bits each but stored with 16-bit gaps
     * so we can't use sizeof(cxt->Seg*s);
     */
#define SIZE_SEGMENT_REG 2
    if (TESTALL(CONTEXT_SEGMENTS, context_flags)) {
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegGs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegGs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegFs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegFs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegEs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegEs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegDs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegDs"))
            return true;
    }
#ifdef X64
    /* For x64:
     * CONTEXT_CONTROL         = SegSs, Rsp, SegCs, Rip, and EFlags.
     * CONTEXT_INTEGER         = Rax, Rcx, Rdx, Rbx, Rbp, Rsi, Rdi, and R8-R15.
     * CONTEXT_SEGMENTS        = SegDs, SegEs, SegFs, and SegGs.
     * CONTEXT_FLOATING_POINT  = Xmm0-Xmm15.
     * CONTEXT_DEBUG_REGISTERS = Dr0-Dr3 and Dr6-Dr7.
     */
    if (TESTALL(CONTEXT_CONTROL, context_flags)) {
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegSs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegSs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Rsp, sizeof(cxt->Rsp),
                           "CONTEXT.Rsp"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegCs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegCs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Rip, sizeof(cxt->Rip),
                           "CONTEXT.Rip"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->EFlags, sizeof(cxt->EFlags),
                           "CONTEXT.Eflags"))
            return true;
    }
    if (TESTALL(CONTEXT_INTEGER, context_flags)) {
        /* Rax through Rbx */
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Rax,
                           (byte *)&cxt->Rsp - (byte *)&cxt->Rax, "CONTEXT.Rax-Rbx"))
            return true;
        /* Rbp through R15 */
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Rbp,
                           (byte *)&cxt->Rip - (byte *)&cxt->Rbp, "CONTEXT.Rbp-R15"))
            return true;
    }
    if (TESTALL(CONTEXT_FLOATING_POINT, context_flags)) {
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Xmm0,
                           (byte *)&cxt->Xmm15 + sizeof(cxt->Xmm15) - (byte *)&cxt->Xmm0,
                           "CONTEXT.XmmX"))
            return true;
    }
#else /* 32-bit X86 */
    ASSERT(TEST(CONTEXT_i486, context_flags),
           "ContextFlags doesn't have CONTEXT_i486 bit set");

    /* CONTEXT structure on x86 consists of the following sections:
     * a) DWORD ContextFlags
     *
     * The following fields should be defined if the corresponding
     * flags are set:
     * b) DWORD Dr{0...3, 6, 7}        - CONTEXT_DEBUG_REGISTERS,
     * c) FLOATING_SAVE_AREA FloatSave - CONTEXT_FLOATING_POINT,
     * d) DWORD Seg{G,F,E,D}s          - CONTEXT_SEGMENTS,
     * e) DWORD E{di,si,bx,dx,cx,ax}   - CONTEXT_INTEGER,
     * f) DWORD Ebp, Eip, SegCs, EFlags, Esp, SegSs - CONTEXT_CONTROL,
     * g) BYTE ExtendedRegisters[...]  - CONTEXT_EXTENDED_REGISTERS.
     */

    if (TESTALL(CONTEXT_FLOATING_POINT, context_flags)) {
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->FloatSave, sizeof(cxt->FloatSave),
                           "CONTEXT.FloatSave"))
            return true;
    }
    if (TESTALL(CONTEXT_INTEGER, context_flags) &&
        ii->arg->sysnum.number != sysnum_CreateThread.number) {
        /* For some reason, cxt->Edi...Eax are not initialized when calling
         * NtCreateThread though CONTEXT_INTEGER flag is set
         */
#    define CONTEXT_NUM_INT_REGS 6
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Edi,
                           CONTEXT_NUM_INT_REGS * sizeof(DWORD), "CONTEXT.Exx"))
            return true;
    }
    if (TESTALL(CONTEXT_CONTROL, context_flags)) {
        if (ii->arg->sysnum.number != sysnum_CreateThread.number) {
            /* Ebp is not initialized when calling NtCreateThread,
             * so we skip it
             */
            if (!report_memarg(ii, arg_info, (app_pc)&cxt->Ebp, sizeof(DWORD),
                               "CONTEXT.Ebp"))
                return true;
        }
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Eip, sizeof(cxt->Eip),
                           "CONTEXT.Eip"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->Esp, sizeof(cxt->Esp),
                           "CONTEXT.Esp"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->EFlags, sizeof(cxt->EFlags),
                           "CONTEXT.Eflags"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegCs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegCs"))
            return true;
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->SegSs, SIZE_SEGMENT_REG,
                           "CONTEXT.SegSs"))
            return true;
    }
    if (TESTALL(CONTEXT_EXTENDED_REGISTERS, context_flags)) {
        if (!report_memarg(ii, arg_info, (app_pc)&cxt->ExtendedRegisters,
                           sizeof(cxt->ExtendedRegisters), "CONTEXT.ExtendedRegisters"))
            return true;
    }
#endif /* X64/X86 */
    /* XXX: handle AVX state too */
    return true;
}

static bool
handle_exception_record_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                               app_pc start, uint size)
{
    const EXCEPTION_RECORD *er = (EXCEPTION_RECORD *)start;
    DWORD num_params;
    /* According to MSDN, NumberParameters stores the number of defined
     * elements of the ExceptionInformation array
     * at the end of the EXCEPTION_RECORD structure.
     * http://msdn.microsoft.com/en-us/library/aa363082(VS.85).aspx
     */
    if (!report_memarg(ii, arg_info, start,
                       sizeof(*er) - sizeof(er->ExceptionInformation),
                       "EXCEPTION_RECORD"))
        return true;
    ASSERT(sizeof(num_params) == sizeof(er->NumberParameters), "");
    if (safe_read((void *)&er->NumberParameters, sizeof(num_params), &num_params)) {
        if (!report_memarg(ii, arg_info, (app_pc)er->ExceptionInformation,
                           num_params * sizeof(er->ExceptionInformation[0]),
                           "EXCEPTION_RECORD.ExceptionInformation"))
            return true;
    }
    return true;
}

static bool
handle_security_qos_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                           app_pc start, uint size)
{
    const SECURITY_QUALITY_OF_SERVICE *s = (SECURITY_QUALITY_OF_SERVICE *)start;
    /* The SECURITY_QUALITY_OF_SERVICE structure is
     * DWORD + DWORD + unsigned char + BOOLEAN
     * so it takes 12 bytes (and its Length field value is 12)
     * but only 10 must be initialized.
     */
    if (!report_memarg(ii, arg_info, start,
                       sizeof(s->Length) + sizeof(s->ImpersonationLevel) +
                           sizeof(s->ContextTrackingMode) + sizeof(s->EffectiveOnly),
                       NULL))
        return true;
    return true;
}

static bool
handle_security_descriptor_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                  app_pc start, uint size)
{
    const SECURITY_DESCRIPTOR *s = (SECURITY_DESCRIPTOR *)start;
    SECURITY_DESCRIPTOR_CONTROL flags;
    ASSERT(s != NULL, "descriptor must not be NULL"); /* caller should check */
    ASSERT(!TEST(SYSARG_WRITE, arg_info->flags), "Should only be called for reads");
    if (!ii->arg->pre) {
        /* Handling pre- is enough for reads */
        return true;
    }
    /* The SECURITY_DESCRIPTOR structure has two fields at the end (Sacl, Dacl)
     * which must be init only when the corresponding bits of Control are set.
     */
    ASSERT(start + sizeof(*s) == (app_pc)&s->Dacl + sizeof(s->Dacl), "");
    if (!report_memarg(ii, arg_info, start, (app_pc)&s->Sacl - start, NULL))
        return true;

    ASSERT(sizeof(flags) == sizeof(s->Control), "");
    if (safe_read((void *)&s->Control, sizeof(flags), &flags)) {
        if (TEST(SE_SACL_PRESENT, flags)) {
            if (!report_memarg(ii, arg_info, (app_pc)&s->Sacl, sizeof(s->Sacl), NULL))
                return true;
        }
        if (TEST(SE_DACL_PRESENT, flags)) {
            if (!report_memarg(ii, arg_info, (app_pc)&s->Dacl, sizeof(s->Dacl), NULL))
                return true;
        }
    }
    return true;
}

bool
handle_unicode_string_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                             app_pc start, uint size, bool ignore_len)
{
    UNICODE_STRING us;
    UNICODE_STRING *arg = (UNICODE_STRING *)start;
    ASSERT(size == sizeof(UNICODE_STRING), "invalid size");

    /* i#99: for optional params, we ignore if NULL. This may lead to false negatives */
    if (arg == NULL)
        return true;

    /* we assume OUT fields just have their Buffer as OUT */
    if (ii->arg->pre) {
        if (TEST(SYSARG_READ, arg_info->flags)) {
            if (!report_memarg(ii, arg_info, (byte *)&arg->Length, sizeof(arg->Length),
                               "UNICODE_STRING.Length"))
                return true;
            /* i#519: MaximumLength may not be initialized in case of IN params. */
        } else {
            if (!report_memarg_type(
                    ii, arg_info->param, SYSARG_READ, (byte *)&arg->MaximumLength,
                    sizeof(arg->MaximumLength), "UNICODE_STRING.MaximumLength",
                    DRSYS_TYPE_UNICODE_STRING, NULL))
                return true;
            /* i#519: Length may not be initialized in case of OUT params. */
        }
        if (!report_memarg(ii, arg_info, (byte *)&arg->Buffer, sizeof(arg->Buffer),
                           "UNICODE_STRING.Buffer"))
            return true;
    }
    if (safe_read((void *)start, sizeof(us), &us)) {
        LOG(ii->arg->drcontext, SYSCALL_VERBOSE,
            "UNICODE_STRING Buffer=" PFX " Length=%d MaximumLength=%d\n",
            (byte *)us.Buffer, us.Length, us.MaximumLength);
        if (ii->arg->pre) {
            if (TEST(SYSARG_READ, arg_info->flags)) {
                /* For IN params, the buffer size is passed as us.Length */
                ASSERT(!ignore_len, "Length must be defined for IN params");
                /* XXX i#519: Length doesn't include NULL, but NULL seems
                 * to be optional, though there is inconsistency.  While it
                 * would be nice to clean up code by complaining if it's
                 * not there, we'd hit false positives in
                 * non-user-controlled code.
                 */
                if (!report_memarg(ii, arg_info, (byte *)us.Buffer, us.Length,
                                   "UNICODE_STRING content"))
                    return true;
            } else {
                /* For OUT params, MaximumLength-sized buffer should be addressable. */
                if (!report_memarg(ii, arg_info, (byte *)us.Buffer, us.MaximumLength,
                                   "UNICODE_STRING capacity"))
                    return true;
            }
        } else if (us.MaximumLength > 0) {
            /* Reminder: we don't do post-processing of IN params. */
            if (ignore_len) {
                /* i#490: wrong Length stored so as workaround we walk the string */
                handle_cwstring(ii, "UNICODE_STRING content", (byte *)us.Buffer,
                                us.MaximumLength, arg_info->param, arg_info->flags, NULL,
                                false);
                if (ii->abort)
                    return true;
            } else {
                if (!report_memarg(ii, arg_info, (byte *)us.Buffer,
                                   /* Length field does not include final NULL.
                                    * We mark it defined even though it may be optional
                                    * in some situations: i#519.
                                    */
                                   us.Length + sizeof(wchar_t), "UNICODE_STRING content"))
                    return true;
            }
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true;
}

bool
handle_object_attributes_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                app_pc start, uint size)
{
    OBJECT_ATTRIBUTES oa;
    OBJECT_ATTRIBUTES *oap = (OBJECT_ATTRIBUTES *)start;
    ASSERT(size == sizeof(OBJECT_ATTRIBUTES), "invalid size");
    /* There's padding between fields on x64 so we split them up. */
    if (!report_memarg(ii, arg_info, (app_pc)&oap->Length, sizeof(oap->Length),
                       "OBJECT_ATTRIBUTES.Length"))
        return true;
    if (!report_memarg(ii, arg_info, (app_pc)&oap->RootDirectory,
                       sizeof(oap->RootDirectory), "OBJECT_ATTRIBUTES.Length"))
        return true;
    if (!report_memarg(ii, arg_info, (app_pc)&oap->ObjectName, sizeof(oap->ObjectName),
                       "OBJECT_ATTRIBUTES.ObjectName"))
        return true;
    if (!report_memarg(ii, arg_info, (app_pc)&oap->Attributes, sizeof(oap->Attributes),
                       "OBJECT_ATTRIBUTES.Attributes"))
        return true;
    if (!report_memarg(ii, arg_info, (app_pc)&oap->SecurityDescriptor,
                       sizeof(oap->SecurityDescriptor),
                       "OBJECT_ATTRIBUTES.SecurityDescriptor"))
        return true;
    if (!report_memarg(ii, arg_info, (app_pc)&oap->SecurityQualityOfService,
                       sizeof(oap->SecurityQualityOfService),
                       "OBJECT_ATTRIBUTES.SecurityQualityOfService"))
        return true;
    if (safe_read((void *)start, sizeof(oa), &oa)) {
        if ((byte *)oa.ObjectName != NULL) {
            handle_unicode_string_access(ii, arg_info, (byte *)oa.ObjectName,
                                         sizeof(*oa.ObjectName), false);
        }
        if (ii->abort)
            return true;
        if ((byte *)oa.SecurityDescriptor != NULL) {
            handle_security_descriptor_access(ii, arg_info, (byte *)oa.SecurityDescriptor,
                                              sizeof(SECURITY_DESCRIPTOR));
        }
        if (ii->abort)
            return true;
        if ((byte *)oa.SecurityQualityOfService != NULL) {
            handle_security_qos_access(ii, arg_info, (byte *)oa.SecurityQualityOfService,
                                       sizeof(SECURITY_QUALITY_OF_SERVICE));
        }
        if (ii->abort)
            return true;
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true;
}

/* pass 0 for size if there is no max size */
bool
handle_cwstring(sysarg_iter_info_t *ii, const char *id, byte *start,
                size_t size /*in bytes*/, int ordinal, uint arg_flags, wchar_t *safe,
                bool check_addr)
{
    /* the kernel wrote a wide string to the buffer: only up to the terminating
     * null should be marked as defined
     */
    uint i;
    wchar_t c;
    /* input params have size 0: for safety stopping at MAX_PATH */
    size_t maxsz = (size == 0) ? (MAX_PATH * sizeof(wchar_t)) : size;
    if (start == NULL)
        return false; /* nothing to do */
    if (ii->arg->pre && !TEST(SYSARG_READ, arg_flags)) {
        if (!check_addr)
            return false;
        if (size > 0) {
            /* if max size specified, on pre-write check whole thing for addr */
            if (!report_memarg_type(ii, ordinal, arg_flags, start, size, id,
                                    DRSYS_TYPE_CSTRING, NULL))
                return true;
            return true;
        }
    }
    if (!ii->arg->pre && !TEST(SYSARG_WRITE, arg_flags))
        return false; /*nothing to do */
    for (i = 0; i < maxsz; i += sizeof(wchar_t)) {
        if (safe != NULL)
            c = safe[i / sizeof(wchar_t)];
        else if (!safe_read(start + i, sizeof(c), &c)) {
            WARN("WARNING: unable to read syscall param string\n");
            break;
        }
        if (c == L'\0')
            break;
    }
    if (!report_memarg_type(ii, ordinal, arg_flags, start,
                            i < maxsz ? (i + sizeof(wchar_t)) : maxsz, id,
                            DRSYS_TYPE_CSTRING, NULL))
        return true;
    return true;
}

static bool
handle_cstring_wide_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                           app_pc start, uint size /*in bytes*/)
{
    return handle_cwstring(ii, NULL, start, size, arg_info->param, arg_info->flags, NULL,
                           /* let normal check ensure full size is addressable (since
                            * OUT user must pass in max size)
                            */
                           false);
}

static bool
handle_alpc_port_attributes_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                   app_pc start, uint size)
{
    ALPC_PORT_ATTRIBUTES *apa = (ALPC_PORT_ATTRIBUTES *)start;
    ASSERT(size == sizeof(ALPC_PORT_ATTRIBUTES), "invalid size");

    if (ii->arg->pre) {
        if (!report_memarg_ex(ii, arg_info->param, DRSYS_PARAM_BOUNDS, start, size,
                              "ALPC_PORT_ATTRIBUTES", DRSYS_TYPE_ALPC_PORT_ATTRIBUTES,
                              NULL, DRSYS_TYPE_INVALID))
            return true;
    }
    if (!report_memarg(ii, arg_info, (byte *)&apa->Flags, sizeof(apa->Flags),
                       "ALPC_PORT_ATTRIBUTES.Flags"))
        return true;
    handle_security_qos_access(ii, arg_info, (byte *)&apa->SecurityQos,
                               sizeof(SECURITY_QUALITY_OF_SERVICE));
    if (ii->abort)
        return true;
    if (!report_memarg(ii, arg_info, (byte *)&apa->MaxMessageLength,
                       ((byte *)&apa->MaxTotalSectionSize) +
                           sizeof(apa->MaxTotalSectionSize) -
                           (byte *)&apa->MaxMessageLength,
                       "ALPC_PORT_ATTRIBUTES MaxMessageLength..MaxTotalSectionSize"))
        return true;
    return true;
}

static bool
handle_alpc_security_attributes_access(sysarg_iter_info_t *ii,
                                       const sysinfo_arg_t *arg_info, app_pc start,
                                       uint size)
{
    ALPC_SECURITY_ATTRIBUTES asa;
    ALPC_SECURITY_ATTRIBUTES *arg = (ALPC_SECURITY_ATTRIBUTES *)start;
    ASSERT(size == sizeof(ALPC_SECURITY_ATTRIBUTES), "invalid size");

    if (!report_memarg(ii, arg_info, start,
                       sizeof(arg->Flags) + sizeof(arg->SecurityQos) +
                           sizeof(arg->ContextHandle),
                       "ALPC_SECURITY_ATTRIBUTES fields"))
        return true;
    if (safe_read((void *)start, sizeof(asa), &asa)) {
        handle_security_qos_access(ii, arg_info, (byte *)asa.SecurityQos,
                                   sizeof(SECURITY_QUALITY_OF_SERVICE));
        if (ii->abort)
            return true;
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true;
}

static bool
handle_alpc_context_attributes_access(sysarg_iter_info_t *ii,
                                      const sysinfo_arg_t *arg_info, app_pc start,
                                      uint size)
{
    /* XXX i#1390: This structure is only used in NtAlpcCancelMessage, and right now only
     * uses three of its fields: MessageContext, MessageID, and CallbackID. This was
     * checked on win7 x86. We should look for updates that use the other fields.
     */
    ALPC_CONTEXT_ATTRIBUTES *aca = (ALPC_CONTEXT_ATTRIBUTES *)start;
    ASSERT(size == sizeof(ALPC_CONTEXT_ATTRIBUTES), "invalid size");

    if (ii->arg->pre) {
        if (!report_memarg_ex(ii, arg_info->param, DRSYS_PARAM_BOUNDS, start, size,
                              "ALPC_CONTEXT_ATTRIBUTES",
                              DRSYS_TYPE_ALPC_CONTEXT_ATTRIBUTES, NULL,
                              DRSYS_TYPE_INVALID))
            return true;
    }
    if (!report_memarg(ii, arg_info, (byte *)&aca->MessageContext,
                       sizeof(aca->MessageContext),
                       "ALPC_CONTEXT_ATTRIBUTES.MessageContext"))
        return true;
    if (!report_memarg(ii, arg_info, (byte *)&aca->MessageID,
                       sizeof(aca->MessageID) + sizeof(aca->CallbackID),
                       "ALPC_CONTEXT_ATTRIBUTES MessageID..CallbackID"))
        return true;
    return true;
}

static bool
handle_alpc_message_attributes_access(sysarg_iter_info_t *ii,
                                      const sysinfo_arg_t *arg_info, app_pc start,
                                      uint size)
{
    /* ALPC attributes are initially passed in by the server or client when a message is
     * sent. A user can request the kernel to expose the attributes back. These attributes
     * are structs laid out one after the other in a particular order, w/ the flags
     * indicating which structs are exposed. The logic was reverse engineered from
     * nt!AlpcpExposeAttributes. The kernel will fill the ValidAttributes field to
     * indicate which attributes were actually exposed.
     */
    ALPC_MESSAGE_ATTRIBUTES ama;
    ALPC_MESSAGE_ATTRIBUTES *arg = (ALPC_MESSAGE_ATTRIBUTES *)start;
    ULONG attributes;
    size_t delta = sizeof(ALPC_MESSAGE_ATTRIBUTES);
    if (safe_read((void *)start, sizeof(ama), &ama)) {
        if (ii->arg->pre) {
            /* AllocatedAttributes needs to be defined */
            if (!report_memarg_type(ii, arg_info->param, SYSARG_READ,
                                    (byte *)&arg->AllocatedAttributes,
                                    sizeof(arg->AllocatedAttributes),
                                    "ALPC_MESSAGE_ATTRIBUTES AllocatedAttributes",
                                    DRSYS_TYPE_ALPC_MESSAGE_ATTRIBUTES, NULL))
                return true;
            attributes = ama.AllocatedAttributes;
        } else {
            attributes = ama.ValidAttributes;
        }
        if (!report_memarg_type(
                ii, arg_info->param, SYSARG_WRITE, (byte *)&arg->ValidAttributes,
                sizeof(arg->ValidAttributes), "ALPC_MESSAGE_ATTRIBUTES ValidAttributes",
                DRSYS_TYPE_ALPC_MESSAGE_ATTRIBUTES, NULL))
            return true;
        if (TEST(ALPC_MESSAGE_SECURITY_ATTRIBUTE, attributes)) {
            /* Kernel does not write SecurityQos field. */
            if (!report_memarg_type(ii, arg_info->param, SYSARG_WRITE, start + delta,
                                    sizeof(((ALPC_SECURITY_ATTRIBUTES *)0)->Flags),
                                    "exposed ALPC_SECURITY_ATTRIBUTES Flags",
                                    DRSYS_TYPE_STRUCT, NULL))
                return true;
            if (!report_memarg_type(
                    ii, arg_info->param, SYSARG_WRITE,
                    start + delta + offsetof(ALPC_SECURITY_ATTRIBUTES, ContextHandle),
                    sizeof(((ALPC_SECURITY_ATTRIBUTES *)0)->ContextHandle),
                    "exposed ALPC_SECURITY_ATTRIBUTES ContextHandle", DRSYS_TYPE_STRUCT,
                    NULL))
                return true;
            delta = sizeof(ALPC_SECURITY_ATTRIBUTES);
        }
        if (TEST(ALPC_MESSAGE_VIEW_ATTRIBUTE, attributes)) {
            /* XXX: The kernel performs checks for each attribute, however it is checked
             * against AllocatedAttributes masked w/ ALPC_MESSAGE_SECURITY_ATTRIBUTE thus
             * making the additional checks unnecessary. Kernel does not write
             * SectionHandle field.
             */
            if (!report_memarg_type(ii, arg_info->param, SYSARG_WRITE, start + delta,
                                    sizeof(((ALPC_DATA_VIEW *)0)->Flags),
                                    "exposed ALPC_DATA_VIEW Flags", DRSYS_TYPE_STRUCT,
                                    NULL))
                return true;
            if (!report_memarg_type(ii, arg_info->param, SYSARG_WRITE,
                                    start + delta + offsetof(ALPC_DATA_VIEW, ViewBase),
                                    sizeof(((ALPC_DATA_VIEW *)0)->ViewBase) +
                                        sizeof(((ALPC_DATA_VIEW *)0)->ViewSize),
                                    "exposed ALPC_DATA_VIEW ViewBase..ViewSize",
                                    DRSYS_TYPE_STRUCT, NULL))
                return true;
            delta += sizeof(ALPC_DATA_VIEW);
        }
        if (TEST(ALPC_MESSAGE_CONTEXT_ATTRIBUTE, attributes)) {
            if (!report_memarg_type(ii, arg_info->param, SYSARG_WRITE, start + delta,
                                    sizeof(ALPC_CONTEXT_ATTRIBUTES),
                                    "exposed ALPC_CONTEXT_ATTRIBUTES", DRSYS_TYPE_STRUCT,
                                    NULL))
                return true;
            delta += sizeof(ALPC_CONTEXT_ATTRIBUTES);
        }
        if (TEST(ALPC_MESSAGE_HANDLE_ATTRIBUTE, attributes)) {
            if (!report_memarg_type(ii, arg_info->param, SYSARG_WRITE, start + delta,
                                    sizeof(ALPC_HANDLE_ATTRIBUTES),
                                    "exposed ALPC_MESSAGE_HANDLE_ATTRIBUTES",
                                    DRSYS_TYPE_STRUCT, NULL))
                return true;
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true;
}

static bool
handle_t2_set_parameters_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                app_pc start, uint size)
{
    PT2_SET_PARAMETERS params = (PT2_SET_PARAMETERS)start;
    ASSERT(size == sizeof(T2_SET_PARAMETERS), "invalid size");

    if (ii->arg->pre) {
        if (!report_memarg_ex(ii, arg_info->param, DRSYS_PARAM_BOUNDS, start, size,
                              "T2_SET_PARAMETERS", DRSYS_TYPE_T2_SET_PARAMETERS, NULL,
                              DRSYS_TYPE_INVALID))
            return true;
    }
    if (!report_memarg(ii, arg_info, (byte *)&params->Version, sizeof(params->Version),
                       "T2_SET_PARAMETERS.Version"))
        return true;
    if (!report_memarg(ii, arg_info, (byte *)&params->NoWakeTolerance,
                       sizeof(params->NoWakeTolerance),
                       "T2_SET_PARAMETERS.NoWakeTolerance"))
        return true;
    return true;
}

static bool
os_handle_syscall_arg_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                             app_pc start, uint size)
{
    if (!TEST(SYSARG_COMPLEX_TYPE, arg_info->flags))
        return false;

    switch (arg_info->misc) {
    case SYSARG_TYPE_PORT_MESSAGE:
        return handle_port_message_access(ii, arg_info, start, size);
    case SYSARG_TYPE_CONTEXT: return handle_context_access(ii, arg_info, start, size);
    case SYSARG_TYPE_EXCEPTION_RECORD:
        return handle_exception_record_access(ii, arg_info, start, size);
    case SYSARG_TYPE_SECURITY_QOS:
        return handle_security_qos_access(ii, arg_info, start, size);
    case SYSARG_TYPE_SECURITY_DESCRIPTOR:
        return handle_security_descriptor_access(ii, arg_info, start, size);
    case SYSARG_TYPE_UNICODE_STRING:
        return handle_unicode_string_access(ii, arg_info, start, size, false);
    case SYSARG_TYPE_UNICODE_STRING_NOLEN:
        return handle_unicode_string_access(ii, arg_info, start, size, true);
    case SYSARG_TYPE_OBJECT_ATTRIBUTES:
        return handle_object_attributes_access(ii, arg_info, start, size);
    case SYSARG_TYPE_CSTRING_WIDE:
        return handle_cstring_wide_access(ii, arg_info, start, size);
    case SYSARG_TYPE_ALPC_PORT_ATTRIBUTES:
        return handle_alpc_port_attributes_access(ii, arg_info, start, size);
    case SYSARG_TYPE_ALPC_SECURITY_ATTRIBUTES:
        return handle_alpc_security_attributes_access(ii, arg_info, start, size);
    case SYSARG_TYPE_ALPC_CONTEXT_ATTRIBUTES:
        return handle_alpc_context_attributes_access(ii, arg_info, start, size);
    case SYSARG_TYPE_ALPC_MESSAGE_ATTRIBUTES:
        return handle_alpc_message_attributes_access(ii, arg_info, start, size);
    case SYSARG_TYPE_T2_SET_PARAMETERS:
        return handle_t2_set_parameters_access(ii, arg_info, start, size);
    }
    return wingdi_process_arg(ii, arg_info, start, size);
}

bool
os_handle_pre_syscall_arg_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                 app_pc start, uint size)
{
    return os_handle_syscall_arg_access(ii, arg_info, start, size);
}

bool
os_handle_post_syscall_arg_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                                  app_pc start, uint size)
{
    return os_handle_syscall_arg_access(ii, arg_info, start, size);
}

/***************************************************************************
 * SHADOW PER-SYSCALL HANDLING
 */

typedef LONG KPRIORITY;
typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;
typedef PROCESS_BASIC_INFORMATION *PPROCESS_BASIC_INFORMATION;

GET_NTDLL(NtQueryInformationProcess,
          (DR_PARAM_IN HANDLE ProcessHandle,
           DR_PARAM_IN PROCESSINFOCLASS ProcessInformationClass,
           DR_PARAM_OUT PVOID ProcessInformation,
           DR_PARAM_IN ULONG ProcessInformationLength,
           DR_PARAM_OUT PULONG ReturnLength OPTIONAL));

static TEB *
get_TEB(void)
{
#ifdef X64
    return (TEB *)__readgsqword(offsetof(TEB, Self));
#else
    return (TEB *)__readfsdword(offsetof(TEB, Self));
#endif
}

static uint
getpid(void)
{
    return (uint)(ptr_uint_t)get_TEB()->ClientId.UniqueProcess;
}

DR_EXPORT
drmf_status_t
drsys_handle_is_current_process(HANDLE h, bool *current)
{
    uint pid, got;
    PROCESS_BASIC_INFORMATION info;
    NTSTATUS res;
    if (current == NULL)
        return DRMF_ERROR_INVALID_PARAMETER;
    if (h == NT_CURRENT_PROCESS) {
        *current = true;
        return DRMF_SUCCESS;
    }
    if (h == NULL) {
        *current = false;
        return DRMF_SUCCESS;
    }
    memset(&info, 0, sizeof(PROCESS_BASIC_INFORMATION));
    res = NtQueryInformationProcess(h, ProcessBasicInformation, &info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);
    if (!NT_SUCCESS(res) || got != sizeof(PROCESS_BASIC_INFORMATION)) {
        /* Each handle has privileges associated with it, and it seems possible
         * to obtain a handle to your own process that has no query privilege,
         * so we relax the assert if it is access denied.
         */
        if (res == STATUS_ACCESS_DENIED)
            return DRMF_ERROR_ACCESS_DENIED;
        ASSERT(false, "internal error");
        /* better to have false positives than negatives? */
        return DRMF_ERROR;
    }
    *current = (info.UniqueProcessId == getpid());
    return DRMF_SUCCESS;
}

static void
handle_post_CreateThread(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    if (NT_SUCCESS(dr_syscall_get_result(drcontext))) {
        /* Even on XP+ where csrss frees the stack, the stack alloc happens
         * in-process and we see it.  The TEB alloc, however, is done by
         * the kernel, and kernel32!CreateRemoteThread writes to the TEB
         * prior to the thread resuming, so we handle it here.
         * We also process the TEB in set_thread_initial_structures() in
         * case someone creates a thread remotely, or in-process but custom
         * so it's not suspended at this point.
         */
        HANDLE thread_handle;
        bool cur_proc;
        /* If not suspended, let set_thread_initial_structures() handle it to
         * avoid races: though since setting as defined the only race would be
         * the thread exiting
         */
        if (pt->sysarg[7] /*bool suspended*/ &&
            drsys_handle_is_current_process((HANDLE)pt->sysarg[3], &cur_proc) ==
                DRMF_SUCCESS &&
            cur_proc &&
            safe_read((byte *)pt->sysarg[0], sizeof(thread_handle), &thread_handle)) {
            /* XXX: this is a new thread.  Should we tell the user to treat
             * its TEB as newly defined memory?
             */
        }
    }
}

static void
handle_pre_CreateThreadEx(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    bool cur_proc;
    if (drsys_handle_is_current_process((HANDLE)pt->sysarg[3], &cur_proc) ==
            DRMF_SUCCESS &&
        cur_proc) {
        create_thread_info_t info;
        if (safe_read(&((create_thread_info_t *)pt->sysarg[10])->struct_size,
                      sizeof(info.struct_size), &info.struct_size)) {
            if (info.struct_size > sizeof(info)) {
                DO_ONCE({ WARN("WARNING: create_thread_info_t size too large\n"); });
                info.struct_size = sizeof(info); /* avoid overflowing the struct */
            }
            if (safe_read((byte *)pt->sysarg[10], info.struct_size, &info)) {
                if (!report_memarg_type(ii, 10, SYSARG_READ, (byte *)pt->sysarg[10],
                                        info.struct_size, "create_thread_info_t",
                                        DRSYS_TYPE_STRUCT, NULL))
                    return;
                if (info.struct_size > offsetof(create_thread_info_t, client_id)) {
                    if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.client_id.buffer,
                                            info.client_id.buffer_size, "PCLIENT_ID",
                                            DRSYS_TYPE_STRUCT, NULL))
                        return;
                }
                if (info.struct_size > offsetof(create_thread_info_t, teb)) {
                    /* This is optional, and omitted in i#342 */
                    if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.teb.buffer,
                                            info.teb.buffer_size, "PTEB",
                                            DRSYS_TYPE_STRUCT, NULL))
                        return;
                }
            }
        }
    }
}

static void
handle_post_CreateThreadEx(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    bool cur_proc;
    if (drsys_handle_is_current_process((HANDLE)pt->sysarg[3], &cur_proc) ==
            DRMF_SUCCESS &&
        cur_proc && NT_SUCCESS(dr_syscall_get_result(drcontext))) {
        HANDLE thread_handle;
        create_thread_info_t info;
        /* See notes in handle_post_CreateThread() */
        if (pt->sysarg[6] /*bool suspended*/ &&
            safe_read((byte *)pt->sysarg[0], sizeof(thread_handle), &thread_handle)) {
            /* XXX: this is a new thread.  Should we tell the user to treat
             * its TEB as newly defined memory?
             */
        }
        if (safe_read(&((create_thread_info_t *)pt->sysarg[10])->struct_size,
                      sizeof(info.struct_size), &info.struct_size)) {
            if (info.struct_size > sizeof(info)) {
                info.struct_size = sizeof(info); /* avoid overflowing the struct */
            }
            if (safe_read((byte *)pt->sysarg[10], info.struct_size, &info)) {
                if (info.struct_size > offsetof(create_thread_info_t, client_id)) {
                    if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.client_id.buffer,
                                            info.client_id.buffer_size, "PCLIENT_ID",
                                            DRSYS_TYPE_STRUCT, NULL))
                        return;
                }
                if (info.struct_size > offsetof(create_thread_info_t, teb)) {
                    if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.teb.buffer,
                                            info.teb.buffer_size, "PTEB",
                                            DRSYS_TYPE_STRUCT, NULL))
                        return;
                }
            }
        }
    }
}

static void
handle_pre_CreateUserProcess(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    create_proc_thread_info_t info;
    if (safe_read((byte *)pt->sysarg[10], sizeof(info), &info)) {
        if (!report_memarg_type(ii, 10, SYSARG_READ, info.nt_path_to_exe.buffer,
                                info.nt_path_to_exe.buffer_size, "path to exe",
                                DRSYS_TYPE_CWARRAY, param_type_names[DRSYS_TYPE_CWARRAY]))
            return;
        if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.client_id.buffer,
                                info.client_id.buffer_size, "PCLIENT_ID",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.exe_stuff.buffer,
                                info.exe_stuff.buffer_size, "exe stuff",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        /* XXX i#98: there are other IN/OUT params but exact form not clear */
    }
}

static void
handle_post_CreateUserProcess(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    if (NT_SUCCESS(dr_syscall_get_result(drcontext))) {
        create_proc_thread_info_t info;
        if (safe_read((byte *)pt->sysarg[10], sizeof(info), &info)) {
            if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.client_id.buffer,
                                    info.client_id.buffer_size, "PCLIENT_ID",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (!report_memarg_type(ii, 10, SYSARG_WRITE, info.exe_stuff.buffer,
                                    info.exe_stuff.buffer_size, "exe_stuff",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            /* XXX i#98: there are other IN/OUT params but exact form not clear */
        }
    }
}

static void
handle_QueryInformationThread(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Some cases are more complex than a single write. */
    THREADINFOCLASS cls = (THREADINFOCLASS)pt->sysarg[1];
    if (cls == ThreadTebInformation) { /* i#1885 */
        THREAD_TEB_INFORMATION info;
        if (!ii->arg->pre && NT_SUCCESS(dr_syscall_get_result(drcontext)) &&
            safe_read((byte *)pt->sysarg[2], sizeof(info), &info)) {
            if (!report_memarg_type(ii, 1, SYSARG_WRITE, info.OutputBuffer,
                                    info.BytesToRead, "TebInfo", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    }
}

static void
handle_QuerySystemInformation(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Normally the buffer is just output.  For the input case here we
     * will mark the buffer as defined b/c of the regular table processing:
     * not a big deal as we'll report any uninit prior to that.
     */
    SYSTEM_INFORMATION_CLASS cls = (SYSTEM_INFORMATION_CLASS)pt->sysarg[0];
    uint out_index =
        drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformationEx) ? 3 : 1;
    if (cls == SystemSessionProcessesInformation) {
        SYSTEM_SESSION_PROCESS_INFORMATION buf;
        if (ii->arg->pre) {
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)pt->sysarg[out_index],
                                    sizeof(buf), "SYSTEM_SESSION_PROCESS_INFORMATION",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        if (safe_read((byte *)pt->sysarg[out_index], sizeof(buf), &buf)) {
            if (!report_memarg_type(ii, 1, SYSARG_WRITE, buf.Buffer, buf.SizeOfBuf,
                                    "Buffer", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    }
    /* i#932: The kernel always writes the size needed info ReturnLength, even
     * on error.  However, for some classes of info, Nebbet claims this value
     * may be zero.  For DrMemory, we can handle this with
     * SYSINFO_RET_SMALL_WRITE_LAST.
     */
}

static void
handle_SetSystemInformation(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Normally the buffer is just input, but some info classes write data */
    SYSTEM_INFORMATION_CLASS cls = (SYSTEM_INFORMATION_CLASS)pt->sysarg[0];
    if (ii->arg->pre)
        return;
    /* Nebbett had this as SystemLoadImage and SYSTEM_LOAD_IMAGE */
    if (cls == SystemLoadGdiDriverInformation) {
        SYSTEM_GDI_DRIVER_INFORMATION *buf =
            (SYSTEM_GDI_DRIVER_INFORMATION *)pt->sysarg[1];
        if (!report_memarg_type(ii, 1, SYSARG_WRITE, (byte *)&buf->ImageAddress,
                                sizeof(*buf) -
                                    offsetof(SYSTEM_GDI_DRIVER_INFORMATION, ImageAddress),
                                "loaded image info", DRSYS_TYPE_STRUCT, NULL))
            return;
        /* Nebbett had this as SystemCreateSession and SYSTEM_CREATE_SESSION */
    } else if (cls == SystemSessionCreate) {
        /* Just a ULONG, no struct */
        if (!report_memarg_type(ii, 1, SYSARG_WRITE, (byte *)pt->sysarg[1], sizeof(ULONG),
                                "session id", DRSYS_TYPE_INT, NULL))
            return;
    }
}

static void
handle_SetInformationProcess(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Normally the buffer is just input, but some info classes write data */
    PROCESSINFOCLASS cls = (PROCESSINFOCLASS)pt->sysarg[1];
    if (cls == ProcessTlsInformation) {
        /* i#1228: the struct is mostly OUT */
        PROCESS_TLS_INFORMATION *buf = (PROCESS_TLS_INFORMATION *)pt->sysarg[2];
        size_t bufsz = (size_t)pt->sysarg[3];
        if (ii->arg->pre) {
            if (!report_memarg_type(ii, 2, SYSARG_READ, (byte *)buf,
                                    offsetof(PROCESS_TLS_INFORMATION, ThreadData),
                                    "input fields", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        if (!report_memarg_type(ii, 2, SYSARG_WRITE, (byte *)&buf->ThreadData,
                                /* XXX: not sure how much it writes.  For now we
                                 * mark the whole capacity.  Does the kernel
                                 * write the written size somewhere?
                                 */
                                bufsz - offsetof(PROCESS_TLS_INFORMATION, ThreadData),
                                "output data", DRSYS_TYPE_STRUCT, NULL))
            return;
    } else if (cls == ProcessThreadStackAllocation) {
        /* i#1563: the struct contains an OUT field */
        if (win_ver.version == DR_WINDOWS_VERSION_VISTA) {
            STACK_ALLOC_INFORMATION_VISTA *buf =
                (STACK_ALLOC_INFORMATION_VISTA *)pt->sysarg[2];
            size_t bufsz = (size_t)pt->sysarg[3];
            if (ii->arg->pre) {
                if (!report_memarg_type(
                        ii, 2, SYSARG_READ, (byte *)buf,
                        MIN(bufsz, offsetof(STACK_ALLOC_INFORMATION_VISTA, BaseAddress)),
                        "input fields", DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
            if (bufsz >= sizeof(*buf) &&
                !report_memarg_type(ii, 2, SYSARG_WRITE, (byte *)&buf->BaseAddress,
                                    sizeof(buf->BaseAddress), "output data",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        } else {
            STACK_ALLOC_INFORMATION *buf = (STACK_ALLOC_INFORMATION *)pt->sysarg[2];
            size_t bufsz = (size_t)pt->sysarg[3];
            if (ii->arg->pre) {
                if (!report_memarg_type(
                        ii, 2, SYSARG_READ, (byte *)buf,
                        MIN(bufsz, offsetof(STACK_ALLOC_INFORMATION, BaseAddress)),
                        "input fields", DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
            if (bufsz >= sizeof(*buf) &&
                !report_memarg_type(ii, 2, SYSARG_WRITE, (byte *)&buf->BaseAddress,
                                    sizeof(buf->BaseAddress), "output data",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    } else {
        if (ii->arg->pre) {
            /* In table this would be "{2, -3, R}" */
            if (!report_memarg_type(ii, 2, SYSARG_READ, (byte *)pt->sysarg[2],
                                    pt->sysarg[3], "ProcessInformation",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    }
}

static void
handle_SetInformationFile(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    FILE_INFORMATION_CLASS cls = (FILE_INFORMATION_CLASS)pt->sysarg[4];
    byte *info = (byte *)pt->sysarg[2];
    ULONG length = (ULONG)pt->sysarg[3];

    /* In table pt->sysarg[2] would be "{2, -3, R}" */
    if (pt->pre) {
        /* pre-syscall */
        /* i#1290: we split checks on fields with padding to avoid false positive
         * UNINIT error reports.
         * We still merge multiple fields with a single check for better performance,
         * and the layout asusmption is checked in app_suite/fs_tests_win.cpp test.
         */
        switch (cls) {
        case FileBasicInformation: {
            /* sizeof(LARGE_INTEGER)*4 + sizeof(ULONG): 36
             * sizeof(FILE_BASIC_INFORMATION): 40, so there are padding there.
             */
            FILE_BASIC_INFORMATION *basic_info;
            basic_info = (FILE_BASIC_INFORMATION *)info;
            if (!report_memarg_type(ii, 2, SYSARG_READ, (byte *)basic_info,
                                    sizeof(LARGE_INTEGER) * 4,
                                    "FILE_BASIC_INFORMATION.*Time", DRSYS_TYPE_STRUCT,
                                    "FILE_BASIC_INFORMATION"))
                return;
            if (!report_memarg_type(ii, 2, SYSARG_READ,
                                    (byte *)&basic_info->FileAttributes,
                                    sizeof(basic_info->FileAttributes),
                                    "FILE_BASIC_INFORMATION.FileAttributes",
                                    DRSYS_TYPE_STRUCT, "FILE_BASIC_INFORMATION"))
                return;
            break;
        }
        case FileLinkInformation:
        case FileRenameInformation: {
            /* FILE_RENAME_INFORMATION has the same struct as
             * FILE_LINK_INFORMATION
             */
            FILE_LINK_INFORMATION *link_info;
            ULONG name_length;
            link_info = (FILE_LINK_INFORMATION *)info;
            if (!report_memarg_type(ii, 2, SYSARG_READ,
                                    (byte *)&link_info->ReplaceIfExists,
                                    sizeof(link_info->ReplaceIfExists),
                                    "FILE_{LINK,RENAME}_INFORMATION.ReplaceIfExists",
                                    DRSYS_TYPE_STRUCT, "FILE_{LINK,RENAME}_INFORMATION"))
                return;
            if (!report_memarg_type(ii, 2, SYSARG_READ, (byte *)&link_info->RootDirectory,
                                    offsetof(FILE_LINK_INFORMATION, FileName) -
                                        offsetof(FILE_LINK_INFORMATION, RootDirectory),
                                    "FILE_{LINK,RENAME}_INFORMATION.RootDirectory "
                                    "and FileNameLength",
                                    DRSYS_TYPE_STRUCT, "FILE_{LINK,RENAME}_INFORMATION"))
                return;
            if (safe_read((ULONG *)&link_info->FileNameLength, sizeof(name_length),
                          &name_length) &&
                name_length != 0) {
                if (!report_memarg_type(
                        ii, 2, SYSARG_READ, (byte *)&link_info->FileName, name_length,
                        "FILE_{LINK,RENAME}_INFORMATION.FileName", DRSYS_TYPE_CWARRAY,
                        "FILE_{LINK,RENAME}_INFORMATION.FileName"))
                    return;
            }
            break;
        }
        case FileShortNameInformation: {
            FILE_NAME_INFORMATION *name_info;
            ULONG name_length;
            name_info = (FILE_NAME_INFORMATION *)info;
            if (!report_memarg_type(ii, 2, SYSARG_READ,
                                    (byte *)&name_info->FileNameLength,
                                    sizeof(name_info->FileNameLength),
                                    "FILE_NAME_INFORMATION.FileNameLength",
                                    DRSYS_TYPE_STRUCT, "FILE_NAME_INFORMATION"))
                return;
            if (safe_read((ULONG *)&name_info->FileNameLength, sizeof(name_length),
                          &name_length) &&
                name_length > 0) {
                if (!report_memarg_type(ii, 2, SYSARG_READ, (byte *)&name_info->FileName,
                                        name_length, "FILE_NAME_INFORMATION.FileName",
                                        DRSYS_TYPE_CWARRAY,
                                        "FILE_NAME_INFORMATION.FileName"))
                    return;
            }
            break;
        }
        default:
            /* assuming no padding in the struct */
            if (!report_memarg_type(ii, 2, SYSARG_READ, info, length,
                                    "input FileInformation", DRSYS_TYPE_STRUCT, NULL))
                return;
            break;
        }
    }
}

static void
handle_PowerInformation(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Normally the buffer is all defined, but some info classes only write some fields */
    POWER_INFORMATION_LEVEL level = (POWER_INFORMATION_LEVEL)pt->sysarg[0];
    if (level == PowerRequestCreate) {
        /* i#1247: fields depend on flags */
        POWER_REQUEST_CREATE *real_req = (POWER_REQUEST_CREATE *)pt->sysarg[1];
        size_t sz = (size_t)pt->sysarg[2];
        POWER_REQUEST_CREATE safe_req;
        if (ii->arg->pre) {
            /* Version and Flags must be defined */
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)real_req,
                                    offsetof(POWER_REQUEST_CREATE, ReasonString),
                                    "POWER_REQUEST_CREATE Version+Flags",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (safe_read((byte *)real_req, sizeof(safe_req), &safe_req)) {
                if (safe_req.Flags == POWER_REQUEST_CONTEXT_SIMPLE_STRING ||
                    safe_req.Flags == POWER_REQUEST_CONTEXT_DETAILED_STRING) {
                    /* XXX: the array of strings and the resource ID seem to
                     * not be passed to the kernel for DETAILED_STRING!
                     * Only the name of the module.
                     */
                    sysinfo_arg_t arg_info = { 1, sizeof(UNICODE_STRING), SYSARG_READ,
                                               0 };
                    handle_unicode_string_access(
                        ii, &arg_info, (byte *)&real_req->ReasonString,
                        sizeof(real_req->ReasonString), false /*honor len*/);
                    if (ii->abort)
                        return;
                } else {
                    /* An unknown flag: we observe 0x80000000 in i#1247.
                     * That flag has no further initialized fields.
                     * We live with false negatives for other unknown flags.
                     */
#define POWER_REQUEST_CONTEXT_UNKNOWN_NOINPUT 0x80000000
                    if (safe_req.Flags != POWER_REQUEST_CONTEXT_UNKNOWN_NOINPUT) {
                        WARN("WARNING: unknown POWER_REQUEST_CREATE.Flags value 0x%x\n",
                             safe_req.Flags);
                    }
                }
            }
        }
    } else if (level == PowerRequestAction) {
        if (ii->arg->pre) {
            /* POWER_REQUEST_ACTION struct.  If it ends up used anywhere
             * else we should move this to a type handler.
             */
            POWER_REQUEST_ACTION *act = (POWER_REQUEST_ACTION *)pt->sysarg[1];
            if (!report_memarg_type(
                    ii, 1, SYSARG_READ, (byte *)act,
                    offsetof(POWER_REQUEST_ACTION, Unknown1) + sizeof(act->Unknown1),
                    "POWER_REQUEST_ACTION 1st 3 fields", DRSYS_TYPE_STRUCT, NULL))
                return;
            if (!report_memarg_type(
                    ii, 1, SYSARG_READ, (byte *)&act->Unknown2, sizeof(act->Unknown2),
                    "POWER_REQUEST_ACTION 4th field", DRSYS_TYPE_POINTER, NULL))
                return;
        }

    } else {
        /* XXX: check the rest of the codes and see whether any are not
         * fully initialized or have weird output buffers.
         * Some are documented under CallNtPowerInformation.
         */
        if (ii->arg->pre) {
            /* In table this would be "{1, -2, R}" */
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)pt->sysarg[1],
                                    pt->sysarg[2], "InputBuffer", DRSYS_TYPE_STRUCT,
                                    NULL))
                return;
        }
    }
}

static void
handle_post_QueryVirtualMemory(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* i#1547: NtQueryVirtualMemory.MemoryWorkingSetList writes the
     * 1st field of MEMORY_WORKING_SET_LIST when it returns
     * STATUS_INFO_LENGTH_MISMATCH if the size is big enough, but does not
     * write to the final bytes-returned param.  Thus we have to special-case
     * that write.  We do not special-case the success value in os_syscall_succeeded():
     * we consider it a failure, as we do not want to do the normal processing.
     */
    ASSERT(!ii->arg->pre, "post only");
    if (dr_syscall_get_result(drcontext) == STATUS_INFO_LENGTH_MISMATCH &&
        pt->sysarg[2] == MemoryWorkingSetList && pt->sysarg[4] >= sizeof(ULONG_PTR)) {
        if (!report_memarg_type(ii, 3, SYSARG_WRITE, (byte *)pt->sysarg[3],
                                sizeof(ULONG_PTR),
                                /* Nebbett and ReactOS call this "NumberOfPages",
                                 * but they also have it as ULONG which is wrong.
                                 * I'm following PSAPI_WORKING_SET_INFORMATION.
                                 */
                                "MEMORY_WORKING_SET_LIST.NumberOfEntries",
                                DRSYS_TYPE_STRUCT, "MEMORY_WORKING_SET_LIST"))
            return;
    }
}

static void
handle_FsControlFile(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ULONG code = (ULONG)pt->sysarg[5];
    switch (code) {
    case FSCTL_PIPE_WAIT:
        /* i#1827: the input struct has a BOOLEAN and thus padding */
        if (ii->arg->pre) {
            FILE_PIPE_WAIT_FOR_BUFFER *data = (FILE_PIPE_WAIT_FOR_BUFFER *)pt->sysarg[6];
            FILE_PIPE_WAIT_FOR_BUFFER local;
            size_t data_sz = (size_t)pt->sysarg[7];
            /* Timeout can be uninitialized if TimeoutSpecified is FALSE. */
            if ((!safe_read((byte *)data, sizeof(local), &local) ||
                 local.TimeoutSpecified) &&
                !report_memarg_type(
                    ii, 1, SYSARG_READ, (byte *)&data->Timeout, sizeof(data->Timeout),
                    "FILE_PIPE_WAIT_FOR_BUFFER.Timeout", DRSYS_TYPE_STRUCT, NULL))
                return;
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)&data->NameLength,
                                    sizeof(data->NameLength),
                                    "FILE_PIPE_WAIT_FOR_BUFFER.NameLength",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)&data->TimeoutSpecified,
                                    sizeof(data->TimeoutSpecified),
                                    "FILE_PIPE_WAIT_FOR_BUFFER.TimeoutSpecified",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)&data->Name,
                                    data_sz - offsetof(FILE_PIPE_WAIT_FOR_BUFFER, Name),
                                    "FILE_PIPE_WAIT_FOR_BUFFER.Name", DRSYS_TYPE_STRUCT,
                                    NULL))
                return;
        }
        break;
    /* XXX: check the rest of the codes and see whether any have padding or
     * optional fields in either the input or output buffers.
     */
    default:
        if (ii->arg->pre) {
            byte *data = (byte *)pt->sysarg[6];
            size_t data_sz = (size_t)pt->sysarg[7];
            if (!report_memarg_type(ii, 1, SYSARG_READ, data, data_sz, "InputBuffer",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    }
}

static void
handle_TraceControl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ULONG code = (ULONG)pt->sysarg[0];
    byte *input = (byte *)pt->sysarg[1];
    size_t sz = (size_t)pt->sysarg[2];
    switch (code) {
    case 0x1e:
        /* XXX i#1865: we do not know the full layout.  We just avoid a false positive
         * on the input buffer by assuming the last 6 bytes are optional/padding.
         */
        if (ii->arg->pre) {
            if (!report_memarg_type(ii, 1, SYSARG_READ, input, sz - 6, "InputBuffer",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    default:
        if (ii->arg->pre) {
            if (!report_memarg_type(ii, 1, SYSARG_READ, input, sz, "InputBuffer",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    }
    /* All other parameters are handled by the table entries */
}

/***************************************************************************
 * IOCTLS
 */

/*
NTSYSAPI NTSTATUS NTAPI
ZwDeviceIoControlFile(
    DR_PARAM_IN HANDLE FileHandle,
    DR_PARAM_IN HANDLE Event OPTIONAL,
    DR_PARAM_IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    DR_PARAM_IN PVOID ApcContext OPTIONAL,
    DR_PARAM_OUT PIO_STATUS_BLOCK IoStatusBlock,
    DR_PARAM_IN ULONG IoControlCode,
    DR_PARAM_IN PVOID InputBuffer OPTIONAL,
    DR_PARAM_IN ULONG InputBufferLength,
    DR_PARAM_OUT PVOID OutputBuffer OPTIONAL,
    DR_PARAM_IN ULONG OutputBufferLength
    );
*/

/* winioctl.h provides:
 * CTL_CODE: Forms code from dev_type, function, access, method.
 * DEVICE_TYPE_FROM_CTL_CODE: Extracts device bits.
 * METHOD_FROM_CTL_CODE: Extracts method bits.
 *
 * Below we provide macros to get the other bits.
 */
#define FUNCTION_FROM_CTL_CODE(code) (((code) >> 2) & 0xfff)
#define ACCESS_FROM_CTL_CODE(code) (((code) >> 14) & 0x3)

/* The AFD (Ancillary Function Driver, afd.sys, for winsock)
 * ioctls don't follow the regular CTL_CODE where the device is << 16.
 * Instead they have the device (FILE_DEVICE_NETWORK == 0x12) << 12,
 * and the function << 2, with access bits always set to 0.
 * NtDeviceIoControlFile only looks at the access and method bits
 * though.
 *
 * XXX this is not foolproof: could be FILE_DEVICE_BEEP with other bits.
 */
#define IS_AFD_IOCTL(code) ((code >> 12) == FILE_DEVICE_NETWORK)
/* Since the AFD "device" overlaps with the function, we have to mask out those
 * overlapping high bits to get the right code.
 */
#define AFD_FUNCTION_FROM_CTL_CODE(code) (FUNCTION_FROM_CTL_CODE(code) & 0x3ff)

#define IOCTL_INBUF_ARGNUM 6
#define IOCTL_OUTBUF_ARGNUM 8

/* XXX: very similar to Linux layouts, though exact constants are different.
 * Still, should be able to share some code.
 */
static void
check_sockaddr(cls_syscall_t *pt, sysarg_iter_info_t *ii, byte *ptr, size_t len,
               bool inbuf, const char *id)
{
    int ordinal = inbuf ? IOCTL_INBUF_ARGNUM : IOCTL_OUTBUF_ARGNUM;
    uint arg_flags = inbuf ? SYSARG_READ : SYSARG_WRITE;
    handle_sockaddr(pt, ii, ptr, len, ordinal, arg_flags, id);
}

/* Macros for shorter, easier-to-read code */
/* N.B.: these return directly, so do not use in functions that need cleanup! */
#define CHECK_DEF(ii, ptr, sz, id)                                                      \
    do {                                                                                \
        if (!report_memarg_type(ii, IOCTL_INBUF_ARGNUM, SYSARG_READ, (byte *)(ptr), sz, \
                                id, DRSYS_TYPE_STRUCT, NULL))                           \
            return;                                                                     \
    } while (0)
#define CHECK_ADDR(ii, ptr, sz, id)                                                   \
    do {                                                                              \
        if (!report_memarg_type(ii, IOCTL_OUTBUF_ARGNUM, SYSARG_WRITE, (byte *)(ptr), \
                                sz, id, DRSYS_TYPE_STRUCT, NULL))                     \
            return;                                                                   \
    } while (0)
#define MARK_WRITE(ii, ptr, sz, id)                                                 \
    do {                                                                            \
        if (!report_memarg_type(ii, IOCTL_OUTBUF_ARGNUM, SYSARG_WRITE, ptr, sz, id, \
                                DRSYS_TYPE_STRUCT, NULL))                           \
            return;                                                                 \
    } while (0)
#define CHECK_OUT_PARAM(ii, ptr, sz, id)                                            \
    do {                                                                            \
        if (!report_memarg_type(ii, IOCTL_OUTBUF_ARGNUM, SYSARG_WRITE, ptr, sz, id, \
                                DRSYS_TYPE_STRUCT, NULL))                           \
            return;                                                                 \
    } while (0)

static void
handle_AFD_ioctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint full_code = (uint)pt->sysarg[5];
    byte *inbuf = (byte *)pt->sysarg[IOCTL_INBUF_ARGNUM];
    uint insz = (uint)pt->sysarg[7];
    /* XXX: put max of insz on all the sizes below */

    /* Extract operation. */
    uint opcode = AFD_FUNCTION_FROM_CTL_CODE(full_code);

    /* We have "8,-9,W" in the table so we only need to handle additional pointers
     * here or cases where subsets of the full output buffer are written.
     *
     * XXX i#410: We treat asynch i/o as happening now rather than trying to
     * watch NtWait* and tracking event objects, though we'll
     * over-estimate the amount written in some cases.
     */

    bool pre_post_ioctl = true;
    /* First check if the given opcode is one of those needing both pre- and
     * post- handling in the first switch. We'll set the pre_post_ioctl to
     * "false" in the default block to continue to the second switch.
     */
    switch (opcode) {
    case AFD_RECV: { /* 5 == 0x12017 */
        /* InputBuffer == AFD_RECV_INFO */
        AFD_RECV_INFO info;
        uint i;
        if (ii->arg->pre)
            CHECK_DEF(ii, inbuf, insz, "AFD_RECV_INFO");

        if (inbuf == NULL || !safe_read(inbuf, sizeof(info), &info)) {
            WARN("WARNING: AFD_RECV: can't read param\n");
            break;
        }

        if (ii->arg->pre) {
            CHECK_DEF(ii, (byte *)info.BufferArray,
                      info.BufferCount * sizeof(*info.BufferArray),
                      "AFD_RECV_INFO.BufferArray");
        }

        for (i = 0; i < info.BufferCount; i++) {
            AFD_WSABUF buf;
            if (safe_read((char *)&info.BufferArray[i], sizeof(buf), &buf)) {
                if (ii->arg->pre)
                    CHECK_ADDR(ii, buf.buf, buf.len, "AFD_RECV_INFO.BufferArray[i].buf");
                else {
                    LOG(drcontext, SYSCALL_VERBOSE,
                        "\tAFD_RECV_INFO buf %d: " PFX "-" PFX "\n", i, buf.buf, buf.len);
                    MARK_WRITE(ii, buf.buf, buf.len, "AFD_RECV_INFO.BufferArray[i].buf");
                }
            } else
                WARN("WARNING: AFD_RECV: can't read param\n");
        }
        break;
    }
    case AFD_RECV_DATAGRAM: { /* 6 ==  0x1201b */
        /* InputBuffer == AFD_RECV_INFO_UDP */
        AFD_RECV_INFO_UDP info;
        uint i;
        if (ii->arg->pre)
            CHECK_DEF(ii, inbuf, insz, "AFD_RECV_INFO_UDP");

        if (inbuf == NULL || !safe_read(inbuf, sizeof(info), &info)) {
            WARN("WARNING: AFD_RECV_DATAGRAM: can't read param\n");
            break;
        }

        if (safe_read(info.AddressLength, sizeof(i), &i)) {
            if (ii->arg->pre)
                CHECK_ADDR(ii, (byte *)info.Address, i, "AFD_RECV_INFO_UDP.Address");
            else {
                /* XXX i#410: This API is asynch and info.Address is an
                 * outparam, so its possible that none of this data is written
                 * yet.  We conservatively assume the whole thing is written,
                 * rather than using check_sockaddr(), which will try to look at
                 * the unwritten sa_family field.
                 */
                MARK_WRITE(ii, (byte *)info.Address, i, "AFD_RECV_INFO_UDP.Address");
            }
        } else
            WARN("WARNING: AFD_RECV_DATAGRAM: can't read AddressLength\n");

        if (ii->arg->pre) {
            CHECK_DEF(ii, info.BufferArray, info.BufferCount * sizeof(*info.BufferArray),
                      "AFD_RECV_INFO_UDP.BufferArray");
        }
        for (i = 0; i < info.BufferCount; i++) {
            AFD_WSABUF buf;
            if (safe_read((char *)&info.BufferArray[i], sizeof(buf), &buf)) {
                if (ii->arg->pre) {
                    CHECK_ADDR(ii, buf.buf, buf.len,
                               "AFD_RECV_INFO_UDP.BufferArray[i].buf");
                } else {
                    LOG(drcontext, SYSCALL_VERBOSE,
                        "\tAFD_RECV_INFO_UDP buf %d: " PFX "-" PFX "\n", i, buf.buf,
                        buf.len);
                    MARK_WRITE(ii, buf.buf, buf.len,
                               "AFD_RECV_INFO_UDP.BufferArray[i].buf");
                }
            } else
                WARN("WARNING: AFD_RECV_DATAGRAM: can't read BufferArray\n");
        }
        break;
    }
    case AFD_SELECT: { /* 9 == 0x12024 */
        AFD_POLL_INFO info;
        uint i;
        AFD_POLL_INFO *ptr = (AFD_POLL_INFO *)inbuf;
        if (ii->arg->pre) {
            /* Have to separate the Boolean since padding after it */
            CHECK_DEF(ii, ptr, sizeof(ptr->Timeout), "AFD_POLL_INFO.Timeout");
            CHECK_DEF(ii, &ptr->HandleCount, sizeof(ptr->HandleCount),
                      "AFD_POLL_INFO.HandleCount");
            CHECK_DEF(ii, &ptr->Exclusive, sizeof(ptr->Exclusive),
                      "AFD_POLL_INFO.Exclusive");
        }

        if (inbuf == NULL || !safe_read(inbuf, sizeof(info), &info) ||
            insz !=
                offsetof(AFD_POLL_INFO, Handles) +
                    info.HandleCount * sizeof(AFD_HANDLE)) {
            WARN("WARNING: unreadable or invalid AFD_POLL_INFO\n");
            break;
        }

        for (i = 0; i < info.HandleCount; i++) {
            /* I'm assuming Status is an output field */
            if (ii->arg->pre) {
                CHECK_DEF(ii, &ptr->Handles[i], offsetof(AFD_HANDLE, Status),
                          "AFD_POLL_INFO.Handles[i]");
            } else {
                MARK_WRITE(ii, (byte *)&ptr->Handles[i].Status,
                           sizeof(ptr->Handles[i].Status),
                           "AFD_POLL_INFO.Handles[i].Status");
            }
        }
        break;
    }
    case AFD_GET_TDI_HANDLES: { /* 13 == 0x12037 */
        if (ii->arg->pre) {
            /* I believe input is a uint of AFD_*_HANDLE flags */
            CHECK_DEF(ii, inbuf, insz, "AFD_GET_TDI_HANDLES flags");
            /* as usual the write param will be auto-checked for addressabilty */
        } else {
            uint outsz = (uint)pt->sysarg[9];
            AFD_TDI_HANDLE_DATA *info = (AFD_TDI_HANDLE_DATA *)pt->sysarg[8];
            uint flags;
            if (safe_read(inbuf, sizeof(flags), &flags) && outsz == sizeof(*info)) {
                if (TEST(AFD_ADDRESS_HANDLE, flags)) {
                    MARK_WRITE(ii, (byte *)&info->TdiAddressHandle,
                               sizeof(info->TdiAddressHandle),
                               "AFD_TDI_HANDLE_DATA.TdiAddressHandle");
                }
                if (TEST(AFD_CONNECTION_HANDLE, flags)) {
                    MARK_WRITE(ii, (byte *)&info->TdiConnectionHandle,
                               sizeof(info->TdiConnectionHandle),
                               "AFD_TDI_HANDLE_DATA.TdiConnectionHandle");
                }
            } else
                WARN("WARNING: unreadable AFD_GET_TDI_HANDLES flags or invalid outsz\n");
        }
        break;
    }
    case AFD_GET_INFO: { /* 30 == 0x1207b */
        if (ii->arg->pre) {
            /* InputBuffer == AFD_INFO.  Only InformationClass need be defined. */
            CHECK_DEF(ii, inbuf, sizeof(((AFD_INFO *)0)->InformationClass),
                      "AFD_INFO.InformationClass");
        } else {
            /* XXX i#378: post-syscall we should only define the particular info
             * fields written.  e.g., only AFD_INFO_GROUP_ID_TYPE uses the
             * LargeInteger field and the rest will leave the extra dword there
             * undefined.  Punting on that for now.
             */
        }

        break;
    }
    default: {
        pre_post_ioctl = false;
    }
    }

    if (pre_post_ioctl || !ii->arg->pre) {
        return;
    }

    /* All the ioctls below need only pre- handling */
    switch (opcode) {
    case AFD_SET_INFO: { /* 14 == 0x1203b */
        /* InputBuffer == AFD_INFO.  If not LARGE_INTEGER, 2nd word can be undef.
         * Padding also need not be defined.
         */
        AFD_INFO info;
        CHECK_DEF(ii, inbuf, sizeof(info.InformationClass), "AFD_INFO.InformationClass");
        if (safe_read(inbuf, sizeof(info), &info)) {
            switch (info.InformationClass) {
            case AFD_INFO_BLOCKING_MODE:
                /* uses BOOLEAN in union */
                CHECK_DEF(ii, inbuf + offsetof(AFD_INFO, Information.Boolean),
                          sizeof(info.Information.Boolean), "AFD_INFO.Information");
                break;
            default:
                /* the other codes are only valid with AFD_GET_INFO */
                WARN("WARNING: AFD_SET_INFO: unknown info opcode\n");
                break;
            }
        } else
            WARN("WARNING: AFD_SET_INFO: cannot read info opcode\n");
        break;
    }
    case AFD_SET_CONTEXT: { /* 17 == 0x12047 */
        /* InputBuffer == SOCKET_CONTEXT.  SOCKET_CONTEXT.Padding need not be defined,
         * and the helper data is var-len.
         *
         * Depending on the Windows version, the SOCKET_CONTEXT struct layout
         * can be different (see i#375). We start with reading the first SOCK_SHARED_INFO
         * field cause it contains the flags needed to identify the layout.
         */
        SOCK_SHARED_INFO sd;
        size_t helper_size, helper_offs;
        byte *l_addr_ptr = NULL, *r_addr_ptr = NULL;

        ASSERT(offsetof(SOCKET_CONTEXT, SharedData) == 0,
               "SOCKET_CONTEXT layout changed?");
        ASSERT(offsetof(SOCKET_CONTEXT_NOGUID, SharedData) == 0,
               "SOCKET_CONTEXT_NOGUID layout changed?");

        CHECK_DEF(ii, inbuf, sizeof(sd), "SOCKET_CONTEXT SharedData");
        if (!safe_read(inbuf, sizeof(sd), &sd)) {
            WARN("WARNING: AFD_SET_CONTEXT: can't read param\n");
            break;
        }

        /* Now that we know the exact layout we can re-read the SOCKET_CONTEXT */
        if (sd.HasGUID) {
            SOCKET_CONTEXT sc;
            CHECK_DEF(ii, inbuf, offsetof(SOCKET_CONTEXT, Padding),
                      "SOCKET_CONTEXT pre-Padding");
            if (!safe_read(inbuf, sizeof(sc), &sc)) {
                WARN("WARNING: AFD_SET_CONTEXT: can't read param\n");
                break;
            }

            /* I'm treating these SOCKADDRS as var-len */
            l_addr_ptr = inbuf + sizeof(SOCKET_CONTEXT);
            r_addr_ptr = inbuf + sizeof(SOCKET_CONTEXT) + sd.SizeOfLocalAddress;
            helper_size = sc.SizeOfHelperData;
            helper_offs =
                sizeof(SOCKET_CONTEXT) + sd.SizeOfLocalAddress + sd.SizeOfRemoteAddress;
        } else {
            SOCKET_CONTEXT_NOGUID sc;
            CHECK_DEF(ii, inbuf, offsetof(SOCKET_CONTEXT_NOGUID, Padding),
                      "SOCKET_CONTEXT pre-Padding");
            if (!safe_read(inbuf, sizeof(sc), &sc)) {
                WARN("WARNING: AFD_SET_CONTEXT: can't read param\n");
                break;
            }

            /* I'm treating these SOCKADDRS as var-len */
            l_addr_ptr = inbuf + sizeof(SOCKET_CONTEXT_NOGUID);
            r_addr_ptr = inbuf + sizeof(SOCKET_CONTEXT_NOGUID) + sd.SizeOfLocalAddress;
            helper_size = sc.SizeOfHelperData;
            helper_offs = sizeof(SOCKET_CONTEXT_NOGUID) + sd.SizeOfLocalAddress +
                sd.SizeOfRemoteAddress;
        }

        if (helper_offs + helper_size != insz) {
            WARN("WARNING AFD_SET_CONTEXT param fields messed up\n");
            break;
        }

        check_sockaddr(pt, ii, l_addr_ptr, sd.SizeOfLocalAddress, true /*in*/,
                       "SOCKET_CONTEXT.LocalAddress");
        /* I'm treating these SOCKADDRS as var-len */
        check_sockaddr(pt, ii, r_addr_ptr, sd.SizeOfRemoteAddress, true /*in*/,
                       "SOCKET_CONTEXT.RemoteAddress");

        /* XXX i#424: helper data could be a struct w/ padding. I have seen pieces of
         * it be uninit on XP. Just ignore the definedness check if helper data
         * is not trivial
         */
        if (helper_size <= 4)
            CHECK_DEF(ii, inbuf + helper_offs, helper_size, "SOCKET_CONTEXT.HelperData");
        break;
    }
    case AFD_BIND: { /* 0 == 0x12003 */
        /* InputBuffer == AFD_BIND_DATA.  Address.Address is var-len and mswsock.dll
         * seems to pass an over-estimate of the real size.
         */
        CHECK_DEF(ii, inbuf, offsetof(AFD_BIND_DATA, Address),
                  "AFD_BIND_DATA pre-Address");
        check_sockaddr(pt, ii, inbuf + offsetof(AFD_BIND_DATA, Address),
                       insz - offsetof(AFD_BIND_DATA, Address), true /*in*/,
                       "AFD_BIND_DATA.Address");
        break;
    }
    case AFD_CONNECT: { /* 1 == 0x12007 */
        /* InputBuffer == AFD_CONNECT_INFO.  RemoteAddress.Address is var-len. */
        AFD_CONNECT_INFO *info = (AFD_CONNECT_INFO *)inbuf;
        /* Have to separate the Boolean since padding after it */
        CHECK_DEF(ii, inbuf, sizeof(info->UseSAN), "AFD_CONNECT_INFO.UseSAN");
        CHECK_DEF(ii, &info->Root, (byte *)&info->RemoteAddress - (byte *)&info->Root,
                  "AFD_CONNECT_INFO pre-RemoteAddress");
        check_sockaddr(pt, ii, (byte *)&info->RemoteAddress,
                       insz - offsetof(AFD_CONNECT_INFO, RemoteAddress), true /*in*/,
                       "AFD_CONNECT_INFO.RemoteAddress");
        break;
    }
    case AFD_DISCONNECT: { /* 10 == 0x1202b */
        /* InputBuffer == AFD_DISCONNECT_INFO.  Padding between fields need not be def. */
        AFD_DISCONNECT_INFO *info = (AFD_DISCONNECT_INFO *)inbuf;
        CHECK_DEF(ii, inbuf, sizeof(info->DisconnectType),
                  "AFD_DISCONNECT_INFO.DisconnectType");
        CHECK_DEF(ii, inbuf + offsetof(AFD_DISCONNECT_INFO, Timeout),
                  sizeof(info->Timeout), "AFD_DISCONNECT_INFO.Timeout");
        break;
    }
    case AFD_DEFER_ACCEPT: { /* 35 == 0x120bf */
        /* InputBuffer == AFD_DEFER_ACCEPT_DATA */
        AFD_DEFER_ACCEPT_DATA *info = (AFD_DEFER_ACCEPT_DATA *)inbuf;
        CHECK_DEF(ii, inbuf, sizeof(info->SequenceNumber),
                  "AFD_DEFER_ACCEPT_DATA.SequenceNumber");
        CHECK_DEF(ii, inbuf + offsetof(AFD_DEFER_ACCEPT_DATA, RejectConnection),
                  sizeof(info->RejectConnection),
                  "AFD_DEFER_ACCEPT_DATA.RejectConnection");
        break;
    }
    case AFD_SEND: { /* 7 == 0x1201f */
        /* InputBuffer == AFD_SEND_INFO */
        AFD_SEND_INFO info;
        CHECK_DEF(ii, inbuf, insz, "AFD_SEND_INFO"); /* no padding */
        if (safe_read(inbuf, sizeof(info), &info)) {
            uint i;
            CHECK_DEF(ii, info.BufferArray, info.BufferCount * sizeof(*info.BufferArray),
                      "AFD_SEND_INFO.BufferArray");
            for (i = 0; i < info.BufferCount; i++) {
                AFD_WSABUF buf;
                if (safe_read((char *)&info.BufferArray[i], sizeof(buf), &buf))
                    CHECK_DEF(ii, buf.buf, buf.len, "AFD_SEND_INFO.BufferArray[i].buf");
                else
                    WARN("WARNING: AFD_SEND: can't read param\n");
            }
        } else
            WARN("WARNING: AFD_SEND: can't read param\n");
        break;
    }
    case AFD_SEND_DATAGRAM: { /* 8 == 0x12023 */
        /* InputBuffer == AFD_SEND_INFO_UDP */
        AFD_SEND_INFO_UDP info;
        ULONG size_of_remote_address;
        void *remote_address;
        ASSERT(sizeof(size_of_remote_address) == sizeof(info.SizeOfRemoteAddress) &&
                   sizeof(remote_address) == sizeof(info.RemoteAddress),
               "sizes don't match");
        /* Looks like AFD_SEND_INFO_UDP has 36 bytes of uninit gap in the middle: i#418 */
        CHECK_DEF(ii, inbuf, offsetof(AFD_SEND_INFO_UDP, UnknownGap),
                  "AFD_SEND_INFO_UDP before gap");
        if (safe_read(inbuf, offsetof(AFD_SEND_INFO_UDP, UnknownGap), &info)) {
            uint i;
            CHECK_DEF(ii, info.BufferArray, info.BufferCount * sizeof(*info.BufferArray),
                      "AFD_SEND_INFO_UDP.BufferArray");
            for (i = 0; i < info.BufferCount; i++) {
                AFD_WSABUF buf;
                if (safe_read((char *)&info.BufferArray[i], sizeof(buf), &buf)) {
                    CHECK_DEF(ii, buf.buf, buf.len,
                              "AFD_SEND_INFO_UDP.BufferArray[i].buf");
                } else
                    WARN("WARNING: AFD_SEND_DATAGRAM: can't read param\n");
            }
        } else
            WARN("WARNING: AFD_SEND_DATAGRAM: can't read param\n");
        CHECK_DEF(ii, inbuf + offsetof(AFD_SEND_INFO_UDP, SizeOfRemoteAddress),
                  sizeof(info.SizeOfRemoteAddress),
                  "AFD_SEND_INFO_UDP.SizeOfRemoteAddress");
        CHECK_DEF(ii, inbuf + offsetof(AFD_SEND_INFO_UDP, RemoteAddress),
                  sizeof(info.RemoteAddress), "AFD_SEND_INFO_UDP.RemoteAddress");
        if (safe_read(inbuf + offsetof(AFD_SEND_INFO_UDP, SizeOfRemoteAddress),
                      sizeof(size_of_remote_address), &size_of_remote_address) &&
            safe_read(inbuf + offsetof(AFD_SEND_INFO_UDP, RemoteAddress),
                      sizeof(remote_address), &remote_address)) {
            CHECK_DEF(ii, remote_address, size_of_remote_address,
                      "AFD_SEND_INFO_UDP.RemoteAddress buffer");
        }

        break;
    }
    case AFD_EVENT_SELECT: { /* 33 == 0x12087 */
        CHECK_DEF(ii, inbuf, insz, "AFD_EVENT_SELECT_INFO");
        break;
    }
    case AFD_ENUM_NETWORK_EVENTS: {                                 /* 34 == 0x1208b */
        CHECK_DEF(ii, inbuf, insz, "AFD_ENUM_NETWORK_EVENTS_INFO"); /*  */
        break;
    }
    case AFD_START_LISTEN: { /* 2 == 0x1200b */
        AFD_LISTEN_DATA *info = (AFD_LISTEN_DATA *)inbuf;
        if (insz != sizeof(AFD_LISTEN_DATA))
            WARN("WARNING: invalid size for AFD_LISTEN_DATA\n");
        /* Have to separate the Booleans since padding after */
        CHECK_DEF(ii, inbuf, sizeof(info->UseSAN), "AFD_LISTEN_DATA.UseSAN");
        CHECK_DEF(ii, &info->Backlog, sizeof(info->Backlog), "AFD_LISTEN_DATA.Backlog");
        CHECK_DEF(ii, &info->UseDelayedAcceptance, sizeof(info->UseDelayedAcceptance),
                  "AFD_LISTEN_DATA.UseDelayedAcceptance");
        break;
    }
    case AFD_ACCEPT: { /* 4 == 0x12010 */
        AFD_ACCEPT_DATA *info = (AFD_ACCEPT_DATA *)inbuf;
        if (insz != sizeof(AFD_ACCEPT_DATA))
            WARN("WARNING: invalid size for AFD_ACCEPT_DATA\n");
        /* Have to separate the Booleans since padding after */
        CHECK_DEF(ii, inbuf, sizeof(info->UseSAN), "AFD_LISTEN_DATA.UseSAN");
        CHECK_DEF(ii, &info->SequenceNumber, sizeof(info->SequenceNumber),
                  "AFD_ACCEPT_DATA.SequenceNumber");
        CHECK_DEF(ii, &info->ListenHandle, sizeof(info->ListenHandle),
                  "AFD_ACCEPT_DATA.ListenHandle");
        break;
    }
    default: {
        /* XXX i#377: add more ioctl codes.
         * I've seen 0x120bf == operation # 47 called by
         * WS2_32.dll!setsockopt.  no uninits.  not sure what it is.
         */
        WARN("WARNING: unknown AFD ioctl " PIFX " => op %d\n", full_code, opcode);
        /* XXX: should perhaps dump a callstack too at higher verbosity */
        /* assume full thing must be defined */
        CHECK_DEF(ii, inbuf, insz, "AFD InputBuffer");
        break;
    }
    }

    ASSERT(ii->arg->pre,
           "Sanity check - we should only process pre- ioctls at this point");
}

/* Handles ioctls of type FILE_DEVICE_NETWORK.  Some codes are documented in
 * wininc/tcpioctl.h.
 */
static void
handle_NET_ioctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint full_code = (uint)pt->sysarg[5];
    byte *inbuf = (byte *)pt->sysarg[IOCTL_INBUF_ARGNUM];
    uint insz = (uint)pt->sysarg[7];
    byte *outbuf = (byte *)pt->sysarg[IOCTL_OUTBUF_ARGNUM];
    uint outsz = (uint)pt->sysarg[9];
    bool handled;

    /* Extract operation. */
    uint function = FUNCTION_FROM_CTL_CODE(full_code);

    ASSERT((uint)FILE_DEVICE_NETWORK == DEVICE_TYPE_FROM_CTL_CODE(full_code),
           "Unknown device type for handle_NET_ioctl!");

    /* Set handled to false in the default path. */
    handled = true;
    switch (full_code) {

    case _TCP_CTL_CODE(0x003, METHOD_NEITHER, FILE_ANY_ACCESS): /* 0x12000f */ {
        /* New in Vista+: called from NSI.dll through
         * IPHPAPI.dll!GetAdaptersInfo.  Found these similar ioctl values, but
         * none of these match the observed behavior:
         * - IOCTL_IPV6_QUERY_NEIGHBOR_CACHE from nddip6.h in WinCE DDK
         * - IOCTL_IP_NAT_DELETE_INTERFACE from ipnat.h in WinCE DDK
         * These checks are based on reverse engineering the interface.
         */
        net_ioctl_003_inout_t data;
        ip_adapter_info_t *adapter_info;
        LOG(drcontext, SYSCALL_VERBOSE, "IOCTL_NET_0x003\n");
        if (inbuf == NULL || inbuf != outbuf || insz != sizeof(data) || insz != outsz) {
            WARN("WARNING: expected same in/out param of size %d for ioctl " PFX "\n",
                 sizeof(data), full_code);
            break;
        }
        if (!safe_read(inbuf, sizeof(data), &data)) {
            WARN("WARNING: unable to read param for ioctl " PFX "\n", full_code);
            break;
        }
        adapter_info = data.adapter_info;
        if (ii->arg->pre && data.buf1) {
            CHECK_DEF(ii, data.buf1, data.buf1_sz, "net ioctl 0x003 buf1");
        }
        CHECK_OUT_PARAM(ii, data.buf2, data.buf2_sz, "net ioctl 0x003 buf2");

        /* Check whole buffer for addressability, but the kernel only writes
         * part of the output, so mark each struct member individually.
         */
        if (ii->arg->pre) {
            if (data.adapter_info_sz != sizeof(ip_adapter_info_t)) {
                WARN("WARNING: adapter info struct size does not match "
                     "expectation: found %d expected %d\n",
                     data.adapter_info_sz, sizeof(ip_adapter_info_t));
            }
            CHECK_ADDR(ii, adapter_info, data.adapter_info_sz,
                       "net ioctl 0x003 adapter_info");
        } else if (adapter_info != NULL) {
            /* XXX: Can we refactor these struct field checks here and above so
             * we only have to write the pointer, type, and field once?
             */
            MARK_WRITE(ii, (byte *)&adapter_info->adapter_name_len,
                       sizeof(adapter_info->adapter_name_len),
                       "net ioctl 0x003 adapter_info->adapter_name_len");
            MARK_WRITE(ii, (byte *)&adapter_info->adapter_name,
                       sizeof(adapter_info->adapter_name),
                       "net ioctl 0x003 adapter_info->adapter_name");
            MARK_WRITE(ii, (byte *)&adapter_info->unknown_a,
                       sizeof(adapter_info->unknown_a),
                       "net ioctl 0x003 adapter_info->unknown_a");
            MARK_WRITE(ii, (byte *)&adapter_info->unknown_b,
                       sizeof(adapter_info->unknown_b),
                       "net ioctl 0x003 adapter_info->unknown_b");
            MARK_WRITE(ii, (byte *)&adapter_info->unknown_c,
                       sizeof(adapter_info->unknown_c),
                       "net ioctl 0x003 adapter_info->unknown_c");
            MARK_WRITE(ii, (byte *)&adapter_info->unknown_d,
                       sizeof(adapter_info->unknown_d),
                       "net ioctl 0x003 adapter_info->unknown_d");
        }
        break;
    }

    case _TCP_CTL_CODE(0x006, METHOD_NEITHER, FILE_ANY_ACCESS): /* 0x12001b */ {
        /* New in Vista+: called from NSI.dll through
         * IPHPAPI.dll!GetAdaptersInfo.
         */
        net_ioctl_006_inout_t data;
        uint buf1sz, buf2sz, buf3sz, buf4sz;
        LOG(drcontext, SYSCALL_VERBOSE, "IOCTL_NET_0x006\n");
        if (inbuf == NULL || inbuf != outbuf || sizeof(data) != insz || insz != outsz) {
            WARN("WARNING: expected same in/out param of size %d for ioctl " PFX "\n",
                 sizeof(data), full_code);
            break;
        }
        if (!safe_read(inbuf, sizeof(data), &data)) {
            WARN("WARNING: unable to read param for ioctl " PFX "\n", full_code);
            break;
        }
        buf1sz = data.buf1_elt_sz * data.num_elts;
        buf2sz = data.buf2_elt_sz * data.num_elts;
        buf3sz = data.buf3_elt_sz * data.num_elts;
        buf4sz = data.buf4_elt_sz * data.num_elts;
        CHECK_OUT_PARAM(ii, data.buf1, buf1sz, "net ioctl 0x006 buf1");
        CHECK_OUT_PARAM(ii, data.buf2, buf2sz, "net ioctl 0x006 buf2");
        CHECK_OUT_PARAM(ii, data.buf3, buf3sz, "net ioctl 0x006 buf3");
        CHECK_OUT_PARAM(ii, data.buf4, buf4sz, "net ioctl 0x006 buf4");
        break;
    }

    /* These are known ioctl values used prior to Vista.  They seem to read and
     * write flat structures and we believe they are handled well by our default
     * behavior.
     */
    case IOCTL_TCP_QUERY_INFORMATION_EX:
    case IOCTL_TCP_SET_INFORMATION_EX:
        if (ii->arg->pre) {
            CHECK_DEF(ii, inbuf, insz, "NET InputBuffer");
        }
        break;

    default: handled = false; break;
    }

    if (!handled) {
        /* Unknown ioctl.  Check inbuf for full def and let table mark outbuf as
         * written.
         */
        if (ii->arg->pre) {
            WARN("WARNING: unhandled NET ioctl " PIFX " => op %d\n", full_code, function);
            CHECK_DEF(ii, inbuf, insz, "NET InputBuffer");
        }
    }
}

static void
handle_DeviceIoControlFile_helper(void *drcontext, cls_syscall_t *pt,
                                  sysarg_iter_info_t *ii)
{
    uint code = (uint)pt->sysarg[5];
    uint device = (uint)DEVICE_TYPE_FROM_CTL_CODE(code);
    byte *inbuf = (byte *)pt->sysarg[IOCTL_INBUF_ARGNUM];
    uint insz = (uint)pt->sysarg[7];

    /* We don't put "6,-7,R" into the table b/c for some ioctls only part of
     * the input buffer needs to be defined.
     */

    /* Common ioctl handling before calling more specific handler. */
    if (ii->arg->pre) {
        if (inbuf == NULL)
            return;
    } else {
        /* We have "8,-9,W" in the table so we only need to handle additional pointers
         * here or cases where subsets of the full output buffer are written.
         *
         * XXX i#410: We treat asynch i/o as happening now rather than trying to
         * watch NtWait* and tracking event objects, though we'll
         * over-estimate the amount written in some cases.
         */
        if (!os_syscall_succeeded(ii->arg->sysnum, NULL, pt))
            return;
    }

    /* XXX: could use SYSINFO_SECONDARY_TABLE instead */
    if (IS_AFD_IOCTL(code)) {
        /* This is redundant for those where entire buffer must be defined but
         * most need subset defined.
         */
        if (ii->arg->pre)
            CHECK_ADDR(ii, inbuf, insz, "InputBuffer");
        handle_AFD_ioctl(drcontext, pt, ii);
    } else if (device == FILE_DEVICE_NETWORK) {
        handle_NET_ioctl(drcontext, pt, ii);
    } else if (device == FILE_DEVICE_CONSOLE) {
        /* XXX i#1156: there's some data structure with padding in
         * at least one of the common transactions here.
         * For now, disabling the inputbuffer check.
         */
    } else {
        /* XXX i#377: add more ioctl codes. */
        WARN("WARNING: unknown ioctl " PIFX " => op %d\n", code,
             FUNCTION_FROM_CTL_CODE(code));
        /* XXX: should perhaps dump a callstack too at higher verbosity */
        /* assume full thing must be defined */
        if (ii->arg->pre)
            CHECK_DEF(ii, inbuf, insz, "InputBuffer");

        /* Table always marks outbuf as written during post callback.
         * XXX i#378: should break down the output buffer as well since it
         * may not all be written to.
         */
    }

    return;
}

static bool
handle_DeviceIoControlFile(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* we use a helper w/ void return value so we can use CHECK_DEF, etc. macros */
    handle_DeviceIoControlFile_helper(drcontext, pt, ii);
    return true; /* handled */
}
#undef CHECK_DEF
#undef CHECK_ADDR
#undef MARK_WRITE

/***************************************************************************
 * SHADOW TOP-LEVEL ROUTINES
 */

void
os_handle_pre_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_CreateThreadEx))
        handle_pre_CreateThreadEx(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_CreateUserProcess))
        handle_pre_CreateUserProcess(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_DeviceIoControlFile))
        handle_DeviceIoControlFile(drcontext, pt, ii);
    /* We compare only primary number here b/c we use secondary number to
     * find correct NtSetSystemInformation in the secondary table.
     */
    else if (&ii->arg->sysnum.number == &sysnum_SetSystemInformation.number)
        handle_SetSystemInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_SetInformationProcess))
        handle_SetInformationProcess(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_SetInformationFile))
        handle_SetInformationFile(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QueryInformationThread))
        handle_QueryInformationThread(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformation) ||
             drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformationWow64) ||
             drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformationEx))
        handle_QuerySystemInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_PowerInformation))
        handle_PowerInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_FsControlFile))
        handle_FsControlFile(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_TraceControl))
        handle_TraceControl(drcontext, pt, ii);
    else
        wingdi_shadow_process_syscall(drcontext, pt, ii);
}

#ifdef DEBUG
/* info to help analyze syscall false positives.
 * maybe could eventually spin some of this off as an strace tool.
 */
void
syscall_diagnostics(void *drcontext, cls_syscall_t *pt)
{
    /* XXX: even though only at -verbose 2, should use safe_read for all derefs */
    syscall_info_t *sysinfo = pt->sysinfo;
    if (sysinfo == NULL)
        return;
    if (!NT_SUCCESS(dr_syscall_get_result(drcontext)))
        return;
    if (strcmp(sysinfo->name, "NtQueryValueKey") == 0) {
        UNICODE_STRING *us = (UNICODE_STRING *)pt->sysarg[1];
        DR_TRY_EXCEPT(drcontext,
                      {
                          LOG(drcontext, 2, "NtQueryValueKey %S => ",
                              (us == NULL || us->Buffer == NULL) ? L"" : us->Buffer);
                      },
                      {
                          /* EXCEPT */
                      });
        if (pt->sysarg[2] == KeyValuePartialInformation) {
            KEY_VALUE_PARTIAL_INFORMATION *info =
                (KEY_VALUE_PARTIAL_INFORMATION *)pt->sysarg[3];
            if (info->Type == REG_SZ || info->Type == REG_EXPAND_SZ ||
                info->Type == REG_MULTI_SZ /*just showing first*/)
                LOG(drcontext, 2, "%.*S", info->DataLength, (wchar_t *)info->Data);
            else
                LOG(drcontext, 2, PFX, *(ptr_int_t *)info->Data);
        } else if (pt->sysarg[2] == KeyValueFullInformation) {
            KEY_VALUE_FULL_INFORMATION *info =
                (KEY_VALUE_FULL_INFORMATION *)pt->sysarg[3];
            LOG(drcontext, 2, "%.*S = ", info->NameLength, info->Name);
            if (info->Type == REG_SZ || info->Type == REG_EXPAND_SZ ||
                info->Type == REG_MULTI_SZ /*just showing first*/) {
                LOG(drcontext, 2, "%.*S", info->DataLength,
                    (wchar_t *)(((byte *)info) + info->DataOffset));
            } else
                LOG(drcontext, 2, PFX, *(ptr_int_t *)(((byte *)info) + info->DataOffset));
        }
        LOG(drcontext, 2, "\n");
    } else if (strcmp(sysinfo->name, "NtOpenFile") == 0 ||
               strcmp(sysinfo->name, "NtCreateFile") == 0) {
        OBJECT_ATTRIBUTES *obj = (OBJECT_ATTRIBUTES *)pt->sysarg[2];
        DR_TRY_EXCEPT(drcontext,
                      {
                          if (obj != NULL && obj->ObjectName != NULL) {
                              LOG(drcontext, 2, "%s %S\n", sysinfo->name,
                                  obj->ObjectName->Buffer);
                          }
                      },
                      {
                          /* EXCEPT */
                      });
    }
}
#endif

void
os_handle_post_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* XXX code org: there's some processing of syscalls in alloc_drmem.c's
     * client_post_syscall() where common/alloc.c identifies the sysnum: but
     * for things that don't have anything to do w/ mem alloc I think it's
     * cleaner to have it all in here rather than having to edit both files.
     * Perhaps NtContinue and NtSetContextThread should also be here?  OTOH,
     * the teb is an alloc.
     */
    /* each handler checks result for success */
    if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_CreateThread))
        handle_post_CreateThread(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_CreateThreadEx))
        handle_post_CreateThreadEx(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_CreateUserProcess))
        handle_post_CreateUserProcess(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_DeviceIoControlFile))
        handle_DeviceIoControlFile(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QueryInformationThread))
        handle_QueryInformationThread(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_SetSystemInformation))
        handle_SetSystemInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_SetInformationProcess))
        handle_SetInformationProcess(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_SetInformationFile))
        handle_SetInformationFile(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformation) ||
             drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformationWow64) ||
             drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QuerySystemInformationEx))
        handle_QuerySystemInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_PowerInformation))
        handle_PowerInformation(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_QueryVirtualMemory)) {
        /* XXX i#1549: if we do split into secondary, this can be limited to
         * NtQueryVirtualMemory.MemoryWorkingSetList, avoiding the param #2
         * checks in os_syscall_succeeded() and handle_post_QueryVirtualMemory().
         */
        handle_post_QueryVirtualMemory(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_FsControlFile))
        handle_FsControlFile(drcontext, pt, ii);
    else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_TraceControl))
        handle_TraceControl(drcontext, pt, ii);
    else
        wingdi_shadow_process_syscall(drcontext, pt, ii);
    DOLOG(2, { syscall_diagnostics(drcontext, pt); });
}
