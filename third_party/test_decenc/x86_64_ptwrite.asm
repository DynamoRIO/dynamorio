
#undef FUNCNAME
#define FUNCNAME test_x86_64_ptwrite_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f3) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(0f) RAW(ae) RAW(21)
        RAW(f3) RAW(0f) RAW(ae) RAW(21)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(21)
        RAW(f3) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(e1)
        RAW(f3) RAW(0f) RAW(ae) RAW(21)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(21)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
