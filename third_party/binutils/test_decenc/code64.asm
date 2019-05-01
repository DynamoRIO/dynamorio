
#undef FUNCNAME
#define FUNCNAME test_code64_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(a0) RAW(21) RAW(43) RAW(65) RAW(87)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        RAW(48)
        RAW(b8) RAW(21) RAW(43) RAW(65) RAW(87)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
