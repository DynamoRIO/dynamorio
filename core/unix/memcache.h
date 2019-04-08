/* **********************************************************
 * Copyright (c) 2013-2018 Google, Inc.  All rights reserved.
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

#ifndef _MEMCACHE_H_
#    define _MEMCACHE_ 1

void
memcache_init(void);

void
memcache_exit(void);

bool
memcache_initialized(void);

void
memcache_lock(void);

void
memcache_unlock(void);

/* start and end_in must be PAGE_SIZE aligned */
void
memcache_update(app_pc start, app_pc end_in, uint prot, int type);

/* start and end must be PAGE_SIZE aligned */
void
memcache_update_locked(app_pc start, app_pc end, uint prot, int type, bool exists);

bool
memcache_remove(app_pc start, app_pc end);

bool
memcache_query_memory(const byte *pc, OUT dr_mem_info_t *out_info);

#    if defined(DEBUG) && defined(INTERNAL)
void
memcache_print(file_t outf, const char *prefix);
#    endif

void
memcache_handle_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
                     bool image);

void
memcache_handle_mremap(dcontext_t *dcontext, byte *base, size_t size, byte *old_base,
                       size_t old_size, uint old_prot, uint old_type);

void
memcache_handle_app_brk(byte *lowest_brk /*if known*/, byte *old_brk, byte *new_brk);

void
memcache_update_all_from_os(void);

#endif /* _MEMCACHE_H_ */
