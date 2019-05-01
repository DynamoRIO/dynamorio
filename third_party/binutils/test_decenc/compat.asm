
#undef FUNCNAME
#define FUNCNAME test_compat_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(dc) RAW(e3)
        RAW(de) RAW(e1)
        RAW(de) RAW(e3)
        RAW(de) RAW(e3)
        RAW(dc) RAW(eb)
        RAW(de) RAW(e9)
        RAW(de) RAW(eb)
        RAW(de) RAW(eb)
        RAW(dc) RAW(f3)
        RAW(de) RAW(f1)
        RAW(de) RAW(f3)
        RAW(de) RAW(f3)
        RAW(dc) RAW(fb)
        RAW(de) RAW(f9)
        RAW(de) RAW(fb)
        RAW(de) RAW(fb)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
