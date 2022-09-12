/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * options.c
 *
 * Options definition and handling of corresponding command line options
 *
 */

#include <stddef.h>

#ifndef NOT_DYNAMORIO_CORE
#    include "globals.h"
#    include "fcache.h"
#    include "monitor.h"
#    include "moduledb.h" /* for the process control defines */
#    include "disassemble.h"

#else /* NOT_DYNAMORIO_CORE */
#    include "configure.h"
#    include <stdio.h> /* snprintf, sscanf */

#    ifdef WINDOWS
#        define inline __inline
#        define snprintf _snprintf
#        define WIN32_LEAN_AND_MEAN
#        include <windows.h> /* no longer included in globals_shared.h */
#    endif

#    include "globals_shared.h"

typedef unsigned int uint;

#    define print_file(f, s, x)

#    ifndef bool
typedef char bool;
#    endif
#    ifndef true
#        define true(1)
#        define false(0)
#    endif
#    ifndef NULL
#        define NULL ((void *)0)
#    endif

struct stats_type {
    int logmask;
    int loglevel;
} thestats;

struct stats_type *d_r_stats = &thestats;

/* define away core only features, depending on use outside the core
 * may want to use some of these, NOTE build environments outside of the
 * core can't handle VARARG macros! */
#    define ASSERT(x)
/* right now all OPTION_PARSE_ERROR's have 6 arguments
 * once libutil, etc. are all using vc8 we can use ...
 */
#    define OPTION_PARSE_ERROR(a, b, c, d, e, f)

static void
ignore_varargs_function(char *format, ...)
{
}
/* right now all FATAL_USAGE_ERROR's have 4 arguments */
#    define FATAL_USAGE_ERROR(a, b, c, d)
/* USAGE_ERROR unfortunately needs a quick fix now */
#    define USAGE_ERROR 1 ? (void)0 : ignore_varargs_function
#    define DOLOG(a, b, c)

#    include "options.h"

#endif /* NOT_DYNAMORIO_CORE */

typedef enum option_type_t {
    OPTION_TYPE_bool,
    OPTION_TYPE_uint,
    OPTION_TYPE_uint_addr,
    OPTION_TYPE_uint_size,
    OPTION_TYPE_uint_time,
    OPTION_TYPE_pathstring_t,
    OPTION_TYPE_liststring_t,
} option_type_t;

typedef enum option_modifier_t {
    OPTION_MOD_STATIC,
    OPTION_MOD_DYNAMIC
} option_modifier_t;

typedef ptr_uint_t uint_size;
typedef uint uint_time;
typedef ptr_uint_t uint_addr;

/* Structure with all the information about an option. */
typedef struct _option_t {
    const char *name;
    uint offset;
    uint size;
    option_type_t type;
    op_pcache_t affects_pcache;
    option_modifier_t modifier;
} option_t;

#ifndef EXPOSE_INTERNAL_OPTIONS
/* default values for internal options are kept in a separate struct */
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache)           \
        default_value,
#    define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                           description, flag, pcache) /* nothing */
/* read only source for default internal option values and names
 * no lock needed since never written
 */
const internal_options_t default_internal_options = {
#    include "optionsx.h"
};
#    undef OPTION_COMMAND_INTERNAL
#    undef OPTION_COMMAND
#endif

/* Definitions for our default values.  See options.h for why we can't
 * have them in an enum in the header.
 */
#undef OPTION_STRING
#undef EMPTY_STRING
/* We don't have strings in the default values. */
#define OPTION_STRING(x) 0
#define EMPTY_STRING 0
#define liststring_t int
#define pathstring_t int
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    const type OPTION_DEFAULT_VALUE_##name = default_value;
#define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                statement, description, flag, pcache)           \
    const type OPTION_DEFAULT_VALUE_##name = default_value;
#include "optionsx.h"
#undef liststring_t
#undef pathstring_t
#undef OPTION_COMMAND
#undef OPTION_COMMAND_INTERNAL
#undef OPTION_STRING
#undef EMPTY_STRING
/* Restore. */
#define OPTION_STRING(x) x
#define EMPTY_STRING \
    {                \
        0            \
    }

#ifdef EXPOSE_INTERNAL_OPTIONS
#    define OPTION_COMMAND_INTERNAL OPTION_COMMAND
#else
/* DON'T FIXME: In order to support easy switching of an internal
   option into user accessible one we could waste some memory by
   allocating fields in options_t even for INTERNAL options.  That would
   let us have INTERNAL_OPTION be a more flexible macro that can use
   either the constant value, or the dynamically set variable
   depending on the option declaration in optionsx.h.  However,
   it increases the code size of this file (there is also a minor increase
   in other object files since more option fields need longer than 8-bit offsets)
   For now we can live without this.
*/
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache) /* nothing, */
#endif

/* Traits of all the options. */
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    { command_line_option,                                                        \
      offsetof(options_t, name),                                                  \
      sizeof(type),                                                               \
      OPTION_TYPE_##type,                                                         \
      pcache,                                                                     \
      OPTION_MOD_##flag },

static const option_t option_traits[] = {
#include "optionsx.h"
};

#undef OPTION_COMMAND

static const int num_options = sizeof(option_traits) / sizeof(option_t);

/* all to default values */
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    default_value,
/* read only source for default option values and names
 * no lock needed since never written
 */
const options_t default_options = {
#include "optionsx.h"
};

#ifndef NOT_DYNAMORIO_CORE /*****************************************/

#    define SELF_PROTECT_OPTIONS() SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT)
#    define SELF_UNPROTECT_OPTIONS() SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT)
/* WARNING: testing positive direction is racy (other threads unprot .data
 * for brief windows), negative is reliable
 */
#    define OPTIONS_PROTECTED() (DATASEC_PROTECTED(DATASEC_RARELY_PROT))

/* Holds a copy of the last read option string from the registry, NOT a
 * canonical option string.
 */
char d_r_option_string[MAX_OPTIONS_STRING] = {
    0,
};
#    define ASSERT_OWN_OPTIONS_LOCK(b, l) ASSERT_OWN_WRITE_LOCK(b, l)
#    define CORE_STATIC static
/* official options */
options_t dynamo_options = {
#    include "optionsx.h"
};
/* Temporary string, static to save stack space in synchronize_dynamic_options().
 * FIXME case 8074: should protect this better as w/o DR dll randomization attacker
 * can repeatedly try to clobber.  Move to heap?  Or shrink stack space elsewhere
 * and put back as synchronize_dynamic_options() local var.
 */
DECLARE_FREQPROT_VAR(char new_option_string[MAX_OPTIONS_STRING],
                     {
                         0,
                     });

/* Temporary structure.  Do not assume that it is initialized. */
options_t temp_options;

/* dynamo_options and temp_options, the two writable option structures,
 * are both protected by this lock, which is kept outside of the protected
 * section to ease bootstrapping issues
 */
DECLARE_CXTSWPROT_VAR(read_write_lock_t options_lock, INIT_READWRITE_LOCK(options_lock));

#else /* !NOT_DYNAMORIO_CORE ****************************************/
#    define ASSERT_OWN_OPTIONS_LOCK(b, l)
#    define CORE_STATIC
#endif /* !NOT_DYNAMORIO_CORE ****************************************/

#undef OPTION_COMMAND

/* INITIALIZATION */

static void
adjust_defaults_for_page_size(options_t *options)
{
#ifndef NOT_DYNAMORIO_CORE /* XXX: clumsy fix for Windows */
    uint page_size = (uint)PAGE_SIZE;

    /* The defaults are known to be appropriate for 4 KiB pages. */
    if (page_size == 4096)
        return;

    /* XXX: This approach is not scalable or maintainable as there may in
     * future be many more options that depend on the page size.
     */

    /* To save space, we trade off some stability/security/debugging value of guard
     * pages by only having them for thread-shared allocations (i#4424).
     * Since 99% of our allocs are in the vmm, there should still be enough guards
     * sprinkled to be quite helpful, and we have separate stack guard pages.
     */
    options->per_thread_guard_pages = false;

    options->vmm_block_size = ALIGN_FORWARD(options->vmm_block_size, page_size);
    options->stack_size = MAX(ALIGN_FORWARD(options->stack_size, page_size), page_size);
#    ifdef UNIX
    options->signal_stack_size =
        MAX(ALIGN_FORWARD(options->signal_stack_size, page_size), page_size);
#    endif
    /* These per-thread sizes do *not* have guard pages (i#4424) and
     * we keep them as small as we can to avoid wasting space.
     * We'd need sub-page allocs (i#4415) to go any smaller.
     */
    options->initial_heap_unit_size =
        MAX(ALIGN_FORWARD(options->initial_heap_unit_size, page_size), page_size);
    options->initial_heap_nonpers_size =
        MAX(ALIGN_FORWARD(options->initial_heap_nonpers_size, page_size), page_size);
    /* We increase the global units to have a higher content-to-guard-page ratio. */
    options->initial_global_heap_unit_size = MAX(
        ALIGN_FORWARD(options->initial_global_heap_unit_size, page_size), 8 * page_size);
    options->max_heap_unit_size =
        MAX(ALIGN_FORWARD(options->max_heap_unit_size, page_size), 64 * page_size);
    options->heap_commit_increment =
        ALIGN_FORWARD(options->heap_commit_increment, page_size);
    /* The cache options must all match for x64.  We go ahead and make them the
     * same for 32-bit as well: the shared cache these days should have large units.
     */
    options->cache_shared_bb_unit_max =
        MAX(ALIGN_FORWARD(options->cache_shared_bb_unit_max, page_size), 8 * page_size);
    options->cache_shared_bb_unit_init =
        MAX(ALIGN_FORWARD(options->cache_shared_bb_unit_init, page_size), 8 * page_size);
    options->cache_shared_bb_unit_upgrade = MAX(
        ALIGN_FORWARD(options->cache_shared_bb_unit_upgrade, page_size), 8 * page_size);
    options->cache_shared_bb_unit_quadruple = MAX(
        ALIGN_FORWARD(options->cache_shared_bb_unit_quadruple, page_size), 8 * page_size);
    options->cache_shared_trace_unit_max = MAX(
        ALIGN_FORWARD(options->cache_shared_trace_unit_max, page_size), 8 * page_size);
    options->cache_shared_trace_unit_init = MAX(
        ALIGN_FORWARD(options->cache_shared_trace_unit_init, page_size), 8 * page_size);
    options->cache_shared_trace_unit_upgrade =
        MAX(ALIGN_FORWARD(options->cache_shared_trace_unit_upgrade, page_size),
            8 * page_size);
    options->cache_shared_trace_unit_quadruple =
        MAX(ALIGN_FORWARD(options->cache_shared_trace_unit_quadruple, page_size),
            8 * page_size);
    /* Private units just need to be page sized for possible selfprot. */
    options->cache_bb_unit_max =
        MAX(ALIGN_FORWARD(options->cache_bb_unit_max, page_size), page_size);
    options->cache_bb_unit_init =
        MAX(ALIGN_FORWARD(options->cache_bb_unit_init, page_size), page_size);
    options->cache_bb_unit_upgrade =
        MAX(ALIGN_FORWARD(options->cache_bb_unit_upgrade, page_size), page_size);
    options->cache_bb_unit_quadruple =
        MAX(ALIGN_FORWARD(options->cache_bb_unit_quadruple, page_size), page_size);
    options->cache_trace_unit_max =
        MAX(ALIGN_FORWARD(options->cache_trace_unit_max, page_size), page_size);
    options->cache_trace_unit_init =
        MAX(ALIGN_FORWARD(options->cache_trace_unit_init, page_size), page_size);
    options->cache_trace_unit_upgrade =
        MAX(ALIGN_FORWARD(options->cache_trace_unit_upgrade, page_size), page_size);
    options->cache_trace_unit_quadruple =
        MAX(ALIGN_FORWARD(options->cache_trace_unit_quadruple, page_size), page_size);
    options->cache_commit_increment =
        ALIGN_FORWARD(options->cache_commit_increment, page_size);
#endif /* !NOT_DYNAMORIO_CORE */
}

/* sets defaults just like the above initialization */
CORE_STATIC void
set_dynamo_options_defaults(options_t *options)
{
    ASSERT_OWN_OPTIONS_LOCK(options == &dynamo_options || options == &temp_options,
                            &options_lock);
    *options = default_options;
    adjust_defaults_for_page_size(options);
}
#undef OPTION_COMMAND_INTERNAL

/* For all other purposes OPTION_COMMAND_INTERNAL should be equivalent
 * to either OPTION_COMMAND or nothing.
 */
#ifdef EXPOSE_INTERNAL_OPTIONS
#    define OPTION_COMMAND_INTERNAL OPTION_COMMAND
#else
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache) /* nothing */
#endif

/* PARSING HANDLER */
/* returns the space delimited or quote-delimited word
 * starting at strpos in the string str, or NULL if none
 * wordbuf is a caller allocated buffer of size wordbuflen that will hold
     the copied word from the original string, since it assumes it cannot modify it
 */
static char *
getword_common(const char *str, const char **strpos, char *wordbuf, uint wordbuflen,
               bool external /*whether called from outside the option parser*/)
{
    uint i = 0;
    const char *pos = *strpos;
    char quote = '\0';

    if (pos < str || pos >= str + strlen(str))
        return NULL;

    if (*pos == '\0')
        return NULL; /* no more words */

    /* eat leading spaces */
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') {
        pos++;
    }

    /* extract the word */
    if (*pos == '\'' || *pos == '\"' || *pos == '`') {
        quote = *pos;
        pos++; /* don't include surrounding quotes in word */
    }
    while (*pos) {
        if (quote != '\0') {
            /* if quoted, only end on matching quote */
            if (*pos == quote) {
                pos++; /* include the quote */
                break;
            }
        } else {
            /* if not quoted, end on whitespace */
            if (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')
                break;
        }
        if (i < wordbuflen - 1) {
            wordbuf[i++] = *pos;
            pos++;
        } else {
            if (!external) {
                OPTION_PARSE_ERROR(ERROR_OPTION_TOO_LONG_TO_PARSE, 4,
                                   get_application_name(), get_application_pid(), strpos,
                                   IF_DEBUG_ELSE("Terminating", "Continuing"));
            }
            /* just return truncated form */
            break;
        }
    }
    if (i == 0 && quote == '\0' /*not a quoted empty string*/)
        return NULL; /* no more words */

    ASSERT(i < wordbuflen);
    wordbuf[i] = '\0';
    *strpos = pos;

    return wordbuf;
}

