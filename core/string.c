/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * string.c - Private string routine implementations.  We need these to achieve
 * libc independence (i#46).
 *
 * These are generally unoptimized because they aren't on DR's critical path.
 * Clients use a privately loaded libc.  If one of these shows up in a profile,
 * we should probably avoid calling it rather than trying to optimize these
 * routines.
 */

#include "globals.h"

#include <limits.h>

/* We used to include <string.h> here to make sure our prototypes match, but
 * string.h often uses macros, intrinsics, and always_inline annotations to
 * define or declare these routines.  We simplify our lives by avoiding
 * string.h.
 *
 * Other files should not include string.h either.  On UNIX, we no longer use
 * the standard string function names, so the linker will not resolve a
 * compiler-inlined symbol to ours.
 */
#ifdef _STRING_H
#    error "Don't include <string.h>"
#endif

/* Private strlen. */
size_t
d_r_strlen(const char *str)
{
    const char *cur = str;
    while (*cur != '\0') {
        cur++;
    }
    return cur - str;
}

/* Private wcslen. */
size_t
d_r_wcslen(const wchar_t *str)
{
    const wchar_t *cur = str;
    while (*cur != L'\0') {
        cur++;
    }
    return cur - str;
}

/* Private strchr.  Returns pointer to first instance of c in str or NULL if c
 * is not present.  If c is '\0', match the terminating NULL instead of
 * returning NULL.
 */
char *
d_r_strchr(const char *str, int c)
{
    while (true) {
        if (*str == c)
            return (char *)str;
        if (*str == '\0')
            return NULL;
        str++;
    }
    return NULL; /* Keep the compiler happy. */
}

/* Private strrchr.  Returns pointer to last instance of c in str or NULL if c
 * is not present.  If c is '\0', match the terminating NULL instead of
 * returning NULL.
 */
char *
d_r_strrchr(const char *str, int c)
{
    const char *ret = NULL;
    while (true) {
        if (*str == c)
            ret = str;
        if (*str == '\0')
            break;
        str++;
    }
    return (char *)ret;
}

/* Private strncpy.  Standard caveat about not copying trailing null byte on
 * truncation applies.
 */
#define D_R_STRNCPY_BODY()                        \
    do {                                          \
        size_t i;                                 \
        for (i = 0; i < n && src[i] != '\0'; i++) \
            dst[i] = src[i];                      \
        /* Pad the rest with nulls. */            \
        for (; i < n; i++)                        \
            dst[i] = '\0';                        \
        return dst;                               \
    } while (0)
char *
d_r_strncpy(char *dst, const char *src, size_t n)
{
    D_R_STRNCPY_BODY();
}

/* Private strncat. */
#define D_R_STRNCAT_BODY()                        \
    do {                                          \
        size_t dest_len = strlen(dest);           \
        size_t i;                                 \
        for (i = 0; i < n && src[i] != '\0'; i++) \
            dest[dest_len + i] = src[i];          \
        dest[dest_len + i] = '\0';                \
        return dest;                              \
    } while (0)
char *
d_r_strncat(char *dest, const char *src, size_t n)
{
    D_R_STRNCAT_BODY();
}

/* Private memcpy is in arch/<arch>/<arch>.asm or memfuncs.asm */

/* Private memset is in arch/<arch>/<arch>.asm or memfuncs.asm */

/* Private memmove.  The only difference between memcpy and memmove is that if
 * you need to shift overlapping data forwards in memory, memmove will do what
 * you want.
 * We also have a version named "memmove" in lib/memmove.c for shared
 * DR libc isolation.
 */

#define D_R_MEMMOVE_BODY()                              \
    do {                                                \
        ssize_t i;                                      \
        byte *dst_b = (byte *)dst;                      \
        const byte *src_b = (const byte *)src;          \
        if (dst < src)                                  \
            return memcpy(dst, src, n);                 \
        /* FIXME: Could use reverse DF and rep movs. */ \
        for (i = n - 1; i >= 0; i--) {                  \
            dst_b[i] = src_b[i];                        \
        }                                               \
        return dst;                                     \
    } while (0)
void *
d_r_memmove(void *dst, const void *src, size_t n)
{
    D_R_MEMMOVE_BODY();
}

#ifdef UNIX
/* gcc emits calls to these *_chk variants in release builds when the size of
 * dst is known at compile time.  In C, the caller is responsible for cleaning
 * up arguments on the stack. We used to alias these *_chk routines to the non-chk
 * routines. Current gcc versions don't accept aliases with a different function
 * signature. Instead, we are now manually inlining the function body. We rely
 * on the caller to clean up the extra dst_len arg.
 */
void *
__memmove_chk(void *dst, const void *src, size_t n, size_t dst_len)
{
    D_R_MEMMOVE_BODY();
}
void *
__strncpy_chk(char *dst, const char *src, size_t n, size_t dst_len)
{
    D_R_STRNCPY_BODY();
}
void *
__strncat_chk(char *dest, const char *src, size_t n, size_t dst_len)
{
    D_R_STRNCAT_BODY();
}
#endif

/* Private strcmp. */
int
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

/* Private strncmp. */
int
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

/* Private memcmp. */
int
d_r_memcmp(const void *left_v, const void *right_v, size_t n)
{
    /* Use unsigned comparisons. */
    const byte *left = left_v;
    const byte *right = right_v;
    size_t i;
    for (i = 0; i < n; i++) {
        if (left[i] < right[i])
            return -1;
        if (left[i] > right[i])
            return 1;
    }
    return 0;
}

/* Private strstr. */
char *
d_r_strstr(const char *haystack, const char *needle)
{
    const char *cur = haystack;
    size_t needle_len = strlen(needle);
    while (*cur != '\0') {
        if (strncmp(cur, needle, needle_len) == 0) {
            return (char *)cur;
        }
        cur++;
    }
    return NULL;
}

/* Private tolower. */
int
d_r_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return (c - ('A' - 'a'));
    return c;
}

