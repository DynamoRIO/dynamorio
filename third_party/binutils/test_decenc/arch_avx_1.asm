
#undef FUNCNAME
#define FUNCNAME test_arch_avx_1_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(c4) RAW(e2) RAW(79) RAW(dc) RAW(11)
        RAW(c4) RAW(e3) RAW(49) RAW(44) RAW(d4) RAW(08)
        /* FIXME i#3578: Support AVX instruction vgf2p8mulb. */
        /* RAW(c4) RAW(e2) RAW(69) RAW(cf) RAW(d9) */
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
