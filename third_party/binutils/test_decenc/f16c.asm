
#undef FUNCNAME
#define FUNCNAME test_f16c_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(c4) RAW(e2) RAW(7d) RAW(13) RAW(e4)
        RAW(c4) RAW(e2) RAW(7d) RAW(13) RAW(21)
        RAW(c4) RAW(e2) RAW(79) RAW(13) RAW(f4)
        RAW(c4) RAW(e2) RAW(79) RAW(13) RAW(21)
        RAW(c4) RAW(e3) RAW(7d) RAW(1d) RAW(e4) RAW(02)
        RAW(c4) RAW(e3) RAW(7d) RAW(1d) RAW(21) RAW(02)
        RAW(c4) RAW(e3) RAW(79) RAW(1d) RAW(e4) RAW(02)
        RAW(c4) RAW(e3) RAW(79) RAW(1d) RAW(21) RAW(02)
        RAW(c4) RAW(e2) RAW(7d) RAW(13) RAW(e4)
        RAW(c4) RAW(e2) RAW(7d) RAW(13) RAW(21)
        RAW(c4) RAW(e2) RAW(7d) RAW(13) RAW(21)
        RAW(c4) RAW(e2) RAW(79) RAW(13) RAW(f4)
        RAW(c4) RAW(e2) RAW(79) RAW(13) RAW(21)
        RAW(c4) RAW(e2) RAW(79) RAW(13) RAW(21)
        RAW(c4) RAW(e3) RAW(7d) RAW(1d) RAW(e4) RAW(02)
        RAW(c4) RAW(e3) RAW(7d) RAW(1d) RAW(21) RAW(02)
        RAW(c4) RAW(e3) RAW(7d) RAW(1d) RAW(21) RAW(02)
        RAW(c4) RAW(e3) RAW(79) RAW(1d) RAW(e4) RAW(02)
        RAW(c4) RAW(e3) RAW(79) RAW(1d) RAW(21) RAW(02)
        RAW(c4) RAW(e3) RAW(79) RAW(1d) RAW(21) RAW(02)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
