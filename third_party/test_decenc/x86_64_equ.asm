
#undef FUNCNAME
#define FUNCNAME test_x86_64_equ_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        /* RAW(62) RAW(e1) RAW(76) RAW(08) RAW(58) RAW(c8) */
        /* RAW(62) RAW(b1) RAW(76) RAW(08) RAW(58) RAW(c1) */
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
