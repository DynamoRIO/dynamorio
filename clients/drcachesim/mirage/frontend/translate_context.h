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
#define MIR_FLAG_REG_START 0x2000

// TMP REGISTERS
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

// FLAG REGISTERS
enum flag_reg_t {
    FLAG_REG_CF = MIR_FLAG_REG_START,
    FLAG_REG_PF = MIR_FLAG_REG_START + 1,
    FLAG_REG_AF = MIR_FLAG_REG_START + 2,
    FLAG_REG_ZF = MIR_FLAG_REG_START + 3,
    FLAG_REG_SF = MIR_FLAG_REG_START + 4,
    FLAG_REG_OF = MIR_FLAG_REG_START + 5,
    FLAG_REG_LAST = FLAG_REG_OF,
};

static const char *flag_reg_names[] = {
    "cf", "pf", "af", "zf", "sf", "of"
};

#define NUM_FLAG_REGS (FLAG_REG_LAST - MIR_FLAG_REG_START + 1)

struct translate_context_t *translate_context_create();
int alloc_tmp_reg(struct translate_context_t *ctx);

inline const char *get_tmp_register_name(int reg){
    return tmp_reg_names[reg - MIR_TMP_REG_START];
}

inline const char *get_flag_register_name(int reg){
    return flag_reg_names[reg - MIR_FLAG_REG_START];
}

void ctx_set_curr_instr(struct translate_context_t *ctx, instr_t *instr);

#endif // __TRANSLATE_CONTEXT_H__