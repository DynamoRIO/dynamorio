/* **********************************************************
 * Copyright (c) 2002-2008 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

#ifdef LOAD_TO_CONST
#    ifndef __LOADTOCONST_H_
#        define __LOADTOCONST_H_

#        define NUM_VALUES_FOR_SPECULATION 40
#        define SAMPLE_PERCENT (9 / 10)

#        define MAX_TRACES_WAITING_FOR_LTC 20
#        include "instr.h"

struct ltc_mem_ref_data {
    opnd_t opnd;
    int vals[NUM_VALUES_FOR_SPECULATION];
    int addresses[NUM_VALUES_FOR_SPECULATION];
};
struct ltc_data {
    struct ltc_mem_ref_data *mem_refs;
    int num_mem_addresses;
    int num_mem_samples;
    bool ltc_already_optimized;
};

#        ifdef LTC_STATS
extern int safe_taken, opt_taken, addrs_analyzed, addrs_made_const, traces_analyzed,
    addrs_seen;
#        endif

void
analyze_memrefs(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

int
check_mem_refs(app_pc tag, int errno, reg_t eflags, reg_t reg_edi, reg_t reg_esi,
               reg_t reg_ebp, reg_t reg_esp, reg_t reg_ebx, reg_t reg_edx, reg_t reg_ecx,
               reg_t reg_eax);
int
get_mem_address(dcontext_t *dcontext, opnd_t mem_access, int regs[8]);
int
get_mem_val(dcontext_t *dcontext, opnd_t mem_access, int address);
void
ltc_trace(dcontext_t *dcontext, fragment_t *, instrlist_t *trace);
void
remove_mem_ref_check(dcontext_t *dcontext, instrlist_t *trace);

#        ifdef SIDELINE
void
LTC_examine_traces(void);
void
LTC_fragment_delete(fragment_t *frag);
#        endif

void
LTC_online_optimize_and_replace(dcontext_t *dcontext, app_pc tag, fragment_t *curfrag);
void
do_single_LTC(dcontext_t *dcontext, instrlist_t *trace, opnd_t mem_access, opnd_t value);
bool
should_replace_load(dcontext_t *dcontext, struct ltc_mem_ref_data data);
opnd_t value_to_replace(struct ltc_mem_ref_data);
bool
instr_replace_reg_with_const_in_src(dcontext_t *dcontext, instr_t *in, int reg, int val);
bool
instr_replace_reg_with_const_in_dst(dcontext_t *dcontext, instr_t *in, int reg, int val);
instrlist_t *
save_eflags_list(dcontext_t *dcontext, fragment_t *frag);
instrlist_t *
restore_eflags_list(dcontext_t *dcontext, fragment_t *frag);
void
change_cbr_due_to_reversed_cmp(instr_t *in);
bool
pc_reads_flags_before_writes(dcontext_t *dcontext, app_pc target);

instr_t *
fix_cmp_containing_constant(dcontext_t *dcontext, instrlist_t *trace, instr_t *in,
                            opnd_t orig_op1, opnd_t orig_op2);

void
replace_self_loop_with_opnd(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist,
                            opnd_t desiredtarget);
#        define TRANSPOSE true
#        define NO_TRANSPOSE false
bool
safe_to_transpose_cmp(dcontext_t *dcontext, instr_t *testinstr);
bool
safe_to_delete_cmp(dcontext_t *dcontext, instr_t *testinstr);
bool
safe_to_modify_cmp(dcontext_t *dcontext, instr_t *testinstr, bool transpose);
bool
becomes_ubr_from_cmp(instr_t *in, int op1, int op2);
bool
opnd_replace_reg_with_val(opnd_t *opnd, int old_reg, int val);
void
constant_propagate(dcontext_t *dcontext, instrlist_t *trace, app_pc tag);
void
pop_instr_off_list(dcontext_t *dcontext, instrlist_t *trace, int opcode);
void
instr_optimize_constant(dcontext_t *dcontext, instr_t *in);
bool
instr_flag_write_necessary(dcontext_t *dcontext, instr_t *in);

void
instr_replace_inplace(dcontext_t *dcontext, instr_t *in, instr_t *);
void
instr_arithmatic_simplify(dcontext_t *dcontext, instr_t *in);
void
instrlist_remove_nops(dcontext_t *dcontext, instrlist_t *trace);
bool
instrlist_depends_on_reg(dcontext_t *dcontext, instrlist_t *trace, int reg);
void
instr_add_to_exitexec_list(dcontext_t *, instr_t *in, instr_t *exitinstr);
void
instrlist_setup_pseudo_exitstubs(dcontext_t *dcontext, instrlist_t *trace);
#    endif
#endif
