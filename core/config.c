/* **********************************************************
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
 * config.c - platform-independent app config routines
 */

#include "configure.h"

#ifndef PARAMS_IN_REGISTRY /* around whole file */

#include "../globals.h"
#ifdef WINDOWS
# include "ntdll.h"
#endif

#include <string.h>

#ifdef LINUX
# define GLOBAL_CONFIG_DIR "/etc/dynamorio"
# define LOCAL_CONFIG_ENV "HOME"
# define LOCAL_CONFIG_SUBDIR ".dynamorio"
#else
# define LOCAL_CONFIG_ENV "USERPROFILE"
# define LOCAL_CONFIG_SUBDIR "dynamorio"
#endif

/* We use separate file names to support apps with the same name
 * that come in different arch flavors
 */
#define CFG_SFX_64 "config64"
#define CFG_SFX_32 "config32"
#ifdef X64
# define CFG_SFX CFG_SFX_64
#else
# define CFG_SFX CFG_SFX_32
#endif

#if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
/* for linking into linux preload we do use libc for now (xref i#46/PR 206369) */
# undef ASSERT
# undef ASSERT_NOT_IMPLEMENTED
# undef ASSERT_NOT_TESTED
# undef FATAL_USAGE_ERROR
# define ASSERT(x) /* nothing */
# define ASSERT_NOT_IMPLEMENTED(x) /* nothing */
# define ASSERT_NOT_TESTED(x) /* nothing */
# define FATAL_USAGE_ERROR(x, ...) /* nothing */
# define print_file fprintf
# undef STDERR
# define STDERR stderr
# undef our_snprintf
# define our_snprintf snprintf
# undef DECLARE_NEVERPROT_VAR
# define DECLARE_NEVERPROT_VAR(var, val) var = (val)
#endif

/* no logfile is set up yet */
#ifdef DEBUG
# ifdef INTERNAL
#  define VERBOSE 0
# else
#  define VERBOSE 0
# endif
DECLARE_NEVERPROT_VAR(static int infolevel, VERBOSE);
# define INFO(level, fmt, ...) do { \
        if (infolevel >= (level)) print_file(STDERR, "<"fmt">\n", __VA_ARGS__); \
    } while (0);
#else
# define INFO(level, fmt, ...) /* nothing */
#endif

/* we store values for each of these vars: */
static const char *const config_var[] = {
    DYNAMORIO_VAR_HOME,
    DYNAMORIO_VAR_LOGDIR,
    DYNAMORIO_VAR_OPTIONS,
    DYNAMORIO_VAR_AUTOINJECT,
    DYNAMORIO_VAR_UNSUPPORTED,
    DYNAMORIO_VAR_RUNUNDER,
    DYNAMORIO_VAR_CMDLINE,
    DYNAMORIO_VAR_ONCRASH,
    DYNAMORIO_VAR_SAFEMARKER,
    DYNAMORIO_VAR_CACHE_ROOT,
    DYNAMORIO_VAR_CACHE_SHARED,
};
#ifdef WINDOWS
static const wchar_t *const w_config_var[] = {
    L_DYNAMORIO_VAR_HOME,
    L_DYNAMORIO_VAR_LOGDIR,
    L_DYNAMORIO_VAR_OPTIONS,
    L_DYNAMORIO_VAR_AUTOINJECT,
    L_DYNAMORIO_VAR_UNSUPPORTED,
    L_DYNAMORIO_VAR_RUNUNDER,
    L_DYNAMORIO_VAR_CMDLINE,
    L_DYNAMORIO_VAR_ONCRASH,
    L_DYNAMORIO_VAR_SAFEMARKER,
    L_DYNAMORIO_VAR_CACHE_ROOT,
    L_DYNAMORIO_VAR_CACHE_SHARED,
};
#endif
#define NUM_CONFIG_VAR (sizeof(config_var)/sizeof(config_var[0]))

/* we want to read config values prior to setting up heap so all data
 * is static
 */
