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

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../../core/unix/include/syscall_linux_x86.h" // for SYS_perf_event_open
#include "dr_api.h"
#include "drpttracer.h"

#if !defined(X86_64) && !defined(LINUX)
#    error "This is only for Linux x86_64."
#endif

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
#    define ASSERT(x, msg) /* nothing */
#endif

#ifdef DEBUG
#    define ERRMSG(...)                      \
        do {                                 \
            dr_fprintf(STDERR, __VA_ARGS__); \
        } while (0)
#else
#    define ERRMSG(...) /* nothing */
#endif

/***************************************************************************
 * UTTILITY FUNCTIONS
 */

static bool
read_file_to_buf(IN const char *filename, OUT char **buf, OUT uint64_t *buf_size)
{
    char *local_buf = NULL;
    uint64_t local_buf_size = 0;
    file_t f = dr_open_file(filename, DR_FILE_READ);
    if (f == INVALID_FILE) {
        ERRMSG("failed to open file %s", filename);
        return false;
    }

    if (!dr_file_size(f, &local_buf_size)) {
        ERRMSG("failed to get file size of %s", filename);
        dr_close_file(f);
        return false;
    }

    local_buf = (char *)dr_global_alloc(local_buf_size);
    if (!dr_read_file(f, local_buf, local_buf_size)) {
        ERRMSG("failed to read file %s", filename);
        dr_global_free(local_buf, local_buf_size);
        dr_close_file(f);
        return false;
    }

    *buf = local_buf;
    *buf_size = local_buf_size;

    dr_close_file(f);
    return true;
}

static void
read_ring_buf_to_buf(IN void *base, IN uint64_t base_size, IN uint64_t head,
                     IN uint64_t tail, INOUT drpttracer_data_buf_t *data_buf)
{
    if (data_buf == NULL) {
        ERRMSG("try to read from NULL buffer\n");
        return;
    }
    uint64_t head_offs = head % base_size;
    uint64_t tail_offs = tail % base_size;
    if (head_offs > tail_offs) {
        data_buf->buf_size = head_offs - tail_offs;
        data_buf->buf = (char *)dr_global_alloc(data_buf->buf_size);
        memcpy(data_buf->buf, (char *)base + tail_offs, data_buf->buf_size);
    } else if (head_offs < tail_offs) {
        data_buf->buf_size = head_offs + base_size - tail_offs;
        data_buf->buf = (char *)dr_global_alloc(data_buf->buf_size);
        memcpy(data_buf->buf, (char *)base + tail_offs, base_size - tail_offs);
        memcpy((char *)data_buf->buf + base_size - tail_offs, (char *)base, head_offs);
    } else {
        data_buf->buf = NULL;
        data_buf->buf_size = 0;
    }
}

/***************************************************************************
 * PMU CONFIG PARSING FUNCTIONS
 */

#define PT_PMU_PERF_TYPE_FILE "/sys/devices/intel_pt/type"
#define PT_PMU_EVENTS_CONFIG_DIR "/sys/devices/intel_pt/format"

static bool
parse_pt_pmu_type(OUT uint32_t *pmu_type)
{
    char *buf = NULL;
    uint64_t buf_size = 0;
    if (!read_file_to_buf(PT_PMU_PERF_TYPE_FILE, &buf, &buf_size)) {
        ERRMSG("failed to read file " PT_PMU_PERF_TYPE_FILE " to buffer\n");
        return false;
    }

    uint32_t type = 0;
    int ret = dr_sscanf(buf, "%" PRIu32, &type);
    if (ret < 1) {
        ERRMSG("failed to parse pmu type from file " PT_PMU_PERF_TYPE_FILE);
        dr_global_free(buf, buf_size);
        return false;
    }

    *pmu_type = type;
    ERRMSG("pmu type is %" PRIu32 "\n", *pmu_type);
    dr_global_free(buf, buf_size);
    return true;
}

#define BITS(x) ((x) == 64 ? -1ULL : (1ULL << (x)) - 1)
static bool
parse_pt_pmu_event_config(IN const char *name, uint64_t val, OUT uint64_t *config)
{
    char *buf = NULL;
    uint64_t buf_size = 0;
    char file_path[MAXIMUM_PATH];
    dr_snprintf(file_path, MAXIMUM_PATH, PT_PMU_EVENTS_CONFIG_DIR "/%s", name);
    if (!read_file_to_buf(file_path, &buf, &buf_size)) {
        ERRMSG("failed to read file %s to buffer", file_path);
        return false;
    }
    int start = 0, end = 0;
    int ret = dr_sscanf(buf, "config:%d-%d", &start, &end);
    if (ret < 1) {
        ERRMSG("failed to parse event's config bit fields from the format file %s",
               file_path);
        dr_global_free(buf, buf_size);
        return false;
    }
    if (ret == 1) {
        end = start;
    }
    uint64_t mask = BITS(end - start + 1);
    if ((val & ~mask) > 0) {
        ERRMSG("pmu event's config value %" PRIu64 " is out of range", val);
        dr_global_free(buf, buf_size);
        return false;
    }

    *config |= (val & mask) << start;
    dr_global_free(buf, buf_size);
    return true;
}

