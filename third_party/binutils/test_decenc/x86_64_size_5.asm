
#undef FUNCNAME
#define FUNCNAME test_x86_64_size_5_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(49) RAW(bf) RAW(00) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(00) RAW(00) RAW(00)
        RAW(49) RAW(bf) RAW(00) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(00) RAW(00) RAW(00)
        RAW(49) RAW(bf) RAW(00) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
