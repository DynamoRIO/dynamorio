
#undef FUNCNAME
#define FUNCNAME test_avx512f_vaes_wig_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(de) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(df) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dc) RAW(72) RAW(7f)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(f4)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(b4) RAW(f4)
        RAW(c0) RAW(1d) RAW(fe) RAW(ff)
        RAW(62) RAW(f2) RAW(55) RAW(48) RAW(dd) RAW(72) RAW(7f)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
