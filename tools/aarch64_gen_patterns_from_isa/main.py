#!/usr/bin/env python

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

# This script takes a directory containing the Arm A64 ISA XML specification
# as input and generates entries for codec.txt and dis-a64-neon.txt.
# Currently it supports a subset of scalar and vector floating point
# instructions.
# The latest version of the latest A64 ISA XML specification should be available
# here:
#   https://developer.arm.com/products/architecture/a-profile/exploration-tools

# FIXME i#2626: Add support for missing instructions, most notably:
#               integer vector arithmetic and reductions.

import os
import re
import random
import xml.etree.ElementTree as ET
from collections import namedtuple, OrderedDict

import click
import attr

from generate_tests import generate_test_strings
from generate_macros import get_macro_string
from generate_codec import generate_pattern, build_decode_str_bin_x


def should_print_box(box):
    """
    Attempt to figure out if a box is "interesting". This is essentially
    any box that is a constraint, has a name or includes unspecified bits ("x")
    """

    should_print = 'constraint' in box.attrib
    if not should_print:
        enc = [c.text for c in box.findall("./c") if c.text is not None]
        should_print = not enc or 'x' in enc
    return should_print


# Subset of types we can handle.
DATATYPE_TO_REG = {
    'advsimd+single-and-double': 'dq',
    'advsimd+half': 'dq',
    'advsimd+complex': 'dq',
    'float+half': 'float_reg',
    'float+single': 'float_reg',
    'float+double': 'float_reg',
}


def get_type_key(class_info):
    return class_info['instr-class'] + '+' + class_info.get('datatype', '')


def op_sort_key(op):
    """
    Fix the operand order. The order in the XML is sometimes wrong.
    Sort operands by their last letter (which should be ascending in the
    order they appear in the assembler string.

    Also make sure that the artificial operands sz and size get added at the
    back.
    """
    name = op.attrib['name']
    if name in ('sz', 'size'):
        return 'Y'

    if name == 'rot':
        return 'Z'

    return name[-1]


def only_bhs(mnemonic):
    return mnemonic in (
        'shadd', 'srhadd', 'shsub', 'smax', 'smin', 'sabd', 'saba', 'mla',
        'mul', 'smaxp', 'sminp', 'uhadd', 'urhadd', 'uhsub', 'umax', 'umin',
        'uabd', 'uaba', 'mls', 'umaxp', 'uminp')


def only_hs(mnemonic):
    return mnemonic in ('sqdmulh', 'sqrdmulh')


def only_b(mnemonic):
    return mnemonic in ('pmul', )


def only_h(mnemonic):
    return mnemonic in ('fmlal', 'fmlal2', 'fmlsl', 'fmlsl2')


@attr.s
class Encoding(object):
    mnemonic = attr.ib()
    class_info = attr.ib()
    boxes = attr.ib()
    known_boxes = attr.ib()
    arch_variant = attr.ib()
    class_id = attr.ib()
    enctags = attr.ib()  # The contents of the 'Instruction Details' column.

    def __attrs_post_init__(self):
        boxes = self.get_named_boxes()
        relevant_ops = [box for box in boxes
                        if should_print_box(box) and box.attrib['name'] != 'Q'
                        and box.attrib['name'] != 'type' and box.attrib['name'] != 'size']
        relevant_ops = sorted(relevant_ops, key=op_sort_key, reverse=True)

        m = re.match(r'.*_([HSD][HSD])_.*', self.enctags)
        if m:
            ot, it = m.group(1)[0].lower(), m.group(1)[1].lower()
            self.outputs = [ot + '0']
            self.inputs = [it + '5']
            return

        self.inputs = [
            self.operand_to_reg_class(op) for op in relevant_ops if op.attrib.get(
                'name', '') != 'Rd']

        self.outputs = [self.operand_to_reg_class(
            op) for op in relevant_ops if op.attrib.get('name', '') == 'Rd']

        if self.reads_dst():
            assert len(self.outputs) == 1
            self.inputs = self.outputs + self.inputs

        sz = self._get_size_op()
        if sz:
            self.inputs.append(sz)

    def _get_size_op(self):
        if self.class_id in ('asimdsamefp16',):
            return 'h_sz'

        if 'size' not in self.known_boxes:
            return None
        if self.mnemonic in ('fmlal', 'fmlal2', 'fmlsl', 'fmlsl2'):
            return None
        sz = self.known_boxes['size']
        if sz and 'x' not in sz:
            return None

        sz = self.get_selected_sizes()
        return sz + '_sz'

    def get_selected_sizes(self):
        if self.class_id in ('asimdsamefp16',):
            return 'h'

        if 'size' not in self.known_boxes:
            assert False

        if only_b(self.mnemonic):
            return 'b'
        if only_h(self.mnemonic):
            return 'h'
        if only_b(self.mnemonic):
            return 'b'
        if only_hs(self.mnemonic):
            return 'hs'
        if only_bhs(self.mnemonic):
            return 'bhs'

        sz = self.known_boxes['size']
        if not sz:
            return 'bhsd'
        elif 'x' in sz:
            assert sz in ('0x', '1x')
            return 'sd'
        else:
            return 'b'

    def inputs_no_dst(self):
        if self.reads_dst():
            return self.inputs[1:]
        return self.inputs

    def reads_dst(self):
        return self.mnemonic in ('fmla', 'fmls',)

    def type_as_str(self):
        TYPE_TO_STR = {
            'advsimd': 'vector',
            'float': 'floating point',
        }
        return TYPE_TO_STR[self.class_info['instr-class']]

    def get_named_boxes(self):
        return [
            box for box in self.boxes if 'name' in box.attrib and not self.known_boxes.get(
                box.attrib['name'], None)]

    def _get_inputs_outputs(self):
        return inputs, outputs

    def operand_to_reg_class(self, box):
        name = box.attrib['name']

        bit_start = int(box.attrib['hibit']) - \
            int(box.attrib.get('width', 1)) + 1
        prefix = ''
        if name == 'rot':
            prefix = 'rot'
        else:
            assert name.startswith('R')
            prefix = DATATYPE_TO_REG[get_type_key(self.class_info)]

        return '{}{}'.format(prefix, bit_start)


