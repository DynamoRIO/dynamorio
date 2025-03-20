/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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

#ifndef _SYSCALL_DRIVER_H_
#define _SYSCALL_DRIVER_H_ 1

void
driver_init(void);

void
driver_exit(void);

void
driver_thread_init(void *drcontext);

void
driver_thread_exit(void *drcontext);

void
driver_handle_callback(void *drcontext);

void
driver_handle_cbret(void *drcontext);

void
driver_pre_syscall(void *drcontext, int sysnum);

bool
driver_freeze_writes(void *drcontext, int sysnum);

bool
driver_process_writes(void *drcontext, int sysnum);

bool
driver_reset_writes(void *drcontext, int sysnum);


#endif /* _SYSCALL_DRIVER_H_ */
