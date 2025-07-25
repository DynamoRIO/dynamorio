/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

/* DynamoRio eXtension Time and Timer Scaling. */

#include "dr_api.h"
#include "drx.h"
#include "drmgr.h"
#include "../ext_utils.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef UNIX
#    ifdef LINUX
#        include "../../core/unix/include/syscall.h"
#    else
#        error Non-Linux not yet supported
#        include <sys/syscall.h>
#    endif
#    include <sys/time.h>
#else
#    error Non-UNIX not supported
#endif

#ifdef DEBUG
static uint verbose = 1;
#    define NOTIFY(level, ...)                   \
        do {                                     \
            if (verbose >= (level)) {            \
                dr_fprintf(STDERR, __VA_ARGS__); \
            }                                    \
        } while (0)
#else
#    define NOTIFY(...) /* nothing */
#endif

#ifndef X64
/* This is not in public headers as it's hidden inside glibc.
 * The 32-bit-build struct timespec has 32-bit fields; this provides 64-bit
 * so the seconds can go beyond the year 2038.
 * (The 64-bit-build struct timespec has 64-bit fields.)
 * XXX: If this were C++ we could templatize and share code to avoid all this
 * duplication of identical code.  We could do header inclusion with macros
 * in C but that seems overkill for this amount of code.
 */
typedef struct _timespec64_t {
    int64 tv_sec;
    int64 tv_nsec;
} timespec64_t;

/* The 32-bit structure has a 32-bit int for seconds, and there is a flag to
 * specify an absolute time, which means it can't reach past 2038: so Linux
 * added a new data struct and new syscalls so 32-bit can use 64-bit values for
 * the seconds. That data structure is not easily found in public headers so we
 * define it ourselves to use with the 64-variant syscalls in 32-bit.
 */
typedef struct _itimerspec64_t {
    timespec64_t it_interval;
    timespec64_t it_value;
} itimerspec64_t;
#endif

typedef struct _per_thread_t {
    struct itimerspec itimer_spec;
#ifndef X64
    itimerspec64_t itimer_spec64;
#endif
    struct itimerval itimer_val;
    struct timespec time_spec;
#ifndef X64
    timespec64_t time_spec64;
#endif
    void *app_read_timer_param;
    void *app_set_timer_param;
} per_thread_t;

/* Globals only written at init time. */
static int tls_idx;
static drx_time_scale_t scale_options;

/* Globals access via DR atomics. */
static int init_count;

static const int MAX_TV_NSEC = 1000000000ULL;
static const int MAX_TV_USEC = 1000000ULL;

static inline bool
is_timespec_zero(struct timespec *spec)
{
    return spec->tv_sec == 0 && spec->tv_nsec == 0;
}

#ifndef X64
static inline bool
is_timespec64_zero(timespec64_t *spec)
{
    return spec->tv_sec == 0 && spec->tv_nsec == 0;
}
#endif

static inline bool
is_timeval_zero(struct timeval *val)
{
    return val->tv_sec == 0 && val->tv_usec == 0;
}

static void
inflate_timespec(void *drcontext, struct timespec *spec, int scale)
{
    NOTIFY(2, "T" TIDFMT "  Original time %" SSZFC ".%.12" SSZFC "\n",
           dr_get_thread_id(drcontext), spec->tv_sec, spec->tv_nsec);
    if (is_timespec_zero(spec) || scale == 1)
        return;
    spec->tv_nsec *= scale;
    spec->tv_sec *= scale;
    spec->tv_sec += spec->tv_nsec / MAX_TV_NSEC;
    spec->tv_nsec = spec->tv_nsec % MAX_TV_NSEC;
    NOTIFY(2, "T" TIDFMT " Inflated time by %dx: now %" SSZFC ".%.12" SSZFC "\n",
           dr_get_thread_id(drcontext), scale, spec->tv_sec, spec->tv_nsec);
}

