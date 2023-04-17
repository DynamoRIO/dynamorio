/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* translation.h -- fault translation
 */

#ifndef _TRANSLATE_H_
#define _TRANSLATE_H_ 1

enum {
    /* Each translation entry represents a sequence of instructions.
     * If TRANSLATE_IDENTICAL is set, that sequence of instructions
     * share the same translation ("identical" == stride 0); otherwise, the
     * translation should advance by instruction length with distance
     * from the head ("contiguous" == stride of instr length).
     * Generally identical instrs are mangled sequences that we inserted,
     * though currently the table creation scheme doesn't follow that
     * on all contig-identical borders.
     */
    TRANSLATE_IDENTICAL = 0x0001,    /* otherwise contiguous */
    TRANSLATE_OUR_MANGLING = 0x0002, /* added by our own mangling (PR 267260) */
    TRANSLATE_CLEAN_CALL = 0x0004,   /* Added by our own mangling. */
};                                   /* no typedef b/c we need ushort not int */

/* Translation table entry (case 3559).
 * PR 299783: for now we only support pc translation, not full arbitrary reg
 * state mappings, which aren't needed for DR but may be nice for clients.
 */
typedef struct _translation_entry_t {
    /* offset from fragment start_pc */
    ushort cache_offs;
    /* TRANSLATE_ flags */
    ushort flags;
    app_pc app;
} translation_entry_t;

/* Translation table that records info for translating cache pc to app
 * pc without reading app memory (used when it is unsafe to do so).
 * The table records only translations at change points, so the
 * recreater must interpolate between them, using either a stride of 0
 * if the previous translation entry is marked "identical" or a stride
 * equal to the instruction length as we decode from the cache if the
 * previous entry is !identical=="contiguous".
 */
typedef struct _translation_info_t {
    uint num_entries;
    /* an array of num_entries elements */
    translation_entry_t translation[1]; /* variable-sized */
} translation_info_t;

/* PR 244737: all generated code is thread-shared on x64 */
#define IS_SHARED_SYSCALL_THREAD_SHARED IF_X64_ELSE(true, false)

/* state translation for faults and thread relocation */
app_pc
recreate_app_pc(dcontext_t *tdcontext, cache_pc pc, fragment_t *f);

typedef enum {
    RECREATE_FAILURE,
    RECREATE_SUCCESS_PC,
    RECREATE_SUCCESS_STATE,
} recreate_success_t;

recreate_success_t
recreate_app_state(dcontext_t *tdcontext, priv_mcontext_t *mcontext, bool restore_memory,
                   fragment_t *f);

void
translation_info_free(dcontext_t *tdcontext, translation_info_t *info);
translation_info_t *
record_translation_info(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist);
void
translation_info_print(const translation_info_t *info, cache_pc start, file_t file);
#ifdef INTERNAL
void
stress_test_recreate_state(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist);
#endif

bool
at_syscall_translation(dcontext_t *dcontext, app_pc pc);

/* Returns a replacement pc if it is a special case such as in an rseq region;
 * else returns pc.
 */
app_pc
translate_restore_special_cases(dcontext_t *dcontext, app_pc pc);

/* Returns the direct translation when given the "official" translation.
 * Some special cases like rseq sequences obfuscate the interrupted PC: i#4041.
 */
app_pc
translate_last_direct_translation(dcontext_t *dcontext, app_pc pc);

void
translate_clear_last_direct_translation(dcontext_t *dcontext);

#endif /* _TRANSLATE_H_ */
