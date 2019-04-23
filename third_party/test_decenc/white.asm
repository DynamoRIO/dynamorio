
#undef FUNCNAME
#define FUNCNAME test_white_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(36) RAW(88) RAW(03)
        RAW(c7) RAW(05) RAW(d7) RAW(11) RAW(00) RAW(00) RAW(7b)
        RAW(00) RAW(00) RAW(00)
        RAW(67) RAW(8a) RAW(78) RAW(7b)
        RAW(ff) RAW(e0)
        RAW(26) RAW(66) RAW(ff) RAW(23)
        RAW(a0) RAW(50) RAW(00) RAW(00) RAW(00) RAW(b0) RAW(20)
        RAW(b7) RAW(13)
        RAW(b7) RAW(13)
        RAW(66) RAW(b8) RAW(13) RAW(00)
        RAW(00) RAW(00)
        RAW(66) RAW(b8) RAW(13) RAW(00)
        RAW(d9) RAW(c9)
        RAW(d9) RAW(c9)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
