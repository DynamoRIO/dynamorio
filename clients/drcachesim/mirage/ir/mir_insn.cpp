#include "mir_insn.h"

mir_insn_t* mir_insn_malloc(mir_opc_t op) {
    mir_insn_t* insn = (mir_insn_t*)malloc(sizeof(mir_insn_t));
    assert(insn != NULL);
    insn->op = op;
    return insn;
}

void mir_insn_free(mir_insn_t *insn) {
    if (insn != NULL) {
        mir_opnd_free(insn->opnd0);
        mir_opnd_free(insn->opnd1);
        mir_opnd_free(insn->dst);
        free(insn);
    }
}

const char* mir_insn_to_str(mir_insn_t* insn) {
    static char buffer[256];
    const char* op_str = mir_opc_to_str(insn->op);
    const char* opnd0_str = mir_opnd_to_str(insn->opnd0);
    const char* opnd1_str = mir_opnd_to_str(insn->opnd1);
    const char* dst_str = mir_opnd_to_str(insn->dst);

    snprintf(buffer, sizeof(buffer), "%s %s, %s -> %s", 
             op_str, opnd0_str, opnd1_str, dst_str);

    return buffer;
}

void mir_insn_malloc_src0_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd0 = mir_opnd_malloc_reg(reg);
}

void mir_insn_malloc_src0_imm(mir_insn_t *insn, int64_t imm) {
    insn->opnd0 = mir_opnd_malloc_imm(imm);
}

void mir_insn_malloc_src1_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd1 = mir_opnd_malloc_reg(reg);
}

void mir_insn_malloc_src1_imm(mir_insn_t *insn, int64_t imm) {
    insn->opnd1 = mir_opnd_malloc_imm(imm);
}

mir_opnd_t* mir_insn_malloc_dst_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->dst = mir_opnd_malloc_reg(reg);
    return insn->dst;
}

void mir_insn_set_src0(mir_insn_t *insn, mir_opnd_t *opnd) {
    insn->opnd0 = opnd;
}

void mir_insn_set_src1(mir_insn_t *insn, mir_opnd_t *opnd) {
    insn->opnd1 = opnd;
}

void mir_insn_set_dst(mir_insn_t *insn, mir_opnd_t *opnd) {
    insn->dst = opnd;
}

// LIST operations
void init_mir_insn_list(mir_insn_list_t* list) {
    list_init(list);
}

void mir_insn_push_front(mir_insn_list_t* list, mir_insn_t* insn) {
    list_push_front(list, &insn->elem);
}

void mir_insn_push_back(mir_insn_list_t* list, mir_insn_t* insn) {
    list_push_back(list, &insn->elem);
}

void print_mir_insn_list(mir_insn_list_t* list) {
    struct list_elem* e;
    for (e = list_begin(list); e != list_end(list); e = list_next(e)) {
        mir_insn_t* insn = list_entry(e, mir_insn_t, elem);
        printf("%s\n", mir_opc_to_str(insn->op));
    }
}