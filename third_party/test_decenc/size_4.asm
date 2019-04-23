
#undef FUNCNAME
#define FUNCNAME test_size_4_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(b8) RAW(50) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(48) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(58) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(1e) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(0e) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(2e) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
