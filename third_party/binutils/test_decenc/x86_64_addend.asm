
#undef FUNCNAME
#define FUNCNAME test_x86_64_addend_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(48) RAW(c7) RAW(c0) RAW(00) RAW(00) RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
