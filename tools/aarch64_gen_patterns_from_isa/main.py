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
import random
import xml.etree.ElementTree as ET
from collections import namedtuple, OrderedDict

import click

from generate_tests import generate_test_strings
from generate_macros import get_macro_string
from generate_codec import generate_pattern



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


def parse_and_sanity_check_xml(file):
    parsed = ET.parse(file)
    root = parsed.getroot()
    if root.tag != 'instructionsection':
        # not all xml files are interesting, some are just navigation pages
        # etc that we do not care about
        print(
            "skipping {} with root tag {}".format(
                file,
                root.tag))
        return None
    return (file, root)


def find_files(root, only):
    paths = []
    for dirpath, _, files in os.walk(root):
        for name in files:
            if not name.endswith(".xml") or (only and only not in name):
                continue
            paths.append(os.path.join(dirpath, name))
    return paths


def load_all_files(path, only):
    files = find_files(path, only)
    print("found {} xml files".format(len(files)))
    return sorted(e for e in map(parse_and_sanity_check_xml, files) if e)


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


def operand_to_reg_class(box, class_info):
    name = box.attrib['name']
    if name == 'sz':
        if class_info['datatype'] == 'single-and-double':
            return 'fsz'
        assert False
    if name == 'size':
        # New FP SIMD instructions added in 8.3 have 1 encoding group for
        # half, single and double, with size being 2 bit wide.
        if class_info['datatype'] in ('half', 'single-and-double', 'complex'):
            return 'fsz2'
        assert False

    bit_start = int(box.attrib['hibit']) - int(box.attrib.get('width', 1)) + 1
    prefix = ''
    if name == 'rot':
        prefix = 'rot'
    else:
        assert name.startswith('R')
        prefix = DATATYPE_TO_REG[get_type_key(class_info)]

    return '{}{}'.format(prefix, bit_start)


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


def get_inputs_outputs(boxes, class_info):
    relevant_ops = [box for box in boxes
                    if should_print_box(box) and box.attrib['name'] != 'Q'
                    and box.attrib['name'] != 'type']
    relevant_ops = sorted(relevant_ops, key=op_sort_key, reverse=True)

    inputs = [operand_to_reg_class(op, class_info)
              for op in relevant_ops if op.attrib.get('name', '') != 'Rd']

    outputs = [operand_to_reg_class(op, class_info)
               for op in relevant_ops if op.attrib.get('name', '') == 'Rd']

    if class_info['reads_dst']:
        assert len(outputs) == 1
        inputs = outputs + inputs

    if get_type_key(class_info) == 'advsimd+half':
        assert 'fsz' not in inputs
        inputs.append('fsz16')

    return inputs, outputs


def is_supported_instr_category(s):
    """ Subset of instruction categories we can handle. """
    return (s.startswith('aarch64/instrs/vector/arithmetic/binary/uniform/') or
            s.startswith('aarch64/instrs/float/arithmetic/'))


def is_supported_encoding(mnemonic, psname, class_info):
    return (mnemonic.lower() not in ('fcadd', 'fcmla') and
            is_supported_instr_category(psname) and
            get_type_key(class_info) in DATATYPE_TO_REG and
            class_info.get('advsimd-type', '') in ('simd', ''))


EncodingBase = namedtuple(
    'Encoding', [
        'mnemonic', 'inputs', 'outputs', 'class_info', 'boxes', 'arch_variant'])


class Encoding(EncodingBase):
    def inputs_no_dst(self):
        if self.class_info['reads_dst']:
            return self.inputs[1:]
        return self.inputs

    def reads_dst(self):
        return self.class_info['reads_dst']

    def type_as_str(self):
        TYPE_TO_STR = {
            'advsimd': 'vector',
            'float': 'floating point',
        }
        return TYPE_TO_STR[self.class_info['instr-class']]


