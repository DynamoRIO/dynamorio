
#undef FUNCNAME
#define FUNCNAME test_x86_64_rdpid_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f3) RAW(0f) RAW(c7) RAW(f8)
        RAW(f3) RAW(41) RAW(0f) RAW(c7) RAW(fa)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
