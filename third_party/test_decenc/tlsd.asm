
#undef FUNCNAME
#define FUNCNAME test_tlsd_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(55)
        RAW(89) RAW(e5)
        RAW(53)
        RAW(50)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(5b)
        RAW(81) RAW(c3) RAW(03) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(04) RAW(1d) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(8d) RAW(83) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(e8) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(83) RAW(c7) RAW(00)
        RAW(8d) RAW(90) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(83) RAW(c6) RAW(00)
        RAW(8d) RAW(88) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8b) RAW(5d) RAW(fc)
        RAW(c9)
        RAW(c3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
