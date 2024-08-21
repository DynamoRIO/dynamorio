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

mir_opnd_t* mir_opnd_malloc_reg(reg_id_t reg);
mir_opnd_t* mir_opnd_malloc_imm(int64_t imm);
void mir_opnd_free(mir_opnd_t *opnd);

const char* mir_opnd_to_str(mir_opnd_t* opnd);

bool
mir_opnd_is_reg(const mir_opnd_t *opnd);

bool
mir_opnd_is_imm(const mir_opnd_t *opnd);

#endif // __MIR_OPND_H_