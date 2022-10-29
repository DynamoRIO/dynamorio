# Instruction Set Listing

## Format

```text
<mnemonic> | <instruction-format> | [field] | <fixedbits>
```

The file name will become the extension.
There is a convention to specify fields in the reverse order of the assembly.
This way the instruction macros are generated properly.

If you want to add a new extension, add a new value to the `riscv64_isa_ext_t`
enum in `codec.h`.

When adding a new instruction make sure to update the `get_rvc_instr_info()` function in `codec.c` to properly decode the opcode.


## Example

```text
add    | r  | rs2 rs1 rd    | 0000000..........000.....0110011
c.lwsp | ci | clwsp_imm crd | 010...........10
```


## Instruction Format

The instruction format should be one of the following:

- r
- r4
- i
- s
- b
- u
- j
- cr
- ci
- css
- ciw
- cl
- cs
- ca
- cb
- cj

This maps to `riscv64_inst_fmt_t` enum in `codec.h` and `Format` enum in
`codec.py` generator. However at this point the format field is not put into
generated `instr_info_t` structures. This may change in the future, so in case
of a new format, keep the above definitions in sync.


## Field

The list of available fields are written below.

For non-compressed instructions:

- rd
- rd(fp)
- rs1
- rs1(fp)
- base
- rs2
- rs2(fp)
- rs3
- fm
- pred
- succ
- aqrl
- csr
- rm
- shamt
- shamt5
- shamt6
- csr_imm
- i_imm
- s_imm
- b_imm
- u_imm
- j_imm
- imm

For compressed instructions:

- crd
- crd(fp)
- crd_
- crd_(fp)
- crd__
- crs1
- crs1_
- crs2
- crs2(fp)
- crs2_
- crs2_(fp)
- cshamt
- caddi16sp_imm
- clwsp_imm
- cldsp_imm
- clui_imm
- cswsp_imm
- csdsp_imm
- ciw_imm
- clw_imm
- cld_imm
- csw_imm
- csd_imm
- cimm5
- cb_imm
- cj_imm

This maps into `riscv64_fld_t` enum in `codec.h` and `Field` enum in `codec.py`
generator.

If you want to add a new field:

1. In `codec.py`:

    1. Add new value to the `Field` enum. The value definition has the following
       members:

       - `value`: Enum value (keep this in sync with `riscv_fld_t` type in
         `codec.h`).
       - `arg_name`: Name to use in instruction creation macros.
       - `is_dest`: True if it is a destination operand.
       - `opsz_def`: Operand size (`OPSZ_*` value) or if this field decodes into
         an operand of a different size depending on instruction - dictionary
         indexed by instruction mnemonic with operand size values.
       - `asm_name`: Name used in assembly comments (if different than
         `arg_name`)
       - `arg_cmt`: Comment for instruction creation macros for this field.

    2. In case the new field encodes an offset for an explicit GPR field-based
       memory reference, update an appropriate `__fixup_*_inst()` function. See
       the `Field.V_L_RS1_DISP` logic for overriding semantics of a field that
       is used as a normal immediate in other cases.

2. Add a new `riscv64_fld_t` enum value in `codec.h`.
3. In `codec.c`:

    1. Add a new operand decode function (see `decode_rd_opnd()` for reference).
       The decode function gets the operand size `op_sz` as defined in
       `codec.py` as well as the source or destination index `idx` of this
       operand.
    2. Add the decode function to the `opnd_decoders` array at the index equal
       to the new field's `riscv64_fld_t` value.
