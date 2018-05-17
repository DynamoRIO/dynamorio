Usage
-----

The script should work with both Python 2 and 3 and depends on
(Click)[http://click.pocoo.org] for argument handling. Just install it in a
virtualenv.

The script can generate entries for codec.txt, dis-a64.txt ir_aarch64.c,
ir_aarch64.expect and instr_create.h for a limited subset of the Armv8-a ISA.
It takes a directory with the XML ISA spec as input. You can get the latest
XML ISA spec from https://developer.arm.com/products/architecture/a-profile/exploration-tools

All output is printed to stdout and needs to be added to the appropriate files.

Example uses:
    # Generate all supported entries for all supported encodings of FMLA.
    python main.py  --only=fmla --to-generate=all ~/ISA_v83A_A64_xml_00bet6

    # Generate all supported entries for all supported encodings.
    python main.py  --to-generate=all ~/ISA_v83A_A64_xml_00bet6

    # Generate codec.txt entries for all supported encodings.
    python main.py  --to-generate=codec ~/ISA_v83A_A64_xml_00bet6
