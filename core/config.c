/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#    include "globals.h"
#    include "heap.h"
#    ifdef WINDOWS
#        include "ntdll.h"
#    endif

/* DYNAMORIO_VAR_CONFIGDIR is searched first, and then these: */
#    ifdef UNIX
#        define GLOBAL_CONFIG_DIR "/etc/dynamorio"
#        define LOCAL_CONFIG_ENV "HOME"
#        define LOCAL_CONFIG_SUBDIR ".dynamorio"
#        define GLOBAL_CONFIG_SUBDIR ""
#    else
#        define LOCAL_CONFIG_ENV "USERPROFILE"
#        define LOCAL_CONFIG_SUBDIR "dynamorio"
#        define GLOBAL_CONFIG_SUBDIR "/config"
#    endif

/* We use separate file names to support apps with the same name
 * that come in different arch flavors
 */
#    define CFG_SFX_64 "config64"
#    define CFG_SFX_32 "config32"
#    ifdef X64
#        define CFG_SFX CFG_SFX_64
#    else
#        define CFG_SFX CFG_SFX_32
#    endif

/* no logfile is set up yet */
#    if defined(DEBUG) && defined(INTERNAL)
#        define VERBOSE 0
#    else
#        define VERBOSE 0
#    endif

#    if defined(NOT_DYNAMORIO_CORE) || defined(NOT_DYNAMORIO_CORE_PROPER)
/* for linking into linux preload we do use libc for now (xref i#46/PR 206369) */
#        undef ASSERT
#        undef ASSERT_NOT_IMPLEMENTED
#        undef ASSERT_NOT_TESTED
#        undef ASSERT_NOT_REACHED
#        undef FATAL_USAGE_ERROR
#        undef USAGE_ERROR
#        define ASSERT(x)                 /* nothing */
#        define ASSERT_NOT_IMPLEMENTED(x) /* nothing */
#        define ASSERT_NOT_TESTED(x)      /* nothing */
#        define ASSERT_NOT_REACHED()      /* nothing */
#        define FATAL_USAGE_ERROR(x, ...) /* nothing */
#        define USAGE_ERROR(x, ...)       /* nothing */
#        ifdef WINDOWS
#            if VERBOSE
extern void
display_verbose_message(char *format, ...);
#                define print_file(f, ...) display_verbose_message(__VA_ARGS__)
#            else
#                define print_file(...) /* nothing */
#            endif
#        else
#            define print_file(...) fprintf(__VA_ARGS__)
#        endif
#        undef STDERR
#        define STDERR stderr
#        undef d_r_snprintf
#        ifdef WINDOWS
#            define d_r_snprintf _snprintf
#        else
#            define d_r_snprintf snprintf
#        endif
#        undef DECLARE_NEVERPROT_VAR
#        define DECLARE_NEVERPROT_VAR(var, val) var = (val)
#    endif

#    ifdef DEBUG
DECLARE_NEVERPROT_VAR(static int infolevel, VERBOSE);
#        define INFO(level, fmt, ...)                               \
            do {                                                    \
                if (infolevel >= (level))                           \
                    print_file(STDERR, "<" fmt ">\n", __VA_ARGS__); \
            } while (0);
#    else
#        define INFO(level, fmt, ...) /* nothing */
#    endif

/* we store values for each of these vars: */
static const char *const config_var[] = {
    DYNAMORIO_VAR_HOME,       DYNAMORIO_VAR_LOGDIR,     DYNAMORIO_VAR_OPTIONS,
    DYNAMORIO_VAR_AUTOINJECT, DYNAMORIO_VAR_ALTINJECT,  DYNAMORIO_VAR_UNSUPPORTED,
    DYNAMORIO_VAR_RUNUNDER,   DYNAMORIO_VAR_CMDLINE,    DYNAMORIO_VAR_ONCRASH,
    DYNAMORIO_VAR_SAFEMARKER, DYNAMORIO_VAR_CACHE_ROOT, DYNAMORIO_VAR_CACHE_SHARED,
};
#    ifdef WINDOWS
static const wchar_t *const w_config_var[] = {
    L_DYNAMORIO_VAR_HOME,       L_DYNAMORIO_VAR_LOGDIR,     L_DYNAMORIO_VAR_OPTIONS,
    L_DYNAMORIO_VAR_AUTOINJECT, L_DYNAMORIO_VAR_ALTINJECT,  L_DYNAMORIO_VAR_UNSUPPORTED,
    L_DYNAMORIO_VAR_RUNUNDER,   L_DYNAMORIO_VAR_CMDLINE,    L_DYNAMORIO_VAR_ONCRASH,
    L_DYNAMORIO_VAR_SAFEMARKER, L_DYNAMORIO_VAR_CACHE_ROOT, L_DYNAMORIO_VAR_CACHE_SHARED,
};
#    endif
#    define NUM_CONFIG_VAR (sizeof(config_var) / sizeof(config_var[0]))

