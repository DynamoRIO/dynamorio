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
# in core/ir/riscv64/isl/*.txt files. This includes:
# - opcode_api.h - List of all opcodes supported by the encoder/decoder.
# - instr_create_api.h - Instruction generation macros.
# - instr_info_trie.h - Instruction info table and a lookup trie for known instructions.
#
# FIXME i#3544: To be done:
# - Generate instruction encoding (at format + fields level?).
# - Gather statistics on instruction usage frequency. If there is a small set
#   of instructions which are used most frequently, then having an
#   instruction-specific [de]ncoder will utilize branch predictor better and not
#   cause dramatically higher instruction cache pollution than per-format
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
from io import StringIO

# Logging helpers. To enable debug logging set DEBUG=1 environment variable.
logger = getLogger('rv64-codec')
log_handler = StreamHandler()
log_handler.setFormatter(Formatter('%(levelname)s: %(message)s'))
logger.addHandler(log_handler)


def dbg(msg, *args, **kwargs):
    logger.debug(msg, *args, **kwargs)


def warn(msg, *args, **kwargs):
    logger.warning(msg, *args, **kwargs)


def info(msg, *args, **kwargs):
    logger.info(msg, *args, **kwargs)


def err(msg, *args, **kwargs):
    logger.error(msg, *args, **kwargs)


env_debug = getenv('DEBUG') or '0'
if env_debug != '0':
    logger.setLevel(DEBUG)


def max_unsigned(bits: int) -> int:
    return (1 << bits) - 1


def count_trailing_zeros(n: int) -> int:
    sn = bin(n)
    return len(sn) - len(sn.rstrip('0'))


def count_trailing_ones(n: int) -> int:
    sn = bin(n)
    return len(sn) - len(sn.rstrip('1'))

def write_if_changed(file, data):
    try:
        if open(file, 'r').read() == data:
            return
    except IOError:
        pass
    open(file, 'w').write(data)

