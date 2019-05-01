
#undef FUNCNAME
#define FUNCNAME test_arch_12_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(0f) RAW(c1) RAW(bb)
        RAW(0f) RAW(da) RAW(c1)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
