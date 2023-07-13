/* ******************************************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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
 * + Add Atom models
 * + Add AMD models
 * + Add ARM support
 */

#include "dr_api.h"
#include "drmgr.h"
#include "droption.h"
#include "options.h"
#include <vector>
#include <string>
#include <sstream>

namespace dynamorio {
namespace drcpusim {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;

// XXX i#1732: make a msgbox on Windows (controlled by option for batch runs)
#define NOTIFY(level, ...)                                          \
    do {                                                            \
        if (dynamorio::drcpusim::op_verbose.get_value() >= (level)) \
            dr_fprintf(STDERR, __VA_ARGS__);                        \
    } while (0)

static bool (*opcode_supported)(instr_t *);

static std::vector<std::string> blocklist;

static app_pc exe_start;

/* DR deliberately does not bother to keep model-specific information in its
 * IR.  Thus we have our own routines here that mostly just check opcodes.
 *
 * We ignore things like undocumented opcodes (e.g., OP_salc), which are later
 * in the opcode enum.
 */

#ifdef X86
/***************************************************************************
 * Intel
 */

#    define CPUID_INTEL_EBX /* Genu */ 0x756e6547
#    define CPUID_INTEL_EDX /* ineI */ 0x49656e69
#    define CPUID_INTEL_ECX /* ntel */ 0x6c65746e

#    define FEAT(DR_proc_val) (1U << ((FEATURE_##DR_proc_val) % 32))

// Family encoding:
//   ext family | ext model | type  | family | model | stepping
//      27:20   |   19:16   | 13:12 |  11:8  |  7:4  |   3:0
static inline unsigned int
cpuid_encode_family(unsigned int family, unsigned int model, unsigned int stepping)
{
    unsigned int ext_family = 0;
    unsigned int ext_model = 0;
    if (family == 6 || family == 15) {
        ext_model = model >> 4;
        model = model & 0xf;
    }
    if (family >= 15) {
        ext_family = family - 15;
        family = 15;
    }
    DR_ASSERT((stepping & ~0xf) == 0);
    DR_ASSERT((model & ~0xf) == 0);
    DR_ASSERT((family & ~0xf) == 0);
    DR_ASSERT((ext_model & ~0xf) == 0);
    return (ext_family << 20) | (ext_model << 16) |
        // type is 0 == Original OEM
        (family << 8) | (model << 4) | stepping;
}

typedef struct _cpuid_model_t {
    unsigned int max_input;
    unsigned int max_ext_input;
    unsigned int encoded_family;
    unsigned int features_edx;
    unsigned int features_ecx;
    unsigned int features_ext_edx;
    unsigned int features_ext_ecx;
    unsigned int features_sext_ebx;
} cpuid_model_t;

static cpuid_model_t *model_info;

static bool
instr_is_3DNow_no_Intel(instr_t *instr)
{
    /* OP_prefetchw is not officially supported on Intel processors
     * prior to Broadwell (cpuid feature bit is not set) yet it won't fault (except
     * maybe on pretty old processors) and will just be a nop.  Windows likes to
     * use it, so we do not complain about it by default.
     */
    return (instr_is_3DNow(instr) &&
            (instr_get_opcode(instr) != OP_prefetchw || !op_allow_prefetchw.get_value()));
}

/***************************************************
 * Pentium
 */
static cpuid_model_t model_Pentium = {
    1,
    // XXX i#1732: manual is confusing: supposed to return real info as though
    // eax was set to the highest supported val?  Just returning 0 for now.
    0,
    // These are values observed on real processors.
    // XXX: DR should add some MODEL_PENTIUM, etc. values.
    cpuid_encode_family(FAMILY_PENTIUM, 2, 11),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        // ISA-affecting:
        FEAT(CX8),
    0, 0, 0, 0
};

static bool
opcode_supported_Pentium(instr_t *instr)
{
#    ifdef X64
    // XXX: someone could construct x64-only opcodes (e.g., OP_movsxd) or
    // instrs (by using REX prefixes) in 32-bit -- we ignore that and assume
    // we only care about instrs in the app binary.
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_mmx(instr) || instr_is_sse(instr) || instr_is_sse2(instr) ||
        instr_is_3DNow_no_Intel(instr) || (opc >= OP_cmovo && opc <= OP_cmovnle) ||
        opc == OP_sysenter || opc == OP_sysexit || opc == OP_fxsave32 ||
        opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Pentium with MMX
 */
static cpuid_model_t model_PentiumMMX = { 2,
                                          0, // see Pentium comment
                                          cpuid_encode_family(FAMILY_PENTIUM, 4, 3),
                                          FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) |
                                              FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
                                              // ISA-affecting:
                                              FEAT(CX8) | FEAT(MMX),
                                          0,
                                          0,
                                          0,
                                          0 };

static bool
opcode_supported_PentiumMMX(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_3DNow_no_Intel(instr) ||
        (opc >= OP_cmovo && opc <= OP_cmovnle) || opc == OP_sysenter ||
        opc == OP_sysexit || opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Pentium Pro
 */
static cpuid_model_t model_PentiumPro = { 2,
                                          0, // see Pentium comment
                                          cpuid_encode_family(FAMILY_PENTIUM_PRO, 1, 7),
                                          FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) |
                                              FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
                                              FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) |
                                              FEAT(PAE) |
                                              // ISA-affecting:
                                              FEAT(CX8) | FEAT(CMOV),
                                          0,
                                          0,
                                          0,
                                          0 };

static bool
opcode_supported_PentiumPro(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_mmx(instr) || instr_is_sse(instr) || instr_is_sse2(instr) ||
        instr_is_3DNow_no_Intel(instr) || opc == OP_sysenter || opc == OP_sysexit ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Klamath Pentium 2
 */
static cpuid_model_t model_Klamath = { 2,
                                       0, // see Pentium comment
                                       cpuid_encode_family(FAMILY_PENTIUM_2, 3, 4),
                                       FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) |
                                           FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
                                           FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) |
                                           FEAT(PAE) |
                                           // ISA-affecting:
                                           FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP),
                                       0,
                                       0,
                                       0,
                                       0 };