/***************************************************************************
 * PERF EVENT RELATED FUNCTIONS
 */

static struct perf_event_attr user_only_pe_attr;
static struct perf_event_attr kernel_only_pe_attr;
static struct perf_event_attr user_kernel_pe_attr;

static bool
pt_perf_event_attr_init(bool user, bool kernel, OUT struct perf_event_attr *attr)
{
    memset(attr, 0, sizeof(struct perf_event_attr));
    attr->size = sizeof(struct perf_event_attr);
    if (!parse_pt_pmu_type(&attr->type)) {
        ERRMSG("failed to parse PT's pmu type\n");
        return false;
    }
    uint64_t config = 0;
    if (kernel) {
        attr->exclude_user = 1;
        attr->exclude_hv = 1;
        if (!parse_pt_pmu_event_config("noretcomp", 1, &config)) {
            ERRMSG(
                "failed to parse PT's pmu event noretcomp to perf_event_attr.config\n");
            return false;
        }
    }
    if (user) {
        attr->exclude_kernel = 1;
        attr->exclude_hv = 1;
        if (!parse_pt_pmu_event_config("cyc", 1, &config)) {
            ERRMSG("failed to parse PT's pmu event cyc to perf_event_attr.config\n");
            return false;
        }
    }
    ERRMSG("config: %" PRIx64 "\n", config);
    attr->config = config;
    attr->disabled = 1;
    return true;
}

static bool
perf_event_open(IN struct perf_event_attr *attr, IN pid_t pid, IN int cpu,
                IN int group_fd, IN unsigned long flags, OUT int *perf_event_fd)
{
    int user_errno = errno;
    errno = 0;
    int fd = syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
    if (fd < 0 || errno != 0) {
        ERRMSG("failed to open perf event: %s\n", strerror(errno));
        return false;
    }
    ERRMSG("perf event opened: %d\n", fd);
    *perf_event_fd = fd;
    errno = user_errno;
    return true;
}

static unsigned
perf_data_mmap_size(int base_size_shift)
{
    return ((1U << base_size_shift) + 1) * sysconf(_SC_PAGESIZE);
}

static unsigned
perf_aux_mmap_size(int aux_size_shift)
{
    return (1U << aux_size_shift) * sysconf(_SC_PAGESIZE);
}

/***************************************************************************
 * PT TRACER HANDLE RELATED FUNCTIONS
 */

typedef struct _pttracer_handle_t {
    int fd;
    int base_size;
    struct perf_event_mmap_page *header;
    void *aux;
} pttracer_handle_t;

/* 2^n size of event ring buffer (in pages) */
#define BASE_SIZE_SHIFT 8
#define AUX_SIZE_SHIFT 8

