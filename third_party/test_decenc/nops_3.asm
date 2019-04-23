
#undef FUNCNAME
#define FUNCNAME test_nops_3_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(90)
        RAW(eb) RAW(1d)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(90)
        RAW(89) RAW(c3)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(8d) RAW(b4) RAW(26) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
