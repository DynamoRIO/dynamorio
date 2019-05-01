
#undef FUNCNAME
#define FUNCNAME test_intelpic_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(c3)
        RAW(8d) RAW(83) RAW(14) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(83) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(24) RAW(85) RAW(0d) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(83) RAW(14) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(24) RAW(85) RAW(0d) RAW(10) RAW(00) RAW(00)
        RAW(ff) RAW(24) RAW(85) RAW(28) RAW(10) RAW(00) RAW(00)
        RAW(90)
        RAW(ff) RAW(24) RAW(85) RAW(29) RAW(10) RAW(00) RAW(00)
        RAW(ff) RAW(24) RAW(85) RAW(37) RAW(10) RAW(00) RAW(00)
        RAW(90)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
