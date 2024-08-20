#include "gen_ops.h"

const char* get_opnd_type(opnd_t opnd) {
    if (opnd_is_reg(opnd)) {
        return "reg";
    } 
    if (opnd_is_immed(opnd)) {
        return "imm";
    }
    if (opnd_is_base_disp(opnd)) {
        return "base_disp";
    }
    if (opnd_is_pc(opnd)) {
        return "pc";
    }
    if (opnd_is_instr(opnd)) {
        return "instr";
    }
    if (opnd_is_null(opnd)) {
        return "null";
    }
    if (opnd_is_near_rel_addr(opnd)) {
        return "near_rel_addr";
    }
    if (opnd_is_far_rel_addr(opnd)) {
        return "far_rel_addr";
    }
    return "unknown";
}

// BASE-DISPLACEMENT
// A helper function to generate a load instruction from a base-displacement src
void gen_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD32);
    mir_insn_set_src0_reg(load_insn, base);
    mir_insn_set_src1_imm(load_insn, disp);
    mir_insn_set_dst_reg(load_insn, 0); // FIXME: do reg allocation
    mir_insn_push_front(mir_insns_list, load_insn);
}

// A helper function to generate a store instruction to a base-displacement dst
void gen_store_to_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32);
    mir_insn_set_src0_reg(store_insn, base);
    mir_insn_set_src1_imm(store_insn, disp);
    mir_insn_set_dst_reg(store_insn, 0); // FIXME: do reg allocation
    mir_insn_push_front(mir_insns_list, store_insn);
}

// NEAR-RELATIVE ADDRESS
// A helper function to generate a load instruction from a near-relative to a pc src
void gen_load_from_near_rel_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    // TODO: implement this
    return;
}

// A helper function to generate a store instruction to a near-relative to a pc dst
void gen_store_to_near_rel_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    // TODO: implement this
    return;
}

// ABSOLUTE ADDRESS
// A helper function to generate a load instruction from a absolute address src
void gen_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    // TODO: implement this
    return;
}

// A helper function to generate a store instruction to a absolute address dst
void gen_store_to_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    // TODO: implement this
    return;
}

void src0_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_src0_reg(insn, opnd_get_reg(opnd));
    }
    else if (opnd_is_immed(opnd)) {
        mir_insn_set_src0_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        printf("encountered memory reference opnd src0 type: %s\n", get_opnd_type(opnd));
        if (opnd_is_abs_addr(opnd)) {
            gen_load_from_abs_addr(opnd, insn, mir_insns_list);
        }
        else if (opnd_is_near_rel_addr(opnd)) {
            gen_load_from_near_rel_addr(opnd, insn, mir_insns_list);
        }
        else if (opnd_is_base_disp(opnd)) {
            gen_load_from_base_disp(opnd, insn, mir_insns_list);
        }
        else {
            printf("unsupported memory reference opnd src0 type: %s\n", get_opnd_type(opnd));
        }
    }
    // else if (opnd_is_far_rel_addr(opnd)) {
    //     gen_load_from_far_rel_addr(opnd, insn, mir_insns_list);
    // }
    else {
        printf("unsupported opnd src0 type: %s\n", get_opnd_type(opnd));
    }
}

void src1_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_src1_reg(insn, opnd_get_reg(opnd));
    }  
    else if (opnd_is_immed(opnd)) {
        mir_insn_set_src1_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        printf("encountered memory reference opnd src1 type: %s\n", get_opnd_type(opnd));
    }
    else {
        printf("unsupported opnd src1 type: %s\n", get_opnd_type(opnd));
    }
}

void dst_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_dst_reg(insn, opnd_get_reg(opnd));
    } 
    else if (opnd_is_memory_reference(opnd)) {
        printf("encountered memory reference opnd dst type: %s\n", get_opnd_type(opnd));
    }
    else {
        printf("unsupported opnd dst type: %s\n", get_opnd_type(opnd));
    }
}

void gen_add_op(instr_t *instr, mir_insn_list_t *mir_insns_list) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 1);

    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);

    // create a new node
    mir_insn_t* core_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_push_front(mir_insns_list, core_insn);

    src0_set_opnd_by_type(src0, core_insn, mir_insns_list);
    src1_set_opnd_by_type(src1, core_insn, mir_insns_list);
    dst_set_opnd_by_type(dst, core_insn, mir_insns_list);
}