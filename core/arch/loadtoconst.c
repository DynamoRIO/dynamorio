/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

#include "configure.h"
#ifdef LOAD_TO_CONST /* around whole file */

#    include "../globals.h"
#    include "arch.h"
#    include "instr.h"
#    include "instr_create_shared.h"
#    include "instrlist.h"
#    include "decode.h"
#    include "decode_fast.h"
#    include "disassemble.h"
#    include "proc.h"
#    include "../fragment.h"
#    include "instrument.h"
#    include "../emit.h"
#    include "../link.h"

#    include "loadtoconst.h"

#    ifdef SIDELINE
fragment_t *frags_waiting_LTC[MAX_TRACES_WAITING_FOR_LTC];
int num_frags_waiting_LTC;
DECLARE_CXTSWPROT_VAR(mutex_t waiting_LTC_lock, INIT_LOCK_FREE(waiting_LTC_lock));
#    endif

#    ifdef LTC_STATS
int safe_taken, opt_taken, addrs_analyzed, addrs_made_const, traces_analyzed, addrs_seen;
#    endif

#    define NUM_TMP_OPNDS 200
static opnd_t tmpOpnds[NUM_TMP_OPNDS];

void
analyze_memrefs(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *instr, *writechecker, *accesschecker, *oldtracetop;
    int a, reg, removalpossibilities = 0, instances_of_address;
    bool regs_modified[8], address_written, regs_written, first_instance_of_address;
    bool has_back_arc;
    opnd_t mem_access;

    LOG(THREAD, LOG_OPTS, 3, "in analyze_memrefs\n");
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "before analyze_memrefs optimization:\n");
    if (d_r_stats->loglevel >= 3)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif
#    ifdef LTC_STATS
    traces_analyzed++;
#    endif

    memset(regs_modified, 0, sizeof(bool) * 8);

    /* first walk through to find which registers are constant */
    for (instr = instrlist_first(trace); instr != NULL; instr = instr_get_next(instr)) {
        for (reg = REG_EAX; reg <= REG_EDI; reg++) {
            if (instr_writes_to_reg(instr, reg)) {
                regs_modified[reg - REG_EAX] = true;
            }
        }
    }

    has_back_arc = !!find_next_self_loop(dcontext, tag, instrlist_first(trace));

    for (instr = instrlist_first(trace); instr != NULL; instr = instr_get_next(instr)) {
        switch (instr_get_opcode(instr)) {
        case OP_int:
        case OP_int3:
        case OP_into:
        case OP_call:
        case OP_syscall:
        case OP_sysenter:
            LOG(THREAD, LOG_OPTS, 3, "trace contains a syscall. fuck it");
            return;
        default:
        }

        if (instr_reads_memory(instr)) {
            int basereg, indexreg;
            address_written = regs_written = false;
            mem_access = instr_get_src_mem_access(instr);
            basereg = opnd_get_base(mem_access);
            indexreg = opnd_get_index(mem_access);

            d_r_logopnd(dcontext, 3, mem_access, "checking this operand in");
            d_r_loginst(dcontext, 3, instr, "\tthis instruction");

            if (mem_access.size == OPSZ_4_short2 &&
                !instr_get_prefix_flag(instr, PREFIX_DATA)) {
            } else if (mem_access.size == OPSZ_4) {
            } else {
                LOG(THREAD, LOG_OPTS, 3,
                    "\tthis operand is not size_v or size_d.  its %s: ",
                    size_names[mem_access.size]);
                d_r_loginst(dcontext, 3, instr, "");
                continue;
            }

            if ((basereg != REG_NULL && basereg != REG_EBP) ||
                (indexreg != REG_NULL && indexreg != REG_EBP)) {
                d_r_logopnd(dcontext, 3, mem_access,
                            "\tthis mem access reads a sketch register, so forget it");
                continue;
            }

            first_instance_of_address = true;
            for (accesschecker = instrlist_first(trace); accesschecker != instr;
                 accesschecker = instr_get_next(accesschecker)) {
                if (instr_reads_memory(accesschecker) &&
                    opnd_same_address(instr_get_src_mem_access(accesschecker),
                                      mem_access) &&
                    accesschecker != instr)
                    first_instance_of_address = false;
            }
            if (!first_instance_of_address) {
                LOG(THREAD, LOG_OPTS, 3, "\tnot the first instance of this address...\n");
                continue;
            }

#    ifdef LTC_STATS
            addrs_seen++;
#    endif

            if (basereg != REG_NULL) {
                ASSERT(basereg >= REG_EAX && basereg <= REG_EDI);
                if (regs_modified[basereg - REG_EAX] == true) {
                    regs_written = true;
                    LOG(THREAD, LOG_OPTS, 3,
                        "\tbase register of access written to, so can't optimize\n");
                }
            }

            if (indexreg != REG_NULL) {
                ASSERT(indexreg >= REG_EAX && indexreg <= REG_EDI);
                if (regs_modified[indexreg - REG_EAX] == true) {
                    regs_written = true;
                    LOG(THREAD, LOG_OPTS, 3,
                        "\tindex register of access written to, so can't optimize\n");
                }
            }

            instances_of_address = 0;

            for (writechecker = instrlist_first(trace); writechecker != NULL;
                 writechecker = instr_get_next(writechecker)) {
                if (instr_writes_memory(writechecker) &&
                    opnd_same_address(mem_access, instr_get_dst(writechecker, 0))) {
                    address_written = true;
                    LOG(THREAD, LOG_OPTS, 3,
                        "\tsame address as access written to, so can't optimize\n");
                }

                if (instr_reads_memory(writechecker) &&
                    opnd_same_address(mem_access, instr_get_src_mem_access(writechecker)))
                    instances_of_address++;
            }

            if (!regs_written && !address_written && opnd_is_base_disp(mem_access) &&
                (instances_of_address > 5 || has_back_arc)) {
                // this load could be turned into a constant move

                // save this mem access operand
                tmpOpnds[removalpossibilities++] = mem_access;
                ASSERT(removalpossibilities < NUM_TMP_OPNDS);

                LOG(THREAD, LOG_OPTS, 3,
                    "%d instances of addresses which could be constants",
                    instances_of_address);
                d_r_logopnd(dcontext, 3, mem_access, "");

                if (has_back_arc) {
                    LOG(THREAD, LOG_OPTS, 3, "this trace loops!\n");
                }

            } else {
                LOG(THREAD, LOG_OPTS, 3,
                    "this instr not optimized because instances of address: %d. backarc "
                    "= %d\n",
                    instances_of_address, has_back_arc);
            }
        }
    }

    LOG(THREAD, LOG_OPTS, 3, "%d removal possibilities in this trace\n",
        removalpossibilities);

    ASSERT(trace->ltc.mem_refs == NULL);

    if (removalpossibilities > 0) {
        // if there actually might be any vals to make constant
        // save a list of the memory references for this trace in the fragment struct
#    ifdef LTC_STATS
        addrs_analyzed += removalpossibilities;
#    endif

        trace->ltc.mem_refs = heap_alloc(dcontext,
                                         sizeof(struct ltc_mem_ref_data) *
                                             removalpossibilities HEAPACCT(ACCT_OTHER));
        trace->ltc.num_mem_addresses = removalpossibilities;
        trace->ltc.num_mem_samples = 0;

        for (a = 0; a < removalpossibilities; a++)
            trace->ltc.mem_refs[a].opnd = tmpOpnds[a];

        oldtracetop = instrlist_first(trace);

        // move the clean call outside of the loop
        replace_self_loop_with_opnd(dcontext, (app_pc)tag, trace,
                                    opnd_create_instr(oldtracetop));

        LOG(THREAD, LOG_OPTS, 3, "LTC: inserting clean call in trace tag= " PFX "\n",
            tag);
#    ifdef SIDELINE
        if (dynamo_options.sideline)
            dr_insert_clean_call(dcontext, trace, oldtracetop, (app_pc)check_mem_refs,
                                 false /*!fp*/, 1, OPND_CREATE_INTPTR(tag));
        else {
#    endif
            // if not sideline then insert code to do optimization immediately
            insert_clean_call_with_arg_jmp_if_ret_true(dcontext, trace, oldtracetop,
                                                       (app_pc)check_mem_refs, (int)tag,
                                                       (app_pc)tag, NULL);
#    ifdef SIDELINE
        }
#    endif

        LOG(THREAD, LOG_OPTS, 3, "inserted clean call to check_mem_refs tag= " PFX "\n",
            tag);
        LOG(THREAD, LOG_OPTS, 3, "newly trace tag=" PFX "  addresses=%d samples=%d\n",
            tag, trace->ltc.num_mem_addresses, trace->ltc.num_mem_samples);
    }
    ASSERT(instr_get_opcode(instrlist_last(trace)) == OP_jmp);
}

