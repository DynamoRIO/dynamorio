#!/bin/env python3
# **********************************************************
# Copyright (c) 2022 Rivos, Inc.    All rights reserved.
# **********************************************************

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of Rivos, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# This script generates encoder/decoder logic for RISC-V instructions described
# in core/arch/riscv64/isl/*.txt files. This includes:
# - opcode_api.h - List of all opcodes supported by the encoder/decoder
# - opcode_names.h - Stringified names of all opcodes.
# - instr_create_api.h - Instruction generation macros.
# - FIXME i#3544: add a list of generated files here.
#
# FIXME i#3544: To be done:
# - Generate instruction encoding (at format + fields level?).
# - Gather statistics on instruction usage frequency. If there is a small set
#   of instructions which are used most frequently, then having an
#   instruction-specific [de]ncoder will utilize branch predictor better and not
#   cause dramatically higher instruction cache polution than a per-format
#   decoding functions with per-field branches.
# - ...

from logging import DEBUG, StreamHandler, getLogger, Formatter, Handler
from os import getenv, path
from pathlib import Path
import re
from readline import insert_text
from sys import argv
from typing import List
from enum import Enum, unique

# Logging helpers. To enable debug logging set DEBUG=1 environment variable
logger = getLogger('rv64-codec')
log_handler = StreamHandler()
log_handler.setFormatter(Formatter('%(levelname)s: %(message)s'))
logger.addHandler(log_handler)


def dbg(msg, *args, **kwargs):
    logger.debug(msg, *args, **kwargs)


def wrn(msg, *args, **kwargs):
    logger.warning(msg, *args, **kwargs)


def nfo(msg, *args, **kwargs):
    logger.info(msg, *args, **kwargs)


def err(msg, *args, **kwargs):
    logger.error(msg, *args, **kwargs)


env_debug = getenv('DEBUG') or '0'
if (env_debug != '0'):
    logger.setLevel(DEBUG)


def maxu(bits: int) -> int:
    return (1 << bits) - 1


def ctz(n: int) -> int:
    sn = bin(n)
    return len(sn) - len(sn.rstrip('0'))


def cto(n: int) -> int:
    sn = bin(n)
    return len(sn) - len(sn.rstrip('1'))


