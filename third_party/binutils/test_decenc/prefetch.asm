
#undef FUNCNAME
#define FUNCNAME test_prefetch_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(0d) RAW(00)
        RAW(0f) RAW(0d) RAW(08)
        /* FIXME i#3577: AMD prefetchwt1 opcode (happens to match Xeon Phi prefetchwt1
         * as well.
         */
        /* RAW(0f) RAW(0d) RAW(10) */
        /* FIXME i#3577: AMD prefetch opcode. */
        /* RAW(0f) RAW(0d) RAW(18) */
        /* RAW(0f) RAW(0d) RAW(20) */
        /* RAW(0f) RAW(0d) RAW(28) */
        /* RAW(0f) RAW(0d) RAW(30) */
        /* RAW(0f) RAW(0d) RAW(38) */
        RAW(0f) RAW(18) RAW(00)
        RAW(0f) RAW(18) RAW(08)
        RAW(0f) RAW(18) RAW(10)
        RAW(0f) RAW(18) RAW(18)
        RAW(0f) RAW(18) RAW(20)
        RAW(0f) RAW(18) RAW(28)
        RAW(0f) RAW(18) RAW(30)
        RAW(0f) RAW(18) RAW(38)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
