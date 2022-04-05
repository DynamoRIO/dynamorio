/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 VMware, Inc.  All rights reserved.
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

/*
 * config.h - platform-independent app config routines
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_ 1

#include "dr_config.h" /* for dr_platform_t */

void
d_r_config_init(void);

void
d_r_config_exit(void);

bool
d_r_config_initialized(void);

void
config_heap_init(void);

void
config_heap_exit(void);

/* up to caller to synchronize */
void
config_reread(void);

const char *
get_config_val(const char *var);

const char *
get_config_val_ex(const char *var, bool *app_specific, bool *from_env);

bool
get_config_val_other_app(const char *appname, process_id_t pid, dr_platform_t platform,
                         const char *var, char *val, size_t valsz, bool *app_specific,
                         bool *from_env, bool *from_1config);

bool
get_config_val_other_arch(const char *var, char *val, size_t valsz, bool *app_specific,
                          bool *from_env, bool *from_1config);

/**************************************************/
#ifdef PARAMS_IN_REGISTRY

#    define PARAM_STR(name) L_IF_WIN(name)
/* redeclared in inject_shared.h */
int
d_r_get_parameter(const wchar_t *name, char *value, int maxlen);

int
get_parameter_ex(const wchar_t *name, char *value, int maxlen, bool ignore_cache);

/**************************************************/
#else

#    define PARAM_STR(name) name

int
d_r_get_parameter(const char *name, char *value, int maxlen);

int
get_parameter_ex(const char *name, char *value, int maxlen, bool ignore_cache);

int
get_unqualified_parameter(const char *name, char *value, int maxlen);

#    ifdef UNIX
bool
should_inject_from_rununder(const char *runstr, bool app_specific, bool from_env,
                            bool *rununder_on OUT);
#    endif

#endif /* PARAMS_IN_REGISTRY */
/**************************************************/

#endif /* _CONFIG_H_ */
