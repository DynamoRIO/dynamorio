#include "gen_ops.h"

// Do nothing for unsupported instructions
void gen_nop_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
}
   
void _gen_arith_op(instr_t *instr, mir_opc_t op, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 1);

    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);

    mir_insn_t* core_insn = mir_insn_malloc(op);
    mir_insn_push_front(mir_insns_list, core_insn);

    src0_set_opnd_by_type(src0, core_insn, mir_insns_list, ctx);
    src1_set_opnd_by_type(src1, core_insn, mir_insns_list, ctx);
    dst_set_opnd_by_type(dst, core_insn, mir_insns_list, ctx);
}

void gen_add_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_ADD, mir_insns_list, ctx);
}

void gen_sub_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_SUB, mir_insns_list, ctx);
}

void gen_or_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_OR, mir_insns_list, ctx);
}

void gen_and_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_AND, mir_insns_list, ctx);
}

void gen_xor_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_XOR, mir_insns_list, ctx);
}

void gen_shl_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_SHL, mir_insns_list, ctx);
}

void gen_shr_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    _gen_arith_op(instr, MIR_OP_SHR, mir_insns_list, ctx);
}

void gen_mov_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 1);
    assert(instr_num_dsts(instr) == 1);

    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t dst0 = instr_get_dst(instr, 0);

    mir_insn_t* core_insn = mir_insn_malloc(MIR_OP_MOV);
    mir_insn_push_front(mir_insns_list, core_insn);

    src0_set_opnd_by_type(src0, core_insn, mir_insns_list, ctx);
    mir_insn_set_src1_imm(core_insn, 0);
    dst_set_opnd_by_type(dst0, core_insn, mir_insns_list, ctx);
}

void gen_lea_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 1);
    assert(instr_num_dsts(instr) == 1);

    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t dst0 = instr_get_dst(instr, 0);
    assert(opnd_is_reg(dst0));

    // TODO: make this a helper function if more insns needs this common pattern
    if (opnd_is_abs_addr(src0) || opnd_is_rel_addr(src0)) {
        mir_insn_t* core_insn = mir_insn_malloc(MIR_OP_MOV);
        mir_insn_push_front(mir_insns_list, core_insn);
        uint64_t addr = (uint64_t)opnd_get_addr(src0);
        mir_insn_set_src0_reg(core_insn, DR_REG_NULL);
        mir_insn_set_src1_imm(core_insn, addr);
        mir_insn_set_dst_reg(core_insn, opnd_get_reg(dst0));
    }
    else if (opnd_is_base_disp(src0)) {
        // ADD base, disp -> dst
        reg_id_t base = opnd_get_base(src0);
        int32_t disp = opnd_get_disp(src0);
        mir_insn_t* add_insn = mir_insn_malloc(MIR_OP_ADD);
        mir_insn_push_back(mir_insns_list, add_insn);
        mir_insn_set_src0_reg(add_insn, base);
        mir_insn_set_src1_imm(add_insn, disp);
        mir_insn_set_dst_reg(add_insn, opnd_get_reg(dst0));
    }
    else {
        printf("unsupported opnd type\n");
        assert(false);
    }
}

