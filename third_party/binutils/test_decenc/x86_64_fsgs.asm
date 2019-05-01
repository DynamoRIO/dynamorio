
#undef FUNCNAME
#define FUNCNAME test_x86_64_fsgs_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f3) RAW(0f) RAW(ae) RAW(c3)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(c3)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(c0)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(c0)
        RAW(f3) RAW(0f) RAW(ae) RAW(cb)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(cb)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(c8)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(c8)
        RAW(f3) RAW(0f) RAW(ae) RAW(d3)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(d3)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(d0)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(d0)
        RAW(f3) RAW(0f) RAW(ae) RAW(db)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(db)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(d8)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(d8)
        RAW(f3) RAW(0f) RAW(ae) RAW(c3)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(c3)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(c0)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(c0)
        RAW(f3) RAW(0f) RAW(ae) RAW(cb)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(cb)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(c8)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(c8)
        RAW(f3) RAW(0f) RAW(ae) RAW(d3)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(d3)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(d0)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(d0)
        RAW(f3) RAW(0f) RAW(ae) RAW(db)
        RAW(f3) RAW(48) RAW(0f) RAW(ae) RAW(db)
        RAW(f3) RAW(41) RAW(0f) RAW(ae) RAW(d8)
        RAW(f3) RAW(49) RAW(0f) RAW(ae) RAW(d8)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