/* we want to read config values prior to setting up heap so all data
 * is static
 */
typedef struct _config_val_t {
    char val[MAX_CONFIG_VALUE];
    /* distinguish set to "" from never set */
    bool has_value;
    /* identify which level: app-specific, default, from env */
    bool app_specific;
    bool from_env;
} config_val_t;

typedef struct _config_vals_t {
    config_val_t vals[NUM_CONFIG_VAR];
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
    bool has_1config;
} config_info_t;

static config_vals_t myvals;
static config_info_t config;
static bool config_initialized;

#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
/* i#521: Re-reading the config takes long enough that we can't leave the data
 * section unprotected while we do it.  We initialize this pointer to a heap
 * allocated config_vals_t struct and use that for doing re-reads.
 */
static config_info_t *config_reread_info;
static config_vals_t *config_reread_vals;
#    endif /* !NOT_DYNAMORIO_CORE && !NOT_DYNAMORIO_CORE_PROPER */

const char *
my_getenv(IF_WINDOWS_ELSE_NP(const wchar_t *, const char *) var, char *buf, size_t bufsz)
{
#    ifdef UNIX
    return getenv(var);
#    else
    wchar_t wbuf[MAX_CONFIG_VALUE];
    if (env_get_value(var, wbuf, BUFFER_SIZE_BYTES(wbuf))) {
        NULL_TERMINATE_BUFFER(wbuf);
        snprintf(buf, bufsz, "%ls", wbuf);
        buf[bufsz - 1] = '\0';
        return buf;
    }
    return NULL;
#    endif
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
                return (const char *)cfg->u.v->vals[i].val;
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
    char buf[MAX_CONFIG_VALUE];
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
    if (cfg->query != NULL) {
        if (strstr(line, cfg->query) == line) {
            char *val = strchr(line, '=');
            if (val != NULL &&
                /* see comment below */
                val == line + strlen(cfg->query)) {
                if (strlen(val + 1) >= BUFFER_SIZE_ELEMENTS(cfg->u.q.answer.val)) {
                    /* not FATAL so release build will continue */
                    USAGE_ERROR("Config value for %s too long: truncating", cfg->query);
                }
                strncpy(cfg->u.q.answer.val, val + 1,
                        BUFFER_SIZE_ELEMENTS(cfg->u.q.answer.val));
                cfg->u.q.answer.app_specific = app_specific;
                cfg->u.q.answer.from_env = false;
                cfg->u.q.have_answer = true;
            }
        }
        return;
    }
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
                    FATAL_USAGE_ERROR(ERROR_CONFIG_FILE_INVALID, 3,
                                      get_application_name(), get_application_pid(),
                                      line);
                    ASSERT_NOT_REACHED();
                }
            } else if (!cfg->u.v->vals[i].has_value || overwrite) {
                if (strlen(val + 1) >= BUFFER_SIZE_ELEMENTS(cfg->u.v->vals[i].val)) {
                    /* not FATAL so release build will continue */
                    USAGE_ERROR("Config value for %s too long: truncating",
                                config_var[i]);
                }
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
#    define BUFSIZE (MAX_CONFIG_VALUE + 128)
    char buf[BUFSIZE];

    char *line, *newline = NULL;
    ssize_t bufread = 0, bufwant;
    ssize_t len;

    while (true) {
        /* break file into lines */
        if (newline == NULL || newline == &BUFFER_LAST_ELEMENT(buf)) {
            bufwant = BUFSIZE - 1;
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
                bufread = os_read(f, buf + len, bufwant);
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
        if (newline == NULL) {
            /* only complain in debug build */
            USAGE_ERROR("Config file line \"%.20s...\" too long: truncating", line);
            NULL_TERMINATE_BUFFER(buf);
            newline = &BUFFER_LAST_ELEMENT(buf);
        }
        *newline = '\0';
        /* handle \r\n line endings */
        if (newline > line && *(newline - 1) == '\r')
            *(newline - 1) = '\0';
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
config_read(config_info_t *cfg, const char *appname_in, process_id_t pid, const char *sfx)
{
    file_t f_app = INVALID_FILE, f_default = INVALID_FILE;
    const char *local, *global;
    const char *appname = appname_in;
    char buf[MAXIMUM_PATH];
    bool check_global = true;
#    ifdef WINDOWS
    int retval;
#    endif
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
     * Custom takes precedence over default local.
     * if local exists, global is NOT read.
     */
    local = my_getenv(L_IF_WIN(DYNAMORIO_VAR_CONFIGDIR), buf, BUFFER_SIZE_BYTES(buf));
    if (local == NULL)
        local = my_getenv(L_IF_WIN(LOCAL_CONFIG_ENV), buf, BUFFER_SIZE_BYTES(buf));
    if (local != NULL) {
        process_id_t pid_to_check = pid;
        if (pid == 0 && cfg == &config)
            pid_to_check = get_process_id();
        if (pid_to_check != 0) {
            /* 1) <local>/appname.<pid>.1config
             * only makes sense for main config for this process
             */
            snprintf(cfg->fname_app, BUFFER_SIZE_ELEMENTS(cfg->fname_app),
                     "%s/%s/%s.%d.1%s", local, LOCAL_CONFIG_SUBDIR, appname, pid_to_check,
                     sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_app);
            INFO(2, "trying config file %s", cfg->fname_app);
            f_app = os_open(cfg->fname_app, OS_OPEN_READ);
            if (f_app != INVALID_FILE)
                cfg->has_1config = true; /* one-time file */
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
                     "%s/%s/default.0%s", local, LOCAL_CONFIG_SUBDIR, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_default);
            INFO(2, "trying config file %s", cfg->fname_default);
            f_default = os_open(cfg->fname_default, OS_OPEN_READ);
        }
    }
#    ifdef WINDOWS
    /* on Windows the global dir is <installbase>/config/ */
    retval =
        get_parameter_from_registry(L_DYNAMORIO_VAR_HOME, buf, BUFFER_SIZE_BYTES(buf));
    NULL_TERMINATE_BUFFER(buf);
    check_global = IS_GET_PARAMETER_SUCCESS(retval);
    global = buf;
#    else
    global = GLOBAL_CONFIG_DIR;
#    endif
    if (check_global) {
        /* 4) <global>/appname.config */
        if (f_app == INVALID_FILE) {
            snprintf(cfg->fname_app, BUFFER_SIZE_ELEMENTS(cfg->fname_app), "%s%s/%s.%s",
                     global, GLOBAL_CONFIG_SUBDIR, appname, sfx);
            NULL_TERMINATE_BUFFER(cfg->fname_app);
            INFO(2, "trying config file %s", cfg->fname_app);
            f_app = os_open(cfg->fname_app, OS_OPEN_READ);
        }
        /* 5) <global>/default.0config */
        if (f_default == INVALID_FILE) {
            snprintf(cfg->fname_default, BUFFER_SIZE_ELEMENTS(cfg->fname_default),
                     "%s%s/default.0%s", global, GLOBAL_CONFIG_SUBDIR, sfx);
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
    if (appname_in == NULL) /* only consider env for cur process */
        set_config_from_env(cfg);
}

/* up to caller to synchronize */
/* no support for otherarch */
void
config_reread(void)
{
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
    file_t f;
    config_info_t *tmp_config;

    if (config_reread_vals != NULL) {
        /* Re-reading the config file is reasonably fast, but not fast enough to
         * leave the data section unprotected without hitting curiosities about
         * datasec_not_prot.
         */
        tmp_config = config_reread_info;
        memcpy(config_reread_info, &config, sizeof(*config_reread_info));
        ASSERT(config_reread_info->u.v == &myvals);
        memcpy(config_reread_vals, &myvals, sizeof(*config_reread_vals));
        config_reread_info->u.v = config_reread_vals;
    } else {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        tmp_config = &config;
    }

    if (tmp_config->fname_app[0] != '\0') {
        f = os_open(tmp_config->fname_app, OS_OPEN_READ);
        if (f != INVALID_FILE) {
            INFO(3, "re-reading app config file %s", tmp_config->fname_app);
            DODEBUG({ infolevel -= 2; });
            read_config_file(f, tmp_config, true, true);
            DODEBUG({ infolevel += 2; });
            os_close(f);
        } else
            INFO(1, "WARNING: unable to re-read config file %s", tmp_config->fname_app);
    }
    if (tmp_config->fname_default[0] != '\0') {
        f = os_open(tmp_config->fname_default, OS_OPEN_READ);
        if (f != INVALID_FILE) {
            INFO(3, "re-reading default config file %s", tmp_config->fname_default);
            DODEBUG({ infolevel -= 2; });
            read_config_file(f, tmp_config, true, true);
            DODEBUG({ infolevel += 2; });
            os_close(f);
        } else
            INFO(1, "WARNING: unable to re-read config file %s",
                 tmp_config->fname_default);
    }
    /* 6) env vars fill in any still-unset values */
    set_config_from_env(tmp_config);

    if (config_reread_vals != NULL) {
        /* Unprotect the data section and copy the config results into the real
         * config.  Only the values should change, config_info_t should stay the
         * same.
         */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        memcpy(&myvals, config_reread_vals, sizeof(myvals));
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    } else {
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
#    else
    ASSERT_NOT_REACHED();
#    endif
}

/* Since querying for other arch or other app typically asks just about
 * one var in isolation, and is rare, rather than reading in the whole
 * thing and requiring a 6K struct we read the files looking for just
 * the var in question.
 */
static bool
get_config_val_other(const char *appname, process_id_t pid, const char *sfx,
                     const char *var, char *val, size_t valsz, bool *app_specific,
                     bool *from_env, bool *from_1config)
{
    /* Can't use heap very easily since used by preinject, injector, and DR
     * so we use a stack var.  WARNING: this is about 1.5K, and config_read
     * uses another 1.2K.
     */
    config_info_t info;
    memset(&info, 0, sizeof(info));
    info.query = var;
    config_read(&info, appname, pid, sfx);
    if (info.u.q.have_answer) {
        if (valsz > BUFFER_SIZE_ELEMENTS(info.u.q.answer.val))
            valsz = BUFFER_SIZE_ELEMENTS(info.u.q.answer.val);
        strncpy(val, info.u.q.answer.val, valsz);
        val[valsz - 1] = '\0';
        if (app_specific != NULL)
            *app_specific = info.u.q.answer.app_specific;
        if (from_env != NULL)
            *from_env = info.u.q.answer.from_env;
        if (from_1config != NULL)
            *from_1config = info.has_1config;
    }
    return info.u.q.have_answer;
}

bool
get_config_val_other_app(const char *appname, process_id_t pid, dr_platform_t platform,
                         const char *var, char *val, size_t valsz, bool *app_specific,
                         bool *from_env, bool *from_1config)
{
    const char *sfx;
    ;
    switch (platform) {
    case DR_PLATFORM_DEFAULT: sfx = CFG_SFX; break;
    case DR_PLATFORM_32BIT: sfx = CFG_SFX_32; break;
    case DR_PLATFORM_64BIT: sfx = CFG_SFX_64; break;
    default: return false; /* invalid parms */
    }
    return get_config_val_other(appname, pid, sfx, var, val, valsz, app_specific,
                                from_env, from_1config);
}

bool
get_config_val_other_arch(const char *var, char *val, size_t valsz, bool *app_specific,
                          bool *from_env, bool *from_1config)
{
    return get_config_val_other(NULL, 0, IF_X64_ELSE(CFG_SFX_32, CFG_SFX_64), var, val,
                                valsz, app_specific, from_env, from_1config);
}

void
d_r_config_init(void)
{
    config.u.v = &myvals;
    config_read(&config, NULL, 0, CFG_SFX);
    config_initialized = true;
}

bool
d_r_config_initialized(void)
{
    return config_initialized;
}

#    ifndef NOT_DYNAMORIO_CORE_PROPER
/* To support re-reading config, we need to heap allocate a config_vals_t array,
 * which we can leave unprotected.
 */
void
config_heap_init(void)
{
    config_reread_info = (config_info_t *)global_heap_alloc(sizeof(*config_reread_info)
                                                                HEAPACCT(ACCT_OTHER));
    config_reread_vals = (config_vals_t *)global_heap_alloc(sizeof(*config_reread_vals)
                                                                HEAPACCT(ACCT_OTHER));

    /* i#1271: to avoid leaving a stale 1config file behind if this process
     * crashes w/o a clean exit, we give up on re-reading the file and delete
     * it now.  It's an anonymous file anyway and not meant for manual updates.
     * The user could override the dynamic_options by re-specifying in
     * the option string, if desired, and re-create the 1config manually.
     * We do this here and not in d_r_config_init() so we can re-read it
     * after reload_dynamorio() in privload_early_inject().
     */
    if (config.has_1config) {
        INFO(2, "deleting config file %s", config.fname_app);
        os_delete_file(config.fname_app);
        dynamo_options.dynamic_options = false;
    }
    /* we ignore otherarch having 1config */
}

void
config_heap_exit(void)
{
    void *tmp;
    if (config_reread_info != NULL) {
        tmp = config_reread_info;
        config_reread_info = NULL;
        global_heap_free(tmp, sizeof(*config_reread_info) HEAPACCT(ACCT_OTHER));
    }
    if (config_reread_vals != NULL) {
        tmp = config_reread_vals;
        config_reread_vals = NULL;
        global_heap_free(tmp, sizeof(*config_reread_vals) HEAPACCT(ACCT_OTHER));
    }
}
#    endif

void
d_r_config_exit(void)
{
#    if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
    if (doing_detach) {
        /* Zero out globals for possible re-attach. */
        memset(&config, 0, sizeof config);
        memset(&myvals, 0, sizeof myvals);
    }
#    endif
    /* nothing -- so not called on fast exit (is called on detach) */
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
    if (!config_initialized)
        return GET_PARAMETER_FAILURE;
    if (ignore_cache)
        config_reread();
    val = get_config_val(name);
    /* env var has top priority, then registry */
    if (val != NULL) {
        strncpy(value, val, maxlen - 1);
        value[maxlen - 1] = '\0'; /* if max no null */
        /* we do not return GET_PARAMETER_NOAPPSPECIFIC like PARAMS_IN_REGISTRY
         * does: caller should use get_config_val_ex instead
         */
        return GET_PARAMETER_SUCCESS;
    }
    return GET_PARAMETER_FAILURE;
}

int
d_r_get_parameter(const char *name, char *value, int maxlen)
{
    return get_parameter_ex(name, value, maxlen, false);
}

int
get_unqualified_parameter(const char *name, char *value, int maxlen)
{
    /* we don't use qualified names w/ our config files yet */
    return d_r_get_parameter(name, value, maxlen);
}

#    ifdef UNIX
/* Handle rununder values (Windows does this in systemwide_should_inject() and
 * has more complex logic as it has more options).
 */
bool
should_inject_from_rununder(const char *runstr, bool app_specific, bool from_env,
                            bool *rununder_on OUT)
{
    int rununder;
    *rununder_on = false;
    if (runstr == NULL || runstr[0] == '\0')
        return false;
    /* decimal only for now */
    if (sscanf(runstr, "%d", &rununder) != 1)
        return false;
    /* env var counts as app-specific */
    if (!app_specific && !from_env) {
        if (TEST(RUNUNDER_ALL, rununder))
            *rununder_on = true;
    } else if (TEST(RUNUNDER_ON, rununder))
        *rununder_on = true;
    /* Linux ignores RUNUNDER_EXPLICIT, RUNUNDER_COMMANDLINE_*, RUNUNDER_ONCE */
    return true;
}
#    endif

#else /* !PARAMS_IN_REGISTRY around whole file */

void
d_r_config_init(void)
{
}

void
d_r_config_exit(void)
{
}

#endif /* !PARAMS_IN_REGISTRY around whole file */
