
#undef FUNCNAME
#define FUNCNAME test_x86_64_drx_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(44) RAW(0f) RAW(21) RAW(c0)
        RAW(44) RAW(0f) RAW(21) RAW(c7)
        RAW(44) RAW(0f) RAW(23) RAW(c0)
        RAW(44) RAW(0f) RAW(23) RAW(c7)
        RAW(44) RAW(0f) RAW(21) RAW(c0)
        RAW(44) RAW(0f) RAW(21) RAW(c7)
        RAW(44) RAW(0f) RAW(23) RAW(c0)
        RAW(44) RAW(0f) RAW(23) RAW(c7)
        RAW(44) RAW(0f) RAW(21) RAW(c0)
        RAW(44) RAW(0f) RAW(21) RAW(c7)
        RAW(44) RAW(0f) RAW(23) RAW(c0)
        RAW(44) RAW(0f) RAW(23) RAW(c7)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