typedef struct _config_val_t {
    char val[MAX_REGISTRY_PARAMETER];
    /* distinguish set to "" from never set */
    bool has_value;
    /* identify which level: app-specific, default, from env */
    bool app_specific;
    bool from_env;
} config_val_t;

typedef struct _config_vals_t {
    config_val_t vals[NUM_CONFIG_VAR];
    bool has_1config;
} config_vals_t;

typedef struct _config_info_t {
    char fname_app[MAXIMUM_PATH];
    char fname_default[MAXIMUM_PATH];
    /* Two modes: if query != NULL, we search for var with name==query
     * and will fill in answer.  if query == NULL, then we fill in v.
     * Perhaps it would be worth the complexity to move fname_* to
     * config_vals_t and reduce stack space of config_info_t (can't
     * use heap very easily since used by preinject, injector, and DR).
     */
    const char *query;
    union {
        struct {
            config_val_t answer;
            bool have_answer;
        } q;
        config_vals_t *v;
    } u;
} config_info_t;

static config_vals_t myvals;
static config_info_t config;

const char *
my_getenv(IF_WINDOWS_ELSE_NP(const wchar_t *, const char *) var, char *buf, size_t bufsz)
{
#ifdef LINUX
    return getenv(var);
#else
    wchar_t wbuf[MAX_REGISTRY_PARAMETER];
    if (env_get_value(var, wbuf, BUFFER_SIZE_BYTES(wbuf))) {
        NULL_TERMINATE_BUFFER(wbuf);
        snprintf(buf, bufsz, "%ls", wbuf);
        buf[bufsz - 1] ='\0';
        return buf;
    }
    return NULL;
#endif
}

const char *
get_config_val_ex(const char *var, bool *app_specific, bool *from_env)
{
    uint i;
    config_info_t *cfg = &config;
    ASSERT(var != NULL);
    ASSERT(cfg->u.v != NULL);
    /* perf: we could stick the names in a hashtable */
    for (i = 0; i < NUM_CONFIG_VAR; i++) {
        if (strstr(var, config_var[i]) == var) {
            if (cfg->u.v->vals[i].has_value) {
                if (app_specific != NULL)
                    *app_specific = cfg->u.v->vals[i].app_specific;
                if (from_env != NULL)
                    *from_env = cfg->u.v->vals[i].from_env;
                return (const char *) cfg->u.v->vals[i].val;
            } else
                return NULL;
        }
    }
    return NULL;
}

const char *
get_config_val(const char *var)
{
    return get_config_val_ex(var, NULL, NULL);
}

static void
set_config_from_env(config_info_t *cfg)
{
    uint i;
    char buf[MAX_REGISTRY_PARAMETER];
    /* perf: we could stick the names in a hashtable */
    for (i = 0; i < NUM_CONFIG_VAR; i++) {
        /* env vars set any unset vars (lower priority than config file) */
        if (!cfg->u.v->vals[i].has_value) {
            const char *env = my_getenv(IF_WINDOWS_ELSE(w_config_var[i], config_var[i]),
                                        buf, BUFFER_SIZE_BYTES(buf));
            if (env != NULL) {
                strncpy(cfg->u.v->vals[i].val, env,
                        BUFFER_SIZE_ELEMENTS(cfg->u.v->vals[i].val));
                cfg->u.v->vals[i].has_value = true;
                cfg->u.v->vals[i].app_specific = false;
                cfg->u.v->vals[i].from_env = true;
                INFO(1, "setting %s from env: \"%s\"", config_var[i], env);
            }
        }
    }
}
 