#ifndef X64
static void
inflate_timespec64(void *drcontext, timespec64_t *spec, int scale)
{
    NOTIFY(2,
           "T" TIDFMT "  Original time %" INT64_FORMAT_CODE ".%.12" INT64_FORMAT_CODE
           "\n",
           dr_get_thread_id(drcontext), spec->tv_sec, spec->tv_nsec);
    if (is_timespec64_zero(spec) || scale == 1)
        return;
    spec->tv_nsec *= scale;
    spec->tv_sec *= scale;
    spec->tv_sec += spec->tv_nsec / MAX_TV_NSEC;
    spec->tv_nsec = spec->tv_nsec % MAX_TV_NSEC;
    NOTIFY(2,
           "T" TIDFMT " Inflated time by %dx: now %" INT64_FORMAT_CODE
           ".%.12" INT64_FORMAT_CODE "\n",
           dr_get_thread_id(drcontext), scale, spec->tv_sec, spec->tv_nsec);
}
#endif

static void
deflate_timespec(void *drcontext, struct timespec *spec, int scale)
{
    if (is_timespec_zero(spec) || scale == 1)
        return;
    spec->tv_nsec /= scale;
    spec->tv_nsec += (spec->tv_sec % MAX_TV_NSEC) * MAX_TV_NSEC / scale;
    spec->tv_sec /= scale;
    NOTIFY(2, "T" TIDFMT "  Deflated time by %dx: now %" SSZFC ".%.12" SSZFC "\n",
           dr_get_thread_id(drcontext), scale, spec->tv_sec, spec->tv_nsec);
}

#ifndef X64
static void
deflate_timespec64(void *drcontext, timespec64_t *spec, int scale)
{
    if (is_timespec64_zero(spec) || scale == 1)
        return;
    spec->tv_nsec /= scale;
    spec->tv_nsec += (spec->tv_sec % MAX_TV_NSEC) * MAX_TV_NSEC / scale;
    spec->tv_sec /= scale;
    NOTIFY(2,
           "T" TIDFMT "  Deflated time by %dx: now %" INT64_FORMAT_CODE
           ".%.12" INT64_FORMAT_CODE "\n",
           dr_get_thread_id(drcontext), scale, spec->tv_sec, spec->tv_nsec);
}
#endif

static void
inflate_timeval(void *drcontext, struct timeval *val, int scale)
{
    NOTIFY(2, "T" TIDFMT "  Original time %" SSZFC ".%.9" SSZFC "\n",
           dr_get_thread_id(drcontext), val->tv_sec, val->tv_usec);
    if (is_timeval_zero(val) || scale == 1)
        return;
    val->tv_usec *= scale;
    val->tv_sec *= scale;
    val->tv_sec += val->tv_usec / MAX_TV_USEC;
    val->tv_usec = val->tv_usec % MAX_TV_USEC;
    NOTIFY(2, "T" TIDFMT "  Inflated time by %dx: now %" SSZFC ".%.9" SSZFC "\n",
           dr_get_thread_id(drcontext), scale, val->tv_sec, val->tv_usec);
}

