
#undef FUNCNAME
#define FUNCNAME test_intel16_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(bf) RAW(06)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(be) RAW(06)
        RAW(00) RAW(00)
        RAW(0f) RAW(be) RAW(06)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(b7) RAW(06)
        RAW(00) RAW(00)
        RAW(66) RAW(0f) RAW(b6) RAW(06)
        RAW(00) RAW(00)
        RAW(0f) RAW(b6) RAW(06)
        RAW(00) RAW(00)
        RAW(8d) RAW(00)
        RAW(8d) RAW(02)
        RAW(8d) RAW(01)
        RAW(8d) RAW(03)
        RAW(8d) RAW(00)
        RAW(8d) RAW(02)
        RAW(8d) RAW(01)
        RAW(8d) RAW(03)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