int
check_mem_refs(app_pc tag, int errno, reg_t eflags, reg_t reg_edi, reg_t reg_esi,
               reg_t reg_ebp, reg_t reg_esp, reg_t reg_ebx, reg_t reg_edx, reg_t reg_ecx,
               reg_t reg_eax)
{
    // regs puts the values into the order of the reg enum from inst.h
    int regs[8] = {
        reg_eax, reg_ecx, reg_edx, reg_ebx, reg_esp, reg_ebp, reg_esi, reg_edi
    };
    int address, val, a;
    dcontext_t *dcontext;
    fragment_t *curfrag;
    trace_only_t *t_curfrag;

    dcontext = get_thread_private_dcontext();

    ASSERT(dcontext);

    curfrag = fragment_lookup(dcontext, tag);
    t_curfrag = TRACE_FIELDS(curfrag);

    ASSERT(curfrag && curfrag->tag);

    // this shouldn't happen very much, just waiting for sideline thread to remove call

    if (t_curfrag->ltc.num_mem_samples == NUM_VALUES_FOR_SPECULATION) {
#    ifdef SIDELINE
        if (dynamo_options.sideline)
            return true;
        else {
#    endif
            LOG(THREAD, LOG_OPTS, 1,
                "should never get called unnecessarily if not -sideline tag=" PFX, tag);
            ASSERT_NOT_REACHED();
#    ifdef SIDELINE
        }
#    endif
    }

    LOG(THREAD, LOG_OPTS, 3,
        "check_mem_refs fragment " PFX " tag=" PFX "  addresses %d samples=%d\n", curfrag,
        curfrag->tag, t_curfrag->ltc.num_mem_addresses, t_curfrag->ltc.num_mem_samples);

    ASSERT(t_curfrag->ltc.num_mem_samples < NUM_VALUES_FOR_SPECULATION);

    for (a = 0; a < t_curfrag->ltc.num_mem_addresses; a++) {
        opnd_t mem_access = t_curfrag->ltc.mem_refs[a].opnd;

        address = get_mem_address(dcontext, mem_access, regs);
        t_curfrag->ltc.mem_refs[a].addresses[t_curfrag->ltc.num_mem_samples] = address;

        val = get_mem_val(dcontext, mem_access, address);
        t_curfrag->ltc.mem_refs[a].vals[t_curfrag->ltc.num_mem_samples] = val;
    }

    // after all addresses are read, *then* increment the number of samples
    // all addresses have the same number of samples

    if (++t_curfrag->ltc.num_mem_samples == NUM_VALUES_FOR_SPECULATION) {
        LOG(THREAD, LOG_OPTS, 3, "fragment (tag=" PFX ") ready for optimization\n",
            curfrag->tag);

#    ifdef DEBUG
        // this block can be removed, its just printing...
        if (d_r_stats->loglevel >= 4) {
            for (address = 0; address < t_curfrag->ltc.num_mem_addresses; address++) {
                d_r_logopnd(dcontext, 3, t_curfrag->ltc.mem_refs[address].opnd,
                            "\tgot enough values for");

                for (a = 0; a < NUM_VALUES_FOR_SPECULATION; a++)

                    LOG(THREAD, LOG_OPTS, 3, "\t\tsample %d: addr=" PFX " val=" PFX "\n",
                        a, t_curfrag->ltc.mem_refs[address].addresses[a],
                        t_curfrag->ltc.mem_refs[address].vals[a]);
            }
        }
#    endif

#    ifdef SIDELINE
        if (dynamo_options.sideline) { // only use the frags_waiting list if using
                                       // sideline
            ASSERT_NOT_IMPLEMENTED(false && "this lock needs DELETE_LOCK");
            d_r_mutex_lock(&waiting_LTC_lock);
            ASSERT(num_frags_waiting_LTC < MAX_TRACES_WAITING_FOR_LTC &&
                   num_frags_waiting_LTC >= 0);

            // check that the fragment isn't already in the list
            // its possible that checkmemrefs could be called twice (by the trace
            // executing twice before sideline_optimize was able to replace the trace
            for (a = 0; a < num_frags_waiting_LTC; a++) {
                if (frags_waiting_LTC[a] == curfrag) { // its already in the list
                    d_r_mutex_unlock(&waiting_LTC_lock);
                    return false; // return value doesn't matter in sideline case
                }
            }

            // if we get here, then the fragment isn't already in the list
            frags_waiting_LTC[num_frags_waiting_LTC++] = curfrag;
            d_r_mutex_unlock(&waiting_LTC_lock);
        } else // if not doing sideline, then do the insertion immediately!
               // (deterministically!)
#    endif
        {
#    ifdef DEBUG
            if (d_r_stats->loglevel >= 3) {
                LOG(THREAD, LOG_OPTS, 3,
                    "check mem refs returning true for tag " PFX "\n", tag);
                disassemble_fragment(dcontext, curfrag, 0);
            }
#    endif
            return true; // returning true
        }
    }
    return false; // if !sideline, then this value means not enough values
}

void
LTC_online_optimize_and_replace(dcontext_t *dcontext, app_pc tag, fragment_t *curfrag)
{
    fragment_t *new_f;
    instrlist_t *ilist;
    trace_only_t *t_curfrag = TRACE_FIELDS(curfrag);
    void *vmlist = NULL;
    DEBUG_DECLARE(bool ok;)
    ASSERT(curfrag);
    LOG(THREAD, LOG_OPTS, 3,
        "LTC: doing online optimization of current trace. tag=" PFX "\n", tag);

    // all this fragment shit is taken from upgrade_to_trace_head in monitor.c
    ilist = decode_fragment(dcontext, curfrag, NULL, NULL, curfrag->flags, NULL, NULL);
    if (instr_get_opcode(instrlist_last(ilist)) != OP_jmp) {
        LOG(THREAD, LOG_OPTS, 3, "This fragment doesn't end in a OP_jmp\n");
        disassemble_fragment(dcontext, curfrag, 0);
        ASSERT_NOT_REACHED();
    }

    remove_mem_ref_check(dcontext, ilist);
    ltc_trace(dcontext, curfrag, ilist); // actually do the optimization

    ASSERT(t_curfrag->ltc.mem_refs);
    heap_free(dcontext, t_curfrag->ltc.mem_refs,
              sizeof(struct ltc_mem_ref_data) *
                  t_curfrag->ltc.num_mem_addresses HEAPACCT(ACCT_OTHER));
    t_curfrag->ltc.mem_refs = NULL;

    DEBUG_DECLARE(ok =)
    vm_area_add_to_list(dcontext, curfrag->tag, &vmlist, curfrag->flags, curfrag,
                        false /*no locks*/);
    ASSERT(ok); /* should never fail for private fragments */
    new_f =
        emit_invisible_fragment(dcontext, curfrag->tag, ilist, curfrag->flags, vmlist);
    fragment_copy_data_fields(dcontext, curfrag, new_f);
    shift_links_to_new_fragment(dcontext, curfrag, new_f);

    fragment_replace(dcontext, curfrag, new_f);
    fragment_delete(dcontext, curfrag,
                    FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE);
    /* free the instrlist_t elements */
    instrlist_clear_and_destroy(dcontext, ilist);

    // returning true means that it should jump back to the top of trace
    // need to make sure that the exit is not linked so that
#    ifdef DEBUG
    if (d_r_stats->loglevel >= 3) {
        LOG(THREAD, LOG_OPTS, 3, "new fragment after doing ltc\n");
        disassemble_fragment(dcontext, new_f, 0);
    }
#    endif
}

// the regs param is as defined in check_mem_refs
int
get_mem_address(dcontext_t *dcontext, opnd_t mem_access, int regs[8])
{

    int indexreg, basereg, index, base, scale, disp;
    int *address;
    d_r_logopnd(dcontext, 3, mem_access, "getting this val");
    ASSERT(opnd_is_near_base_disp(mem_access));

    indexreg = opnd_get_index(mem_access);

    if (indexreg != REG_NULL) {
        ASSERT(reg_is_32bit(indexreg));
        index = regs[indexreg - REG_EAX];
    } else
        index = 0;

    basereg = opnd_get_base(mem_access);
    if (basereg != REG_NULL) {
        ASSERT(reg_is_32bit(basereg));
        base = regs[basereg - REG_EAX];
    } else
        base = 0;

    scale = opnd_get_scale(mem_access);
    disp = opnd_get_disp(mem_access);

    address = (int *)(base + index * scale + disp);
    LOG(THREAD, LOG_OPTS, 3,
        "base=" PFX ", index=" PFX ", scale=" PFX ", disp=" PFX ". address = " PFX "\n",
        base, index, scale, disp, address);

    return (int)address;
}

int
get_mem_val(dcontext_t *dcontext, opnd_t mem_access, int address)
{
    int val;
    int *addrp = (int *)address;
    // its necessary to to switch on the operand size so you don't fetch too much
    // cuz that could possibly cause a segfault
    switch (mem_access.size) {
    case OPSZ_4:        // 32 bits
    case OPSZ_4_short2: // 32 or 16, but i've only seen 32 so...
        val = *addrp;
        break;
    case OPSZ_1: val = (int)(*(char *)addrp); break;
    case OPSZ_2: val = (int)(*(short *)addrp); break;
    default: d_r_logopnd(dcontext, 3, mem_access, "funky operand"); ASSERT_NOT_REACHED();
    }
    LOG(THREAD, LOG_OPTS, 3, "in get_mem_val addr= %d ", address);
    d_r_logopnd(dcontext, 3, mem_access, "");
    LOG(THREAD, LOG_OPTS, 3, "value =  " PFX "\n", val);
    return val;
}

