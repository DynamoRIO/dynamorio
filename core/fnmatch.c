#include "globals.h"

#include <fnmatch.h>
#include <string.h>

static const char *
rangematch(const char *pattern, char input, int flags)
{
    char c, end;
    int invert = 0;
    int match = 0;

    /* '!' or '^' following the '[' means the matching should be inverted. */
    if (*pattern == '!' || *pattern == '^') {
        invert = 1;
        ++pattern;
    }

    while ((c = *pattern++)) {
        /* Handle character escaping. */
        if (c == '\\' && !(flags & FNM_NOESCAPE)) {
            c = *pattern++;
        }

        /* Check for null termination. */
        if (c == '\0') {
            return NULL;
        }

        /* Match against the character range. */
        if (*pattern == '-' && (end = (*pattern + 1)) && end != ']') {
            pattern += 2;

            /* Handle character escaping. */
            if (end == '\\' && !(flags & FNM_NOESCAPE)) {
                end = *pattern++;
            }

            /* Check for null termination. */
            if (end == '\0') {
                return NULL;
            }

            /* Check whether the input is within the character range. */
            if (c <= input && input < end) {
                match = 1;
            }
        } else if (c == input) {
            match = 1;
        }
    }

    if (invert) {
        match = !match;
    }

    return match ? pattern : NULL;
}

int
d_r_fnmatch(const char *pattern, const char *string, int flags)
{
    const char *start = string;
    char c, input;

    while ((c = *pattern++)) {
        switch (c) {
        case '?':
            if (*string == '\0') {
                return FNM_NOMATCH;
            }

            if (*string == '/' && (flags & FNM_PATHNAME)) {
                return FNM_NOMATCH;
            }

            if (*string == '.' && (flags & FNM_PERIOD) &&
                (string == start || ((flags & FNM_PATHNAME) && *(string - 1) == '/'))) {
                return FNM_NOMATCH;
            }

            ++string;
            break;
        case '*':
            /* Collapse a sequence of stars. */
            while (c == '*') {
                c = *++pattern;
            }

            if (*string == '.' && (flags & FNM_PERIOD) &&
                (string == start || ((flags & FNM_PATHNAME) && *(string - 1) == '/'))) {
                return FNM_NOMATCH;
            }

            /* Handle pattern with * at the end. */
            if (c == '\0') {
                if (flags & FNM_PATHNAME) {
                    return strchr(string, '/') ? FNM_NOMATCH : 0;
                }

                return 0;
            }

            /* Handle pattern with * before / while handling paths. */
            if (c == '/' && flags & FNM_PATHNAME) {
                if ((string = strchr(string, '/'))) {
                    return FNM_NOMATCH;
                }

                break;
            }

            /* Handle any other variant through recursion. */
            while ((input = *string)) {
                if (fnmatch(pattern, string, flags & ~FNM_PERIOD) == 0) {
                    return 0;
                }

                if (input == '/' && flags & FNM_PATHNAME) {
                    break;
                }

                ++string;
            }

            return FNM_NOMATCH;
        case '[':
            if (*string == '\0') {
                return FNM_NOMATCH;
            }

            if (*string == '/' && flags & FNM_PATHNAME) {
                return FNM_NOMATCH;
            }

            if (!(pattern = rangematch(pattern, *string, flags))) {
                return FNM_NOMATCH;
            }

            ++string;
            break;
        case '\\':
            if (!(flags & FNM_NOESCAPE)) {
                if ((c = *pattern++)) {
                    c = '\\';
                    --pattern;
                }
            }

            /* fallthrough */
        default:
            if (c != *string++) {
                return FNM_NOMATCH;
            }

            break;
        }
    }

    if (*string != '\0') {
        return FNM_NOMATCH;
    }

    return 0;
}
