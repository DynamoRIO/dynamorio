
#undef FUNCNAME
#define FUNCNAME test_svme_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(0f) RAW(01) RAW(dd)
        RAW(0f) RAW(01) RAW(df)
        RAW(0f) RAW(01) RAW(de)
        RAW(0f) RAW(01) RAW(dc)
        RAW(0f) RAW(01) RAW(da)
        RAW(0f) RAW(01) RAW(d9)
        RAW(0f) RAW(01) RAW(d8)
        RAW(0f) RAW(01) RAW(db)
        RAW(0f) RAW(01) RAW(de)
        RAW(0f) RAW(01) RAW(df)
        RAW(0f) RAW(01) RAW(da)
        RAW(0f) RAW(01) RAW(d8)
        RAW(0f) RAW(01) RAW(db)
        RAW(0f) RAW(01) RAW(de)
        RAW(0f) RAW(01) RAW(df)
        RAW(0f) RAW(01) RAW(da)
        RAW(0f) RAW(01) RAW(d8)
        RAW(0f) RAW(01) RAW(db)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
