#include "dr_mir_api.h"

void dr_gen_mir_ops(instr_t *instr) {

    int opc = instr_get_opcode(instr);

    mir_insn_list_t insn_list; // TODO: on the stack for now
    init_mir_insn_list(&insn_list);
    struct translate_context_t *ctx = translate_context_create();
    ctx_set_curr_instr(ctx, instr);

    switch (opc) {
        case OP_add:
            gen_add_op(instr, &insn_list, ctx);
            break;
        case OP_sub:
            gen_sub_op(instr, &insn_list, ctx);
            break;
        case OP_or:
            gen_or_op(instr, &insn_list, ctx);
            break;
        case OP_and:
            gen_and_op(instr, &insn_list, ctx);
            break;
        case OP_xor:
            gen_xor_op(instr, &insn_list, ctx);
            break;
        case OP_push:
            gen_push_op(instr, &insn_list, ctx);
            break;
        default:
            printf("Unsupported opcode: %d\n", opc);
            break;
    }
    print_mir_insn_list(&insn_list);
}