// FIXME: performance optimize the assertions, or potentialy remove them
// push -> [sp_sub_insn, store_insn]
void gen_push_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 2);
    // assertion: push reg must be expressed as
    // push {reg/mem}, sp -> sp, [sp, -size] in dr format
    opnd_t src0 = instr_get_src(instr, 0);
    assert(opnd_is_reg(src0) || opnd_is_memory_reference(src0));
    assert(opnd_get_reg(instr_get_src(instr, 1)) == REG_XSP);
    assert(opnd_get_reg(instr_get_dst(instr, 0)) == REG_XSP);
    assert(opnd_get_base(instr_get_dst(instr, 1)) == REG_XSP);
    // setting up source operand
    uint size = opnd_size_in_bytes(opnd_get_size(src0));
    assert(opnd_get_disp(instr_get_dst(instr, 1)) == (int)(-size));
    // SUB sp, sp, size
    mir_insn_t* sp_sub_insn = mir_insn_malloc(MIR_OP_SUB);
    mir_insn_push_back(mir_insns_list, sp_sub_insn);
    mir_insn_set_src0_imm(sp_sub_insn, size);
    mir_insn_set_src1_reg(sp_sub_insn, REG_XSP);
    mir_insn_set_dst_reg(sp_sub_insn, REG_XSP);
    // setting up opcode by size
    mir_opc_t store_opc;
    switch (size) {
        case 1: store_opc = MIR_OP_ST8; break;
        case 2: store_opc = MIR_OP_ST16; break;
        case 4: store_opc = MIR_OP_ST32; break;
        case 8: store_opc = MIR_OP_ST64; break;
        default: assert(false);
    }
    // ST src0, [sp, size]
    mir_insn_t* store_insn = mir_insn_malloc(store_opc);
    mir_insn_push_back(mir_insns_list, store_insn);
    src2_set_opnd_by_type(src0, store_insn, mir_insns_list, ctx);
    mir_insn_set_src0_imm(store_insn, 0);
    mir_insn_set_src1_reg(store_insn, REG_XSP);
}

// pop -> [load_insn, sp_add_insn]
void gen_pop_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 2);
    // assertion: pop reg must be expressed as
    // pop [sp, size], sp -> sp, {reg/mem} in dr format
    opnd_t dst0 = instr_get_dst(instr, 0);
    assert(opnd_is_reg(dst0) || opnd_is_memory_reference(dst0));
    assert(opnd_get_reg(instr_get_src(instr, 0)) == REG_XSP);
    assert(opnd_get_base(instr_get_src(instr, 1)) == REG_XSP);
    assert(opnd_get_reg(instr_get_dst(instr, 1)) == REG_XSP);
    // setting up destination operand
    uint size = opnd_size_in_bytes(opnd_get_size(dst0));
    assert(opnd_get_disp(instr_get_src(instr, 1)) == 0);
    // setting up opcode by size
    mir_opc_t load_opc;
    switch (size) {
        case 1: load_opc = MIR_OP_LD8; break;
        case 2: load_opc = MIR_OP_LD16; break;
        case 4: load_opc = MIR_OP_LD32; break;
        case 8: load_opc = MIR_OP_LD64; break;
        default: assert(false);
    }
    // LD [sp, size], dst0
    mir_insn_t* load_insn = mir_insn_malloc(load_opc);
    mir_insn_push_back(mir_insns_list, load_insn);
    mir_insn_set_src0_imm(load_insn, 0);
    mir_insn_set_src1_reg(load_insn, REG_XSP);
    dst_set_opnd_by_type(dst0, load_insn, mir_insns_list, ctx);
    // ADD sp, sp, size
    mir_insn_t* sp_add_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_push_back(mir_insns_list, sp_add_insn);
    mir_insn_set_src0_imm(sp_add_insn, size);
    mir_insn_set_src1_reg(sp_add_insn, REG_XSP);
    mir_insn_set_dst_reg(sp_add_insn, REG_XSP);
}

