
#undef FUNCNAME
#define FUNCNAME test_x86_64_gidt_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(01) RAW(08)
        RAW(0f) RAW(01) RAW(18)
        RAW(0f) RAW(01) RAW(00)
        RAW(0f) RAW(01) RAW(10)
        RAW(0f) RAW(01) RAW(08)
        RAW(0f) RAW(01) RAW(18)
        RAW(0f) RAW(01) RAW(00)
        RAW(0f) RAW(01) RAW(10)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