#    ifdef SIDELINE
void
LTC_examine_traces()
{
    fragment_t *curfrag;
    trace_only_t *t_curfrag;
    ASSERT(num_frags_waiting_LTC >= 0);
    ASSERT(num_frags_waiting_LTC <= MAX_TRACES_WAITING_FOR_LTC);

    if (num_frags_waiting_LTC == 0) {
        return;
    }

    LOG(GLOBAL, LOG_OPTS, 3, "in LTC_examine_traces, %d frags need optimizing\n",
        num_frags_waiting_LTC);

    d_r_mutex_lock(&do_not_delete_lock);
    d_r_mutex_lock(&waiting_LTC_lock);

    curfrag = frags_waiting_LTC[0];
    t_curfrag = TRACE_FIELDS(curfrag);

    num_frags_waiting_LTC--;
    memmove(frags_waiting_LTC, &frags_waiting_LTC[1],
            sizeof(fragment_t *) * num_frags_waiting_LTC);

    d_r_mutex_unlock(&waiting_LTC_lock);

    if (t_curfrag->ltc.ltc_already_optimized) {
        LOG(GLOBAL, LOG_OPTS, 3,
            "LTC_examine_traces: encountered this frag in frags_waiting, but its already "
            "optimized\n",
            curfrag->tag);
        d_r_mutex_unlock(&do_not_delete_lock);
        return;
    }

    LOG(GLOBAL, LOG_OPTS, 3, "LTC_examine_traces: about to optimize F%d\n", curfrag->tag);
    t_curfrag->ltc.ltc_already_optimized = true;
    sideline_optimize(curfrag, remove_mem_ref_check, ltc_trace);
    d_r_mutex_unlock(&do_not_delete_lock);
}

// if a fragment is ever deleted, then remove it from the list of fragments
// that need to be tested for their

void
LTC_fragment_delete(fragment_t *frag)
{
    int a;
    // make sure this list isn't added to while we're in it
    d_r_mutex_lock(&waiting_LTC_lock);

    for (a = 0; a < num_frags_waiting_LTC; a++) {
        if (frag == frags_waiting_LTC[a]) {
            // if the frag deleted is in the list of those waiting to be optimized
            // remove it by shifting the rest of the list down over it.
            // not efficient but it works...

            memmove(&frags_waiting_LTC[a], &frags_waiting_LTC[a + 1],
                    sizeof(fragment_t *) * (num_frags_waiting_LTC - a));
            num_frags_waiting_LTC--;
        }
    }

    d_r_mutex_unlock(&waiting_LTC_lock);
}
#    endif

void
remove_mem_ref_check(dcontext_t *dcontext, instrlist_t *trace)
{
    LOG(THREAD, LOG_OPTS, 3, "remove_mem_ref_check\n");

    ASSERT(instr_get_opcode(instrlist_last(trace)) == OP_jmp);

#    ifdef SIDELINE
    if (dynamo_options.sideline) {
        pop_instr_off_list(dcontext, trace,
                           OP_mov_st); // first instr in clean_call_arg is a store
        pop_instr_off_list(dcontext, trace, OP_mov_ld); // then a load
        pop_instr_off_list(dcontext, trace, OP_pushf);
        pop_instr_off_list(dcontext, trace, OP_pusha);
        pop_instr_off_list(dcontext, trace, OP_push_imm);
        pop_instr_off_list(dcontext, trace, OP_call);
        pop_instr_off_list(dcontext, trace, OP_pop);
        pop_instr_off_list(dcontext, trace, OP_popa);
        pop_instr_off_list(dcontext, trace, OP_popf);
        pop_instr_off_list(dcontext, trace, OP_mov_ld);
    } else {
#    endif
        pop_instr_off_list(dcontext, trace,
                           OP_mov_st); // first instr in clean_call_arg is a store
        pop_instr_off_list(dcontext, trace, OP_mov_ld); // then a load
        pop_instr_off_list(dcontext, trace, OP_pushf);  // ...
        pop_instr_off_list(dcontext, trace, OP_pusha);
        pop_instr_off_list(dcontext, trace, OP_push_imm);
        pop_instr_off_list(dcontext, trace, OP_call);
        pop_instr_off_list(dcontext, trace, OP_pop);
        pop_instr_off_list(dcontext, trace, OP_cmp);
        pop_instr_off_list(dcontext, trace, OP_je);
        pop_instr_off_list(dcontext, trace, OP_popa);
        pop_instr_off_list(dcontext, trace, OP_popf);
        pop_instr_off_list(dcontext, trace, OP_mov_ld);
        pop_instr_off_list(dcontext, trace, OP_jmp);
        pop_instr_off_list(dcontext, trace, OP_popa);
        pop_instr_off_list(dcontext, trace, OP_popf);
        pop_instr_off_list(dcontext, trace, OP_mov_ld);
#    ifdef SIDELINE
    }
#    endif

    // find the pc that it used to jump to get below the inserted call
    // and replace that jump with a jump to the new top
    // replace the jmp to the new top of the trace
    replace_self_loop_with_opnd(dcontext, instrlist_first(trace)->bytes, trace,
                                opnd_create_instr(instrlist_first(trace)));
}

void
pop_instr_off_list(dcontext_t *dcontext, instrlist_t *trace, int opcode)
{
    instr_t *instr = instrlist_first(trace);
    ASSERT(instr_get_opcode(instr) == opcode);
    instrlist_remove(trace, instr);
    instr_destroy(dcontext, instr);
}

void
ltc_trace(dcontext_t *dcontext, fragment_t *frag, instrlist_t *trace)
{
    instrlist_t *opt_trace, *comparisons;
    instr_t *top_safe;
    opnd_t mem_ref, valop;
    int a, num_replaced;
    size_t top_opt_bytes;
    trace_only_t *t_frag = TRACE_FIELDS(frag);
    num_replaced = 0;

    ASSERT(t_frag);
    ASSERT(trace);

    LOG(THREAD, LOG_OPTS, 3,
        "ltc_trace. should actually do the optimization! tag " PFX ", top bytes " PFX
        "\n",
        frag->tag, instrlist_first(trace)->bytes);

    LOG(THREAD, LOG_OPTS, 3, "trace before ltc_trace\n");
#    ifdef DEBUG
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif

#    ifdef SIDELINE
    if (!dynamo_options.sideline)
#    endif
        ASSERT(t_frag->ltc.mem_refs);

    LOG(THREAD, LOG_OPTS, 3, "making self loop point to trace's tag");
    replace_self_loop_with_opnd(dcontext, (app_pc)instrlist_first(trace)->bytes, trace,
                                opnd_create_pc(frag->tag));
    opt_trace = instrlist_clone(dcontext, trace);

    top_safe = instrlist_first(trace);
    LOG(THREAD, LOG_OPTS, 3,
        "calling replace self loop for safe to point at top of safe part\n");
    replace_self_loop_with_opnd(dcontext, (app_pc)frag->tag, trace,
                                opnd_create_instr(top_safe));

    for (a = 0; a < t_frag->ltc.num_mem_addresses; a++)
        if (should_replace_load(dcontext, t_frag->ltc.mem_refs[a]))
            num_replaced++;

    if (num_replaced == 0) {
        LOG(THREAD, LOG_OPTS, 3,
            "should_replace returned false for all mem refs. doing nothing\n");
        return;
    }

    num_replaced = 0;

    instrlist_prepend_instrlist(dcontext, trace, restore_eflags_list(dcontext, frag));

#    ifdef LTC_STATS
    instrlist_prepend(
        trace, INSTR_CREATE_inc(dcontext, OPND_CREATE_MEM32(REG_NULL, (int)&safe_taken)));
#    endif

    top_safe = instrlist_first(trace);
    top_opt_bytes = (size_t)instrlist_first(opt_trace)->bytes; // save old top

    // save eflags at top of comparison
    comparisons = save_eflags_list(dcontext, frag);

    for (a = 0; a < t_frag->ltc.num_mem_addresses; a++) {
        if (should_replace_load(dcontext, t_frag->ltc.mem_refs[a])) {
            num_replaced++;

            mem_ref = t_frag->ltc.mem_refs[a].opnd;
            valop = value_to_replace(t_frag->ltc.mem_refs[a]);

            d_r_logopnd(dcontext, 3, mem_ref, "\tthis memory ref");
            d_r_logopnd(dcontext, 3, valop, "\tgets this value");

            do_single_LTC(dcontext, opt_trace, mem_ref, valop);
#    ifdef LTC_STATS
            addrs_made_const++;
#    endif

            // rather than doing the lea stuff, check the register values directly
            /*      if (opnd_get_base(mem_ref)!=REG_NULL) {
                    LOG(THREAD, LOG_OPTS, 3,"base reg isn't null, so inserting a check for
               it too\n");
                    instrlist_append(comparisons,INSTR_CREATE_cmp(dcontext,opnd_create_reg(opnd_get_base(mem_ref)),OPND_CREATE_INT32(t_frag->mem_refs[a].base_val)));

                    instrlist_append(comparisons,INSTR_CREATE_jcc(dcontext,OP_jne,opnd_create_instr(top_safe)));
                    }
                    if (opnd_get_index(mem_ref)!=REG_NULL) {
                    LOG(THREAD, LOG_OPTS, 3,"index reg isn't null, so inserting a check
               for it too\n");
                    instrlist_append(comparisons,INSTR_CREATE_cmp(dcontext,opnd_create_reg(opnd_get_index(mem_ref)),OPND_CREATE_INT32(t_frag->mem_refs[a].index_val)));
                    instrlist_append(comparisons,INSTR_CREATE_jcc(dcontext,OP_jne,opnd_create_instr(top_safe)));
                    }
            */

            // add a check that the value is what it should be
            instrlist_append(comparisons, INSTR_CREATE_cmp(dcontext, mem_ref, valop));
            instrlist_append(
                comparisons,
                INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_instr(top_safe)));

        } else {
            d_r_logopnd(dcontext, 3, t_frag->ltc.mem_refs[a].opnd,
                        "not replacing me because of bad sampled vals");
        }
    }

    LOG(THREAD, LOG_OPTS, 3, "after ltc");
