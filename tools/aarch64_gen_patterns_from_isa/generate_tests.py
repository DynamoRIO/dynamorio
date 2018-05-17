# **********************************************************
# Copyright (c) 2018 ARM Limited. All rights reserved.
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
# * Neither the name of ARM Limited nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

import random
import subprocess

TYPE_TO_STR2 = {
    'advsimd': 'vector',
    'float': 'scalar',
}


def generate_reg_strs(op, num_strs):
    """
    Generates a set of test strings for op. For immediate that take special
    values, like fsz, we generate a list with possible values.
    For registers, we generate a list of strings expressing each possible size
    of the register operand. In that case, num_strs indicates the number of
    combinations required, e.g. dq and 3  means a vector fp register with 3
    different element sizes.
    """
    if op == 'fsz':
        return [], ['$0x02', '$0x03', '$0x02']
    if op == 'fsz16':
        return [], ['$0x01', '$0x01']
    if op == 'fsz2':
        return [], ['$0x02', '$0x03', '$0x02', '$0x01', '$0x01']
    if op == 'rot12':
        assert num_strs == 5
        return (['90', '270', '90', '270', '90'],
                ['$0x00', '$0x01', '$0x00', '$0x01', '$0x00'])
    if op == 'rot11':
        assert num_strs == 5
        return ['0', '90', '0', '90', '0'], [
            '$0x00', '$0x01', '$0x00', '$0x01', '$0x00']

    assert op.startswith('dq') or op.startswith('float_reg')
    asm = []
    ir = []
    if op.startswith('dq'):
        if num_strs == 3:
            temp_asm = ['v{}.4s', 'v{}.2d', 'v{}.2s']
            temp_ir = ['%q{}', '%q{}', '%d{}']
        elif num_strs == 2:
            temp_asm = ['v{}.8h', 'v{}.4h']
            temp_ir = ['%q{}', '%d{}']
        elif num_strs == 5:
            temp_asm = ['v{}.4s', 'v{}.2d', 'v{}.2s', 'v{}.8h', 'v{}.4h']
            temp_ir = ['%q{}', '%q{}', '%d{}', '%q{}', '%d{}']
        else:
            assert False
    elif op.startswith('float_reg'):
        temp_asm = ['d{}', 's{}', 'h{}']
        temp_ir = ['%d{}', '%s{}', '%h{}']

    num = random.randint(0, 31)
    for (t_asm, t_ir) in zip(temp_asm, temp_ir):
        asm.append(t_asm.format(num))
        ir.append(t_ir.format(num))
    return asm, ir


SIZE_TO_MACRO = {
    '$0x01': 'OPND_CREATE_HALF()',
    '$0x02': 'OPND_CREATE_SINGLE()',
    '$0x03': 'OPND_CREATE_DOUBLE()',
}

ROT11_TO_MACRO = {
    '$0x00': 'OPND_CREATE_ROT0()',
    '$0x01': 'OPND_CREATE_ROT90()',
}

ROT12_TO_MACRO = {
    '$0x00': 'OPND_CREATE_ROT90()',
    '$0x01': 'OPND_CREATE_ROT270',
}


def ir_arg_to_c(ir):
    """
    Returns a string to create ir in C.
    """
    for (ir_prefix, c) in [('%d', 'D'), ('%q', 'Q'), ('%s', 'S'), ('%h', 'H')]:
        if not ir.startswith(ir_prefix):
            continue
        return "opnd_create_reg(DR_REG_" + c + ir.replace(ir_prefix, '') + ")"
    return SIZE_TO_MACRO[ir]


def generate_api_test(opcode, ir_ops, str_type):
    """
    Returns C statements to create an instruction with opcode and ir_ops as
    arguments, as well as a call to test_instr_encoding for the generated
    instruction.
    """
    c_args = [ir_arg_to_c(ir) for ir in ir_ops]
    macro_start = '    instr = INSTR_CREATE_{}_{}(dc,'.format(opcode, str_type)
    return '\n'.join(['',
                      '{}\n{});'.format(macro_start,
                                        ',\n'.join((len(macro_start) - 3) * ' ' + arg for arg in c_args)),
                      '    test_instr_encoding(dc, OP_{}, instr);'.format(opcode)])