def parse_encoding(enc_row):
    name = enc_row.find(
        "./td[@class='iformname']").text.split(' ')[0].replace(',', '').lower()
    if 'label' in enc_row.attrib:
        name = enc_row.attrib['label'].lower()
    bitfields = [f.text for f in enc_row.findall("./td[@class='bitfield']")]
    return (name, bitfields, enc_row.attrib['encname'])


def should_ignore(mnemonic):
    return mnemonic in ('unallocated',)


CLASS_INFOS = {
    'asimdsame': {
        'instr-class': 'advsimd',
        'datatype': 'single-and-double',
    },
    'asimdsamefp16': {
        'instr-class': 'advsimd',
        'datatype': 'half',
    },
    'floatdp1': {
        'instr-class': 'float',
        'datatype': 'single',
    },
    'floatdp2': {
        'instr-class': 'float',
        'datatype': 'single',
    },
    'floatdp3': {
        'instr-class': 'float',
        'datatype': 'single',
    },
}


def parse_instr_class(iclass):
    class_id = iclass.attrib['id']
    class_info = CLASS_INFOS[class_id]
    instr_table = iclass.find('./instructiontable')

    headers = [h.text for h in instr_table.findall(
        "./thead/tr[@id='heading2']/th")]
    encs_xml = [parse_encoding(row) for row in instr_table.findall(
        "./tbody/tr[@class='instructiontable']")]

    boxes = iclass.findall("./regdiagram/box")

    seen = set()
    encs = []
    for (mnemonic, known_vals, label) in encs_xml:
        known = {k: v for (k, v) in zip(headers, known_vals)}
        if should_ignore(mnemonic) or mnemonic + known['opcode'] in seen:
            continue

        # floatdp2 lists all operand widths (h, s, d) explicitly. We want to
        # have a single line with float_reg for them.
        if class_id in (
            'floatdp1',
            'floatdp2',
                'floatdp3') and xml[0] != 'fcvt':
            seen.add(xml[0] + known['opcode'])
            del known['type']

        # Those instructions have 0x/1x as their size, but x == 1 is reserved.
        if mnemonic in ('fmlal', 'fmlal2', 'fmlsl', 'fmlsl2'):
            known['size'] = known['size'].replace('x', '0')
        eo = Encoding(mnemonic=mnemonic, class_info=class_info,
                      boxes=boxes, arch_variant='', known_boxes=known,
                      class_id=class_id, enctags=label)
        encs.append(eo)

    return iclass.attrib['title'], encs


@click.command()
@click.option(
    '--only',
    default='',
    help='Only include files with only in their name.')
@click.option(
    '--to-generate',
    default='codec',
    help='Generate strings for either codec.txt (codec), tests (tests), '
         'macros (macros) or everything (all)')
@click.argument('encindex')
def main(encindex, only, to_generate):
    # Deterministic seed, so we get reproduce-able results.
    random.seed(a=10)

    parsed = ET.parse(open(encindex))
    root = parsed.getroot()
    classes = root.findall("iclass_sect")

    for c in classes:
        if c.attrib['id'] != only:
            continue
        name, encs = parse_instr_class(c)
        if to_generate in ('codec', 'all'):
            codec_strs = map(generate_pattern, encs)
            print("\n# {}".format(name))
            print('\n'.join(codec_strs))

        if to_generate in ('tests', 'all'):
            dis_a64, api, expected = zip(*map(generate_test_strings, encs))
            print("dis-a64.txt:")
            print("\n# {}".format(name))
            print("\n".join(line for sl in dis_a64 for line in sl))

            print("\n\nir_aarch64.c:")
            print("\n/* {} */".format(name))
            print("\n".join(line for sl in api for line in sl))

            print("\n\nir_aarch64.expect:")
            print("\n".join(line for sl in expected for line in sl))

        if to_generate in ('macros', 'all'):
            macro_strs = map(get_macro_string, encs)
            print("\ninstr_create.h:")
            print("\n/* -------- {:-<60} */".format(name + ' '))
            print("\n".join(macro_strs))

    return 0


if __name__ == '__main__':
    main()