#    ifdef DEBUG
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif

    if (!dynamo_options.safe_loads_to_const) {
        constant_propagate(dcontext, opt_trace, frag->tag);
        instrlist_remove_nops(dcontext, opt_trace);
    }

    ASSERT(instr_get_prev(instrlist_first(trace)) == NULL);
    ASSERT(instr_get_next(instrlist_last(trace)) == NULL);

    if (dynamo_options.remove_dead_code) {
        LOG(THREAD, LOG_OPTS, 3, "doing remove_dead_code on ltc'd trace");
        remove_dead_code(dcontext, frag->tag, opt_trace);
    }

    if (dynamo_options.rlr) {
        LOG(THREAD, LOG_OPTS, 3, "doing rlr removal on ltc'd trace");
        remove_redundant_loads(dcontext, frag->tag, opt_trace);
    }

    LOG(THREAD, LOG_OPTS, 3, "replacing opt self-loop\n");
    replace_self_loop_with_opnd(dcontext, (app_pc)frag->tag, opt_trace,
                                opnd_create_instr(instrlist_first(opt_trace)));

    instrlist_prepend_instrlist(dcontext, opt_trace, restore_eflags_list(dcontext, frag));
#    ifdef LTC_STATS
    instrlist_prepend(
        opt_trace,
        INSTR_CREATE_inc(dcontext, OPND_CREATE_MEM32(REG_NULL, (int)&opt_taken)));
#    endif

    instrlist_prepend_instrlist(dcontext, trace, opt_trace);
    instrlist_prepend_instrlist(dcontext, trace,
                                comparisons); // comparisons should be first

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "after LTC optimization:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif
}

void
do_single_LTC(dcontext_t *dcontext, instrlist_t *trace, opnd_t mem_access,
              opnd_t const_value)
{
    instr_t *in, *next;
    int val;
    opnd_t instr_mem_access;
    val = (int)opnd_get_immed_int(const_value);
    in = instrlist_first(trace);
    LOG(THREAD, LOG_OPTS, 3, "entering do_single_LTC\n");

    while (in) {
        next = instr_get_next(in);
        d_r_loginst(dcontext, 3, in, "in_single_LTC, examining");
        ASSERT(instr_is_encoding_possible(in));
        if (instr_reads_memory(in)) { // could this be if (!op_lea)
            instr_mem_access = instr_get_src_mem_access(in);
            if (opnd_same_address(instr_mem_access, mem_access)) {
                if (instr_get_opcode(in) == OP_mov_ld) {
                    d_r_loginst(dcontext, 3, in, "\tdoing LTC on mov");
                    d_r_logopnd(dcontext, 3, mem_access, "replacing this operand");
                    d_r_logopnd(dcontext, 3, const_value, "with this");
                    instr_set_opcode(in, OP_mov_imm);
                    instr_replace_src_opnd(in, mem_access, const_value);
                } else if (instr_get_opcode(in) == OP_cmp) {
                    opnd_t orig_op1, orig_op2;
                    orig_op1 = instr_get_src(in, 0);
                    orig_op2 = instr_get_src(in, 1);
                    d_r_loginst(dcontext, 3, in, "\tdoing LTC on cmp");
                    d_r_logopnd(dcontext, 3, const_value, "\tnewvalue");
                    instr_replace_src_opnd(in, mem_access, const_value);
                    d_r_loginst(dcontext, 3, in, "\tafter replacing mem access");

                    next = fix_cmp_containing_constant(dcontext, trace, in, orig_op1,
                                                       orig_op2);
                } else {
                    // try to replace and see if its encodable
                    d_r_loginst(dcontext, 3, in, "\tdoing LTC on a non mov, cmp");
                    instr_replace_src_opnd(in, instr_mem_access, const_value);
                    if (!instr_is_encoding_possible(in)) {
                        instr_replace_src_opnd(in, const_value, instr_mem_access);
                        ASSERT(instr_is_encoding_possible(in));
                    }
                    d_r_loginst(dcontext, 3, in, "\tafter replacing mem access");
                    instr_arithmatic_simplify(dcontext, in);
                }
            }
        }
        in = next;
    }
    LOG(THREAD, LOG_OPTS, 3, "exiting do_single_LTC\n");
}

/*
   takes in a cmp that contains operands that are invalid. does
   what is necessary to make the instruction encode, either
   swapping operands or if there are two constants, it eliminates
   the cmp if possible and walks the rest of the list, fixing
   jcc's appropriately.  returns the next instruction to examine.
*/
instr_t *
fix_cmp_containing_constant(dcontext_t *dcontext, instrlist_t *trace, instr_t *in,
                            opnd_t orig_op1, opnd_t orig_op2)
{
    instr_t *next, *jcc;
    opnd_t tmp;
    next = instr_get_next(in);
    if (instr_is_encoding_possible(in))
        return next;

    // first, try transposing the operands to make it encode
    if ((opnd_is_reg(instr_get_src(in, 0)) || opnd_is_reg(instr_get_src(in, 1))) &&
        safe_to_transpose_cmp(dcontext, in)) {

        tmp = instr_get_src(in, 0); // swap operands
        instr_set_src(in, 0, instr_get_src(in, 1));
        instr_set_src(in, 1, tmp);
        if (instr_is_encoding_possible(in)) {
            // cmp switched, so now switch direction of jcc's that care
            for (jcc = instr_get_next(in); (jcc != NULL) &&
                 ((instr_get_arith_flags(jcc, DR_QUERY_DEFAULT) & EFLAGS_WRITE_6) == 0);
                 jcc = instr_get_next(jcc)) {
                if (instr_is_cbr(jcc)) {
                    d_r_loginst(dcontext, 3, jcc,
                                "change_cbr_due_to_reversed_cmp called on");
                    change_cbr_due_to_reversed_cmp(jcc);
                }
            }
        } else { // swapping operands didn't really work. so put them back and try other
                 // stuff
            instr_set_src(in, 0, orig_op1);
            instr_set_src(in, 1, orig_op2);
            d_r_loginst(
                dcontext, 3, in,
                "wasn't swappable, even though one opnd was a reg. whats up with that?");
            ASSERT_CURIOSITY(false);
        }
    }

    else if ((opnd_is_immed(instr_get_src(in, 0)) &&
              opnd_is_immed(instr_get_src(in, 1))) &&
             safe_to_delete_cmp(dcontext, in)) { // cmp gets two constants
        int op1, op2;
        instr_t *nextjcc;
        d_r_loginst(dcontext, 3, in, "swapping order didn't help, must be 2 constants");

        op1 = (int)opnd_get_immed_int(instr_get_src(in, 0));
        op2 = (int)opnd_get_immed_int(instr_get_src(in, 1));

        d_r_loginst(dcontext, 3, in, "got the two constants");

        ASSERT(instr_get_opcode(in) == OP_cmp);

        for (jcc = instr_get_next(in); jcc != NULL &&
             ((instr_get_arith_flags(jcc, DR_QUERY_DEFAULT) & EFLAGS_WRITE_6) == 0);
             jcc = nextjcc) {
            nextjcc = instr_get_next(jcc);
            d_r_loginst(dcontext, 3, jcc, "walking to try to remove cmp");
            if (instr_is_cbr(jcc)) {
                if (becomes_ubr_from_cmp(jcc, op1, op2)) {
                    // the cbr becomes an unconditional jmp
                    d_r_loginst(dcontext, 3, jcc, "becomes an unconditional jmp");
                    instr_set_opcode(jcc, OP_jmp);
                    ASSERT(instr_is_encoding_possible(jcc));

                    jcc = instr_get_next(jcc);
                    while (jcc) { // removing instrs after jmp
                        nextjcc = instr_get_next(jcc);
                        d_r_loginst(dcontext, 3, jcc,
                                    "removed because it follows an unconditional jmp");
                        instrlist_remove(trace, jcc);
                        instr_destroy(dcontext, jcc);
                        jcc = nextjcc;
                    }
                    nextjcc = NULL;
                } else { // otherwise never jumps, so remove
                    d_r_loginst(dcontext, 3, jcc, "will never jmp, so removed");
                    instrlist_remove(trace, jcc);
                    instr_destroy(dcontext, jcc);
                }
            }
        }

        // remove the cmp instruction
        next = instr_get_next(in);
        d_r_loginst(dcontext, 3, in, "this cmp isn't needed, so remove");
        d_r_loginst(dcontext, 3, next, "setting next to");
        instrlist_remove(trace, in);
        instr_destroy(dcontext, in);
        in = NULL;

    } else { // if none of that shit helped the situation, put back original cmp operands
        instr_set_src(in, 0, orig_op1);
        instr_set_src(in, 1, orig_op2);
        d_r_loginst(
            dcontext, 3, in,
            "wasn't able to fix this instr by either removing or transposing cmp, "
            "original opnds back");
        ASSERT(instr_is_encoding_possible(in));
    }

    if (in && !instr_is_encoding_possible(in)) {
        d_r_loginst(dcontext, 0, in, "error encoding me");
        ASSERT_NOT_REACHED();
    }
    return next;
}

