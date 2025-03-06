/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
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

/* This is meant to be included into a file that only contains a system
 * call table.  Thus it has no mechanism for undefining.
 */

#ifndef _TABLE_DEFINES_H
#define _TABLE_DEFINES_H 1

#define OK (SYSINFO_ALL_PARAMS_KNOWN)
#define UNKNOWN 0
#define W (SYSARG_WRITE)
#define R (SYSARG_READ)
#define CT (SYSARG_COMPLEX_TYPE)
#define HT (SYSARG_HAS_TYPE)
#define WI (SYSARG_WRITE | SYSARG_LENGTH_INOUT)
#define IO (SYSARG_POST_SIZE_IO_STATUS)
#define CSTRING (SYSARG_TYPE_CSTRING)
#define RET (SYSARG_POST_SIZE_RETVAL)
#define RNTST (DRSYS_TYPE_NTSTATUS)
#define RLONG (DRSYS_TYPE_SIGNED_INT)

#ifdef LINUX
/* See the big comment "64-bit vs 32-bit" in drsyscall_linux.c. */
# ifdef X86
#  define PACKNUM(x64,x86,arm, aarch64) ((((uint)x64) << 16) | (x86 & 0xffff))
# elif defined(ARM)
/* XXX i#1569: for AArch64 we'll have to see how the numbers change.
 * We can't pack in the same way b/c the arm-specific syscalls use top bits.
 */
#  define PACKNUM(x64,x86,arm, aarch64) arm
# elif defined(AARCH64)
#  define PACKNUM(x64,x86,arm, aarch64) aarch64
# endif
#endif

#ifdef WINDOWS
# define WINNT    DR_WINDOWS_VERSION_NT
# define WIN2K    DR_WINDOWS_VERSION_2000
# define WINXP    DR_WINDOWS_VERSION_XP
# define WIN2K3   DR_WINDOWS_VERSION_2003
# define WINVISTA DR_WINDOWS_VERSION_VISTA
# define WIN7     DR_WINDOWS_VERSION_7
# define WIN8     DR_WINDOWS_VERSION_8
# define WIN81    DR_WINDOWS_VERSION_8_1
# define WIN10    DR_WINDOWS_VERSION_10
# define WIN11    DR_WINDOWS_VERSION_10_1511
# define WIN12    DR_WINDOWS_VERSION_10_1607
# define WIN13    DR_WINDOWS_VERSION_10_1703
# define WIN14    DR_WINDOWS_VERSION_10_1709
# define WIN15    DR_WINDOWS_VERSION_10_1803
#endif

#endif /* _TABLE_DEFINES_H 1 */
