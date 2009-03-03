/* **********************************************************
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

/* vmkuw.c - VMKernel UserWorld support functions */

#include "configure.h"
#ifndef VMX86_SERVER
#error This is only needed for supporting ESX/ESXi
#endif


/*
 * Our goal is to use unmodified VMware header files as much as
 * possible, and hopefully we can #define or rename any conflicting C
 * or CPP identifiers before including our headers.  If that is
 * insufficient we'll preferably generate clean copies with regexp
 * replacements, and hopefully we should not need to resort to
 * copy&paste of relevant pieces and try to maintain our own header
 * copies.
 *
 */

#include "vmware.h"
#include "vm_basic_types.h"

#include "uwvmk.h"
#include "uwvmkAPI.h"

#include "user_layout.h"

#include <stdlib.h>
#include <string.h>

/* Avoid problems with use of errno as var name in rest of file */
#undef errno

#include <sys/types.h>
#include <sys/sysctl.h>

/* Avoid multiple definitions of typedefs with VMware headers */
#define DR_DO_NOT_DEFINE_uint64
#define DR_DO_NOT_DEFINE_uint32
#define DR_DO_NOT_DEFINE_int32
#define DR_DO_NOT_DEFINE_int64
#define DR_DO_NOT_DEFINE_MAX_MIN

/* Allow our macro definitions to override ones defined in VMware headers */

#undef ASSERT
#undef ALIGNED
#undef ASSERT_NOT_TESTED
#undef ASSERT_NOT_IMPLEMENTED
#undef PAGE_SIZE

#include "globals_shared.h"
#include "globals.h"
#include "utils.h"

#include "vmkuw.h"

enum {VMKUW_TYPE_NONE,
      VMKUW_TYPE_KERN,
      VMKUW_TYPE_VISOR,
      VMKUW_TYPE_KERN64,
      VMKUW_TYPE_VISOR64};

/* Determine if we are running on VMKernel and if yes, what type. */
static int
os_get_vmk_type(void)
{
    /* should get initialized early on before self-protection is turned on */
    static int vmkernel_type = -1;

    /* Note that using get_uname() might be more portable than sysctl */
    /* but for now matching with bora/lib/misc/hostType.c */

    if (vmkernel_type == -1) {
        char osname[128];
        size_t osname_len;
        int kern_ostype_ctl[] = { CTL_KERN, KERN_OSTYPE };
        int rc;
      
        /* Only if the KERN_OSTYPE sysctl returns one of
         * USERWORLD_SYSCTL_KERN_OSTYPE, USERWORLD_SYSCTL_VISOR_OSTYPE
         * or USERWORLD_SYSCTL_VISOR64_OSTYPE
         */

        osname_len = sizeof(osname);
        rc = sysctl(kern_ostype_ctl, ARRAYSIZE(kern_ostype_ctl),
                    osname, &osname_len,
                    0, 0);
        if (rc == 0) {
            osname_len = MAX(sizeof (osname), osname_len);
            if (strncmp(osname, USERWORLD_SYSCTL_VISOR64_OSTYPE, osname_len) == 0) {
                vmkernel_type = VMKUW_TYPE_VISOR64;
            } else if (strncmp(osname, USERWORLD_SYSCTL_KERN64_OSTYPE, osname_len) == 0) {
                vmkernel_type = VMKUW_TYPE_KERN64;
            } else if (strncmp(osname, USERWORLD_SYSCTL_VISOR_OSTYPE, osname_len) == 0) {
                vmkernel_type = VMKUW_TYPE_VISOR;
            } else if (strncmp(osname, USERWORLD_SYSCTL_KERN_OSTYPE, osname_len) == 0) {
                vmkernel_type = VMKUW_TYPE_KERN;
            } else {
                vmkernel_type = VMKUW_TYPE_NONE;
            }
            LOG(GLOBAL, LOG_ALL, 1, "%s: vmkernel_type = %d\n", __func__, vmkernel_type);
        } else {
            LOG(GLOBAL, LOG_ALL, 1,
                "sysctl([ CTL_KERN, KERN_OSTYPE ]) failed\n");
            vmkernel_type = 0;
        }
    }
    return (vmkernel_type);
}

bool
os_in_vmkernel_userworld(void)
{
    /*
     * We're running in a userworld if 'userworld' (ESX COS) or 'vmkernel' (ESXi) show up
     * in the sysctl ostype field.  (Vanilla linux and COS applications return 'Linux'
     * for this field.)
     */
    return os_get_vmk_type() > 0;
}

