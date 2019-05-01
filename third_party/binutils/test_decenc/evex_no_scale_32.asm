
#undef FUNCNAME
#define FUNCNAME test_evex_no_scale_32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(62) RAW(f1) RAW(7c) RAW(48) RAW(28) RAW(04) RAW(05)
        RAW(40) RAW(00) RAW(00) RAW(00)
        RAW(62) RAW(f1) RAW(7c) RAW(48) RAW(28) RAW(04) RAW(25)
        RAW(40) RAW(00) RAW(00) RAW(00)
        RAW(62) RAW(f1) RAW(7c) RAW(48) RAW(28) RAW(05) RAW(40)
        RAW(00) RAW(00) RAW(00)
        RAW(67) RAW(62) RAW(f1) RAW(7c) RAW(48) RAW(28) RAW(06)
        RAW(40)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
