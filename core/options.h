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

/*
 * options.h
 *
 * options struct and prototypes from options.c
 * for use with core and gui/nm
 */

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "options_struct.h"

/* For default integer values we use an enum, while for default string values we
 * use the default_options struct instance that is already being used
 * for option parsing, and a second one for internal options when !INTERNAL
 */
extern const options_t default_options;

#ifndef EXPOSE_INTERNAL_OPTIONS
/* only needs to contain string options, but no compile-time way to
 * do that without having OPTION_COMMAND_INTERNAL_STRING()!
 */
extern const internal_options_t default_internal_options;
#endif

#define IS_OPTION_INTERNAL(name) (OPTION_IS_INTERNAL_##name)
#define IS_OPTION_STRING(name) (OPTION_IS_STRING_##name)
/* FIXME : figure out a way to handle types here so that we don't have to cast
 * strings to ints to avoid compiler warnings */
#define DEFAULT_OPTION_VALUE(name) \
    (IS_OPTION_STRING(name) ? (int)default_options.name : OPTION_DEFAULT_VALUE_##name)
#ifdef EXPOSE_INTERNAL_OPTIONS
#    define DEFAULT_INTERNAL_OPTION_VALUE DEFAULT_OPTION_VALUE
#else
#    define DEFAULT_INTERNAL_OPTION_VALUE(name)                 \
        (IS_OPTION_STRING(name) ? default_internal_options.name \
                                : OPTION_DEFAULT_VALUE_##name)
#endif
/* for non-internal builds we don't support setting the default value
 * since they are all constants
 */
#define SET_DEFAULT_VALUE(name) (dynamo_options.name = DEFAULT_OPTION_VALUE(name))

/* checks for incompatible option values */
/* max==0 means no max and 0 is an ok value */
/* if option is incompatible, will try to touch up the option
 * by assigning min to make it valid returns true if changed the
 * option value
 */
bool
check_param_bounds(ptr_uint_t *val, ptr_uint_t min, ptr_uint_t max, const char *name);

int
options_init(void);

/* Only frees a lock: does not destroy any options info other exit routines
 * might need (due to orderings during exit). See options_detach for resetting
 * options back to defaults for static re-attach.
 */
void
options_exit(void);

/* Resets options to default values for the purpose of a full cleanup
 * during static detach. It is called after nearly all other cleanup has
 * occurred.
 */
void
options_detach(void);

int
synchronize_dynamic_options(void);

#ifdef WINDOWS
const options_t *
get_process_options(HANDLE process_handle);
#endif

/*
 * if minimal then the options string only contains values different than
 *  the defaults, otherwise it explicitly lists all options being used
 */
void
get_dynamo_options_string(options_t *options, char *opstr, int len, bool minimal);

/* Fills opstr with a minimal string of only
 * persistent-cache-affecting options whose effect is >= pcache_effect
 * and that are different from the defaults.
 */
void
get_pcache_dynamo_options_string(options_t *options, char *opstr, int len,
                                 op_pcache_t pcache_effect);

bool
has_pcache_dynamo_options(options_t *options, op_pcache_t pcache_effect);

char *
d_r_parse_word(const char *str, const char **strpos, char *wordbuf, uint wordbuflen);

void
options_enable_code_api_dependences(options_t *options);

/****************************************************************************/
#ifdef NOT_DYNAMORIO_CORE
void
set_dynamo_options_defaults(options_t *options);
int
set_dynamo_options(options_t *options, const char *optstr);
#else /* !NOT_DYNAMORIO_CORE */

#    include "utils.h"

/* are any fragments (potentially) shared? */
#    define SHARED_FRAGMENTS_ENABLED() \
        (DYNAMO_OPTION(shared_bbs) || DYNAMO_OPTION(shared_traces))

/* PR 244737: for x64 we use a "unified" scheme were thread-shared and
 * thread-private code always uses TLS for scratch space, eliminating
 * reachability issues.  We require -private_ib_in_tls for x64 as well
 * as for SHARED_FRAGMENTS_ENABLED and use that option to also
 * cover non-ib scratch space.
 */
#    define SCRATCH_ALWAYS_TLS() (DYNAMO_OPTION(private_ib_in_tls))

/* are any traces (potentially) private? */
/* FIXME Fix this if we permit private & shared traces to co-exist */
#    define PRIVATE_TRACES_ENABLED() \
        (!DYNAMO_OPTION(disable_traces) && !DYNAMO_OPTION(shared_traces))

/* are shared BBs ibl targets? */
#    define SHARED_BB_IB_TARGETS() \
        (DYNAMO_OPTION(shared_bbs) && DYNAMO_OPTION(bb_ibl_targets))

/* What this answers is
 * "Are only shared BBs being created and are they valid IB targets?"
 * It's used when a client wants to know if shared BBs are the only
 * possible IB targets. So, for example, this is false whenever trace
 * building is active, even if traces are not being added to the
 * lookup tables.
 */
#    define SHARED_BB_ONLY_IB_TARGETS() \
        (SHARED_BB_IB_TARGETS() && DYNAMO_OPTION(disable_traces))

/* are any shared fragments ibl targets? */
#    define SHARED_IB_TARGETS() (DYNAMO_OPTION(shared_traces) || SHARED_BB_IB_TARGETS())

/* are any IBT tables (potentially) shared? */
#    define SHARED_IBT_TABLES_ENABLED() \
        (DYNAMO_OPTION(shared_bb_ibt_tables) || DYNAMO_OPTION(shared_trace_ibt_tables))

#    define TRACEDUMP_ENABLED()                                                   \
        (!DYNAMO_OPTION(disable_traces) &&                                        \
         (INTERNAL_OPTION(tracedump_text) || INTERNAL_OPTION(tracedump_binary) || \
          INTERNAL_OPTION(tracedump_origins)))

#    define RUNNING_WITHOUT_CODE_CACHE() \
        (IF_HOTP(DYNAMO_OPTION(hotp_only) ||) DYNAMO_OPTION(thin_client))

#    ifndef NOT_DYNAMORIO_CORE_PROPER
#        define CLIENT_OR_STANDALONE() (standalone_library || CLIENTS_EXIST())
#    else
#        define CLIENT_OR_STANDALONE() false
#    endif

extern char d_r_option_string[];
extern read_write_lock_t options_lock;

/* check for empty is considered safe w/o the read lock
 * this takes the field name only, and not {DYNAMO,INTERNAL}_OPTION macro,
 * since those macros will ASSERT_OWN_READWRITE_LOCK(<is_stringtype>, &options_lock)
 */
#    define IS_STRING_OPTION_EMPTY(op) ((dynamo_options.op)[0] == '\0')

/* single character check for ALL is considered safe w/o the read lock
 * similarly to IS_STRING_OPTION_EMPTY see above
 */
#    define IS_LISTSTRING_OPTION_FORALL(op) ((dynamo_options.op)[0] == '*')

#    ifdef EXPOSE_INTERNAL_OPTIONS
#        define IS_INTERNAL_STRING_OPTION_EMPTY(op) IS_STRING_OPTION_EMPTY(op)
#    else
#        define IS_INTERNAL_STRING_OPTION_EMPTY(op) \
            ((default_internal_options.op)[0] == '\0')
#    endif

#    ifdef STATIC_LIBRARY
/* For our static model, we enable -code_api and assume that client code could
 * be run at any time, even if there's no dr_init.
 */
#        define CLIENTS_EXIST() true
#    else
#        define CLIENTS_EXIST() (!IS_INTERNAL_STRING_OPTION_EMPTY(client_lib))
#    endif

/* 0=ret => 1, 1=call* => 2, 2=jmp* => 4 */
#    define COARSE_FILL_IBL_MASK(branch_type) (1 << (branch_type))

/* full access to string requires read lock */
static inline void
string_option_read_lock()
{
    d_r_read_lock(&options_lock);
}
static inline void
string_option_read_unlock()
{
    d_r_read_unlock(&options_lock);
}

typedef enum {
    LIST_NO_MATCH = 0, /* ensure can use as bool */
    LIST_ON_DEFAULT = 1,
    LIST_ON_APPEND = 2,
} list_default_or_append_t;

/* compare short_name, usually module name, against a list option of the combined
 * default option (that could be overridden) and append list that is usually used
 */
list_default_or_append_t
check_list_default_and_append(liststring_t default_list, liststring_t append_list,
                              const char *short_name);

void
options_make_writable(void);

void
options_restore_readonly(void);

#endif /* !NOT_DYNAMORIO_CORE */

#ifdef __cplusplus
}
#endif

#endif /* _OPTIONS_H_ */
