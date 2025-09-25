/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2016-2024 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _MANGLE_AARCH64_H_
#define _MANGLE_AARCH64_H_

/* Defined in aarch64.asm. */
void
icache_op_ic_ivau_asm(void);
void
icache_op_isb_asm(void);

/* XXX i#7643: The design with this global struct will probably have
 * to be changed. There is at least one inter-thread issue with the
 * current code when an address given by an IC IVAU gets picked up by
 * a different thread on a different core; the second thread could
 * then get swapped out, and the first thread could attempt to run the
 * modified code, before the flush_fragments_from_region has happened.
 */
typedef struct ALIGN_VAR(16) _icache_op_struct_t {
    /* This flag is set if any icache lines have been invalidated. */
    unsigned int flag;
    /* The lower half of the address of "lock" must be non-zero as we want to
     * acquire the lock using only two free registers and STXR Ws, Wt, [Xn]
     * requires s != t and s != n, so we use t == n. With this ordering of the
     * members alignment guarantees that bit 2 of the address of "lock" is set.
     */
    unsigned int lock;
    /* The icache line size. This is discovered using the system register
     * ctr_el0 and will be (1 << (2 + n)) with 0 <= n < 16.
     */
    size_t linesize;
    /* If these are equal then no icache lines have been invalidated. Otherwise
     * they are both aligned to the icache line size and describe a set of
     * consecutive icache lines (which could wrap around the top of memory).
     */
    void *begin, *end;
    /* Some space to spill registers. */
    ptr_uint_t spill[2];
} icache_op_struct_t;

#endif /* _MANGLE_AARCH64_H_ */