@unique
class Field(Enum):
    # Fields in uncompressed instructions
    RD = 1
    RDFP = 2
    RS1 = 3
    RS1FP = 4
    BASE = 5
    RS2 = 6
    RS2FP = 7
    RS3 = 8
    FM = 9
    PRED = 10
    SUCC = 11
    AQRL = 12
    CSR = 13
    RM = 14
    SHAMT = 15
    SHAMT5 = 16
    SHAMT6 = 17
    I_IMM = 18
    S_IMM = 19
    B_IMM = 20
    U_IMM = 21
    J_IMM = 22
    IMM = 23  # Used only for parsing ISA files. Concatenated into V_RS1_DISP
    # Fields in compressed instructions
    CRD = 24
    CRDFP = 25
    CRS1 = 26
    CRS2 = 27
    CRS2FP = 28
    CRD_ = 29
    CRD_FP = 30
    CRS1_ = 31
    CRS2_ = 32
    CRS2_FP = 33
    CRD__ = 34
    CSHAMT = 35
    CSR_IMM = 36
    CADDI16SP_IMM = 37
    CLWSP_IMM = 38
    CLDSP_IMM = 39
    CLUI_IMM = 40
    CSWSP_IMM = 41
    CSDSP_IMM = 42
    CIW_IMM = 43
    CLW_IMM = 44
    CLD_IMM = 45
    CSW_IMM = 46
    CSD_IMM = 47
    CIMM5 = 48
    CB_IMM = 49
    CJ_IMM = 50
    # Virtual fields en/decoding special cases.
    V_L_RS1_DISP = 51
    V_S_RS1_DISP = 52

    def is_dest(self) -> bool:
        return self in [Field.RD, Field.RDFP, Field.CRD, Field.CRDFP,
                        Field.CRD_, Field.CRD_FP, Field.CRD__, Field.CSWSP_IMM, Field.CSDSP_IMM, Field.CSW_IMM, Field.CSD_IMM, Field.V_S_RS1_DISP]

    def __str__(self) -> str:
        return self.name.lower().replace("fp", "(fp)")

    def arg_name(self) -> str:
        if (self in [Field.RD, Field.RDFP, Field.CRD, Field.CRDFP,
                     Field.CRD_, Field.CRD_FP, Field.CRD__]):
            return "rd"
        if (self in [Field.RS1, Field.RS1FP, Field.CRS1, Field.CRS1_]):
            return "rs1"
        if (self in [Field.BASE]):
            return "base"
        if (self in [Field.RS2, Field.RS2FP, Field.CRS2, Field.CRS2FP,
                     Field.CRS2_, Field.CRS2_FP]):
            return "rs2"
        if (self in [Field.RS3]):
            return "rs3"
        if (self in [Field.FM]):
            return "fm"
        if (self in [Field.PRED]):
            return "pred"
        if (self in [Field.SUCC]):
            return "succ"
        if (self in [Field.AQRL]):
            return "aqrl"
        if (self in [Field.CSR]):
            return "csr"
        if (self in [Field.RM]):
            return "rm"
        if (self in [Field.SHAMT, Field.SHAMT5, Field.SHAMT6, Field.CSHAMT]):
            return "shamt"
        if (self in [Field.I_IMM, Field.CSR_IMM, Field.S_IMM, Field.B_IMM,
                     Field.U_IMM, Field.J_IMM, Field.CADDI16SP_IMM,
                     Field.CLUI_IMM, Field.CIW_IMM, Field.CIMM5, Field.CB_IMM,
                     Field.CJ_IMM]):
            return "imm"
        if (self in [Field.CSWSP_IMM, Field.CSDSP_IMM,
                     Field.CLWSP_IMM, Field.CLDSP_IMM]):
            return "sp_offset"
        if (self in [Field.CLD_IMM, Field.CLW_IMM, Field.CSD_IMM, Field.CSW_IMM,
                     Field.V_L_RS1_DISP, Field.V_S_RS1_DISP]):
            return "mem"
        return "<invalid>"

    def asm_name(self) -> str:
        if (self in [Field.CLD_IMM, Field.CLW_IMM, Field.CSD_IMM, Field.CSW_IMM,
                     Field.V_L_RS1_DISP, Field.V_S_RS1_DISP]):
            return "imm(rs1)"
        else:
            return self.arg_name()

    def formatted_name(self) -> str:
        return self.arg_name().capitalize()

    def from_str(fld: str):
        return Field[fld.upper().replace("(FP)", "FP")]

    def arg_comment(self) -> str:
        cmt_str = [
            '',
            'The output register (inst[11:7]).',
            'The output floating-point register (inst[11:7]).',
            'The first input register (inst[19:15]).',
            'The first input floating-point register (inst[19:15]).',
            'The `base` field in RISC-V Base Cache Management Operation ISA Extensions (inst[19:15]).',
            'The second input register (inst[24:20]).',
            'The second input floating-point register (inst[24:20]).',
            'The third input register (inst[31:27]).',
            'The fence semantics (inst[31:28]).',
            'The bitmap with predecessor constraints for FENCE (inst[27:24]).',
            'The bitmap with successor constraints for FENCE (inst[23:20]).',
            'The acquire-release constraint field (inst[26:25]).',
            'The Configuration/Status Register id (inst[31:20]).',
            'The rounding-mode (inst[14:12]).',
            'The `shamt` field (bit range is determined by XLEN).',
            'The `shamt` field that uses 5-bits.',
            'The `shamt` field that uses 6-bits.',
            'The immediate field in the I-type format.',
            'The immediate field in the S-type format.',
            'The immediate field in the B-type format.',
            'The 20-bit immediate field in the U-type format.',
            'The immediate field in the J-type format.',
            'The immediate field in PREFETCH instructions.',
            'The output register in `CR`, `CI` RVC formats (inst[11:7])',
            'The output floating-point register in `CR`, `CI` RVC formats (inst[11:7])',
            'The first input register in `CR`, `CI` RVC formats (inst[11:7]).',
            'The second input register in `CR`, `CSS` RVC formats (inst[6:2]).',
            'The second input floating-point register in `CR`, `CSS` RVC formats (inst[6:2]).',
            'The output register in `CIW`, `CL` RVC formats (inst[4:2])',
            'The output floating-point register in `CIW`, `CL` RVC formats (inst[4:2])',
            'The first input register in `CL`, `CS`, `CA`, `CB` RVC formats (inst[9:7]).',
            'The second input register in `CS`, `CA` RVC formats (inst[4:2]).',
            'The second input floating-point register in `CS`, `CA` RVC formats (inst[4:2]).',
            'The output register in `CA` RVC format (inst[9:7])',
            'The `shamt` field in the RVC format.',
            'The immediate field in a CSR instruction.',
            'The immediate field in a C.ADDI16SP instruction.',
            'The SP-relative memory location (sp+imm: imm & 0x3 == 0).',
            'The SP-relative memory location (sp+imm: imm & 0x7 == 0).',
            'The immediate field in a C.LUI instruction.',
            'The SP-relative memory location (sp+imm: imm & 0x3 == 0).',
            'The SP-relative memory location (sp+imm: imm & 0x7 == 0).',
            'The immediate field in a CIW format instruction.',
            'The register-relative memory location (reg+imm: imm & 0x3 == 0).',
            'The register-relative memory location (reg+imm: imm & 0x7 == 0).',
            'The register-relative memory location (reg+imm: imm & 0x3 == 0).',
            'The register-relative memory location (reg+imm: imm & 0x7 == 0).',
            'The immediate field in a C.ADDI, C.ADDIW, C.LI, and C.ANDI instruction.',
            'The immediate field in a a CB format instruction (C.BEQZ and C.BNEZ).',
            'The immediate field in a CJ format instruction.',
            'The register-relative memory source location (reg+imm).',
            'The register-relative memory target location (reg+imm).',
        ]
        return cmt_str[self.value]

    def size_str(self) -> int:
        '''
        Return DynamoRIO enum representing operand size.

        All register operands have a pointer (OPND_PTR) size.
        Immediate operand sizes depend on the instruction to match the assembly
        if possible. I.e.:
        - LUI's U_IMM operand is OPSZ_20b and not OPSZ_4 because assembly takes
          the upper 20 bits of the created immediate.
        - C.LD's CLD_IMM is OPSZ_1 and not OPSZ_5b because assembly takes the
          full 1-byte offset value, not the top 5 bits of it.
        - BLT's B_IMM is OPSZ_2 because the target address must be 2-byte
          aligned.

        This could be a separate enum but it seems like an overkill.
        '''
        sz_str = [
            'OPSZ_NA',  # NONE
            'OPSZ_PTR',  # RD
            'OPSZ_PTR',  # RDFP
            'OPSZ_PTR',  # RS1
            'OPSZ_PTR',  # RS1FP
            'OPSZ_0',  # BASE - 0 because cache-line is platform dependent.
            'OPSZ_PTR',  # RS2
            'OPSZ_PTR',  # RS2FP
            'OPSZ_PTR',  # RS3
            'OPSZ_4b',  # FM
            'OPSZ_4b',  # PRED
            'OPSZ_4b',  # SUCC
            'OPSZ_2b',  # AQRL
            'OPSZ_PTR',  # CSR
            'OPSZ_3b',  # RM
            'OPSZ_5b',  # SHAMT
            'OPSZ_6b',  # SHAMT5
            'OPSZ_7b',  # SHAMT6
            'OPSZ_12b',  # I_IMM
            'OPSZ_12b',  # S_IMM
            'OPSZ_2',  # B_IMM
            'OPSZ_20b',  # U_IMM
            'OPSZ_2',  # J_IMM
            'OPSZ_12b',  # IMM
            'OPSZ_PTR',  # CRD
            'OPSZ_PTR',  # CRDFP
            'OPSZ_PTR',  # CRS1
            'OPSZ_PTR',  # CRS2
            'OPSZ_PTR',  # CRS2FP
            'OPSZ_PTR',  # CRD_
            'OPSZ_PTR',  # CRD_FP
            'OPSZ_PTR',  # CRS1_
            'OPSZ_PTR',  # CRS2_
            'OPSZ_PTR',  # CRS2_FP
            'OPSZ_PTR',  # CRD__
            'OPSZ_6b',  # CSHAMT
            'OPSZ_5b',  # CSR_IMM
            'OPSZ_10b',  # CADDI16SP_IMM
            'OPSZ_1',  # CLWSP_IMM
            'OPSZ_9b',  # CLDSP_IMM
            'OPSZ_6b',  # CLUI_IMM
            'OPSZ_1',  # CSWSP_IMM
            'OPSZ_9b',  # CSDSP_IMM
            'OPSZ_10b',  # CIW_IMM
            'OPSZ_7b',  # CLW_IMM
            'OPSZ_1',  # CLD_IMM
            'OPSZ_7b',  # CSW_IMM
            'OPSZ_1',  # CSD_IMM
            'OPSZ_6b',  # CIMM5
            'OPSZ_2',  # CB_IMM
            'OPSZ_2',  # CJ_IMM
            'OPSZ_12b',  # V_L_RS1_DISP
            'OPSZ_12b',  # V_S_RS1_DISP
        ]
        return sz_str[self.value]


