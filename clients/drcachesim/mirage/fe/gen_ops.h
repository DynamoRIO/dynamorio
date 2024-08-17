#ifndef __GEN_OPS_H__
#define __GEN_OPS_H__

#include "dr_api.h"
#include "assert.h"

#include "list"

// a helper function to get the type of an operand
const char* get_opnd_type(opnd_t opnd);

void gen_add_op(instr_t *instr);

#endif // __GEN_OPS_H__