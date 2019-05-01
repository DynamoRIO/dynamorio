
#undef FUNCNAME
#define FUNCNAME test_x86_64_avx512f_vaes_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(de) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(de) RAW(b4) RAW(f0)
        RAW(23) RAW(01) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(df) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(df) RAW(b4) RAW(f0)
        RAW(23) RAW(01) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(dc) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(dc) RAW(b4) RAW(f0)
        RAW(23) RAW(01) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(dd) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(dd) RAW(b4) RAW(f0)
        RAW(23) RAW(01) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(de) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(de) RAW(b4) RAW(f0)
        RAW(34) RAW(12) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(df) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(df) RAW(b4) RAW(f0)
        RAW(34) RAW(12) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(dc) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(dc) RAW(b4) RAW(f0)
        RAW(34) RAW(12) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(72) RAW(7f)
        RAW(62) RAW(02) RAW(15) RAW(40) RAW(dd) RAW(f4)
        RAW(62) RAW(22) RAW(15) RAW(40) RAW(dd) RAW(b4) RAW(f0)
        RAW(34) RAW(12) RAW(00) RAW(00)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(72) RAW(7f)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