/* internal version */
static char *
getword(const char *str, const char **strpos, char *wordbuf, uint wordbuflen)
{
    return getword_common(str, strpos, wordbuf, wordbuflen, false /*internal*/);
}

/* exported version */
char *
d_r_parse_word(const char *str, const char **strpos, char *wordbuf, uint wordbuflen)
{
    return getword_common(str, strpos, wordbuf, wordbuflen, true /*external*/);
}

#define ISBOOL_bool 1
#define ISBOOL_uint 0
#define ISBOOL_uint_size 0
#define ISBOOL_uint_time 0
#define ISBOOL_uint_addr 0
#define ISBOOL_pathstring_t 0
#define ISBOOL_liststring_t 0

static void
parse_bool(bool *var, void *value)
{
    *var = *(bool *)value;
}

static void
parse_uint(uint *var, void *value)
{
    uint num;
    char *opt = (char *)value;

    if ((sscanf(opt, "0x%x", &num) == 1) ||
        (sscanf(opt, "%d", &num) == 1)) { /* atoi(opt) */
        *var = num;
    } else {
        /* var should be pre-initialized to default */
        OPTION_PARSE_ERROR(ERROR_OPTION_BAD_NUMBER_FORMAT, 4, get_application_name(),
                           get_application_pid(), opt,
                           IF_DEBUG_ELSE("Terminating", "Continuing"));
    }
}

static void
parse_uint_size(ptr_uint_t *var, void *value)
{
    char unit;
    ptr_int_t num;
    ptr_int_t factor = 0;

    if (sscanf((char *)value, SZFMT "%c", &num, &unit) == 1) {
        /* no unit specifier, default unit is Kilo for compatibility */
        factor = 1024;
    } else {
        switch (unit) {
        case 'B': // plain bytes
        case 'b': factor = 1; break;
        case 'K': // Kilo (bytes)
        case 'k': factor = 1024; break;
        case 'M': // Mega (bytes)
        case 'm': factor = 1024 * 1024; break;
        case 'G': // Giga (bytes)
        case 'g': factor = 1024ULL * 1024 * 1024; break;
        default:
            /* var should be pre-initialized to default */
            OPTION_PARSE_ERROR(ERROR_OPTION_UNKNOWN_SIZE_SPECIFIER, 4,
                               get_application_name(), get_application_pid(),
                               (char *)value, IF_DEBUG_ELSE("Terminating", "Continuing"));
            return;
        }
    }
    *var = num * factor;
}

static void
parse_uint_time(uint *var, void *value)
{
    char unit;
    int num;
    int factor = 0;

    if (sscanf((char *)value, "%d%c", &num, &unit) == 1) {
        /* no unit specifier, default unit is milliseconds */
        factor = 1;
    } else {
        switch (unit) {
        case 's': factor = 1000; break;      // seconds in milliseconds
        case 'm': factor = 1000 * 60; break; // minutes in milliseconds
        default:
            /* var should be pre-initialized to default */
            OPTION_PARSE_ERROR(ERROR_OPTION_UNKNOWN_TIME_SPECIFIER, 4,
                               get_application_name(), get_application_pid(),
                               (char *)value, IF_DEBUG_ELSE("Terminating", "Continuing"));
            return;
        }
    }
    *var = num * factor;
}

static void
parse_uint_addr(ptr_uint_t *var, void *value)
{
    ptr_uint_t num;
    char *opt = (char *)value;

    if (sscanf(opt, PIFX, &num) == 1) { /* atoi(opt) */
        *var = num;
    } else {
        /* var should be pre-initialized to default */
        OPTION_PARSE_ERROR(ERROR_OPTION_BAD_NUMBER_FORMAT, 4, get_application_name(),
                           get_application_pid(), opt,
                           IF_DEBUG_ELSE("Terminating", "Continuing"));
    }
}

static inline void
parse_pathstring_t(pathstring_t *var, void *value)
{
    strncpy(*var, (char *)value, MAX_PATH_OPTION_LENGTH - 1);
    if (strlen((char *)value) > MAX_PATH_OPTION_LENGTH) {
        OPTION_PARSE_ERROR(ERROR_OPTION_TOO_LONG_TO_PARSE, 4, get_application_name(),
                           get_application_pid(), (char *)value,
                           IF_DEBUG_ELSE("Terminating", "Continuing"));
    }
    /* truncate if max (strncpy doesn't put NULL) */
    (*var)[MAX_PATH_OPTION_LENGTH - 1] = '\0';
}

static void
parse_liststring_t(liststring_t *var, void *value)
{
    /* Case 5727: append by default (separating via ';') for liststring_t, as
     * opposed to what we do for all other option types where the final
     * specifier overwrites all previous.  The special prefix '#' can be used to
     * indicate overwrite.
     */
    size_t len;
    if (*((char *)value) == '#') {
        strncpy(*var, (((char *)value) + 1), MAX_LIST_OPTION_LENGTH - 1);
        len = strlen(((char *)value) + 1);
    } else {
        len = strlen(*var) + strlen((char *)value);
        if (*var[0] != '\0') {
            len++; /*;*/
            strncat(*var, ";", MAX_LIST_OPTION_LENGTH - 1 - strlen(*var));
        }
        strncat(*var, (char *)value, MAX_LIST_OPTION_LENGTH - 1 - strlen(*var));
    }
    if (len >= MAX_LIST_OPTION_LENGTH) {
        /* FIXME: no longer is value always the single too-long factor
         * (could be appending a short option to a very long string), so
         * should we change the message to "option is too long, truncating"?
         */
        OPTION_PARSE_ERROR(ERROR_OPTION_TOO_LONG_TO_PARSE, 4, get_application_name(),
                           get_application_pid(), *var,
                           IF_DEBUG_ELSE("list Terminating", "Continuing"));
    }
    /* truncate if max (strncpy doesn't put NULL, even though strncat does) */
    (*var)[MAX_LIST_OPTION_LENGTH - 1] = '\0';
}

static void
parse_by_type(enum option_type_t type, void *ptr1, void *ptr2)
{
    switch (type) {
    case OPTION_TYPE_bool: parse_bool(ptr1, ptr2); break;
    case OPTION_TYPE_uint: parse_uint(ptr1, ptr2); break;
    case OPTION_TYPE_uint_size: parse_uint_size(ptr1, ptr2); break;
    case OPTION_TYPE_uint_time: parse_uint_time(ptr1, ptr2); break;
    case OPTION_TYPE_uint_addr: parse_uint_addr(ptr1, ptr2); break;
    case OPTION_TYPE_pathstring_t: parse_pathstring_t(ptr1, ptr2); break;
    case OPTION_TYPE_liststring_t: parse_liststring_t(ptr1, ptr2); break;
    }
}

/* We mark this function to be NOINLINE so that in case the compiler unrolls the
 * loop where this function is used, this function is not copied over there
 * mutliple times.  Copying over this code increases code size significantly
 * especially since strcmp() is declared either as macro or as inline.
 */
static NOINLINE void
set_bool_opt(const char *opt, const char *command_line_option, bool *value_true,
             bool *value_false, void **value)
{
    if (strcmp(opt + 1, command_line_option) == 0) {
        *value = value_true;
    } else if (strncmp(opt + 1, "no_", 3) == 0 &&
               strcmp(opt + 4, command_line_option) == 0) {
        *value = value_false;
    }
}

static NOINLINE void
set_nonbool_opt(const char *opt, const char *command_line_option, const char *optstr,
                const char **pos, char *wordbuffer, int max_option_length, void **value)
{
    if (strcmp(opt + 1, command_line_option) == 0) {
        *value = getword(optstr, pos, wordbuffer, max_option_length);
        /* FIXME: check argument */
    }
}

static NOINLINE void
run_option_command(int index, options_t *options, bool for_this_process)
{
    int j = 0;
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    if (index == j) {                                                             \
        statement;                                                                \
    }                                                                             \
    ++j;
#include "optionsx.h"

#undef OPTION_COMMAND
}

/* PR 330860: the for_this_process bool is read in OPTION_COMMAND statements */
static int
set_dynamo_options_common(options_t *options, const char *optstr, bool for_this_process)
{
    char *opt;
    const char *pos = optstr;
    bool got_badopt = false;
    char badopt[MAX_OPTION_LENGTH];

    char wordbuffer[MAX_OPTION_LENGTH];

    /* used in the OPTION_COMMAND define above, declared here to save stack
     * space FIXME : value_true and value_false could be static const if
     * we mark the value arguments to the parse functions const */
    void *value = NULL;
    bool value_true = true, value_false = false;

    if (optstr == NULL)
        return 0;

    ASSERT_OWN_OPTIONS_LOCK(options == &dynamo_options || options == &temp_options,
                            &options_lock);
    ASSERT(!OPTIONS_PROTECTED());
    while ((opt = getword(optstr, &pos, wordbuffer, sizeof(wordbuffer))) != NULL) {
        if (opt[0] == '-') {
            value = NULL;
            int i = 0;
            for (i = 0; i < num_options; ++i) {
                if (option_traits[i].type == OPTION_TYPE_bool) {
                    set_bool_opt(opt, option_traits[i].name, &value_true, &value_false,
                                 &value);
                } else {
                    set_nonbool_opt(opt, option_traits[i].name, optstr, &pos, wordbuffer,
                                    sizeof(wordbuffer), &value);
                }
                if (value != NULL) {
                    void *optptr = (char *)(options) + option_traits[i].offset;
                    parse_by_type(option_traits[i].type, optptr, value);
                    run_option_command(i, options, for_this_process);
                    break;
                }
            }
            if (value != NULL) {
                continue;
            }
        }

        /* no matching option found */
        if (!got_badopt) {
            snprintf(badopt, BUFFER_SIZE_ELEMENTS(badopt), "%s", opt);
            NULL_TERMINATE_BUFFER(badopt);
        }
        got_badopt = true;
    }

    /* we only report the first bad option */
    if (got_badopt) {
        OPTION_PARSE_ERROR(ERROR_OPTION_UNKNOWN, 4, get_application_name(),
                           get_application_pid(), badopt,
                           IF_DEBUG_ELSE("Terminating", "Continuing"));
    }

    return (int)got_badopt;
}

CORE_STATIC int
set_dynamo_options(options_t *options, const char *optstr)
{
    return set_dynamo_options_common(options, optstr, true);
}

#if !defined(NOT_DYNAMORIO_CORE) && defined(WINDOWS)
static int
set_dynamo_options_other_process(options_t *options, const char *optstr)
{
    return set_dynamo_options_common(options, optstr, false);
}
#endif

/* max==0 means no max and 0 is an ok value */
/* if option is incompatible, will try to touch up the option
 * by assigning min to make it valid returns true if changed the
 * option value
 */
bool
check_param_bounds(ptr_uint_t *val, ptr_uint_t min, ptr_uint_t max, const char *name)
{
    bool ret = false;
    ptr_uint_t new_val;
    if ((max == 0 && *val != 0 && *val < min) ||
        (max > 0 && (*val < min || *val > max))) {
        if (max == 0) {
            new_val = min;
            USAGE_ERROR("%s must be >= " SZFMT ", resetting from " SZFMT " to " SZFMT,
                        name, min, *val, new_val);
        } else {
            new_val = max;
            USAGE_ERROR("%s must be >= " SZFMT " and <= " SZFMT ", resetting from " SZFMT
                        " to " SZFMT,
                        name, min, max, *val, new_val);
        }
        *val = new_val;
        ret = true;
    }
    DOLOG(1, LOG_CACHE, {
        if (*val == 0) {
            LOG(GLOBAL, LOG_CACHE, 1, "%s: <unlimited>\n", name);
        } else {
            LOG(GLOBAL, LOG_CACHE, 1, "%s: " SZFMT " KB\n", name, *val / 1024);
        }
    });
    return ret;
}

/* Print an option string from an option struct.
 * Xref case 7939, in DEBUG builds the ? : creates an unshared implicit local
 * that led to huge stack usage for update_dynamic_options() so we use
 * methods instead of macros for those.  While we could make these inline
 * (which DEBUG will ignore and in release will result in the same code as
 * MACRO version) these are way off the hot path and this way cuts the
 * size of the release build dll by ~7kb.
 */
static void
PRINT_STRING_bool(char *optionbuff, const void *val_ptr, const char *option)
{
    bool value = *(const bool *)val_ptr;
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s%s ", value ? "" : "no_", option);
}
static void
PRINT_STRING_uint(char *optionbuff, const void *val_ptr, const char *option)
{
    /* FIXME: 0x100 hack to get logmask printed in hex,
     * loglevel etc in decimal */
    uint value = *(const uint *)val_ptr;
    snprintf(optionbuff, MAX_OPTION_LENGTH, (value > 0x100 ? "-%s 0x%x " : "-%s %u "),
             option, value);
}
static void
PRINT_STRING_uint_size(char *optionbuff, const void *val_ptr, const char *option)
{
    ptr_uint_t value = *(const ptr_uint_t *)val_ptr;
    char code = 'B';
    if (value >= 1024ULL * 1024 * 1024 && value % 1024ULL * 1024 * 1024 == 0) {
        value = value / (1024ULL * 1024 * 1024);
        code = 'G';
    } else if (value >= 1024 * 1024 && value % 1024 * 1024 == 0) {
        value = value / (1024 * 1024);
        code = 'M';
    } else if (value >= 1024 && value % 1024 == 0) {
        value = value / 1024;
        code = 'K';
    }
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s " SZFMT "%c ", option, value, code);
}
static void
PRINT_STRING_uint_time(char *optionbuff, const void *val_ptr, const char *option)
{
    uint value = *(const uint *)val_ptr;
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s %d ", option, value);
}
static void
PRINT_STRING_uint_addr(char *optionbuff, const void *val_ptr, const char *option)
{
    ptr_uint_t value = *(const ptr_uint_t *)val_ptr;
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s " PIFX " ", option, value);
}
static void
PRINT_STRING_pathstring_t(char *optionbuff, const void *val_ptr, const char *option)
{
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s '%s' ", option,
             (*(const pathstring_t *)val_ptr));
}
static void
PRINT_STRING_liststring_t(char *optionbuff, const void *val_ptr, const char *option)
{
    snprintf(optionbuff, MAX_OPTION_LENGTH, "-%s '%s' ", option,
             (*(const liststring_t *)val_ptr));
}

