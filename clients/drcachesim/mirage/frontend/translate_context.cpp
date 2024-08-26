#include "translate_context.h"

struct translate_context_t *translate_context_create() {
    struct translate_context_t *ctx = (struct translate_context_t *)malloc(sizeof(struct translate_context_t));
    assert(ctx != NULL);
    ctx->tmp_reg_map = bitmap_create(NUM_TMP_REGS);
    return ctx;
}

int alloc_tmp_reg(struct translate_context_t *ctx) {
    return bitmap_acquire(ctx->tmp_reg_map) + MIR_TMP_REG_START;
}

void ctx_set_curr_instr(struct translate_context_t *ctx, instr_t *instr) {
    ctx->curr_instr = instr;
}