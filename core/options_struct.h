/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2009 VMware, Inc.  All rights reserved.
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

/* options_struct.h: options struct.  This is separated out from options.h as it
 * is required for arch/atomic_exports.h to use ASSERT which references dynamo_options.
 */

#ifndef _OPTIONS_STRUCT_H_
#define _OPTIONS_STRUCT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Security policy flags.  Note that if the off flag is not explicitly
 * used in an option default value definition, then the option doesn't
 * support that flag.
 */
typedef enum {
    /* security mechanism needed for detection can be completely disabled */
    OPTION_ENABLED = 0x1,  /* if set, security mechanism is on */
    OPTION_DISABLED = 0x0, /* if not set, security mechanism is off  */

    /* security policy can be built on top.  For stateless policies
     * all flags can be dynamic options.  For stateful security mechanisms as
     * long as OPTION_ENABLED stays on for gathering information, the
     * security policies OPTION_BLOCK|OPTION_REPORT can be dynamically
     * set.  If no detection action is set, a detection check may be skipped.
     */

    /* associated security policy can be either blocking or not */
    /* If set, app action (i.e., bad behavior) is disallowed and remediation
     * action (kill thread, kill process, throw exception) is performed.
     * Note: detect_mode will override this flag.
     * FIXME: app_thread_policy_helper seems to be the one place where
     * detect_mode doesn't override this; case 9088 tracks this; xref case 8451
     * for an explanation of why this seemed to leave as such.
     */
    OPTION_BLOCK = 0x2,
    OPTION_NO_BLOCK = 0x0,

    /* policies that lend themselves to standard attack handling may use this flag */
    OPTION_HANDLING = 0x4,    /* if set, overrides default attack handling */
    OPTION_NO_HANDLING = 0x0, /* when not set, default attack handling is used */

    /* report or stay silent */
    OPTION_REPORT = 0x8,    /* if set, report action is being taken */
    OPTION_NO_REPORT = 0x0, /* if not set, action is taken silently */

    /* Block ignoring detect_mode; to handle case 10610. */
    OPTION_BLOCK_IGNORE_DETECT = 0x20,

    /* modifications in security policy or detection mechanism are controlled with */
    OPTION_CUSTOM = 0x100,  /* alternative policy bit - custom meaning per option */
    OPTION_NO_CUSTOM = 0x0, /* alternative policy bit - custom meaning per option */
} security_option_t;

/* values taken by the option hook_conflict */
enum {
    /* these are mutually exclusive */
    HOOKED_TRAMPOLINE_DIE = 0,         /* throw a fatal error if chained */
    HOOKED_TRAMPOLINE_SQUASH = 1,      /* modify any existing chains with a good guess */
    HOOKED_TRAMPOLINE_CHAIN = 2,       /* rerelativize and mangle to support chaining */
    HOOKED_TRAMPOLINE_HOOK_DEEPER = 3, /* move our hook deeper into the function */
    HOOKED_TRAMPOLINE_NO_HOOK = 4,     /* give up on adding our hook */
    HOOKED_TRAMPOLINE_MAX = 4,
};

/* for options.appfault_mask */
enum {
    /* XXX: we don't raise on handled signals b/c nobody would want notification
     * on timer signals.  Should we raise on other handled signals?
     */
    APPFAULT_FAULT = 0x0001, /* Unhandled signal, or any Windows exception */
    APPFAULT_CRASH = 0x0002, /* Unhandled signal or exception (NYI on Windows) */
};

/* for all option uses */
#define uint_size ptr_uint_t
#define uint_time uint
/* So far all addr_t are external so we don't have a 64-bit problem */
#define uint_addr ptr_uint_t
/* XXX: For signed integer options, we'll need to correctly sign-extend in
 * dr_get_integer_option.
 */

