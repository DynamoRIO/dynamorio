#include "gen_ops.h"

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

void gen_push_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 2);
    // assertion: push reg must be expressed as
    // push {reg}, sp -> sp, [sp, -size] in dr format
    assert(opnd_get_reg(instr_get_src(instr, 1)) == REG_XSP);
    assert(opnd_get_reg(instr_get_dst(instr, 0)) == REG_XSP);
    assert(opnd_get_base(instr_get_dst(instr, 1)) == REG_XSP);

    opnd_t src0 = instr_get_src(instr, 0);
    uint size = opnd_size_in_bytes(opnd_get_size(src0));
    assert(opnd_get_disp(instr_get_dst(instr, 1)) == (int)(-size));

    mir_insn_t* sp_sub_insn = mir_insn_malloc(MIR_OP_SUB);
    // [sp_sub_insn]
    mir_insn_push_back(mir_insns_list, sp_sub_insn);
    mir_insn_malloc_src0_imm(sp_sub_insn, size);
    mir_opnd_t* src1_reg = mir_insn_malloc_src1_reg(sp_sub_insn, REG_XSP);
    mir_insn_set_dst(sp_sub_insn, src1_reg);

    mir_opc_t store_opc;
    switch (size) {
        case 1: store_opc = MIR_OP_ST8; break;
        case 2: store_opc = MIR_OP_ST16; break;
        case 4: store_opc = MIR_OP_ST32; break;
        case 8: store_opc = MIR_OP_ST64; break;
        default: assert(false);
    }

    // [sp_sub_insn, store_insn]
    mir_insn_t* store_insn = mir_insn_malloc(store_opc);
    mir_insn_push_back(mir_insns_list, store_insn);
    src2_set_opnd_by_type(src0, store_insn, mir_insns_list, ctx);
    mir_insn_malloc_src0_imm(store_insn, 0);
    mir_insn_malloc_src1_reg(store_insn, REG_XSP);
}

void gen_pop_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 2);
    // assertion: pop reg must be expressed as
    // pop [sp, size], sp -> sp, {reg} in dr format
    assert(opnd_get_reg(instr_get_src(instr, 0)) == REG_XSP);
    assert(opnd_get_base(instr_get_src(instr, 1)) == REG_XSP);
    assert(opnd_get_reg(instr_get_dst(instr, 1)) == REG_XSP);

    opnd_t dst0 = instr_get_dst(instr, 0);
    uint size = opnd_size_in_bytes(opnd_get_size(dst0));
    // printf("size: %d\n", size);
    // printf("opnd_get_disp(instr_get_src(instr, 1)): %d\n", opnd_get_disp(instr_get_src(instr, 1)));
    assert(opnd_get_disp(instr_get_src(instr, 1)) == 0);

    mir_opc_t load_opc;
    switch (size) {
        case 1: load_opc = MIR_OP_LD8; break;
        case 2: load_opc = MIR_OP_LD16; break;
        case 4: load_opc = MIR_OP_LD32; break;
        case 8: load_opc = MIR_OP_LD64; break;
        default: assert(false);
    }
    // [load_insn]
    mir_insn_t* load_insn = mir_insn_malloc(load_opc);
    mir_insn_push_back(mir_insns_list, load_insn);
    mir_insn_malloc_src0_imm(load_insn, 0);
    mir_insn_malloc_src1_reg(load_insn, REG_XSP);
    mir_insn_malloc_dst_reg(load_insn, opnd_get_reg(dst0));

    // [load_insn, sp_add_insn]
    mir_insn_t* sp_add_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_push_back(mir_insns_list, sp_add_insn);
    mir_insn_malloc_src0_imm(sp_add_insn, size);
    mir_opnd_t* src1_reg = mir_insn_malloc_src1_reg(sp_add_insn, REG_XSP);
    mir_insn_set_dst(sp_add_insn, src1_reg);
}