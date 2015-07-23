/* ******************************************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* drcpusim.cpp: client for simulating instruction sets of legacy processors
 *
 * XXX i#1732: add more features, such as:
 * + Add more recent Intel models
 * + Add AMD models
 * + Add ARM support
 */

#include "dr_api.h"
#include "drmgr.h"
#include "droption.h"
#include "options.h"

// XXX i#1732: make a msgbox on Windows (controlled by option for batch runs)
#define NOTIFY(level, ...) do {            \
    if (op_verbose.get_value() >= (level)) \
        dr_fprintf(STDERR, __VA_ARGS__);   \
} while (0)

static bool (*opcode_supported)(instr_t *);

/* DR deliberately does not bother to keep model-specific information in its
 * IR.  Thus we have our own routines here that mostly just check opcodes.
 */

#ifdef X86
static bool
opcode_supported_Pentium(instr_t *instr)
{
    /* Pentium CPUID features:
     *          CMPXCHG8B
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_mmx(instr) || instr_is_sse(instr) || instr_is_sse2(instr) ||
        instr_is_3DNow(instr) ||
        (opc >= OP_cmovo && opc <= OP_cmovnle) ||
        opc == OP_sysenter || opc == OP_sysexit ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}

static bool
opcode_supported_PentiumMMX(instr_t *instr)
{
    /* Pentium with MMX CPUID features:
     *  MMX     CMPXCHG8B
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) ||
        instr_is_3DNow(instr) ||
        (opc >= OP_cmovo && opc <= OP_cmovnle) ||
        opc == OP_sysenter || opc == OP_sysexit ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}

static bool
opcode_supported_PentiumPro(instr_t *instr)
{
    /* Pentium Pro CPUID features:
     *          CMOV
     *          CMPXCHG8B
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_mmx(instr) || instr_is_sse(instr) || instr_is_sse2(instr) ||
        instr_is_3DNow(instr) ||
        opc == OP_sysenter || opc == OP_sysexit ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}

static bool
opcode_supported_Klamath(instr_t *instr)
{
    /* Klamath Pentium 2 CPUID features:
     *  MMX     CMOV
     *          CMPXCHG8B
     *          SYSENTER/SYSEXIT
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_3DNow(instr) ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}

static bool
opcode_supported_Deschutes(instr_t *instr)
{
    /* Deschutes Pentium 2 CPUID features:
     *  MMX     CMOV
     *          CMPXCHG8B
     *          FXSAVE/FXRSTORE
     *          SYSENTER/SYSEXIT
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_3DNow(instr) ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}

static bool
opcode_supported_Pentium3(instr_t *instr)
{
    /* Pentium 3 CPUID features:
     *  MMX     CMOV
     *  SSE     CMPXCHG8B
     *          FXSAVE/FXRSTORE
     *          SYSENTER/SYSEXIT
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse2(instr) || instr_is_3DNow(instr) ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
# endif
}
#endif

static void
report_invalid_opcode(int opc, app_pc pc)
{
    // XXX i#1732: add drsyms and provide file + line# (will require locating dbghelp
    // and installing it in the release package).
    // XXX i#1732: ideally, provide a callstack: this is where we'd want DrCallstack.
    NOTIFY(0, "<Invalid %s opcode \"%s\" @ "PFX".  Aborting.>\n",
           op_cpu.get_value().c_str(), decode_opcode_name(opc), pc);
    dr_abort();
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb,
                      instr_t *instr, bool for_trace,
                      bool translating, void *user_data)
{
    // We check meta instrs too
    if (!opcode_supported(instr))
        report_invalid_opcode(instr_get_opcode(instr), instr_get_app_pc(instr));
#ifdef X86
    if (instr_get_opcode(instr) == OP_cpuid) {
        // XXX i#1732: fool cpuid for target processor
    }
#endif
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO CPU Simulator", "http://dynamorio.org/issues");

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv,
                                       &parse_err, NULL)) {
        NOTIFY(0, "Usage error: %s\nUsage:\n%s", parse_err.c_str(),
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
    if (op_cpu.get_value().empty()) {
        NOTIFY(0, "Usage error: cpu is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }

#ifdef X86
    if (op_cpu.get_value() == "Pentium") {
        opcode_supported = opcode_supported_Pentium;
    } else if (op_cpu.get_value() == "PentiumMMX") {
        opcode_supported = opcode_supported_PentiumMMX;
    } else if (op_cpu.get_value() == "PentiumPro") {
        opcode_supported = opcode_supported_PentiumPro;
    } else if (op_cpu.get_value() == "Pentium2" ||
               op_cpu.get_value() == "Klamath") {
        opcode_supported = opcode_supported_Klamath;
    } else if (op_cpu.get_value() == "Deschutes") {
        opcode_supported = opcode_supported_Deschutes;
    } else if (op_cpu.get_value() == "Pentium3" ||
               op_cpu.get_value() == "Coppermine" ||
               op_cpu.get_value() == "Tualatin") {
        opcode_supported = opcode_supported_Pentium3;
    } else {
        // FIXME i#1732: add the other models.
        // Maybe also add particular features like SSE2.
        NOTIFY(0, "Usage error: invalid cpu %s\nUsage:\n%s", op_cpu.get_value().c_str(),
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
#else
    // XXX i#1732: no ARM support yet
    NOTIFY(0, "ARM not supported yet\n");
    dr_abort();
#endif

    if (!drmgr_init())
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
}
