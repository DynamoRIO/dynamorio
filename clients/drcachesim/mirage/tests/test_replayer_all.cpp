#include "replayer.h"
#include "mir_insn.h"

// test basic mov operations
void test_replayer_mov() {
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    mir_insn_t* reg_imm_mov_insn = mir_insn_malloc(MIR_OP_MOV);
    mir_insn_set_src0_imm(reg_imm_mov_insn, 0xdeadbeef);
    mir_insn_set_src1_imm(reg_imm_mov_insn, 0);
    mir_insn_set_dst_reg(reg_imm_mov_insn, DR_REG_START_GPR);
    mir_insn_push_back(&insn_list, reg_imm_mov_insn);

    mir_insn_t* reg_reg_mov_insn = mir_insn_malloc(MIR_OP_MOV);
    mir_insn_set_src0_reg(reg_reg_mov_insn, DR_REG_START_GPR);
    mir_insn_set_src1_reg(reg_reg_mov_insn, REG_NULL);
    mir_insn_set_dst_reg(reg_reg_mov_insn, DR_REG_START_GPR + 1);
    mir_insn_push_back(&insn_list, reg_reg_mov_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 0xdeadbeef);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 0xdeadbeef);
}

// test basic arithmetic operations like add
void test_replayer_add() {
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    mir_insn_t* imm_imm_add_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_set_src0_imm(imm_imm_add_insn, 1);
    mir_insn_set_src1_imm(imm_imm_add_insn, 2);
    mir_insn_set_dst_reg(imm_imm_add_insn, DR_REG_START_GPR); // 3
    mir_insn_push_back(&insn_list, imm_imm_add_insn);

    mir_insn_t* reg_imm_add_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_set_src0_reg(reg_imm_add_insn, DR_REG_START_GPR); // 3
    mir_insn_set_src1_imm(reg_imm_add_insn, 2);
    mir_insn_set_dst_reg(reg_imm_add_insn, DR_REG_START_GPR + 1); // 5
    mir_insn_push_back(&insn_list, reg_imm_add_insn);

    mir_insn_t* reg_reg_add_insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_set_src0_reg(reg_reg_add_insn, DR_REG_START_GPR); // 3
    mir_insn_set_src1_reg(reg_reg_add_insn, DR_REG_START_GPR + 1); // 5
    mir_insn_set_dst_reg(reg_reg_add_insn, DR_REG_START_GPR + 2); // 8
    mir_insn_push_back(&insn_list, reg_reg_add_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 3);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 5);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 2) == 8);
}

// test a mix of arithmetic operations
void test_replayer_arithmetic() {

}

int main() {
    printf("Running tests...\n");
    test_replayer_mov();
    test_replayer_add();
    printf("All tests passed!\n");
    return 0;
}