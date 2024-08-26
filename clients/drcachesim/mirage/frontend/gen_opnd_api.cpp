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

void gen_src0_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_abs_addr(opnd) || opnd_is_rel_addr(opnd)) {
        gen_src0_load_from_abs_addr(opnd, insn, mir_insns_list, ctx);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_src0_load_from_base_disp(opnd, insn, mir_insns_list, ctx);
    } else {
        printf("unsupported memref opnd src0 type: %s\n", get_opnd_type(opnd));
    }
}

void gen_src1_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_abs_addr(opnd) || opnd_is_rel_addr(opnd)) {
        gen_src1_load_from_abs_addr(opnd, insn, mir_insns_list, ctx);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_src1_load_from_base_disp(opnd, insn, mir_insns_list, ctx);
    }
}

void gen_src2_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_abs_addr(opnd) || opnd_is_rel_addr(opnd)) {
        gen_src2_load_from_abs_addr(opnd, insn, mir_insns_list, ctx);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_src2_load_from_base_disp(opnd, insn, mir_insns_list, ctx);
    }
}

void gen_dst_to_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_abs_addr(opnd) || opnd_is_rel_addr(opnd)) {
        gen_dst_store_to_abs_addr(opnd, insn, mir_insns_list, ctx);
    }
    else if (opnd_is_base_disp(opnd)) {
        gen_dst_store_to_base_disp(opnd, insn, mir_insns_list, ctx);
    }
}

// ABSOLUTE ADDRESS
void _gen_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, int src_num, struct translate_context_t *ctx) {
    void *addr = opnd_get_addr(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD64);
    mir_insn_set_src0_imm(load_insn, (int64_t)addr);
    mir_insn_set_src1_reg(load_insn, DR_REG_NULL);
    reg_id_t tmp_dst_reg = alloc_tmp_reg(ctx);
    mir_insn_set_dst_reg(load_insn, tmp_dst_reg);
    if (src_num == 0) {
        mir_insn_set_src0_reg(insn, tmp_dst_reg);
    }
    else if (src_num == 1) {
        mir_insn_set_src1_reg(insn, tmp_dst_reg);
    }
    else if (src_num == 2) {
        // Useful when insn is a store instruction, where dst is the src0
        mir_insn_set_dst_reg(insn, tmp_dst_reg);
    }
    mir_insn_insert_before(load_insn, insn);
}

// A helper function to generate a load instruction from a absolute address src
void gen_src0_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_abs_addr(opnd, insn, mir_insns_list, 0, ctx);
}

void gen_src1_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_abs_addr(opnd, insn, mir_insns_list, 1, ctx);
}

void gen_src2_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_abs_addr(opnd, insn, mir_insns_list, 2, ctx);
}

void gen_dst_store_to_abs_addr(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {

    void *addr = opnd_get_addr(opnd);
    // Patch the original instruction
    reg_id_t tmp_dst_reg = alloc_tmp_reg(ctx);
    mir_insn_set_dst_reg(insn, tmp_dst_reg);

    // Generate the store instruction
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST64);
    mir_insn_set_dst_reg(store_insn, tmp_dst_reg);  
    mir_insn_set_src0_reg(store_insn, DR_REG_NULL);
    mir_insn_set_src1_imm(store_insn, (int64_t)addr);
    mir_insn_insert_after(store_insn, insn);
    return;
}

// BASE-DISPLACEMENT
// A helper function to generate a load instruction from a base-displacement src
void _gen_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, int src_num, struct translate_context_t *ctx) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    mir_insn_t* load_insn = mir_insn_malloc(MIR_OP_LD64);
    mir_insn_set_src0_reg(load_insn, base);
    mir_insn_set_src1_imm(load_insn, disp);
    reg_id_t tmp_dst_reg = alloc_tmp_reg(ctx);
    mir_insn_set_dst_reg(load_insn, tmp_dst_reg);
    if (src_num == 0) {
        mir_insn_set_src0_reg(insn, tmp_dst_reg);
    }
    else if (src_num == 1) {
        mir_insn_set_src1_reg(insn, tmp_dst_reg);
    } else if (src_num == 2) {
        // Useful when insn is a store instruction, where dst is the src0
        mir_insn_set_dst_reg(insn, tmp_dst_reg);
    }
    mir_insn_insert_before(load_insn, insn);
}

void gen_src0_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_base_disp(opnd, insn, mir_insns_list, 0, ctx);
}

void gen_src1_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_base_disp(opnd, insn, mir_insns_list, 1, ctx);
}

// src2 is an alias used when insn is STORE, where dst is used as a source
void gen_src2_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_load_from_base_disp(opnd, insn, mir_insns_list, 2, ctx);
}

// A helper function to generate a store instruction to a base-displacement dst,
// the store instruction is inserted after the original instruction,
// and the original instruction is patched to use the tmp register as the dst
void gen_dst_store_to_base_disp(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);

    // Patch the original instruction
    reg_id_t tmp_dst_reg = alloc_tmp_reg(ctx);
    mir_insn_set_dst_reg(insn, tmp_dst_reg);

    // Generate the store instruction
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST64);
    mir_insn_set_dst_reg(store_insn, tmp_dst_reg);
    mir_insn_set_src0_reg(store_insn, base);
    mir_insn_set_src1_imm(store_insn, disp);
    mir_insn_insert_after(store_insn, insn);
    return;
}

void src0_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_src0_reg(insn, opnd_get_reg(opnd));
    }
    else if (opnd_is_immed(opnd)) {
        mir_insn_set_src0_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        gen_src0_from_memref(opnd, insn, mir_insns_list, ctx);
    }
    // else if (opnd_is_far_rel_addr(opnd)) {
    //     gen_load_from_far_rel_addr(opnd, insn, mir_insns_list);
    // }
    else {
        printf("unsupported opnd src0 type: %s\n", get_opnd_type(opnd));
    }
}

void src1_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_src1_reg(insn, opnd_get_reg(opnd));
    }  
    else if (opnd_is_immed(opnd)) {
        mir_insn_set_src1_imm(insn, opnd_get_immed_int(opnd));
    }
    else if (opnd_is_memory_reference(opnd)) {
        gen_src1_from_memref(opnd, insn, mir_insns_list, ctx);
    }
    else {
        printf("unsupported opnd src1 type: %s\n", get_opnd_type(opnd));
    }
}

// src2 is an alias used when insn is STORE, where dst is used as a source
void src2_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_dst_reg(insn, opnd_get_reg(opnd));
    } else if (opnd_is_memory_reference(opnd)) {
        gen_src2_from_memref(opnd, insn, mir_insns_list, ctx);
    } else {
        printf("unsupported opnd src2 type: %s\n", get_opnd_type(opnd));
    }
}

void dst_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, 
                        mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    if (opnd_is_reg(opnd)) {
        mir_insn_set_dst_reg(insn, opnd_get_reg(opnd));
    } 
    else if (opnd_is_memory_reference(opnd)) {
        gen_dst_to_memref(opnd, insn, mir_insns_list, ctx);
    }
    else {
        printf("unsupported opnd dst type: %s\n", get_opnd_type(opnd));
    }
}