static bool
opcode_supported_Klamath(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_3DNow_no_Intel(instr) ||
        opc == OP_fxsave32 || opc == OP_fxrstor32 ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Deschutes Pentium 2
 */
static cpuid_model_t model_Deschutes = {
    2,
    0, // see Pentium comment
    cpuid_encode_family(FAMILY_PENTIUM_2, 5, 2),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR),
    0,
    0,
    0,
    0
};

static bool
opcode_supported_Deschutes(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse(instr) || instr_is_sse2(instr) || instr_is_3DNow_no_Intel(instr) ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Pentium 3
 */
static cpuid_model_t model_Pentium3 = {
    3,
    0, // see Pentium comment
    cpuid_encode_family(FAMILY_PENTIUM_3, 7, 2),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE),
    0,
    0,
    0,
    0
};

static bool
opcode_supported_Pentium3(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_sse2(instr) || instr_is_3DNow_no_Intel(instr) ||
        // We assume that new opcodes from SSE3+ (incl OP_monitor and OP_mwait)
        // were appended to the enum.
        opc >= OP_fisttp)
        return false;
    return true;
#    endif
}

/***************************************************
 * Banias
 */
static cpuid_model_t model_Banias = {
    2,
    0x80000004,
    cpuid_encode_family(FAMILY_PENTIUM_4, 2, 4),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    0,
    0,
    0,
    0
};

static bool
opcode_supported_Banias(instr_t *instr)
{
#    ifdef X64
    return false;
#    else
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) ||
        // We assume that new and only new opcodes from SSE3+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_fisttp && !instr_is_sse2(instr)))
        return false;
    return true;
#    endif
}

/***************************************************
 * Prescott
 */
/* We simplify and assume that all Prescott models support 64-bit,
 * ignoring the early E-series models.
 */
