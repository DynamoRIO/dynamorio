#include "dr_mir_api.h"

void dr_gen_mir_ops(instr_t *instr) {
    int opc = instr_get_opcode(instr);
    switch (opc) {
        case OP_add:
            gen_add_op(instr);
            break;
        default:
            printf("Unsupported opcode: %d\n", opc);
            break;
    }
}