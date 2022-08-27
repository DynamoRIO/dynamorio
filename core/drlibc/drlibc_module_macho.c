/* *******************************************************************************
 * Copyright (c) 2013-2019 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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

/* Mach-O file parsing support shared with non-core. */

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "module_private.h"
#include "memquery_macos.h"
#include "module_macos_dyld.h"
#include <mach-o/ldsyms.h> /* _mh_dylib_header */
#include <mach-o/loader.h> /* mach_header */
#include <mach-o/dyld_images.h>
#include <mach/thread_status.h> /* i386_thread_state_t */
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <sys/syscall.h>
#include <stddef.h> /* offsetof */
#include <dlfcn.h>

/* Like is_elf_so_header(), if size == 0 then safe-reads the header; else
 * assumes that [base, base+size) is readable.
 */
bool
is_macho_header(app_pc base, size_t size)
{
    struct mach_header hdr_safe;
    struct mach_header *hdr;
    if (base == NULL)
        return false;
    if (size >= sizeof(hdr_safe)) {
        hdr = (struct mach_header *)base;
    } else {
        if (!d_r_safe_read(base, sizeof(hdr_safe), &hdr_safe))
            return false;
        hdr = &hdr_safe;
    }
    ASSERT(offsetof(struct mach_header, filetype) ==
           offsetof(struct mach_header_64, filetype));
    if ((hdr->magic == MH_MAGIC && hdr->cputype == CPU_TYPE_X86) ||
        (hdr->magic == MH_MAGIC_64 &&
         (hdr->cputype == CPU_TYPE_X86_64 || hdr->cputype == CPU_TYPE_ARM64))) {
        /* We shouldn't see MH_PRELOAD as it can't be loaded by the kernel */
        if (hdr->filetype == MH_EXECUTE || hdr->filetype == MH_DYLIB ||
            hdr->filetype == MH_BUNDLE || hdr->filetype == MH_DYLINKER ||
            hdr->filetype == MH_FVMLIB) {
            return true;
        }
    }
    return false;
}

static bool
platform_from_macho(file_t f, dr_platform_t *platform)
{
    struct mach_header mach_hdr;
    if (os_read(f, &mach_hdr, sizeof(mach_hdr)) != sizeof(mach_hdr))
        return false;
    if (!is_macho_header((app_pc)&mach_hdr, sizeof(mach_hdr)))
        return false;
    switch (mach_hdr.cputype) {
    case CPU_TYPE_ARM64: *platform = DR_PLATFORM_64BIT; break;
    case CPU_TYPE_X86_64: *platform = DR_PLATFORM_64BIT; break;
    case CPU_TYPE_X86: *platform = DR_PLATFORM_32BIT; break;
    default: return false;
    }
    return true;
}

bool
module_get_platform(file_t f, dr_platform_t *platform, dr_platform_t *alt_platform)
{
    struct fat_header fat_hdr;
    /* Both headers start with a 32-bit magic # */
    uint32_t magic;
    if (os_read(f, &magic, sizeof(magic)) != sizeof(magic) || !os_seek(f, 0, SEEK_SET))
        return false;
    if (magic == FAT_CIGAM) { /* big-endian */
        /* This is a "fat" or "universal" binary */
        struct fat_arch arch;
        uint num, i;
        bool found_main = false, found_alt = false;
        dr_platform_t local_alt = DR_PLATFORM_NONE;
        int64 cur_pos;
        if (os_read(f, &fat_hdr, sizeof(fat_hdr)) != sizeof(fat_hdr))
            return false;
        /* OSSwapInt32 is a macro, so there's no lib dependence here */
        num = OSSwapInt32(fat_hdr.nfat_arch);
        for (i = 0; i < num; i++) {
            if (os_read(f, &arch, sizeof(arch)) != sizeof(arch))
                return false;
            cur_pos = os_tell(f);
            /* The primary platform is the one that will be used on an execve,
             * which is the one that matches the kernel's bitwidth.
             */
            if ((kernel_is_64bit() && OSSwapInt32(arch.cputype) == CPU_TYPE_X86_64) ||
                (!kernel_is_64bit() && OSSwapInt32(arch.cputype) == CPU_TYPE_X86)) {
                /* Line up right before the Mach-O header */
                if (!os_seek(f, OSSwapInt32(arch.offset), SEEK_SET))
                    return false;
                if (!platform_from_macho(f, platform))
                    return false;
                found_main = true;
                if (found_alt)
                    break;
            } else if (OSSwapInt32(arch.cputype) == CPU_TYPE_X86_64 ||
                       OSSwapInt32(arch.cputype) == CPU_TYPE_X86) {
                /* Line up right before the Mach-O header */
                if (!os_seek(f, OSSwapInt32(arch.offset), SEEK_SET))
                    return false;
                if (platform_from_macho(f, &local_alt)) {
                    found_alt = true;
                    if (found_main)
                        break;
                }
            }
            if (!os_seek(f, cur_pos, SEEK_SET))
                return false;
        }
        if (!found_main && found_alt) {
            *platform = local_alt;
            local_alt = DR_PLATFORM_NONE;
        }
        if (alt_platform != NULL)
            *alt_platform = local_alt;
        return (found_main || found_alt);
    } else if (alt_platform != NULL)
        *alt_platform = DR_PLATFORM_NONE;
    return platform_from_macho(f, platform);
}
