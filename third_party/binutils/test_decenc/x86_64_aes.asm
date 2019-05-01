
#undef FUNCNAME
#define FUNCNAME test_x86_64_aes_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(66) RAW(0f) RAW(38) RAW(dc) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(dc) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(dd) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(dd) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(de) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(de) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(df) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(df) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(db) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(db) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(df) RAW(01) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(df) RAW(c1) RAW(08)
        RAW(66) RAW(0f) RAW(38) RAW(dc) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(dc) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(dd) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(dd) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(de) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(de) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(df) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(df) RAW(c1)
        RAW(66) RAW(0f) RAW(38) RAW(db) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(db) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(df) RAW(01) RAW(08)
        RAW(66) RAW(0f) RAW(3a) RAW(df) RAW(c1) RAW(08)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