static void
deflate_timeval(void *drcontext, struct timeval *val, int scale)
{
    if (is_timeval_zero(val))
        return;
    val->tv_usec /= scale;
    val->tv_usec += (val->tv_sec % MAX_TV_USEC) * MAX_TV_USEC / scale;
    val->tv_sec /= scale;
    NOTIFY(2, "T" TIDFMT "  Deflated time by %dx: now %" SSZFC ".%.9" SSZFC "\n",
           dr_get_thread_id(drcontext), scale, val->tv_sec, val->tv_usec);
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    memset(data, 0, sizeof(*data));
    drmgr_set_tls_field(drcontext, tls_idx, data);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
    switch (sysnum) {
    case SYS_timer_settime:
    case SYS_timer_gettime:
#ifndef X64
    case SYS_timer_gettime64:
    case SYS_timer_settime64:
#endif
    case SYS_setitimer:
    case SYS_getitimer: return true;
    }
    return false;
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    switch (sysnum) {
    case SYS_timer_settime: {
        int flags = (int)dr_syscall_get_param(drcontext, 1);
        struct itimerspec *new_spec =
            (struct itimerspec *)dr_syscall_get_param(drcontext, 2);
        struct itimerspec *old_spec =
            (struct itimerspec *)dr_syscall_get_param(drcontext, 3);
        NOTIFY(2, "T" TIDFMT " timer_settime flags=%d, old=%p, new=%p\n",
               dr_get_thread_id(drcontext), flags, new_spec, old_spec);
        data->app_set_timer_param = new_spec;
        data->app_read_timer_param = old_spec;
        if (TEST(TIMER_ABSTIME, flags)) {
            /* TODO i#7504: Handle TIMER_ABSTIME and SYS_timer_getoverrun. */
            NOTIFY(0, "Absolute time is not supported\n");
            data->app_read_timer_param = NULL; /* Don't scale in post. */
            return true;
        }
        size_t wrote;
        if (dr_safe_read(new_spec, sizeof(data->itimer_spec), &data->itimer_spec,
                         &wrote) &&
            wrote == sizeof(data->itimer_spec)) {
            inflate_timespec(drcontext, &data->itimer_spec.it_interval,
                             scale_options.timer_scale);
            inflate_timespec(drcontext, &data->itimer_spec.it_value,
                             scale_options.timer_scale);
            dr_syscall_set_param(drcontext, 2, (reg_t)&data->itimer_spec);
        }
        break;
    }
#ifndef X64
    case SYS_timer_settime64: {
        int flags = (int)dr_syscall_get_param(drcontext, 1);
        itimerspec64_t *new_spec = (itimerspec64_t *)dr_syscall_get_param(drcontext, 2);
        itimerspec64_t *old_spec = (itimerspec64_t *)dr_syscall_get_param(drcontext, 3);
        NOTIFY(2, "T" TIDFMT " timer_settime64 flags=%d, old=%p, new=%p\n",
               dr_get_thread_id(drcontext), flags, new_spec, old_spec);
        data->app_set_timer_param = new_spec;
        data->app_read_timer_param = old_spec;
        if (TEST(TIMER_ABSTIME, flags)) {
            /* TODO i#7504: Handle TIMER_ABSTIME and SYS_timer_getoverrun. */
            NOTIFY(0, "Absolute time is not supported\n");
            data->app_read_timer_param = NULL; /* Don't scale in post. */
            return true;
        }
        size_t wrote;
        if (dr_safe_read(new_spec, sizeof(data->itimer_spec64), &data->itimer_spec64,
                         &wrote) &&
            wrote == sizeof(data->itimer_spec64)) {
            inflate_timespec64(drcontext, &data->itimer_spec64.it_interval,
                               scale_options.timer_scale);
            inflate_timespec64(drcontext, &data->itimer_spec64.it_value,
                               scale_options.timer_scale);
            dr_syscall_set_param(drcontext, 2, (reg_t)&data->itimer_spec64);
        }
        break;
    }
#endif
    case SYS_timer_gettime:
        NOTIFY(2, "T" TIDFMT " timer_gettime\n", dr_get_thread_id(drcontext));
        data->app_read_timer_param = (void *)dr_syscall_get_param(drcontext, 1);
        break;
#ifndef X64
    case SYS_timer_gettime64:
        NOTIFY(2, "T" TIDFMT " timer_gettime64\n", dr_get_thread_id(drcontext));
        data->app_read_timer_param = (void *)dr_syscall_get_param(drcontext, 1);
        break;
#endif
    case SYS_setitimer: {
        NOTIFY(2, "T" TIDFMT " setitimer\n", dr_get_thread_id(drcontext));
        struct itimerval *new_val =
            (struct itimerval *)dr_syscall_get_param(drcontext, 1);
        struct itimerval *old_val =
            (struct itimerval *)dr_syscall_get_param(drcontext, 2);
        data->app_set_timer_param = new_val;
        data->app_read_timer_param = old_val;
        size_t wrote;
        if (dr_safe_read(new_val, sizeof(data->itimer_val), &data->itimer_val, &wrote) &&
            wrote == sizeof(data->itimer_val)) {
            inflate_timeval(drcontext, &data->itimer_val.it_interval,
                            scale_options.timer_scale);
            inflate_timeval(drcontext, &data->itimer_val.it_value,
                            scale_options.timer_scale);
            dr_syscall_set_param(drcontext, 1, (reg_t)&data->itimer_val);
        }
        break;
    }
    case SYS_getitimer:
        NOTIFY(2, "T" TIDFMT " getitimer\n", dr_get_thread_id(drcontext));
        data->app_read_timer_param = (void *)dr_syscall_get_param(drcontext, 1);
        break;
    case SYS_nanosleep: {
        struct timespec *spec = (struct timespec *)dr_syscall_get_param(drcontext, 0);
        struct timespec *remain = (struct timespec *)dr_syscall_get_param(drcontext, 1);
        NOTIFY(2, "T" TIDFMT " nanosleep time=%p, remaim=%p\n",
               dr_get_thread_id(drcontext), spec, remain);
        data->app_set_timer_param = spec;
        data->app_read_timer_param = remain;
        size_t wrote;
        if (dr_safe_read(spec, sizeof(data->time_spec), &data->time_spec, &wrote) &&
            wrote == sizeof(data->time_spec)) {
            inflate_timespec(drcontext, &data->time_spec, scale_options.timeout_scale);
            dr_syscall_set_param(drcontext, 0, (reg_t)&data->time_spec);
        }
        break;
    }
    case SYS_clock_nanosleep: {
        int flags = (int)dr_syscall_get_param(drcontext, 1);
        struct timespec *spec = (struct timespec *)dr_syscall_get_param(drcontext, 2);
        struct timespec *remain = (struct timespec *)dr_syscall_get_param(drcontext, 3);
        NOTIFY(2, "T" TIDFMT " clock_nanosleep flags=%d, time=%p, remaim=%p\n",
               dr_get_thread_id(drcontext), flags, spec, remain);
        data->app_set_timer_param = spec;
        data->app_read_timer_param = remain;
        if (TEST(TIMER_ABSTIME, flags)) {
            /* TODO i#7504: Handle TIMER_ABSTIME and SYS_timer_getoverrun. */
            NOTIFY(0, "Absolute time is not supported\n");
            data->app_read_timer_param = NULL; /* Don't scale in post. */
            return true;
        }
        size_t wrote;
        if (dr_safe_read(spec, sizeof(data->time_spec), &data->time_spec, &wrote) &&
            wrote == sizeof(data->time_spec)) {
            inflate_timespec(drcontext, &data->time_spec, scale_options.timeout_scale);
            dr_syscall_set_param(drcontext, 2, (reg_t)&data->time_spec);
        }
        break;
    }
#ifndef X64
    case SYS_clock_nanosleep_time64: {
        int flags = (int)dr_syscall_get_param(drcontext, 1);
        timespec64_t *spec = (timespec64_t *)dr_syscall_get_param(drcontext, 2);
        timespec64_t *remain = (timespec64_t *)dr_syscall_get_param(drcontext, 3);
        NOTIFY(2, "T" TIDFMT " clock_nanosleep_time64 flags=%d, time=%p, remaim=%p\n",
               dr_get_thread_id(drcontext), flags, spec, remain);
        data->app_set_timer_param = spec;
        data->app_read_timer_param = remain;
        if (TEST(TIMER_ABSTIME, flags)) {
            /* TODO i#7504: Handle TIMER_ABSTIME and SYS_timer_getoverrun. */
            NOTIFY(0, "Absolute time is not supported\n");
            data->app_read_timer_param = NULL; /* Don't scale in post. */
            return true;
        }
        size_t wrote;
        if (dr_safe_read(spec, sizeof(data->time_spec64), &data->time_spec64, &wrote) &&
            wrote == sizeof(data->time_spec64)) {
            inflate_timespec64(drcontext, &data->time_spec64,
                               scale_options.timeout_scale);
            dr_syscall_set_param(drcontext, 2, (reg_t)&data->time_spec64);
        }
        break;
    }
#endif
    }
    return true;
}