static void
process_config_line(char *line, config_info_t *cfg, bool app_specific, bool overwrite)
{
    uint i;
    ASSERT(line != NULL);
    /* perf: we could stick the names in a hashtable */
    for (i = 0; i < NUM_CONFIG_VAR; i++) {
        if (strstr(line, config_var[i]) == line) {
            char *val = strchr(line, '=');
            if (val == NULL ||
                /* we don't have any vars that are prefixes of others so we
                 * can do a hard match on whole var.
                 * for parsing simplicity we don't allow whitespace before '='.
                 */
                val != line + strlen(config_var[i])) {
                if (cfg->query == NULL) { /* only complain about this process */
                    FATAL_USAGE_ERROR(ERROR_CONFIG_FILE_INVALID, 3, line);
                    ASSERT_NOT_REACHED();
                }
            } else if (cfg->query != NULL) {
                strncpy(cfg->u.q.answer.val, val + 1,
                        BUFFER_SIZE_ELEMENTS(cfg->u.q.answer.val));
                cfg->u.q.have_answer = true;
            } else if (!cfg->u.v->vals[i].has_value || overwrite) {
                strncpy(cfg->u.v->vals[i].val, val + 1,
                        BUFFER_SIZE_ELEMENTS(cfg->u.v->vals[i].val));
                cfg->u.v->vals[i].has_value = true;
                cfg->u.v->vals[i].app_specific = app_specific;
                cfg->u.v->vals[i].from_env = false;
                INFO(1, "setting %s from file: \"%s\"", config_var[i],
                     cfg->u.v->vals[i].val);
            }
        }
    }
}

static void
read_config_file(file_t f, config_info_t *cfg, bool app_specific, bool overwrite)
{
    /* we are single-threaded for init, and config_reread() holds the
     * options lock, but get_config_val_other() is called when child
     * processes are created and thus other threads are around: so use
     * a smaller buffer.  these files are pretty small anyway.  but, our
     * buffer needs to hold at least one full line: we assume var name plus
     * '=' plus newline chars < 128.
     */
#   define BUFSIZE (MAX_REGISTRY_PARAMETER+128)
    char buf[BUFSIZE];

    char *line, *newline = NULL;
    int bufread = 0, bufwant;
    int len;

    while (true) {
        /* break file into lines */
        if (newline == NULL) {
            bufwant = BUFSIZE-1;
            bufread = os_read(f, buf, bufwant);
            ASSERT(bufread <= bufwant);
            if (bufread <= 0)
                break;
            buf[bufread] = '\0';
            newline = strchr(buf, '\n');
            line = buf;
        } else {
            line = newline + 1;
            newline = strchr(line, '\n');
            if (newline == NULL) {
                /* shift 1st part of line to start of buf, then read in rest */
                /* the memory for the processed part can be reused  */
                bufwant = line - buf;
                ASSERT(bufwant <= bufread);
                len = bufread - bufwant; /* what is left from last time */
                /* using memmove since strings can overlap */
                if (len > 0)
                    memmove(buf, line, len);
                bufread = os_read(f, buf+len, bufwant);
                ASSERT(bufread <= bufwant);
                if (bufread <= 0)
                    break;
                bufread += len; /* bufread is total in buf */
                buf[bufread] = '\0';
                newline = strchr(buf, '\n');
                line = buf;
            }
        }
        /* buffer is big enough to hold at least one line */
        ASSERT(newline != NULL);
        *newline = '\0';
        /* handle \r\n line endings */
        if (newline > line && *(newline-1) == '\r')
            *(newline-1) = '\0';
        /* now we have one line */
        INFO(3, "config file line: \"%s\"", line);
        /* we support blank lines and comments */
        if (line[0] == '\0' || line[0] == '#')
            continue;
        process_config_line(line, cfg, app_specific, overwrite);
        if (cfg->query != NULL && cfg->u.q.have_answer)
            break;
    }
}

