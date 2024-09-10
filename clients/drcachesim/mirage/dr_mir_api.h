#ifndef __DR_MIR_API_H_
#define __DR_MIR_API_H

#include "dr_api.h"

#include "gen_ops.h"
#include "list.h"

void
dr_gen_mir_ops(instr_t *instr, mir_insn_list_t *insn_list);

#endif // __DR_MIR_API_H_