/* **********************************************************
 * Copyright (c) 2020-2025 Google, Inc.  All rights reserved.
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

#define DR_FAST_IR 1
#include "dr_api.h"
#include "droption.h"
#include <iostream>
#include <sstream>

namespace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

// XXX i#1684: We want cross-arch decoding support so a single build can decode
// AArchXX and x86.  For now, a separate build is needed.
// XXX i#4021: -syntax option not yet supported on ARM.
#ifdef X86
droption_t<std::string>
    op_mode(DROPTION_SCOPE_FRONTEND, "mode",
#    ifdef X86_64
            "x64",
#    else
            "x86",
#    endif
            "Decodes using the specified mode: 'x64', 'x86', or 'regdeps'.",
            "Decodes using the specified mode: 'x64', 'x86', or 'regdeps' ('x64' only "
            "supported in 64-bit builds).");
droption_t<std::string>
    op_syntax(DROPTION_SCOPE_FRONTEND, "syntax", "",
              "Uses the specified syntax: 'intel', 'att' or 'dr'.",
              "Uses the specified syntax: 'intel', 'att' or 'dr'. Defaults to 'intel' "
              "for 'x64' or 'x86' modes; is always 'dr' for 'regdeps' mode.");
#elif defined(ARM)
droption_t<std::string>
    op_mode(DROPTION_SCOPE_FRONTEND, "mode", "arm",
            "Decodes using the specified mode: 'arm', 'thumb', or 'regdeps'.",
            "Decodes using the specified mode: 'arm', 'thumb', or 'regdeps'.");
#elif defined(AARCH64)
droption_t<std::string>
    op_mode(DROPTION_SCOPE_FRONTEND, "mode", "aarch64",
            "Decodes using the specified mode: 'aarch64' or 'regdeps'.",
            "Decodes using the specified mode: 'aarch64' or 'regdeps'.");
droption_t<unsigned int>
    op_vl(DROPTION_SCOPE_FRONTEND, "vl", 128,
          "Sets the SVE vector length to one of: 128 256 384 512 640 768 896 1024 "
          "1152 1280 1408 1536 1664 1792 1920 2048.",
          "Sets the SVE vector length to one of: 128 256 384 512 640 768 896 1024 "
          "1152 1280 1408 1536 1664 1792 1920 2048.");
#elif defined(RISCV64)
droption_t<std::string>
    op_mode(DROPTION_SCOPE_FRONTEND, "mode", "riscv64",
            "Decodes using the specified mode: 'riscv64' or 'regdeps'.",
            "Decodes using the specified mode: 'riscv64' or 'regdeps'.");
droption_t<unsigned int>
    op_vl(DROPTION_SCOPE_FRONTEND, "vl", 128,
          "Sets the RVV vector length from 64 to 65536 in the power of 2.",
          "Sets the RVV vector length from 64 to 65536 in the power of 2.");
#else
#    error Unsupported ISA.
#endif

droption_t<bool> op_show_bytes(DROPTION_SCOPE_FRONTEND, "show_bytes", true,
                               "Display the instruction encoding bytes.",
                               "Display the instruction encoding bytes.");

#if defined(AARCH64) || defined(ARM) || defined(RISCV64)
#    define MAX_INSTR_LENGTH 4
#else
#    define MAX_INSTR_LENGTH 17
#endif

static bool
parse_bytes(std::string token, std::vector<byte> &bytes)
{
    // Assume everything is hex even if it has no leading 0x or \x.
    // Assume that values larger than one byte are machine words in
    // little-endian form, which we want to split into bytes in the endian order.
    // (This is how aarchxx encodings are always represented; for x86, this is
    // the format for raw data obtained from od or gdb or a binary file.)
    uint64 entry;
    std::stringstream stream;
    size_t digits = token.size();
    if (digits > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
        digits -= 2;
    stream << std::hex << token;
    if (!(stream >> entry))
        return false;
    for (unsigned int i = 0; i < digits / 2; ++i) {
        bytes.push_back(entry & 0xff);
        entry >>= 8;
    }
    return true;
}
}; // namespace

int
main(int argc, const char *argv[])
{
    int last_index;
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, argv, &parse_err,
                                       &last_index)) {
        std::cerr << "Usage error: " << parse_err << "\nUsage:\n " << argv[0]
                  << " [options] <hexadecimal bytes to decode as args or stdin>\n"
                  << "Bytes do not need leading 0x.  Single-token multi-byte values are "
                  << "assumed to be little-endian words.\n"
                  << "Options:\n"
                  << droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        return 1;
    }

    void *dcontext = GLOBAL_DCONTEXT;

    // Set the default syntax based on build architecture.
    dr_disasm_flags_t syntax =
#ifdef X86
        DR_DISASM_INTEL
#elif defined(RISCV64)
        DR_DISASM_RISCV
#else
        DR_DISASM_DR
#endif
        ;

    // Set the ISA mode if supplied. Override the default syntax to become DR_DISASM_DR
    // when the user asks for 'regdeps'.
    if (!op_mode.get_value().empty()) {
#ifdef X86
        dr_isa_mode_t mode = DR_ISA_AMD64;
        if (op_mode.get_value() == "x86")
            mode = DR_ISA_IA32;
#    ifdef X86_64
        else if (op_mode.get_value() == "x64")
            mode = DR_ISA_AMD64;
#    endif
        else if (op_mode.get_value() == "regdeps") {
            mode = DR_ISA_REGDEPS;
            syntax = DR_DISASM_DR;
        }
#elif defined(ARM)
        dr_isa_mode_t mode = DR_ISA_ARM_A32;
        if (op_mode.get_value() == "arm")
            mode = DR_ISA_ARM_A32;
        else if (op_mode.get_value() == "thumb")
            mode = DR_ISA_ARM_THUMB;
        else if (op_mode.get_value() == "regdeps")
            mode = DR_ISA_REGDEPS;
#elif defined(AARCH64)
        dr_isa_mode_t mode = DR_ISA_ARM_A64;
        if (op_mode.get_value() == "aarch64")
            mode = DR_ISA_ARM_A64;
        else if (op_mode.get_value() == "regdeps")
            mode = DR_ISA_REGDEPS;
#elif defined(RISCV64)
        dr_isa_mode_t mode = DR_ISA_RV64;
        if (op_mode.get_value() == "riscv64")
            mode = DR_ISA_RV64;
        else if (op_mode.get_value() == "regdeps") {
            mode = DR_ISA_REGDEPS;
            syntax = DR_DISASM_DR;
        }
#else
#    error Unsupported ISA
#endif
        else {
            std::cerr << "Unknown mode '" << op_mode.get_value() << "'\n";
            return 1;
        }
        if (!dr_set_isa_mode(dcontext, mode, NULL)) {
            std::cerr << "Failed to set ISA mode.\n";
            return 1;
        }
    }

#if defined(AARCH64) || defined(RISCV64)
    dr_set_vector_length(op_vl.get_value());
#endif

    // XXX i#4021: arm not yet supported.
#ifdef X86
    // Set the syntax if supplied.
    if (!op_syntax.get_value().empty()) {
        if (op_syntax.get_value() == "intel") {
            syntax = DR_DISASM_INTEL;
            if (op_mode.get_value() == "regdeps") {
                std::cerr << "'regdeps' mode does not support 'intel' syntax\n";
                return 1;
            }
        } else if (op_syntax.get_value() == "att") {
            syntax = DR_DISASM_ATT;
            if (op_mode.get_value() == "regdeps") {
                std::cerr << "'regdeps' mode does not support 'intel' syntax\n";
                return 1;
            }
        } else if (op_syntax.get_value() == "dr")
            syntax = DR_DISASM_DR;
        else {
            std::cerr << "Unknown syntax '" << op_syntax.get_value() << "'\n";
            return 1;
        }
    }
#endif
    disassemble_set_syntax(syntax);

    // Turn the arguments into a series of hex values.
    std::vector<byte> bytes;
    for (int i = last_index; i < argc; ++i) {
        if (!parse_bytes(argv[i], bytes)) {
            std::cerr << "failed to parse '" << argv[i] << "' as a hexadecimal number\n";
            return 1;
        }
    }

    // Process stdin if there are no arguments.
    if (last_index == argc) {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::stringstream tokenize(line);
            std::string token;
            while (tokenize >> token) {
                if (!parse_bytes(token, bytes)) {
                    std::cerr << "failed to parse '" << token
                              << "' as a hexadecimal number\n";
                    return 1;
                }
            }
        }
    }

    if (bytes.empty()) {
        std::cerr << "no bytes specified to disassemble\n";
        return 1;
    }

    size_t data_size = bytes.size();
    // Now allocate a "redzone" to avoid DR's decoder going off the end of the
    // buffer.
    for (int i = 0; i < MAX_INSTR_LENGTH; ++i)
        bytes.push_back(0);
    byte *pc = &bytes[0];
    byte *stop_pc = &bytes[data_size - 1];
    while (pc <= stop_pc) {
        // Check ahead of time to see whether this instruction enters the redzone
        // (or, we could disassemble into a buffer and check before printing it).
        if (pc + decode_sizeof(dcontext, pc, NULL _IF_X86_64(NULL)) > stop_pc + 1) {
            std::cerr << "disassembly failed: invalid instruction: not enough bytes:";
            for (; pc <= stop_pc; ++pc)
                std::cerr << " 0x" << std::hex << static_cast<int>(*pc);
            std::cerr << "\n";
            break;
        }
        pc =
            disassemble_with_info(dcontext, pc, STDOUT, false, op_show_bytes.get_value());
        if (pc == NULL) {
            std::cerr << "disassembly failed: invalid instruction\n";
            break;
        }
    }

    return 0;
}
