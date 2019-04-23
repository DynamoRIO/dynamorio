
#undef FUNCNAME
#define FUNCNAME test_avx512er_rcig_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(f2) RAW(55) RAW(1f) RAW(cb) RAW(f4)
        RAW(62) RAW(f2) RAW(d5) RAW(1f) RAW(cb) RAW(f4)
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(f2) RAW(55) RAW(1f) RAW(cd) RAW(f4)
        RAW(62) RAW(f2) RAW(d5) RAW(1f) RAW(cd) RAW(f4)
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(c8) RAW(f5)
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(ca) RAW(f5)
        RAW(62) RAW(f2) RAW(55) RAW(1f) RAW(cb) RAW(f4)
        RAW(62) RAW(f2) RAW(d5) RAW(1f) RAW(cb) RAW(f4)
        RAW(62) RAW(f2) RAW(7d) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(f2) RAW(fd) RAW(18) RAW(cc) RAW(f5)
        RAW(62) RAW(f2) RAW(55) RAW(1f) RAW(cd) RAW(f4)
        RAW(62) RAW(f2) RAW(d5) RAW(1f) RAW(cd) RAW(f4)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