static cpuid_model_t model_Prescott = {
    5, // XXX: maybe 2, maybe 6?
    0x80000008,
    cpuid_encode_family(FAMILY_PENTIUM_4, 4, 10),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) | FEAT(HTT) | FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16),
    FEAT(EM64T) | FEAT(XD_Bit),
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Prescott(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) ||
        // We assume that new and only new opcodes from SSSE3+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_pshufb &&
         !instr_is_sse2(instr)
#    ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
#    endif
         ))
        return false;
    return true;
}

/***************************************************
 * Merom
 */
// XXX: I'm ignoring the eax=6 table (digital thermal sensors)
static cpuid_model_t model_Merom = {
    10,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_CORE_MEROM, 11),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) /* no HTT */ |
        FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3),
    FEAT(EM64T) | FEAT(XD_Bit),
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Merom(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_popcnt &&
         !instr_is_sse2(instr)
#    ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
#    endif
         ))
        return false;
    return true;
}

/***************************************************
 * Penryn
 */
// XXX i#1732: Penryn stepping 10 added XSAVE: yet otherwise it seems to be a
// Sandybridge addition.  My gcc 4.8.3 generates OP_xgetbv which makes it seem
// like it should be present on older processors?  Something's not right.
static cpuid_model_t model_Penryn = {
    10,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_CORE_PENRYN, 6),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) /* no HTT */ |
        FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3) | FEAT(SSE41),
    FEAT(EM64T) | FEAT(XD_Bit),
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Penryn(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_popcnt && !instr_is_sse2(instr) &&
         !instr_is_sse41(instr)
#    ifdef X64
         // Allow new x64 opcodes
         && opc != OP_movsxd && opc != OP_swapgs
#    endif
         ))
        return false;
    return true;
}

/***************************************************
 * Nehalem
 */
// XXX: I'm ignoring the eax=6 table (Turbo Boost)
static cpuid_model_t model_Nehalem = {
    10,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_I7_GAINESTOWN, 5),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) | FEAT(HTT) | FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3) | FEAT(SSE41) |
        FEAT(SSE42) | FEAT(POPCNT),
    FEAT(EM64T) | FEAT(XD_Bit) | FEAT(RDTSCP),
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Nehalem(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) || (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        // We assume that new and only new opcodes from SSE4+ were
        // appended to the enum, except some SSE2 added late.
        (opc >= OP_vmcall && !instr_is_sse2(instr) && opc != OP_rdtscp))
        return false;
    return true;
}

/***************************************************
 * Westmere
 */
static cpuid_model_t model_Westmere = {
    10,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_I7_WESTMERE, 2),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) | FEAT(HTT) | FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) | FEAT(PCID) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3) | FEAT(SSE41) |
        FEAT(SSE42) | FEAT(POPCNT) | FEAT(AES) | FEAT(PCLMULQDQ),
    FEAT(EM64T) | FEAT(XD_Bit) | FEAT(RDTSCP) | FEAT(PDPE1GB),
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Westmere(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) || (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        // We assume that new and only new opcodes were appended to
        // the enum, except some SSE2 added late.
        // We assume we don't care about AMD SVM or Intel VMX (user-mode only).
        (opc >= OP_movbe && !instr_is_sse2(instr)))
        return false;
    return true;
}

/***************************************************
 * Sandybridge
 */
static cpuid_model_t model_Sandybridge = {
    11,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_SANDYBRIDGE, 7),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) | FEAT(HTT) | FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) | FEAT(PCID) | FEAT(x2APIC) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3) | FEAT(SSE41) |
        FEAT(SSE42) | FEAT(POPCNT) | FEAT(AES) | FEAT(PCLMULQDQ) | FEAT(AVX) |
        FEAT(XSAVE) | FEAT(OSXSAVE),
    FEAT(EM64T) | FEAT(XD_Bit) | FEAT(RDTSCP), /*no PDPE1GB */
    FEAT(LAHF),
    0
};

