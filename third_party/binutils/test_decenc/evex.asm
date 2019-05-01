
#undef FUNCNAME
#define FUNCNAME test_evex_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(62) RAW(f1) RAW(d6) RAW(38) RAW(2a) RAW(f0)
        RAW(62) RAW(f1) RAW(d7) RAW(38) RAW(2a) RAW(f0)
        RAW(62) RAW(f1) RAW(d6) RAW(08) RAW(7b) RAW(f0)
        RAW(62) RAW(f1) RAW(d7) RAW(08) RAW(7b) RAW(f0)
        RAW(62) RAW(f1) RAW(d6) RAW(38) RAW(7b) RAW(f0)
        RAW(62) RAW(f1) RAW(d7) RAW(38) RAW(7b) RAW(f0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
