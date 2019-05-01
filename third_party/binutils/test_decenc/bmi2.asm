
#undef FUNCNAME
#define FUNCNAME test_bmi2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(c4) RAW(e3) RAW(7b) RAW(f0) RAW(d8) RAW(07)
        RAW(c4) RAW(e3) RAW(7b) RAW(f0) RAW(19) RAW(07)
        /* FIXME i#3578: Support mulx. */
        /* RAW(c4) RAW(e2) RAW(63) RAW(f6) RAW(f0) */
        /* RAW(c4) RAW(e2) RAW(63) RAW(f6) RAW(31) */
        RAW(c4) RAW(e2) RAW(63) RAW(f5) RAW(f0)
        RAW(c4) RAW(e2) RAW(63) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(62) RAW(f5) RAW(f0)
        RAW(c4) RAW(e2) RAW(62) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(78) RAW(f5) RAW(f3)
        RAW(c4) RAW(e2) RAW(60) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(7a) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(62) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(79) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(61) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(7b) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(63) RAW(f7) RAW(31)
        RAW(c4) RAW(e3) RAW(7b) RAW(f0) RAW(d8) RAW(07)
        RAW(c4) RAW(e3) RAW(7b) RAW(f0) RAW(19) RAW(07)
        RAW(c4) RAW(e3) RAW(7b) RAW(f0) RAW(19) RAW(07)
        /* FIXME i#3578: Support mulx. */
        /* RAW(c4) RAW(e2) RAW(63) RAW(f6) RAW(f0) */
        /* RAW(c4) RAW(e2) RAW(63) RAW(f6) RAW(31) */
        /* RAW(c4) RAW(e2) RAW(63) RAW(f6) RAW(31) */
        RAW(c4) RAW(e2) RAW(63) RAW(f5) RAW(f0)
        RAW(c4) RAW(e2) RAW(63) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(63) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(62) RAW(f5) RAW(f0)
        RAW(c4) RAW(e2) RAW(62) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(62) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(78) RAW(f5) RAW(f3)
        RAW(c4) RAW(e2) RAW(60) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(60) RAW(f5) RAW(31)
        RAW(c4) RAW(e2) RAW(7a) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(62) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(62) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(79) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(61) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(61) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(7b) RAW(f7) RAW(f3)
        RAW(c4) RAW(e2) RAW(63) RAW(f7) RAW(31)
        RAW(c4) RAW(e2) RAW(63) RAW(f7) RAW(31)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
