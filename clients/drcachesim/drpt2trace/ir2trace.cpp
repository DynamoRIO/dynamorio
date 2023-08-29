/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

#include "ir2trace.h"
#include "dr_api.h"

namespace dynamorio {
namespace drmemtrace {

#undef VPRINT_HEADER
#define VPRINT_HEADER()                  \
    do {                                 \
        fprintf(stderr, "drir2trace: "); \
    } while (0)

#undef VPRINT
#define VPRINT(level, ...)                \
    do {                                  \
        if (verbosity >= (level)) {       \
            VPRINT_HEADER();              \
            fprintf(stderr, __VA_ARGS__); \
            fflush(stderr);               \
        }                                 \
    } while (0)

#define ERRMSG_HEADER "[drpt2ir] "

ir2trace_convert_status_t
ir2trace_t::convert(IN drir_t &drir, INOUT std::vector<trace_entry_t> &trace,
                    IN int verbosity)
{
    if (drir.get_ilist() == NULL) {
        return IR2TRACE_CONV_ERROR_INVALID_PARAMETER;
    }
    instr_t *instr = instrlist_first(drir.get_ilist());
    while (instr != NULL) {
        trace_entry_t entry = {};

        if (!trace.empty() && trace.back().type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP) {
            if (instr_get_prev(instr) == nullptr ||
                !opnd_is_pc(instr_get_target(instr_get_prev(instr)))) {
                VPRINT(1, "Invalid branch instruction.\n");
                return IR2TRACE_CONV_ERROR_INVALID_PARAMETER;
            }
            app_pc target = opnd_get_pc(instr_get_target(instr_get_prev(instr)));
            if (reinterpret_cast<uintptr_t>(target) == entry.addr)
                trace.back().type = TRACE_TYPE_INSTR_TAKEN_JUMP;
            else
                trace.back().type = TRACE_TYPE_INSTR_UNTAKEN_JUMP;
        }

        /* Obtain the specific type of instruction.
         * TODO i#5505: The following code shares similarities with
         * instru_t::instr_to_instr_type(). After successfully linking the drir2trace
         * library to raw2trace, the redundancy should be eliminated by removing the
         * subsequent code.
         */
        entry.type = TRACE_TYPE_INSTR;
        if (instr_opcode_valid(instr)) {
            if (instr_is_call_direct(instr)) {
                entry.type = TRACE_TYPE_INSTR_DIRECT_CALL;
            } else if (instr_is_call_indirect(instr)) {
                entry.type = TRACE_TYPE_INSTR_INDIRECT_CALL;
            } else if (instr_is_return(instr)) {
                entry.type = TRACE_TYPE_INSTR_RETURN;
            } else if (instr_is_ubr(instr)) {
                entry.type = TRACE_TYPE_INSTR_DIRECT_JUMP;
            } else if (instr_is_mbr(instr)) {
                entry.type = TRACE_TYPE_INSTR_INDIRECT_JUMP;
            } else if (instr_is_cbr(instr)) {
                // We update this on the next iteration.
                entry.type = TRACE_TYPE_INSTR_CONDITIONAL_JUMP;
            } else if (instr_get_opcode(instr) == OP_sysenter) {
                entry.type = TRACE_TYPE_INSTR_SYSENTER;
            } else if (instr_is_rep_string_op(instr)) {
                entry.type = TRACE_TYPE_INSTR_MAYBE_FETCH;
            }
        } else {
            VPRINT(1, "Trying to convert an invalid instruction.\n");
        }

        entry.size = instr_length(GLOBAL_DCONTEXT, instr);
        entry.addr = (uintptr_t)instr_get_app_pc(instr);

        trace.push_back(entry);

        instr = instr_get_next(instr);
    }
    return IR2TRACE_CONV_SUCCESS;
}

} // namespace drmemtrace
} // namespace dynamorio
