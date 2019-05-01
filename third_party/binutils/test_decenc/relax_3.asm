
#undef FUNCNAME
#define FUNCNAME test_relax_3_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(eb) RAW(21)
        RAW(eb) RAW(1b)
        RAW(eb) RAW(1b)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(e9) RAW(fc) RAW(ff) RAW(ff) RAW(ff)
        RAW(c3)
        RAW(c3)
        RAW(c3)
        RAW(c3)
        RAW(c3)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
