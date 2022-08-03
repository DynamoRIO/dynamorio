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
#include <linux/perf_event.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../../core/unix/include/syscall_linux_x86.h" // for SYS_perf_event_open
#include "dr_api.h"
#include "drpttracer.h"

#if !defined(X86_64) || !defined(LINUX)
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

/* drpttracer supports multiple types of tracing. To avoid the overhead of re-initializing
 * perf_event_attr for each request, drpttracer stores all tracing types' perf_event_attr
 * as global static variables, then drpttracer can directly use them when the caller wants
 * to start tracing.
 */
static struct perf_event_attr user_only_pe_attr;
static struct perf_event_attr kernel_only_pe_attr;
static struct perf_event_attr user_kernel_pe_attr;

/* pt_shared_metadata is used to store shared meta data: cpu_family, cpu_model,
 * cpu_stepping.
 */
static pt_metadata_t pt_shared_metadata;

/***************************************************************************
 * UTILITY FUNCTIONS
 */

/* This function will use dr_global_alloc() to allocate the buffer. So the caller needs to
 * use dr_global_free() to free the buffer.
 */
static bool
read_file_to_buf(IN const char *filename, OUT char **buf, OUT uint64_t *buf_size)
{
    ASSERT(filename != NULL, "filename is NULL");
    ASSERT(buf != NULL, "buf is NULL");
    ASSERT(buf_size != NULL, "buf_size is NULL");
    char *local_buf = NULL;
    uint64_t local_buf_size = 0;
    file_t f = dr_open_file(filename, DR_FILE_READ);
    if (f == INVALID_FILE) {
        ERRMSG("failed to open file %s\n", filename);
        return false;
    }

    if (!dr_file_size(f, &local_buf_size)) {
        ERRMSG("failed to get file size of %s\n", filename);
        dr_close_file(f);
        return false;
    }

    local_buf = (char *)dr_global_alloc(local_buf_size);
    /* XXX: When read files in /sys/devices/intel_pt/, the value returned from
     * dr_read_file(file_size) doesn't equal dr_file_size().
     */
    if (dr_read_file(f, local_buf, local_buf_size) == 0) {
        ERRMSG("failed to read file %s\n", filename);
        dr_global_free(local_buf, local_buf_size);
        dr_close_file(f);
        return false;
    }

    *buf = local_buf;
    *buf_size = local_buf_size;

    dr_close_file(f);
    return true;
}

/***************************************************************************
 * PMU CONFIG PARSING FUNCTIONS
 */

#define PT_PMU_PERF_TYPE_FILE "/sys/devices/intel_pt/type"
#define PT_PMU_EVENTS_CONFIG_DIR "/sys/devices/intel_pt/format"

/* This function is used to get the Intel PT's PMU type. */
static bool
parse_pt_pmu_type(OUT uint32_t *pmu_type)
{
    ASSERT(pmu_type != NULL, "pmu_type is NULL");
    /* Read type str from /sys/devices/intel_pt/type. */
    char *buf = NULL;
    uint64_t buf_size = 0;
    if (!read_file_to_buf(PT_PMU_PERF_TYPE_FILE, &buf, &buf_size)) {
        ERRMSG("failed to read file " PT_PMU_PERF_TYPE_FILE " to buffer\n");
        return false;
    }

    /* Parse type str to get the PMU type. */
    uint32_t type = 0;
    int ret = dr_sscanf(buf, "%" PRIu32, &type);
    if (ret < 1) {
        ERRMSG("failed to parse pmu type from file " PT_PMU_PERF_TYPE_FILE "\n");
        dr_global_free(buf, buf_size);
        return false;
    }
    *pmu_type = type;

    dr_global_free(buf, buf_size);
    return true;
}

/* This function is used to set the PMU event's config.
 * It first gets the bit field of the 'name' in perf_event_attr.config and then
 * sets the 'val' to the corresponding bit.
 */
