#include "dr_mir_api.h"

void dr_gen_mir_ops(instr_t *instr, mir_insn_list_t *insn_list) {

    int opc = instr_get_opcode(instr);

    struct translate_context_t *ctx = translate_context_create();
    ctx_set_curr_instr(ctx, instr);

    switch (opc) {
        case OP_nop_modrm:
        case OP_rdtsc:
            gen_nop_op(instr, insn_list, ctx);
            break;
        // Arithmetic & Bitwise instructions
        case OP_add:
            gen_add_op(instr, insn_list, ctx);
            break;
        case OP_sub:
            gen_sub_op(instr, insn_list, ctx);
            break;
        case OP_or:
            gen_or_op(instr, insn_list, ctx);
            break;
        case OP_and:
            gen_and_op(instr, insn_list, ctx);
            break;
        case OP_xor:
            gen_xor_op(instr, insn_list, ctx);
            break;
        case OP_shl:
            gen_shl_op(instr, insn_list, ctx);
            break;
        case OP_shr:
            gen_shr_op(instr, insn_list, ctx);
            break;
        // All movs are handled the same way
        case OP_mov_ld:
        case OP_mov_st:
        case OP_mov_imm:
        case OP_mov_seg:
        case OP_mov_priv:
        case OP_cwde:
            gen_mov_op(instr, insn_list, ctx);
            break;
        case OP_lea:
            gen_lea_op(instr, insn_list, ctx);
            break;
        // Compounded instructions
        case OP_push:
            gen_push_op(instr, insn_list, ctx);
            break;
        case OP_pop:
            gen_pop_op(instr, insn_list, ctx);
            break;
        case OP_call:
            gen_call_op(instr, insn_list, ctx);
            break;
        case OP_ret_far:
        case OP_ret:
            gen_ret_op(instr, insn_list, ctx);
            break;
        case OP_test:
            gen_test_op(instr, insn_list, ctx);
            break;
        case OP_cmp:
            gen_cmp_op(instr, insn_list, ctx);
            break;
        case OP_adc:
            gen_adc_op(instr, insn_list, ctx);
            break;
        // All jumps are trivially handled for now
        case OP_jmp:
        case OP_jmp_short:
        case OP_jmp_ind:
        case OP_jmp_far:
        case OP_jmp_far_ind:
        case OP_jo_short:
        case OP_jno_short:
        case OP_jb_short:
        case OP_jnb_short:
        case OP_jz_short:
        case OP_jnz_short:
        case OP_jbe_short:
        case OP_jnbe_short:
        case OP_js_short:
        case OP_jns_short:
        case OP_jp_short:
        case OP_jnp_short:
        case OP_jl_short:
        case OP_jnl_short:
        case OP_jle_short:
        case OP_jnle_short:
        case OP_jo:
        case OP_jno:
        case OP_jb:
        case OP_jnb:
        case OP_jz:
        case OP_jnz:
        case OP_jbe:
        case OP_jnbe:
        case OP_js:
        case OP_jns:
        case OP_jp:
        case OP_jnp:
        case OP_jl:
        case OP_jnl:
        case OP_jle:
        case OP_jnle:
            gen_jump_op(instr, insn_list, ctx);
            break;
        default:
            printf("Unsupported opcode: %d\n", opc);
            break;
    }
    print_mir_insn_list(insn_list);
}