static bool
opcode_supported_Sandybridge(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) || (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        opc == OP_movbe ||
        // We assume that new and only new opcodes were appended to
        // the enum, except some SSE2 and split *xsave64 added late.
        // We assume we don't care about AMD SVM.
        (opc >= OP_vcvtph2ps && !(opc >= OP_movq2dq && opc <= OP_xsaveopt64)))
        return false;
    return true;
}

/***************************************************
 * Ivybridge
 */
static cpuid_model_t model_Ivybridge = {
    11,
    0x80000008,
    cpuid_encode_family(FAMILY_CORE_2, MODEL_IVYBRIDGE, 9),
    FEAT(FPU) | FEAT(VME) | FEAT(DE) | FEAT(PSE) | FEAT(TSC) | FEAT(MSR) | FEAT(MCE) |
        FEAT(MTRR) | FEAT(MCA) | FEAT(PGE) | FEAT(PAE) | FEAT(PSE_36) | FEAT(PAT) |
        FEAT(APIC) | FEAT(DS) | FEAT(SS) | FEAT(TM) | FEAT(ACPI) | FEAT(HTT) | FEAT(PBE) |
        // ISA-affecting:
        FEAT(CX8) | FEAT(CMOV) | FEAT(MMX) | FEAT(SEP) | FEAT(FXSR) | FEAT(SSE) |
        FEAT(SSE2) | FEAT(CLFSH),
    FEAT(DTES64) | FEAT(DS_CPL) | FEAT(CID) | FEAT(xTPR) | FEAT(EST) | FEAT(TM2) |
        FEAT(VMX) | FEAT(SMX) | FEAT(PDCM) | FEAT(PCID) | FEAT(x2APIC) |
        // ISA-affecting:
        FEAT(SSE3) | FEAT(MONITOR) | FEAT(CX16) | FEAT(SSSE3) | FEAT(SSE41) |
        FEAT(SSE42) | FEAT(POPCNT) | FEAT(AES) | FEAT(PCLMULQDQ) | FEAT(AVX) |
        FEAT(XSAVE) | FEAT(OSXSAVE) | FEAT(F16C) | FEAT(RDRAND),
    FEAT(EM64T) | FEAT(XD_Bit) | FEAT(RDTSCP), /*no PDPE1GB */
    FEAT(LAHF),
    FEAT(FSGSBASE) | FEAT(ERMSB)
};

static bool
opcode_supported_Ivybridge(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (instr_is_3DNow_no_Intel(instr) || (instr_is_sse4A(instr) && opc != OP_popcnt) ||
        opc == OP_movbe ||
        // FMA
        (opc >= OP_vfmadd132ps && opc <= OP_vfnmsub231sd) ||
        // We assume that new and only new opcodes were appended to the enum.
        // We assume we don't care about AMD SVM.
        opc >= OP_rdseed)
        return false;
    return true;
}

// Called via clean call after the cpuid instr, with spill slots 1 and 2 holding
// the input eax and ecx.
static void
fake_cpuid(void)
{
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mc; // memset to zero is expensive so no full init
    reg_t input_eax = dr_read_saved_reg(drcontext, SPILL_SLOT_1);
    reg_t input_ecx = dr_read_saved_reg(drcontext, SPILL_SLOT_2);
    mc.size = sizeof(mc);
    mc.flags = DR_MC_INTEGER;
    dr_get_mcontext(drcontext, &mc);

    if (input_eax == 0) {
        // Pretend Intel
        mc.xax = model_info->max_input;
        mc.xbx = CPUID_INTEL_EBX;
        mc.xdx = CPUID_INTEL_EDX;
        mc.xcx = CPUID_INTEL_ECX;
        dr_set_mcontext(drcontext, &mc);
    } else if (input_eax == 1) {
        mc.xax = model_info->encoded_family;
        mc.xdx = model_info->features_edx;
        mc.xcx = model_info->features_ecx;
        dr_set_mcontext(drcontext, &mc);
    } else if (input_eax == 0x80000000) {
        mc.xax = model_info->max_ext_input;
        dr_set_mcontext(drcontext, &mc);
    } else if (input_eax == 0x80000001) {
        if (model_info->max_ext_input >= 0x80000001) {
            mc.xdx = model_info->features_ext_edx;
            mc.xcx = model_info->features_ext_ecx;
        } else {
            mc.xdx = 0;
            mc.xcx = 0;
        }
        dr_set_mcontext(drcontext, &mc);
    } else if (input_eax == 7 && input_ecx == 0) {
        if (model_info->max_input >= 7) {
            mc.xbx = model_info->features_sext_ebx;
        } else {
            mc.xbx = 0;
        }
        dr_set_mcontext(drcontext, &mc);
    }
}
#endif /* X86 */

