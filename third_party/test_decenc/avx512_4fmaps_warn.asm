
#undef FUNCNAME
#define FUNCNAME test_avx512_4fmaps_warn_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f2) RAW(7f) RAW(48) RAW(9a) RAW(30)
        RAW(62) RAW(f2) RAW(77) RAW(48) RAW(9a) RAW(30)
        RAW(62) RAW(f2) RAW(6f) RAW(48) RAW(9a) RAW(30)
        RAW(62) RAW(f2) RAW(67) RAW(48) RAW(9a) RAW(30)
        RAW(62) RAW(f2) RAW(5f) RAW(48) RAW(9a) RAW(30)
        RAW(62) RAW(f2) RAW(7f) RAW(48) RAW(aa) RAW(30)
        RAW(62) RAW(f2) RAW(77) RAW(48) RAW(aa) RAW(30)
        RAW(62) RAW(f2) RAW(6f) RAW(48) RAW(aa) RAW(30)
        RAW(62) RAW(f2) RAW(67) RAW(48) RAW(aa) RAW(30)
        RAW(62) RAW(f2) RAW(5f) RAW(48) RAW(aa) RAW(30)
        RAW(62) RAW(f2) RAW(7f) RAW(08) RAW(9b) RAW(30)
        RAW(62) RAW(f2) RAW(77) RAW(08) RAW(9b) RAW(30)
        RAW(62) RAW(f2) RAW(6f) RAW(08) RAW(9b) RAW(30)
        RAW(62) RAW(f2) RAW(67) RAW(08) RAW(9b) RAW(30)
        RAW(62) RAW(f2) RAW(5f) RAW(08) RAW(9b) RAW(30)
        RAW(62) RAW(f2) RAW(7f) RAW(08) RAW(ab) RAW(30)
        RAW(62) RAW(f2) RAW(77) RAW(08) RAW(ab) RAW(30)
        RAW(62) RAW(f2) RAW(6f) RAW(08) RAW(ab) RAW(30)
        RAW(62) RAW(f2) RAW(67) RAW(08) RAW(ab) RAW(30)
        RAW(62) RAW(f2) RAW(5f) RAW(08) RAW(ab) RAW(30)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
