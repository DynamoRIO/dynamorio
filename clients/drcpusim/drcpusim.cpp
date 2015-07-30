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
 * + Whitelist/blacklist to ignore system libraries
 * + Add more recent Intel models
 * + Add Atom models
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
 *
 * We ignore things like undocumented opcodes (e.g., OP_salc), which are later
 * in the opcode enum.
 */

#ifdef X86
static bool
opcode_supported_Pentium(instr_t *instr)
{
    /* Pentium CPUID features:
     *          CMPXCHG8B
     */
# ifdef X64
    // XXX: someone could construct x64-only opcodes (e.g., OP_movsxd) or
    // instrs (by using REX prefixes) in 32-bit -- we ignore that and assume
    // we only care about instrs in the app binary.
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

static bool
opcode_supported_Banias(instr_t *instr)
{
    /* Banias CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG8B
     *          FXSAVE/FXRSTORE
     *          SYSENTER/SYSEXIT
     */
# ifdef X64
    return false;
# else
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        // We assume that new and only new opcodes from SSE3+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_fisttp && !instr_is_sse2(instr)))
        return false;
    return true;
# endif
}

/* We simplify and assume that all Prescott models support 64-bit,
 * ignoring the early E-series models.
 */
static bool
opcode_supported_Prescott(instr_t *instr)
{
    /* Prescott CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *          FXSAVE/FXRSTORE
     *          MONITOR/MWAIT
     *          SYSENTER/SYSEXIT
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        // We assume that new and only new opcodes from SSSE3+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_pshufb && !instr_is_sse2(instr)
# ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
# endif
         ))
        return false;
    return true;
}

static bool
opcode_supported_Merom(instr_t *instr)
{
    /* Merom CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   FXSAVE/FXRSTORE
     *          MONITOR/MWAIT
     *          SYSENTER/SYSEXIT
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_popcnt && !instr_is_sse2(instr)
# ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
# endif
        ))
        return false;
    return true;
}

// XXX i#1732: Penryn stepping 10 added XSAVE: yet otherwise it seems to be a
// Sandybridge addition.  My gcc 4.8.3 generates OP_xgetbv which makes it seem
// like it should be present on older processors?  Something's not right.
static bool
opcode_supported_Penryn(instr_t *instr)
{
    /* Penryn CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   FXSAVE/FXRSTORE
     *  SSE4.1  MONITOR/MWAIT
     *          SYSENTER/SYSEXIT
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_popcnt && !instr_is_sse2(instr) && !instr_is_sse41(instr)
# ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
# endif
        ))
        return false;
    return true;
}

static bool
opcode_supported_Nehalem(instr_t *instr)
{
    /* Nehalem CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   FXSAVE/FXRSTORE
     *  SSE4.1  MONITOR/MWAIT
     *  SSE4.2  POPCNT
     *          RDTSCP
     *          SYSENTER/SYSEXIT
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_vmcall && !instr_is_sse2(instr) && opc != OP_rdtscp))
        return false;
    return true;
}

static bool
opcode_supported_Westmere(instr_t *instr)
{
    /* Westmere CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   FXSAVE/FXRSTORE
     *  SSE4.1  MONITOR/MWAIT
     *  SSE4.2  PCLMULDQ
     *  AES     POPCNT
     *          RDTSCP
     *          SYSENTER/SYSEXIT
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        // We assume that new and only new opcodes were appended to
        // the enum, except some SSE2 added late.
        // We assume we don't care about AMD SVM or Intel VMX (user-mode only).
        (opc >= OP_movbe && !instr_is_sse2(instr)))
        return false;
    return true;
}

static bool
opcode_supported_Sandybridge(instr_t *instr)
{
    /* Sandybridge CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   FXSAVE/FXRSTORE
     *  SSE4.1  MONITOR/MWAIT
     *  SSE4.2  PCLMULDQ
     *  AES     POPCNT
     *  AVX     RDTSCP
     *          SYSENTER/SYSEXIT
     *          XSAVE/XRESTORE states
     *          XSETBV/XGETBV are enabled
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        opc == OP_movbe ||
        // We assume that new and only new opcodes were appended to
        // the enum, except some SSE2 and split *xsave64 added late.
        // We assume we don't care about AMD SVM.
        (opc >= OP_vcvtph2ps && !(opc >= OP_movq2dq && opc <= OP_xsaveopt64)))
        return false;
    return true;
}

static bool
opcode_supported_Ivybridge(instr_t *instr)
{
    /* Ivybridge CPUID features:
     *  MMX     CLFLUSH
     *  SSE     CMOV
     *  SSE2    CMPXCHG16B
     *  SSE3    CMPXCHG8B
     *  SSSE3   Enhanced REP MOVSB/STOSB
     *  SSE4.1  FXSAVE/FXRSTORE
     *  SSE4.2  MONITOR/MWAIT
     *  AES     PCLMULDQ
     *  AVX     POPCNT
     *  F16C    RD/WR FSGSBASE instructions
     *          RDRAND
     *          RDTSCP
     *          SYSENTER/SYSEXIT
     *          XSAVE/XRESTORE states
     *          XSETBV/XGETBV are enabled
     */
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow(instr) ||
        (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        opc == OP_movbe ||
        // FMA
        (opc >= OP_vfmadd132ps && opc <= OP_vfnmsub231sd) ||
        // We assume that new and only new opcodes were appended to the enum.
        // We assume we don't care about AMD SVM.
        opc >= OP_rdseed)
        return false;
    return true;
}
#endif /* X86 */

static void
report_invalid_opcode(int opc, app_pc pc)
{
    // XXX i#1732: add drsyms and provide file + line# (will require locating dbghelp
    // and installing it in the release package).
    // XXX i#1732: ideally, provide a callstack: this is where we'd want DrCallstack.
    NOTIFY(0, "<Invalid %s instruction \"%s\" @ "PFX".  Aborting.>\n",
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
    } else if (op_cpu.get_value() == "PentiumM" ||
               op_cpu.get_value() == "Banias" ||
               op_cpu.get_value() == "Dothan" ||
               // These are early Pentium4 models
               op_cpu.get_value() == "Willamette" ||
               op_cpu.get_value() == "Northwood") {
        opcode_supported = opcode_supported_Banias;
    } else if (op_cpu.get_value() == "Pentium4" ||
               op_cpu.get_value() == "Prescott" ||
               op_cpu.get_value() == "Presler") {
        opcode_supported = opcode_supported_Prescott;
    } else if (op_cpu.get_value() == "Core2" ||
               op_cpu.get_value() == "Merom") {
        opcode_supported = opcode_supported_Merom;
    } else if (op_cpu.get_value() == "Penryn") {
        opcode_supported = opcode_supported_Penryn;
    } else if (op_cpu.get_value() == "Nehalem") {
        opcode_supported = opcode_supported_Nehalem;
    } else if (op_cpu.get_value() == "Westmere") {
        opcode_supported = opcode_supported_Westmere;
    } else if (op_cpu.get_value() == "Sandybridge") {
        opcode_supported = opcode_supported_Sandybridge;
    } else if (op_cpu.get_value() == "Ivybridge") {
        opcode_supported = opcode_supported_Ivybridge;
    } else {
        // XXX i#1732: add Atom and AMD models.
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
