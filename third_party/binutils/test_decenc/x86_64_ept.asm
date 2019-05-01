
#undef FUNCNAME
#define FUNCNAME test_x86_64_ept_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(38) RAW(80) RAW(19)
        RAW(66) RAW(44) RAW(0f) RAW(38) RAW(80) RAW(19)
        RAW(66) RAW(0f) RAW(38) RAW(81) RAW(19)
        RAW(66) RAW(44) RAW(0f) RAW(38) RAW(81) RAW(19)
        RAW(66) RAW(0f) RAW(38) RAW(80) RAW(19)
        RAW(66) RAW(44) RAW(0f) RAW(38) RAW(80) RAW(19)
        RAW(66) RAW(0f) RAW(38) RAW(81) RAW(19)
        RAW(66) RAW(44) RAW(0f) RAW(38) RAW(81) RAW(19)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