static void
print_option_type(enum option_type_t type, char *optionbuff, const void *val_ptr,
                  const char *option)
{
    switch (type) {
    case OPTION_TYPE_bool: PRINT_STRING_bool(optionbuff, val_ptr, option); break;
    case OPTION_TYPE_uint: PRINT_STRING_uint(optionbuff, val_ptr, option); break;
    case OPTION_TYPE_uint_size:
        PRINT_STRING_uint_size(optionbuff, val_ptr, option);
        break;
    case OPTION_TYPE_uint_time:
        PRINT_STRING_uint_time(optionbuff, val_ptr, option);
        break;
    case OPTION_TYPE_uint_addr:
        PRINT_STRING_uint_addr(optionbuff, val_ptr, option);
        break;
    case OPTION_TYPE_pathstring_t:
        PRINT_STRING_pathstring_t(optionbuff, val_ptr, option);
        break;
    case OPTION_TYPE_liststring_t:
        PRINT_STRING_liststring_t(optionbuff, val_ptr, option);
        break;
    }
}

static int
DIFF_bool(const void *ptr1, const void *ptr2)
{
    bool val1 = *(const bool *)(ptr1);
    bool val2 = *(const bool *)(ptr2);
    return val1 != val2;
}
static int
DIFF_uint(const void *ptr1, const void *ptr2)
{
    uint val1 = *(const uint *)(ptr1);
    uint val2 = *(const uint *)(ptr2);
    return val1 != val2;
}
static int
DIFF_uint_size(const void *ptr1, const void *ptr2)
{
    ptr_uint_t val1 = *(const ptr_uint_t *)(ptr1);
    ptr_uint_t val2 = *(const ptr_uint_t *)(ptr2);
    return val1 != val2;
}
static int
DIFF_uint_time(const void *ptr1, const void *ptr2)
{
    return DIFF_uint(ptr1, ptr2);
}
static int
DIFF_uint_addr(const void *ptr1, const void *ptr2)
{
    return DIFF_uint_size(ptr1, ptr2);
}
static int
DIFF_pathstring_t(const void *ptr1, const void *ptr2)
{
    const char *val1 = (const char *)ptr1;
    const char *val2 = (const char *)ptr2;
    return strcmp(val1, val2);
}
static int
DIFF_liststring_t(const void *ptr1, const void *ptr2)
{
    const char *val1 = (const char *)ptr1;
    const char *val2 = (const char *)ptr2;
    return strcmp(val1, val2);
}

static int
diff_by_type(enum option_type_t type, const void *ptr1, const void *ptr2)
{
    switch (type) {
    case OPTION_TYPE_bool: return DIFF_bool(ptr1, ptr2);
    case OPTION_TYPE_uint: return DIFF_uint(ptr1, ptr2);
    case OPTION_TYPE_uint_size: return DIFF_uint_size(ptr1, ptr2);
    case OPTION_TYPE_uint_time: return DIFF_uint_time(ptr1, ptr2);
    case OPTION_TYPE_uint_addr: return DIFF_uint_addr(ptr1, ptr2);
    case OPTION_TYPE_pathstring_t: return DIFF_pathstring_t(ptr1, ptr2);
    case OPTION_TYPE_liststring_t: return DIFF_liststring_t(ptr1, ptr2);
    }
    return 0;
}

static void
COPY_bool(void *ptr1, const void *ptr2)
{
    *(bool *)(ptr1) = *(const bool *)(ptr2);
}
static void
COPY_uint(void *ptr1, const void *ptr2)
{
    *(uint *)(ptr1) = *(const uint *)(ptr2);
}
static void
COPY_uint_size(void *ptr1, const void *ptr2)
{
    *(ptr_uint_t *)(ptr1) = *(const ptr_uint_t *)(ptr2);
}
static void
COPY_uint_time(void *ptr1, const void *ptr2)
{
    COPY_uint(ptr1, ptr2);
}
static void
COPY_uint_addr(void *ptr1, const void *ptr2)
{
    COPY_uint_size(ptr1, ptr2);
}
static void
COPY_pathstring_t(void *ptr1, const void *ptr2)
{
    char *val1 = (char *)ptr1;
    const char *val2 = (const char *)ptr2;
    strncpy(val1, val2, sizeof(pathstring_t));
}
static void
COPY_liststring_t(void *ptr1, const void *ptr2)
{
    char *val1 = (char *)ptr1;
    const char *val2 = (const char *)ptr2;
    strncpy(val1, val2, sizeof(liststring_t));
}

static void
copy_by_type(enum option_type_t type, void *ptr1, const void *ptr2)
{
    switch (type) {
    case OPTION_TYPE_bool: COPY_bool(ptr1, ptr2); break;
    case OPTION_TYPE_uint: COPY_uint(ptr1, ptr2); break;
    case OPTION_TYPE_uint_size: COPY_uint_size(ptr1, ptr2); break;
    case OPTION_TYPE_uint_time: COPY_uint_time(ptr1, ptr2); break;
    case OPTION_TYPE_uint_addr: COPY_uint_addr(ptr1, ptr2); break;
    case OPTION_TYPE_pathstring_t: COPY_pathstring_t(ptr1, ptr2); break;
    case OPTION_TYPE_liststring_t: COPY_liststring_t(ptr1, ptr2); break;
    }
}

/* Keep in synch with get_pcache_dynamo_options_string */
void
get_dynamo_options_string(options_t *options, char *opstr, int len, bool minimal)
{
    char optionbuff[MAX_OPTION_LENGTH];
    opstr[0] = 0;

    int i;
    for (i = 0; i < num_options; ++i) {
        if (option_traits[i].name[0] != ' ') { /* not synthetic */
            const void *val1 = (char *)(options) + option_traits[i].offset;
            const void *val2 = (char *)(&default_options) + option_traits[i].offset;
            if (!minimal || diff_by_type(option_traits[i].type, val1, val2)) {
                print_option_type(option_traits[i].type, optionbuff, val1,
                                  option_traits[i].name);
                NULL_TERMINATE_BUFFER(optionbuff);
                strncat(opstr, optionbuff, len - strlen(opstr) - 1);
            }
        }
    }

    opstr[len - 1] = 0;
}

/* Fills opstr with a minimal string of only
 * persistent-cache-affecting options whose effect is >= pcache_effect
 * and that are different from the defaults.
 * Keep in synch with get_dynamo_options_string.
 */
void
get_pcache_dynamo_options_string(options_t *options, char *opstr, int len,
                                 op_pcache_t pcache_effect)
{
    char optionbuff[MAX_OPTION_LENGTH];
    opstr[0] = 0;

    int i;
    for (i = 0; i < num_options; ++i) {
        if (option_traits[i].affects_pcache >= pcache_effect &&
            option_traits[i].name[0] != ' ') { /* not synthetic */
            const void *val1 = (char *)(options) + option_traits[i].offset;
            const void *val2 = (char *)(&default_options) + option_traits[i].offset;
            if (diff_by_type(option_traits[i].type, val1, val2)) {
                print_option_type(option_traits[i].type, optionbuff, val1,
                                  option_traits[i].name);
                NULL_TERMINATE_BUFFER(optionbuff);
                strncat(opstr, optionbuff, len - strlen(opstr) - 1);
            }
        }
    }

    opstr[len - 1] = 0;
}

/* Returns whether any persistent-cache-affecting options whose effect
 * is == pcache_effect were passed in that are different from the defaults.
 */
bool
has_pcache_dynamo_options(options_t *options, op_pcache_t pcache_effect)
{
    int i;
    for (i = 0; i < num_options; ++i) {
        if (option_traits[i].affects_pcache == pcache_effect) {
            const void *val1 = (char *)(options) + option_traits[i].offset;
            const void *val2 = (char *)(&default_options) + option_traits[i].offset;
            if (diff_by_type(option_traits[i].type, val1, val2)) {
                return true;
            }
        }
    }
    return false;
}

#if defined(DEBUG) && defined(INTERNAL)
/* Used in update_dynamic_options() below. Usage is thread-safe as potential
 * accesses are protected by the options_lock. */
static char optionbuff[MAX_OPTION_LENGTH];
static char new_optionbuff[MAX_OPTION_LENGTH];
#endif

/* need to see if any dynamic options have changed and copy them over */
/* returns number of updated dynamic options */
static int
update_dynamic_options(options_t *options, options_t *new_options)
{
    int updated = 0;

    ASSERT_OWN_OPTIONS_LOCK(options == &dynamo_options || options == &temp_options,
                            &options_lock);
    ASSERT(!OPTIONS_PROTECTED());
    int i;
    for (i = 0; i < num_options; ++i) {
        void *val1 = (char *)(options) + option_traits[i].offset;
        const void *val2 = (char *)(new_options) + option_traits[i].offset;

        if (OPTION_MOD_DYNAMIC == option_traits[i].modifier) {
            if (diff_by_type(option_traits[i].type, val1, val2)) {
                copy_by_type(option_traits[i].type, val1, val2);
                updated++;
            }
        } else {
            DOLOG(2, LOG_TOP, {
                if (diff_by_type(option_traits[i].type, val1, val2)) {
                    print_option_type(option_traits[i].type, optionbuff, val1,
                                      option_traits[i].name);
                    NULL_TERMINATE_BUFFER(optionbuff);
                    print_option_type(option_traits[i].type, new_optionbuff, val2,
                                      option_traits[i].name);
                    NULL_TERMINATE_BUFFER(new_optionbuff);
                    LOG(GLOBAL, LOG_TOP, 2,
                        "Updating dynamic options : Ignoring static option change "
                        "(%.*s changed to %.*s)\n",
                        MAX_LOG_LENGTH / 2 - 80, optionbuff, MAX_LOG_LENGTH / 2 - 80,
                        new_optionbuff);
                }
            });
        }
    }

    return updated;
}

void
options_enable_code_api_dependences(options_t *options)
{
    if (!options->code_api)
        return;

        /* PR 202669: larger stack size since we're saving a 512-byte
         * buffer on the stack when saving fp state.
         * Also, C++ RTL initialization (even when a C++
         * client does little else) can take a lot of stack space.
         * Furthermore, dbghelp.dll usage via drsyms has been observed
         * to require 36KB, which is already beyond the minimum to
         * share gencode in the same 64K alloc as the stack.
         *
         * XXX: if we raise this beyond 56KB we should adjust the
         * logic in heap_mmap_reserve_post_stack() to handle sharing the
         * tail end of a multi-64K-region stack.
         */
#ifndef NOT_DYNAMORIO_CORE /* XXX: clumsy fix for Windows */
    options->stack_size = MAX(options->stack_size, ALIGN_FORWARD(56 * 1024, PAGE_SIZE));
#endif
#ifdef UNIX
    /* We assume that clients avoid private library code, within reason, and
     * don't need as much space when handling signals.  We still raise the
     * limit a little while saving some per-thread space.
     */
    options->signal_stack_size =
        MAX(options->signal_stack_size, ALIGN_FORWARD(32 * 1024, PAGE_SIZE));
#endif

    /* For CI builds we'll disable elision by default since we
     * expect most CI users will prefer a view of the
     * instruction stream that's as unmodified as possible.
     * Also xref PR 214169: eliding calls presents a confusing
     * view of basic blocks since clients see both the call
     * and the called function in the same block.  TODO PR
     * 214169: pass both sides to the client and merge
     * internally to get the best of both worlds.
     */
    options->max_elide_jmp = 0;
    options->max_elide_call = 0;

    /* indcall2direct causes problems with the code manip API,
     * so disable by default (xref PR 214051 & PR 214169).
     * Even if we address those issues, we may want to keep
     * disabled if we expect users will be confused by this
     * optimization.
     */
    options->indcall2direct = false;

    /* To support clients changing syscall numbers we need to
     * be able to swap ignored for non-ignored (xref PR 307284)
     */
    options->inline_ignored_syscalls = false;

    /* Clients usually want to see all the code, regardless of bugs and
     * perf issues, so we empty the default native exec list when using
     * -code_api.  The user can override this behavior by passing their
     * own -native_exec_list.
     * However the .pexe section thing on Vista is too dangerous so we
     * leave that on. */
    memset(options->native_exec_default_list, 0,
           sizeof(options->native_exec_default_list));
    options->native_exec_managed_code = false;

    /* Don't randomize dynamorio.dll */
    IF_WINDOWS(options->aslr_dr = false;)
}

/****************************************************************************/
#ifndef NOT_DYNAMORIO_CORE
/* compare short_name, usually module name, against a list option of the combined
 * default option (that could be overridden) and append list that is usually used
 */
list_default_or_append_t
check_list_default_and_append(liststring_t default_list, liststring_t append_list,
                              const char *short_name)
{
    list_default_or_append_t onlist = LIST_NO_MATCH;
    ASSERT(short_name != NULL);
    /* The wildcard '*' is currently expected to be tested by callers
     * to allow modules without a PE name.  FIXME: Alternatively could
     * check whether either list is * and also handle NULL name.
     *
     * FIXME: case 3858 about providing a substitute PE name
     */
    /* inlined IS_STRING_OPTION_EMPTY */
    if (!(default_list[0] == '\0')) {
        string_option_read_lock();
        LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 2,
            "check_list_default_and_append: module %s vs default list %s\n", short_name,
            default_list);
        if (check_filter(default_list, short_name))
            onlist = LIST_ON_DEFAULT;
        string_option_read_unlock();
    }
    if (!onlist && !(append_list[0] == '\0')) {
        string_option_read_lock();
        LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 2,
            "check_list_default_and_append: module %s vs append list %s\n", short_name,
            append_list);
        if (check_filter(append_list, short_name))
            onlist = LIST_ON_APPEND;
        string_option_read_unlock();
    }
    return onlist;
}

