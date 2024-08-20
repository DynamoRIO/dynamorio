#ifndef __GEN_OPS_H__
#define __GEN_OPS_H__

#include "dr_api.h"
#include "assert.h"

#include "mir_insn.h"
#include "mir_opnd.h"
#include "list.h"

#include "gen_opnd_api.h"

void gen_add_op(instr_t *instr, mir_insn_list_t *mir_insns_list);

#endif // __GEN_OPS_H__