class Field(str, Enum):
    arg_name: str
    _asm_name: str
    arg_cmt: str
    # The OPSZ_* definition is either a string in case operand size is common
    # across instructions or a dictionary indexed by the instruction name. A
    # special entry at key '' is used for instructions not found in the
    # dictionary.
    opsz_def: dict[str, str] | str
    is_dest: bool

    def __new__(cls, value: int, arg_name: str, is_dest: bool,
                opsz_def: dict[str, str] | str, asm_name: str,
                arg_cmt: str):
        # Take str as a base object because we need a concrete class. It won't
        # be used anyway.
        obj = str.__new__(cls, str(value))
        obj._value_ = value
        obj.arg_name = arg_name
        obj.opsz_def = opsz_def
        obj._asm_name = asm_name if asm_name != '' else obj.arg_name
        obj.arg_cmt = arg_cmt
        obj.is_dest = is_dest
        return obj

    # Fields in uncompressed instructions.
    RD = (1,
          'rd',
          True,
          'OPSZ_PTR',
          '',
          'The output register (inst[11:7]).'
          )
    RDFP = (2,
            'rd',
            True,
            'OPSZ_PTR',
            '',
            'The output floating-point register (inst[11:7]).'
            )
    RS1 = (3,
           'rs1',
           False,
           'OPSZ_PTR',
           '',
           'The first input register (inst[19:15]).'
           )
    RS1FP = (4,
             'rs1',
             False,
             'OPSZ_PTR',
             '',
             'The first input floating-point register (inst[19:15]).'
             )
    BASE = (5,
            'base',
            False,
            'OPSZ_0',
            '',
            'The `base` field in RISC-V Base Cache Management Operation ISA Extensions (inst[19:15]).'
            )
    RS2 = (6,
           'rs2',
           False,
           'OPSZ_PTR',
           '',
           'The second input register (inst[24:20]).'
           )
    RS2FP = (7,
             'rs2',
             False,
             'OPSZ_PTR',
             '',
             'The second input floating-point register (inst[24:20]).'
             )
    RS3FP = (8,
           'rs3',
           False,
           'OPSZ_PTR',
           '',
           'The third input register (inst[31:27]).'
           )
    FM = (9,
          'fm',
          False,
          'OPSZ_4b',
          '',
          'The fence semantics (inst[31:28]).'
          )
    PRED = (10,
            'pred',
            False,
            'OPSZ_4b',
            '',
            'The bitmap with predecessor constraints for FENCE (inst[27:24]).'
            )
    SUCC = (11,
            'succ',
            False,
            'OPSZ_4b',
            '',
            'The bitmap with successor constraints for FENCE (inst[23:20]).'
            )
    AQRL = (12,
            'aqrl',
            False,
            'OPSZ_2b',
            '',
            'The acquire-release constraint field (inst[26:25]).'
            )
    CSR = (13,
           'csr',
           False,
           'OPSZ_PTR',
           '',
           'The configuration/status register id (inst[31:20]).'
           )
    RM = (14,
          'rm',
          False,
          'OPSZ_3b',
          '',
          'The rounding-mode (inst[14:12]).'
          )
    SHAMT = (15,
             'shamt',
             False,
             'OPSZ_5b',
             '',
             'The `shamt` field (bit range is determined by XLEN).'
             )
    SHAMT5 = (16,
              'shamt',
              False,
              'OPSZ_6b',
              '',
              'The `shamt` field that uses only 5 bits.'
              )
    SHAMT6 = (17,
              'shamt',
              False,
              'OPSZ_7b',
              '',
              'The `shamt` field that uses only 6 bits.'
              )
    I_IMM = (18,
             'imm',
             False,
             'OPSZ_12b',
             '',
             'The immediate field in the I-type format.'
             )
    S_IMM = (19,
             'imm',
             False,
             'OPSZ_12b',
             '',
             'The immediate field in the S-type format.'
             )
    B_IMM = (20,
             'pc_rel',
             False,
             'OPSZ_2',
             '',
             'The immediate field in the B-type format.'
             )
    U_IMM = (21,
             'imm',
             False,
             'OPSZ_20b',
             '',
             'The 20-bit immediate field in the U-type format.'
             )
    U_IMMPC = (22,
               'imm',
               False,
               'OPSZ_20b',
               '',
               'The 20-bit immediate field in the U-type format (PC-relative).'
               )
    J_IMM = (23,
             'pc_rel',
             False,
             'OPSZ_2',
             '',
             'The immediate field in the J-type format.'
             )
    IMM = (24,  # Used only for parsing ISA files. Concatenated into V_RS1_DISP.
           'imm',
           False,
           'OPSZ_12b',
           '',
           'The immediate field in PREFETCH instructions.'
           )
    CRD = (25,
           'rd',
           True,
           'OPSZ_PTR',
           '',
           'The output register in `CR`, `CI` RVC formats (inst[11:7])'
           )
    CRDFP = (26,
             'rd',
             True,
             'OPSZ_PTR',
             '',
             'The output floating-point register in `CR`, `CI` RVC formats (inst[11:7])'
             )
    CRS1 = (27,
            'rs1',
            False,
            'OPSZ_PTR',
            '',
            'The first input register in `CR`, `CI` RVC formats (inst[11:7]).'
            )
    CRS2 = (28,
            'rs2',
            False,
            'OPSZ_PTR',
            '',
            'The second input register in `CR`, `CSS` RVC formats (inst[6:2]).'
            )
    CRS2FP = (29,
              'rs2',
              False,
              'OPSZ_PTR',
              '',
              'The second input floating-point register in `CR`, `CSS` RVC formats (inst[6:2]).'
              )
    # Fields in compressed instructions.
    CRD_ = (30,
            'rd',
            True,
            'OPSZ_PTR',
            '',
            'The output register in `CIW`, `CL` RVC formats (inst[4:2])'
            )
    CRD_FP = (31,
              'rd',
              True,
              'OPSZ_PTR',
              '',
              'The output floating-point register in `CIW`, `CL` RVC formats (inst[4:2])'
              )
    CRS1_ = (32,
             'rs1',
             False,
             'OPSZ_PTR',
             '',
             'The first input register in `CL`, `CS`, `CA`, `CB` RVC formats (inst[9:7]).'
             )
    CRS2_ = (33,
             'rs2',
             False,
             'OPSZ_PTR',
             '',
             'The second input register in `CS`, `CA` RVC formats (inst[4:2]).'
             )
    CRS2_FP = (34,
               'rs2',
               False,
               'OPSZ_PTR',
               '',
               'The second input floating-point register in `CS`, `CA` RVC formats (inst[4:2]).'
               )
    CRD__ = (35,
             'rd',
             True,
             'OPSZ_PTR',
             '',
             'The output register in `CA` RVC format (inst[9:7])'
             )
    CSHAMT = (36,
              'shamt',
              False,
              'OPSZ_6b',
              '',
              'The `shamt` field in the RVC format.'
              )
    CSR_IMM = (37,
               'imm',
               False,
               'OPSZ_5b',
               '',
               'The immediate field in a CSR instruction.'
               )
    CADDI16SP_IMM = (38,
                     'imm',
                     False,
                     'OPSZ_10b',
                     '',
                     'The immediate field in a C.ADDI16SP instruction.'
                     )
    CLWSP_IMM = (39,
                 'sp_offset',
                 False,
                 'OPSZ_1',
                 '',
                 'The SP-relative memory location (sp+imm: imm & 0x3 == 0).'
                 )
    CLDSP_IMM = (40,
                 'sp_offset',
                 False,
                 'OPSZ_9b',
                 '',
                 'The SP-relative memory location (sp+imm: imm & 0x7 == 0).'
                 )
    CLUI_IMM = (41,
                'imm',
                False,
                'OPSZ_6b',
                '',
                'The immediate field in a C.LUI instruction.'
                )
    CSWSP_IMM = (42,
                 'sp_offset',
                 True,
                 'OPSZ_1',
                 '',
                 'The SP-relative memory location (sp+imm: imm & 0x3 == 0).'
                 )
    CSDSP_IMM = (43,
                 'sp_offset',
                 True,
                 'OPSZ_9b',
                 '',
                 'The SP-relative memory location (sp+imm: imm & 0x7 == 0).'
                 )
    CIW_IMM = (44,
               'imm',
               False,
               'OPSZ_10b',
               '',
               'The immediate field in a CIW format instruction.'
               )
    CLW_IMM = (45,
               'mem',
               False,
               'OPSZ_7b',
               'im(rs1)', 'The register-relative memory location (reg+imm: imm & 0x3 == 0).')
    CLD_IMM = (46,
               'mem',
               False,
               'OPSZ_1',
               'im(rs1)', 'The register-relative memory location (reg+imm: imm & 0x7 == 0).')
    CSW_IMM = (47,
               'mem',
               True,
               'OPSZ_7b',
               'im(rs1)', 'The register-relative memory location (reg+imm: imm & 0x3 == 0).')
    CSD_IMM = (48,
               'mem',
               True,
               'OPSZ_1',
               'im(rs1)', 'The register-relative memory location (reg+imm: imm & 0x7 == 0).')
    CIMM5 = (49,
             'imm',
             False,
             'OPSZ_6b',
             '',
             'The immediate field in a C.ADDI, C.ADDIW, C.LI, and C.ANDI instruction.'
             )
    CB_IMM = (50,
              'pc_rel',
              False,
              'OPSZ_2',
              '',
              'The immediate field in a a CB format instruction (C.BEQZ and C.BNEZ).'
              )
    CJ_IMM = (51,
              'pc_rel',
              False,
              'OPSZ_2',
              '',
              'The immediate field in a CJ format instruction.'
              )
    # Virtual fields en/decoding special cases.
    V_L_RS1_DISP = (52,
                    'mem',
                    False,
                    {
                        '': 'OPSZ_0', 'lb': 'OPSZ_1', 'lh': 'OPSZ_2', 'lw': 'OPSZ_4',
                        'ld': 'OPSZ_8', 'lbu': 'OPSZ_1', 'lhu': 'OPSZ_2', 'lwu': 'OPSZ_4',
                        'sb': 'OPSZ_1', 'sh': 'OPSZ_2', 'sw': 'OPSZ_4', 'sd': 'OPSZ_8',
                        'flw': 'OPSZ_4', 'fld': 'OPSZ_8', 'fsw': 'OPSZ_4', 'fsd': 'OPSZ_8',
                        'flq': 'OPSZ_16', 'fsq': 'OPSZ_16'
                    },
                    'im(rs1)',
                    'The register-relative memory source location (reg+imm).'
                    )
    V_S_RS1_DISP = (53,
                    'mem',
                    True,
                    {
                        '': 'OPSZ_0', 'lb': 'OPSZ_1', 'lh': 'OPSZ_2', 'lw': 'OPSZ_4',
                        'ld': 'OPSZ_8', 'lbu': 'OPSZ_1', 'lhu': 'OPSZ_2', 'lwu': 'OPSZ_4',
                        'sb': 'OPSZ_1', 'sh': 'OPSZ_2', 'sw': 'OPSZ_4', 'sd': 'OPSZ_8',
                        'flw': 'OPSZ_4', 'fld': 'OPSZ_8', 'fsw': 'OPSZ_4', 'fsd': 'OPSZ_8',
                        'flq': 'OPSZ_16', 'fsq': 'OPSZ_16'
                    },
                    'im(rs1)',
                    'The register-relative memory target location (reg+imm).'
                    )

    def __str__(self) -> str:
        return self.name.lower().replace("fp", "(fp)")

    def asm_name(self) -> str:
        return self._asm_name if self._asm_name != '' else self.arg_name

    def formatted_name(self) -> str:
        return self.arg_name.capitalize()

    def from_str(fld: str):
        return Field[fld.upper().replace("(FP)", "FP")]

    def opsz(self, inst_name: str):
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
        '''
        if type(self.opsz_def) is str:
            return self.opsz_def
        if inst_name not in self.opsz_def:
            inst_name = ''
        return self.opsz_def[inst_name]


@unique
class Format(Enum):
    # Uncompressed instructions.
    R = 1
    R4 = 2
    I = 3
    S = 4
    B = 5
    U = 6
    J = 7
    # Only compressed instructions below.
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
        if (opc == 0b00 or opc == 0b10) and funct3 not in [0, 0b100]:  # LOAD/STORE instructions
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            # Immediate argument will handle the base+disp.
            if opc == 0b00:
                inst.flds.pop(1)
            inst.flds.reverse()
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif Field.CB_IMM in inst.flds:
            # Compare-and-branch instructions need their branch operand moved
            # to the 1st source operand slot as required by instr_set_target().
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            cb_imm = inst.flds.pop(0)
            inst.flds.append(cb_imm)
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')

    def __fixup_uncompressed_inst(self, inst: Instruction):
        opc = (inst.match & inst.mask) & 0x7F
        if opc in [0b0000011, 0b0000111]:  # LOAD instructions
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            inst.flds[0] = Field.V_L_RS1_DISP
            inst.flds.pop(1)
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif opc in [0b0100011, 0b0100111]:  # STORE instructions
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            inst.flds[2] = Field.V_S_RS1_DISP
            inst.flds.pop(0)
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif inst.mask == 0x1f07fff and inst.match in [0x6013, 0x106013, 0x306013]:
            # prefetch.[irw] instructions
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            inst.flds[0] = Field.V_S_RS1_DISP
            inst.flds.pop(1)
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')
        elif Field.B_IMM in inst.flds:
            # Compare-and-branch instructions need their branch operand moved
            # to the 1st source operand slot as required by instr_set_target().
            dbg(f'fixup: {inst.name} {[f.name for f in inst.flds]}')
            b_imm = inst.flds.pop(0)
            inst.flds.append(b_imm)
            dbg(f'    -> {" " * len(inst.name)} {[f.name for f in inst.flds]}')

        # FIXME i#3544: Should we fixup the 4B wide NOP (00000013) for the sake
        # of disassembly? Though it might cause issues in encoding because we'd
        # need an extra entry in the instr_infos table since NOP aliases with
        # ADDI (it's an alias).

    def __fixup_instructions(self):
        '''
        Fixup ISL definitions to better match DynamoRIO logic.

        Some instructions may require operand fixups to streamline decoding in
        the codec.c. I.e.:
        - LOAD/STORE instructions merge their rs1 + i_imm into a single operand
          which is later decoded as a base+disp operand. This is needed as i_imm
          may have immediate semantics in other instructions. On the other hand
          compressed LOAD/STORE instructions only get their rs1 operand removed
          as their immediate operand type (clw_imm, cld_imm) is only used in
          base+disp context.
        - Instructions utilizing b_imm and cb_imm have those operands put at
          the end of the operand list to ensure they are the first operand. This
          is because codec.c is using instr_set_target() which assumes that
          branch target is the first source operand.
        '''
        for inst in self.instructions:
            if inst.is_compressed():
                self.__fixup_compressed_inst(inst)
            else:
                self.__fixup_uncompressed_inst(inst)

    def __parse_isl_file(self, isl_file: Path) -> bool:
        '''
        Parse a single Instruction Set Listing file.

        The file base name is interpreted as an ISA extension name (including the
        base ISA).

        Returns true if parsing succeeded.
        '''
        ext = path.basename(isl_file).removesuffix('.txt')
        with open(isl_file, 'r') as f:
            for line in f:
                line = line.split("#")[0].strip()
                if len(line) == 0:
                    continue
                tokens = [t.strip() for t in line.split('|')]
                assert (len(tokens) == 4)
                name = tokens[0]
                fmt = tokens[1]
                flds = tokens[2].strip().split(' ')
                mask = 0
                match = 0
                shift = len(tokens[3])
                if name.startswith("c.") or name == "unimp":
                    if shift != 16:
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
            if id and not i.is_compressed() and id.mask == i.mask:
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
            if not res:
                break
        self.__fixup_instructions()
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
        # Replace tmpl_fld in the template_file and save as out_file.
        buf = StringIO()
        with open(template_file, 'r') as tf:
            for line in tf:
                if '*/ OP_' in line:
                    nmb = re.findall(r'\d+', line)
                    if len(nmb) != 1:
                        warn(
                            f"Template opcode line is missing an index?: '{line}'")
                    idx = int(nmb[0])
                elif tmpl_fld in line:
                    found_template_fld = True

                    if idx == -1:
                        warn("Starting index not found, using 0")
                    idx += 1
                    IslGenerator.OP_TBL_OFFSET = idx
                    lines = []
                    # Generate a list of:
                    #    OP_<opcode>,   /**< <extension> <opcode> opcode. */
                    for i in self.instructions:
                        lines.append(
                            f"    /* {idx:3d} */ OP_{i.formatted_name()} = {idx},   /**< {i.ext} {i.name} opcode. */")
                        idx += 1

                    line = line.replace(tmpl_fld, '\n'.join(lines))
                buf.write(line)
        write_if_changed(out_file, buf.getvalue())
        if not found_template_fld:
            err(f"{tmpl_fld} not found in {template_file}")
        return found_template_fld

    def generate_instr_macros(self, template_file, out_file) -> bool:
        '''
        Generate the instr_create_api.h header file.

        Returns true if the output file was written successfully.
        '''
        tmpl_fld = '@INSTR_MACROS@'
        if len(self.instructions) == 0:
            return False
        found_template_fld = False
        # Replace tmpl_fld in the template_file and save as out_file.
        buf = StringIO()
        with open(template_file, 'r') as tf:
            for line in tf:
                if tmpl_fld in line:
                    found_template_fld = True

                    lines = []
                    # Generate a list of:
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
                                [f' * \param {f.formatted_name():6}  {f.arg_cmt}' for f in flds])
                        nd = len([f for f in flds if f.is_dest])
                        ns = len(flds) - nd
                        lines.append(
                            f'''/**
 * Creates a(n) {i.name} instruction.
 *
 * \param dc      The void * dcontext used to allocate memory for the instr_t.{arg_comments}
 */
