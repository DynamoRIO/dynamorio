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


def build_decode_str_bin_x(boxes, known, x="x"):
    """"
    Returns a binary string e.g. 1110001101xxxxx00101, where x is
    unspecified e.g. where a register is encoded in the instruction.
    """
    str_ = ""
    for box in boxes:
        width = int(box.attrib['width']) if 'width' in box.attrib else 1
        name = box.attrib['name'] if 'name' in box.attrib else ''
        if 'settings' not in box.attrib or 'constraint' in box.attrib:
            if name in known and known[name]:
                str_ += known[name]
            else:
                str_ += x * width
        else:
            str_ += ''.join(c.text.strip("()")
                            for c in box.findall("./c")).replace("x", x)
    return str_


def generate_pattern(enc):
    """
    Creates a pattern for enc for codec.txt of the form
    bit pattern                       mnemonic output_ops : input_ops
    0x1011101x1xxxxx111011xxxxxxxxxx  facgt    dq0        : dq5 dq16 fsz
    """
    arg_string = ' : '.join([' '.join(enc.outputs), ' '.join(enc.inputs)])

    return '{}     {:<9} {}{}'.format(
        build_decode_str_bin_x(enc.boxes, enc.known_boxes),
        enc.mnemonic,
        arg_string, '')