/* security options have to be enabled to be blocking or reporting */
#    define SECURITY_OPTION_CONSISTENT(security_option)                                  \
        do {                                                                             \
            if (!TEST(OPTION_ENABLED, DYNAMO_OPTION(security_option)) &&                 \
                TESTANY(OPTION_BLOCK | OPTION_REPORT, DYNAMO_OPTION(security_option))) { \
                USAGE_ERROR("Incompatible settings for %s", #security_option);           \
                dynamo_options.security_option = OPTION_DISABLED;                        \
                changed_options = true;                                                  \
            }                                                                            \
        } while (0)

/* check for incompatible options */
static bool
check_option_compatibility_helper(int recurse_count)
{
    bool changed_options = false;
#    ifdef AARCH64
    if (!DYNAMO_OPTION(bb_prefixes)) {
        USAGE_ERROR("bb_prefixes must be true on AArch64");
        dynamo_options.bb_prefixes = true;
        changed_options = true;
    }
#    endif
#    ifdef EXPOSE_INTERNAL_OPTIONS
    if (DYNAMO_OPTION(vmm_block_size) < MIN_VMM_BLOCK_SIZE) {
        USAGE_ERROR("vmm_block_size (%d) must be >= %d, setting to min",
                    DYNAMO_OPTION(vmm_block_size), MIN_VMM_BLOCK_SIZE);
        dynamo_options.vmm_block_size = MIN_VMM_BLOCK_SIZE;
        changed_options = true;
    }
    if (!INTERNAL_OPTION(inline_calls) && !DYNAMO_OPTION(disable_traces)) {
        /* cannot disable inlining of calls and build traces (currently) */
        USAGE_ERROR("-no_inline_calls not compatible with -disable_traces, setting "
                    "to default");
        SET_DEFAULT_VALUE(inline_calls);
        SET_DEFAULT_VALUE(disable_traces);
        changed_options = true;
    }
    if (INTERNAL_OPTION(tracedump_binary) && INTERNAL_OPTION(tracedump_text)) {
        USAGE_ERROR("Cannot set both -tracedump_binary and -tracedump_text, setting "
                    "to default");
        SET_DEFAULT_VALUE(tracedump_binary);
        SET_DEFAULT_VALUE(tracedump_text);
        changed_options = true;
    }
    if (INTERNAL_OPTION(trace_threshold) > USHRT_MAX) {
        USAGE_ERROR("trace threshold (%d) must be <= USHRT_MAX (%d), setting to max",
                    INTERNAL_OPTION(trace_threshold), USHRT_MAX, USHRT_MAX);
        /* user was probably trying to make the threshold very high, set it to
         * max
         */
        /* from derek : this could wreak havoc w/ trace building fencepost
         * cases... in the case where if head gets hot but somebody
         * else is building trace w/ it you wait, and end up inc-ing counter
         * again, in which case would wrap around and not be hot!
         * (THCI already has problem w/ that b/c it only checks for == not >=
         * (to avoid eflags))
         */
        /* FIXME : we may want to set to USHRT_MAX-10 or some such, same with
         * check above */
        dynamo_options.trace_threshold = USHRT_MAX;
        changed_options = true;
    }
    if (INTERNAL_OPTION(trace_counter_on_delete) > INTERNAL_OPTION(trace_threshold)) {
        USAGE_ERROR("trace_counter_on_delete cannot be > trace_threshold");
        SET_DEFAULT_VALUE(trace_counter_on_delete);
        changed_options = true;
    }
    if (INTERNAL_OPTION(alt_hash_func) >= HASH_FUNCTION_ENUM_MAX) {
        USAGE_ERROR("Invalid selection (%d) for shared cache hash func, must be < %d",
                    INTERNAL_OPTION(alt_hash_func), HASH_FUNCTION_ENUM_MAX);
        SET_DEFAULT_VALUE(alt_hash_func);
        changed_options = true;
    }
    if (DYNAMO_OPTION(inline_bb_ibl) && DYNAMO_OPTION(shared_bbs) &&
        !DYNAMO_OPTION(atomic_inlined_linking)) {
        USAGE_ERROR("-inline_bb_ibl requires -atomic_inlined_linking when -shared_bbs");
        dynamo_options.atomic_inlined_linking = true;
        changed_options = true;
    }
#        ifdef SHARING_STUDY
    if (INTERNAL_OPTION(fragment_sharing_study) && SHARED_FRAGMENTS_ENABLED()) {
        USAGE_ERROR("-fragment_sharing_study requires only private fragments");
        dynamo_options.fragment_sharing_study = false;
        changed_options = true;
    }
#        endif
#    endif /* EXPOSE_INTERNAL_OPTIONS */

    if (!ALIGNED(DYNAMO_OPTION(stack_size), PAGE_SIZE)) {
        USAGE_ERROR("-stack_size must be at least 12K and a multiple of the page size");
        SET_DEFAULT_VALUE(stack_size);
        changed_options = true;
    }

#    if defined(TRACE_HEAD_CACHE_INCR)
    if (DYNAMO_OPTION(pad_jmps)) {
        USAGE_ERROR("-pad_jmps not supported in this build yet");
    }
#    endif

    /****************************************************************************
     * warn of unfinished and untested self-protection options
     * FIXME: update once these features are complete
     */
    if (
#    ifdef WINDOWS
        /* FIXME: CACHE isn't multithread safe yet */
        TEST(SELFPROT_CACHE, dynamo_options.protect_mask) ||
#    endif
        /* FIXME: LOCAL has some unresolved issues w/ new heap units, etc. */
        TEST(SELFPROT_LOCAL, dynamo_options.protect_mask) ||
        TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        ASSERT_NOT_TESTED();
    }
    /* warn of incompatible options */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask) &&
        !TEST(SELFPROT_GLOBAL, dynamo_options.protect_mask)) {
        USAGE_ERROR("dcontext is only actually protected if global is as well");
        /* FIXME: turn off dcontext?  or let upcontext be split anyway? */
    }
    /* FIXME: better way to enforce these incompatibilities w/ certain builds
     * than by turning off protection?  Should we halt instead?
     */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask) &&
        SHARED_FRAGMENTS_ENABLED()) {
        /* FIXME: get all shared gen routines to properly handle unprotected_context_t */
        USAGE_ERROR("Shared cache does not support protecting dcontext yet");
        dynamo_options.protect_mask &= ~SELFPROT_DCONTEXT;
        changed_options = true;
    }

#    if defined(MACOS) && defined(AARCH64)
    if (TEST(SELFPROT_GENCODE, dynamo_options.protect_mask)) {
        USAGE_ERROR("memory protection changes incompatible with MAP_JIT");
        dynamo_options.protect_mask &= ~SELFPROT_GENCODE;
        changed_options = true;
    }
#    endif

#    ifdef TRACE_HEAD_CACHE_INCR
    if (TESTANY(SELFPROT_LOCAL | SELFPROT_GLOBAL, dynamo_options.protect_mask)) {
        USAGE_ERROR("Cannot protect heap in a TRACE_HEAD_CACHE_INCR build");
        dynamo_options.protect_mask &= ~(SELFPROT_LOCAL | SELFPROT_GLOBAL);
        changed_options = true;
    }
#    endif
    /* case 9714: The client interface is compatible with the current default
     * protect_mask of 0x101, but is incompatible with the following:
     */
    if (TESTANY(SELFPROT_DATA_CXTSW | SELFPROT_GLOBAL | SELFPROT_DCONTEXT |
                    SELFPROT_LOCAL | SELFPROT_CACHE | SELFPROT_STACK,
                dynamo_options.protect_mask)) {
        USAGE_ERROR("client support incompatible with protect_mask %x at this time",
                    dynamo_options.protect_mask);
        dynamo_options.protect_mask &=
            ~(SELFPROT_DATA_CXTSW | SELFPROT_GLOBAL | SELFPROT_DCONTEXT | SELFPROT_LOCAL |
              SELFPROT_CACHE | SELFPROT_STACK);
        changed_options = true;
    }
    if (PRIVATE_TRACES_ENABLED() && DYNAMO_OPTION(shared_bbs)) {
        /* Due to complications with shadowing, we do not support
         * private traces and shared bbs if we allow clients to make custom
         * traces (which is always enabled).
         */
        USAGE_ERROR("private traces incompatible with shared bbs");
        dynamo_options.shared_bbs = false;
        changed_options = true;
    }
    /****************************************************************************/

#    if defined(PROFILE_RDTSC) && defined(SIDELINE)
    if (dynamo_options.profile_times && dynamo_options.sideline) {
        USAGE_ERROR("-profile_times incompatible with -sideline, setting to defaults");
        SET_DEFAULT_VALUE(profile_times);
        SET_DEFAULT_VALUE(sideline);
        changed_options = true;
    }
#    endif

#    ifdef UNIX
#        ifndef HAVE_TLS
    if (SHARED_FRAGMENTS_ENABLED()) {
        USAGE_ERROR("shared fragments not supported on this OS");
        dynamo_options.shared_bbs = false;
        dynamo_options.shared_traces = false;
        changed_options = true;
    }
#            if defined(X64) && !(defined(MACOS) && defined(AARCH64))
    /* PR 361894: we do not support x64 without TLS (xref PR 244737) */
#                error X64 requires HAVE_TLS
#            endif
#        endif

#        if !defined(HAVE_MEMINFO) && defined(PROGRAM_SHEPHERDING)
    /* PR 235433: without +x info we cannot support code origins */
    if (DYNAMO_OPTION(code_origins)) {
        USAGE_ERROR("-code_origins not supported on this OS");
        dynamo_options.code_origins = false;
        changed_options = true;
    }
    /* FIXME: We can't support certain GBOP policies, either.  Anything else? */
#        endif
#    endif

    /* Manipulate all of the options needed for -shared_traces. */
    if (DYNAMO_OPTION(shared_traces)) {
        if (!DYNAMO_OPTION(private_ib_in_tls)) {
            SYSLOG_INTERNAL_INFO("-shared_traces requires -private_ib_in_tls, enabling");
            dynamo_options.private_ib_in_tls = true;
            changed_options = true;
        }
        if (!DYNAMO_OPTION(shared_trace_ibl_routine)) {
            SYSLOG_INTERNAL_INFO("-shared_traces requires -shared_trace_ibl_routine, "
                                 "enabling");
            dynamo_options.shared_trace_ibl_routine = true;
            changed_options = true;
        }
        if (!DYNAMO_OPTION(atomic_inlined_linking)) {
            SYSLOG_INTERNAL_INFO("-shared_traces requires -atomic_inlined_linking, "
                                 "enabling");
            dynamo_options.atomic_inlined_linking = true;
            changed_options = true;
        }
    }
#    ifdef EXPOSE_INTERNAL_OPTIONS
#        ifdef DEADLOCK_AVOIDANCE
    if (INTERNAL_OPTION(mutex_callstack) > MAX_MUTEX_CALLSTACK) {
        USAGE_ERROR("-mutex_callstack is compiled with MAX_MUTEX_CALLSTACK=%d",
                    MAX_MUTEX_CALLSTACK);
        dynamo_options.mutex_callstack = MAX_MUTEX_CALLSTACK;
        changed_options = true;
    }
#        endif
    if (INTERNAL_OPTION(unsafe_ignore_eflags_ibl) &&
        !INTERNAL_OPTION(unsafe_ignore_eflags_prefix)) {
        USAGE_ERROR("-unsafe_ignore_eflags_ibl requires -unsafe_ignore_eflags_prefix, "
                    "enabling");
        dynamo_options.unsafe_ignore_eflags_prefix = true;
        changed_options = true;
    }
#        ifdef X64
    /* saving in the trace and restoring in ibl means that
     * -unsafe_ignore_eflags_{trace,ibl} must be equivalent
     */
    if ((INTERNAL_OPTION(unsafe_ignore_eflags_ibl) &&
         !INTERNAL_OPTION(unsafe_ignore_eflags_trace)) ||
        (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl) &&
         INTERNAL_OPTION(unsafe_ignore_eflags_trace))) {
        USAGE_ERROR("-unsafe_ignore_eflags_ibl must match -unsafe_ignore_eflags_trace "
                    "for x64: enabling both");
        dynamo_options.unsafe_ignore_eflags_trace = true;
        dynamo_options.unsafe_ignore_eflags_ibl = true;
        changed_options = true;
    }
#        endif
#    endif /* EXPOSE_INTERNAL_OPTIONS */
#    ifdef X64
    if (DYNAMO_OPTION(heap_in_lower_4GB) && !DYNAMO_OPTION(reachable_heap)) {
        USAGE_ERROR("-heap_in_lower_4GB requires -reachable_heap: "
                    "enabling.");
        dynamo_options.reachable_heap = true;
        changed_options = true;
    }
