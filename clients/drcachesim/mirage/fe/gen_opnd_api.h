#ifndef __GEN_OPND_API_H__
#define __GEN_OPND_API_H__

#include "dr_api.h"

#include "mir_insn.h"
#include "mir_opnd.h"
#include "list.h"

const char* get_opnd_type(opnd_t opnd);

void gen_src0_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_src1_from_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_dst_to_memref(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);

void src0_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void src1_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void dst_set_opnd_by_type(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);

// ABSOLUTE ADDRESS or RELATIVE ADDRESS
void gen_src0_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_src1_load_from_abs_addr(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_dst_store_to_abs_addr(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);

// BASE DISPLACEMENT
void gen_src0_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_src1_load_from_base_disp(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);
void gen_dst_store_to_base_disp(opnd_t opnd, mir_insn_t* insn, mir_insn_list_t* mir_insns_list, struct translate_context_t *ctx);

#endif // __GEN_OPND_API_H__