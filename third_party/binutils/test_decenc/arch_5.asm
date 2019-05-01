
#undef FUNCNAME
#define FUNCNAME test_arch_5_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
