
#undef FUNCNAME
#define FUNCNAME test_cldemote_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(1c) RAW(01)
        RAW(0f) RAW(1c) RAW(84) RAW(f4) RAW(c0) RAW(1d) RAW(fe)
        RAW(ff)
        RAW(0f) RAW(1c) RAW(01)
        RAW(0f) RAW(1c) RAW(84) RAW(f4) RAW(c0) RAW(1d) RAW(fe)
        RAW(ff)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