bool
safe_to_transpose_cmp(dcontext_t *dcontext, instr_t *testinstr)
{
    return safe_to_modify_cmp(dcontext, testinstr, TRANSPOSE);
}
bool
safe_to_delete_cmp(dcontext_t *dcontext, instr_t *testinstr)
{
    return safe_to_modify_cmp(dcontext, testinstr, NO_TRANSPOSE);
}

/* deals only with arithmetic flags */
bool
instr_flag_write_necessary(dcontext_t *dcontext, instr_t *in)
{
    instr_t *walker;
    uint eflags;

    // if the instr doesn't write, return false
    if (!(instr_get_arith_flags(in, DR_QUERY_DEFAULT) & EFLAGS_WRITE_6))
        return false;

    walker = instr_get_next(in);
    // otherwise, see if the written flags are read
    while (walker) {
        eflags = instr_get_arith_flags(walker, DR_QUERY_DEFAULT);
        if (eflags & EFLAGS_READ_6)
            return true;
        else if (eflags & EFLAGS_WRITE_6)
            return false;
        else if (instr_is_exit_cti(walker) &&
                 pc_reads_flags_before_writes(dcontext,
                                              opnd_get_pc(instr_get_target(walker))))
            return true;
        walker = instr_get_next(walker);
    }
    return false;
}

/*  this func tests if the currently modified cmp instruction could be unsafe
    a cmp is considered unsafe if it would write flags in a way that they could be
    seen outside of this trace.

    if transpose==NO_TRANSPOSE, then it is checking whether the cmp instr could be removed
   as is if TRANSPOSE, then its checking if the instruction could have its operands
   transposed and it wouldn't affect anything else...
*/

bool
safe_to_modify_cmp(dcontext_t *dcontext, instr_t *testinstr, bool transpose)
{
    opnd_t cmp_op1, cmp_op2;
    instr_t *in;
    bool cmp_both_ops_immed;
    uint eflags;

    // fix: why is this an if? can't I ASSERT(instr_get_opcode(testinstr)==OP_cmp) ?

    if (instr_get_opcode(testinstr) == OP_cmp) {
        if (!transpose) {
            cmp_op1 = instr_get_src(testinstr, 0);
            cmp_op2 = instr_get_src(testinstr, 1);
        } else {
            cmp_op1 = instr_get_src(testinstr, 1);
            cmp_op2 = instr_get_src(testinstr, 0);
        }
        cmp_both_ops_immed = opnd_is_immed(cmp_op1) && opnd_is_immed(cmp_op2);
    } else
        cmp_both_ops_immed = false;

    LOG(THREAD, LOG_OPTS, 3, "in safe_to_modify_cmp: bothopsimmed=%d\n",
        cmp_both_ops_immed);

    for (in = instr_get_next(testinstr); in != NULL; in = instr_get_next(in)) {
        eflags = instr_get_arith_flags(in, DR_QUERY_DEFAULT);

        d_r_loginst(dcontext, 3, in, "\texamining");

        // if the jump is always taken
        if (instr_is_exit_cti(in)) {
            d_r_loginst(dcontext, 3, in, "\texit cti");
            if (cmp_both_ops_immed && instr_is_cbr(in)) {
                ASSERT(instr_get_opcode(testinstr) == OP_cmp);

                // if its a cbr, then you can find out if its taken
                if (transpose == TRANSPOSE) {
                    if (becomes_ubr_from_cmp(in, (int)opnd_get_immed_int(cmp_op2),
                                             (int)opnd_get_immed_int(cmp_op1))) {
                        // if the jump is always taken
                        if (pc_reads_flags_before_writes(
                                dcontext, opnd_get_pc(instr_get_target(in)))) {
                            d_r_loginst(
                                dcontext, 3, in,
                                "jcc always taken, but dest pc depends on flags, true");
                            return false; // if dest pc depends on the flags
                        } else {
                            d_r_loginst(
                                dcontext, 3, in,
                                "jcc always taken,  dest pc overwrites flag, ret false");
                            return true; // if dest pc doesn't depend on flags, then safe
                                         // to modify cmp
                        }
                    }
                } else {
                    ASSERT(transpose == NO_TRANSPOSE);
                    if (becomes_ubr_from_cmp(in, (int)opnd_get_immed_int(cmp_op1),
                                             (int)opnd_get_immed_int(cmp_op2))) {
                        // if the jump is always taken
                        if (pc_reads_flags_before_writes(
                                dcontext, opnd_get_pc(instr_get_target(in)))) {
                            d_r_loginst(
                                dcontext, 3, in,
                                "jcc always taken, but dest pc depends on flags, true");
                            return false; // if dest pc depends on the flags
                        } else {
                            d_r_loginst(
                                dcontext, 3, in,
                                "jcc always taken,  dest pc overwrites flag, ret false");
                            return true; // if dest pc doesn't depend on flags, then safe
                                         // to modify cmp
                        }
                    }
                }
            }
            // if not a cbr, then you have to assume the branch is taken
            else if (pc_reads_flags_before_writes(dcontext,
                                                  opnd_get_pc(instr_get_target(in)))) {
                d_r_loginst(
                    dcontext, 3, in,
                    "\tin safe_to_modify_cmp, this CBR may be taken and reads flags "
                    "before writing");
                return false;
            } else {
                d_r_loginst(
                    dcontext, 3, in,
                    "\tin safe_to_modify_cmp, this CBR may be taken but it writes "
                    "before reads, so the cmp can be changed");
            }
        } else {
            if ((eflags & EFLAGS_READ_6) != 0) {
                d_r_loginst(dcontext, 3, in,
                            "\treads the flags. cmp needed treturning false: ");
                return false;
            }
            if ((eflags & EFLAGS_WRITE_6) != 0) {
                d_r_loginst(dcontext, 3, in,
                            "\twritesthe flags. cmp not needed, ret true:");
                return true;
            }
        }
    }

    // the end of a trace should be a jmp.
    // if this poitn is reached, then it means that jmp's dest writs before reading
    // so its safe to modify the cmp
    return true;
}

bool
pc_reads_flags_before_writes(dcontext_t *dcontext, app_pc target)
{
    uint eflags;
    do {
        target = decode_eflags_usage(dcontext, target, &eflags, DR_QUERY_DEFAULT);
        if ((eflags & EFLAGS_READ_6) != 0) {
            d_r_loginst(dcontext, 3, &tinst, "reads cmpflags before writing");
            return true;
        }
        /* if writes but doesn't read, ok */
        if ((eflags & EFLAGS_WRITE_6) != 0) {
            return false;
        }
        /* stop at 1st cti, don't try to recurse,
         * common case should hit a writer of CF
         * prior to 1st cti
         */
    } while (target != NULL && !instr_is_cti(&tinst));

    return true;
}

bool
becomes_ubr_from_cmp(instr_t *in, int op1, int op2)
{
    ASSERT(instr_is_cbr(in));

    switch (instr_get_opcode(in)) {
    case OP_jle: return (op1 <= op2);
    case OP_jnle: return (op1 > op2);
    case OP_jl: return (op1 < op2);
    case OP_jnl: return (op1 >= op2);
    case OP_jz: return (op1 == op2);
    case OP_jnz: return (op1 != op2);

    case OP_jb: return ((unsigned)op1 < (unsigned)op2);
    case OP_jnb: return ((unsigned)op1 >= (unsigned)op2);
    case OP_jbe: return ((unsigned)op1 <= (unsigned)op2);
    case OP_jnbe: return ((unsigned)op1 > (unsigned)op2);
    default: ASSERT_NOT_REACHED();
    }
    return false;
}

void
change_cbr_due_to_reversed_cmp(instr_t *in)
{
    int opc;
    ASSERT(instr_is_cbr(in));

    switch (instr_get_opcode(in)) {
    case OP_jle: opc = OP_jnl; break;
    case OP_jnle: opc = OP_jl; break;
    case OP_jl: opc = OP_jnle; break;
    case OP_jnl: opc = OP_jle; break;
    case OP_jz:
    case OP_jnz: return;
    case OP_jb: opc = OP_jnbe; break;
    case OP_jnb: opc = OP_jbe; break;
    case OP_jbe: opc = OP_jnb; break;
    case OP_jnbe: opc = OP_jb; break;
    default: ASSERT_NOT_REACHED();
    }
    instr_set_opcode(in, opc);
}

// currently the simple, conservative algorithm is commented out
#    if 0
/* this function is now just a simple check to see if all the sampled
   values are the same.  later this could be changed to something
   more sophisticated
*/
bool
should_replace_load(struct ltc_mem_ref_data data)
{
    int a,firstval,firstaddr;
    firstval=data.vals[0];
    firstaddr=data.addresses[0];

    for (a=1;a<NUM_VALUES_FOR_SPECULATION;a++) {
        if (data.vals[a]!=firstval)
            return false;
        if (data.addresses[a]!=firstaddr)
            return false;
    }

    //if they're all the same, return true...
    return true;
}