/* Private strcasecmp. */
int
d_r_strcasecmp(const char *left, const char *right)
{
    size_t i;
    for (i = 0; left[i] != '\0' || right[i] != '\0'; i++) {
        int l = tolower(left[i]);
        int r = tolower(right[i]);
        if (l < r)
            return -1;
        if (l > r)
            return 1;
    }
    return 0;
}

/* Private strtoul.  Actual parsing is implemented in io.c.  We use plain
 * "unsigned long" to match libc prototype regardless of our internal typedefs.
 *
 * libc strtoul will set errno to ERANGE on failure.  Our internal callers don't
 * check for failure, so we don't bother.  If they need to handle failure, they
 * can call parse_int directly.
 */
unsigned long
d_r_strtoul(const char *str, char **end, int base)
{
    uint64 num;
    const char *parse_end = d_r_parse_int(str, &num, base, 0 /*width*/, true /*signed*/);
    if (end != NULL)
        *end = (char *)parse_end;
    if (parse_end == NULL)
        return ULONG_MAX;
    return (unsigned long)num; /* truncate */
}

#ifdef STANDALONE_UNIT_TEST
/* Even in a debug build, gcc does crazy constant folding and removes our call
 * to strrchr, breaking the test.
 */
static const char *
identity(const char *str)
{
    return str;
}

void
unit_test_string(void)
{
    static const char test_path[] = "/path/to/file";
    const char *ret;
    char buf[MAXIMUM_PATH];
    unsigned long num;

    print_file(STDERR, "testing string\n");

    /* strchr */
    ret = strchr(identity(test_path), '/');
    EXPECT(ret == test_path, true);
    ret = strchr(identity(test_path), '\0');
    EXPECT(ret != NULL, true);
    EXPECT(*ret, '\0');

    /* strrchr */
    ret = strrchr(identity(test_path), '/');
    EXPECT(strcmp(ret, "/file"), 0);
    ret = strrchr(identity(test_path), '\0');
    EXPECT(ret != NULL, true);
    EXPECT(*ret, '\0');

    /* strncpy, strncat */
    strncpy(buf, test_path, sizeof(buf));
    EXPECT(is_region_memset_to_char((byte *)buf + strlen(test_path),
                                    sizeof(buf) - strlen(test_path), '\0'),
           true);
    strncat(buf, "/foo_wont_copy", 4);
    EXPECT(strcmp(buf, "/path/to/file/foo"), 0);

    /* strtoul */
    num = strtoul(identity("-10"), NULL, 0);
    EXPECT((long)num, -10); /* negative */
    num = strtoul(identity("0777"), NULL, 0);
    EXPECT(num, 0777); /* octal */
    num = strtoul(identity("0xdeadBEEF"), NULL, 0);
    EXPECT(num, 0xdeadbeef); /* hex */
    num = strtoul(identity("deadBEEF next"), (char **)&ret, 16);
    EXPECT(num, 0xdeadbeef);         /* non-0x prefixed hex */
    EXPECT(strcmp(ret, " next"), 0); /* end */
    num = strtoul(identity("1001a"), NULL, 2);
    EXPECT(num, 9); /* binary */
    num = strtoul(identity("1aZ"), NULL, 36);
    EXPECT(num, 1 * 36 * 36 + 10 * 36 + 35); /* weird base */
    num = strtoul(identity("1aZ"), (char **)&ret, 37);
    EXPECT(num, ULONG_MAX); /* invalid base */
    EXPECT(ret == NULL, true);

    /* memmove */
    strncpy(buf, test_path, sizeof(buf));
    memmove(buf + 4, buf, strlen(buf) + 1);
    memcpy(buf, "/foo", 4);
    EXPECT(strcmp(buf, "/foo/path/to/file"), 0);

    print_file(STDERR, "done testing string\n");
}

#endif /* STANDALONE_UNIT_TEST */
