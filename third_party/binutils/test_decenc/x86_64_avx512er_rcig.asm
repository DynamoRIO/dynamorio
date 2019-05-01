
#undef FUNCNAME
#define FUNCNAME test_x86_64_avx512er_rcig_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(02) RAW(15) RAW(10) RAW(cb) RAW(f4)
        RAW(62) RAW(02) RAW(95) RAW(10) RAW(cb) RAW(f4)
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(02) RAW(15) RAW(10) RAW(cd) RAW(f4)
        RAW(62) RAW(02) RAW(95) RAW(10) RAW(cd) RAW(f4)
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(02) RAW(15) RAW(10) RAW(cb) RAW(f4)
        RAW(62) RAW(02) RAW(95) RAW(10) RAW(cb) RAW(f4)
        RAW(62) RAW(02) RAW(7d) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(02) RAW(fd) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(02) RAW(15) RAW(10) RAW(cd) RAW(f4)
        RAW(62) RAW(02) RAW(95) RAW(10) RAW(cd) RAW(f4)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
