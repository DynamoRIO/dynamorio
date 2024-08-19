#include "mir_opnd.h"

mir_opnd_t* mir_opnd_malloc_reg(reg_id_t reg) {
    mir_opnd_t* opnd = (mir_opnd_t*)malloc(sizeof(mir_opnd_t));
    assert(opnd != NULL);
    opnd->type = MIR_OPND_REG;
    opnd->value.reg = reg;
    return opnd;
}

mir_opnd_t* mir_opnd_malloc_imm(int64_t imm) {
    mir_opnd_t* opnd = (mir_opnd_t*)malloc(sizeof(mir_opnd_t));
    assert(opnd != NULL);
    opnd->type = MIR_OPND_IMM;
    opnd->value.imm = imm;
    return opnd;
}

void mir_opnd_free(mir_opnd_t *opnd) {
    if (opnd != NULL) {
        free(opnd);
    }
}

const char* mir_opnd_to_str(mir_opnd_t* opnd) {
    static char buffer[256];
    if (opnd->type == MIR_OPND_REG) {
        snprintf(buffer, sizeof(buffer), "r%d", opnd->value.reg);
    } else if (opnd->type == MIR_OPND_IMM) {
        snprintf(buffer, sizeof(buffer), "i%ld", opnd->value.imm);
    }
    return buffer;
}

bool mir_opnd_is_reg(const mir_opnd_t *opnd) {
    return opnd->type == MIR_OPND_REG;
}

bool mir_opnd_is_imm(const mir_opnd_t *opnd) {
    return opnd->type == MIR_OPND_IMM;
}