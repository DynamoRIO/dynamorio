
#undef FUNCNAME
#define FUNCNAME test_optimize_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(a9) RAW(7f) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(a9) RAW(7f) RAW(00)
        RAW(a8) RAW(7f)
        RAW(f7) RAW(c3) RAW(7f) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(f7) RAW(c3) RAW(7f) RAW(00)
        RAW(f6) RAW(c3) RAW(7f)
        RAW(f7) RAW(c7) RAW(7f) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(f7) RAW(c7) RAW(7f) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