static bool
parse_pt_pmu_event_config(IN const char *name, uint64_t val, OUT uint64_t *config)
{
    /* Read config str from /sys/devices/intel_pt/format/name. */
    char *buf = NULL;
    uint64_t buf_size = 0;
    char file_path[MAXIMUM_PATH];
    dr_snprintf(file_path, MAXIMUM_PATH, PT_PMU_EVENTS_CONFIG_DIR "/%s", name);
    if (!read_file_to_buf(file_path, &buf, &buf_size)) {
        ERRMSG("failed to read file %s to buffer\n", file_path);
        return false;
    }

    /* Parse config str to get the bit field of the 'name'. */
    int start = 0, end = 0;
    int ret = dr_sscanf(buf, "config:%d-%d", &start, &end);
    if (ret < 1) {
        ERRMSG("failed to parse event's config bit fields from the format file %s\n",
               file_path);
        dr_global_free(buf, buf_size);
        return false;
    }
    if (ret == 1) {
        end = start;
    }

#define BITS(x) ((x) == 64 ? -1ULL : (1ULL << (x)) - 1)
    /* Set the 'val' in the corresponding bit. */
    uint64_t mask = BITS(end - start + 1);
    if ((val & ~mask) > 0) {
        ERRMSG("pmu event's config value %" PRIu64 " is out of range\n", val);
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

/* This function is used to initialize one perf_event_attr.
 * For kernel only tracing:
 *  The perf_event_attr is set to be the same as running
 *  'perf record -e intel_pt/noretcomp/k'
 * For user only tracing:
 *  The perf_event_attr is set to be the same as running
 *  'perf record -e intel_pt/cyc,noretcomp/u'
 * For both kernel and user tracing:
 *  The perf_event_attr is set to be the same as running
 *  'perf record -e intel_pt/cyc,noretcomp/uk'
 *
 * "Why do we need to set cyc in user-only or kernel & user mode?"
 *
 * When we set cyc, Intel PT will record cyc packets in the PT data. It provides timestamp
 * information that can help us to synchronize the PT's sideband record data with PT data.
 * Such as, we need to sync the mmap and mmap2 events with PT trace. So that we can ensure
 * the PT decoder can find the right images.
 *
 * "Why do we need to set noretcomp?"
 *
 * When we set noretcomp, Intel PT will disable the "return compression" feature. Then
 * when a function returns, a TIP packet is produced. This setting lets Intel PT produce
 * more packets, making the decoding process more reliable.
 */
static bool
pt_perf_event_attr_init(bool user, bool kernel, OUT struct perf_event_attr *attr)
{
    ASSERT(attr != NULL, "attr is NULL");
    ASSERT(user || kernel, "user and kernel are all false");
    memset(attr, 0, sizeof(struct perf_event_attr));
    attr->size = sizeof(struct perf_event_attr);

    /* Set the event type. */
    if (!parse_pt_pmu_type(&attr->type)) {
        ERRMSG("failed to parse PT's pmu type\n");
        return false;
    }

    /* Set the event config. */
    uint64_t config = 0;
    if (!parse_pt_pmu_event_config("noretcomp", 1, &config)) {
        ERRMSG("failed to parse PT's pmu event noretcomp to perf_event_attr.config\n");
        return false;
    }
    if (user) {
        if (!parse_pt_pmu_event_config("cyc", 1, &config)) {
            ERRMSG("failed to parse PT's pmu event cyc to perf_event_attr.config\n");
            return false;
        }
    }
    attr->config = config;

    attr->exclude_hv = 1;
    if (!kernel) {
        attr->exclude_kernel = 1;
    }
    if (!user) {
        attr->exclude_user = 1;
    }
    attr->disabled = 1;
    return true;
}

/* This function is used to open a perf event.
 */
static int
perf_event_open(IN struct perf_event_attr *attr, IN pid_t pid, IN int cpu,
                IN int group_fd, IN unsigned long flags)
{
    errno = 0;
    int fd = syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
    if (fd < 0 || errno != 0) {
        ERRMSG("failed to open perf event: %s\n", strerror(errno));
        return -1;
    }
    return fd;
}

/***************************************************************************
 * PT META DATA RELATED FUNCTIONS
 */

/* This function is used to get the PT's shared meta data:
 *  cpu_family, cpu_model, cpu_step
 */
static void
pt_shared_metadata_init(pt_metadata_t *metadata)
{
    memset(metadata, 0, sizeof(pt_metadata_t));
    metadata->cpu_family = (uint16_t)proc_get_family();
    metadata->cpu_model = (uint8_t)proc_get_model();
    metadata->cpu_stepping = (uint8_t)proc_get_stepping();
}

/***************************************************************************
 * PT TRACER HANDLE RELATED FUNCTIONS
 */

/* The PT trace handle. The handle is used to store information about the currently active
 * tracing. And the handle's data fields are opaque to the caller.
 */
typedef struct _pttracer_handle_t {
    /* The file descriptor of the perf event. */
    int fd;

    /* The size of the perf event file's mmap pages.
     *  base_size = sizeof(header page) + sizeof(event ring buffer)
     */
    int base_size;
    union {
        /* The perf event file's mmap pages.
         * The mmap pages stores the header page and the perf event's ring buffer.
         */
        void *base;
        /* The header of the perf event file's mmap pages.
         * The header is located at the beginning of the mmap pages.
         * And it stores the all ring buffers' offset, size, head and tail.
         * Also, it contains the perf event's meta data:
         *  time_shift, time_mult, and time_zero.
         */
        struct perf_event_mmap_page *header;
    };
    /* The ring buffer of the perf event's auxiliary data.
     *  sizeof(aux ring buffer) = header->aux_size.
     */
    void *aux;
    /* The mode of the tracing. */
    drpttracer_tracing_mode_t tracing_mode;
} pttracer_handle_t;

/* This function is used to generate a pttracer_handle_t from the perf event file
 * descriptor.
 */
static pttracer_handle_t *
pttracer_handle_create(IN void *drcontext, IN int fd, IN uint pt_size_shift,
                       IN uint sideband_size_shift,
                       IN drpttracer_tracing_mode_t tracing_mode)
{
    if (pt_size_shift == 0 || sideband_size_shift == 0) {
        ERRMSG("pt_size_shift and sideband_size_shift must be greater than 0\n");
        return NULL;
    }

    /* Mmap the perf event file. */
    uint64_t base_size = (uint64_t)((1UL << sideband_size_shift) + 1) * dr_page_size();
    uint64_t base_mmap_size = base_size;
    void *base =
        dr_map_file(fd, &base_mmap_size, 0, NULL, DR_MEMPROT_READ | DR_MEMPROT_WRITE, 0);
    if (base == NULL || base_mmap_size != base_size) {
        ERRMSG("failed to mmap perf event for pt tracing\n");
        dr_close_file(fd);
        return NULL;
    }

    /* Get the header of the perf event file's mmap pages. */
    struct perf_event_mmap_page *header = (struct perf_event_mmap_page *)base;
    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size = (uint64_t)(1UL << pt_size_shift) * dr_page_size();

    /* Mmap the ring buffer of the perf event's auxiliary data. */
    uint64_t aux_mmap_size = header->aux_size;
    void *aux =
        dr_map_file(fd, &aux_mmap_size, header->aux_offset, NULL, DR_MEMPROT_READ, 0);
    if (aux == NULL || aux_mmap_size != header->aux_size) {
        ERRMSG("failed to mmap aux for pt tracing\n");
        dr_unmap_file(base, base_size);
        dr_close_file(fd);
        return NULL;
    }

    /* Alloc the handle and set it. */
    pttracer_handle_t *handle =
        (pttracer_handle_t *)dr_thread_alloc(drcontext, sizeof(pttracer_handle_t));
    handle->fd = fd;
    handle->base = base;
    handle->base_size = base_size;
    handle->aux = aux;
    handle->tracing_mode = tracing_mode;
    return handle;
}

/* This function is used to free a pttracer_handle_t.
 */
static void
pttracer_handle_free(IN void *drcontext, pttracer_handle_t *handle)
{
    if (handle == NULL)
        return;
    if (handle->fd >= 0)
        dr_close_file(handle->fd);
    if (handle->aux != NULL)
        dr_unmap_file(handle->aux, handle->header->aux_size);
    if (handle->base != NULL)
        dr_unmap_file(handle->base, handle->base_size);

    dr_thread_free(drcontext, handle, sizeof(pttracer_handle_t));
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
    pt_shared_metadata_init(&pt_shared_metadata);
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

    /* nothing yet: but putting in API up front in case need later */
}

/***************************************************************************
 * PT TRACING APIS
 */

DR_EXPORT
drpttracer_status_t
drpttracer_start_tracing(IN void *drcontext, IN drpttracer_tracing_mode_t mode,
                         IN uint pt_size_shift, IN uint sideband_size_shift,
                         OUT void **tracer_handle)
{
    ASSERT(tracer_handle != NULL, "tracer_handle is NULL");
    /* Select one perf_event_attr for current tracing. */
    struct perf_event_attr attr;
    if (mode == DRPTTRACER_TRACING_ONLY_USER) {
        attr = user_only_pe_attr;
    } else if (mode == DRPTTRACER_TRACING_ONLY_KERNEL) {
        attr = kernel_only_pe_attr;
    } else if (mode == DRPTTRACER_TRACING_USER_AND_KERNEL) {
        attr = user_kernel_pe_attr;
    } else {
        ASSERT(false, "invalid tracing mode");
        return DRPTTRACER_ERROR_INVALID_PARAMETER;
    }

    /* Open perf event. */
    int perf_event_fd = perf_event_open(&attr, dr_get_thread_id(drcontext), -1, -1, 0);
    if (perf_event_fd < 0) {
        ASSERT(false, "failed to open perf event");
        return DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT;
    }

    /* Initialize the pttracer_handle_t. */
    pttracer_handle_t *handle = pttracer_handle_create(
        drcontext, perf_event_fd, pt_size_shift, sideband_size_shift, mode);
    if (handle == NULL) {
        ASSERT(false, "failed to create pttracer_handle");
        return DRPTTRACER_ERROR_FAILED_TO_CREATE_PTTRACER_HANDLE;
    }

    /* Start the pt tracing. */
    errno = 0;
    if (ioctl(perf_event_fd, PERF_EVENT_IOC_RESET, 0) < 0 ||
        ioctl(perf_event_fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        ERRMSG("Error Message: %s\n", strerror(errno));
        ASSERT(false, "failed to start tracing");
        pttracer_handle_free(drcontext, handle);
        return DRPTTRACER_ERROR_FAILED_TO_START_TRACING;
    }

    *(pttracer_handle_t **)tracer_handle = handle;
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_end_tracing(IN void *drcontext, INOUT void *tracer_handle,
                       OUT drpttracer_output_t **output)
{
    pttracer_handle_t *handle = (pttracer_handle_t *)tracer_handle;
    if (handle == NULL || handle->fd < 0) {
        ASSERT(false, "invalid pttracer handle");
        return DRPTTRACER_ERROR_INVALID_PARAMETER;
    }

    if (output == NULL) {
        ASSERT(false, "invalid output");
        return DRPTTRACER_ERROR_INVALID_PARAMETER;
    }

    /* Stop the pt tracing. */
    errno = 0;
    if (ioctl(handle->fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
        ERRMSG("error Message: %s\n", strerror(errno));
        ASSERT(false, "failed to stop tracing");
        return DRPTTRACER_ERROR_FAILED_TO_STOP_TRACING;
    }

    *output =
        (drpttracer_output_t *)dr_thread_alloc(drcontext, sizeof(drpttracer_output_t));
    memset(*output, 0, sizeof(drpttracer_output_t));

    memcpy(&(*output)->metadata, &pt_shared_metadata, sizeof(pt_metadata_t));
    (*output)->metadata.time_shift = handle->header->time_shift;
    (*output)->metadata.time_mult = handle->header->time_mult;
    (*output)->metadata.time_zero = handle->header->time_zero;

    if (handle->header->aux_head > handle->header->aux_size) {
        ERRMSG("the buffer is full and the new PT data is overwritten\n");
        ASSERT(false, "aux_head > aux_size");
        return DRPTTRACER_ERROR_OVERWRITTEN_PT_TRACE;
    }

    (*output)->pt_buf_size = handle->header->aux_head - handle->header->aux_tail;
    (*output)->pt_buf = dr_thread_alloc(drcontext, (*output)->pt_buf_size);
    memcpy((*output)->pt_buf, (uint8_t *)handle->aux, (*output)->pt_buf_size);
    if (handle->tracing_mode == DRPTTRACER_TRACING_ONLY_USER ||
        handle->tracing_mode == DRPTTRACER_TRACING_USER_AND_KERNEL) {
        if (handle->header->data_head > handle->header->data_size) {
            ERRMSG("the buffer is full and the new PT's sideband data is overwritten\n");
            ASSERT(false, "data_head > data_size");
            return DRPTTRACER_ERROR_OVERWRITTEN_SIDEBAND_DATA;
        }
        (*output)->sideband_buf_size =
            handle->header->data_head - handle->header->data_tail;
        (*output)->sideband_buf =
            dr_thread_alloc(drcontext, (*output)->sideband_buf_size);
        memcpy((*output)->sideband_buf,
               (uint8_t *)handle->base + handle->header->data_tail,
               (*output)->sideband_buf_size);
    } else {
        /* Even when tracing only kernel instructions, there is some sideband data.
         * Because we don't need this data in future processes, we discard it.
         */
        (*output)->sideband_buf = NULL;
        (*output)->sideband_buf_size = 0;
    }

    pttracer_handle_free(drcontext, handle);
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_destroy_output(IN void *drcontext, IN drpttracer_output_t *output)
{
    if (output == NULL) {
        ASSERT(false, "trying to free NULL output buffer");
        return DRPTTRACER_ERROR_INVALID_PARAMETER;
    }
    if (output->pt_buf == NULL) {
        ASSERT(false, "trying to free NULL PT buffer");
        return DRPTTRACER_ERROR_INVALID_PARAMETER;
    }
    dr_thread_free(drcontext, output->pt_buf, output->pt_buf_size);
    if (output->sideband_buf != NULL) {
        dr_thread_free(drcontext, output->sideband_buf, output->sideband_buf_size);
    }
    dr_thread_free(drcontext, output, sizeof(drpttracer_output_t));
    return DRPTTRACER_SUCCESS;
}
