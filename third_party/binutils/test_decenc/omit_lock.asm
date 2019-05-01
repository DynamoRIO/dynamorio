
#undef FUNCNAME
#define FUNCNAME test_omit_lock_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f0) RAW(f0) RAW(83) RAW(00) RAW(01)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
