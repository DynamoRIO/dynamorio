#include "replayer.h"
#include "mir_insn.h"

// test basic mov operations
void test_replayer_mov() {
    printf("===> simple mov ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    // x1 <- 0xdeadbeef
    mir_insn_t reg_imm_mov_insn = {
        MIR_OP_MOV,
        {MIR_OPND_IMM, {.imm = 0xdeadbeef}},
        {MIR_OPND_IMM, {.imm = 0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}
    };
    mir_insn_push_back(&insn_list, &reg_imm_mov_insn);

    // x2 <- x1
    mir_insn_t reg_reg_mov_insn = {
        MIR_OP_MOV,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}},
        {MIR_OPND_IMM, {.imm = 0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}
    };
    mir_insn_push_back(&insn_list, &reg_reg_mov_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 0xdeadbeef);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 0xdeadbeef);
    printf("passed!\n");
}

// test basic arithmetic operations 
void test_replayer_add() {
    printf("===> simple reg&imm ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);

    // x1 <- 1 + 2
    mir_insn_t imm_imm_add_insn = {
        MIR_OP_ADD,
        {MIR_OPND_IMM, {.imm = 1}},
        {MIR_OPND_IMM, {.imm = 2}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}} // 3
    };
    mir_insn_push_back(&insn_list, &imm_imm_add_insn);

    // x2 <- x1 + 3
    mir_insn_t reg_imm_add_insn = {
        MIR_OP_ADD,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_IMM, {.imm = 3}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}} // 6
    };
    mir_insn_push_back(&insn_list, &reg_imm_add_insn);

    replayer.replay(&insn_list);

    // x3 <- x1 + x2
    mir_insn_t reg_reg_add_insn = {
        MIR_OP_ADD,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}, // 6
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}} // 9
    };
    mir_insn_push_back(&insn_list, &reg_reg_add_insn);

    // TEMP REG
    // t0 <- x1 + x2
    mir_insn_t reg_reg_add_tmp_insn = {
        MIR_OP_ADD,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}, // 6
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}} // 9
    };
    mir_insn_push_back(&insn_list, &reg_reg_add_tmp_insn);

    // x4 <- x1 + t0
    mir_insn_t reg_tmp_add_insn = {
        MIR_OP_ADD,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}}, // 9
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 3}} // 12
    };
    mir_insn_push_back(&insn_list, &reg_tmp_add_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 3);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 6);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 2) == 9);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 3) == 12);
    printf("passed!\n");
}

// test a mix of arithmetic operations
void test_replayer_arithmetic() {
    printf("===> arithmetic ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    // x1 <- 1 + 2 
    mir_insn_t imm_imm_add_insn = {
        MIR_OP_ADD,
        {MIR_OPND_IMM, {.imm = 1}},
        {MIR_OPND_IMM, {.imm = 2}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}} // 3
    };
    mir_insn_push_back(&insn_list, &imm_imm_add_insn);

    // x2 <- x1(3) - 2
    mir_insn_t reg_imm_sub_insn = {
        MIR_OP_SUB,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_IMM, {.imm = 2}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}} // 1
    };
    mir_insn_push_back(&insn_list, &reg_imm_sub_insn);

    // x3 <- x1(3) * x2(1)
    mir_insn_t reg_reg_mul_insn = {
        MIR_OP_MUL,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}, // 1
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}} // 3
    };
    mir_insn_push_back(&insn_list, &reg_reg_mul_insn);

    // x4 <- x1(3) / x2(1)
    mir_insn_t reg_reg_div_insn = {
        MIR_OP_DIV,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}, // 1
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 3}} // 3
    };
    mir_insn_push_back(&insn_list, &reg_reg_div_insn);

    // x5 <- x1(3) % 2
    mir_insn_t reg_imm_rem_insn = {
        MIR_OP_REM,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 3
        {MIR_OPND_IMM, {.imm = 2}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 4}} // 1
    };
    mir_insn_push_back(&insn_list, &reg_imm_rem_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 3);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 1);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 2) == 3);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 3) == 3);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 4) == 1);
    printf("passed!\n");
}

