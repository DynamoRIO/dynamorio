#ifndef __GEN_OPS_H__
#define __GEN_OPS_H__

#include "dr_api.h"
#include "assert.h"

void gen_add_op(instr_t *instr);

void print_opnd_type(opnd_t opnd);

#endif // __GEN_OPS_H__