static void
event_post_syscall(void *drcontext, int sysnum)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_syscall_result_info_t info = {
        sizeof(info),
    };
    info.use_errno = true;
    if (!dr_syscall_get_result_ex(drcontext, &info))
        NOTIFY(1, "Can't tell whether syscall %d failed\n", sysnum);
    /* We need to pretend the actual value is the original un-inflated value. */
    switch (sysnum) {
    case SYS_timer_settime:
        dr_syscall_set_param(drcontext, 2, (reg_t)data->app_set_timer_param);
        /* Deliberate fallthrough. */
    case SYS_timer_gettime: {
        size_t wrote;
        if (!info.succeeded) {
            NOTIFY(1, "Syscall %d failed with %p: not scaling\n", sysnum, info.value);
        } else if (data->app_read_timer_param != NULL &&
                   dr_safe_read(data->app_read_timer_param, sizeof(data->itimer_spec),
                                &data->itimer_spec, &wrote) &&
                   wrote == sizeof(data->itimer_spec)) {
            deflate_timespec(drcontext, &data->itimer_spec.it_interval,
                             scale_options.timer_scale);
            deflate_timespec(drcontext, &data->itimer_spec.it_value,
                             scale_options.timer_scale);
            if (!dr_safe_write(data->app_read_timer_param, sizeof(data->itimer_spec),
                               &data->itimer_spec, &wrote) ||
                wrote != sizeof(data->itimer_spec)) {
                NOTIFY(0, "Failed to modify timer cur value\n");
            }
        }
        break;
    }
#ifndef X64
    case SYS_timer_settime64:
        dr_syscall_set_param(drcontext, 2, (reg_t)data->app_set_timer_param);
        /* Deliberate fallthrough. */
    case SYS_timer_gettime64: {
        size_t wrote;
        if (!info.succeeded) {
            NOTIFY(1, "Syscall %d failed with %p: not scaling\n", sysnum, info.value);
        } else if (data->app_read_timer_param != NULL &&
                   dr_safe_read(data->app_read_timer_param, sizeof(data->itimer_spec64),
                                &data->itimer_spec64, &wrote) &&
                   wrote == sizeof(data->itimer_spec64)) {
            deflate_timespec64(drcontext, &data->itimer_spec64.it_interval,
                               scale_options.timer_scale);
            deflate_timespec64(drcontext, &data->itimer_spec64.it_value,
                               scale_options.timer_scale);
            if (!dr_safe_write(data->app_read_timer_param, sizeof(data->itimer_spec64),
                               &data->itimer_spec64, &wrote) ||
                wrote != sizeof(data->itimer_spec64)) {
                NOTIFY(0, "Failed to modify timer cur value\n");
            }
        }
        break;
    }
#endif
    case SYS_setitimer:
        dr_syscall_set_param(drcontext, 1, (reg_t)data->app_set_timer_param);
        /* Deliberate fallthrough. */
    case SYS_getitimer: {
        size_t wrote;
        if (!info.succeeded) {
            NOTIFY(1, "Syscall %d failed with %p: not scaling\n", sysnum, info.value);
        } else if (data->app_read_timer_param != NULL &&
                   dr_safe_read(data->app_read_timer_param, sizeof(data->itimer_val),
                                &data->itimer_val, &wrote) &&
                   wrote == sizeof(data->itimer_val)) {
            deflate_timeval(drcontext, &data->itimer_val.it_interval,
                            scale_options.timer_scale);
            deflate_timeval(drcontext, &data->itimer_val.it_value,
                            scale_options.timer_scale);
            if (!dr_safe_write(data->app_read_timer_param, sizeof(data->itimer_val),
                               &data->itimer_val, &wrote) ||
                wrote != sizeof(data->itimer_val)) {
                NOTIFY(0, "Failed to modify timer cur value\n");
            }
        }
        break;
    }
    case SYS_nanosleep:
#ifdef X86
        /* For AArch64 and RISC-V parameter 0 becomes the return value, so we do not
         * want to restore the pre-syscall value.
         */
        dr_syscall_set_param(drcontext, 0, (reg_t)data->app_set_timer_param);
#endif
        /* Deliberate fallthrough. */
    case SYS_clock_nanosleep: {
        if (sysnum == SYS_clock_nanosleep)
            dr_syscall_set_param(drcontext, 2, (reg_t)data->app_set_timer_param);
        size_t wrote;
        if (info.succeeded) {
            /* Nothing to do: the remainder is only written on EINTR. */
        } else if (info.errno_value != EINTR) {
            NOTIFY(1, "Syscall %d failed with %p: not scaling\n", sysnum, info.value);
        } else if (data->app_read_timer_param != NULL &&
                   dr_safe_read(data->app_read_timer_param, sizeof(data->time_spec),
                                &data->time_spec, &wrote) &&
                   wrote == sizeof(data->time_spec)) {
            deflate_timespec(drcontext, &data->time_spec, scale_options.timeout_scale);
            if (!dr_safe_write(data->app_read_timer_param, sizeof(data->time_spec),
                               &data->time_spec, &wrote) ||
                wrote != sizeof(data->time_spec)) {
                NOTIFY(0, "Failed to modify sleep remaining value\n");
            }
        }
        break;
    }
#ifndef X64
    case SYS_clock_nanosleep_time64: {
        dr_syscall_set_param(drcontext, 2, (reg_t)data->app_set_timer_param);
        size_t wrote;
        if (info.succeeded) {
            /* Nothing to do: the remainder is only written on EINTR. */
        } else if (info.errno_value != EINTR) {
            NOTIFY(1, "Syscall %d failed with %p: not scaling\n", sysnum, info.value);
        } else if (data->app_read_timer_param != NULL &&
                   dr_safe_read(data->app_read_timer_param, sizeof(data->time_spec64),
                                &data->time_spec64, &wrote) &&
                   wrote == sizeof(data->time_spec64)) {
            deflate_timespec64(drcontext, &data->time_spec64,
                               scale_options.timeout_scale);
            if (!dr_safe_write(data->app_read_timer_param, sizeof(data->time_spec64),
                               &data->time_spec64, &wrote) ||
                wrote != sizeof(data->time_spec64)) {
                NOTIFY(0, "Failed to modify sleep remaining value\n");
            }
        }
        break;
    }
#endif
    }
}

