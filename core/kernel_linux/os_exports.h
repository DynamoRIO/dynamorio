/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

#ifndef _OS_EXPORTS_H_
#define _OS_EXPORTS_H_

#include "../os_shared.h"
#include "arch_exports.h"

/* The smallest granularity the OS supports */
#define OS_ALLOC_GRANULARITY (4 * 1024)
#define MAP_FILE_VIEW_ALIGNMENT (4 * 1024)

ushort
os_get_app_tls_base_offset(reg_id_t reg);

ushort
os_get_app_tls_reg_offset(reg_id_t reg);

/* in os.c */
void
os_fork_init(dcontext_t *dcontext);

thread_id_t
get_tls_thread_id(void);
thread_id_t
get_sys_thread_id(void);
bool
is_thread_terminated(dcontext_t *dcontext);
bool
os_wait_thread_terminated(dcontext_t *dcontext);

/* new segment support
 * name is a string
 * wx should contain one of the strings "w", "wx", "x", or ""
 */
/* FIXME: also want control over where in rw region or ro region this
 * section goes -- for cl, order linked seems to do it, but for linux
 * will need a linker script (see linux/os.c for the nspdata problem)
 */
#define DECLARE_DATA_SECTION(name, wx)                \
    asm(".section " name ", \"a" wx "\", @progbits"); \
    asm(".align 0x1000");

#define END_DATA_SECTION_DECLARATIONS() \
    asm(".section .data");              \
    asm(".align 0x1000");               \
    asm(".text");

/* the VAR_IN_SECTION macro change where each var goes */
#define START_DATA_SECTION(name, wx) /* nothing */
#define END_DATA_SECTION()           /* nothing */

/* Any assignment, even to 0, puts vars in current .data and not .bss for cl,
 * but for gcc we need to explicitly declare which section.  We still need
 * the .section asm above to give section attributes and alignment.
 */
#define VAR_IN_SECTION(name) __attribute__((section(name)))

/* pc of the end of the syscall instr itself */
extern app_pc vsyscall_syscall_end_pc;
/* pc where kernel returns control after sysenter vsyscall */
extern app_pc vsyscall_sysenter_return_pc;

#define CONTEXT_HEAP_SIZE(sc) (sizeof(sc))
#define CONTEXT_HEAP_SIZE_OPAQUE (CONTEXT_HEAP_SIZE(struct sigcontext))

/* in pcprofile.c */
void
pcprofile_fragment_deleted(dcontext_t *dcontext, fragment_t *f);
void
pcprofile_thread_exit(dcontext_t *dcontext);

#endif /* _OS_EXPORTS_H_ */
