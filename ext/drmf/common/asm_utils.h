/* **********************************************************
 * Copyright (c) 2010-2024 Google, Inc.  All rights reserved.
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

#ifndef _ASM_UTILS_H_
#define _ASM_UTILS_H_ 1

/* Returns the current values of xsp and xbp */
void
get_stack_registers(reg_t *xsp DR_PARAM_OUT, reg_t *xbp DR_PARAM_OUT);

/* Returns the current values of xsp and xbp */
void
get_unwind_registers(reg_t *xsp DR_PARAM_OUT, reg_t *xbp DR_PARAM_OUT,
                     app_pc *xip DR_PARAM_OUT);

#ifdef UNIX
ptr_int_t
raw_syscall(uint sysnum, uint num_args, ...);
#endif

/* Scans the memory in [xsp - count - ARG_SZ, xsp - ARG_SZ) and set it to 0
 * if the content looks like a pointer.
 * The count must be multiple of ARG_SZ.
 */
void
zero_pointers_on_stack(size_t count);

#endif /* _ASM_UTILS_H_ */
