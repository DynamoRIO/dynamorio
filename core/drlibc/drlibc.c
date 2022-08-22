/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Default implementation to avoid the user of drlibc having to supply one.
 * We declare this as weak for Linux and MacOS, and rely on MSVC prioritizing a
 * .obj def over this .lib def (note that the MSVC linker only does that
 * when the symbol in question is *by itself* in a single .obj: hence this file,
 * rather than putting this into say asm_shared.asm).
 */

/* Avoid pulling in deps from instr_inline.h included from globals.h */
#define DR_NO_FAST_IR

#include "../globals.h" /* just to disable warning C4206 about an empty file */

#ifdef MACOS
#    include <sys/utsname.h> /* for struct utsname */
#endif

/***************************************************************************
 * Compatibility layer for sharing a single compiled-once library
 * between core and non-core.
 * Some are separated out into drlibc_notdr_* files since MSVC behaves
 * better when duplicate symbols are each in their own .obj.
 */

WEAK bool
safe_read_if_fast(const void *base, size_t size, void *out_buf)
{
    return d_r_safe_read(base, size, out_buf);
}

WEAK int
d_r_strcmp(const char *left, const char *right)
{
    size_t i;
    for (i = 0; left[i] != '\0' || right[i] != '\0'; i++) {
        if (left[i] < right[i])
            return -1;
        if (left[i] > right[i])
            return 1;
    }
    return 0;
}

WEAK file_t main_logfile;
WEAK options_t dynamo_options;

#ifdef MACOS
WEAK int
d_r_strncmp(const char *left, const char *right, size_t n)
{
    size_t i;
    for (i = 0; i < n && (left[i] != '\0' || right[i] != '\0'); i++) {
        if (left[i] < right[i])
            return -1;
        if (left[i] > right[i])
            return 1;
    }
    return 0;
}

WEAK bool
kernel_is_64bit(void)
{
    struct utsname uinfo;
    if (uname(&uinfo) != 0)
        return true; /* guess */
    return (strncmp(uinfo.machine, "x86_64", sizeof("x86_64")) == 0);
}
#endif

/***************************************************************************/

#ifdef AARCH64

#    ifdef MACOS
/* This is from libc. */
extern void
sys_icache_invalidate(void *, size_t);
#    endif

void
clear_icache(void *beg, void *end)
{
#    ifdef DR_HOST_NOT_TARGET
    ASSERT_NOT_REACHED();
#    else
    size_t dcache_line_size;
    size_t icache_line_size;
    ptr_uint_t beg_uint = (ptr_uint_t)beg;
    ptr_uint_t end_uint = (ptr_uint_t)end;
    ptr_uint_t addr;

    if (beg_uint >= end_uint)
        return;

    if (!get_cache_line_size(&dcache_line_size, &icache_line_size)) {
        /* We don't expect get_cache_line_size to return false, as this code is
         * invoked only when (not DR_HOST_NOT_TARGET).
         */
        ASSERT_NOT_REACHED();
    }

    /* Flush data cache to point of unification, one line at a time. */
    addr = ALIGN_BACKWARD(beg_uint, dcache_line_size);
    do {
        __asm__ __volatile__("dc cvau, %0" : : "r"(addr) : "memory");
        addr += dcache_line_size;
    } while (addr != ALIGN_FORWARD(end_uint, dcache_line_size));

    /* Data Synchronization Barrier */
    __asm__ __volatile__("dsb ish" : : : "memory");

    /* Invalidate instruction cache to point of unification, one line at a time. */
    addr = ALIGN_BACKWARD(beg_uint, icache_line_size);
    do {
        __asm__ __volatile__("ic ivau, %0" : : "r"(addr) : "memory");
        addr += icache_line_size;
    } while (addr != ALIGN_FORWARD(end_uint, icache_line_size));

    /* Data Synchronization Barrier */
    __asm__ __volatile__("dsb ish" : : : "memory");

    /* Instruction Synchronization Barrier */
    __asm__ __volatile__("isb" : : : "memory");

    /* XXX i#5383: Do we need this in addition?  This is from PR #5497. */
    IF_MACOS64(sys_icache_invalidate(beg, end_uint - beg_uint));

#    endif
}

/* Obtains dcache and icache line size and sets the values at the given pointers.
 * Returns false if no value was written.
 * This is required to be called at init time when linked into DR. This is to
 * avoid races and write issues with the static variable used.
 *
 * XXX i#1684: Design better support for builds where host!=target, e.g.
 * a64-on-x86 for which this function does not set any cache line size; also
 * x86-on-a64 for which we currently attempt to use cpuid (which is not available
 * on a64) to set cache line size in core/arch/x86/proc.c.
 * For these builds, it may be better to set some properties like cache_line_size
 * to the host's value, but not for all e.g. num_simd_registers.
 */