static void
scale_itimers(void *drcontext, bool inflate)
{
    /* We use dr_invoke_syscall_as_app() because DR needs to intercept these to
     * interact with its multiplexing of app and client itimers (and maybe POSIX
     * timers in the future).  We do not want to trigger our own syscall
     * events for these, and DR indeed does not raise client events here.
     */
    NOTIFY(2, "Scaling itimers\n");
    const int TIMER_TYPES[] = { ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF };
    int res;
    for (int i = 0; i < sizeof(TIMER_TYPES) / sizeof(TIMER_TYPES[0]); ++i) {
        NOTIFY(2, "Scaling itimer %d\n", i);
        struct itimerval val;
        // We use dr_invoke_syscall_as_app() because DR needs to intercept these to
        // interact with its multiplexing of app and client itimers.
        res = dr_invoke_syscall_as_app(drcontext, SYS_getitimer, 2, TIMER_TYPES[i], &val);
        if (res != 0) {
            NOTIFY(0, "Failed to call getitimer for id %d: %d\n", i, res);
            continue;
        }
        NOTIFY(3, "As-queried: value=%lu.%lu interval=%lu.%lu\n", val.it_value.tv_sec,
               val.it_value.tv_usec, val.it_interval.tv_sec, val.it_interval.tv_usec);
        if (is_timeval_zero(&val.it_value) && is_timeval_zero(&val.it_interval)) {
            /* Disabled; nothing to do. */
            continue;
        }
        /* If the timer just fired, we can have a zero it_value.  If we send that
         * to setitimer we will disable the timer, so we reset it ourselves.
         */
        if (is_timeval_zero(&val.it_value)) {
            val.it_value.tv_sec = val.it_interval.tv_sec;
            val.it_value.tv_usec = val.it_interval.tv_usec;
        }
        if (inflate) {
            inflate_timeval(drcontext, &val.it_interval, scale_options.timer_scale);
            inflate_timeval(drcontext, &val.it_value, scale_options.timer_scale);
        } else {
            deflate_timeval(drcontext, &val.it_interval, scale_options.timer_scale);
            deflate_timeval(drcontext, &val.it_value, scale_options.timer_scale);
        }
        res = dr_invoke_syscall_as_app(drcontext, SYS_setitimer, 3, TIMER_TYPES[i], &val,
                                       NULL);
        if (res != 0)
            NOTIFY(0, "Failed to call setitimer for id %d: %d\n", i, res);
    }
}

