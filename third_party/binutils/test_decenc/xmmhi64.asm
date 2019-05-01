
#undef FUNCNAME
#define FUNCNAME test_xmmhi64_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(b1) RAW(74) RAW(08) RAW(58) RAW(c0)
        RAW(62) RAW(b1) RAW(74) RAW(28) RAW(58) RAW(c0)
        RAW(62) RAW(b1) RAW(74) RAW(48) RAW(58) RAW(c0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