bool
os_in_vmkernel_32bit(void)
{
    /* FIXME PR 363075: we can distinguish visor from classic but not 32-bit
     * (3.5) from 64-bit (4.0); we allow running on 3.5 via this option.
     * os_get_vmk_type() on esx 4.0 classic returns VMKUW_TYPE_KERN
     * and 4.0 visor returns VMKUW_TYPE_VISOR instead of the 64
     * versions.  We only use this as a runtime check for whether
     * SYS_exit_group is implemented, so if a user on 3.5 or 3.5i does
     * not set the runtime option we will simply hang on exit in debug
     * and probably crash in release.
     */
    return (DYNAMO_OPTION(esx_32bit));
}

/* Exported symbol also allows scanning symbol table to statically
 * verify if a library matches a running kernel
 */
const uint32 vmkuw_syscall = UWVMKSYSCALL_CHECKSUM; /* generated in uwvmk_dist.h */

/* Verify that the UWVMK kernel and our userspace versions match. */
static bool
vmk_verify_syscall_version(void)
{
    uint32 uversion = vmkuw_syscall;
    uint32 kversion = 0xbad0beef;
    int rc;

    if (DYNAMO_OPTION(vmkuw_version) != 0)
        uversion = DYNAMO_OPTION(vmkuw_version);

    ASSERT(os_in_vmkernel_userworld());

    /* VMKernel_GetSyscallVersion has a constant syscall number across builds */
    rc = VMKernel_GetSyscallVersion(&kversion);
    if (rc != 0) {
        LOG(GLOBAL, LOG_ALL, 1,
            "vmk_verify_syscall_version: "
            "Error getting vmkernel syscall version: %d\n", rc);
        return false;
    }

    LOG(GLOBAL, LOG_ALL, 2,
        "%s: Kernel UWVMK version = %#x, "
        "User UWVMK version = %#x\n",
        __func__, 
        kversion, uversion);

    if (kversion == uversion) {
        return true;
    }

    LOG(GLOBAL, LOG_ALL, 1,
        "%s: FAIL: kernel UWVMK version (%#x) and user-mode UWVMK "
        "version (%#x) do not match.\n",
        __func__, kversion, uversion);
   
    LOG(GLOBAL, LOG_ALL, 1,
        "** Please verify that the DynamoRIO build corresponds to "
        "the ESX/Visor build\n"
        "(or set your VMTREE)**\n");

    return false;
}

/* vmk-specific initializations */
void
vmk_init(void)
{
    if (!os_in_vmkernel_userworld()) {
        LOG(GLOBAL, LOG_ALL, 1,
            "** You are running a DynamoRIO version that supports ESX on Linux or COS.\n"
            "  Did you want to run this application as a userworld?\n");
        return;
    }

    if (!vmk_verify_syscall_version()) {
        /* we should just abort here, no point in letting application run */
        SYSLOG(SYSLOG_CRITICAL, ESX_VERSION_MISMATCH,
               2, get_application_name(), get_application_pid());
        os_terminate(NULL, TERMINATE_PROCESS);
    }

    ASSERT((byte *) DYNAMO_OPTION(vm_base) >= os_vmk_mmap_text_start() &&
           ((byte *) DYNAMO_OPTION(vm_base)) + 
           DYNAMO_OPTION(vm_max_offset) +
           DYNAMO_OPTION(vm_size) <= os_vmk_mmap_text_end());
}


/*
 * Actual coredump location may vary -
 * in release builds check tail of /var/log/messages for generated coredump
 */
void
vmk_request_live_coredump(const char *msg)
{
    char coreDumpPath[1024];
    VMK_ReturnStatus status;

    if (!os_in_vmkernel_userworld())
        return;

    status = VMKernel_LiveCoreDump(coreDumpPath, sizeof coreDumpPath);

    if (status != VMK_OK) {
        LOG(THREAD_GET, LOG_ALL, 1, "LiveCoreDump %s returned error code %#x\n",
               msg, status);
    } else {
        LOG(THREAD_GET, LOG_ALL, 1, "LiveCoreDump %s created core at %s\n",
            msg, coreDumpPath);
    }
}

timestamp_t
vmk_get_timer_frequency(void)
{
    VMK_ReturnStatus status;
    uint32 tsc_kHz_estimate;

    status = VMKernel_GetTSCkhzEstimate(&tsc_kHz_estimate);
    if (status != VMK_OK) {
        ASSERT_NOT_TESTED();
        LOG(THREAD_GET, LOG_ALL, 1, "%s: failed, error code %#x\n", __func__, status);
        return 3*1000*1000;     /* 3GHz as a reasonable estimate for the next decade */
    }

    LOG(THREAD_GET, LOG_ALL, 1, "%s: TSC estimate in KHz: %d\n", __func__, tsc_kHz_estimate);
    return tsc_kHz_estimate;
}

byte *
os_vmk_mmap_text_start(void)
{
    return (byte *) VMK_USER_FIRST_MMAP_TEXT_VADDR;
}

byte *
os_vmk_mmap_text_end(void)
{
    return (byte *) ALIGN_FORWARD(VMK_USER_LAST_MMAP_TEXT_VADDR, PAGE_SIZE);
}