/* Caller must set filebuf_pos and filebuf_read to 0 for first call.
 * XXX: Move into core DR API?
 */
static bool
dr_get_line(file_t fd, char *filebuf, size_t filebuf_max, size_t *filebuf_read,
            size_t *filebuf_pos, char *linebuf, size_t linebuf_max)
{
    size_t linebuf_pos = 0;
    do {
        if (*filebuf_pos >= *filebuf_read) {
            *filebuf_read = dr_read_file(fd, filebuf, filebuf_max);
            if (*filebuf_read == 0)
                return false;
            *filebuf_pos = 0;
        }
        if (filebuf[*filebuf_pos] != '\n' && linebuf_pos + 1 < linebuf_max) {
            linebuf[linebuf_pos] = filebuf[*filebuf_pos];
            ++linebuf_pos;
        }
        ++(*filebuf_pos);
    } while (filebuf[*filebuf_pos] != '\n');
    linebuf[linebuf_pos] = '\0';
    return true;
}

static void
scale_posix_timers(void *drcontext, bool inflate)
{
    NOTIFY(2, "Scaling POSIX timers\n");
    file_t fd = dr_open_file("/proc/self/timers", DR_FILE_READ);
    if (fd == INVALID_FILE) {
        NOTIFY(0, "Failed to enumerate POSIX timers\n");
        return;
    }
#define FILE_BUF_SIZE 256
    char filebuf[FILE_BUF_SIZE];
#define LINE_BUF_SIZE 64
    char linebuf[FILE_BUF_SIZE];
    size_t filebuf_read = 0, filebuf_pos = 0;
    int res;
    while (dr_get_line(fd, filebuf, BUFFER_SIZE_BYTES(filebuf), &filebuf_read,
                       &filebuf_pos, linebuf, BUFFER_SIZE_BYTES(linebuf))) {
        NOTIFY(2, "Read line: |%s|\n", linebuf);
        int id = -1;
        int found = dr_sscanf(linebuf, "ID: %d", &id);
        if (found == 1 && id >= 0) {
#ifdef X64
            struct itimerspec spec;
            /* We use dr_invoke_syscall_as_app() because DR needs to intercept these to
             * interact with its multiplexing of app and client itimers (and maybe POSIX
             * timers in the future).  We do not want to trigger our own syscall
             * events for these, and DR indeed does not raise client events here.
             */
            res = dr_invoke_syscall_as_app(drcontext, SYS_timer_gettime, 2, id, &spec);
            if (res != 0) {
                NOTIFY(0, "Failed to call timer_gettime for id %d: %d\n", id, res);
                continue;
            }
            if (is_timespec_zero(&spec.it_value) && is_timespec_zero(&spec.it_interval)) {
                /* Disabled; nothing to do. */
                continue;
            }
            /* If the timer just fired, we can have a zero it_value.  If we send that
             * to timer_settime we will disable the timer, so we reset it ourselves.
             */
            if (is_timespec_zero(&spec.it_value)) {
                spec.it_value.tv_sec = spec.it_interval.tv_sec;
                spec.it_value.tv_nsec = spec.it_interval.tv_nsec;
            }
            if (inflate) {
                inflate_timespec(drcontext, &spec.it_interval, scale_options.timer_scale);
                inflate_timespec(drcontext, &spec.it_value, scale_options.timer_scale);
            } else {
                deflate_timespec(drcontext, &spec.it_interval, scale_options.timer_scale);
                deflate_timespec(drcontext, &spec.it_value, scale_options.timer_scale);
            }
            res = dr_invoke_syscall_as_app(drcontext, SYS_timer_settime, 4, id, 0, &spec,
                                           NULL);
            if (res != 0)
                NOTIFY(0, "Failed to call timer_settime for id %d: %d\n", id, res);
#else
            itimerspec64_t spec;
            /* See above comment about dr_invoke_syscall_as_app(). */
            res = dr_invoke_syscall_as_app(drcontext, SYS_timer_gettime64, 2, id, &spec);
            if (res != 0) {
                NOTIFY(0, "Failed to call timer_gettime64 for id %d: %d\n", id, res);
                continue;
            }
            if (is_timespec64_zero(&spec.it_value) &&
                is_timespec64_zero(&spec.it_interval)) {
                /* Disabled; nothing to do. */
                continue;
            }
            /* If the timer just fired, we can have a zero it_value.  If we send that
             * to timer_settime we will disable the timer, so we reset it ourselves.
             */
            if (is_timespec64_zero(&spec.it_value)) {
                spec.it_value.tv_sec = spec.it_interval.tv_sec;
                spec.it_value.tv_nsec = spec.it_interval.tv_nsec;
            }
            if (inflate) {
                inflate_timespec64(drcontext, &spec.it_interval,
                                   scale_options.timer_scale);
                inflate_timespec64(drcontext, &spec.it_value, scale_options.timer_scale);
            } else {
                deflate_timespec64(drcontext, &spec.it_interval,
                                   scale_options.timer_scale);
                deflate_timespec64(drcontext, &spec.it_value, scale_options.timer_scale);
            }
            res = dr_invoke_syscall_as_app(drcontext, SYS_timer_settime64, 4, id, 0,
                                           &spec, NULL);
            if (res != 0)
                NOTIFY(0, "Failed to call timer_settime64 for id %d: %d\n", id, res);
#endif
        }
    }
    dr_close_file(fd);
}