// call -> [sp_sub_insn, store_insn, jmp_insn]
void gen_call_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 2);
    // assertion: call reg must be expressed as
    // call addr, sp -> sp, [sp, -size] in dr format
    opnd_t src0 = instr_get_src(instr, 0);
    // printf("src0 type: %s\n", get_opnd_type(src0));
    assert(opnd_is_pc(src0));
    assert(opnd_get_reg(instr_get_src(instr, 1)) == REG_XSP);
    assert(opnd_get_reg(instr_get_dst(instr, 0)) == REG_XSP);
    assert(opnd_get_base(instr_get_dst(instr, 1)) == REG_XSP);
    // setting up source operand
    // opnd_t src0 = instr_get_src(instr, 0);

    // FIXME: use sp to get the size of the registers, check if this assumption is correct
    opnd_t src1_sp = instr_get_src(instr, 1);
    uint size = opnd_size_in_bytes(opnd_get_size(src1_sp));
    // SUB sp, sp, size
    mir_insn_t* sp_sub_insn = mir_insn_malloc(MIR_OP_SUB);
    mir_insn_push_back(mir_insns_list, sp_sub_insn);
    mir_insn_set_src0_imm(sp_sub_insn, size);
    mir_insn_set_src1_reg(sp_sub_insn, REG_XSP);
    mir_insn_set_dst_reg(sp_sub_insn, REG_XSP);
    // ST src0, [sp, size]
    mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32);
    mir_insn_push_back(mir_insns_list, store_insn);
    app_pc call_addr = instr_get_app_pc(instr);
    // get the literal pointer value
    mir_insn_set_dst_imm(store_insn, (uint64_t)call_addr);
    mir_insn_set_src0_reg(store_insn, REG_XSP);
    mir_insn_set_src1_imm(store_insn, size);
    // JMP src0 - ommitted for now as it is not needed for the simulation
    mir_insn_t* jmp_insn = mir_insn_malloc(MIR_OP_JMP);
    mir_insn_push_back(mir_insns_list, jmp_insn);
    app_pc jmp_addr = opnd_get_pc(src0);
    mir_insn_set_src0_reg(jmp_insn, REG_NULL);
    mir_insn_set_src1_imm(jmp_insn, (uint64_t)jmp_addr);
    mir_insn_set_dst_reg(jmp_insn, REG_NULL);
}

void gen_test_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    // AND reg0, reg1 -> tmp0
    int tmp0 = alloc_tmp_reg(ctx);
    mir_insn_t* and_insn = mir_insn_malloc(MIR_OP_AND);
    mir_insn_push_back(mir_insns_list, and_insn);
    src0_set_opnd_by_type(src0, and_insn, mir_insns_list, ctx);
    src1_set_opnd_by_type(src1, and_insn, mir_insns_list, ctx);
    mir_insn_set_dst_reg(and_insn, tmp0);
    // W_FLAG tmp0
    mir_insn_t* w_flag_insn = mir_insn_malloc(MIR_OP_W_FLAG);
    mir_insn_push_back(mir_insns_list, w_flag_insn);
    mir_insn_set_src0_reg(w_flag_insn, tmp0);
    mir_insn_set_src1_reg(w_flag_insn, REG_NULL);
    mir_insn_set_dst_reg(w_flag_insn, REG_NULL);
}

void gen_cmp_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    // SUB reg0, reg1 -> tmp0
    int tmp0 = alloc_tmp_reg(ctx);
    mir_insn_t* sub_insn = mir_insn_malloc(MIR_OP_SUB);
    mir_insn_push_back(mir_insns_list, sub_insn);
    src0_set_opnd_by_type(src0, sub_insn, mir_insns_list, ctx);
    src1_set_opnd_by_type(src1, sub_insn, mir_insns_list, ctx);
    mir_insn_set_dst_reg(sub_insn, tmp0);
    // W_FLAG tmp0
    mir_insn_t* w_flag_insn = mir_insn_malloc(MIR_OP_W_FLAG);
    mir_insn_push_back(mir_insns_list, w_flag_insn);
    mir_insn_set_src0_reg(w_flag_insn, tmp0);
    mir_insn_set_src1_reg(w_flag_insn, REG_NULL);
    mir_insn_set_dst_reg(w_flag_insn, REG_NULL);
}

void gen_adc_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    uint r_eflags = instr_get_eflags(instr, DR_QUERY_INCLUDE_COND_SRCS);
    printf("adc r_eflags: %x\n", r_eflags);
    uint w_eflags = instr_get_eflags(instr, DR_QUERY_INCLUDE_COND_DSTS);
    printf("adc w_eflags: %x\n", w_eflags);
}

void gen_jump_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 1);
    assert(instr_num_dsts(instr) == 0);
    opnd_t src0 = instr_get_src(instr, 0);
    assert(opnd_is_pc(src0));
    // JMP src0
    mir_insn_t* jmp_insn = mir_insn_malloc(MIR_OP_JMP);
    mir_insn_push_back(mir_insns_list, jmp_insn);
    mir_insn_set_src0_reg(jmp_insn, REG_NULL);
    mir_insn_set_src1_imm(jmp_insn, (uint64_t)opnd_get_pc(src0));
    mir_insn_set_dst_reg(jmp_insn, REG_NULL);
}