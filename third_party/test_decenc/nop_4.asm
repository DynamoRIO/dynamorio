
#undef FUNCNAME
#define FUNCNAME test_nop_4_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(31) RAW(c0)
        RAW(85) RAW(c0)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(90)
        RAW(31) RAW(c0)
        RAW(31) RAW(c0)
        RAW(89) RAW(c0)
        RAW(89) RAW(c0)
        RAW(89) RAW(c0)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
