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
    // assert(instr_num_dsts(instr) == 1);
    assert(opnd_get_index(instr_get_src(instr, 1)) == 0); // src1 in NULL

    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    opnd_t dst0 = instr_get_dst(instr, 0);
    opnd_t dst1 = instr_get_dst(instr, 1);

    printf("num_srcs: %d\n", instr_num_srcs(instr));
    printf("src0 type: %s\n", get_opnd_type(src0));
    printf("src0 index: %d\n", opnd_get_index(src0));
    printf("src0 name: %s\n", get_register_name(opnd_get_index(src0)));

    printf("src1 type: %s\n", get_opnd_type(src1));
    printf("src1 index: %d\n", opnd_get_index(src1));
    printf("src1 name: %s\n", get_register_name(opnd_get_index(src1)));

    printf("num dsts: %d\n", instr_num_dsts(instr));
    printf("dst0 type: %s\n", get_opnd_type(dst0));
    printf("dst0 index: %d\n", opnd_get_index(dst0));
    printf("dst0 name: %s\n", get_register_name(opnd_get_index(dst0)));

    printf("dst1 type: %s\n", get_opnd_type(dst1));
    printf("dst1 index: %d\n", opnd_get_index(dst1));
    printf("dst1 name: %s\n", get_register_name(opnd_get_index(dst1)));
    printf("dst1 offset: %d\n", opnd_get_disp(dst1));

    // assert(false);
    // opnd_t src0 = instr_get_src(instr, 0);

    // opnd_size_t size = opnd_get_size(src0);

    // mir_insn_t* sp_sub_insn = mir_insn_malloc(MIR_OP_SUB);
    // // [sp_sub_insn]
    // mir_insn_push_back(mir_insns_list, sp_sub_insn);
    // mir_insn_malloc_src0_imm(sp_sub_insn, size);
    // mir_opnd_t* src1_reg = mir_insn_malloc_src1_reg(sp_sub_insn, REG_XSP);
    // mir_insn_set_dst(sp_sub_insn, src1_reg);

    // mir_insn_t* store_insn = mir_insn_malloc(MIR_OP_ST32); // FIXME: size
    // // [sp_sub_insn, store_insn]
    // mir_insn_insert_after(store_insn, sp_sub_insn);
    // src2_set_opnd_by_type(src0, store_insn, mir_insns_list, ctx);
    // mir_insn_malloc_src0_imm(store_insn, 0);
    // mir_insn_malloc_src1_reg(store_insn, REG_XSP);
}