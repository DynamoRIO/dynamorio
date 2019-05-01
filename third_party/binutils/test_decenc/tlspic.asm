
#undef FUNCNAME
#define FUNCNAME test_tlspic_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(55)
        RAW(89) RAW(e5)
        RAW(53)
        RAW(50)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(5b)
        RAW(81) RAW(c3) RAW(03) RAW(00) RAW(00) RAW(00)
        RAW(65) RAW(a1) RAW(00) RAW(00) RAW(00) RAW(00) RAW(83)
        RAW(c6) RAW(00) RAW(2b)
        RAW(83) RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(8b) RAW(83) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(83) RAW(c6) RAW(00)
        RAW(65) RAW(8b) RAW(00)
        RAW(65) RAW(8b) RAW(0d) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(03) RAW(8b) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(5d) RAW(fc)
        RAW(c9)
        RAW(c3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