#    endif
    if (RUNNING_WITHOUT_CODE_CACHE() && DYNAMO_OPTION(enable_reset)) {
        /* No reset for hotp_only and thin_client modes; case 8389. */
        USAGE_ERROR("-enable_reset can't be used with -hotp_only or -thin_client");
        DISABLE_RESET(&dynamo_options);
    }
    if (DYNAMO_OPTION(reset_at_vmm_percent_free_limit) > 100) {
        USAGE_ERROR("-reset_at_vmm_percent_free_limit is percentage value, "
                    "can't be > 100");
        dynamo_options.reset_at_vmm_percent_free_limit = 100;
        changed_options = true;
    }
    if (DYNAMO_OPTION(reset_at_commit_percent_free_limit) > 100) {
        USAGE_ERROR("-reset_at_commit_percent_free_limit is percentage value, can't "
                    "be > 100");
        dynamo_options.reset_at_commit_percent_free_limit = 100;
        changed_options = true;
    }
    if (!DYNAMO_OPTION(enable_reset)) {
        if (DYNAMO_OPTION(reset_at_nth_thread)) {
            USAGE_ERROR("-reset_at_nth_thread requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        }
#    ifdef EXPOSE_INTERNAL_OPTIONS
        else if (INTERNAL_OPTION(reset_at_fragment_count)) {
            USAGE_ERROR("-reset_at_fragment_count requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        }
#    endif /* EXPOSE_INTERNAL_OPTIONS */
        else if (DYNAMO_OPTION(reset_at_switch_to_os_at_vmm_limit)) {
            USAGE_ERROR("-reset_at_switch_to_os_at_vmm_limit requires -enable_reset, "
                        " enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_vmm_percent_free_limit) != 0) {
            USAGE_ERROR("-reset_at_vmm_percent_free_limit requires -enable_reset, "
                        " enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_vmm_free_limit) != 0) {
            USAGE_ERROR("-reset_at_vmm_free_limit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_vmm_full)) {
            USAGE_ERROR("-reset_at_vmm_full requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_commit_percent_free_limit) != 0) {
            USAGE_ERROR("-reset_at_commit_percent_free_limit requires -enable_reset,"
                        " enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_commit_free_limit) != 0) {
            USAGE_ERROR("-reset_at_commit_free_limit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_every_nth_pending) > 0) {
            USAGE_ERROR("-reset_every_nth_pending requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_nth_bb_unit) > 0) {
            USAGE_ERROR("-reset_at_nth_bb_unit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_at_nth_trace_unit) > 0) {
            USAGE_ERROR("-reset_at_nth_trace_unit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_every_nth_bb_unit) > 0) {
            USAGE_ERROR("-reset_every_nth_bb_unit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        } else if (DYNAMO_OPTION(reset_every_nth_trace_unit) > 0) {
            USAGE_ERROR("-reset_every_nth_trace_unit requires -enable_reset, enabling");
            dynamo_options.enable_reset = true;
            changed_options = true;
        }
    }

#    ifdef TRACE_HEAD_CACHE_INCR
    if (dynamo_options.shared_traces) {
        USAGE_ERROR("Cannot share traces in a TRACE_HEAD_CACHE_INCR build");
        dynamo_options.shared_traces = false;
        changed_options = true;
    }
#    endif
    /* FIXME We support only shared BBs as IBTs when trace building is on. */
    if (DYNAMO_OPTION(bb_ibl_targets) && !DYNAMO_OPTION(disable_traces) &&
        !DYNAMO_OPTION(shared_bbs)) {
        USAGE_ERROR("-bb_ibl_targets w/traces not supported w/-no_shared_bbs, disabling");
        dynamo_options.bb_ibl_targets = false;
        changed_options = true;
    }
    /* We need private_ib_in_tls for shared BB IBTs. */
    if (DYNAMO_OPTION(bb_ibl_targets) && DYNAMO_OPTION(shared_bbs) &&
        !DYNAMO_OPTION(private_ib_in_tls)) {
        SYSLOG_INTERNAL_INFO("-bb_ibl_targets w/traces requires -private_ib_in_tls, "
                             "enabling");
        dynamo_options.private_ib_in_tls = true;
        changed_options = true;
    }
    /* We need shared tables for shared BB IBTs when trace building is on. */
    if (DYNAMO_OPTION(bb_ibl_targets) && DYNAMO_OPTION(shared_bbs) &&
        !DYNAMO_OPTION(disable_traces) && !DYNAMO_OPTION(shared_bb_ibt_tables)) {
        SYSLOG_INTERNAL_INFO("-bb_ibl_targets -shared_bbs w/traces requires "
                             "-shared_bb_ibt_tables, enabling");
        dynamo_options.shared_bb_ibt_tables = true;
        changed_options = true;
    }
    /* If we're still using BBs as IBTs when trace building is on and want to
     * add traces to the BB IBT tables, don't let private traces get added to
     * a shared table.
     */
    if (DYNAMO_OPTION(bb_ibl_targets) && !DYNAMO_OPTION(disable_traces) &&
        DYNAMO_OPTION(bb_ibt_table_includes_traces) &&
        DYNAMO_OPTION(shared_bb_ibt_tables) && !DYNAMO_OPTION(shared_traces)) {
        SYSLOG_INTERNAL_INFO("-bb_ibt_table_includes_traces -shared_bb_ibt_tables "
                             "requires -shared_traces, disabling "
                             "-bb_ibt_table_includes_traces");
        dynamo_options.bb_ibt_table_includes_traces = false;
        changed_options = true;
    }
    /* When using BBs as IBTs when trace building is on and adding traces to
     * the BB IBT table, BBs & traces must use the same type of prefix. */
    if (DYNAMO_OPTION(bb_ibl_targets) && !DYNAMO_OPTION(disable_traces) &&
        DYNAMO_OPTION(bb_ibt_table_includes_traces) &&
        (DYNAMO_OPTION(trace_single_restore_prefix) !=
         DYNAMO_OPTION(bb_single_restore_prefix))) {
        SYSLOG_INTERNAL_INFO("For -bb_ibl_targets -bb_ibt_table_includes_traces, "
                             "traces & BBs must use identical prefixes");
        /* FIXME We could either set trace_single_restore_prefix and
         * bb_single_restore_prefix to the same value or use
         * -no_bb_ibt_table_includes_traces. For now, we do the latter as
         * it's less disruptive overall -- the trace prefix setting isn't
         * modified and full prefixes are not used on BBs, limiting the
         * cache/memory size increase. We need to measure to determine
         * the proper thing to do.
         */
        SYSLOG_INTERNAL_INFO("Disabling -bb_ibt_table_includes_traces");
        dynamo_options.bb_ibt_table_includes_traces = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(syscalls_synch_flush) && !DYNAMO_OPTION(shared_deletion)) {
        /* right now syscalls_synch_flush only affects shared_deletion, so we want
         * to disable it when shared_deletion is off -- but don't yell at user so
         * not a USAGE_ERROR, simply an info event
         */
        SYSLOG_INTERNAL_INFO("-syscalls_synch_flush requires -shared_deletion, "
                             "disabling");
        dynamo_options.syscalls_synch_flush = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(free_private_stubs) && !DYNAMO_OPTION(separate_private_stubs)) {
        USAGE_ERROR("-free_private_stubs requires -separate_private_stubs, disabling");
        dynamo_options.free_private_stubs = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(unsafe_free_shared_stubs) &&
        !DYNAMO_OPTION(separate_shared_stubs)) {
        USAGE_ERROR("-unsafe_free_shared_stubs requires -separate_shared_stubs, "
                    "disabling");
        dynamo_options.unsafe_free_shared_stubs = false;
        changed_options = true;
    }
#    ifdef EXPOSE_INTERNAL_OPTIONS
    if (!DYNAMO_OPTION(indirect_stubs)) {
#        ifdef ARM
        USAGE_ERROR("ARM requires -indirect_stubs, enabling");
        dynamo_options.indirect_stubs = true;
        changed_options = true;
#        endif
#        ifdef PROGRAM_SHEPHERDING
        if (DYNAMO_OPTION(ret_after_call) ||
            DYNAMO_OPTION(rct_ind_call) != OPTION_DISABLED ||
            DYNAMO_OPTION(rct_ind_jump) != OPTION_DISABLED) {
            USAGE_ERROR("C, E, and F policies require -indirect_stubs, enabling");
            dynamo_options.indirect_stubs = true;
            changed_options = true;
        }
#        endif
#        ifdef HASHTABLE_STATISTICS
        if ((!DYNAMO_OPTION(shared_traces) && DYNAMO_OPTION(inline_trace_ibl)) ||
            (!DYNAMO_OPTION(shared_bbs) && DYNAMO_OPTION(inline_bb_ibl))) {
            USAGE_ERROR("private inlined ibl requires -indirect_stubs, enabling");
            dynamo_options.indirect_stubs = true;
            changed_options = true;
        }
#        endif
    }
#    endif
    if ((DYNAMO_OPTION(finite_shared_bb_cache) ||
         DYNAMO_OPTION(finite_shared_trace_cache)) &&
        !DYNAMO_OPTION(cache_shared_free_list)) {
        USAGE_ERROR("-finite_shared_{bb,trace}_cache requires -cache_shared_free_list, "
                    "enabling");
        dynamo_options.cache_shared_free_list = true;
        changed_options = true;
    }
#    if defined(X64) || defined(ARM)
    if (!DYNAMO_OPTION(private_ib_in_tls)) {
        USAGE_ERROR("-private_ib_in_tls is required for x64 and ARM");
        dynamo_options.private_ib_in_tls = true;
        changed_options = true;
    }
#    endif
#    ifdef WINDOWS
    if (DYNAMO_OPTION(shared_fragment_shared_syscalls) &&
        !DYNAMO_OPTION(shared_syscalls)) {
        SYSLOG_INTERNAL_INFO("-shared_fragment_shared_syscalls requires "
                             "-shared_syscalls, disabling");
        dynamo_options.shared_fragment_shared_syscalls = false;
        changed_options = true;
    }
#        ifdef X64
    if (!DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        /* we use tls for the continuation pc, and the shared gencode, always */
        USAGE_ERROR("-shared_fragment_shared_syscalls is required for x64");
        dynamo_options.shared_fragment_shared_syscalls = true;
        changed_options = true;
    }
    if (DYNAMO_OPTION(x86_to_x64_ibl_opt) && !DYNAMO_OPTION(x86_to_x64)) {
        SYSLOG_INTERNAL_INFO("-x86_to_x64 is required for x86_to_x64_ibl_opt. "
                             "Disabling -x86_to_x64_ibl_opt.");
        dynamo_options.x86_to_x64_ibl_opt = false;
        changed_options = true;
    }
#        endif
    /* We retain shared_fragment_shared_syscalls as a separate option since
     * it can be used -- but isn't required -- for shared BBs only mode. */
    if (SHARED_FRAGMENTS_ENABLED() && DYNAMO_OPTION(shared_syscalls) &&
        !DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        SYSLOG_INTERNAL_INFO("-shared_{bbs|traces} w/-shared_syscalls requires "
                             "-shared_fragment_shared_syscalls, enabling");
        dynamo_options.shared_fragment_shared_syscalls = true;
        changed_options = true;
    }
    if (SHARED_IBT_TABLES_ENABLED() && DYNAMO_OPTION(shared_syscalls) &&
        !DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        SYSLOG_INTERNAL_INFO("-shared_{bb|trace}_ibt_tables requires "
                             "-shared_fragment_shared_syscalls, enabling");
        dynamo_options.shared_fragment_shared_syscalls = true;
        changed_options = true;
    }
    /* Don't leave -shared_fragment_shared_syscalls on if we're not using shared
     * fragments: case 8027. */
    /* FIXME We could try to eliminate the info msg by pulling this logic and
     * associated processing into an OPTION_COMMAND (but OPTION_COMMAND has
     * its own imperfections).
     */
    if (DYNAMO_OPTION(shared_fragment_shared_syscalls) &&
        /* x64 uses -shared_fragment_shared_syscalls always */
        IF_X64_ELSE(false, !SHARED_FRAGMENTS_ENABLED())) {
        SYSLOG_INTERNAL_INFO("-shared_fragment_shared_syscalls requires "
                             "-shared_{bbs|traces}, disabling");
        dynamo_options.shared_fragment_shared_syscalls = false;
        changed_options = true;
    }
    /* We don't yet support shared BBs and private traces targeting shared syscall
     * simultaneously: case 5436. */
    if (DYNAMO_OPTION(shared_syscalls) && DYNAMO_OPTION(shared_bbs) &&
        !DYNAMO_OPTION(shared_traces) && !DYNAMO_OPTION(disable_traces)) {
        SYSLOG_INTERNAL_INFO("-shared_syscalls not supported with -shared_bbs "
                             "-no_shared_traces, disabling");
        dynamo_options.shared_syscalls = false;
        changed_options = true;
    }
#        ifdef EXPOSE_INTERNAL_OPTIONS
    if (INTERNAL_OPTION(shared_syscalls_fastpath) && !DYNAMO_OPTION(shared_syscalls)) {
        SYSLOG_INTERNAL_INFO("-shared_syscalls_fastpath requires -shared_syscalls, "
                             "disabling");
        dynamo_options.shared_syscalls_fastpath = false;
        changed_options = true;
    }
    if (INTERNAL_OPTION(shared_syscalls_fastpath) && !DYNAMO_OPTION(disable_traces)) {
        SYSLOG_INTERNAL_INFO("-shared_syscalls_fastpath requires -disable_traces, "
                             "disabling");
        dynamo_options.shared_syscalls_fastpath = false;
        changed_options = true;
    }
#        endif
#    endif
    if (DYNAMO_OPTION(shared_bb_ibt_tables) && !DYNAMO_OPTION(shared_bbs)) {
        SYSLOG_INTERNAL_INFO("-shared_bb_ibt_tables requires -shared_bbs, disabling");
        dynamo_options.shared_bb_ibt_tables = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(shared_bb_ibt_tables) && !DYNAMO_OPTION(bb_ibl_targets)) {
        SYSLOG_INTERNAL_INFO("-shared_bb_ibt_tables requires -bb_ibl_targets, disabling");
        dynamo_options.shared_bb_ibt_tables = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(shared_trace_ibt_tables) && !DYNAMO_OPTION(shared_traces)) {
        SYSLOG_INTERNAL_INFO("-shared_bb_ibt_tables requires -shared_traces, disabling");
        dynamo_options.shared_trace_ibt_tables = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(shared_trace_ibt_tables) && DYNAMO_OPTION(trace_ibt_groom) > 0) {
        USAGE_ERROR("-trace_ibt_groom incompatible -shared_trace_ibt_tables, disabling");
        dynamo_options.trace_ibt_groom = 0;
        changed_options = true;
    }
    if (DYNAMO_OPTION(shared_bb_ibt_tables) && DYNAMO_OPTION(bb_ibt_groom) > 0) {
        USAGE_ERROR("-bb_ibt_groom incompatible -shared_bb_ibt_tables, disabling");
        dynamo_options.bb_ibt_groom = 0;
        changed_options = true;
    }
    if (DYNAMO_OPTION(bb_ibt_groom) != 0 &&
        DYNAMO_OPTION(bb_ibt_groom) > DYNAMO_OPTION(private_bb_ibl_targets_load)) {
        SYSLOG_INTERNAL_INFO("-bb_ibt_groom > private_bb_ibl_targets_load, disabling");
        dynamo_options.bb_ibt_groom = 0;
        changed_options = true;
    }
    if (DYNAMO_OPTION(trace_ibt_groom) != 0 &&
        DYNAMO_OPTION(trace_ibt_groom) > DYNAMO_OPTION(private_ibl_targets_load)) {
        SYSLOG_INTERNAL_INFO("-trace_ibt_groom > private_ibl_targets_load, disabling");
        dynamo_options.trace_ibt_groom = 0;
        changed_options = true;
    }
#    if defined(UNIX) && defined(HAVE_TLS)
    if (!DYNAMO_OPTION(ibl_table_in_tls)) {
        /* xref PR 211147 */
        SYSLOG_INTERNAL_INFO("-no_ibl_table_in_tls invalid on unix, disabling");
        dynamo_options.ibl_table_in_tls = true;
        changed_options = true;
    }
#    endif
    if (DYNAMO_OPTION(IAT_elide) && !DYNAMO_OPTION(IAT_convert)) {
        USAGE_ERROR("-IAT_elide requires -IAT_convert, enabling");
        dynamo_options.IAT_convert = true;
        changed_options = true;
    }

    if (DYNAMO_OPTION(sandbox_writable) && DYNAMO_OPTION(sandbox_non_text)) {
        USAGE_ERROR("-sandbox_writable and -sandbox_non_text are mutually exclusive, "
                    "using -sandbox_non_text");
        dynamo_options.sandbox_writable = false;
        dynamo_options.sandbox_non_text = true;
        changed_options = true;
    }
    if (DYNAMO_OPTION(sandbox2ro_threshold) == 1U) {
        /* since we inc the counter before executing a selfmod fragment,
         * a threshold of 1 can result in no progress
         */
        USAGE_ERROR("-sandbox2ro_threshold cannot be 1, changing to 2");
        dynamo_options.sandbox2ro_threshold = 2U;
        changed_options = true;
    }

#    ifdef WINDOWS
    if (DYNAMO_OPTION(stack_guard_pages)) {
        /* XXX i#2595: this does not interact well with -vm_reserve. */
        USAGE_ERROR("-stack_guard_pages is not supported on Windows");
        dynamo_options.stack_guard_pages = false;
        changed_options = true;
    }
#        ifdef PROGRAM_SHEPHERDING
    if (DYNAMO_OPTION(IAT_convert) && !DYNAMO_OPTION(emulate_IAT_writes)) {
        /* FIXME: case 1948 we should in fact depend on emulate_IAT_read */
        USAGE_ERROR("-IAT_convert requires -emulate_IAT_writes, enabling");
        dynamo_options.emulate_IAT_writes = true;
        changed_options = true;
    }
#        else
    if (DYNAMO_OPTION(IAT_convert)) {
        /* FIXME: case 1948 we should in fact depend on emulate_IAT_read */
        USAGE_ERROR("-IAT_convert requires unavailable -emulate_IAT_writes, "
                    "disabling IAT_convert");
        dynamo_options.IAT_convert = false;
        changed_options = true;
    }
#        endif /* PROGRAM_SHEPHERDING */
#    endif     /* WINDOWS */
#    ifdef X64
    if (DYNAMO_OPTION(satisfy_w_xor_x) &&
        (DYNAMO_OPTION(coarse_enable_freeze) || DYNAMO_OPTION(use_persisted))) {
        /* FIXME i#1566: Just not implemented yet. */
        USAGE_ERROR("-satisfy_w_xor_x does not support persistent caches");
        dynamo_options.satisfy_w_xor_x = false;
        changed_options = true;
    }
#    else
    if (DYNAMO_OPTION(satisfy_w_xor_x)) {
        USAGE_ERROR("-satisfy_w_xor_x is not supported on 32-bit");
        dynamo_options.satisfy_w_xor_x = false;
        changed_options = true;
    }
#    endif
#    ifdef WINDOWS
    if (DYNAMO_OPTION(satisfy_w_xor_x)) {
        /* FIXME i#1566: Just not implemented yet. */
        USAGE_ERROR("-satisfy_w_xor_x is not supported on Windows");
        dynamo_options.satisfy_w_xor_x = false;
        changed_options = true;
    }
#    endif
#    ifdef WINDOWS
    /* In theory ignore syscalls should work for int system calls, and also for
     * sysenter system calls when Sygate SPA is not installed [though haven't
     * tested].  However, ignored sysenter system calls when SPA is installed
     * may lead to them reporting/blocking violations on certain platforms as
     * the necessary mangling is too much at this point (FIXME). */
    if (DYNAMO_OPTION(ignore_syscalls) && DYNAMO_OPTION(sygate_sysenter)) {
        USAGE_ERROR("-ignore_syscalls can't be used with -sygate_sysenter");
        dynamo_options.ignore_syscalls = false;
        changed_options = true;
    }
    /* shared/ignore syscall writes to sysenter_storage dcontext field which
     * should be in upcontext or something FIXME */
    if (DYNAMO_OPTION(sygate_sysenter) &&
        TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask))) {
        USAGE_ERROR("-sygate_sysenter incompatbile with -protect_mask dc");
        dynamo_options.protect_mask &= ~SELFPROT_DCONTEXT;
        changed_options = true;
    }
    if (DYNAMO_OPTION(hook_conflict) > HOOKED_TRAMPOLINE_MAX ||
        DYNAMO_OPTION(hook_conflict) == HOOKED_TRAMPOLINE_HOOK_DEEPER) {
        USAGE_ERROR("-hook_conflict invalid or unsupported value");
        SET_DEFAULT_VALUE(hook_conflict);
        changed_options = true;
    }
    if (DYNAMO_OPTION(native_exec_hook_conflict) > HOOKED_TRAMPOLINE_MAX ||
        DYNAMO_OPTION(native_exec_hook_conflict) == HOOKED_TRAMPOLINE_CHAIN) {
        USAGE_ERROR("-native_exec_hook_conflict invalid or unsupported value");
        SET_DEFAULT_VALUE(native_exec_hook_conflict);
        changed_options = true;
    }
    if (INTERNAL_OPTION(private_peb) && !INTERNAL_OPTION(private_loader)) {
        /* The private peb is set up in loader.c */
        USAGE_ERROR("-private_peb requires -private_loader");
        dynamo_options.private_peb = false;
        changed_options = true;
    }
#    endif

#    ifdef WINDOWS
    SECURITY_OPTION_CONSISTENT(apc_policy);
    SECURITY_OPTION_CONSISTENT(thread_policy);
#    endif
#    ifdef RETURN_AFTER_CALL
    SECURITY_OPTION_CONSISTENT(rct_ret_unreadable);
#    endif
#    ifdef RCT_IND_BRANCH
    SECURITY_OPTION_CONSISTENT(rct_ind_call);
    SECURITY_OPTION_CONSISTENT(rct_ind_jump);
    if (!DYNAMO_OPTION(ret_after_call) &&
        TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) {
        SYSLOG_INTERNAL_INFO(".F depends on .C after calls, disabling .F");
        dynamo_options.rct_ind_jump = OPTION_DISABLED;
        changed_options = true;
    }
#    endif

    if (DYNAMO_OPTION(ibl_hash_func_offset) > IBL_HASH_FUNC_OFFSET_MAX) {
        USAGE_ERROR(
            "-ibl_hash_func_offset currently can only be 0, 1, 2, or 3" IF_X64(" or 4"));
        dynamo_options.ibl_hash_func_offset = IBL_HASH_FUNC_OFFSET_MAX;
        changed_options = true;
    }

#    ifdef HOT_PATCHING_INTERFACE
    /* -hot_patching controls all code relating to reading policies, modes,
     * loading dlls, nudging, etc.  can't do -hotp_only without those.
     */
    if (DYNAMO_OPTION(hotp_only) && !DYNAMO_OPTION(hot_patching)) {
        USAGE_ERROR("-hotp_only depends on -hot_patching, enabling -hot_patching");
        dynamo_options.hot_patching = true;
        changed_options = true;
    }
    /* -hotp_only can't rely on interp to identify/trap system calls as app
     * image will be patched directly, i.e., no interp.  -native_exec_syscalls
     * is needed to gain control for important app system calls.
     */
    if (DYNAMO_OPTION(hotp_only) && !DYNAMO_OPTION(native_exec_syscalls)) {
        USAGE_ERROR("-hotp_only depends on -native_exec_syscalls, "
                    "enabling -native_exec_syscalls");
        dynamo_options.native_exec_syscalls = true;
        changed_options = true;
    }
#        ifdef RCT_IND_BRANCH
    if (DYNAMO_OPTION(hotp_only) &&
        (DYNAMO_OPTION(rct_ind_call) != OPTION_DISABLED ||
         DYNAMO_OPTION(rct_ind_jump) != OPTION_DISABLED)) {
        USAGE_ERROR("-rct_ind_{call,jump} incompatible w/ -hotp_only, disabling");
        dynamo_options.rct_ind_call = OPTION_DISABLED;
        dynamo_options.rct_ind_jump = OPTION_DISABLED;
        changed_options = true;
    }
#        endif
#        ifdef RETURN_AFTER_CALL
    if (DYNAMO_OPTION(hotp_only) && DYNAMO_OPTION(ret_after_call)) {
        USAGE_ERROR("-ret_after_call incompatible w/ -hotp_only, disabling");
        dynamo_options.ret_after_call = false;
        changed_options = true;
    }
    if (DYNAMO_OPTION(borland_SEH_rct) && !DYNAMO_OPTION(process_SEH_push)) {
        USAGE_ERROR("-borland_SEH_rct requires -process_SEH_push, enabling");
        dynamo_options.process_SEH_push = true;
        changed_options = true;
    }
#        endif
#        ifdef KSTATS
    /* case 6837: FIXME: remove once -hotp_only -kstats work */
    if (DYNAMO_OPTION(hotp_only) && DYNAMO_OPTION(kstats)) {
        USAGE_ERROR("-hotp_only doesn't support -kstats");
        dynamo_options.kstats = false;
        changed_options = true;
    }
#        endif /* KSTATS */
    /* Probe API needs hot_patching.  Also, for the time being at least,
     * liveshields shouldn't be on when probe api is on.
     */
    if (DYNAMO_OPTION(probe_api)) {
        if (!DYNAMO_OPTION(hot_patching)) {
            USAGE_ERROR("-probe_api needs -hot_patching");
            dynamo_options.hot_patching = true;
            changed_options = true;
        }
        if (DYNAMO_OPTION(liveshields)) {
            USAGE_ERROR("-probe_api and -liveshields aren't compatible");
            dynamo_options.liveshields = false;
            changed_options = true;
        }
    }
#    endif /* HOT_PATCHING_INTERFACE */
    /* i#660/PR 226578 - Probe API doesn't flush pcaches conflicting with hotpatches. */
    if (DYNAMO_OPTION(probe_api) && DYNAMO_OPTION(use_persisted)) {
        USAGE_ERROR("-probe_api and -use_persisted aren't compatible");
        dynamo_options.use_persisted = false;
        changed_options = true;
    }
#    ifdef UNIX
    /* PR 304708: we intercept all signals for a better client interface */
    if (DYNAMO_OPTION(code_api) && !DYNAMO_OPTION(intercept_all_signals)) {
        USAGE_ERROR("-code_api requires -intercept_all_signals");
        dynamo_options.intercept_all_signals = true;
        changed_options = true;
    }
#    endif
#    ifdef UNIX
    if (DYNAMO_OPTION(max_pending_signals) < 1) {
        USAGE_ERROR("-max_pending_signals must be at least 1");
        dynamo_options.max_pending_signals = 1;
        changed_options = true;
    }
#    endif
#    ifdef CALL_PROFILE
    if (DYNAMO_OPTION(prof_caller) > MAX_CALL_PROFILE_DEPTH) {
        USAGE_ERROR("-prof_caller must be <= %d", MAX_CALL_PROFILE_DEPTH);
        dynamo_options.prof_caller = MAX_CALL_PROFILE_DEPTH;
        changed_options = true;
    }
#    endif

#    ifdef WINDOWS_PC_SAMPLE
    if (DYNAMO_OPTION(prof_pcs_global) < 8 || DYNAMO_OPTION(prof_pcs_global) > 32) {
        USAGE_ERROR("-prof_pcs_global must be >=8 and <= 32, setting to default");
        SET_DEFAULT_VALUE(prof_pcs_global);
        changed_options = true;
    }
    if (DYNAMO_OPTION(prof_pcs_stubs) < 2 || DYNAMO_OPTION(prof_pcs_stubs) > 32) {
        USAGE_ERROR("-prof_pcs_stubs must be >= 2 and <= 32, setting to default");
        /* maybe better to clamp to closest bound */
        SET_DEFAULT_VALUE(prof_pcs_stubs);
        changed_options = true;
    }
#    endif

#    ifdef UNIX
    if (DYNAMO_OPTION(early_inject) && !DYNAMO_OPTION(private_loader)) {
        USAGE_ERROR("-early_inject requires -private_loader, turning on -private_loader");
        dynamo_options.private_loader = true;
        changed_options = true;
    }
#    endif

#    ifdef WINDOWS
    if (DYNAMO_OPTION(inject_at_create_process) && !DYNAMO_OPTION(early_inject)) {
        USAGE_ERROR("-inject_at_create_process requires -early_inject, setting to "
                    "defaults");
        SET_DEFAULT_VALUE(inject_at_create_process);
        SET_DEFAULT_VALUE(early_inject);
        changed_options = true;
    }
    if (DYNAMO_OPTION(follow_systemwide) && !DYNAMO_OPTION(early_inject) &&
        !IS_STRING_OPTION_EMPTY(block_mod_load_list_default) &&
        !check_filter(DYNAMO_OPTION(block_mod_load_list_default), "dynamorio.dll")) {
        USAGE_ERROR("follow_systemwide is dangerous without -early_inject unless "
                    "-block_mod_load_list[_default] includes dynamorio.dll");
        dynamo_options.follow_systemwide = false;
        changed_options = true;
    }
#        ifndef NOT_DYNAMORIO_CORE /* can't check without get_os_version etc. */
    if (DYNAMO_OPTION(early_inject)) {
        /* using early inject */
        if (DYNAMO_OPTION(early_inject_location) ==
                INJECT_LOCATION_LdrpLoadImportModule ||
            (DYNAMO_OPTION(early_inject_location) == INJECT_LOCATION_LdrDefault &&
             (get_os_version() == WINDOWS_VERSION_NT ||
              get_os_version() == WINDOWS_VERSION_2000))) {
            /* we will be using INJECT_LOCATION_LdrpLoadImportModule for
             * child processes */
            if (!dr_early_injected ||
                dr_early_injected_location != INJECT_LOCATION_LdrpLoadImportModule) {
                /* can't get address from parent */
                /* our method of finding the address relies on
                 * -native_exec_syscalls */
                if (!DYNAMO_OPTION(native_exec_syscalls)) {
                    USAGE_ERROR("early_inject_location LdrpLoadImportModule requires "
                                "-native_exec_syscalls for first process in chain");
                    /* FIXME is this the best remediation choice? */
                    dynamo_options.native_exec_syscalls = true;
                    changed_options = true;
                    /* FIXME - check that helper dlls exist, need a way of
                     * finding systemroot for that. */
                }
            }
        }
    }
    if (DYNAMO_OPTION(early_inject_location) > INJECT_LOCATION_MAX) {
        USAGE_ERROR("invalid value for -early_inject_location, setting default");
        SET_DEFAULT_VALUE(early_inject_location);
        changed_options = true;
    }
    if (DYNAMO_OPTION(early_inject_location) == INJECT_LOCATION_LdrCustom &&
        DYNAMO_OPTION(early_inject_address) == 0) {
        USAGE_ERROR("early_inject_location LdrCustom requires setting "
                    "-early_inject_address");
        SET_DEFAULT_VALUE(early_inject_location);
        changed_options = true;
    }
    if ((DYNAMO_OPTION(follow_children) || DYNAMO_OPTION(follow_systemwide) ||
         DYNAMO_OPTION(follow_explicit_children)) &&
        get_os_version() >= WINDOWS_VERSION_VISTA &&
        !DYNAMO_OPTION(inject_at_create_process) &&
        !DYNAMO_OPTION(vista_inject_at_create_process)) {
        /* We won't follow into child processes.  Won't affect the current
         * proccess so only a warning. */
        SYSLOG_INTERNAL_WARNING("Vista+ requires -vista_inject_at_create_process "
                                "to follow into child processes");
    }
#            ifdef PROCESS_CONTROL
    if (IS_PROCESS_CONTROL_ON() && !DYNAMO_OPTION(follow_systemwide)) {
        /* Process control can happen even in slisted processes, so
         * thin_client need not be true.  To reliably control all processes,
         * dr must exist in all of them, so follow_systemwide and runall must
         * be true.
         */
        USAGE_ERROR("-process_controls needs -follow_systemwide");
        dynamo_options.follow_systemwide = true;
        changed_options = true;

        /* FIXME: assert that the global rununder registry key is set to
         * rununder_all, but how?
         */
    }
    if (IS_PROCESS_CONTROL_ON() && DYNAMO_OPTION(pc_num_hashes) < 100) {
        USAGE_ERROR("-pc_num_hashes must be at least 100 to minimize auto "
                    "shut off of process control");
        dynamo_options.pc_num_hashes = 100;
        changed_options = true;
    }
#            endif /* PROCESS_CONTROL */
    if (DYNAMO_OPTION(thin_client)) {
        /* Note: can't change all these options here because the recursion
         * exceeds the limit, so leaving it to the user to fix it.

         * If thin_client is specified, it will override client, low, and all
         * the options shown below.  The check for client/low check will only
         * fix those options that won't be fixed by the subsequent if, i.e.,
         * vm* options, that is why there is no else if
         */
        /* Is there any option for high/server? */
        if (DYNAMO_OPTION(client) || DYNAMO_OPTION(low)) {
            USAGE_ERROR("-thin_client won't work with -client or -low");
            dynamo_options.client = false;
            dynamo_options.low = false;
            dynamo_options.vm_size = 2 * 1024 * 1024;
            dynamo_options.vm_base = 0;
            dynamo_options.vm_max_offset = 0;
            changed_options = true;
        }
#            ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hot_patching) || DYNAMO_OPTION(hotp_only)) {
            USAGE_ERROR("-thin_client doesn't support hot patching");
            dynamo_options.hot_patching = false;
            dynamo_options.hotp_only = false;
            changed_options = true;
        }
#            endif
#            ifdef GBOP
        if (DYNAMO_OPTION(gbop)) {
            USAGE_ERROR("-thin_client doesn't support gbop");
            dynamo_options.gbop = 0;
            changed_options = true;
        }
#            endif
#            ifdef WINDOWS
        if (DYNAMO_OPTION(aslr)) {
            USAGE_ERROR("-thin_client doesn't support aslr ");
            dynamo_options.aslr = 0;
            changed_options = true;
        }
#            endif
        if (!DYNAMO_OPTION(native_exec_syscalls)) {
            USAGE_ERROR("-thin_client needs -native_exec_syscalls");
            dynamo_options.native_exec_syscalls = true;
            changed_options = true;
        }
#            ifdef KSTATS
        /* Same issue as hotp_only; case 6837. */
        if (DYNAMO_OPTION(kstats)) {
            USAGE_ERROR("-thin_client doesn't support -kstats");
            dynamo_options.kstats = false;
            changed_options = true;
        }
#            endif

        /* FIXME: not tested on vista where ldr_init_thunk is hooked first
         *        and has a different process creation mechanism; case 8576.
         */
    }
#        endif /* !NOT_DYNAMORIO_CORE */
#    endif     /* WINDOWS */

    if (!IS_INTERNAL_STRING_OPTION_EMPTY(client_lib) &&
        !(INTERNAL_OPTION(code_api) ||
          INTERNAL_OPTION(probe_api) IF_PROG_SHEP(|| DYNAMO_OPTION(security_api)))) {
        USAGE_ERROR("-client_lib requires at least one API flag");
    }

    if (DYNAMO_OPTION(coarse_units)) {
        if (DYNAMO_OPTION(bb_prefixes)) {
            /* coarse_units doesn't support prefixes in general.
             * the variation by addr prefix according to processor type
             * is also not stored in pcaches.
             */
            USAGE_ERROR("-coarse_units incompatible with -bb_prefixes: disabling");
            dynamo_options.coarse_units = false;
            changed_options = true;
        }
        if (!DYNAMO_OPTION(shared_bbs)) {
            USAGE_ERROR("-coarse_units requires -shared_bbs, enabling");
            dynamo_options.shared_bbs = true;
            changed_options = true;
        }
        if (DYNAMO_OPTION(inline_bb_ibl)) {
            USAGE_ERROR("-coarse_units not compatible with -inline_bb_ibl, disabling");
            dynamo_options.inline_bb_ibl = false;
            changed_options = true;
        }
        if (DYNAMO_OPTION(bb_ibl_targets) && !DYNAMO_OPTION(disable_traces)) {
            /* case 147/9636: NYI */
            USAGE_ERROR("-coarse_units not compatible with -bb_ibl_targets in "
                        "presence of traces, disabling");
            dynamo_options.bb_ibl_targets = false;
            changed_options = true;
        }
#    ifdef EXPOSE_INTERNAL_OPTIONS
        if (!DYNAMO_OPTION(indirect_stubs)) {
            /* FIXME case 8827: wouldn't be hard to support, just need to ensure the
             * shared use of the ibl fake stubs is properly separated in dispatch
             */
            USAGE_ERROR("case 8827: -coarse_units requires -indirect_stubs, enabling");
            dynamo_options.indirect_stubs = true;
            changed_options = true;
        }
        if (INTERNAL_OPTION(store_translations)) {
            /* FIXME case 9707: NYI */
            USAGE_ERROR("case 9707: -coarse_units does not support -store_translations, "
                        "disabling");
            dynamo_options.store_translations = false;
            changed_options = true;
        }
#    endif
        if (DYNAMO_OPTION(IAT_elide)) {
            /* FIXME case 9710: NYI */
            USAGE_ERROR("case 9710: -coarse_units does not support -IAT_elide, "
                        "disabling");
            dynamo_options.IAT_elide = false;
            changed_options = true;
        }
        if (DYNAMO_OPTION(unsafe_freeze_elide_sole_ubr) &&
            !DYNAMO_OPTION(coarse_freeze_elide_ubr)) {
            USAGE_ERROR("-unsafe_freeze_elide_sole_ubr requires "
                        "-coarse_freeze_elide_ubr, enabling");
            dynamo_options.coarse_freeze_elide_ubr = true;
            changed_options = true;
        }
#    ifdef PROGRAM_SHEPHERDING
        if (DYNAMO_OPTION(coarse_merge_iat) &&
            !DYNAMO_OPTION(executable_if_rx_text)
                IF_WINDOWS(&&!DYNAMO_OPTION(executable_after_load))) {
            /* case 8640: relies on -executable_{if_rx_text,after_load} */
            USAGE_ERROR("-coarse_merge_iat requires "
                        "-executable_{if_rx_text,after_load}; disabling");
            dynamo_options.coarse_merge_iat = false;
            changed_options = true;
        }
#    endif
    }

    if (!DYNAMO_OPTION(persist_per_user) &&
        (DYNAMO_OPTION(validate_owner_dir) || DYNAMO_OPTION(validate_owner_file))) {
        USAGE_ERROR("-no_persist_per_user is insecure\n"
                    "disabling validation, you are on your own!");
        dynamo_options.validate_owner_file = false;
        dynamo_options.validate_owner_dir = false;
        changed_options = true;
    }

#    ifdef DGC_DIAGNOSTICS
    if (INTERNAL_OPTION(mangle_app_seg)) {
        /* i#107: -mangle_app_seg use a fragment flag FRAG_HAS_MOV_SEG
         * that shares the same value with FRAG_DYNGEN_RESTRICTED used
         * in DGC_DIAGNOSTICS, so they cannot be used together.
         */
        USAGE_ERROR("-mangle_app_seg not compatible with DGC_DIAGNOSTICS; "
                    "disabling\n");
        dynamo_options.mangle_app_seg = false;
        changed_options = true;
    }
#    endif

#    ifdef UNIX
#        if (defined(ARM) || defined(LINUX)) && !defined(STATIC_LIBRARY)
    if (!INTERNAL_OPTION(private_loader)) {
        /* On ARM, to make DR work in gdb, we must use private loader to make
         * the TLS format match what gdb wants to see.
         * On Linux, we just don't want the libdl.so dependence for -early.
         */
        if (IF_ARM_ELSE(true, DYNAMO_OPTION(early_inject))) {
            USAGE_ERROR("-private_loader must be true on ARM or on Linux");
            dynamo_options.private_loader = true;
            changed_options = true;
        }
    }
#        endif
    if (INTERNAL_OPTION(private_loader)) {
#        ifdef X86
        if (!INTERNAL_OPTION(mangle_app_seg)) {
            USAGE_ERROR("-private_loader requires -mangle_app_seg");
            dynamo_options.mangle_app_seg = true;
            changed_options = true;
        }
#        endif
        if (INTERNAL_OPTION(client_lib_tls_size) < 1) {
            USAGE_ERROR("client_lib_tls_size is too small, set back to default");
            dynamo_options.client_lib_tls_size = 1;
            changed_options = true;
        }
#        define MAX_NUM_LIB_TLS_PAGES 4
        if (INTERNAL_OPTION(client_lib_tls_size) > MAX_NUM_LIB_TLS_PAGES) {
            USAGE_ERROR("client_lib_tls_size is too big, set to be maximum");
            dynamo_options.client_lib_tls_size = MAX_NUM_LIB_TLS_PAGES;
            changed_options = true;
        }
    }
#    endif

    if (DYNAMO_OPTION(native_exec_opt)) {
#    ifdef WINDOWS
        /* i#1238-c#1: we do not support inline optimization in Windows */
        USAGE_ERROR("-native_exec_opt is not supported in Windows");
        dynamo_options.native_exec_opt = false;
        changed_options = true;
#    endif
#    ifdef KSTATS
        /* i#1238-c#4: we do not support inline optimization with kstats */
        if (DYNAMO_OPTION(kstats)) {
            USAGE_ERROR("-native_exec_opt does not support -kstats");
            dynamo_options.kstats = false;
            changed_options = true;
        }
#    endif
        if (!DYNAMO_OPTION(disable_traces)) {
            USAGE_ERROR("-native_exec_opt does not support traces");
            DISABLE_TRACES((&dynamo_options));
            changed_options = true;
        }
    }

#    ifdef X64
    if (DYNAMO_OPTION(x86_to_x64)) {
        /* i#1494: to avoid decode_fragment messing up the 32-bit/64-bit mode,
         * we do not support any cases of using decode_fragment, including
         * trace and coarse_units (coarse-grain code cache management).
         */
        if (!DYNAMO_OPTION(disable_traces)) {
            USAGE_ERROR("-x86_to_x64 does not support traces");
            DISABLE_TRACES((&dynamo_options));
            changed_options = true;
        }
        if (DYNAMO_OPTION(coarse_units)) {
            USAGE_ERROR("-coarse_units incompatible with -x86_to_x64: disabling");
            DISABLE_COARSE_UNITS((&dynamo_options));
            changed_options = true;
        }
    }
#    endif

#    ifdef ARM
    if (DYNAMO_OPTION(steal_reg) < 8 /* DR_REG_STOLEN_MIN */ ||
        DYNAMO_OPTION(steal_reg) > IF_X64_ELSE(29, 12) /* DR_REG_STOLEN_MAX */) {
        USAGE_ERROR("-steal_reg only supports register between r8 and r12(A32)/r29(A64)");
        dynamo_options.steal_reg = IF_X64_ELSE(28 /*r28*/, 10 /*r10*/);
        changed_options = true;
    }
#    endif

#    ifdef DEBUG
    if (INTERNAL_OPTION(log_at_fragment_count) > 0 && d_r_stats->loglevel > 1) {
        /* start out at 1 */
        if (dynamo_options.stats_loglevel <= 1)
            USAGE_ERROR("-log_at_fragment_count expects >1 delayed loglevel");
        d_r_stats->loglevel = 1;
        changed_options = true;
    }
#    endif

#    ifndef NOT_DYNAMORIO_CORE
    /* fcache param checks rather involved, leave them in fcache.c */
    /* case 7626: don't short-circuit checks, as later ones may be needed */
    changed_options = fcache_check_option_compatibility() || changed_options;
    changed_options = heap_check_option_compatibility() || changed_options;

    changed_options = os_check_option_compatibility() || changed_options;
    disassemble_options_init();
#    endif

    if (changed_options) {
        if (recurse_count > 5) {
            /* prevent infinite loop, should never recurse this many times */
            FATAL_USAGE_ERROR(OPTION_VERIFICATION_RECURSION, 2, get_application_name(),
                              get_application_pid());
        } else {
            check_option_compatibility_helper(recurse_count + 1);
        }
    }
    return !changed_options;
}

/* returns true if changed any options */
static bool
check_option_compatibility()
{
    ASSERT_OWN_OPTIONS_LOCK(true, &options_lock);
    ASSERT(!OPTIONS_PROTECTED());
    return check_option_compatibility_helper(0);
}

/* returns true if changed any options */
static bool
check_dynamic_option_compatibility()
{
    ASSERT_OWN_OPTIONS_LOCK(true, &options_lock);
    /* NOTE : use non-synch form of USAGE_ERROR  in here to avoid
     * infinite recursion */
    return false; /* nothing to check for yet */
}

/* initialize dynamo options */
int
options_init()
{
    int ret = 0, retval;

    /* .lspdata pages start out writable so no unprotect needed here */
    d_r_write_lock(&options_lock);
    ASSERT(sizeof(dynamo_options) == sizeof(options_t));
    /* get dynamo options */
    adjust_defaults_for_page_size(&dynamo_options);
    retval = d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_OPTIONS), d_r_option_string,
                               sizeof(d_r_option_string));
    if (IS_GET_PARAMETER_SUCCESS(retval))
        ret = set_dynamo_options(&dynamo_options, d_r_option_string);
#    ifdef STATIC_LIBRARY
    /* For dynamorio_static, we always enable code_api as it's a pain to set
     * DR runtime options -- unless otherwise requested.
     */
    options_enable_code_api_dependences(&dynamo_options);
#    endif
    check_option_compatibility();
    /* options will be protected when DR init is completed */
    d_r_write_unlock(&options_lock);
    return ret;
}

/* Clean up dynamo option state.  We can't clear/reset actual option values here,
 * as those are used in other exit routines called later.  We have a separate
 * options_detach() for that.
 */
void
options_exit()
{
    DELETE_READWRITE_LOCK(options_lock);
}

/* Reset dynamo options to defaults. */
void
options_detach()
{
    /* We do not use options_make_writable() as locks are already gone at this point. */
    SELF_UNPROTECT_OPTIONS();
    dynamo_options = default_options;
    /* Not worth bothering to re-protect. */
}

/* this function returns holding the options lock */
void
options_make_writable()
{
    ASSERT_DO_NOT_OWN_WRITE_LOCK(true, &options_lock);
    d_r_write_lock(&options_lock);
    SELF_UNPROTECT_OPTIONS();
}

/* assumes caller holds the options lock -- typically by calling
 * options_make_writable() beforehand
 */
void
options_restore_readonly()
{
    ASSERT_OWN_WRITE_LOCK(true, &options_lock);
    SELF_PROTECT_OPTIONS();
    d_r_write_unlock(&options_lock);
}

/* updates dynamic options and returns if any were changed */
int
synchronize_dynamic_options()
{
    int updated, retval;

    if (!dynamo_options.dynamic_options)
        return 0;

    /* dynamic options */
    STATS_INC(option_synchronizations);

    /* make entire sequence atomic, esp since we're using a shared temp structure
     * to save stack space.
     * if we already have the lock, we must be in the middle of an update,
     * so this becomes a nop.
     */
    if (self_owns_write_lock(&options_lock) ||
        /* avoid hangs reporting errors or warnings by using a trylock (xref i#1198) */
        (!dynamo_initialized && options_lock.num_readers > 0)) {
        STATS_INC(option_synchronizations_nop);
        return 0;
    }

    d_r_write_lock(&options_lock);

    /* check again now that we hold write lock in case was modified */
    if (!dynamo_options.dynamic_options) {
        STATS_INC(option_synchronizations_nop);
        d_r_write_unlock(&options_lock);
        return 0;
    }

    /* get options */
    retval = get_parameter_ex(PARAM_STR(DYNAMORIO_VAR_OPTIONS), new_option_string,
                              sizeof(new_option_string), true /*ignore cache*/);
    if (IS_GET_PARAMETER_FAILURE(retval)) {
        STATS_INC(option_synchronizations_nop);
        d_r_write_unlock(&options_lock);
        return 0;
    }

    if (strcmp(d_r_option_string, new_option_string) == 0) {
        STATS_INC(option_synchronizations_nop);
        d_r_write_unlock(&options_lock);
        return 0;
    }

    SELF_UNPROTECT_OPTIONS();
    set_dynamo_options_defaults(&temp_options);
    set_dynamo_options(&temp_options, new_option_string);
    updated = update_dynamic_options(&dynamo_options, &temp_options);
#    if defined(EXPOSE_INTERNAL_OPTIONS) && defined(INTERNAL)
    bool compatibility_fixup =
#    endif
        check_dynamic_option_compatibility();
    /* d_r_option_string holds a copy of the last read registry value */
    strncpy(d_r_option_string, new_option_string,
            BUFFER_SIZE_ELEMENTS(d_r_option_string));
    NULL_TERMINATE_BUFFER(d_r_option_string);
    SELF_PROTECT_OPTIONS();

    LOG(GLOBAL, LOG_ALL, 2, "synchronize_dynamic_options: %s, updated = %d\n",
        new_option_string, updated);

#    ifdef EXPOSE_INTERNAL_OPTIONS
    if (updated) {
        get_dynamo_options_string(&dynamo_options, new_option_string,
                                  sizeof(new_option_string), true);
        SYSLOG_INTERNAL_NO_OPTION_SYNCH(
            SYSLOG_INFORMATION, "Updated options = \"%s\"%s", new_option_string,
            compatibility_fixup ? " after required compatibility fixups!" : "");
        d_r_write_unlock(&options_lock);
    } else
#    endif /* EXPOSE_INTERNAL_OPTIONS */
        d_r_write_unlock(&options_lock);

    return updated;
}

#    ifdef WINDOWS
/* Currently used to get child options to prevent aslr_dr for thin_client
 * processes.  Assumes other processes, though there is nothing wrong with
 * using this to read the current process's options; we just guard against it
 * because it is already done in other places like init and dynamic option
 * update.
 * Return value : Pointer to the global temp_options struct; so don't try to
 *                  free it!
 * Note         : The CALLER IS RESPONSIBLE for unlocking the options_lock
 *                  (write lock held) and shouldn't rely on the returned
 *                  pointer after that.
 */
const options_t *
get_process_options(HANDLE process_handle)
{
    uint err;

    /* Shouldn't be using this for the current process. */
    ASSERT(process_handle != NT_CURRENT_PROCESS && process_handle != NT_CURRENT_THREAD &&
           process_handle != NULL);
    ASSERT(!READWRITE_LOCK_HELD(&options_lock));

    d_r_write_lock(&options_lock);
    SELF_UNPROTECT_OPTIONS();

    /* Making an assumption that the core will be the same for the parent and
     * child if set_dynamo_options_default is to work correctly.  I think it is
     * reasonable.  FIXME: match parent & child cores & then use set default,
     * what otherwise?
     */
    set_dynamo_options_defaults(&temp_options);
    err = get_process_parameter(process_handle, PARAM_STR(DYNAMORIO_VAR_OPTIONS),
                                new_option_string, sizeof(new_option_string));
    /* PR 330860: be sure not to set for this process */
    if (IS_GET_PARAMETER_SUCCESS(err))
        set_dynamo_options_other_process(&temp_options, new_option_string);

    /* FIXME: options_t compatibility check isn't done because that function
     * operates directly on dynamo_options!  As this is currently used only to
     * detect if child is in thin_client we don't have to fix it because no
     * option turns on thin_client and because fixing the compatibility checker
     * so close to 4.2 release isn't a good idea.  Case 9193 tracks this.
     */

    SELF_PROTECT_OPTIONS();

    /* Note: we are delibrately not unlocking options_lock; caller will do it.
     * This is done so as to not expose a lot of the options module
     * functionality outside when having to access another process's options
     * temporarily. */
    return &temp_options;
}
#    endif /* WINDOWS */

/* Function for identifying string type. */
static bool
is_string_type(enum option_type_t type)
{
    return type == OPTION_TYPE_pathstring_t || type == OPTION_TYPE_liststring_t;
}

/* i#771: Allow the client to query all DR runtime options. */
DR_API
bool
dr_get_string_option(const char *option_name, char *buf OUT, size_t len)
{
    bool found = false;
    CLIENT_ASSERT(buf != NULL, "invalid parameter");
    string_option_read_lock();
    int i;
    CLIENT_ASSERT(num_options >= 0, "invalid number of options");
    for (i = 0; i < num_options; ++i) {
        if (is_string_type(option_traits[i].type) &&
            strcmp(option_name, option_traits[i].name) == 0) {
            const void *val = (char *)(&dynamo_options) + option_traits[i].offset;
            CLIENT_ASSERT(val != NULL, "invalid address");
            strncpy(buf, val, len);
            found = true;
            break;
        }
    }
    string_option_read_unlock();
    if (len > 0)
        buf[len - 1] = '\0';
    return found;
}

DR_API
bool
dr_get_integer_option(const char *option_name, uint64 *val OUT)
{
    CLIENT_ASSERT(val != NULL, "invalid parameter");
    *val = 0;
    int i = 0;
    for (i = 0; i < num_options; ++i) {
        if (!is_string_type(option_traits[i].type) &&
            strcmp(option_name, option_traits[i].name) == 0) {
            const void *dopts_ptr = (char *)(&dynamo_options) + option_traits[i].offset;
            memcpy((void *)val, dopts_ptr, option_traits[i].size);
            return true;
        }
    }
    return false;
}

#endif /* NOT_DYNAMORIO_CORE */

#ifdef STANDALONE_UNIT_TEST

static void
show_dynamo_options(bool minimal)
{
    /* Printing all options requires a large buffer.  This is test code, so we
     * can still put this on the stack.
     */
    char opstring[8 * MAX_OPTIONS_STRING];

    get_dynamo_options_string(&dynamo_options, opstring, sizeof(opstring), minimal);
    /* we exceed write_file's internal buffer size */
    os_write(STDERR, opstring, strlen(opstring));
}

/* USAGE Show descriptions of all available options */
static void
show_dynamo_option_descriptions()
{
#    define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                           description, flag, pcache)                                 \
        if (command_line_option[0] != ' ') { /* not synthetic */                      \
            print_file(STDERR, "-%-20s %s\n", command_line_option, description);      \
        }
#    include "optionsx.h"
#    undef OPTION_COMMAND
}

void
unit_test_options(void)
{
    char buf[MAX_OPTIONS_STRING];
    options_t new_options;
    int updated;

    d_r_write_lock(&options_lock); /* simplicity: just grab whole time */
    SELF_UNPROTECT_OPTIONS();

    /* FIXME: actually use asserts for automated testing that does not require
     * visual inspection
     */

    /* FIXME: test invalid options -- w/o dying! */

    print_file(STDERR, "default---\n");
    show_dynamo_options(false);
    print_file(STDERR, "\nbefore first set---\n");
    set_dynamo_options(
        &dynamo_options,
        "-loglevel 1 -logmask 0x10 -block_mod_load_list "
        "'mylib.dll;evilbad.dll;really_long_name_for_a_dll.dll' -stderr_mask 12");
    show_dynamo_options(true);

    print_file(STDERR, "\nbefore second set---\n");
    set_dynamo_options(
        &dynamo_options,
        "-logmask 17 -cache_bb_max 20 -cache_trace_max 20M -svchost_timeout 3m");
    show_dynamo_options(true);

    set_dynamo_options_defaults(&new_options);
    set_dynamo_options(
        &new_options,
        "-logmask 7 -cache_bb_max 20 -cache_trace_max 20M -svchost_timeout 3m");
    updated = update_dynamic_options(&dynamo_options, &new_options);
    print_file(STDERR, "updated %d\n", updated);
    show_dynamo_options(true);

    show_dynamo_option_descriptions();

    get_dynamo_options_string(&dynamo_options, buf, MAXIMUM_PATH, 1);
    print_file(STDERR, "options string: %s\n", buf);

    get_dynamo_options_string(&dynamo_options, buf, MAXIMUM_PATH, 0);
    print_file(STDERR, "full options string: %s\n", buf);

    set_dynamo_options_defaults(&dynamo_options);
    get_dynamo_options_string(&dynamo_options, buf, MAXIMUM_PATH, 1);
    print_file(STDERR, "default ops string: %s\n", buf);

#    ifdef X64
    /* Sanity-check pointer-sized integer values handling >int sizes. */
    set_dynamo_options(&dynamo_options, "-vmheap_size 16384M -persist_short_digest 8K");
    EXPECT_EQ(dynamo_options.vmheap_size, 16 * 1024 * 1024 * 1024ULL);
    char opstring[MAX_OPTIONS_STRING];
    /* Ensure we print it back out with the shortest value+suffix.
     * We include a smaller option to ensure we avoid printing out "0G".
     */
    get_dynamo_options_string(&dynamo_options, opstring, sizeof(opstring), true);
    EXPECT_EQ(0, strcmp(opstring, "-vmheap_size 16G -persist_short_digest 8K "));
#    endif

    SELF_PROTECT_OPTIONS();
    d_r_write_unlock(&options_lock);
}

#endif /* STANDALONE_UNIT_TEST */
