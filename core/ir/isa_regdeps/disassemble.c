#include "../globals.h"

#include "disassemble.h"

/* DR_ISA_REGDEPS instruction encodings can be at most 16 bytes and are 4 byte aligned,
 * hence we can have at most 4 lines.
 */
#define REGDEPS_BYTES_PER_LINE 4

int
print_regdeps_encoding_bytes_to_buffer(char *buf, size_t bufsz,
                                       size_t *sofar DR_PARAM_INOUT, byte *pc,
                                       byte *next_pc)
{
    int sz = (int)(next_pc - pc);
    int i;
    int extra_sz = 0;
    if (sz > REGDEPS_BYTES_PER_LINE) {
        extra_sz = sz - REGDEPS_BYTES_PER_LINE;
        sz = REGDEPS_BYTES_PER_LINE;
    }
    print_to_buffer(buf, bufsz, sofar, " ");
    for (i = 0; i < sz; i += 4)
        print_to_buffer(buf, bufsz, sofar, "%08x", *((uint *)(pc + i)));
    for (i = sz; i < REGDEPS_BYTES_PER_LINE; ++i)
        print_to_buffer(buf, bufsz, sofar, "  ");
    print_to_buffer(buf, bufsz, sofar, " ");
    return extra_sz;
}

void
print_extra_regdeps_encoding_bytes_to_buffer(char *buf, size_t bufsz,
                                             size_t *sofar DR_PARAM_INOUT, byte *pc,
                                             byte *next_pc, int extra_sz,
                                             const char *extra_bytes_prefix)
{
    /* + 1 accounts for line 0 printed by print_regdeps_encoding_bytes_to_buffer().
     */
    int num_lines =
        (ALIGN_FORWARD(extra_sz, REGDEPS_BYTES_PER_LINE) / REGDEPS_BYTES_PER_LINE) + 1;
    /* We start from line 1 because line 0 has already been printed by
     * print_regdeps_encoding_bytes_to_buffer().
     */
    for (int line = 1; line < num_lines; ++line) {
        int sz = extra_sz;
        if (sz > REGDEPS_BYTES_PER_LINE)
            sz = REGDEPS_BYTES_PER_LINE;
        print_to_buffer(buf, bufsz, sofar, "%s ", extra_bytes_prefix);
        for (int i = 0; i < sz; i += 4) {
            print_to_buffer(buf, bufsz, sofar, "%08x",
                            *((uint *)(pc + line * REGDEPS_BYTES_PER_LINE + i)));
        }
        for (int i = sz; i < REGDEPS_BYTES_PER_LINE; ++i)
            print_to_buffer(buf, bufsz, sofar, "  ");
        print_to_buffer(buf, bufsz, sofar, "\n");
        extra_sz -= REGDEPS_BYTES_PER_LINE;
    }
}
