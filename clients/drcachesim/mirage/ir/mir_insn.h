#ifndef __MIR_INSN_H__
#define __MIR_INSN_H__

#include "dr_api.h"
#include "mir_opc.h"

struct mir_insn {
    mir_opc_t op;
    mir_opnd_t opnd0;
    mir_opnd_t opnd1;
    mir_opnd_t dst;
};

mir_insn_t*
mir_insn_init(void);

void set_src0_reg(mir_insn_t *insn, reg_id_t reg);
void set_src0_imm(mir_insn_t *insn, imm_t imm);
void set_src1_reg(mir_insn_t *insn, reg_id_t reg);
void set_src1_imm(mir_insn_t *insn, imm_t imm);
void set_dst_reg(mir_insn_t *insn, reg_id_t reg);

void 
gen_load_from_basedisp(mir_insn_t *insn, opnd_t opnd);

void 
gen_store_to_basedisp(mir_insn_t *insn, opnd_t opnd);

#endif // __MIR_INSN_H__