//value_to_replace is intimately involved with the should_replace_load alg
//if one is changed, so should the other. now its just real simple
//for no:, this func assumes that they're all the same value
opnd_t
value_to_replace(struct ltc_mem_ref_data data)
{
    return opnd_create_immed_int(data.vals[0],data.opnd.size);
}
#    else
/* this should_replace_load will replace the load if there are any values
   that appear more than half the time. much more aggressive
*/
bool
should_replace_load(dcontext_t *dcontext, struct ltc_mem_ref_data data)
{
    int vals[NUM_VALUES_FOR_SPECULATION], numvals = 0;
    int freq[NUM_VALUES_FOR_SPECULATION];
    int a, b, curval;

    d_r_logopnd(dcontext, 3, data.opnd, "in should_replace_load on this operand");

    for (a = 0; a < NUM_VALUES_FOR_SPECULATION; a++) {
        curval = data.vals[a];
        for (b = 0; b < numvals; b++)
            if (vals[b] == curval) {
                freq[b]++;
                break;
            }
        if (b == numvals) { // that means the value wasn't already there
            freq[numvals] = 1;
            vals[numvals++] = curval; // so add the value to the list!
        }
    }

    // now that the freq table is generated, find any vals that are big enough
    for (a = 0; a < numvals; a++) {
        LOG(THREAD, LOG_OPTS, 3, "%d:\t%d samples of value " PFX "\n", a, freq[a],
            vals[a]);
        if (freq[a] > (NUM_VALUES_FOR_SPECULATION * SAMPLE_PERCENT)) {
            LOG(THREAD, LOG_OPTS, 3,
                "yes we should replace, %d of %d samples (%d unique value(s)) had "
                "value= " PFX "\n",
                freq[a], NUM_VALUES_FOR_SPECULATION, numvals, vals[a]);
            return true;
        }
    }

    // if it got this far, then there are no values that happen more than half the time
    // so we shouldn't replace
    d_r_logopnd(dcontext, 3, data.opnd, "should_replace_load returning false on");
    return false;
}

/* this code is just a copy of should_replace_load, but with a different return value
 */
opnd_t
value_to_replace(struct ltc_mem_ref_data data)
{
    int vals[NUM_VALUES_FOR_SPECULATION], numvals = 0;
    int freq[NUM_VALUES_FOR_SPECULATION];
    int a, b, curval;

    for (a = 0; a < NUM_VALUES_FOR_SPECULATION; a++) {
        curval = data.vals[a];
        for (b = 0; b < numvals; b++)
            if (vals[b] == curval) {
                freq[b]++;
                break;
            }
        if (b == numvals) { // that means the value wasn't already there
            freq[numvals] = 1;
            vals[numvals++] = curval; // so add the value to the list!
        }
    }

    // now that the freq table is generated, find any vals that are big enough
    for (a = 0; a < numvals; a++) {
        if (freq[a] > (NUM_VALUES_FOR_SPECULATION * SAMPLE_PERCENT))
            return opnd_create_immed_int(vals[a], data.opnd.size);
    }

    ASSERT_NOT_REACHED();
    return opnd_create_immed_int(vals[0], data.opnd.size);
}

#    endif

/* saves eax to dcontext
   saves flags in ah and al. don't overwrite them! */
