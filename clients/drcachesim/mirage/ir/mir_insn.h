#ifndef __MIR_INSN_H__
#define __MIR_INSN_H__

#include "dr_api.h"
#include "mir_opc.h"
#include "mir_opnd.h"

#include "list.h"

#include "assert.h"

struct mir_insn_t {
    mir_opc_t op;
    mir_opnd_t* opnd0;
    mir_opnd_t* opnd1;
    mir_opnd_t* dst;
    struct list_elem elem;
};


mir_insn_t* mir_insn_malloc(mir_opc_t op);
void mir_insn_free(mir_insn_t *insn);

// malloc a new opnd
void mir_insn_malloc_src0_reg(mir_insn_t *insn, reg_id_t reg);
void mir_insn_malloc_src0_imm(mir_insn_t *insn, int64_t imm);
void mir_insn_malloc_src1_reg(mir_insn_t *insn, reg_id_t reg);
void mir_insn_malloc_src1_imm(mir_insn_t *insn, int64_t imm);
mir_opnd_t* mir_insn_malloc_dst_reg(mir_insn_t *insn, reg_id_t reg);

void mir_insn_set_op(mir_insn_t *insn, mir_opc_t op);

// set to an existing opnd
void mir_insn_set_src0(mir_insn_t *insn, mir_opnd_t *opnd);
void mir_insn_set_src1(mir_insn_t *insn, mir_opnd_t *opnd);
void mir_insn_set_dst(mir_insn_t *insn, mir_opnd_t *opnd);

// LIST of MIR
typedef struct list mir_insn_list_t;

void init_mir_insn_list(mir_insn_list_t* list);
void mir_insn_push_front(mir_insn_list_t* list, mir_insn_t* insn);
void mir_insn_push_back(mir_insn_list_t* list, mir_insn_t* insn);
void print_mir_insn_list(mir_insn_list_t* list);

#endif // __MIR_INSN_H__