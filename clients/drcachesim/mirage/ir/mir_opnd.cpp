#include "mir_opnd.h"

bool mir_opnd_is_reg(const mir_opnd_t *opnd) {
    return opnd.type == MIR_OPND_REG;
}

bool mir_opnd_is_imm(const mir_opnd_t *opnd) {
    return opnd.type == MIR_OPND_IMM;
}

mir_opnd_t mir_opnd_from_reg(reg_id_t reg) {
    mir_opnd_t opnd;
    opnd.type = MIR_OPND_REG;
    opnd.reg = reg;
    return opnd;
}

mir_opnd_t mir_opnd_from_imm(imm_t imm) {
    mir_opnd_t opnd;
    opnd.type = MIR_OPND_IMM;
    opnd.imm = imm;
    return opnd;
}