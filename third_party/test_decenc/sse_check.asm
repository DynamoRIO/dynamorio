
#undef FUNCNAME
#define FUNCNAME test_sse_check_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(58) RAW(ca)
        RAW(66) RAW(0f) RAW(58) RAW(ca)
        RAW(66) RAW(0f) RAW(d0) RAW(ca)
        RAW(66) RAW(0f) RAW(38) RAW(01) RAW(ca)
        RAW(66) RAW(0f) RAW(38) RAW(15) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(37) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(44) RAW(d1) RAW(ff)
        RAW(66) RAW(0f) RAW(38) RAW(de) RAW(d1)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