// test basic logical operations
void test_replayer_logical() {
    printf("===> logical ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    // x1 <- 0x10101010 & 0x01010101
    mir_insn_t imm_imm_and_insn = {
        MIR_OP_AND,
        {MIR_OPND_IMM, {.imm = 0x10101010}},
        {MIR_OPND_IMM, {.imm = 0x01010101}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}} // 0x00000000
    };
    mir_insn_push_back(&insn_list, &imm_imm_and_insn);

    // x2 <- x1(0x00000000) | 0x0f0f0f0f
    mir_insn_t reg_imm_or_insn = {
        MIR_OP_OR,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 0x00000000
        {MIR_OPND_IMM, {.imm = 0x0f0f0f0f}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}} // 0x0f0f0f0f
    };
    mir_insn_push_back(&insn_list, &reg_imm_or_insn);

    // x3 <- x2(0x0f0f0f0f) ^ 0xf0f0f0f0
    mir_insn_t reg_imm_xor_insn = {
        MIR_OP_XOR,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}}, // 0x0f0f0f0f
        {MIR_OPND_IMM, {.imm = 0xf0f0f0f0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}} // 0xffffffff
    };
    mir_insn_push_back(&insn_list, &reg_imm_xor_insn);

    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 0x00000000);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 0x0f0f0f0f);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 2) == 0xffffffff);
    printf("passed!\n");
}

// test basic memory operations
void test_replayer_memory() {
    printf("===> memory ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    // mov x1 <- 0x12345670
    mir_insn_t imm_imm_mov_insn = {
        MIR_OP_MOV,
        {MIR_OPND_IMM, {.imm = 0x12345670}},
        {MIR_OPND_IMM, {.imm = 0x0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}
    };
    mir_insn_push_back(&insn_list, &imm_imm_mov_insn);
    

    // st 0x12 -> mem[0x12345678]
    mir_insn_t reg_imm_st_insn = {
        MIR_OP_ST8,
        {MIR_OPND_IMM, {.imm = 0x12345678}},
        {MIR_OPND_IMM, {.imm = 0x0}},
        {MIR_OPND_IMM, {.imm = 0x12}}
    };
    mir_insn_push_back(&insn_list, &reg_imm_st_insn);
    
    // ld x2 <- mem[0x12345678]
    mir_insn_t imm_imm_ld_insn = {
        MIR_OP_LD8,
        {MIR_OPND_IMM, {.imm = 0x12345678}},
        {MIR_OPND_IMM, {.imm = 0x0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}} // 0x12
    };
    mir_insn_push_back(&insn_list, &imm_imm_ld_insn);

    // ld x3 <- [x1 + 0x8]
    mir_insn_t reg_imm_ld_insn = {
        MIR_OP_LD8,
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}}, // 0x12345678
        {MIR_OPND_IMM, {.imm = 0x8}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 2}} // 0x70
    };
    mir_insn_push_back(&insn_list, &reg_imm_ld_insn);
    
    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 0x12345670);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 0x12);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 2) == 0x12);
    printf("passed!\n");
}

// test mixed length memory operations
void test_replayer_mixed_length_memory() {
    printf("===> mixed length memory ===> ");
    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    // st32 0xdeadbeef -> mem[0x12345678]
    mir_insn_t reg_imm_st_insn = {
        MIR_OP_ST32,
        {MIR_OPND_IMM, {.imm = 0x12345678}},
        {MIR_OPND_IMM, {.imm = 0x0}},
        {MIR_OPND_IMM, {.imm = 0xdeadbeef}}
    };
    mir_insn_push_back(&insn_list, &reg_imm_st_insn);

    // ld16 x1 <- mem[0x12345678]
    mir_insn_t imm_imm_ld_16_insn = {
        MIR_OP_LD16,
        {MIR_OPND_IMM, {.imm = 0x12345678}},
        {MIR_OPND_IMM, {.imm = 0x0}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR}} // 0xbeef
    };
    mir_insn_push_back(&insn_list, &imm_imm_ld_16_insn);
    

    // ld8 x1 <- mem[0x12345678 + 2]
    mir_insn_t imm_imm_ld_8_insn = {
        MIR_OP_LD8,
        {MIR_OPND_IMM, {.imm = 0x12345678}},
        {MIR_OPND_IMM, {.imm = 0x2}},
        {MIR_OPND_REG, {.reg = DR_REG_START_GPR + 1}} // 0xad
    };
    mir_insn_push_back(&insn_list, &imm_imm_ld_8_insn);
    
    replayer.replay(&insn_list);
    assert(replayer.get_reg_val(DR_REG_START_GPR) == 0xbeef);
    assert(replayer.get_reg_val(DR_REG_START_GPR + 1) == 0xad);
    printf("passed!\n");
}

int main() {
    printf("Running tests...\n");
    test_replayer_mov();
    test_replayer_add();
    test_replayer_arithmetic();
    test_replayer_logical();
    test_replayer_memory();
    test_replayer_mixed_length_memory();
    // test_flag_regfile();
    /* TODO: test plans:
        - test memory operations
        - test control flow - do nothing
        - simple assembly program - load from file
        - random testing
        - corner cases?
    */
    printf("All tests passed!\n");
    return 0;
}