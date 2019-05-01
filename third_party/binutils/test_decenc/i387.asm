
#undef FUNCNAME
#define FUNCNAME test_i387_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(d9) RAW(ff)
        RAW(d9) RAW(f5)
        RAW(d9) RAW(fe)
        RAW(d9) RAW(fb)
        RAW(dd) RAW(e1)
        RAW(dd) RAW(e9)
        RAW(da) RAW(e9)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
