#ifndef __MIR_OPND_H_
#define __MIR_OPND_H_

#include "dr_api.h"

enum mir_opnd_type_t {
    MIR_OPND_REG,
    MIR_OPND_IMM,
};

// FIXME: inefficient alignment
struct mir_opnd_t {
    mir_opnd_type_t type;
    union {
        reg_id_t reg;
        int64_t imm;
    } value;
};

bool
mir_opnd_is_reg(const mir_opnd_t *opnd);

bool
mir_opnd_is_imm(const mir_opnd_t *opnd);

#endif // __MIR_OPND_H_