static bool
pttracer_handle_create(IN int fd, OUT pttracer_handle_t **handle)
{
    uint64_t base_mmap_size = perf_data_mmap_size(BASE_SIZE_SHIFT);
    uint64_t base_size = base_mmap_size;
    // struct perf_event_mmap_page *mpage = (struct perf_event_mmap_page *)dr_map_file(
    //     fd, &base_size, 0, NULL, DR_MEMPROT_READ | DR_MEMPROT_WRITE, 0);
    // if (mpage == NULL || base_size != base_mmap_size) {
    struct perf_event_mmap_page *mpage = (struct perf_event_mmap_page *)mmap(
        NULL, base_size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
    if (mpage == (struct perf_event_mmap_page *)MAP_FAILED) {
        ERRMSG("failed to mmap perf event for pt tracing\n");
        dr_close_file(fd);
        return false;
    }

    uint64_t aux_offset = mpage->data_offset + mpage->data_size;
    uint64_t aux_mmap_size = perf_aux_mmap_size(AUX_SIZE_SHIFT);
    uint64_t aux_size = aux_mmap_size;
    ERRMSG("pttracer_handle_create: fd = %d, base_size=%" PRIu64 " aux_offset = % " PRIu64
           " aux_size = %" PRIu64 "\n",
           fd, base_mmap_size, aux_offset, aux_size);
    errno = 0;
    void *aux =
        mmap(NULL, (size_t)aux_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, aux_offset);
    if (aux == MAP_FAILED) {
        ERRMSG("failed to mmap perf event for pt tracing %s\n", strerror(errno));
        dr_close_file(fd);
        return false;
    }
    // void *aux = dr_map_file(fd, &aux_size, mpage->aux_offset, NULL,
    //                         DR_MEMPROT_READ | DR_MEMPROT_WRITE, 0);
    // if (mpage == NULL || aux_size != aux_mmap_size) {
    //     ERRMSG("failed to mmap aux for pt tracing\n");
    //     dr_unmap_file(mpage, base_size);
    //     dr_close_file(fd);
    //     return false;
    // }
    mpage->aux_offset = aux_offset;
    mpage->aux_size = aux_mmap_size;

    ERRMSG("pttracer_handle_create: base_size=%d, aux_size=%d\n", mpage->data_size,
           mpage->aux_size);

    *handle = (pttracer_handle_t *)(sizeof(pttracer_handle_t));
    (*handle)->fd = fd;
    (*handle)->base_size = base_size;
    (*handle)->header = mpage;
    (*handle)->aux = aux;
    return true;
}

static void
pttracer_handle_free(pttracer_handle_t *handle)
{
    if (handle == NULL)
        return;
    if (handle->fd >= 0)
        dr_close_file(handle->fd);
    if (handle->aux != NULL)
        dr_unmap_file(handle->aux, handle->header->aux_size);
    if (handle->header != NULL)
        dr_unmap_file(handle->header, handle->base_size);

    dr_global_free(handle, sizeof(pttracer_handle_t));
}

/***************************************************************************
 * INIT APIS
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
    if (!(pt_perf_event_attr_init(true, false, &user_only_pe_attr) &&
          pt_perf_event_attr_init(false, true, &kernel_only_pe_attr) &&
          pt_perf_event_attr_init(true, true, &user_kernel_pe_attr))) {
        ASSERT(false, "failed to init drpttracer's perf_event_attr");
        drpttracer_exit();
        return false;
    }
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
}

/***************************************************************************
 * PT TRACING APIS
 */

DR_EXPORT
drpttracer_status_t
drpttracer_start_tracing(IN thread_id_t thread_id, IN bool user, IN bool kernel,
                         OUT void **tracer_handle)
{
    struct perf_event_attr *attr = NULL;
    if (user && kernel) {
        attr = &user_kernel_pe_attr;
    } else if (user) {
        attr = &user_only_pe_attr;
    } else if (kernel) {
        attr = &kernel_only_pe_attr;
    } else {
        ASSERT(false, "invalid arguments");
        return DRPTTRACER_ERROR_INVALID_ARGUMENT;
    }
    attr->disabled = 1;

    ERRMSG("drpttracer_start_tracing: thread_id = %d, user = %d, kernel = %d\n",
           thread_id, user, kernel);

    int perf_event_fd = -1;
    if (!perf_event_open(attr, getpid(), -1, -1, 0, &perf_event_fd)) {
        ASSERT(false, "failed to open perf event");
        return DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT;
    }

    if (!pttracer_handle_create(perf_event_fd, (pttracer_handle_t **)tracer_handle)) {
        ASSERT(false, "failed to create pttracer_handle");
        return DRPTTRACER_ERROR_FAILED_TO_CREATE_PTTRACER_HANDLE;
    }

    ioctl(perf_event_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_event_fd, PERF_EVENT_IOC_ENABLE, 0);
    return DRPTTRACER_SUCCESS;
}
DR_EXPORT
drpttracer_status_t
drpttracer_end_tracing(IN void **tracer_handle, OUT drpttracer_data_buf_t *pt_data,
                       OUT drpttracer_data_buf_t *sideband_data)
{
    pttracer_handle_t *handle = (pttracer_handle_t *)*tracer_handle;
    if (handle == NULL || handle->fd < 0) {
        ASSERT(false, "invalid arguments");
        return DRPTTRACER_ERROR_INVALID_ARGUMENT;
    }
    ioctl(handle->fd, PERF_EVENT_IOC_DISABLE, 0);
    close(handle->fd);
    if (pt_data != NULL) {
        read_ring_buf_to_buf(handle->aux, handle->header->aux_size,
                             handle->header->aux_head, handle->header->aux_tail, pt_data);
    }
    if (sideband_data != NULL) {
        read_ring_buf_to_buf(handle->header, handle->header->data_size,
                             handle->header->data_head, handle->header->data_tail,
                             sideband_data);
    }
    pttracer_handle_free(handle);
    *tracer_handle = NULL;
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_create_data_buffer(OUT drpttracer_data_buf_t **data_buf)
{
    *data_buf = (drpttracer_data_buf_t *)dr_global_alloc(sizeof(drpttracer_data_buf_t));
    memset(*data_buf, 0, sizeof(drpttracer_data_buf_t));
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_delete_data_buffer(IN drpttracer_data_buf_t **data_buf)
{
    if (*data_buf == NULL) {
        ASSERT(false, "try to delete NULL buffer");
        return DRPTTRACER_ERROR_TRY_TO_DELETE_NULL_BUFFER;
    }
    dr_global_free((*data_buf)->buf, (*data_buf)->buf_size);
    dr_global_free((*data_buf), sizeof(drpttracer_data_buf_t));
    *data_buf = NULL;
    return DRPTTRACER_SUCCESS;
}