instrlist_t *
save_eflags_list(dcontext_t *dcontext, fragment_t *frag)
{
    instrlist_t *ilist;
    ilist = instrlist_create(dcontext);
    if (dynamo_options.safe_loads_to_const) {
        instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
        instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
        instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
    }
    // save only some
    if (((frag->flags & FRAG_WRITES_EFLAGS_6) == 0) ||
        dynamo_options.safe_loads_to_const) {
        instrlist_append(ilist,
                         instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
        instrlist_append(ilist, INSTR_CREATE_lahf(dcontext));
    }

    if (!TEST(FRAG_WRITES_EFLAGS_OF, frag->flags) || dynamo_options.safe_loads_to_const) {
        /* must have saved eax */
        ASSERT(!TEST(FRAG_WRITES_EFLAGS_6, frag->flags) ||
               dynamo_options.safe_loads_to_const);
        instrlist_append(ilist,
                         INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
    }

    return ilist;
}

instrlist_t *
restore_eflags_list(dcontext_t *dcontext, fragment_t *frag)
{
    instrlist_t *ilist;

    ilist = instrlist_create(dcontext);

    /* now do an add such that OF will be set only if seto set al to 1 */

    if (dynamo_options.safe_loads_to_const) {
        instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
        instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
    }

    if (((frag->flags & FRAG_WRITES_EFLAGS_OF) == 0) ||
        dynamo_options.safe_loads_to_const) {
        instrlist_append(
            ilist,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    if (((frag->flags & FRAG_WRITES_EFLAGS_6) == 0) ||
        dynamo_options.safe_loads_to_const) {
        instrlist_append(ilist, INSTR_CREATE_sahf(dcontext));
        instrlist_append(
            ilist, instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    }
    return ilist;
}

void
constant_propagate(dcontext_t *dcontext, instrlist_t *trace, app_pc tag)
{

    instr_t *instr, *next, *conwalker, *conwalker_next;
    int cons, reg, cons_size;
    bool mov_immed_needed, reg_overwritten;
    opnd_t regop, orig1, orig2;
    LOG(THREAD, LOG_OPTS, 3, "before constant_propagate\n");
#    ifdef DEBUG
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif

    for (instr = instrlist_first(trace); instr != NULL; instr = instr_get_next(instr)) {
        if (instr_get_opcode(instr) == OP_mov_imm) {
            cons = (int)opnd_get_immed_int(instr_get_src(instr, 0));
            cons_size = opnd_get_size(instr_get_src(instr, 0));
            regop = instr_get_dst(instr, 0);
            reg = opnd_get_reg(regop);

            LOG(THREAD, LOG_OPTS, 3, "reg=%s, val=" PFX " ", reg_names[reg], cons);
            d_r_loginst(dcontext, 3, instr, "trying to remove me via constant prop");

            conwalker = instr_get_next(instr);
            mov_immed_needed = false;
            reg_overwritten = false;
            while (conwalker != NULL) {
                conwalker_next = instr_get_next(conwalker);

                if (instr_is_cti(conwalker) && !mov_immed_needed) {
                    opnd_t target = instr_get_target(conwalker);
                    if (opnd_is_near_pc(target) && (opnd_get_pc(target) == tag)) {

                        d_r_loginst(dcontext, 3, conwalker, "this CTI is a self-loop");
                        if (instrlist_depends_on_reg(dcontext, trace, reg)) {
                            LOG(THREAD, LOG_OPTS, 3,
                                "\tsubsequent loop iteration needs the val, so keep "
                                "mov_immed\n");
                            mov_immed_needed = true;
                        } else {
                            LOG(THREAD, LOG_OPTS, 3,
                                "\tloop doesn't need this mov_immed, so lose it\n");
                        }
                    }

                    else {
                        instr_t *foo;
                        d_r_loginst(dcontext, 3, conwalker,
                                    "reached cti without overwriting reg, trying to add "
                                    "instr to the pseudo exitstub");
                        d_r_loginst(dcontext, 3, conwalker,
                                    "this CTI is NOT a self-loop");
                        instr_add_to_exitexec_list(dcontext, conwalker,
                                                   instr_clone(dcontext, instr));

#    ifdef DEBUG
                        if (d_r_stats->loglevel >= 3)
                            for (foo = instr; foo != conwalker->next;
                                 foo = instr_get_next(foo))
                                d_r_loginst(dcontext, 3, foo, "\twalking\t");
#    endif
                    }
                } else if (instr_get_opcode(conwalker) ==
                           OP_lahf /*to catch ind branch*/) {
                    d_r_loginst(dcontext, 3, conwalker,
                                "reached cti without overwriting reg, mov_imm needed");
                    mov_immed_needed = true;
                }

                if (!instr_replace_reg_with_const_in_src(dcontext, conwalker, reg,
                                                         cons)) {
                    if (instr_get_opcode(conwalker) == OP_cmp) {
                        instr_t *oldnext;
                        d_r_loginst(dcontext, 3, conwalker,
                                    "trying to optimize this cmp");
                        orig1 = instr_get_src(conwalker, 0);
                        orig2 = instr_get_src(conwalker, 1);

                        // replace the register with the constant
                        d_r_logopnd(dcontext, 3, regop, "trying to replace this operand");
                        instr_replace_src_opnd(conwalker, regop, OPND_CREATE_INT32(cons));
                        d_r_loginst(dcontext, 3, conwalker,
                                    "replaced reg with const int");
                        // if they're both immeds
                        oldnext = conwalker_next;
                        conwalker_next = fix_cmp_containing_constant(
                            dcontext, trace, conwalker, orig1, orig2);
                        if (oldnext == conwalker_next)
                            mov_immed_needed = true;
                    } else { // if its not a compare
                        d_r_loginst(dcontext, 3, conwalker,
                                    "couldn't replace reg in src, mov_immed needed");
                        mov_immed_needed = true;
                    }
                }

                if (!instr_replace_reg_with_const_in_dst(dcontext, conwalker, reg,
                                                         cons)) {
                    d_r_loginst(dcontext, 3, conwalker,
                                "couldn't replace reg in dst, mov_immed needed");
                    mov_immed_needed = true;
                }

                if (instr_writes_to_reg(conwalker, reg)) {
                    d_r_loginst(dcontext, 3, conwalker, "writes to the reg, so move on");
                    reg_overwritten = true;
                    // if it doesn't overwrite the entire register
                    // then there is still a dependancy on the full 32 bit previous value
                    // so keep the immediate... man, x86 sucks
                    if (!instr_writes_to_exact_reg(conwalker, reg)) {
                        mov_immed_needed = true;
                        d_r_loginst(dcontext, 3, conwalker,
                                    "\tmov_imm needed because of me");
                    }
                    break;
                }

                conwalker = conwalker_next;
            }

            // josh just changed this line
            if (!reg_overwritten) {
                mov_immed_needed = true;
                LOG(THREAD, LOG_OPTS, 3,
                    "\tmov immed needed because the reg isn't overwritten");
            }

            if (!mov_immed_needed) {
                /* remove it */
                next = instr_get_next(instr);
                /* if you're removing the first element in trace
                 * then fix loop to point to second element (which
                 * becomes first element)
                 */

                if (instr == instrlist_first(trace)) {
                    d_r_loginst(dcontext, 3, instr,
                                "trying to remove the first item in the trace");
                    replace_self_loop_with_opnd(dcontext, (size_t)0, trace,
                                                opnd_create_instr(next));
                }
                d_r_loginst(dcontext, 3, instr, "this mov imm isn't needed, remove");
                instrlist_remove(trace, instr);
                instr_destroy(dcontext, instr);
                instr = next;
            }
        }
    }
    ASSERT(instr_get_opcode(instrlist_last(trace)) == OP_jmp);

    instrlist_setup_pseudo_exitstubs(dcontext, trace);

    LOG(THREAD, LOG_OPTS, 3, "after constant_propagate\n");
#    ifdef DEBUG
    if (d_r_stats->loglevel >= 3)
        instrlist_disassemble(dcontext, 0, trace, THREAD);
#    endif
}
// returns true is all's good. (either replaced successfully, or no reg. reference)
// returns false is there's a problem and the mov_immed is needed
bool
instr_replace_reg_with_const_in_src(dcontext_t *dcontext, instr_t *in, int reg, int val)
{
    int a;
    opnd_t op, oldop;
    ASSERT(instr_is_encoding_possible(in));

    for (a = 0; a < instr_num_srcs(in); a++) {
        oldop = op = instr_get_src(in, a);

        if (opnd_is_reg(op) && !opnd_is_reg_32bit(op) &&
            (dr_reg_fixer[opnd_get_reg(op)] == dr_reg_fixer[reg])) {
            LOG(THREAD, LOG_OPTS, 3, "problems with sub registers\n");
            return false; // if its a sub register of reg, then we can't do anything and
                          // need the immed
        }

        if (opnd_replace_reg_with_val(&op, reg, val)) {
            instr_set_src(in, a, op);
            d_r_loginst(dcontext, 3, in, "replaced by this in src");
            // THIS SHOULD BE FIXED BY FIXING is_encoding_possible so it doesn't
            // give true when there is a constant in a rep* instruction
            // when thats fixed, then the next if statement can be

            // fix me: should handle other things like rep_stos?
            if ((instr_get_opcode(in) == OP_rep_cmps ||
                 instr_get_opcode(in) == OP_rep_movs) &&
                (reg == REG_ECX)) {
                d_r_loginst(dcontext, 3, in, "quirk involving rep* instructions and ECX");
                instr_set_src(in, a, oldop);
                return false;
            }

            /* this check checks the case where a single register is used as 2 or more
               sources our code doesn't handle this yet (the constant propagator should be
               able to do some arithmetic simplification with this case).
            */
            if (instr_reg_in_src(in, reg)) {
                d_r_loginst(
                    dcontext, 3, in,
                    "FIXME: doesn't yet handle two src instances of the same register");
                instr_set_src(in, a, oldop);
                return false;
            }

            instr_arithmatic_simplify(dcontext, in);
        }

        if (!instr_is_encoding_possible(in)) {
            d_r_loginst(dcontext, 3, in,
                        "replace_reg_with_const: encoding not possible, so putting back "
                        "original");
            instr_set_src(in, a, oldop);
            d_r_loginst(dcontext, 3, in, "original instr");
            ASSERT(instr_is_encoding_possible(in));
            return false;
        }
    }
    return true;
}

// returns true is all's good. (either replaced successfully, or no reg. reference)
// returns false is there's a problem and the mov_immed is needed
bool
instr_replace_reg_with_const_in_dst(dcontext_t *dcontext, instr_t *in, int reg, int val)
{
    int a;
    opnd_t op, oldop;
    for (a = 0; a < instr_num_dsts(in); a++) {
        oldop = op = instr_get_dst(in, a);
        if (opnd_is_memory_reference(oldop)) {

            if (opnd_replace_reg_with_val(&op, reg, val)) {
                instr_set_dst(in, a, op);
                d_r_loginst(dcontext, 3, in, "replaced by this in dst");
            }
            if (!instr_is_encoding_possible(in)) {
                d_r_loginst(dcontext, 3, in,
                            "encoding not possible, so putting back original");
                instr_set_dst(in, a, oldop);
                d_r_loginst(dcontext, 3, in, "original instr");
                ASSERT(instr_is_encoding_possible(in));
                return false;
            }
        }
    }
    return true;
}

/* Ignores segments */
bool
opnd_replace_reg_with_val(opnd_t *opnd, int old_reg, int val)
{
    switch (opnd->kind) {
    case NULL_kind:
    case IMMED_INTEGER_kind:
    case IMMED_FLOAT_kind:
    case IMMED_DOUBLE_kind:
    case PC_kind:
    case FAR_PC_kind:
    case INSTR_kind:
    case FAR_INSTR_kind:
    case MEM_INSTR_kind:
#    if defined(X64) || defined(ARM)
    case REL_ADDR_kind:
#        ifdef X64
    case ABS_ADDR_kind:
#        endif
#    endif
        return false;

    case REG_kind:
        if (old_reg == opnd_get_reg(*opnd)) {
            *opnd = OPND_CREATE_INT32(val);
            return true;
        }
        return false;

    case BASE_DISP_kind: {
        byte size = opnd_get_size(*opnd);
        if (old_reg == opnd_get_base(*opnd)) {
            int b = REG_NULL;
            int i = opnd_get_index(*opnd);
            int s = opnd_get_scale(*opnd);
            int d = opnd_get_disp(*opnd) + val;
            *opnd = opnd_create_base_disp(b, i, s, d, size);
            return true;
        } else if (old_reg == opnd_get_index(*opnd)) {
            int b = opnd_get_base(*opnd);
            int i = REG_NULL;
            int s = 0;
            int d = opnd_get_disp(*opnd) + val * opnd_get_scale(*opnd);
            *opnd = opnd_create_base_disp(b, i, s, d, size);
            return true;
        }
    }
        return false;

    case FAR_BASE_DISP_kind: {
        byte size = opnd_get_size(*opnd);
        int seg = opnd_get_segment(*opnd);
        if (old_reg == opnd_get_base(*opnd)) {
            int b = REG_NULL;
            int i = opnd_get_index(*opnd);
            int s = opnd_get_scale(*opnd);
            int d = opnd_get_disp(*opnd) + val;
            *opnd = opnd_create_far_base_disp(seg, b, i, s, d, size);
            return true;
        } else if (old_reg == opnd_get_index(*opnd)) {
            int b = opnd_get_base(*opnd);
            int i = REG_NULL;
            int s = 0;
            int d = opnd_get_disp(*opnd) + val * opnd_get_scale(*opnd);
            *opnd = opnd_create_far_base_disp(seg, b, i, s, d, size);

            return true;
        }
    }
        return false;

    default: ASSERT_NOT_REACHED(); return false;
    }
}

void
replace_self_loop_with_opnd(dcontext_t *dcontext, app_pc tag, instrlist_t *trace,
                            opnd_t desiredtarget)
{
    instr_t *top = instrlist_first(trace), *in;
    opnd_t targetop;

    in = top;

    LOG(THREAD, LOG_OPTS, 3,
        "entering replace_self_loop_with_opnd looking for tag " PFX ".\n", tag);

    while (in != NULL) {
#    ifdef DEBUG
        d_r_loginst(dcontext, 3, in, "examining me in replace self loop");
        LOG(THREAD, LOG_OPTS, 3, "my bytes are: " PFX "\n", in->bytes);
#    endif
        if (instr_is_cbr(in) || instr_is_ubr(in)) {
            targetop = instr_get_target(in);
            if (opnd_is_near_pc(targetop) && opnd_get_pc(targetop) == tag) {
                d_r_loginst(dcontext, 3, in, "self_loop (pc target==tag) fixing in");
                instr_set_target(in, desiredtarget);
            } else if (opnd_is_near_instr(targetop) && opnd_get_instr(targetop) == top) {
                d_r_loginst(dcontext, 3, in, "self_loop (inter traget==top)fixing in");
                d_r_logopnd(dcontext, 3, desiredtarget, "self_loop in now points to");
                instr_set_target(in, desiredtarget);
            }
        }
        in = instr_get_next(in);
    }
}

/*
  copies replacee into in. frees replacee (with heap_free, not
  instr_free), so that in uses replacee's operands */

/* this function tries to change the instr into a mov_immed */
void
instr_arithmatic_simplify(dcontext_t *dcontext, instr_t *in)
{
    opnd_t op1, op2, dst;
    instr_t *newinstr = NULL;
    int opcode;
    int newvalue;
    d_r_loginst(dcontext, 3, in, "arithmatic simplify called on");

    opcode = instr_get_opcode(in);
    if (instr_flag_write_necessary(dcontext, in))
        return;

    /* first check the single source instructions to make encodable and optimize */
    if ((instr_num_srcs(in) == 1) && opnd_is_immed_int(op1 = instr_get_src(in, 0))) {
        opnd_t dst;
        int val = (int)opnd_get_immed_int(op1);
        dst = instr_get_dst(in, 0);
        switch (opcode) {
        case OP_mov_ld:
            newinstr = INSTR_CREATE_mov_imm(dcontext, instr_get_dst(in, 0), op1);
            /*
                    is this fucking necessary?
                    if (!instr_is_encoding_possible(newinstr)) {
                    instr_free(dcontext,newinstr);
                    newinstr=INSTR_CREATE_mov_st(dcontext,instr_get_dst(in,0),op1);
                    }
            */
            break;
            /* untested by josh
        case OP_fild:
        case OP_fld:
            if (val==0 && opnd_is_reg(dst)&& (opnd_get_reg(dst)==REG_ST0)) {
                d_r_loginst(dcontext,3,in,
                            "arithmatic simplify: making fload of 0 to store!");
                newinstr=INSTR_CREATE_fldz(dcontext); break;
            }
            */
        case OP_inc:
            d_r_loginst(dcontext, 3, in, "arithmatic simplify: making inc to store!");
            newinstr = INSTR_CREATE_mov_imm(dcontext, instr_get_dst(in, 0),
                                            OPND_CREATE_INT32(val + 1));
            break;
        case OP_dec:
            d_r_loginst(dcontext, 3, in, "arithmatic simplify: making dec to store!");
            newinstr = INSTR_CREATE_mov_imm(dcontext, instr_get_dst(in, 0),
                                            OPND_CREATE_INT32(val - 1));
            break;

        case OP_push: newinstr = INSTR_CREATE_push_imm(dcontext, op1); break;
        }
    } else if (instr_num_srcs(in) == 2) {
        op1 = instr_get_src(in, 0);
        op2 = instr_get_src(in, 1);

        if (opnd_is_immed_int(op1) && opnd_is_immed_int(op2)) {
            /* only if both operands are immediates... */
            int val1, val2;
            val1 = (int)opnd_get_immed_int(op1);
            val2 = (int)opnd_get_immed_int(op2);

            dst = instr_get_dst(in, 0);
            switch (opcode) {
            case OP_sar:
                newvalue = val2 >> val1; // why is this order weird
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                break;
                // fix me: don't optimize away the carry trick
            case OP_add:
                newvalue = val2 + val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_ADD!");
                break;
            case OP_imul:
                newvalue = val2 * val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_imul!");
                break;
            case OP_mul:
                newvalue = (unsigned)val2 * (unsigned)val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_mul!");
                break;
            case OP_and:
                newvalue = val2 & val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_and!");
                break;
            case OP_or:
                newvalue = val2 | val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_or!");
                break;
            case OP_xor:
                newvalue = val2 ^ val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_xor!");
                break;
            case OP_sub:
                newvalue = val2 - val1;
                newinstr =
                    INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(newvalue));
                d_r_loginst(dcontext, 3, in, "arithmatic simplify: OP_sub!");
                break;
            }
        } else if (opnd_is_immed_int(op1) || opnd_is_immed_int(op2)) {
            /* only gets here if exactly one operand is an immediate */
            int cons;
            opnd_t other;
            if (opnd_is_immed_int(op1)) {
                cons = (int)opnd_get_immed_int(op1);
                other = op2;
            } else {
                cons = (int)opnd_get_immed_int(op2);
                other = op1;
            }
            dst = instr_get_dst(in, 0);
            switch (opcode) {
            case OP_and:
                if (cons == 0xffffffff) {
                    newinstr = INSTR_CREATE_nop(dcontext);
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_and to OP_nop");
                } else if (cons == 0) {

                    if (opnd_is_reg(dst)) {
                        newinstr = INSTR_CREATE_xor(dcontext, dst, dst);
                        d_r_loginst(dcontext, 3, in,
                                    "arithmatic simplify: turned OP_and to zeroing xor");
                    } else {
                        ASSERT(opnd_is_memory_reference(dst));
                        newinstr =
                            INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(0));
                        d_r_loginst(
                            dcontext, 3, in,
                            "arithmatic simplify: turned OP_and to zeroing mov_imm");
                    }
                }
                break;

            case OP_or:
                if (cons == 0xffffffff) {
                    newinstr = INSTR_CREATE_mov_imm(dcontext, dst,
                                                    OPND_CREATE_INT32(0xffffffff));
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_or into mov 0xffffffff");
                } else if (cons == 0) {
                    newinstr = INSTR_CREATE_nop(dcontext);
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_or into nop");
                }
                break;
            case OP_add:
                if (cons == 0) {
                    newinstr = INSTR_CREATE_nop(dcontext);
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_add into nop");
                }
                break;
            case OP_sub:
                /* fixme
                if (cons==0) {
                    newinstr=INSTR_CREATE_nop(dcontext);
                    d_r_loginst(dcontext,3,in,"arithmatic simplify: turned OP_sub into
                nop");
                }
                */
                break;
            case OP_mul:
            case OP_imul:
                if (cons == 1) {
                    newinstr = INSTR_CREATE_nop(dcontext);
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_add into nop");
                } else if (cons == 0) {
                    newinstr = INSTR_CREATE_mov_imm(dcontext, dst, OPND_CREATE_INT32(0));
                    d_r_loginst(dcontext, 3, in,
                                "arithmatic simplify: turned OP_mul with 0 to mov_immed");
                }
                break;
            }
        }
    }

    if (newinstr) {
        d_r_loginst(dcontext, 3, newinstr, "with me");

        ASSERT(instr_is_encoding_possible(newinstr));

        instr_replace_inplace(dcontext, in, newinstr);
    }
}

void
instr_replace_inplace(dcontext_t *dcontext, instr_t *in, instr_t *replacee)
{
    instr_t *next, *prev;
    next = instr_get_next(in);
    prev = instr_get_prev(in);

    instr_reset(dcontext, in);
    memcpy(in, replacee, sizeof(instr_t));

    instr_set_next(in, next);
    instr_set_prev(in, prev);
    heap_free(dcontext, replacee, sizeof(instr_t) HEAPACCT(ACCT_INSTR));
}

void
instrlist_remove_nops(dcontext_t *dcontext, instrlist_t *trace)
{
    instr_t *in, *next, *walker;
    in = instrlist_first(trace);

    while (in) {
        next = instr_get_next(in);

        if (instr_get_opcode(in) == OP_nop) {

            // fix any instr targets that point to this instr
            for (walker = instrlist_first(trace); walker; walker = instr_get_next(walker))
                if (instr_is_cbr(walker) || instr_is_ubr(walker)) {
                    opnd_t targetop = instr_get_target(walker);
                    if (opnd_is_near_instr(targetop) && (in == opnd_get_instr(targetop)))
                        instr_set_target(walker, opnd_create_instr(instr_get_next(in)));
                }

            instrlist_remove(trace, in);
            instr_destroy(dcontext, in);
        }
        in = next;
    }
}

/*
  checks if the instr list depends on the value of reg
  but does not check if any instr lists that it calls depend on the value.
*/

bool
instrlist_depends_on_reg(dcontext_t *dcontext, instrlist_t *trace, int reg)
{
    instr_t *in;

    for (in = instrlist_first(trace); in; in = instr_get_next(in)) {

        if (instr_reg_in_src(in, reg))
            return true;

        if (instr_reg_in_dst(in, reg)) {
            if (instr_writes_to_reg(in, reg))
                return false;
            else
                return true; // register is read indirect through mem access
        }
    }

    return true;
}

void
instr_add_to_exitexec_list(dcontext_t *dcontext, instr_t *in, instr_t *exitinstr)
{
    ASSERT(instr_is_cti(in));

    if (!in->exitlist)
        in->exitlist = instrlist_create(dcontext);

    instrlist_append(in->exitlist, exitinstr);
    d_r_loginst(dcontext, 3, exitinstr, "adding this to exit list");
    LOG(THREAD, LOG_OPTS, 3, "exitinstr=" PFX "\n", exitinstr);
}
void
instrlist_setup_pseudo_exitstubs(dcontext_t *dcontext, instrlist_t *trace)
{
    instrlist_t *exitlist;
    instr_t *instr, *newtarget;

    exitlist = instrlist_create(dcontext);

    for (instr = instrlist_first(trace); instr != NULL; instr = instr_get_next(instr)) {
        if (instr->exitlist) {
            ASSERT(instr_is_cti(instr));
            instrlist_append(instr->exitlist,
                             INSTR_CREATE_jmp(dcontext, instr_get_target(instr)));
            d_r_loginst(dcontext, 3, instr, "setting up pseudo exit stub for me");

            newtarget = instrlist_first(instr->exitlist);
            instrlist_append_instrlist(dcontext, exitlist, instr->exitlist);

            instr_set_target(instr, opnd_create_instr(newtarget));

            ASSERT(instr_is_encoding_possible(instr));

            instr->exitlist = NULL;
#    ifdef DEBUG
            if (d_r_stats->loglevel >= 3) {
                d_r_loginst(dcontext, 3, instr, "after setting");
                instrlist_disassemble(dcontext, 0, trace, THREAD);
                instrlist_disassemble(dcontext, 0, exitlist, THREAD);
            }
#    endif
        }
    }

    instrlist_append_instrlist(dcontext, trace, exitlist);
}

#endif /* LOAD_TO_CONST */