/* to dispatch on string default values, kept in struct not enum */
#define ISSTRING_bool 0
#define ISSTRING_uint 0
#define ISSTRING_uint_size 0
#define ISSTRING_uint_time 0
#define ISSTRING_ptr_uint_t 0
#define ISSTRING_pathstring_t 1
#define ISSTRING_liststring_t 1

/* Does this option affect persistent cache formation? */
typedef enum {
    OP_PCACHE_NOP = 0,    /* No effect on pcaches */
    OP_PCACHE_LOCAL = 1,  /* Can only relax (not tighten), and when it relaxes any
                           * module that module is marked via
                           * os_module_set_flag(MODULE_WAS_EXEMPTED).
                           */
    OP_PCACHE_GLOBAL = 2, /* Affects pcaches but not called out as local. */
} op_pcache_t;

/* We use an enum for default values of non-string options so that the compiler
 * will fold them away (cl, at least, won't do it even for struct fields
 * declared const), and also to determine whether options are internal or take
 * string values or affect peristent caches.
 * In order for this to work optionsx.h strings must either be EMPTY_STRING or
 * a string in the OPTION_STRING() macro.
 * N.B.: for 64-bit presumably every enum value is 64-bit-wide b/c some
 * of them are.  FIXME: could split up the enums to save space: or are we
 * never actually storing these values?
 */
#define OPTION_STRING(x) 0 /* no string in enum */
#define EMPTY_STRING 0     /* no string in enum */
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    OPTION_IS_INTERNAL_##name = false, OPTION_IS_STRING_##name = ISSTRING_##type, \
    OPTION_AFFECTS_PCACHE_##name = pcache,
#define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option,  \
                                statement, description, flag, pcache)            \
    OPTION_IS_INTERNAL_##name = true, OPTION_IS_STRING_##name = ISSTRING_##type, \
    OPTION_AFFECTS_PCACHE_##name = pcache,
enum option_is_internal {
#include "optionsx.h"
};
/* We have to make the default values not part of the enum since MSVC uses
 * "long" as the enum type and we'd have truncation of large values.
 * Instead we have to declare constant variables.
 * Hopefully the compiler will treat as constants and optimize away any
 * memory references.
 */
#undef OPTION_COMMAND
#undef OPTION_COMMAND_INTERNAL
#define liststring_t int
#define pathstring_t int
#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    extern const type OPTION_DEFAULT_VALUE_##name;
#define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                statement, description, flag, pcache)           \
    extern const type OPTION_DEFAULT_VALUE_##name;
#include "optionsx.h"
#undef liststring_t
#undef pathstring_t
#undef OPTION_COMMAND
#undef OPTION_COMMAND_INTERNAL
#undef OPTION_STRING
#undef EMPTY_STRING

/* for all other option uses */
#define OPTION_STRING(x) x
#define EMPTY_STRING \
    {                \
        0            \
    } /* fills the constant char structure with zeroes */

/* the Option struct typedef */
#ifdef EXPOSE_INTERNAL_OPTIONS
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache)           \
        type name;
#else
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache) /* nothing */
#endif

#define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                       description, flag, pcache)                                 \
    type name;
typedef struct _options_t {
#include "optionsx.h"
} options_t;

#undef OPTION_COMMAND
#undef OPTION_COMMAND_INTERNAL

#ifndef EXPOSE_INTERNAL_OPTIONS
/* special struct for internal option default values */
#    define OPTION_COMMAND(type, name, default_value, command_line_option, statement, \
                           description, flag, pcache) /* nothing */
#    define OPTION_COMMAND_INTERNAL(type, name, default_value, command_line_option, \
                                    statement, description, flag, pcache)           \
        const type name;
typedef struct _internal_options_t {
#    include "optionsx.h"
} internal_options_t;
#    undef OPTION_COMMAND
#    undef OPTION_COMMAND_INTERNAL
#endif

#undef uint_size
#undef uint_time
#undef uint_addr

extern options_t dynamo_options;

#ifdef __cplusplus
}
#endif

#endif /* _OPTIONS_STRUCT_H_ */