static void
config_read(config_info_t *cfg, const char *appname, const char *sfx)
{
    file_t f_app = INVALID_FILE, f_default = INVALID_FILE;
    const char *local, *global;
    char buf[MAXIMUM_PATH];
    bool check_global = true;
#ifdef WINDOWS
    int retval;
#endif
    ASSERT(cfg->query != NULL || cfg->u.v != NULL);
    /* for now we only support config files by short name: we'll see
     * whether we need to also support full paths
     */
    if (appname == NULL)
        appname = get_application_short_name();
    /* try in precedence order to find a config file.
     * if app-specific exists, default at that level is also read to fill in any
     * unspecified values.
     * env vars are always read and used to fill in any unspecified values.
     * if local exists, global is NOT read.
     */
    local = my_getenv(L_IF_WIN(LOCAL_CONFIG_ENV), buf, BUFFER_SIZE_BYTES(buf));
    if (local != NULL) {
        if (cfg == &config) {
            /* 1) <local>/appname.<pid>.1config
             * only makes sense for main config for this process
             */
            snprintf(cfg->fname_app, BUFFER_SIZE_ELEMENTS(cfg->fname_app),
                     "%s/%s/%s.%d.1%s",
                     local, LOCAL_CONFIG_SUBDIR, appname, get_process_id(), sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_app);
            INFO(2, "trying config file %s", cfg->fname_app);
            f_app = os_open(cfg->fname_app, OS_OPEN_READ);
            if (f_app != INVALID_FILE && cfg->query == NULL)
                cfg->u.v->has_1config = true; /* one-time file */
        }
        /* 2) <local>/appname.config */
        if (f_app == INVALID_FILE) {
            snprintf(cfg->fname_app, BUFFER_SIZE_ELEMENTS(cfg->fname_app), "%s/%s/%s.%s",
                     local, LOCAL_CONFIG_SUBDIR, appname, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_app);
            INFO(2, "trying config file %s", cfg->fname_app);
            f_app = os_open(cfg->fname_app, OS_OPEN_READ);
        }
        /* 3) <local>/default.0config */
        if (f_default == INVALID_FILE) {
            snprintf(cfg->fname_default, BUFFER_SIZE_ELEMENTS(cfg->fname_default),
                     "%s/%s/default.0%s",
                     local, LOCAL_CONFIG_SUBDIR, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_default);
            INFO(2, "trying config file %s", cfg->fname_default);
            f_default = os_open(cfg->fname_default, OS_OPEN_READ);
        }
    }
#ifdef WINDOWS
    /* on Windows the global dir is <installbase>/config/ */
    retval = get_parameter_from_registry(L_DYNAMORIO_VAR_HOME,
                                         buf, BUFFER_SIZE_BYTES(buf));
    NULL_TERMINATE_BUFFER(buf);
    check_global = IS_GET_PARAMETER_SUCCESS(retval);
    global = buf;
#else
    global = GLOBAL_CONFIG_DIR;
#endif
    if (check_global) {
        /* 4) <global>/appname.config */
        if (f_app == INVALID_FILE) {
            snprintf(cfg->fname_app, BUFFER_SIZE_ELEMENTS(cfg->fname_app), 
                     "%s/"IF_WINDOWS("config/")"%s.%s",
                     global, appname, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_app);
            INFO(2, "trying config file %s", cfg->fname_app);
            f_app = os_open(cfg->fname_app, OS_OPEN_READ);
        }
        /* 5) <global>/default.0config */
        if (f_default == INVALID_FILE) {
            snprintf(cfg->fname_default, BUFFER_SIZE_ELEMENTS(cfg->fname_default),
                     "%s/default.0%s", global, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_default);
            INFO(2, "trying config file %s", cfg->fname_default);
            f_default = os_open(cfg->fname_default, OS_OPEN_READ);
        }
    }
    if (f_app != INVALID_FILE) {
        INFO(1, "reading app config file %s", cfg->fname_app);
        read_config_file(f_app, cfg, true, false);
        os_close(f_app);
    } else
        INFO(1, "WARNING: no app config file found%s", "");
    if (f_default != INVALID_FILE) {
        INFO(1, "reading default config file %s", cfg->fname_default);
        read_config_file(f_default, cfg, false, false);
        os_close(f_default);
    } else
        INFO(1, "no default config file found%s", "");
    /* 6) env vars fill in any still-unset values */
    set_config_from_env(cfg);
}

