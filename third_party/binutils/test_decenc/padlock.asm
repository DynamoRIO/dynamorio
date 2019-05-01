
#undef FUNCNAME
#define FUNCNAME test_padlock_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(a7) RAW(c0)
        RAW(f3) RAW(0f) RAW(a7) RAW(c0)
        RAW(f3) RAW(0f) RAW(a7) RAW(c8)
        RAW(f3) RAW(0f) RAW(a7) RAW(c8)
        RAW(f3) RAW(0f) RAW(a7) RAW(d0)
        RAW(f3) RAW(0f) RAW(a7) RAW(d0)
        RAW(f3) RAW(0f) RAW(a7) RAW(e0)
        RAW(f3) RAW(0f) RAW(a7) RAW(e0)
        RAW(f3) RAW(0f) RAW(a7) RAW(e8)
        RAW(f3) RAW(0f) RAW(a7) RAW(e8)
        RAW(0f) RAW(a7) RAW(c0)
        RAW(f3) RAW(0f) RAW(a7) RAW(c0)
        RAW(f3) RAW(0f) RAW(a6) RAW(c0)
        RAW(f3) RAW(0f) RAW(a6) RAW(c0)
        RAW(f3) RAW(0f) RAW(a6) RAW(c8)
        RAW(f3) RAW(0f) RAW(a6) RAW(c8)
        RAW(f3) RAW(0f) RAW(a6) RAW(d0)
        RAW(f3) RAW(0f) RAW(a6) RAW(d0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
