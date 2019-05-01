
#undef FUNCNAME
#define FUNCNAME test_tlsnopic_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(8b) RAW(15) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(81) RAW(c2) RAW(08) RAW(00) RAW(00) RAW(00)
        RAW(65) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(2b)
        RAW(82) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(65) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(2d)
        RAW(00) RAW(00) RAW(00)
        RAW(00) RAW(65) RAW(8b)
        RAW(0d) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(81) RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(65) RAW(8b) RAW(0d) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(90)
        RAW(8d) RAW(81) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(8d) RAW(91) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(65) RAW(8b)
        RAW(00) RAW(65)
        RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(03) RAW(05)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(c3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