/* up to caller to synchronize */
/* no support for otherarch */
void
config_reread(void)
{
    file_t f;
    if (config.fname_app[0] != '\0') {
        f = os_open(config.fname_app, OS_OPEN_READ);
        if (f != INVALID_FILE) {
            INFO(3, "re-reading app config file %s", config.fname_app);
            DODEBUG({ infolevel -= 2; });
            read_config_file(f, &config, true, true);
            DODEBUG({ infolevel += 2; });
            os_close(f);
        } else
            INFO(1, "WARNING: unable to re-read config file %s", config.fname_app);
    }
    if (config.fname_default[0] != '\0') {
        f = os_open(config.fname_default, OS_OPEN_READ);
        if (f != INVALID_FILE) {
            INFO(3, "re-reading default config file %s", config.fname_default);
            DODEBUG({ infolevel -= 2; });
            read_config_file(f, &config, true, true);
            DODEBUG({ infolevel += 2; });
            os_close(f);
        } else
            INFO(1, "WARNING: unable to re-read config file %s", config.fname_default);
    }
    /* 6) env vars fill in any still-unset values */
    set_config_from_env(&config);
}

/* Since querying for other arch or other app typically asks just about
 * one var in isolation, and is rare, rather than reading in the whole
 * thing and requiring a 6K struct we read the files looking for just
 * the var in question.
 */
static bool
get_config_val_other(const char *appname, const char *sfx,
                     const char *var, char *val, size_t valsz)
{
    /* Can't use heap very easily since used by preinject, injector, and DR
     * so we use a stack var.  WARNING: this is about 1K!
     */
    config_info_t info;
    memset(&info, 0, sizeof(info));
    info.query = var;
    config_read(&info, appname, sfx);
    if (info.u.q.have_answer) {
        if (valsz > BUFFER_SIZE_ELEMENTS(info.u.q.answer.val))
            valsz = BUFFER_SIZE_ELEMENTS(info.u.q.answer.val);
        strncpy(val, info.u.q.answer.val, valsz);
        val[valsz-1] = '\0';
    }
    return info.u.q.have_answer;
}

bool
get_config_val_other_app(const char *appname, const char *var, char *val, size_t valsz)
{
    return get_config_val_other(appname, CFG_SFX, var, val, valsz);
}

bool
get_config_val_other_arch(const char *var, char *val, size_t valsz)
{
    return get_config_val_other(NULL, IF_X64_ELSE(CFG_SFX_32, CFG_SFX_64),
                                var, val, valsz);
}

void
config_init(void)
{
    config.u.v = &myvals;
    config_read(&config, NULL, CFG_SFX);
}

void
config_exit(void)
{
#ifndef NOT_DYNAMORIO_CORE_PROPER
    /* if core, then we're done with 1-time config file.
     * we can't delete this up front b/c we want to support synch ops
     */
    if (config.u.v->has_1config) {
        INFO(2, "deleting config file %s", config.fname_app);
        os_delete_file(config.fname_app);
    }
    /* we ignore otherarch having 1config */
#endif
}

/* Our parameters (option string, logdir, etc.) can be configured
 * through files or environment variables.
 * For the old registry-based scheme, define PARAMS_IN_REGISTRY.
 * value is a buffer allocated by the caller to hold the
 * resulting value.
 */
int
get_parameter_ex(const char *name, char *value, int maxlen, bool ignore_cache)
{
    const char *val;
    if (ignore_cache)
        config_reread();
    val = get_config_val(name);
    /* env var has top priority, then registry */
    if (val != NULL) {
        strncpy(value, val, maxlen-1);
        value[maxlen-1]  = '\0'; /* if max no null */
        /* we do not return GET_PARAMETER_NOAPPSPECIFIC like PARAMS_IN_REGISTRY
         * does: caller should use get_config_val_ex instead
         */
        return GET_PARAMETER_SUCCESS;
    } 
    return GET_PARAMETER_FAILURE;
}

int
get_parameter(const char *name, char *value, int maxlen)
{
    return get_parameter_ex(name, value, maxlen, false);
}

int
get_unqualified_parameter(const char *name, char *value, int maxlen)
{
    /* we don't use qualified names w/ our config files yet */
    return get_parameter(name, value, maxlen);
}

#else /* !PARAMS_IN_REGISTRY around whole file */

void
config_init(void)
{
}

void
config_exit(void)
{
}

#endif /* !PARAMS_IN_REGISTRY around whole file */