def generate_dis_test(mnemonic, asm_ops, ir_str):
    """
    Returns a line for dis-a64.txt, of the form
    Binary encoding : Assembly representation   : IR representation
    4e400fc2        : fmla v2.8h, v30.8h, v0.8h : fmla   %q2 %q30 %q0 $0x01 -> %q2

    It calls out to gcc to generate the binary encoding. Gcc needs to support
    -march=armv8.3-a.
    """
    asm_str = '{} {}'.format(mnemonic, ', '.join(asm_ops))
    with open('/tmp/autogen.s', 'wt') as f:
        f.write(asm_str)
        f.write('\n')

    subprocess.check_call(
        ["gcc", "-march=armv8.3-a", "-c", "-o", "/tmp/autogen.o", "/tmp/autogen.s", ])
    out = subprocess.check_output(["objdump", "-d", "/tmp/autogen.o"])
    enc_str = out.decode('utf-8').split('\n')[-2][6:14]
    return '{} : {} : {}'.format(enc_str, asm_str, ir_str)


def num_combinations_to_test(enc):
    """
    Returns the number of test cases we need for this encoding.
    For example:
      * We need 3 tests for an FP SIMD encoding with single and double,
        because input registers have 3 different element sizes: .4S .2S, .2D
     * We need 3 tests for a scalar FP instruction, because we need to test
       different register widths: Hx, Sx, Dx
    """

    if enc.class_info['instr-class'] == 'float':
        return 3
    else:
        # FP SIMD single and double encoding
        if 'fsz' in enc.inputs:
            return 3
        # FP SIMD half encoding
        elif 'fsz16' in enc.inputs:
            return 2
        # FP SIMD half, single and float encoding
        elif 'fsz2' in enc.inputs:
            return 5
        else:
            assert False


def generate_test_strings(enc):
    """
    Returns a list of dis-a64, ir_aarch64.c and ir_aarch64.expect tests.
    """
    input_ir = []
    asm_ops = []
    output_ir = []

    test_lines = []
    api_tests = []
    api_tests_expected = []
    num_strs = num_combinations_to_test(enc)

    for op in enc.outputs:
        asm, ir = generate_reg_strs(op, num_strs)
        asm_ops.append(asm)
        output_ir.append(ir)
        if enc.reads_dst():
            input_ir.append(ir)

    for op in enc.inputs:
        asm, ir = generate_reg_strs(op, num_strs)
        if op == 'dq0':
            continue
        if asm:
            asm_ops.append(asm)
        if ir:
            input_ir.append(ir)

    # Above we built tables like
    # input_asm = [ [ v1, v4],
    #               [ v9, v7] ]
    # The first list contains the test operands for the first input operand
    # in each test instruction we generate, i.e. we generate the following
    # test strings:
    #   foo ... v1, v9
    #   foo ... v4, v7
    for j in range(0, num_strs):
        in_ir = [input_ir[i][j] for i in range(0, len(input_ir))]
        asm = [asm_ops[i][j] for i in range(0, len(asm_ops))]
        out_ir = [output_ir[i][j] for i in range(0, len(output_ir))]
        ir_str = '{} {} -> {}'.format(enc.mnemonic.ljust(6, ' '),
                                      ' '.join(in_ir), ' '.join(out_ir))

        test_lines.append(generate_dis_test(enc.mnemonic, asm, ir_str))

        if enc.reads_dst():
            api_in_ir = in_ir[1:]
        else:
            api_in_ir = in_ir

        api_tests.append(generate_api_test(enc.mnemonic, out_ir +
                                           api_in_ir, TYPE_TO_STR2[enc.class_info['instr-class']]))
        api_tests_expected.append(ir_str)

    return test_lines, api_tests, api_tests_expected
