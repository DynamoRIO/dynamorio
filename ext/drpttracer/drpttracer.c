/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Intel PT Tracing Extension. */

#include <unistd.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <dirent.h> /* opendir, readdir */
#include "../core/unix/include/syscall_linux_x86.h"

#include "dr_api.h"
#include "drmgr.h"
#include "hashtable.h"

#if !defined(X86_64) && !defined(LINUX)
#    error "This is only for Linux x86_64."
#endif

/* currently using asserts on internal logic sanity checks (never on
 * input from user)
 */
#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define DODEBUG(statement) \
        do {                   \
            statement          \
        } while (0)
#else
#    define ASSERT(x, msg) /* nothing */
#    define DODEBUG(statement)
#endif

typedef struct _range_t {
    union {
        uint32_t range;
        struct {
            uint32_t start : 16;
            uint32_t end : 16;
        };
    };
} range_t;

#define PT_PMU_TYPE_FILE "/sys/bus/event_source/devices/intel_pt/type"

/* The PMU type id of intel_pt is different on different machines.
 * We read it from PT_PMU_TYPE_FILE.
 */
static int pt_pmu_type;

#define PT_PMU_EVENT_CONFIG_DIR "/sys/bus/event_source/devices/intel_pt/format"

/* The intel_pt config terms table of intel_pt PMU events. The item's key is the event's
 * name, and the value is the event's bit fields in perf_event_attr.config.
 * We get them from the files in PT_PMU_EVENT_CONFIG_DIR.
 */
static hashtable_t pt_config_terms_table;
#define PT_CONFIG_TERMS_TABLE_HASH_BITS 12

typedef struct _perf_fd_t {
    int fd;
    struct perf_event_mmap_page *mpage;
    int buf_size_shift;
} perf_fd_t;

typedef struct _perf_aux_map_t {
    void *aux_map;
} perf_aux_map_t;

typedef struct _per_thread_t {
    perf_fd_t perf_fd;
    perf_aux_map_t perf_aux_map;
} per_thread_t;

/***************************************************************************
 * PERF EVENT RELATED FUNCTIONS
 */

static bool
read_file_to_buffer(IN const char *filename, OUT char **buf, OUT uint64_t *buf_size)
{
    char *local_buf = NULL;
    uint64_t local_buf_size = 0;
    file_t *f = dr_open_file(filename, DR_FILE_READ);
    if (f == INVALID_FILE) {
        ASSERT(false, "failed to open file %s", filename);
        return false;
    }

    if (dr_file_size(f, &local_buf_size) != 0) {
        ASSERT(false, "failed to get file size of %s", filename);
        dr_close_file(f);
        return false;
    }

    local_buf = (char *)dr_global_alloc(local_buf_size);
    if (dr_read_file(f, local_buf, local_buf_size)) {
        ASSERT(false, "failed to read file %s", filename);
        dr_global_free(local_buf, local_buf_size);
        dr_close_file(f);
        return false;
    }

    *buf = local_buf;
    *buf_size = local_buf_size;

    dr_close_file(f);
    return true;
}

static bool
try_parse_range(IN char *str, OUT range_t *range)
{
    char *endptr;
    uint32_t start, end;

    errno = 0;
    start = strtou(str, &endptr, 10);
    if (endptr == str || errno != 0) {
        ASSERT(false, "failed to parse range start from %s", str);
        return false;
    }
    if (*endptr == '-') {
        errno = 0;
        end = strtou(endptr + 1, &endptr, 10);
        if (endptr == str + 1 || errno != 0) {
            ASSERT(false, "failed to parse range end from %s", str);
            return false;
        }
    } else {
        end = start;
    }
    range->start = start;
    range->end = end;
    return true;
}

static bool
init_pt_pmu_type()
{
    char *type_str = NULL;
    uint64_t type_str_size = 0;

    if (!read_file_to_buffer(PT_PMU_TYPE_FILE, &type_str, &type_str_size)) {
        ASSERT(false, "failed to read file %s to buffer", PT_PMU_TYPE_FILE);
        return false;
    }
    errno = 0;
    pt_pmu_type = atoi(type_str);
    if (errno != 0) {
        ASSERT(false, "failed to parse pmu type from %s", type_str);
        dr_global_free(type_str, type_str_size);
        return false;
    }
    dr_global_free(type_str, type_str_size);
    return true;
}

static bool
init_pt_config_terms_table()
{
    DIR *dir = opendir(PT_PMU_EVENT_CONFIG_DIR);
    if (dir == NULL) {
        ASSERT(false, "failed to open directory %s", PT_PMU_EVENT_CONFIG_DIR);
        return false;
    }
    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        char *filename = ent->d_name;
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            continue;
        }
        char *config_str = NULL;
        uint64_t config_str_size = 0;
        char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", PT_PMU_EVENT_CONFIG_DIR, filename);
        if (!read_file_to_buffer(file_path, &config_str, &config_str_size)) {
            ASSERT(false, "failed to read file %s to buffer", file_path);
            continue;
        }
        int range_start_index = strlen("config:");
        if (config_str_size < range_start_index) {
            ASSERT(false, "failed to parse config from %s", config_str);
            dr_global_free(config_str, config_str_size);
            continue;
        }
        range_t range;
        range.range = 0;
        if (!try_parse_range(config_str + range_start_index, &range)) {
            ASSERT(false, "failed to parse range from %s", config_str);
            dr_global_free(config_str, config_str_size);
            continue;
        }
        hashtable_add(&pt_config_terms_table, filename, &range);
    }
}

static int
perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,
                unsigned long flags)
{
    return syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
}

/***************************************************************************
 * FORWARD DECLS
 */

static void
drpttracer_thread_init(void *drcontext);

static void
drpttracer_thread_exit(void *drcontext);

/***************************************************************************
 * INIT
 */

static int drpttracer_init_count;

DR_EXPORT
bool
drpttracer_init(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drpttracer_init_count, 1);
    if (count > 1)
        return true;
    if (!init_pt_pmu_type()) {
        ASSERT(false, "failed to init pt pmu type");
        dr_atomic_add32_return_sum(&drpttracer_init_count, -1);
        return false;
    }
    hashtable_init(&pt_config_terms_table, PT_CONFIG_TERMS_TABLE_HASH_BITS, HASH_STRING,
                   true);
    if (!init_pt_config_terms_table()) {
        ASSERT(false, "failed to init pt config terms table");
        dr_atomic_add32_return_sum(&drpttracer_init_count, -1);
        hashtable_delete(&pt_config_terms_table);
        return false;
    }

    drmgr_init();
    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;
    if (!drmgr_register_thread_init_event(drpttracer_thread_init))
        return false;
    if (!drmgr_register_thread_exit_event(drpttracer_thread_exit))
        return false;

    return true;
}

DR_EXPORT
void
drpttracer_exit(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drpttracer_init_count, -1);
    if (count != 0)
        return;

    if (!drmgr_unregister_thread_init_event(drpttracer_thread_init) ||
        !drmgr_unregister_thread_exit_event(drpttracer_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx))
        ASSERT(false, "failed to unregister in drpttracer_exit");

    hashtable_delete(&pt_config_terms_table);

    drmgr_exit();
}

static void
drpttracer_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
drpttracer_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

/***************************************************************************
 * INTEL PT TRACING
 */

drpttracer_status_t
drpttracer_start_trace(void *drcontext, bool exclude_user, bool exclude_kernel)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt->pfd != -1)
        return DRPTTRACER_STATUS_ALREADY_STARTED;
}
