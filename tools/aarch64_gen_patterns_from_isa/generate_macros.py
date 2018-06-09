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

ARG_TO_REG = {
    'dq0': ('Rd', 'The output register.'),
    'dq5': ('Rm', 'The first input register.'),
    'dq16': ('Rn', 'The second input register.'),
    'dq10': ('Ra', 'The third input register.'),
    'float_reg0': ('Rd', 'The output register.'),
    'float_reg5': ('Rm', 'The first input register.'),
    'float_reg16': ('Rn', 'The second input register.'),
    'float_reg10': ('Ra', 'The third input register.'),
    'sd_sz': ('width', 'The vector element width. Use either OPND_CREATE_HALF(), \n *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().'),
    'hsd_sz': ('width', 'The vector element width. Use either OPND_CREATE_HALF(), \n *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().'),
    'bhsd_sz': ('width', 'The vector element width. Use either OPND_CREATE_BYTE(), OPND_CREATE_HALF(), \n *                OPND_CREATE_SINGLE() or OPND_CREATE_DOUBLE().'),
    'bhs_sz': ('width', 'The vector element width. Use either OPND_CREATE_BYTE(), OPND_CREATE_HALF(), \n *                OPND_CREATE_SINGLE().'),
    'hs_sz': ('width', 'The vector element width. Use either OPND_CREATE_HALF(), \n *                OPND_CREATE_SINGLE().'),
    'b_sz': ('width', 'The vector element width. Use either OPND_CREATE_BYTE().'),
}

TYPE_TO_STR2 = {
    'advsimd': 'vector',
    'float': 'scalar',
}


def get_param_comment(param):
    name, text = ARG_TO_REG[param]
    return '{name: <8}{text}'.format(name=name, text=text)


def get_doc_comments(enc):
    params = [' * \param {}'.format(get_param_comment(p)) for p in enc.outputs] + \
        [' * \param {}'.format(get_param_comment(p)) for p in enc.inputs_no_dst()]

    if enc.reads_dst():
        params[0] = params[0] + ' The instruction also reads this register.'

    comment = """
/**
 * Creates a {} {} instruction.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
{}
 */
"""
    return comment.format(
        enc.mnemonic, enc.type_as_str(), '\n'.join(params))


def get_macro(enc):
    args = ', '.join(ARG_TO_REG[p][0] for p in enc.outputs + enc.inputs)
    call = '    instr_create_{}dst_{}src(dc, OP_{}, {})'.format(
        len(enc.outputs), len(enc.inputs), enc.mnemonic, args)

    args_def = ', '.join(ARG_TO_REG[p][0]
                         for p in enc.outputs + enc.inputs_no_dst())
    return '#define INSTR_CREATE_{}_{}(dc, {}) \\\n{}'.format(
        enc.mnemonic, TYPE_TO_STR2[enc.class_info['instr-class']], args_def, call)


def get_macro_string(enc):
    return '{}{}'.format(get_doc_comments(enc), get_macro(enc))