static void
report_invalid_opcode(int opc, app_pc pc)
{
    // XXX i#1732: add drsyms and provide file + line# (will require locating dbghelp
    // and installing it in the release package).
    // XXX i#1732: ideally, provide a callstack: this is where we'd want DrCallstack.
    module_data_t *mod = dr_lookup_module(pc);
    const char *modname = NULL;
    if (op_ignore_all_libs.get_value() && mod != NULL && mod->start != exe_start) {
        dr_free_module_data(mod);
        return;
    }
    if (mod != NULL)
        modname = dr_module_preferred_name(mod);
    if (modname != NULL) {
        for (std::vector<std::string>::iterator i = blocklist.begin();
             i != blocklist.end(); ++i) {
            if (*i == modname) {
                dr_free_module_data(mod);
                return;
            }
        }
        // It would be nice to share pieces of the msg but we'd want to
        // build up a buffer to ensure a single atomic print.
        NOTIFY(0, "<Invalid %s instruction \"%s\" @ %s+" PIFX ".  %s.>\n",
               op_cpu.get_value().c_str(), decode_opcode_name(opc), modname,
               pc - mod->start, op_continue.get_value() ? "Continuing" : "Aborting");
    } else {
        NOTIFY(0, "<Invalid %s instruction \"%s\" @ " PFX ".  %s.>\n",
               op_cpu.get_value().c_str(), decode_opcode_name(opc), pc,
               op_continue.get_value() ? "Continuing" : "Aborting");
    }
    if (mod != NULL)
        dr_free_module_data(mod);
    if (!op_continue.get_value())
        dr_abort();
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    // We check meta instrs too
    if (!opcode_supported(instr))
        report_invalid_opcode(instr_get_opcode(instr), instr_get_app_pc(instr));
#ifdef X86
    if (op_fool_cpuid.get_value() && instr_get_opcode(instr) == OP_cpuid) {
        // It's non-trivial to fully emulate cpuid, or even to emulate the cases
        // we care about (e.g., we don't want to be filling in the brand index
        // or APIC ID or anything).  Thus we save the inputs and correct the
        // output after we let the cpuid instr execute as normal.
        //
        // XXX: technically DR doesn't promise to preserve these across the
        // cpuid but we're willing to risk that (we know DR won't do any
        // selfmod or other intensive mangling for cpuid).
        // We could work around by indirecting through a drmgr slot or using
        // raw DR slots.
        dr_save_reg(drcontext, bb, instr, DR_REG_XAX, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, instr, DR_REG_XCX, SPILL_SLOT_2);
        // XXX: technically drmgr doesn't want us inserting instrs *after* the
        // app instr but this is the simplest way to go.
        dr_insert_clean_call_ex(
            drcontext, bb, instr_get_next(instr), (void *)fake_cpuid,
            static_cast<dr_cleancall_save_t>(DR_CLEANCALL_READS_APP_CONTEXT |
                                             DR_CLEANCALL_WRITES_APP_CONTEXT),
            0);
    }
#endif
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    drmgr_exit();
}

