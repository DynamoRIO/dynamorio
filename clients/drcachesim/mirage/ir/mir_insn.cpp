#include "mir_insn.h"

mir_insn_t* mir_insn_init(void) {
    mir_insn_t *insn = (mir_insn_t *)malloc(sizeof(mir_insn_t));
    if (insn == NULL) {
        return NULL;
    }
    insn->op = MIR_OP_NULL;
    insn->opnd0 = mir_opnd_from_imm(0);
    insn->opnd1 = mir_opnd_from_imm(0);
    insn->dst = mir_opnd_from_imm(0);
    return insn;
}

void set_src0_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd0 = mir_opnd_from_reg(reg);
}

void set_src0_imm(mir_insn_t *insn, imm_t imm) {
    insn->opnd0 = mir_opnd_from_imm(imm);
}

void set_src1_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd1 = mir_opnd_from_reg(reg);
}

void set_src1_imm(mir_insn_t *insn, imm_t imm) {
    insn->opnd1 = mir_opnd_from_imm(imm);
}

void set_dst_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->dst = mir_opnd_from_reg(reg);
}

void gen_load_from_basedisp(mir_insn_t *insn, opnd_t opnd) {
    assert(opnd.type == MIR_OPND_BASEDISP);
    reg_id_t base = opnd_get_base(opnd);
    int32_t disp = opnd_get_disp(opnd);
    insn->op = MIR_OP_LOAD;
    insn->opnd0 = mir_opnd_from_reg(base);
    insn->opnd1 = mir_opnd_from_imm(disp);
    insn->dst = mir_alloc_tmp(); //FIXME: unimplemented
}