DR_EXPORT
bool
drx_register_time_scaling(drx_time_scale_t *options)
{
    /* As documented, can only be called once (before unregister is called). */
    int count = dr_atomic_add32_return_sum(&init_count, 1);
    if (count != 1) {
        dr_atomic_add32_return_sum(&init_count, -1);
        return false;
    }

    if (options->struct_size != sizeof(drx_time_scale_t))
        return false;
    if (options->timer_scale == 0 || options->timeout_scale == 0)
        return false; /* Invalid scale. */
    if (options->timer_scale == 1 && options->timeout_scale == 1) {
        /* No real scaling, but we continue to allow testing the full infrasturcture
         * without actual scaling.
         */
    }

    scale_options = *options;

    drmgr_priority_t init_priority = { sizeof(init_priority),
                                       DRMGR_PRIORITY_NAME_DRX_SCALE_INIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_INIT_DRX_SCALE };
    drmgr_priority_t exit_priority = { sizeof(exit_priority),
                                       DRMGR_PRIORITY_NAME_DRX_SCALE_EXIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_EXIT_DRX_SCALE };
    drmgr_priority_t presys_priority = { sizeof(init_priority),
                                         DRMGR_PRIORITY_NAME_DRX_SCALE_PRE_SYS, NULL,
                                         NULL, DRMGR_PRIORITY_PRE_SYS_DRX_SCALE };
    drmgr_priority_t postsys_priority = { sizeof(init_priority),
                                          DRMGR_PRIORITY_NAME_DRX_SCALE_POST_SYS, NULL,
                                          NULL, DRMGR_PRIORITY_POST_SYS_DRX_SCALE };

    dr_register_filter_syscall_event(event_filter_syscall);

    if (!drmgr_register_thread_init_event_ex(event_thread_init, &init_priority) ||
        !drmgr_register_thread_exit_event_ex(event_thread_exit, &exit_priority) ||
        !drmgr_register_pre_syscall_event_ex(event_pre_syscall, &presys_priority) ||
        !drmgr_register_post_syscall_event_ex(event_post_syscall, &postsys_priority))
        return false;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;

    void *drcontext = dr_get_current_drcontext();
    /* XXX i#7504: For dynamic attach, at process init time other threads are not
     * yet taken over and so our timer sweep here can be inaccurate with the
     * gap between now and taking over other threads.
     * If we move this to the post-attach event, though, we need to record
     * what we inflated so we don't double-inflate a syscall-inflated timer
     * seen in the gap before we get to the post-attach event.
     * It would be nicer if DR suspended all the other threads prior to
     * process init here, when attaching.
     */
    scale_itimers(drcontext, /*inflate=*/true);
    scale_posix_timers(drcontext, /*inflate=*/true);

    return true;
}

DR_EXPORT
bool
drx_unregister_time_scaling()
{
    int count = dr_atomic_add32_return_sum(&init_count, -1);
    if (count != 0)
        return false;

    void *drcontext = dr_get_current_drcontext();
    scale_itimers(drcontext, /*inflate=*/false);
    scale_posix_timers(drcontext, /*inflate=*/false);

    return drmgr_unregister_tls_field(tls_idx) &&
        dr_unregister_filter_syscall_event(event_filter_syscall) &&
        drmgr_register_pre_syscall_event(event_pre_syscall) &&
        drmgr_register_post_syscall_event(event_post_syscall) &&
        drmgr_unregister_thread_init_event(event_thread_init) &&
        drmgr_unregister_thread_exit_event(event_thread_exit);
}