def parse_encodings(class_, reads_dst):
    """
    Creates a list of Encoding objects for class_
    """
    class_info = {
        'reads_dst': reads_dst,
    }

    parsed_encodings = []

    # Some instructions have multiple encodings (e.g. ADD (vector) has two).
    encodings = class_.findall("./encoding")
    for encoding in encodings:
        mnemonic = encoding.find(
            "./docvars/docvar[@key='mnemonic']").attrib['value']

        regdiagram = class_.find("./regdiagram")
        psname = regdiagram.get('psname')

        # Gather some useful information about the encoding class.
        docvars = encoding.findall("./docvars/docvar")
        for dv in docvars:
            attrs = dv.attrib
            if attrs['key'] not in ('datatype', 'instr-class', 'advsimd-type'):
                continue
            class_info[attrs['key']] = attrs['value']
        if 'datatype' not in class_info:
            class_info['datatype'] = psname.split('/')[-1]

        if not is_supported_encoding(mnemonic, psname, class_info):
            parsed_encodings.append('# Missing encoding: {}'.format(psname))
            continue

        boxes = class_.findall("./regdiagram/box")
        # we make the general assumption that named boxes are "interesting" (although in
        # some cases they are still fully specified or are simply constraints
        # on the encoding)
        named_boxes = [box for box in boxes if 'name' in box.attrib]
        inputs, outputs = get_inputs_outputs(named_boxes, class_info)

        # Get the architecture variant of the encoding, e.g. ARM v8.2. We
        # change ARM to Arm. For Arm v8.0, the arch variant is empty.
        arch_variants = class_.find('arch_variants')
        if arch_variants is not None and len(arch_variants) == 1:
            arch_variant = ' # {}'.format(
                arch_variants[0].get('name').replace(
                    'ARM', 'Arm'))
        else:
            assert not arch_variants
            arch_variant = ''

        parsed_encodings.append(
            Encoding(mnemonic=mnemonic.lower(), inputs=inputs,
                     outputs=outputs, class_info=class_info, boxes=boxes,
                     arch_variant=arch_variant))
    return parsed_encodings


def reads_dst(root):
    """
    Checks if this instruction reads the destination register Vd.
    """
    execute_text = root.find("./ps_section/ps/pstext")
    if execute_text is not None:
        execute_text = ''.join(execute_text.itertext())
    else:
        execute_text = ''

    return '= V[d]' in execute_text


def deduplicate_and_sort(l):
    return list(OrderedDict.fromkeys(l))


def filter_unsupported(l):
    """ Returns a new list containing only Encoding instances. """
    return [e for e in l if isinstance(e, Encoding)]


def filter_supported(l):
    """ Returns a new list containing only unsupported encoding messages. """
    return [e for e in l if isinstance(e, str)]


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
@click.argument('isa_dir')
def main(isa_dir, only, to_generate):
    # load files from arguments on command line (these can be files or
    # directories)
    roots = load_all_files(isa_dir, only)

    # Deterministic seed, so we get reproduce-able results.
    random.seed(a=10)
    test_strs = []
    macro_strs = []
    api_tests_strs = []
    api_tests_expected_strs = []
    codec_strs = []
    missing = []
    for (_, root) in roots:
        classes = root.findall("./classes/iclass")
        encodings = [
            enc for class_ in classes for enc in parse_encodings(
                class_, reads_dst(root))]

        supported_encs = filter_unsupported(encodings)
        patterns = deduplicate_and_sort(map(generate_pattern, supported_encs))

        if to_generate == 'missing':
            missing.extend(filter_supported(encodings))

        if not patterns:
            continue

        if to_generate in ('codec', 'all'):
            header = "\n# {}".format(root.find('./heading').text)
            codec_strs.append('{}\n{}'.format(header, '\n'.join(patterns)))

        if to_generate in ('tests', 'all'):
            asm_tests, api_tests, api_tests_expected = zip(
                *map(generate_test_strings, supported_encs))
            test_strs += deduplicate_and_sort(
                [e for l in asm_tests for e in l])
            api_tests_strs += deduplicate_and_sort(
                [e for l in api_tests for e in l])
            api_tests_expected_strs += deduplicate_and_sort(
                [e for l in api_tests_expected for e in l])

        if to_generate in ('macros', 'all'):
            macro_strs += deduplicate_and_sort(
                map(get_macro_string, supported_encs))

    if codec_strs:
        print('codec.txt:')
        print('\n'.join(codec_strs))

    if test_strs:
        print("dis-a64.txt:")
        print("\n".join(test_strs))

    if api_tests_strs:
        print("\n\nir_aarch64.c:")
        print("\n".join(api_tests_strs))

    if api_tests_expected_strs:
        print("\n\nir_aarch64.expect:")
        print("\n".join(api_tests_expected_strs))

    if macro_strs:
        print("\ninstr_create.h:")
        print("\n".join(macro_strs))

    if missing:
        print('\nMissing encodings:')
        print('\n'.join(missing))

    return 0


if __name__ == '__main__':
    main()
