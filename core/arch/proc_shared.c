/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2008 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * proc_shared.c - processor-specific shared routines
 */

#include "../globals.h"
#include "proc.h"
#include "instr.h"               /* for dr_insert_{save,restore}_fpstate */
#include "instrument.h"          /* for dr_insert_{save,restore}_fpstate */
#include "instr_create_shared.h" /* for dr_insert_{save,restore}_fpstate */

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* cache_line_size is exported for efficient access.
 * FIXME: In case the processor doesn't support the
 * cpuid instruction, use a default value of 32.
 * (see case 463 for discussion)
 */
size_t cache_line_size = 32;
static ptr_uint_t mask; /* bits that should be 0 to be cache-line-aligned */
cpu_info_t cpu_info = { VENDOR_UNKNOWN,
#ifdef AARCHXX
                        0,
                        0,
#endif
                        0,
                        0,
                        0,
                        0,
                        CACHE_SIZE_UNKNOWN,
                        CACHE_SIZE_UNKNOWN,
                        CACHE_SIZE_UNKNOWN,
#if defined(RISCV64)
                        /* FIXME i#3544: Not implemented */
                        { 0 },
#else
                        { 0, 0, 0, 0 },
#endif
                        { 0x6e6b6e75, 0x006e776f } };

void
proc_set_cache_size(uint val, uint *dst)
{
    CLIENT_ASSERT(dst != NULL, "invalid internal param");
    switch (val) {
    case 8: *dst = CACHE_SIZE_8_KB; break;
    case 16: *dst = CACHE_SIZE_16_KB; break;
    case 32: *dst = CACHE_SIZE_32_KB; break;
    case 64: *dst = CACHE_SIZE_64_KB; break;
    case 128: *dst = CACHE_SIZE_128_KB; break;
    case 256: *dst = CACHE_SIZE_256_KB; break;
    case 512: *dst = CACHE_SIZE_512_KB; break;
    case 1024: *dst = CACHE_SIZE_1_MB; break;
    case 2048: *dst = CACHE_SIZE_2_MB; break;
    default: SYSLOG_INTERNAL_ERROR("Unknown processor cache size"); break;
    }
}

void
proc_init(void)
{
    LOG(GLOBAL, LOG_TOP, 1, "Running on a %d CPU machine\n", get_num_processors());
    proc_init_arch();
    CLIENT_ASSERT(cache_line_size > 0, "invalid cache line size");
    mask = (cache_line_size - 1);

    LOG(GLOBAL, LOG_TOP, 1, "Cache line size is %d bytes\n", cache_line_size);
    LOG(GLOBAL, LOG_TOP, 1, "L1 icache=%s, L1 dcache=%s, L2 cache=%s\n",
        proc_get_cache_size_str(proc_get_L1_icache_size()),
        proc_get_cache_size_str(proc_get_L1_dcache_size()),
        proc_get_cache_size_str(proc_get_L2_cache_size()));
    LOG(GLOBAL, LOG_TOP, 1, "Processor brand string = %s\n", cpu_info.brand_string);
    LOG(GLOBAL, LOG_TOP, 1, "Type=0x%x, Family=0x%x, Model=0x%x, Stepping=0x%x\n",
        cpu_info.type, cpu_info.family, cpu_info.model, cpu_info.stepping);

#ifdef AARCHXX
    /* XXX: Should we create an arch/aarchxx/proc.c just for this code? */
#    define PROC_CPUINFO "/proc/cpuinfo"
#    define CPU_ARCH_LINE_FORMAT "CPU architecture: %u\n"
    file_t cpuinfo = os_open(PROC_CPUINFO, OS_OPEN_READ);
    /* This can happen in a chroot or if /proc is disabled. */
    if (cpuinfo != INVALID_FILE) {
        char *buf = global_heap_alloc(PAGE_SIZE HEAPACCT(ACCT_OTHER));
        ssize_t nread = os_read(cpuinfo, buf, PAGE_SIZE - 1);
        if (nread > 0) {
            buf[nread] = '\0';
            char *arch_line = strstr(buf, "CPU architecture");
            if (arch_line != NULL &&
                sscanf(arch_line, CPU_ARCH_LINE_FORMAT, &cpu_info.architecture) == 1) {
                LOG(GLOBAL, LOG_ALL, 2, "Processor architecture: %u\n",
                    cpu_info.architecture);
            }
        }
        global_heap_free(buf, PAGE_SIZE HEAPACCT(ACCT_OTHER));
        os_close(cpuinfo);
    }
#endif
}

uint
proc_get_vendor(void)
{
    return cpu_info.vendor;
}

DR_API
int
proc_set_vendor(uint new_vendor)
{
    if (new_vendor == VENDOR_INTEL || new_vendor == VENDOR_AMD ||
        new_vendor == VENDOR_ARM) {
        uint old_vendor = cpu_info.vendor;
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        cpu_info.vendor = new_vendor;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        return old_vendor;
    } else {
        CLIENT_ASSERT(false, "invalid vendor");
        return -1;
    }
}

uint
proc_get_family(void)
{
    return cpu_info.family;
}

uint
proc_get_type(void)
{
    return cpu_info.type;
}

/* FIXME: Add MODEL_ constants to proc.h?? */
uint
proc_get_model(void)
{
    return cpu_info.model;
}

uint
proc_get_stepping(void)
{
    return cpu_info.stepping;
}

#ifdef AARCHXX
uint
proc_get_architecture(void)
{
    return cpu_info.architecture;
}

uint
proc_get_vector_length_bytes(void)
{
    return cpu_info.sve_vector_length_bytes;
}
#endif

features_t *
proc_get_all_feature_bits(void)
{
    return &cpu_info.features;
}

char *
proc_get_brand_string(void)
{
    return (char *)cpu_info.brand_string;
}

cache_size_t
proc_get_L1_icache_size(void)
{
    return cpu_info.L1_icache_size;
}

cache_size_t
proc_get_L1_dcache_size(void)
{
    return cpu_info.L1_dcache_size;
}

cache_size_t
proc_get_L2_cache_size(void)
{
    return cpu_info.L2_cache_size;
}

const char *
proc_get_cache_size_str(cache_size_t size)
{
    static const char *strings[] = { "8 KB",   "16 KB",  "32 KB", "64 KB", "128 KB",
                                     "256 KB", "512 KB", "1 MB",  "2 MB",  "unknown" };
    CLIENT_ASSERT(size <= CACHE_SIZE_UNKNOWN, "proc_get_cache_size_str: invalid size");
    return strings[size];
}

size_t
proc_get_cache_line_size(void)
{
    return cache_line_size;
}

/* check to see if addr is cache aligned */
bool
proc_is_cache_aligned(void *addr)
{
    return (((ptr_uint_t)addr & mask) == 0x0);
}

/* Given an address or number of bytes sz, return a number >= sz that is divisible
   by the cache line size. */
ptr_uint_t
proc_bump_to_end_of_cache_line(ptr_uint_t sz)
{
    if ((sz & mask) == 0x0)
        return sz; /* sz already a multiple of the line size */

    return ((sz + cache_line_size) & ~mask);
}

/* yes same result as PAGE_START...FIXME: get rid of one of them? */
void *
proc_get_containing_page(void *addr)
{
    return (void *)(((ptr_uint_t)addr) & ~(PAGE_SIZE - 1));
}
