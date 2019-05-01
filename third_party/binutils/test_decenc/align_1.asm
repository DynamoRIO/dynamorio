
#undef FUNCNAME
#define FUNCNAME test_align_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(89) RAW(f8)
        RAW(8d) RAW(b6) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ba) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(76) RAW(00)
        RAW(01) RAW(c2)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
