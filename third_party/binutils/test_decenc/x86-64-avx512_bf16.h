// clang-format off
byte bf16_test00[] = { 0x62, 0x02, 0x17, 0x40, 0x72, 0xf4,}; //
byte bf16_test01[] = { 0x62, 0x22, 0x17, 0x47, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
byte bf16_test02[] = { 0x62, 0x42, 0x17, 0x50, 0x72, 0x31,}; //
byte bf16_test03[] = { 0x62, 0x62, 0x17, 0x40, 0x72, 0x71, 0x7f,}; //
byte bf16_test04[] = { 0x62, 0x62, 0x17, 0xd7, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
byte bf16_test05[] = { 0x62, 0x02, 0x7e, 0x48, 0x72, 0xf5,}; //
byte bf16_test06[] = { 0x62, 0x22, 0x7e, 0x4f, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
byte bf16_test07[] = { 0x62, 0x42, 0x7e, 0x58, 0x72, 0x31,}; //
byte bf16_test08[] = { 0x62, 0x62, 0x7e, 0x48, 0x72, 0x71, 0x7f,}; //
byte bf16_test09[] = { 0x62, 0x62, 0x7e, 0xdf, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
byte bf16_test10[] = { 0x62, 0x02, 0x16, 0x40, 0x52, 0xf4,}; //
byte bf16_test11[] = { 0x62, 0x22, 0x16, 0x47, 0x52, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
byte bf16_test12[] = { 0x62, 0x42, 0x16, 0x50, 0x52, 0x31,}; //
byte bf16_test13[] = { 0x62, 0x62, 0x16, 0x40, 0x52, 0x71, 0x7f,}; //
byte bf16_test14[] = { 0x62, 0x62, 0x16, 0xd7, 0x52, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //

ENCODE_TEST_4args(bf16_test00, vcvtne2ps2bf16_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), REGARG (ZMM28));
ENCODE_TEST_4args(bf16_test01, vcvtne2ps2bf16_mask, 0, REGARG (ZMM30), REGARG(K7), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14, 8, 0x10000000, OPSZ_64));
ENCODE_TEST_4args(bf16_test02, vcvtne2ps2bf16_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), opnd_create_base_disp(DR_REG_R9, DR_REG_NULL, 0, 0, OPSZ_4));
ENCODE_TEST_4args(bf16_test03, vcvtne2ps2bf16_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0, 0x1fc0, OPSZ_64));
ENCODE_TEST_4args(bf16_test04, vcvtne2ps2bf16_mask, ENCODE_FLAG_Z, REGARG (ZMM30), REGARG(K7), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, 0xffffe000, OPSZ_4));
ENCODE_TEST_3args(bf16_test05, vcvtneps2bf16_mask, ENCODE_FLAG_SET_DST_SIZE_HALF, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29));
ENCODE_TEST_3args(bf16_test06, vcvtneps2bf16_mask, ENCODE_FLAG_SET_DST_SIZE_HALF, REGARG (ZMM30), REGARG(K7), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14, 8, 0x10000000, OPSZ_64));
ENCODE_TEST_3args(bf16_test07, vcvtneps2bf16_mask, ENCODE_FLAG_SET_DST_SIZE_HALF, REGARG (ZMM30), REGARG(K0), opnd_create_base_disp(DR_REG_R9, DR_REG_NULL, 0, 0, OPSZ_4));
ENCODE_TEST_3args(bf16_test08, vcvtneps2bf16_mask, ENCODE_FLAG_SET_DST_SIZE_HALF, REGARG (ZMM30), REGARG(K0), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0, 0x1fc0, OPSZ_64));
ENCODE_TEST_3args(bf16_test09, vcvtneps2bf16_mask, ENCODE_FLAG_SET_DST_SIZE_HALF|ENCODE_FLAG_Z, REGARG (ZMM30), REGARG(K7), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, 0xffffe000, OPSZ_4));
ENCODE_TEST_4args(bf16_test10, vdpbf16ps_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), REGARG (ZMM28));
ENCODE_TEST_4args(bf16_test11, vdpbf16ps_mask, 0, REGARG (ZMM30), REGARG(K7), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14, 8, 0x10000000, OPSZ_64));
ENCODE_TEST_4args(bf16_test12, vdpbf16ps_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), opnd_create_base_disp(DR_REG_R9, DR_REG_NULL, 0, 0, OPSZ_4));
ENCODE_TEST_4args(bf16_test13, vdpbf16ps_mask, 0, REGARG (ZMM30), REGARG(K0), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0, 0x1fc0, OPSZ_64));
ENCODE_TEST_4args(bf16_test14, vdpbf16ps_mask, ENCODE_FLAG_Z, REGARG (ZMM30), REGARG(K7), REGARG (ZMM29), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, 0xffffe000, OPSZ_4));
