
#undef FUNCNAME
#define FUNCNAME test_vaes_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(c4) RAW(e2) RAW(4d) RAW(dc) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(dc) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(dd) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(dd) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(de) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(de) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(df) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(df) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(dc) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(dc) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(dc) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(dd) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(dd) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(dd) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(de) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(de) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(de) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(df) RAW(d4)
        RAW(c4) RAW(e2) RAW(4d) RAW(df) RAW(39)
        RAW(c4) RAW(e2) RAW(4d) RAW(df) RAW(39)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