bool
get_cache_line_size(OUT size_t *dcache_line_size, OUT size_t *icache_line_size)
{
#    ifndef DR_HOST_NOT_TARGET
    static size_t cache_info = 0;

    /* "Cache Type Register" contains:
     * CTR_EL0 [31]    : 1
     * CTR_EL0 [19:16] : Log2 of number of 4-byte words in smallest dcache line
     * CTR_EL0 [3:0]   : Log2 of number of 4-byte words in smallest icache line
     * https://developer.arm.com/documentation/ddi0595/2021-09/AArch64-Registers/
     * CTR-EL0--Cache-Type-Register
     *
     * Also, the whitepaper below documents AArch64 words being 32 bits wide.
     * https://developer.arm.com/-/media/Files/pdf/
     * graphics-and-multimedia/Porting%20to%20ARM%2064-bit.pdf
     */
    if (cache_info == 0) {
#        if defined(MACOS)
        /* FIXME i#5383: Put in a proper solution; maybe getauxval() syscall with
         * AT_HWCAP/AT_HWCAP2?
         * mrs traps to illegal instruction on M1;
         * hackily hardwire to "sysctl -a hw machdep.cpu" from one machine to
         * make forward progress for now.
         */
        cache_info = (1 << 31) | (7 << 16) | (7 << 0);
#        else
        __asm__ __volatile__("mrs %0, ctr_el0" : "=r"(cache_info));
#        endif
    }
    if (dcache_line_size != NULL)
        *dcache_line_size = 4 << (cache_info >> 16 & 0xf);
    if (icache_line_size != NULL)
        *icache_line_size = 4 << (cache_info & 0xf);
    return true;
#    endif
    return false;
}
#endif

#ifdef UNIX
/* Parse the first line of a "#!" script. If the input is recognised, the string
 * pointed to by str is overwritten with null terminators, as necessary, *interp
 * is set to point at the script interpreter, and *arg to point at the optional
 * argument, if there is one, or NULL. The accepted syntax is "#!", followed by
 * optional spaces (' ' or '\t'), followed by the file path (any characters except
 * spaces, '\n' and '\0'), optionally followed by the argument, followed by '\n'
 * or '\0'. The argument may contain any character except '\n' and '\0', including
 * spaces, but leading and trailing spaces are removed.
 */
static bool
is_shebang(INOUT char *str, OUT char **interp, OUT char **arg)
{
    char *p, *arg_end;

    if (str[0] != '#' || str[1] != '!')
        return false;
    p = str + 2;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (*p == '\n' || *p == '\0')
        return false;
    /* We have an interpreter. */
    *interp = p++;
    while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
        ++p;
    if (*p == '\n' || *p == '\0') {
        *p = '\0';
        *arg = NULL;
        return true;
    }
    *p++ = '\0';
    while (*p == ' ' || *p == '\t')
        ++p;
    if (*p == '\n' || *p == '\0') {
        *arg = NULL;
        return true;
    }
    /* We have an argument. */
    *arg = p++;
    arg_end = p;
    while (*p != '\n' && *p != '\0') {
        if (*p != ' ' && *p != '\t')
            arg_end = p + 1;
        ++p;
    }
    *arg_end = '\0';
    return true;
}

bool
find_script_interpreter(OUT script_interpreter_t *result, IN const char *fname,
                        ssize_t (*reader)(const char *pathname, void *buf, size_t count))
{
    const int max_line_len = SCRIPT_LINE_MAX;
    const int max_recursion = SCRIPT_RECURSION_MAX;
    char **argv = result->argv;
    char *interp, *arg;
    const char *file;
    ssize_t len;
    int i, argc;

    file = fname;
    for (i = 0; i < max_recursion; i++) {
        len = reader(file, result->buffer[i], max_line_len);
        if (len < 0)
            break;
        result->buffer[0][len] = 0;
        if (!is_shebang(result->buffer[i], &interp, &arg))
            break;

        /* Add strings to argv: arg first as we will reverse later. */
        if (arg != NULL)
            *argv++ = arg;
        *argv++ = interp;

        file = interp;
    }

    if (i == 0)
        return false;

    if (i == max_recursion) {
        /* Check that the final script interpreter is not itself a script. */
        char tmp[sizeof(result->buffer)];
        len = reader(*(argv - 1), tmp, max_line_len);
        if (len >= 0) {
            tmp[len] = 0;
            if (is_shebang(tmp, &interp, &arg)) {
                result->argc = 0;
                result->argv[0] = NULL;
                return true;
            }
        }
    }

    argc = argv - result->argv;
    result->argc = argc;
    /* Reverse order of arguments and null-terminate. */
    for (i = 0; i < argc / 2; i++) {
        char *tmp = result->argv[i];
        result->argv[i] = result->argv[argc - 1 - i];
        result->argv[argc - 1 - i] = tmp;
    }
    result->argv[argc] = NULL;
    return true;
}
#endif

#ifdef WINDOWS
/***************************************************************************
 * Windows mode switching support.
 */

/* We set a default equal to the observed 0x2b value on every Windows version.
 * The core calls d_r_set_ss_selector() to update to the underlying value.
 */
int d_r_ss_value = 0x2b;
#endif
