/* **********************************************************
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2005-2007 Determina Corp. */

/*
 * hotpatch.h - Exported interfaces from the hot patching module
 */

#ifndef _HOTPATCH_H_
#define _HOTPATCH_H_ 1

#ifdef HOT_PATCHING_INTERFACE /* Around the whole file */

/*----------------------------------------------------------------------------*/
/* Exported data types */

/* This type identifies a patch point that was matched during offset lookup.
 * It describes the module, set and vulnerability the matching patch point
 * belongs to.
 */
typedef struct {
    /* Array index of the matching vulnerability in hotp_vul_table. */
    int vul_index;

    /* Array index of the matching set in hotp_vul_table[vul_index]. */
    int set_index;
    int module_index; /* Matching module index in the matching set. */
    int ppoint_index; /* Matching patch point index in the matching module. */
} hotp_offset_match_t;

/*----------------------------------------------------------------------------*/
/* Exported data */
typedef enum {
    /* Definitions of context types that are to be dumped into forensics files.
     * Used by hotp, hotp_only & gbop to dump the right app context; case 8099.
     * This is to overcome the limitations in using our current interface
     * to report violations; planned to be fixed for Marlin - case 8079.
     */
    CXT_TYPE_HOT_PATCH,
    CXT_TYPE_CORE_HOOK
} cxt_type_t;

/* Leak to handle case 9593.  This should go if we find a cleaner solution. */
#    ifdef HEAP_ACCOUNTING
DEBUG_DECLARE(extern int hotp_only_tramp_bytes_leaked;)
#    endif
#    ifdef DEBUG_MEMORY
DEBUG_DECLARE(bool hotp_only_contains_leaked_trampoline(byte *pc, size_t size);)
#    endif
/*----------------------------------------------------------------------------*/
/* Exported function prototypes */
bool
hotp_does_region_need_patch(const app_pc start, const app_pc end,
                            bool own_hot_patch_lock);
bool
hotp_inject(dcontext_t *dcontext, instrlist_t *ilist);

/* hotp_process_image() can be called in two different ways, one to process
 * the image and the other one to just check it (bool just_check).  just_check
 * is used only in hotp_only mode to find out if a dll being loaded needs
 * patching.
 * FIXME: have another wrapper for just_check case; keeps it clean.
 */
void
hotp_process_image(const app_pc base, const bool loaded, const bool own_hot_patch_lock,
                   const bool just_check, bool *needs_processing,
                   const thread_record_t **all_threads, const int num_threads);

bool
hotp_ppoint_on_list(app_rva_t ppoint, app_rva_t *hotp_ppoint_vec,
                    uint hotp_ppoint_vec_num);

/* Returns the number of patch points for the matched vuls in [start,end).
 * For now this routine assumes that [start,end) is contained in a single module.
 * The caller must own the hotp_vul_table_lock (as a read lock).
 */
int
hotp_num_matched_patch_points(const app_pc start, const app_pc end);

/* Stores in vec the offsets for all the matched patch points in [start,end).
 * Returns -1 if vec_num is too small (still fills it up).
 * For now this routine assumes that [start,end) is contained in a single module.
 * The caller must own the hotp_vul_table_lock (as a read lock).
 */
int
hotp_get_matched_patch_points(const app_pc start, const app_pc end, app_rva_t *vec,
                              uint vec_num);

/* Checks whether any matched patch point in [start, end) is not listed on
 * hotp_ppoint_vec.  If hotp_ppoint_vec is NULL just checks whether any patch
 * point is matched in the region.  For now this routine assumes that
 * [start,end) is contained in a single module.
 */
bool
hotp_point_not_on_list(const app_pc start, const app_pc end, bool own_hot_patch_lock,
                       app_rva_t *hotp_ppoint_vec, uint hotp_ppoint_vec_num);

void
hotp_nudge_handler(uint nudge_action_mask);
void
hotp_init(void);
void
hotp_exit(void);
void
hotp_reset_init(void);
void
hotp_reset_free(void);
read_write_lock_t *
hotp_get_lock(void);
void
hotp_print_diagnostics(file_t diagnostics_file);
bool
hotp_only_in_tramp(const app_pc eip);
void
hotp_only_detach_helper(void);
void
hotp_only_mem_prot_change(const app_pc start, const size_t size, const bool remove,
                          const bool inject);
void
hotp_spill_before_notify(dcontext_t *dcontext, fragment_t **frag_spill /* OUT */,
                         fragment_t *new_frag, const app_pc new_frag_tag,
                         app_pc *new_tag_spill /* OUT */, const app_pc new_tag,
                         priv_mcontext_t *cxt_spill /* OUT */, const void *new_cxt,
                         cxt_type_t cxt_type);
void
hotp_restore_after_notify(dcontext_t *dcontext, const fragment_t *old_frag,
                          const app_pc old_next_tag, const priv_mcontext_t *old_cxt);

#endif /* HOT_PATCHING_INTERFACE  Around the whole file */
#endif /* _HOTPATCH_H_ 1 */
