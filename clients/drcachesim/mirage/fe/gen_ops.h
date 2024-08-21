#ifndef __GEN_OPS_H__
#define __GEN_OPS_H__

#include "dr_api.h"
#include "assert.h"

#include "mir_insn.h"
#include "mir_opnd.h"
#include "list.h"

#include "gen_opnd_api.h"

void gen_add_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);
void gen_sub_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);

void gen_or_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);
void gen_and_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);
void gen_xor_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);

void gen_push_op(instr_t *instr, mir_insn_list_t *mir_insns_list, struct translate_context_t *ctx);
#endif // __GEN_OPS_H__