@unique
class Format(Enum):
    # Uncompressed instructions
    R = 1
    R4 = 2
    I = 3
    S = 4
    B = 5
    U = 6
    J = 7
    # Only compressed instructions below
    CR = 8
    CI = 9
    CSS = 10
    CIW = 11
    CL = 12
    CS = 13
    CA = 14
    CB = 15
    CJ = 16

    def __str__(self) -> str:
        return self.name.lower()


class Instruction:
    def __init__(self, name: str, fmt: str, mask: int, match: int, flds: List[str], ext: str) -> None:
        self.name: str = name
        self.fmt: Format = Format[fmt.upper()]
        self.mask: int = mask
        self.match: int = match
        self.flds: List[Field] = [
            Field.from_str(f) for f in flds if len(f) > 0]
        self.ext = ext

    def is_compressed(self) -> bool:
        return (self.match & 0b11) < 0b11

    def formatted_name(self) -> str:
        '''
        The formatted name will be used as the struct field name and enum variant.
        '''
        return self.name.lower().replace('.', "_")

    def formatted_ext(self) -> str:
        '''
        The formatted extension name used as an enum variant.
        '''
        return f'RISCV64_ISA_EXT_{self.ext.upper()}'

    def __str__(self) -> str:
        fields = ' '.join([str(f) for f in self.flds])
        sz = 15 if self.is_compressed() else 31
        bits = ''.join([str((self.match >> i) & 1) if (
            (self.mask >> i) & 1) == 1 else '.' for i in range(sz, -1, -1)])
        return f"{self.name} | {self.fmt} | {fields} | {bits} | {self.ext}"


