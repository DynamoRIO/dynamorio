#include "gen_opnd_api.h"

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

void gen_src0_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list) {
    if (opnd_is_abs_addr(opnd)) {
        gen_src0_load_from_abs_addr(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_near_rel_addr(opnd)) {
        gen_src0_load_from_near_rel(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_src0_load_from_base_disp(opnd, insn, mir_insns_list);
    } else {
        printf("unsupported memref opnd src0 type: %s\n", get_opnd_type(opnd));
    }
}

void gen_src1_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list) {
    if (opnd_is_abs_addr(opnd)) {
        gen_src1_load_from_abs_addr(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_near_rel_addr(opnd)) {
        gen_src1_load_from_near_rel(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_src1_load_from_base_disp(opnd, insn, mir_insns_list);
    }
}

void gen_dst_to_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list) {
    if (opnd_is_abs_addr(opnd)) {
        gen_dst_store_to_abs_addr(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_near_rel_addr(opnd)) {
        gen_dst_store_to_near_rel(opnd, insn, mir_insns_list);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_dst_store_to_base_disp(opnd, insn, mir_insns_list);
    }
}

// ABSOLUTE ADDRESS
void _gen_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, bool src_num) {
    void *addr = opnd_get_addr(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD32);
    mir_insn_malloc_src0_imm(load_insn, (int64_t)addr);
    mir_insn_malloc_src1_reg(load_insn, 0); // FIXME: do zero-reg special case
    mir_opnd_t* dst_tmp = mir_insn_malloc_dst_reg(load_insn, 0); // FIXME: do reg allocation
    if (src_num == 0) {
        mir_insn_set_src0(insn, dst_tmp);
    }
    else {
        mir_insn_set_src1(insn, dst_tmp);
    }
    mir_insn_push_front(mir_insns_list, load_insn);
}

// A helper function to generate a load instruction from a absolute address src
void gen_src0_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_abs_addr(opnd, insn, mir_insns_list, 0);
}

void gen_src1_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_abs_addr(opnd, insn, mir_insns_list, 1);
}

// A helper function to generate a store instruction to a absolute address dst
void gen_dst_store_to_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    void *addr = opnd_get_addr(opnd);
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32);
    mir_insn_malloc_src0_imm(store_insn, (int64_t)addr);
    mir_insn_malloc_src1_reg(store_insn, 0); // FIXME: do reg allocation
    // The tmp register is immediately stored and discarded, so we don't need to
    // save it for any future use. 
    mir_insn_malloc_dst_reg(store_insn, 0); // FIXME: do reg allocation
    mir_insn_push_front(mir_insns_list, store_insn);
    return;
}

// NEAR-RELATIVE ADDRESS
// A helper function to generate a load instruction from a near-relative to a pc src
void _gen_load_from_near_rel_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, bool src_num) {
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD32);
    mir_insn_malloc_src0_imm(load_insn, disp);
    mir_insn_malloc_src1_reg(load_insn, 0); // FIXME: do PC_REG special case since x86 EIP is not accessible
    mir_opnd_t* dst_tmp = mir_insn_malloc_dst_reg(load_insn, 0); // FIXME: do reg allocation
    if (src_num == 0) {
        mir_insn_set_src0(insn, dst_tmp);
    }
    else {
        mir_insn_set_src1(insn, dst_tmp);
    }
    mir_insn_push_front(mir_insns_list, load_insn);
    return;
}

void gen_src0_load_from_near_rel(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_near_rel_addr(opnd, insn, mir_insns_list, 0);
}

void gen_src1_load_from_near_rel(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_near_rel_addr(opnd, insn, mir_insns_list, 1);
}

// A helper function to generate a store instruction to a near-relative to a pc dst
void gen_dst_store_to_near_rel(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32);
    mir_insn_malloc_src0_imm(store_insn, disp);
    mir_insn_malloc_src1_reg(store_insn, 0); // FIXME: do PC_REG special case since x86 EIP is not accessible
    mir_insn_malloc_dst_reg(store_insn, 0); // FIXME: do reg allocation
    mir_insn_push_front(mir_insns_list, store_insn);
    return;
}

// BASE-DISPLACEMENT
// A helper function to generate a load instruction from a base-displacement src
void _gen_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, bool src_num) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD32);
    mir_insn_malloc_src0_reg(load_insn, base);
    mir_insn_malloc_src1_imm(load_insn, disp);
    mir_insn_malloc_dst_reg(load_insn, 0); // FIXME: do reg allocation
    mir_opnd_t* dst_tmp = mir_insn_malloc_dst_reg(load_insn, 0); // FIXME: do reg allocation
    if (src_num == 0) {
        mir_insn_set_src0(insn, dst_tmp);
    }
    else {
        mir_insn_set_src1(insn, dst_tmp);
    }
    mir_insn_push_front(mir_insns_list, load_insn);
}

void gen_src0_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_base_disp(opnd, insn, mir_insns_list, 0);
}

void gen_src1_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    _gen_load_from_base_disp(opnd, insn, mir_insns_list, 1);
}

// A helper function to generate a store instruction to a base-displacement dst
void gen_dst_store_to_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32);
    mir_insn_malloc_src0_reg(store_insn, base);
    mir_insn_malloc_src1_imm(store_insn, disp);
    mir_insn_malloc_dst_reg(store_insn, 0); // FIXME: do reg allocation
    mir_insn_push_front(mir_insns_list, store_insn);
}

void src0_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    if (opnd_is_reg(opnd)) {
        mir_insn_malloc_src0_reg(insn, opnd_get_reg(opnd));
    }
    else if (opnd_is_immed(opnd)) {
        mir_insn_malloc_src0_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        gen_src0_from_memref(opnd, insn, mir_insns_list);
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
        mir_insn_malloc_src1_reg(insn, opnd_get_reg(opnd));
    }  
    else if (opnd_is_immed(opnd)) {
        mir_insn_malloc_src1_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        gen_src1_from_memref(opnd, insn, mir_insns_list);
    }
    else {
        printf("unsupported opnd src1 type: %s\n", get_opnd_type(opnd));
    }
}

void dst_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list) {
    if (opnd_is_reg(opnd)) {
        mir_insn_malloc_dst_reg(insn, opnd_get_reg(opnd));
    } 
    else if (opnd_is_memory_reference(opnd)) {
        gen_dst_to_memref(opnd, insn, mir_insns_list);
    }
    else {
        printf("unsupported opnd dst type: %s\n", get_opnd_type(opnd));
    }
}