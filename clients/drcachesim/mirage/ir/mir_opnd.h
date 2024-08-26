#ifndef __MIR_OPND_H_
#define __MIR_OPND_H_

#include "dr_api.h"
#include "assert.h"
#include "translate_context.h"

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

#endif // __MIR_OPND_H_