static void
set_opcode_and_model()
{
#ifdef X86
    if (op_cpu.get_value() == "Pentium") {
        opcode_supported = opcode_supported_Pentium;
        model_info = &model_Pentium;
    } else if (op_cpu.get_value() == "PentiumMMX") {
        opcode_supported = opcode_supported_PentiumMMX;
        model_info = &model_PentiumMMX;
    } else if (op_cpu.get_value() == "PentiumPro") {
        opcode_supported = opcode_supported_PentiumPro;
        model_info = &model_PentiumPro;
    } else if (op_cpu.get_value() == "Pentium2" || op_cpu.get_value() == "Klamath") {
        opcode_supported = opcode_supported_Klamath;
        model_info = &model_Klamath;
    } else if (op_cpu.get_value() == "Deschutes") {
        opcode_supported = opcode_supported_Deschutes;
        model_info = &model_Deschutes;
    } else if (op_cpu.get_value() == "Pentium3" || op_cpu.get_value() == "Coppermine" ||
               op_cpu.get_value() == "Tualatin") {
        opcode_supported = opcode_supported_Pentium3;
        model_info = &model_Pentium3;
    } else if (op_cpu.get_value() == "PentiumM" || op_cpu.get_value() == "Banias" ||
               op_cpu.get_value() == "Dothan" ||
               // These are early Pentium4 models
               op_cpu.get_value() == "Willamette" || op_cpu.get_value() == "Northwood") {
        opcode_supported = opcode_supported_Banias;
        model_info = &model_Banias;
    } else if (op_cpu.get_value() == "Pentium4" || op_cpu.get_value() == "Prescott" ||
               op_cpu.get_value() == "Presler") {
        opcode_supported = opcode_supported_Prescott;
        model_info = &model_Prescott;
    } else if (op_cpu.get_value() == "Core2" || op_cpu.get_value() == "Merom") {
        opcode_supported = opcode_supported_Merom;
        model_info = &model_Merom;
    } else if (op_cpu.get_value() == "Penryn") {
        opcode_supported = opcode_supported_Penryn;
        model_info = &model_Penryn;
    } else if (op_cpu.get_value() == "Nehalem") {
        opcode_supported = opcode_supported_Nehalem;
        model_info = &model_Nehalem;
    } else if (op_cpu.get_value() == "Westmere") {
        opcode_supported = opcode_supported_Westmere;
        model_info = &model_Westmere;
    } else if (op_cpu.get_value() == "Sandybridge") {
        opcode_supported = opcode_supported_Sandybridge;
        model_info = &model_Sandybridge;
    } else if (op_cpu.get_value() == "Ivybridge") {
        opcode_supported = opcode_supported_Ivybridge;
        model_info = &model_Ivybridge;
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
}

} // namespace drcpusim
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    using ::dynamorio::droption::droption_parser_t;
    using ::dynamorio::droption::DROPTION_SCOPE_ALL;
    using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;

    dr_set_client_name("DynamoRIO CPU Simulator", "http://dynamorio.org/issues");

#ifdef WINDOWS
    dr_enable_console_printing();
#endif

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err,
                                       NULL)) {
        NOTIFY(0, "Usage error: %s\nUsage:\n%s", parse_err.c_str(),
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }
    if (dynamorio::drcpusim::op_cpu.get_value().empty()) {
        NOTIFY(0, "Usage error: cpu is required\nUsage:\n%s",
               droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        dr_abort();
    }

    dynamorio::drcpusim::set_opcode_and_model();

    if (!dynamorio::drcpusim::op_blocklist.get_value().empty()) {
        std::stringstream stream(dynamorio::drcpusim::op_blocklist.get_value());
        std::string entry;
        while (std::getline(stream, entry, ':'))
            dynamorio::drcpusim::blocklist.push_back(entry);
    }

    if (dynamorio::drcpusim::op_ignore_all_libs.get_value()) {
        module_data_t *exe = dr_get_main_module();
        DR_ASSERT(exe != NULL);
        dynamorio::drcpusim::exe_start = exe->start;
        dr_free_module_data(exe);
    }

    if (!drmgr_init())
        DR_ASSERT(false);

    /* register events */
    dr_register_exit_event(dynamorio::drcpusim::event_exit);
    if (!drmgr_register_bb_instrumentation_event(
            NULL, dynamorio::drcpusim::event_app_instruction, NULL))
        DR_ASSERT(false);
}
