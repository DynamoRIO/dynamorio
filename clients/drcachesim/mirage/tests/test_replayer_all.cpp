#include "replayer.h"
#include "mir_insn.h"

int main() {

    Replayer replayer(INIT_STRATEGY_ZERO);
    mir_insn_list_t insn_list;
    init_mir_insn_list(&insn_list);
    
    mir_insn_t* insn = mir_insn_malloc(MIR_OP_ADD);
    mir_insn_set_src0_imm(insn, 1);
    mir_insn_set_src1_imm(insn, 2);
    mir_insn_set_dst_reg(insn, DR_REG_START_GPR);
    mir_insn_push_back(&insn_list, insn);

    replayer.replay(&insn_list);

    assert(replayer.get_reg_val(DR_REG_START_GPR) == 3);

    printf("All tests passed!\n");
    return 0;
}