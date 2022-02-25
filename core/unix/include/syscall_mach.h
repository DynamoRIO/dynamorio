/* *******************************************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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
 * MacOS Mach system calls
 */

#ifndef _SYSCALL_MACH_H_
#define _SYSCALL_MACH_H_ 1

#define SYSCALL_NUM_MARKER_MACH 0x1000000
#define SYSCALL_NUM_MARKER_BSD 0x2000000
#define SYSCALL_NUM_MARKER_MACHDEP 0x3000000

#define MACH__kernelrpc_mach_vm_allocate_trap 10
#define MACH__kernelrpc_mach_vm_deallocate_trap 12
#define MACH__kernelrpc_mach_vm_protect_trap 14
#define MACH__kernelrpc_mach_vm_map_trap 15
#define MACH__kernelrpc_mach_port_allocate_trap 16
#define MACH__kernelrpc_mach_port_destroy_trap 17
#define MACH__kernelrpc_mach_port_deallocate_trap 18
#define MACH__kernelrpc_mach_port_mod_refs_trap 19
#define MACH__kernelrpc_mach_port_move_member_trap 20
#define MACH__kernelrpc_mach_port_insert_right_trap 21
#define MACH__kernelrpc_mach_port_insert_member_trap 22
#define MACH__kernelrpc_mach_port_extract_member_trap 23
#define MACH__kernelrpc_mach_port_construct_trap 24
#define MACH__kernelrpc_mach_port_destruct_trap 25
#define MACH_mach_reply_port 26
#define MACH_thread_self_trap 27
#define MACH_task_self_trap 28
#define MACH_host_self_trap 29
#define MACH_mach_msg_trap 31
#define MACH_mach_msg_overwrite_trap 32
#define MACH_semaphore_signal_trap 33
#define MACH_semaphore_signal_all_trap 34
#define MACH_semaphore_signal_thread_trap 35
#define MACH_semaphore_wait_trap 36
#define MACH_semaphore_wait_signal_trap 37
#define MACH_semaphore_timedwait_trap 38
#define MACH_semaphore_timedwait_signal_trap 39
#define MACH__kernelrpc_mach_port_guard_trap 41
#define MACH__kernelrpc_mach_port_unguard_trap 42
#define MACH_map_fd 43
#define MACH_task_name_for_pid 44
#define MACH_task_for_pid 45
#define MACH_pid_for_task 46
#define MACH_macx_swapon 48
#define MACH_macx_swapoff 49
#define MACH_macx_triggers 51
#define MACH_macx_backing_store_suspend 52
#define MACH_macx_backing_store_recovery 53
#define MACH_pfz_exit 58
#define MACH_swtch_pri 59
#define MACH_swtch 60
#define MACH_thread_switch 61
#define MACH_clock_sleep_trap 62
#define MACH_mach_timebase_info_trap 89
#define MACH_mach_wait_until_trap 90
#define MACH_mk_timer_create_trap 91
#define MACH_mk_timer_destroy_trap 92
#define MACH_mk_timer_arm_trap 93
#define MACH_mk_timer_cancel_trap 94
#define MACH_iokit_user_client_trap 100

#endif /* _SYSCALL_MACH_H_ */
