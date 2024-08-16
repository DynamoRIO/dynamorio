#include "gen_ops.h"

void gen_add_op(instr_t *instr) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 1);
    assert(opnd_is_reg(instr_get_src(instr, 0)));
    assert(opnd_is_reg(instr_get_dst(instr, 0)));
    if (opnd_is_reg(instr_get_src(instr, 1))) {
        // add reg, reg
        reg_id_t opnd0 = opnd_get_reg(instr_get_src(instr, 0));
        reg_id_t opnd1 = opnd_get_reg(instr_get_src(instr, 1));
        printf("add %d, %d\n", opnd0, opnd1);
    } else if (opnd_is_immed(instr_get_src(instr, 1)))
    {
        // add reg, imm
        reg_id_t opnd0 = opnd_get_reg(instr_get_src(instr, 0));
        int64 opnd1 = opnd_get_immed_int(instr_get_src(instr, 1));
        printf("add %d, %ld\n", opnd0, opnd1);
    } else {
        printf("add opnd1 is not reg or imm\n");
    }
    
}