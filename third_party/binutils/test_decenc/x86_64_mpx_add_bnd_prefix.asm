
#undef FUNCNAME
#define FUNCNAME test_x86_64_mpx_add_bnd_prefix_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(e8) RAW(09) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(10)
        RAW(74) RAW(05)
        RAW(eb) RAW(03)
        RAW(ff) RAW(23)
        RAW(c3)
        RAW(f3) RAW(c3)
        RAW(f3) RAW(c3)
        RAW(f2) RAW(c3)
        RAW(f2) RAW(c3)
        RAW(f2) RAW(e8) RAW(f2) RAW(ff) RAW(ff) RAW(ff)
        RAW(48) RAW(01) RAW(c3)
        RAW(e2) RAW(ed)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
