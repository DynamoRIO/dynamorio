
#undef FUNCNAME
#define FUNCNAME test_xsave_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(ae) RAW(2b)
        RAW(0f) RAW(ae) RAW(23)
        RAW(0f) RAW(ae) RAW(33)
        RAW(0f) RAW(01) RAW(d0)
        RAW(0f) RAW(01) RAW(d1)
        RAW(0f) RAW(ae) RAW(29)
        RAW(0f) RAW(ae) RAW(21)
        RAW(0f) RAW(ae) RAW(31)
        RAW(0f) RAW(ae) RAW(20)
        RAW(0f) RAW(ae) RAW(28)
        RAW(0f) RAW(ae) RAW(20)
        RAW(0f) RAW(ae) RAW(28)
        RAW(0f) RAW(ae) RAW(20)
        RAW(0f) RAW(ae) RAW(28)
        RAW(0f) RAW(ae) RAW(20)
        RAW(0f) RAW(ae) RAW(28)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
