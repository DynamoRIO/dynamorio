#ifndef __TRANSLATE_CONTEXT_H__
#define __TRANSLATE_CONTEXT_H__

// This file contains the definition of the translation context
// used by the other files in the frontend.

#include "dr_api.h"
#include "bitmap.h"

struct translate_context_t {
    // The current instruction being translated.
    instr_t *curr_instr;
    struct bitmap_t *tmp_reg_map;
};

// register allocation context

#define MIR_TMP_REG_START 0x1000

enum tmp_reg_t {
    TMP_REG_0 = MIR_TMP_REG_START,
    TMP_REG_1 = MIR_TMP_REG_START + 1,
    TMP_REG_2 = MIR_TMP_REG_START + 2,
    TMP_REG_3 = MIR_TMP_REG_START + 3,
    TMP_REG_LAST = TMP_REG_3,
};

static const char *tmp_reg_names[] = {
    "t0", "t1", "t2", "t3"
};

#define NUM_TMP_REGS (TMP_REG_LAST - MIR_TMP_REG_START + 1)

struct translate_context_t *translate_context_create();
int alloc_tmp_reg(struct translate_context_t *ctx);

inline const char *get_tmp_register_name(int reg){
    return tmp_reg_names[reg - MIR_TMP_REG_START];
}

void ctx_set_curr_instr(struct translate_context_t *ctx, instr_t *instr);

#endif // __TRANSLATE_CONTEXT_H__