/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
 * Copyright (c) 2023      Arm Limited.   All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRio eXtension utilities */

#include "dr_api.h"

extern int drx_scatter_gather_tls_idx;

/* Make each scatter or gather instruction be in their own basic block.
 */
bool
scatter_gather_split_bb(void *drcontext, instrlist_t *bb,
                        DR_PARAM_OUT instr_t **sg_instr);

/* Tell drx_event_restore_state() that an expansion has occurred. */
void
drx_mark_scatter_gather_expanded(void);

typedef struct _scatter_gather_info_t {
#if defined(X86)
    bool is_evex;
#endif
    bool is_load;

#if defined(AARCH64)
    /* The vector element size for all vector registers used by the instruction.
     * This applies to:
     *      gather_dst_reg/scatter_src_reg for all
     *          scatter/gather/predicated-contiguous-access instructions,
     *      base_reg for vector+immediate scatter/gather instructions,
     *      index_reg for scalar+vector scatter/gather instructions.
     */
    opnd_size_t element_size;
#elif defined(X86)
    opnd_size_t scalar_index_size;
#endif

    opnd_size_t scalar_value_size;
    opnd_size_t scatter_gather_size;
    reg_id_t mask_reg;
    reg_id_t base_reg;
    reg_id_t index_reg;
    union {
        reg_id_t gather_dst_reg;
        reg_id_t scatter_src_reg;
    };
    int disp;
#if defined(X86)
    int scale;
#elif defined(AARCH64)
    dr_extend_type_t extend;
    uint extend_amount;
    uint reg_count; /* Number of registers accessed. If >1
                     * gather_dst_reg/scatter_src_reg is the first register.
                     */
    bool scaled;
    bool is_scalar_value_signed;
    bool is_replicating; /* The instruction is an ld1rq[bhwd] or ld1ro[bhwd] instruction
                          * which loads a fixed size vector that is replicated to fill
                          * the destination register.
                          */
    enum {
        DRX_NORMAL_FAULTING,
        DRX_FIRST_FAULTING,
        DRX_NON_FAULTING,
    } faulting_behavior;
#endif
} scatter_gather_info_t;

/* These architecture specific functions and defined in scatter_gather_${ARCH_NAME}.c
 * and used by functions in scatter_gather_shared.c
 */
void
drx_scatter_gather_thread_init(void *drcontext);

void
drx_scatter_gather_thread_exit(void *drcontext);

bool
drx_scatter_gather_restore_state(void *drcontext, dr_restore_state_info_t *info,
                                 instr_t *sg_inst);

/* Check if an instruction has been marked as a load or store that is part of a
 * scatter/gather instruction expansion.
 */
bool
scatter_gather_is_expanded_ld_st(instr_t *instr);

/* Mark an instruction as a load or store that is part of a scatter/gather instruction
 * expansion.
 */
void
scatter_gather_tag_expanded_ld_st(instr_t *instr);
