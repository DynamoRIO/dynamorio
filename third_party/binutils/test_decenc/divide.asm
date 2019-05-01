
#undef FUNCNAME
#define FUNCNAME test_divide_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(01) RAW(00)
        RAW(00) RAW(00)
        RAW(02) RAW(00)
        RAW(00) RAW(00)
        RAW(03) RAW(00)
        RAW(00) RAW(00)
        RAW(04) RAW(00)
        RAW(00) RAW(00)
        RAW(05)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
