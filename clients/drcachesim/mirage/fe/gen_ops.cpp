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