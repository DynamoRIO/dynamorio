
#undef FUNCNAME
#define FUNCNAME test_size_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(b8) RAW(50) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(48) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(58) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(1e) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(0e) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(2e) RAW(00) RAW(00) RAW(00)
        RAW(b8) RAW(90) RAW(99) RAW(99) RAW(19)
        RAW(b8) RAW(70) RAW(99) RAW(99) RAW(19)
        RAW(b8) RAW(b0) RAW(99) RAW(99) RAW(19)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
