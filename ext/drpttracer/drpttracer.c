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
#include <inttypes.h>
#include <linux/perf_event.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
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
        ASSERT(false, "failed to open file");
        return false;
    }

    if (dr_file_size(f, &local_buf_size) != 0) {
        ASSERT(false, "failed to get file size");
        dr_close_file(f);
        return false;
    }

    local_buf = (char *)dr_global_alloc(local_buf_size);
    if (dr_read_file(f, local_buf, local_buf_size)) {
        ASSERT(false, "failed to read file");
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
read_ring_buf_to_buf(IN void *base, IN __u64 base_size, IN __u64 head, IN __u64 tail,
                     INOUT drpttracer_data_buf_t *data_buf)
{
    if (data_buf == NULL) {
        ASSERT(false, "buf is NULL");
        return;
    }
    __u64 head_offs = head % base_size;
    __u64 tail_offs = tail % base_size;
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
parse_pt_pmu_type(OUT __u32 *pmu_type)
{
    char *buf = NULL;
    uint64_t buf_size = 0;
    if (!read_file_to_buf(PT_PMU_PERF_TYPE_FILE, &buf, &buf_size)) {
        ASSERT(false, "failed to read file " PT_PMU_PERF_TYPE_FILE " to buffer");
        return false;
    }

    errno = 0;
    int type = atoi(buf);
    if (errno != 0) {
        ASSERT(false, "failed to parse pmu type");
        dr_global_free(buf, buf_size);
        return false;
    }

    *pmu_type = (__u32)type;
    dr_global_free(buf, buf_size);
    return true;
}

#define BITS(x) ((x) == 64 ? -1ULL : (1ULL << (x)) - 1)
static bool
parse_pt_pmu_event_config(IN const char *name, __u64 val, OUT __u64 *config)
{
    char *buf = NULL;
    uint64_t buf_size = 0;
    char file_path[MAXIMUM_PATH];
    snprintf(file_path, MAXIMUM_PATH, PT_PMU_EVENTS_CONFIG_DIR "/%s", name);
    if (!read_file_to_buf(file_path, &buf, &buf_size)) {
        ASSERT(false, "failed to read file to buffer");
        return false;
    }
    int start = 0, end = 0;
    int ret = dr_sscanf(buf, "config:%d-%d", &start, &end);
    if (ret < 1) {
        ASSERT(false, "failed to parse event's config bit fields from the format file");
        dr_global_free(buf, buf_size);
        return false;
    }
    if (ret == 1) {
        end = start;
    }
    __u64 mask = BITS(end - start + 1);
    if ((val & ~mask) > 0) {
        ASSERT(false, "pmu event's config value is out of range");
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
        ASSERT(false, "failed to parse PT's pmu type");
        return false;
    }
    if (kernel) {
        attr->exclude_user = 1;
        attr->exclude_hv = 1;
        if (!parse_pt_pmu_event_config("noretcomp", 1, &attr->config)) {
            ASSERT(false,
                   "failed to parse PT's pmu event noretcomp to perf_event_attr.config");
            return false;
        }
    }
    if (user) {
        attr->exclude_kernel = 1;
        attr->exclude_hv = 1;
        if (!parse_pt_pmu_event_config("cyc", 1, &attr->config)) {
            ASSERT(false, "failed to parse PT's pmu event cyc to perf_event_attr.config");
            return false;
        }
    }

    attr->disabled = 1;
    return true;
}

static int
perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,
                unsigned long flags)
{
    return syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static unsigned
perf_mmap_size(int buf_size_shift)
{
    return ((1U << buf_size_shift) + 1) * sysconf(_SC_PAGESIZE);
}

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
    if (pt_perf_event_attr_init(true, false, &user_only_pe_attr) &&
        pt_perf_event_attr_init(false, true, &kernel_only_pe_attr) &&
        pt_perf_event_attr_init(true, true, &user_kernel_pe_attr)) {
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

typedef struct _pttracer_handle_t {
    int fd;
    int base_size;
    struct perf_event_mmap_page *header;
    void *aux;
} pttracer_handle_t;

/* 2^n size of event ring buffer (in pages) */
#define BUF_SIZE_SHIFT 8

drpttracer_status_t
drpttracer_start_trace(IN bool user, IN bool kernel, OUT void *tracer_handle)
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

    int fd = perf_event_open(attr, 0, -1, -1, 0);
    if (fd < 0) {
        ASSERT(false, "failed to init perf event for pt tracing");
        return DRPTTRACER_ERROR_FAILED_TO_OPEN_PERF_EVENT;
    }

    int base_size = perf_mmap_size(BUF_SIZE_SHIFT);
    struct perf_event_mmap_page *mpage =
        mmap(NULL, base_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mpage == (struct perf_event_mmap_page *)MAP_FAILED) {
        ASSERT(false, "failed to mmap perf event for pt tracing");
        close(fd);
        return DRPTTRACER_ERROR_FAILED_TO_MMAP_PERF_EVENT;
    }

    mpage->aux_offset = mpage->data_offset + mpage->data_size;
    mpage->aux_size = perf_mmap_size(BUF_SIZE_SHIFT);
    void *aux = mmap(NULL, mpage->aux_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (aux == (void *)MAP_FAILED) {
        ASSERT(false, "failed to mmap aux for pt tracing");
        munmap(mpage, base_size);
        close(fd);
        return DRPTTRACER_ERROR_FAILED_TO_MMAP_AUX;
    }

    pttracer_handle_t *handle =
        (pttracer_handle_t *)dr_global_alloc(sizeof(pttracer_handle_t));
    handle->fd = fd;
    handle->base_size = base_size;
    handle->header = mpage;
    handle->aux = aux;
    tracer_handle = (void *)handle;

    ioctl(handle->fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(handle->fd, PERF_EVENT_IOC_ENABLE, 0);
    return DRPTTRACER_SUCCESS;
}

drpttracer_status_t
drpttracer_end_trace(IN void *tracer_handle, OUT drpttracer_data_buf_t *pt_data,
                     OUT drpttracer_data_buf_t *sideband_data)
{
    pttracer_handle_t *handle = (pttracer_handle_t *)tracer_handle;
    if (handle == NULL && handle->fd < 0) {
        ASSERT(false, "invalid arguments");
        return DRPTTRACER_ERROR_INVALID_ARGUMENT;
    }
    ioctl(handle->fd, PERF_EVENT_IOC_DISABLE, 0);
    close(handle->fd);
    if (pt_data != NULL) {
        read_ring_buf_to_buf(handle->aux, handle->header->aux_size,
                             handle->header->aux_head, handle->header->aux_tail, pt_data);
    }
    munmap(handle->aux, handle->header->aux_size);
    if (sideband_data != NULL) {
        read_ring_buf_to_buf(handle->header, handle->header->data_size,
                             handle->header->data_head, handle->header->data_tail,
                             sideband_data);
    }
    munmap(handle->header, handle->base_size);
    dr_global_free(handle, sizeof(pttracer_handle_t));
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_create_data_buffer(OUT drpttracer_data_buf_t *data_buf)
{
    data_buf = (drpttracer_data_buf_t *)dr_global_alloc(sizeof(drpttracer_data_buf_t));
    memset(data_buf, 0, sizeof(drpttracer_data_buf_t));
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_delete_data_buffer(IN drpttracer_data_buf_t *data_buf)
{
    if (data_buf == NULL) {
        ASSERT(false, "failed to delete data buffer");
        return DRPTTRACER_ERROR_FAILED_TO_FREE_NULL_BUFFER;
    }
    dr_global_free(data_buf->buf, data_buf->buf_size);
    dr_global_free(data_buf, sizeof(drpttracer_data_buf_t));
    return DRPTTRACER_SUCCESS;
}

DR_EXPORT
drpttracer_status_t
drpttracer_dump_kcore_and_kallsyms(IN const char *dir)
{
    char script_template[512] = "#/bin/sh\n"
                                "sudo $(which perf) record --kcore -e "
                                "intel_pt/cyc,noretcomp/k echo '' >/dev/null 2>&1\n"
                                "sudo /usr/bin/cp perf.data/kcore_dir/kcore %s\n"
                                "sudo /usr/bin/cp perf.data/kcore_dir/kallsyms %s\n"
                                "sudo /usr/bin/rm -rf perf.data\n"
                                "sudo /usr/bin/chmod 644 %s/kcore %s/kallsyms\n";
    int script_max_len = 512 + MAXIMUM_PATH * 4;
    char *script = (char *)dr_global_alloc(sizeof(char) * script_max_len);
    dr_snprintf(script, script_max_len, script_template, dir, dir, dir, dir);
    if(system(script) == -1){
        ASSERT(false, "failed to dump kcore and kallsym");
        dr_global_free(script, sizeof(char) * script_max_len);
        return DRPTTRACER_ERROR_FAILED_TO_DUMP_KCORE_AND_KALLSYMS;
    }
    dr_global_free(script, sizeof(char) * script_max_len);
    return DRPTTRACER_SUCCESS;
}