class IslGenerator:
    # Offset into opcode enum () to the first instruction. Updated by
    # generate_opcodes().
    OP_TBL_OFFSET = -1

    class TrieNode:
        def __init__(self, mask: int, shift: int, index: int, ctx: str = ''):
            self.mask = mask
            self.shift = shift
            self.index = index
            self.ctx = ctx

    def __init__(self) -> None:
        self.instructions: List[Instruction] = []
        pass

    def __fixup_compressed_inst(self, inst: Instruction):
        opc = (inst.match & inst.mask) & 0x3
        funct3 = (inst.match & inst.mask) >> 13
        if (opc == 0 and funct3 not in [0, 0b100]):  # LOAD/STORE instructions
            dbg(f'{inst.name} {[f.name for f in inst.flds]}')
            # Immediate argument will handle the base+disp
            inst.flds.pop(1)
            dbg(f'{" " * len(inst.name)} {[f.name for f in inst.flds]}')

    def __fixup_uncompressed_inst(self, inst: Instruction):
        opc = (inst.match & inst.mask) & 0x7F
        if (opc in [0b0000011, 0b0000111]):  # LOAD instructions
            dbg(f'{inst.name} {[f.name for f in inst.flds]}')
            inst.flds[0] = Field.V_L_RS1_DISP
            inst.flds.pop(1)
            dbg(f'{" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif (opc in [0b0100011, 0b0100111]):  # STORE instructions
            dbg(f'{inst.name} {[f.name for f in inst.flds]}')
            inst.flds[0] = Field.V_S_RS1_DISP
            inst.flds.pop(2)
            dbg(f'{" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif (inst.mask == 0x1f07fff and inst.match in [0x6013, 0x106013, 0x306013]):
            # prefetch.[irw] instructions
            dbg(f'{inst.name} {[f.name for f in inst.flds]}')
            inst.flds[0] = Field.V_S_RS1_DISP
            inst.flds.pop(1)
            dbg(f'{" " * len(inst.name)} {[f.name for f in inst.flds]}')
        # FIXME i#3544: Should we fixup the 4B wide NOP (00000013) for the sake
        # of disassembly? Though it might cause issues in encoding because we'd
        # need an extra entry in the instr_infos table since NOP aliases with
        # ADDI (it's an alias).

    def __detect_virt_fields(self):
        for inst in self.instructions:
            if (inst.is_compressed()):
                self.__fixup_compressed_inst(inst)
            else:
                self.__fixup_uncompressed_inst(inst)

    def __parse_isl_file(self, isl_file: Path) -> bool:
        '''
        Parse a single Instruction Set Listing file.

        The file base name is interpreted as an ISA extension name (including the
        base ISA).

        Returns true if parsing succeeded.'''
        ext = path.basename(isl_file).removesuffix('.txt')
        with open(isl_file, 'r') as f:
            for line in f:
                line = line.split("#")[0].strip()
                if (len(line) == 0):
                    continue
                tokens = [t.strip() for t in line.split('|')]
                assert (len(tokens) == 4)
                name = tokens[0]
                fmt = tokens[1]
                flds = tokens[2].strip().split(' ')
                mask = 0
                match = 0
                shift = len(tokens[3])
                if (name.startswith("c.") or name == "unimp"):
                    if (shift != 16):
                        err(
                            f"Invalid compressed instruction size: {name} {shift} != 16")
                        return False
                elif shift != 32:
                    err(
                        f"Invalid uncompressed instruction size: {name} {shift} != 32")
                    return False
                shift -= 1
                for c in tokens[3]:
                    if c == '.':
                        mask |= 0 << shift
                        match |= 0 << shift
                    elif c == '1':
                        mask |= 1 << shift
                        match |= 1 << shift
                    elif c == '0':
                        mask |= 1 << shift
                        match |= 0 << shift
                    else:
                        err(f"invalid mask for {name}: {tokens[3]}")
                        return False
                    shift -= 1
                self.instructions.append(
                    Instruction(name, fmt, mask, match, flds, ext))

        return True

    def __sanity_check(self) -> bool:
        '''
        Ensure there are no duplicates among uncompressed instructions.

        Note that compressed instructions can be duplicated and require
        validation taking the ISA + operand levels in mind.
        '''
        inst_dict = {}
        for i in self.instructions:
            id = inst_dict.get(i.match)
            if (id and not i.is_compressed() and id.mask == i.mask):
                err(f'Duplicate instruction: {i}')
                return False
            inst_dict[i.match] = i
        return True

    def parse_isl(self, isl_path) -> bool:
        '''
        Parse Instruction Set Listing files into the generator object.

        Iterates over all .txt files in a given path assuming each file contains
        instructions from a single ISA extension (or base ISA).

        Returns true if parsing of all files succeeded.
        '''
        p = Path(isl_path)
        res = True
        for f in [f for f in p.iterdir() if f.name.endswith(".txt")]:
            res = res and self.__parse_isl_file(f)
            if (not res):
                break
        self.__detect_virt_fields()
        return res and self.__sanity_check()

    def generate_opcodes(self, template_file, out_file) -> bool:
        '''
        Generate the opcode_api.h header file.

        Returns true if the output file was written successfully.
        '''
        tmpl_fld = '@OPCODES@'
        if len(self.instructions) == 0:
            return False
        found_template_fld = False
        idx = -1
        # Replace tmpl_fld in the template_file and save as out_file
        with open(template_file, 'r') as tf, open(out_file, 'w') as of:
            for line in tf:
                if ('*/ OP_' in line):
                    nmb = re.findall(r'\d+', line)
                    if (len(nmb) != 1):
                        wrn(
                            f"Template opcode line is missing an index?: '{line}'")
                    idx = int(nmb[0])
                elif (tmpl_fld in line):
                    found_template_fld = True

                    if (idx == -1):
                        wrn("Starting index not found, using 0")
                    idx += 1
                    IslGenerator.OP_TBL_OFFSET = idx
                    lines = []
                    # Generate list of:
                    #    OP_<opcode>,   /**< <extension> <opcode> opcode. */
                    for i in self.instructions:
                        lines.append(
                            f"    /* {idx:3d} */ OP_{i.formatted_name()} = {idx},   /**< {i.ext} {i.name} opcode. */")
                        idx += 1

                    line = line.replace(tmpl_fld, '\n'.join(lines))
                of.write(line)
        if (not found_template_fld):
            err(f"{tmpl_fld} not found in {template_file}")
        return found_template_fld

    def generate_instr_macros(self, template_file, out_file) -> bool:
        '''
        Generate the instr_create_api.h header file.

        Returns true if the output file was written successfully.'''
        tmpl_fld = '@INSTR_MACROS@'
        if len(self.instructions) == 0:
            return False
        found_template_fld = False
        # Replace tmpl_fld in the template_file and save as out_file
        with open(template_file, 'r') as tf, open(out_file, 'w') as of:
            for line in tf:
                if (tmpl_fld in line):
                    found_template_fld = True

                    lines = []
                    # Generate list of:
                    #    #define INSTR_CREATE_<opcode>(dc, <arguments>) \
                    #      instr_create_<n_dst>dst_<n_src>src(dc, OP_<opcode>, <arguments>)
                    for i in self.instructions:
                        flds = [f for f in i.flds]
                        flds.reverse()
                        args = ''
                        arg_comments = ''
                        if len(flds) > 0:
                            args += ', '
                            args += ', '.join([f.formatted_name()
                                              for f in flds])
                            arg_comments += '\n'
                            arg_comments += '\n'.join(
                                [f' * \param {f.formatted_name():6}  {f.arg_comment()}' for f in flds])
                        nd = len([f for f in flds if f.is_dest()])
                        ns = len(flds) - nd
                        lines.append(
                            f'''/**
 * Creates a {i.name} instruction.
 *
 * \param dc      The void * dcontext used to allocate memory for the instr_t.{arg_comments}
 */
#define INSTR_CREATE_{i.formatted_name()}(dc{args}) \\
    instr_create_{nd}dst_{ns}src(dc, OP_{i.formatted_name()}{args})\n''')
                    line = line.replace(tmpl_fld, '\n'.join(lines))
                of.write(line)
        if (not found_template_fld):
            err(f"{tmpl_fld} not found in {template_file}")
        return found_template_fld

    def construct_trie(self, op_offset) -> List[TrieNode]:
        '''
        Construct a trie lookup array for parsed non-compressed instructions.

        Compressed instructions are left out because their encoding can alias
        with non-compressed instructions. Therefore compressed instructions will
        be handled in a separate decode/encode function.
        '''
        trie: List[self.TrieNode] = []
        trie_index = 0
        trie_buckets: dict[int: List[Instruction]] = {
            0: [i for i in self.instructions if not i.is_compressed()]
        }
        # FIXME i#3544: There is an issue with the current construction
        # algorithm for instructions which may alias other instructions, i.e.
        # prefetch.[irw] is encoded as ori with rd=0. So we need to change the
        # prefix creation mechanism here.

        dbg(f'{len(trie_buckets[0])} instructions in total.')
        trie.append(self.TrieNode(0x7f, 0, 1))
        while trie_index < len(trie):
            instructions = trie_buckets.get(trie_index)
            if (instructions is None):
                trie_index += 1
                continue
            dbg(f'trie_buckets[{trie_index}]:len({len(instructions)}):'
                f' {[i.name for i in instructions]}')
            mask = trie[trie_index].mask
            shift = trie[trie_index].shift
            if (mask == 0):  # This denotes a leaf node.
                trie_index += 1
                continue
            bucket_size = mask + 1
            dbg(f'mask:        {mask:032b}')
            dbg(f'shift:       {shift}')
            dbg(f'bucket size: {bucket_size}')
            buckets: List[List[Instruction]] = [[]
                                                for i in range(0, bucket_size)]
            # Each trie bucket contains a list of instructions sharing
            # (inst.match >> shift) & mask, where mask is a continuous set of 1
            # bits.
            # There is a special case when trie creation algorithm detects
            # aliased instructions (that is having different inst.mask but the
            # ame inst.match - i.e. ori and prefetch.[irw]). In that case bucket
            # will contain exact match instructions (inst.mask == mask) and ones
            # which may have any other value across the bucket's mask.
            non_exact_match = []
            for instruction in instructions:
                imatch = (instruction.match >> shift) & mask
                imask = (instruction.mask >> shift) & mask
                assert (imatch < bucket_size)
                # First all exact-match instructions need to be put into their
                # search bucket.
                if (mask == imask):
                    buckets[imatch].append(instruction)
                # Non-exact match instructions will be put in all other search
                # buckets after we know all the positions of exact-match
                # buckets.
                else:
                    non_exact_match.append(instruction)
            # Finally append the non-exact matches.
            if (len(non_exact_match) > 0):
                for bucket in buckets:
                    if (len(bucket) == 0):
                        bucket += non_exact_match
            trie[trie_index].index = len(trie)
            idx = -1
            for bucket in buckets:
                idx += 1
                l = len(bucket)
                if (l != 0):
                    dbg(f'buckets[(i >> {shift}) & 0b{buckets.index(bucket):b}]: \n  ' +
                        '\n  '.join(
                            [f'{i.name:16s}: {i.match:032b} & {i.mask:032b}'
                             for i in bucket]))
                if (l == 0):
                    trie.append(self.TrieNode(0, 0, maxu(16)))
                elif (l == 1):
                    name = bucket[0].name
                    wanted = [i for i in self.instructions if i.name == name][0]
                    wi = op_offset + self.instructions.index(wanted)
                    trie.append(self.TrieNode(0, 0, wi, name))
                else:
                    mask_anded = maxu(32)
                    common_match = maxu(32)
                    common_comparator = bucket[0].match
                    for instruction in bucket:
                        mask_anded &= instruction.mask
                        common_match &= ~(common_comparator ^
                                          instruction.match) & maxu(32)
                        common_comparator &= common_match
                    if (mask_anded & ~common_match & maxu(32) == 0):
                        # If mask is 0, it means we've reached the end of the
                        # decoded instruction but there are still more than 1
                        # elements in the search bucket. This means the bucket
                        # contains aliased instructions. In this case we need
                        # to fixup the mask so that it points to the place where
                        # masks differ between instructions. The rest of the
                        # handling is done above during search bucket list
                        # creation.
                        dbg('  Bucket with aliased instructions, mask fixed up!')
                        mask_anded = ~mask_anded & maxu(32)
                    else:
                        mask_anded &= ~common_match & maxu(32)
                    next_shift = ctz(mask_anded)
                    next_mask = (1 << cto(mask_anded >> next_shift)) - 1
                    next_trie_index = len(trie)
                    dbg(f'  trie[{next_trie_index}] <- (i >> {next_shift}) & 0b{next_mask:b}')
                    trie.append(self.TrieNode(next_mask, next_shift, 0))
                    trie_buckets[next_trie_index] = bucket
            trie_index += 1
        return trie

    def generate_instr_info_trie(self, out_file, op_offset) -> bool:
        '''
        Generate the instr_info_trie.c file.

        Returns true if the output file was written successfully.'''
        if (op_offset < 0):
            err("Invalid OP_* offset")
            return False
        if len(self.instructions) == 0:
            return False
        with open(out_file, 'w') as of:
            # Keep the order of fields here in sync with the documentation in
            # core/ir/riscv64/codec.c
            OPND_TGT = ['dst1', 'src1', 'src2', 'src3', 'dst2']
            instr_infos = []
            trie = []
            # Generate rv_instr_info_t list
            for i in self.instructions:
                flds = [f for f in i.flds]
                flds.reverse()
                asm_args = ''
                opnds = []
                ndst = 0
                nsrc = 0
                if len(flds) > 0:
                    asm_args += ' '
                    asm_args += ', '.join([f.asm_name() for f in flds])
                    isrc = 1
                    for f in flds:
                        if f.is_dest():
                            oidx = 0
                            ndst += 1
                        else:
                            oidx = isrc
                            isrc += 1
                            nsrc += 1
                        opnds += f'''
            .{OPND_TGT[oidx]}_type = RISCV64_FLD_{f.name},
            .{OPND_TGT[oidx]}_size = {f.size_str()},'''
                instr_infos.append(f'''[OP_{i.formatted_name()}] = {{ /* {i.name}{asm_args} */
        .nfo = {{
            .type = OP_{i.formatted_name()},
            .opcode = 0x{(ndst << 31) | (nsrc << 28):08x}, /* {ndst} dst, {nsrc} src */
            .name = "{i.name}",{''.join(opnds)}
            .code = (((uint64){hex(i.match)}) << 32) | ({hex(i.mask)}),
        }},
        .ext = {i.formatted_ext()},
    }},''')
            # Generate trie
            trie = self.construct_trie(op_offset)
            trie = [
                f'{{.mask = {hex(t.mask)}, .shift = {t.shift}, .index = {t.index}}},{f" /* {t.ctx} */" if len(t.ctx) > 0 else ""}' for t in trie]
            instr_infos = '\n    '.join(instr_infos)
            trie = '\n    '.join(trie)
            of.write(f'''
/* This file is generated by codec.py. */

/** Instruction info array. */
rv_instr_info_t instr_infos[] = {{
    {instr_infos}
}};

/** Trie lookup structure. */
trie_node_t instr_infos_trie[] = {{
    {trie}
}};
''')
        return True


OP_FNAME = "opcode_api.h"
INSTM_FNAME = "instr_create_api.h"
INSTRINFO_FNAME = "instr_info_trie.h"

if __name__ == '__main__':
    if (len(argv) < 4):
        wrn(
            f"Usage: {path.basename(__file__)} <isl-path> <template-path> <output-path>")
        exit(1)
    isl_path = argv[1]
    template_path = argv[2]
    out_path = argv[3]
    generator = IslGenerator()

    res = generator.parse_isl(isl_path)
    if (not res):
        err("Failed to parse Instruction Set Listings")
        exit(2)

    dbg("Parsed instructions:")
    for i in generator.instructions:
        dbg(f"  {i}")

    res = generator.generate_opcodes(path.join(
        template_path, f"{OP_FNAME}.in"), path.join(out_path, OP_FNAME))
    if (not res):
        err(f"Failed to generate {OP_FNAME}")
        exit(3)

    res = generator.generate_instr_macros(path.join(
        template_path, f"{INSTM_FNAME}.in"), path.join(out_path, INSTM_FNAME))
    if (not res):
        err(f"Failed to generate {INSTM_FNAME}")
        exit(4)

    res = generator.generate_instr_info_trie(
        path.join(out_path, INSTRINFO_FNAME), IslGenerator.OP_TBL_OFFSET)
    if (not res):
        err(f"Failed to generate {INSTRINFO_FNAME}")
        exit(4)
