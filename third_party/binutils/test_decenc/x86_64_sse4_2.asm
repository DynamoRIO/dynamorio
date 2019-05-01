
#undef FUNCNAME
#define FUNCNAME test_x86_64_sse4_2_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        /* FIXME i#3578: crc32w %cx,%ebx. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(19)
        /* FIXME i#3578: crc32w (%rcx),%ebx. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(19) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        /* FIXME i#3578: crc32w %cx,%ebx. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(66) RAW(0f) RAW(38) RAW(37) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(37) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(61) RAW(01) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(61) RAW(c1) RAW(00)
        RAW(66) RAW(48) RAW(0f) RAW(3a) RAW(61) RAW(01) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(61) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(60) RAW(01) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(60) RAW(c1) RAW(01)
        RAW(66) RAW(48) RAW(0f) RAW(3a) RAW(60) RAW(01) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(60) RAW(c1) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(63) RAW(01) RAW(02)
        RAW(66) RAW(0f) RAW(3a) RAW(63) RAW(c1) RAW(02)
        RAW(66) RAW(0f) RAW(3a) RAW(62) RAW(01) RAW(03)
        RAW(66) RAW(0f) RAW(3a) RAW(62) RAW(c1) RAW(03)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(d9)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        /* FIXME i#3578: crc32w %cx,%ebx. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(19)
        RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(19)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f0) RAW(d9)
        /* FIXME i#3578: crc32w %cx,%ebx. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(f2) RAW(48) RAW(0f) RAW(38) RAW(f1) RAW(d9)
        RAW(66) RAW(0f) RAW(38) RAW(37) RAW(01)
        RAW(66) RAW(0f) RAW(38) RAW(37) RAW(c1)
        RAW(66) RAW(0f) RAW(3a) RAW(61) RAW(01) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(61) RAW(c1) RAW(00)
        RAW(66) RAW(0f) RAW(3a) RAW(60) RAW(01) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(60) RAW(c1) RAW(01)
        RAW(66) RAW(0f) RAW(3a) RAW(63) RAW(01) RAW(02)
        RAW(66) RAW(0f) RAW(3a) RAW(63) RAW(c1) RAW(02)
        RAW(66) RAW(0f) RAW(3a) RAW(62) RAW(01) RAW(03)
        RAW(66) RAW(0f) RAW(3a) RAW(62) RAW(c1) RAW(03)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(0f) RAW(b8) RAW(19)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(19)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(d9)
        RAW(66) RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(0f) RAW(b8) RAW(d9)
        RAW(f3) RAW(48) RAW(0f) RAW(b8) RAW(d9)
        RAW(00) RAW(00)
        RAW(00) RAW(00)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
