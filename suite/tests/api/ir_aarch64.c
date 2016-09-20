/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
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

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

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

static void
test_base_disp_extend(dr_extend_type_t ext, bool scaled, opnd_size_t size, uint amount)
{
    opnd_t opnd;
    dr_extend_type_t ext_out;
    bool scaled_out, success;
    uint amount_out;

    opnd = opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_X2, ext, scaled, 0, 0, size);
    ext_out = opnd_get_index_extend(opnd, &scaled_out, &amount_out);
    ASSERT(ext == ext_out && scaled == scaled_out && amount == amount_out);
    opnd = opnd_create_base_disp_aarch64(DR_REG_X1, DR_REG_X2,
                                         DR_EXTEND_UXTX, false, 0, 0, size);
    success = opnd_set_index_extend(&opnd, ext, scaled);
    ASSERT(success);
    ext_out = opnd_get_index_extend(opnd, &scaled_out, &amount_out);
    ASSERT(ext == ext_out && scaled == scaled_out && amount == amount_out);
}

static void
test_extend(void *dc)
{
    opnd_t opnd;
    dr_extend_type_t ext;
    bool scaled;
    uint amount;

    test_base_disp_extend(DR_EXTEND_UXTW, true, OPSZ_1, 0);
    test_base_disp_extend(DR_EXTEND_UXTX, true, OPSZ_2, 1);
    test_base_disp_extend(DR_EXTEND_SXTW, true, OPSZ_4, 2);
    test_base_disp_extend(DR_EXTEND_SXTX, true, OPSZ_8, 3);
    test_base_disp_extend(DR_EXTEND_UXTW, true, OPSZ_16, 4);
    test_base_disp_extend(DR_EXTEND_UXTX, true, OPSZ_0, 3);
    test_base_disp_extend(DR_EXTEND_SXTW, false, OPSZ_4, 0);
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif

    test_extend(dcontext);

    print("all done\n");
    return 0;
}
