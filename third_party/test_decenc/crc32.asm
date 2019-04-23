
#undef FUNCNAME
#define FUNCNAME test_crc32_s_asm
DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* There is intentionally no prologue and epilogue. This function is not
         * supposed to be actually called. We are using the function like a label
         * for the decoder.
         */
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(06)
        /* FIXME i#3578: crc32w (%esi),%eax. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(06) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(06)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(c0)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(c0)
        /* FIXME i#3578: crc32w (%esi),%eax. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0) */
        /* FIXME i#3578: crc32w (%esi),%eax. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0)
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(06)
        /* FIXME i#3578: crc32w (%esi),%eax. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(06) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(06)
        RAW(f2) RAW(0f) RAW(38) RAW(f0) RAW(c0)
        /* FIXME i#3578: crc32w (%esi),%eax. */
        /* RAW(66) RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0) */
        RAW(f2) RAW(0f) RAW(38) RAW(f1) RAW(c0)
        END_OF_FUNCTION_MARKER
END_FUNC(FUNCNAME)
