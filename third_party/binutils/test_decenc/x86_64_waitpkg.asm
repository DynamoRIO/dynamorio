
#undef FUNCNAME
#define FUNCNAME test_x86_64_waitpkg_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(f3) RAW(0f) RAW(ae) RAW(f0)
        RAW(f3) RAW(41) RAW(0f) RAW(ae)
        RAW(f2) RAW(67) RAW(f3) RAW(41) RAW(0f) RAW(ae)
        RAW(f2) RAW(f2) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(f2) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(f2) RAW(41) RAW(0f) RAW(ae)
        RAW(f2) RAW(f2) RAW(41) RAW(0f) RAW(ae)
        RAW(f2) RAW(66) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(66) RAW(0f) RAW(ae)
        RAW(f1)
        RAW(66) RAW(41) RAW(0f) RAW(ae)
        RAW(f2) RAW(66) RAW(41) RAW(0f) RAW(ae)
        RAW(f2)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