#define INSTR_CREATE_{i.formatted_name()}(dc{args}) \\
    instr_create_{nd}dst_{ns}src(dc, OP_{i.formatted_name()}{args})\n''')
                    line = line.replace(tmpl_fld, '\n'.join(lines))
                buf.write(line)
        write_if_changed(out_file, buf.getvalue())
        if not found_template_fld:
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
            if instructions is None:
                trie_index += 1
                continue
            dbg(f'trie_buckets[{trie_index}]:len({len(instructions)}):'
                f' {[i.name for i in instructions]}')
            mask = trie[trie_index].mask
            shift = trie[trie_index].shift
            if mask == 0:  # This denotes a leaf node.
                trie_index += 1
                continue
            bucket_size = mask + 1
            dbg(f'mask:        {mask:032b}')
            dbg(f'shift:       {shift}')
            dbg(f'bucket size: {bucket_size}')
            buckets: List[List[Instruction]] = [[]
                                                for i in range(0, bucket_size)]
            # Each trie bucket contains a list of instructions sharing
            # (inst.match >> shift) & mask, where mask is a contiguous set of 1
            # bits.
            # There is a special case when trie creation algorithm detects
            # aliased instructions (that is having different inst.mask but the
            # same inst.match - i.e. ori and prefetch.[irw]). In that case
            # bucket will contain exact match instructions (inst.mask == mask)
            # and ones which may have any other value across the bucket's mask.
            non_exact_match = []
            for instruction in instructions:
                imatch = (instruction.match >> shift) & mask
                imask = (instruction.mask >> shift) & mask
                assert (imatch < bucket_size)
                # First all exact-match instructions need to be put into their
                # search bucket.
                if mask == imask:
                    buckets[imatch].append(instruction)
                # Non-exact match instructions will be put in all other search
                # buckets after we know all the positions of exact-match
                # buckets.
                else:
                    non_exact_match.append(instruction)
            # Finally append the non-exact matches.
            if len(non_exact_match) > 0:
                for bucket in buckets:
                    if len(bucket) == 0:
                        bucket += non_exact_match
            trie[trie_index].index = len(trie)
            idx = -1
            for bucket in buckets:
                idx += 1
                l = len(bucket)
                if l != 0:
                    dbg(f'buckets[(i >> {shift}) & 0b{buckets.index(bucket):b}]: \n  ' +
                        '\n  '.join(
                            [f'{i.name:16s}: {i.match:032b} & {i.mask:032b}'
                             for i in bucket]))
                if l == 0:
                    trie.append(self.TrieNode(0, 0, max_unsigned(16)))
                elif l == 1:
                    name = bucket[0].name
                    wanted = [i for i in self.instructions if i.name == name][0]
                    wi = op_offset + self.instructions.index(wanted)
                    trie.append(self.TrieNode(0, 0, wi, name))
                else:
                    mask_anded = max_unsigned(32)
                    common_match = max_unsigned(32)
                    common_comparator = bucket[0].match
                    for instruction in bucket:
                        mask_anded &= instruction.mask
                        common_match &= ~(common_comparator ^
                                          instruction.match) & max_unsigned(32)
                        common_comparator &= common_match
                    if mask_anded & ~common_match & max_unsigned(32) == 0:
                        # If mask is 0, it means we've reached the end of the
                        # decoded instruction but there are still more than 1
                        # elements in the search bucket. This means the bucket
                        # contains aliased instructions. In this case we need
                        # to fixup the mask so that it points to the place where
                        # masks differ between instructions. The rest of the
                        # handling is done above during search bucket list
                        # creation.
                        dbg('  Bucket with aliased instructions, mask fixed up!')
                        mask_anded = ~mask_anded & max_unsigned(32)
                    else:
                        mask_anded &= ~common_match & max_unsigned(32)
                    next_shift = count_trailing_zeros(mask_anded)
                    next_mask = (1 << count_trailing_ones(
                        mask_anded >> next_shift)) - 1
                    next_trie_index = len(trie)
                    dbg(f'  trie[{next_trie_index}] <- (i >> {next_shift}) & 0b{next_mask:b}')
                    trie.append(self.TrieNode(next_mask, next_shift, 0))
                    trie_buckets[next_trie_index] = bucket
            trie_index += 1
        return trie

    def generate_instr_info_trie(self, out_file, op_offset) -> bool:
        '''
        Generate the instr_info_trie.c file.

        Returns true if the output file was written successfully.
        '''
        if op_offset < 0:
            err("Invalid OP_* offset")
            return False
        if len(self.instructions) == 0:
            return False
        buf = StringIO()
        # Keep the order of fields here in sync with the documentation in
        # core/ir/riscv64/codec.c.
        OPND_TGT = ['dst1', 'src1', 'src2', 'src3', 'dst2']
        instr_infos = []
        trie = []
        # Generate the rv_instr_info_t list.
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
                    if f.is_dest:
                        oidx = 0
                        ndst += 1
                    else:
                        oidx = isrc
                        isrc += 1
                        nsrc += 1
                    opnds += f'''
        .{OPND_TGT[oidx]}_type = RISCV64_FLD_{f.name},
        .{OPND_TGT[oidx]}_size = {f.opsz(i.name)},'''
            instr_infos.append(f'''[OP_{i.formatted_name()}] = {{ /* {i.name}{asm_args} */
    .info = {{
        .type = OP_{i.formatted_name()},
        .opcode = 0x{(ndst << 31) | (nsrc << 28):08x}, /* {ndst} dst, {nsrc} src */
        .name = "{i.name}",{''.join(opnds)}
        .code = (((uint64){hex(i.match)}) << 32) | ({hex(i.mask)}),
    }},
    .ext = {i.formatted_ext()},
}},''')
        # Generate the trie.
        trie = self.construct_trie(op_offset)
        trie = [
            f'{{.mask = {hex(t.mask)}, .shift = {t.shift}, .index = {t.index}}},{f" /* {t.ctx} */" if len(t.ctx) > 0 else ""}' for t in trie]
        instr_infos = '\n    '.join(instr_infos)
        trie = '\n    '.join(trie)
        write_if_changed(out_file, f'''
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
    if len(argv) < 4:
        warn(
            f"Usage: {path.basename(__file__)} <isl-path> <template-path> <output-path>")
        exit(1)
    isl_path = argv[1]
    template_path = argv[2]
    out_path = argv[3]
    generator = IslGenerator()

    res = generator.parse_isl(isl_path)
    if not res:
        err("Failed to parse Instruction Set Listings")
        exit(2)

    dbg("Parsed instructions:")
    for i in generator.instructions:
        dbg(f"  {i}")

    res = generator.generate_opcodes(path.join(
        template_path, f"{OP_FNAME}.in"), path.join(out_path, OP_FNAME))
    if not res:
        err(f"Failed to generate {OP_FNAME}")
        exit(3)

    res = generator.generate_instr_macros(path.join(
        template_path, f"{INSTM_FNAME}.in"), path.join(out_path, INSTM_FNAME))
    if not res:
        err(f"Failed to generate {INSTM_FNAME}")
        exit(4)

    res = generator.generate_instr_info_trie(
        path.join(out_path, INSTRINFO_FNAME), IslGenerator.OP_TBL_OFFSET)
    if not res:
        err(f"Failed to generate {INSTRINFO_FNAME}")
        exit(4)
