
#undef FUNCNAME
#define FUNCNAME test_x86_64_segment_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(8c) RAW(18)
        RAW(8c) RAW(18)
        RAW(8e) RAW(18)
        RAW(8e) RAW(18)
        RAW(48) RAW(8c) RAW(d8)
        RAW(48) RAW(8e) RAW(d8)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
