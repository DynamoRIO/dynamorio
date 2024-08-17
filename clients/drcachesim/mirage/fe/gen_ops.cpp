#include "gen_ops.h"

const char* get_opnd_type(opnd_t opnd) {
    if (opnd_is_reg(opnd)) {
        return "reg";
    } 
    if (opnd_is_immed(opnd)) {
        return "imm";
    }
    if (opnd_is_base_disp(opnd)) {
        return "base_disp";
    }
    if (opnd_is_pc(opnd)) {
        return "pc";
    }
    if (opnd_is_instr(opnd)) {
        return "instr";
    }
    return "unknown";
}

void gen_add_op(instr_t *instr, std::list<mir_insn_t*> *mir_insns) {
    assert(instr_num_srcs(instr) == 2);
    assert(instr_num_dsts(instr) == 1);
    assert(mir_insns->size() == 0);

    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);

    // create a new node
    mir_insn_t core_insn;
    mir_insn_init(&core_insn);
    mir_insns->push_back(&core_insn);

    // add reg, reg => reg
    if (opnd_is_reg(src0) && opnd_is_reg(src1) && opnd_is_reg(dst)) {
        reg_id_t src0_reg = opnd_get_reg(src0);
        reg_id_t src1_reg = opnd_get_reg(src1);
        reg_id_t dst_reg = opnd_get_reg(dst);
        printf("add reg %d, reg %d to reg %d\n", src0_reg, src1_reg, dst_reg); 
    }

    // add imm, reg => reg
    else if (opnd_is_immed(src0) && opnd_is_reg(src1) && opnd_is_reg(dst)) {
        int64_t src0_imm = opnd_get_immed_int(src0);
        reg_id_t src1_reg = opnd_get_reg(src1);
        reg_id_t dst_reg = opnd_get_reg(dst);
        printf("addi imm %ld, reg %d to reg %d\n", src0_imm, src1_reg, dst_reg);
    }

    // add imm, base_disp => base_disp
    else if (opnd_is_immed(src0) && opnd_is_base_disp(src1) && opnd_is_base_disp(dst)) {
        int64_t src0_imm = opnd_get_immed_int(src0);
        reg_id_t src1_base = opnd_get_base(src1);
        int src1_disp = opnd_get_disp(src1);
        reg_id_t dst_base = opnd_get_base(dst);
        int dst_disp = opnd_get_disp(dst);
        printf("load imm %d, base reg %d to TMP reg\n", src1_disp, src1_base);
        printf("addi imm %ld, TMP reg to TMP reg\n", src0_imm);
        printf("store TMP reg, %d disp to base reg %d\n", dst_disp, dst_base);
    }

    // add base_disp, reg => base_disp
    else if (opnd_is_base_disp(src0) && opnd_is_reg(src1) && opnd_is_base_disp(dst)) {
        reg_id_t src0_base = opnd_get_base(src0);
        int src0_disp = opnd_get_disp(src0);
        reg_id_t src1_reg = opnd_get_reg(src1);
        reg_id_t dst_reg = opnd_get_reg(dst);
        printf("load base reg %d, %d disp to TMP reg\n", src0_base, src0_disp);
        printf("add reg %d, TMP reg to reg %d\n", src1_reg, dst_reg);
    }

    else {
        printf("unsupported opnd type\n");
        printf("src0: %s\n", get_opnd_type(src0));
        printf("src1: %s\n", get_opnd_type(src1));    
        printf("dst: %s\n", get_opnd_type(dst));
    }
}