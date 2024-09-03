#include "mir_insn.h"

mir_insn_t* mir_insn_malloc(mir_opc_t op) {
    mir_insn_t* insn = (mir_insn_t*)malloc(sizeof(mir_insn_t));
    assert(insn != NULL);
    insn->op = op;
    return insn;
}

void mir_insn_free(mir_insn_t *insn) {
    if (insn != NULL) {
        free(insn);
    }
}

const char* get_mir_opnd_name(reg_id_t reg) {
    if (reg >= MIR_FLAG_REG_START && reg < FLAG_REG_LAST) {
        return get_flag_register_name(reg);
    } else if (reg >= MIR_TMP_REG_START && reg < TMP_REG_LAST) {
        return get_tmp_register_name(reg);
    } else {
        return get_register_name(reg);
    }
    return NULL;
}

const char* mir_insn_to_str(mir_insn_t* insn) {
    static char buffer[256];
    const char* op_str = mir_opc_to_str(insn->op);

    static char opnd0_str[256];
    static char opnd1_str[256];
    static char dst_str[256];

    if (insn->opnd0.type == MIR_OPND_REG) {
        snprintf(opnd0_str, sizeof(opnd0_str), "R[%s](%d)", get_mir_opnd_name(insn->opnd0.value.reg), insn->opnd0.value.reg);
    } else if (insn->opnd0.type == MIR_OPND_IMM) {
        snprintf(opnd0_str, sizeof(opnd0_str), "I[%ld]", insn->opnd0.value.imm);
    }

    if (insn->opnd1.type == MIR_OPND_REG) {
        snprintf(opnd1_str, sizeof(opnd1_str), "R[%s](%d)", get_mir_opnd_name(insn->opnd1.value.reg), insn->opnd1.value.reg);
    } else if (insn->opnd1.type == MIR_OPND_IMM) {
        snprintf(opnd1_str, sizeof(opnd1_str), "I[%ld]", insn->opnd1.value.imm);
    }

    if (insn->dst.type == MIR_OPND_REG) {
        snprintf(dst_str, sizeof(dst_str), "R[%s](%d)", get_mir_opnd_name(insn->dst.value.reg), insn->dst.value.reg);
    } else if (insn->dst.type == MIR_OPND_IMM) {
        snprintf(dst_str, sizeof(dst_str), "I[%ld]", insn->dst.value.imm);
    }

    if (mir_opc_is_store(insn->op)) {
        snprintf(buffer, sizeof(buffer), "%s %s -> [%s + %s]", op_str, dst_str, opnd1_str, opnd0_str);
    } else if (mir_opc_is_load(insn->op)) {
        snprintf(buffer, sizeof(buffer), "%s [%s + %s] -> %s", op_str, opnd1_str, opnd0_str, dst_str);
    } else {
        snprintf(buffer, sizeof(buffer), "%s %s, %s -> %s", 
                 op_str, opnd0_str, opnd1_str, dst_str);
    }

    return buffer;
}

void mir_insn_set_src0_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd0.type = MIR_OPND_REG;
    insn->opnd0.value.reg = reg;
}

void mir_insn_set_src0_imm(mir_insn_t *insn, int64_t imm) {
    insn->opnd0.type = MIR_OPND_IMM;
    insn->opnd0.value.imm = imm;
}

void mir_insn_set_src1_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->opnd1.type = MIR_OPND_REG;
    insn->opnd1.value.reg = reg;
}

void mir_insn_set_src1_imm(mir_insn_t *insn, int64_t imm) {
    insn->opnd1.type = MIR_OPND_IMM;
    insn->opnd1.value.imm = imm;
}

void mir_insn_set_dst_reg(mir_insn_t *insn, reg_id_t reg) {
    insn->dst.type = MIR_OPND_REG;
    insn->dst.value.reg = reg;
}

void mir_insn_set_dst_imm(mir_insn_t *insn, int64_t imm) {
    insn->dst.type = MIR_OPND_IMM;
    insn->dst.value.imm = imm;
}

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
        printf("%s\n", mir_insn_to_str(insn));
    }
}

void mir_insn_insert_before(mir_insn_t* insn, mir_insn_t* before) {
    struct list_elem* e = &before->elem;
    list_insert_before(e, &insn->elem);
}

void mir_insn_insert_after(mir_insn_t* insn, mir_insn_t* after) {
    struct list_elem* e = &after->elem;
    list_insert_after(e, &insn->elem);
}