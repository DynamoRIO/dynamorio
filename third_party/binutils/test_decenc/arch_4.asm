
#undef FUNCNAME
#define FUNCNAME test_arch_4_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        /* XXX i#3578, ud0 w/ modrm unsupported. */
        /* RAW(0f) RAW(ff) RAW(07) */
        RAW(0f) RAW(b9) RAW(07)
        RAW(0f) RAW(0b)
        RAW(0f) RAW(0b)
        RAW(0f) RAW(b9) RAW(07)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
