
#undef FUNCNAME
#define FUNCNAME test_fence_as_lock_add_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(0f) RAW(ae) RAW(e8)
        RAW(0f) RAW(ae) RAW(f0)
        RAW(0f) RAW(ae) RAW(f8)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
