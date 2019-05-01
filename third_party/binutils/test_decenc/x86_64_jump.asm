
#undef FUNCNAME
#define FUNCNAME test_x86_64_jump_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        END_PROLOG
        RAW(eb) RAW(fe)
        RAW(e9) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(24) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(e7)
        RAW(ff) RAW(27)
        RAW(ff) RAW(2c) RAW(bd) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(ff) RAW(2c) RAW(bd) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(ff) RAW(2c) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(ff) RAW(2c) RAW(25) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(e8) RAW(cb) RAW(ff) RAW(ff) RAW(ff)
        RAW(e8) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(14) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(ff) RAW(d7)
        RAW(ff) RAW(17)
        RAW(ff) RAW(1c) RAW(bd) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(ff) RAW(1c) RAW(bd) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(ff) RAW(1c) RAW(25) RAW(00) RAW(00) RAW(00) RAW(00)
        RAW(66) RAW(ff) RAW(1c) RAW(25) RAW(00) RAW(00) RAW(00)
        RAW(00)
        RAW(67) RAW(e3) RAW(00)
        RAW(90)
        RAW(e3) RAW(00)
        RAW(90)
        RAW(66) RAW(ff) RAW(13)
        RAW(ff) RAW(1b)
        RAW(66) RAW(ff) RAW(23)
        RAW(ff) RAW(2b)
        RAW(eb) RAW(00)
        RAW(90)
        RAW(67) RAW(e3) RAW(00)
        RAW(90)
        RAW(e3) RAW(00)
        RAW(90)
        RAW(eb) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
