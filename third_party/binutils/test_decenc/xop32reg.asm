
#undef FUNCNAME
#define FUNCNAME test_xop32reg_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(8f) RAW(e9) RAW(78) RAW(e1) RAW(4d) RAW(c2)
        RAW(8f) RAW(c9) RAW(78) RAW(e1) RAW(4d) RAW(c2)
        RAW(8f) RAW(e8) RAW(40) RAW(cd) RAW(04) RAW(08) RAW(07)
        RAW(8f) RAW(c8) RAW(40) RAW(cd) RAW(04) RAW(08) RAW(07)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
