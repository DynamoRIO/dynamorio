/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Define DR_FAST_IR to verify that everything compiles when we call the inline
 * versions of these routines.
 */
#ifndef STANDALONE_DECODER
# define DR_FAST_IR 1
#endif

/* Uses the DR CLIENT_INTERFACE API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 */

#ifndef USE_DYNAMO
#error NEED USE_DYNAMO
#endif

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#define VERBOSE 0

#ifdef STANDALONE_DECODER
# define ASSERT(x) \
    ((void)((!(x)) ? \
        (fprintf(stderr, "ASSERT FAILURE: %s:%d: %s\n", __FILE__,  __LINE__, #x),\
         abort(), 0) : 0))
#else
# define ASSERT(x) \
    ((void)((!(x)) ? \
        (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", __FILE__,  __LINE__, #x),\
         dr_abort(), 0) : 0))
#endif

static byte buf[8192];

/***************************************************************************
 * XXX i#1686: we need to add the IR consistency checks for ARM that we have on
 * x86, ensuring that these are all consistent with each other:
 * - decode
 * - INSTR_CREATE_
 * - encode
 */

/*
 ***************************************************************************/

static void
test_pred(void *dc)
{
    byte *pc;
    instr_t *inst;
    dr_isa_mode_t old_mode;
    dr_set_isa_mode(dc, DR_ISA_ARM_A32, &old_mode);
    inst = INSTR_PRED
        (INSTR_CREATE_sel
         (dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
          opnd_create_reg(DR_REG_R1)),
         DR_PRED_EQ);
    ASSERT(instr_get_eflags(inst, DR_QUERY_INCLUDE_COND_SRCS) == EFLAGS_READ_ALL);
    ASSERT(instr_get_eflags(inst, 0) == EFLAGS_READ_ARITH);
    instr_free(dc, inst);
    inst = INSTR_CREATE_sel
        (dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
         opnd_create_reg(DR_REG_R1));
    ASSERT(instr_get_eflags(inst, DR_QUERY_INCLUDE_COND_SRCS) == EFLAGS_READ_GE);
    ASSERT(instr_get_eflags(inst, 0) == EFLAGS_READ_GE);
    instr_free(dc, inst);
    dr_set_isa_mode(dc, old_mode, NULL);
}

static void
test_pcrel(void *dc)
{
    byte *pc;
    instr_t *inst;
    inst = INSTR_CREATE_ldr
        (dc, opnd_create_reg(DR_REG_R0),
         opnd_create_rel_addr((void *)&buf[128], OPSZ_PTR));
    pc = instr_encode(dc, inst, buf);
    /* (gdb) x/2i buf+1
     *    0x120b9 <buf+1>:     ldr     r0, [pc, #124]  ; (0x12138 <buf+128>)
     */
    ASSERT(pc != NULL);
    instr_free(dc, inst);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    /* XXX i#1686: add tests of all opcodes for internal consistency */

    /* XXX i#1686: add tests of XINST_CREATE macros */

    test_pcrel(dcontext);

    test_pred(dcontext);

    print("all done\n");
    return 0;
}
