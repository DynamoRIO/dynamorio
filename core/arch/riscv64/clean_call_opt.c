#include "../globals.h"
#include "arch.h"

void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

app_pc
check_callee_instr_level2(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc,
                          app_pc cur_pc, app_pc tgt_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

bool
check_callee_ilist_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
analyze_clean_call_aflags(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *where, opnd_t *args)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                        instr_t *where, opnd_t *args)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
