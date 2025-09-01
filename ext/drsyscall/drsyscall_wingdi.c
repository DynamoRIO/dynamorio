/* **********************************************************
 * Copyright (c) 2011-2025 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Need this defined and to the latest to get the latest defines and types */
#define _WIN32_WINNT 0x0601 /* == _WIN32_WINNT_WIN7 */
#define WINVER _WIN32_WINNT

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"
#include <stddef.h> /* offsetof */

/* for NtGdi* syscalls */
#include <wingdi.h> /* usually from windows.h; required by winddi.h + ENUMLOGFONTEXDVW */
#define NT_BUILD_ENVIRONMENT 1 /* for d3dnthal.h */
#include <d3dnthal.h>
#include <winddi.h> /* required by ntgdityp.h and prntfont.h */
#include <prntfont.h>
#include "../drmf/wininc/ntgdityp.h"
#include <ntgdi.h>
#include <winspool.h>   /* for DRIVER_INFO_2W */
#include <dxgiformat.h> /* for DXGI_FORMAT */

/* for NtUser* syscalls */
#include "../drmf/wininc/ndk_extypes.h" /* required by ntuser.h */
#include "../drmf/wininc/ntuser.h"
#include "../drmf/wininc/ntuser_win8.h"
#include <commctrl.h> /* for EM_GETCUEBANNER */

/***************************************************************************/
/* System calls with wrappers in user32.dll.
 */

drsys_sysnum_t sysnum_UserSystemParametersInfo = { -1, 0 };
drsys_sysnum_t sysnum_UserMenuInfo = { -1, 0 };
drsys_sysnum_t sysnum_UserMenuItemInfo = { -1, 0 };
drsys_sysnum_t sysnum_UserGetAltTabInfo = { -1, 0 };
drsys_sysnum_t sysnum_UserGetRawInputBuffer = { -1, 0 };
drsys_sysnum_t sysnum_UserGetRawInputData = { -1, 0 };
drsys_sysnum_t sysnum_UserGetRawInputDeviceInfo = { -1, 0 };
drsys_sysnum_t sysnum_UserTrackMouseEvent = { -1, 0 };
drsys_sysnum_t sysnum_UserLoadKeyboardLayoutEx = { -1, 0 };
drsys_sysnum_t sysnum_UserCreateWindowStation = { -1, 0 };
drsys_sysnum_t sysnum_UserMessageCall = { -1, 0 };
drsys_sysnum_t sysnum_UserCreateAcceleratorTable = { -1, 0 };
drsys_sysnum_t sysnum_UserCopyAcceleratorTable = { -1, 0 };
drsys_sysnum_t sysnum_UserSetScrollInfo = { -1, 0 };

/* Table that maps usercall names to secondary syscall numbers.
 * Number can be 0 so we store +1.
 */
#define USERCALL_TABLE_HASH_BITS 8
static hashtable_t usercall_table;

/***************************************************************************
 * NtUserCall* secondary system call numbers
 */

#define NONE -1

static const char *const usercall_names[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
#    type "." #    name,
#include "drsyscall_usercallx.h"
#undef USERCALL
};
#define NUM_USERCALL_NAMES (sizeof(usercall_names) / sizeof(usercall_names[0]))

static const char *const usercall_primary[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
#    type,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_1803_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w15,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_1709_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w14,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_1703_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w13,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_1607_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w12,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_1511_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w11,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win10_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w10,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win81_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w81,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win8_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w8,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win7_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w7,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int winvistaSP2_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    vistaSP2,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int winvistaSP01_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    vistaSP01,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win2003_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w2003,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int winxp_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    xp,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

static const int win2k_usercall_nums[] = {
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, w11, \
                 w12, w13, w14, w15)                                                     \
    w2k,
#include "drsyscall_usercallx.h"
#undef USERCALL
};

/***************************************************************************
 * System calls with wrappers in gdi32.dll.
 */

drsys_sysnum_t sysnum_GdiCreatePaletteInternal = { -1, 0 };
drsys_sysnum_t sysnum_GdiCheckBitmapBits = { -1, 0 };
drsys_sysnum_t sysnum_GdiHfontCreate = { -1, 0 };
drsys_sysnum_t sysnum_GdiDoPalette = { -1, 0 };
drsys_sysnum_t sysnum_GdiExtTextOutW = { -1, 0 };
drsys_sysnum_t sysnum_GdiDescribePixelFormat = { -1, 0 };
drsys_sysnum_t sysnum_GdiGetRasterizerCaps = { -1, 0 };
drsys_sysnum_t sysnum_GdiPolyPolyDraw = { -1, 0 };

/***************************************************************************
 * TOP-LEVEL
 */

uint
wingdi_get_secondary_syscall_num(void *drcontext, const char *name, uint primary_num)
{
    drsys_sysnum_t num;
    const char *skip_primary;

    num.secondary = (uint)(ptr_uint_t)hashtable_lookup(&usercall_table, (void *)name);
    if (num.secondary == 0) {
        LOG(drcontext, SYSCALL_VERBOSE, "WARNING: could not find usercall %s\n", name);
        return -1;
    }
    num.secondary = num.secondary - 1 /*+1 in usercall table*/;
    num.number = primary_num;

    /* add secondary usercall with & without primary prefix */
    name2num_entry_add(drcontext, name, num, false /*no Zw*/, false);
    skip_primary = strstr(name, ".");
    if (skip_primary != NULL &&
        /* don't add unknown w/o primary */
        strstr(name, ".UNKNOWN") == NULL) {
        name2num_entry_add(drcontext, skip_primary + 1 /*"."*/, num, false /*no Zw*/,
                           false);
    }
    return num.secondary;
}

void
wingdi_add_usercall(void *drcontext, const char *name, int num)
{
    IF_DEBUG(bool ok;)
    /* We might be called from sysnum file parsing prior to drsyscall_wingdi_init: */
    if (usercall_table.table == NULL) {
        /* We duplicate all strings to handle synum files (i#1908) */
        hashtable_init(&usercall_table, USERCALL_TABLE_HASH_BITS, HASH_STRING,
                       true /*strdup*/);
    }
    LOG(drcontext, SYSCALL_VERBOSE + 1, "name2num usercall: adding %s => %d\n", name,
        num);
    IF_DEBUG(ok =)
    hashtable_add(&usercall_table, (void *)name,
                  (void *)(ptr_uint_t)(num + 1 /*avoid 0*/));
    DOLOG(1, {
        if (!ok)
            LOG(drcontext, 1, "Dup usercall entry for %s\n", name);
    });
    ASSERT(ok, "no dup entries in usercall_table");
}

drmf_status_t
drsyscall_wingdi_init(void *drcontext, app_pc ntdll_base, dr_os_version_info_t *ver,
                      bool use_usercall_table)
{
    uint i;
    const int *usercalls;
    if (usercall_table.table == NULL) { /* may be initialized earlier */
        hashtable_init(&usercall_table, USERCALL_TABLE_HASH_BITS, HASH_STRING,
                       true /*strdup*/);
    }
    if (!use_usercall_table) {
        /* i#1908: while the usercall numbers don't change as much, they do
         * shift around, and it's better to use our unknown syscall heuristics
         * rather than get something completely wrong.  Our syscall file supports
         * usercall numbers to get the right behavior.
         */
        return DRMF_SUCCESS;
    }
    LOG(drcontext, 1, "Windows version is %d.%d.%d\n", ver->version,
        ver->service_pack_major, ver->service_pack_minor);
    switch (ver->version) {
    case DR_WINDOWS_VERSION_10_1803: usercalls = win10_1803_usercall_nums; break;
    case DR_WINDOWS_VERSION_10_1709: usercalls = win10_1709_usercall_nums; break;
    case DR_WINDOWS_VERSION_10_1703: usercalls = win10_1703_usercall_nums; break;
    case DR_WINDOWS_VERSION_10_1607: usercalls = win10_1607_usercall_nums; break;
    case DR_WINDOWS_VERSION_10_1511: usercalls = win10_1511_usercall_nums; break;
    case DR_WINDOWS_VERSION_10: usercalls = win10_usercall_nums; break;
    case DR_WINDOWS_VERSION_8_1: usercalls = win81_usercall_nums; break;
    case DR_WINDOWS_VERSION_8: usercalls = win8_usercall_nums; break;
    case DR_WINDOWS_VERSION_7: usercalls = win7_usercall_nums; break;
    case DR_WINDOWS_VERSION_VISTA: {
        if (ver->service_pack_major >= 2)
            usercalls = winvistaSP2_usercall_nums;
        else
            usercalls = winvistaSP01_usercall_nums;
        break;
    }
    case DR_WINDOWS_VERSION_2003: usercalls = win2003_usercall_nums; break;
    case DR_WINDOWS_VERSION_XP: usercalls = winxp_usercall_nums; break;
    case DR_WINDOWS_VERSION_2000: usercalls = win2k_usercall_nums; break;
    case DR_WINDOWS_VERSION_NT:
    default: return DRMF_WARNING_UNSUPPORTED_KERNEL;
    }

    /* Set up hashtable to translate usercall names to numbers */
    for (i = 0; i < NUM_USERCALL_NAMES; i++) {
        if (usercalls[i] != NONE) {
            wingdi_add_usercall(drcontext, usercall_names[i], usercalls[i]);
        }
    }

    return DRMF_SUCCESS;
}

void
drsyscall_wingdi_exit(void)
{
    hashtable_delete(&usercall_table);
}

void
drsyscall_wingdi_thread_init(void *drcontext)
{
}

void
drsyscall_wingdi_thread_exit(void *drcontext)
{
}

/***************************************************************************
 * CUSTOM SYSCALL DATA STRUCTURE HANDLING
 */

/* XXX i#488: if too many params can take atoms or strings, should perhaps
 * query to verify really an atom to avoid false negatives with
 * bad string pointers
 */
static bool
is_atom(void *ptr)
{
    /* top 2 bytes are guaranteed to be 0 */
    return ((ptr_uint_t)ptr) < 0x10000;
}

/* XXX i#488: see is_atom comment */
static bool
is_int_resource(void *ptr)
{
    /* top 2 bytes are guaranteed to be 0 */
    return IS_INTRESOURCE(ptr);
}

bool
handle_large_string_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                           app_pc start, uint size)
{
    LARGE_STRING ls;
    LARGE_STRING *arg = (LARGE_STRING *)start;
    drsys_param_type_t type_val = DRSYS_TYPE_LARGE_STRING;
    const char *type_name = "LARGE_STRING";
    ASSERT(size == sizeof(LARGE_STRING), "invalid size");
    /* I've seen an atom (or int resource?) here
     * XXX i#488: avoid false neg: not too many of these now though
     * so we allow on all syscalls
     */
    if (is_atom(start))
        return true; /* handled */
    /* we assume OUT fields just have their Buffer as OUT */
    if (ii->arg->pre) {
        if (!report_memarg(ii, arg_info, (byte *)&arg->Length, sizeof(arg->Length),
                           "LARGE_STRING.Length"))
            return true;
        /* this will include LARGE_STRING.bAnsi */
        if (!report_memarg(ii, arg_info,
                           /* we assume no padding (can't take & or offsetof bitfield) */
                           (byte *)&arg->Length + sizeof(arg->Length),
                           sizeof(ULONG /*+bAnsi*/), "LARGE_STRING.MaximumLength"))
            return true;
        if (!report_memarg(ii, arg_info, (byte *)&arg->Buffer, sizeof(arg->Buffer),
                           "LARGE_STRING.Buffer"))
            return true;
    }
    if (safe_read((void *)start, sizeof(ls), &ls)) {
        if (ii->arg->pre) {
            if (!report_memarg_ex(ii, arg_info->param, DRSYS_PARAM_BOUNDS,
                                  (byte *)ls.Buffer, ls.MaximumLength,
                                  "LARGE_STRING capacity", DRSYS_TYPE_LARGE_STRING, NULL,
                                  DRSYS_TYPE_INVALID))
                return true;
            if (TEST(SYSARG_READ, arg_info->flags)) {
                if (!report_memarg(ii, arg_info, (byte *)ls.Buffer, ls.Length,
                                   "LARGE_STRING content"))
                    return true;
            }
        } else if (TEST(SYSARG_WRITE, arg_info->flags)) {
            if (!report_memarg(ii, arg_info, (byte *)ls.Buffer, ls.Length,
                               "LARGE_STRING content"))
                return true;
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true; /* handled */
}

bool
handle_devmodew_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                       app_pc start, uint size)
{
    /* DEVMODEW is var-len by windows ver plus optional private driver data appended */
    /* can't use a DEVMODEW as ours may be longer than app's if on older windows */
    char buf[offsetof(DEVMODEW, dmFields)]; /* need dmSize and dmDriverExtra */
    DEVMODEW *safe;
    DEVMODEW *param = (DEVMODEW *)start;
    if (ii->arg->pre) {
        /* XXX: for writes, are we sure all these fields should be set by the caller?
         * That's what my pre-drsyscall code had so going with it for now.
         */
        if (!report_memarg_type(ii, arg_info->param, SYSARG_READ, start,
                                BUFFER_SIZE_BYTES(buf), "DEVMODEW through dmDriverExtra",
                                SYSARG_TYPE_DEVMODEW, NULL))
            return true;
    }
    if (safe_read(start, BUFFER_SIZE_BYTES(buf), buf)) {
        safe = (DEVMODEW *)buf;
        ASSERT(safe->dmSize > offsetof(DEVMODEW, dmFormName), "invalid size");
        /* there's some padding in the middle */
        if (!report_memarg(ii, arg_info, (byte *)&param->dmFields,
                           ((byte *)&param->dmCollate) + sizeof(safe->dmCollate) -
                               (byte *)&param->dmFields,
                           "DEVMODEW dmFields through dmCollate"))
            return true;
        if (!report_memarg(ii, arg_info, (byte *)&param->dmFormName,
                           (start + safe->dmSize) - (byte *)(&param->dmFormName),
                           "DEVMODEW dmFormName onward"))
            return true;
        if (!report_memarg(ii, arg_info, start + safe->dmSize, safe->dmDriverExtra,
                           "DEVMODEW driver extra info"))
            return true;
        ;
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true; /* handled */
}

bool
handle_wndclassexw_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                          app_pc start, uint size)
{
    WNDCLASSEXW safe;
    /* i#499: it seems that cbSize is not set for NtUserGetClassInfo when using
     * user32!GetClassInfo so we use sizeof for writes.  I suspect that once
     * they add any more new fields they will start using it.  We could
     * alternatively keep the check here and treat this is a user32.dll bug and
     * suppress it.
     */
    bool use_cbSize = TEST(SYSARG_READ, arg_info->flags);
    if (ii->arg->pre && use_cbSize) {
        if (!report_memarg_type(ii, arg_info->param, SYSARG_READ, start,
                                sizeof(safe.cbSize), "WNDCLASSEX.cbSize",
                                SYSARG_TYPE_WNDCLASSEXW, NULL))
            return true;
    }
    if (safe_read(start, sizeof(safe), &safe)) {
        if (!report_memarg(ii, arg_info, start,
                           use_cbSize ? safe.cbSize : sizeof(WNDCLASSEX), "WNDCLASSEX"))
            return true;
        /* For WRITE there is no capacity here so nothing to check (i#505) */
        if ((ii->arg->pre && TEST(SYSARG_READ, arg_info->flags)) ||
            (!ii->arg->pre && TEST(SYSARG_WRITE, arg_info->flags))) {
            /* lpszMenuName can be from MAKEINTRESOURCE, and
             * lpszClassName can be an atom
             */
            if ((!use_cbSize || safe.cbSize > offsetof(WNDCLASSEX, lpszMenuName)) &&
                !is_atom((void *)safe.lpszMenuName)) {
                handle_cwstring(ii, "WNDCLASSEXW.lpszMenuName", (byte *)safe.lpszMenuName,
                                0, arg_info->param, arg_info->flags, NULL, true);
                if (ii->abort)
                    return true;
            }
            if ((!use_cbSize || safe.cbSize > offsetof(WNDCLASSEX, lpszClassName)) &&
                !is_int_resource((void *)safe.lpszClassName)) {
                handle_cwstring(ii, "WNDCLASSEXW.lpszClassName",
                                /* docs say 256 is max length: we read until
                                 * NULL though
                                 */
                                (byte *)safe.lpszClassName, 0, arg_info->param,
                                arg_info->flags, NULL, true);
                if (ii->abort)
                    return true;
            }
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true; /* handled */
}

bool
handle_clsmenuname_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                          app_pc start, uint size)
{
    CLSMENUNAME safe;
    if (!report_memarg(ii, arg_info, start, size, "CLSMENUNAME"))
        return true;
    if (ii->arg->pre && !TEST(SYSARG_READ, arg_info->flags)) {
        /* looks like even the UNICODE_STRING is not set up: contains garbage,
         * so presumably kernel creates it and doesn't just write to Buffer
         */
        return true; /* handled */
    }
    /* XXX i#487: CLSMENUNAME format is not fully known and doesn't seem
     * to match this, on win7 at least
     */
#if 0 /* disabled: see comment above */
    if (safe_read(start, sizeof(safe), &safe)) {
        if (!is_atom(safe.pszClientAnsiMenuName)) {
            handle_cstring(pre, sysnum, "CLSMENUNAME.lpszMenuName",
                           safe.pszClientAnsiMenuName, 0, arg_info->flags,
                           NULL, true);
            if (ii->abort)
                return true;
        }
        if (!is_atom(safe.pwszClientUnicodeMenuName)) {
            handle_cwstring(ii, "CLSMENUNAME.lpszMenuName",
                            (byte *) safe.pwszClientUnicodeMenuName, 0,
                            arg_info->param, arg_info->flags, NULL, true);
            if (ii->abort)
                return true;
        }
        /* XXX: I've seen the pusMenuName pointer itself be an atom, though
         * perhaps should also handle just the Buffer being an atom?
         */
        if (!is_atom(safe.pusMenuName)) {
            handle_unicode_string_access(ii, arg_info,
                                         (byte *) safe.pusMenuName,
                                         sizeof(UNICODE_STRING), false);
            if (ii->abort)
                return true;
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
#endif
    return true; /* handled */
}

bool
handle_menuiteminfow_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                            app_pc start, uint size)
{
    MENUITEMINFOW *real = (MENUITEMINFOW *)start;
    MENUITEMINFOW safe;
    bool check_dwTypeData = false;
    /* user must set cbSize for set or get */
    if (ii->arg->pre) {
        if (!report_memarg_type(ii, arg_info->param, SYSARG_READ, start,
                                sizeof(safe.cbSize), "MENUITEMINFOW.cbSize",
                                SYSARG_TYPE_MENUITEMINFOW, NULL))
            return true;
    }
    if (safe_read(start, sizeof(safe), &safe)) {
        if (ii->arg->pre) {
            if (!report_memarg_ex(ii, arg_info->param, DRSYS_PARAM_BOUNDS, start,
                                  safe.cbSize, "MENUITEMINFOW", DRSYS_TYPE_MENUITEMINFOW,
                                  NULL, DRSYS_TYPE_INVALID))
                return true;
        }
        if (TEST(MIIM_BITMAP, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, hbmpItem)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->hbmpItem,
                               sizeof(real->hbmpItem), "MENUITEMINFOW.hbmpItem"))
                return true;
        }
        if (TEST(MIIM_CHECKMARKS, safe.fMask)) {
            if (safe.cbSize > offsetof(MENUITEMINFOW, hbmpChecked)) {
                if (!report_memarg(ii, arg_info, (byte *)&real->hbmpChecked,
                                   sizeof(real->hbmpChecked),
                                   "MENUITEMINFOW.hbmpChecked"))
                    return true;
            }
            if (safe.cbSize > offsetof(MENUITEMINFOW, hbmpUnchecked)) {
                if (!report_memarg(ii, arg_info, (byte *)&real->hbmpUnchecked,
                                   sizeof(real->hbmpUnchecked),
                                   "MENUITEMINFOW.hbmpUnchecked"))
                    return true;
            }
        }
        if (TEST(MIIM_DATA, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, dwItemData)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->dwItemData,
                               sizeof(real->dwItemData), "MENUITEMINFOW.dwItemData"))
                return true;
        }
        if (TEST(MIIM_FTYPE, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, fType)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->fType, sizeof(real->fType),
                               "MENUITEMINFOW.fType"))
                return true;
        }
        if (TEST(MIIM_ID, safe.fMask) && safe.cbSize > offsetof(MENUITEMINFOW, wID)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->wID, sizeof(real->wID),
                               "MENUITEMINFOW.wID"))
                return true;
        }
        if (TEST(MIIM_STATE, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, fState)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->fState, sizeof(real->fState),
                               "MENUITEMINFOW.fState"))
                return true;
        }
        if (TEST(MIIM_STRING, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, dwTypeData)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->dwTypeData,
                               sizeof(real->dwTypeData), "MENUITEMINFOW.dwTypeData"))
                return true;
            check_dwTypeData = true;
        }
        if (TEST(MIIM_SUBMENU, safe.fMask) &&
            safe.cbSize > offsetof(MENUITEMINFOW, hSubMenu)) {
            if (!report_memarg(ii, arg_info, (byte *)&real->hSubMenu,
                               sizeof(real->hSubMenu), "MENUITEMINFOW.hSubMenu"))
                return true;
        }
        if (TEST(MIIM_TYPE, safe.fMask) &&
            !TESTANY(MIIM_BITMAP | MIIM_FTYPE | MIIM_STRING, safe.fMask)) {
            if (safe.cbSize > offsetof(MENUITEMINFOW, fType)) {
                if (!report_memarg(ii, arg_info, (byte *)&real->fType,
                                   sizeof(real->fType), "MENUITEMINFOW.fType"))
                    return true;
            }
            if (safe.cbSize > offsetof(MENUITEMINFOW, dwTypeData)) {
                if (!report_memarg(ii, arg_info, (byte *)&real->dwTypeData,
                                   sizeof(real->dwTypeData), "MENUITEMINFOW.dwTypeData"))
                    return true;
                check_dwTypeData = true;
            }
        }
        if (check_dwTypeData) {
            /* i#1463: when retrieving, kernel sets safe.cch so we don't have
             * to walk the string.  When setting, cch is ignored.
             */
            if (TEST(SYSARG_WRITE, arg_info->flags)) {
                if (safe.cbSize > offsetof(MENUITEMINFOW, cch)) {
                    if (ii->arg->pre) {
                        /* User must set cch to capacity of dwTypeData */
                        if (!report_memarg(ii, arg_info, (byte *)&real->cch,
                                           sizeof(real->cch), "MENUITEMINFOW.cch"))
                            return true;
                    }
                    if (!report_memarg(ii, arg_info, (byte *)safe.dwTypeData,
                                       (safe.cch + 1 /*null*/) * sizeof(wchar_t),
                                       "MENUITEMINFOW.dwTypeData"))
                        return true;
                }
            } else {
                handle_cwstring(ii, "MENUITEMINFOW.dwTypeData", (byte *)safe.dwTypeData,
                                0, arg_info->param, arg_info->flags, NULL, true);
                if (ii->abort)
                    return true;
            }
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
    return true; /* handled */
}

bool
handle_bitmapinfo_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                         app_pc start, uint size)
{
    /* bmiColors is variable-length and the number of entries in the
     * array depends on the values of the biBitCount and biClrUsed
     * members of the BITMAPINFOHEADER struct.
     */
    BITMAPINFOHEADER bmi;
    size = sizeof(bmi);

    if (safe_read(start, sizeof(bmi), &bmi)) {
        if (bmi.biSize != sizeof(bmi))
            WARN("WARNING: biSize: %d != sizeof(bmi): %d", bmi.biSize, sizeof(bmi));
        switch (bmi.biBitCount) {
        case 0: break;
        case 1:
            /* bmiColors contains two entries */
            size += 2 * sizeof(RGBQUAD);
            break;
        case 4:
            /* If bmiClrUsed is 0 then bmiColors contains 16 entries,
             * otherwise bmiColors contains the number in bmiClrUsed.
             */
            if (bmi.biClrUsed == 0)
                size += 16 * sizeof(RGBQUAD);
            else
                size += bmi.biClrUsed * sizeof(RGBQUAD);
            break;
        case 8:
            /* Same as case 4, except max of 256 entries */
            if (bmi.biClrUsed == 0)
                size += 256 * sizeof(RGBQUAD);
            else
                size += bmi.biClrUsed * sizeof(RGBQUAD);
            break;
        case 16:
        case 32:
            /* If biCompression is BI_RGB, then bmiColors is not used. If it is
             * BI_BITFIELDS, then it contains 3 DWORD color masks. If it's a
             * palette-based device, the color table starts immediately following
             * the 3 DWORD color masks.
             */
            if (bmi.biCompression == BI_BITFIELDS)
                size += 3 * sizeof(DWORD);
            if (bmi.biClrUsed != 0)
                size += bmi.biClrUsed * sizeof(RGBQUAD);
            break;
        case 24:
            /* bmiColors is not used unless used on pallete-based devices */
            if (bmi.biClrUsed != 0)
                size += bmi.biClrUsed * sizeof(RGBQUAD);
            break;
        default: WARN("WARNING: biBitCount should not be %d\n", bmi.biBitCount); break;
        }
    }

    if (!report_memarg(ii, arg_info, start, size, NULL))
        return true;
    return true;
}

static void
handle_logfont(sysarg_iter_info_t *ii, byte *start, size_t size, int ordinal,
               uint arg_flags, LOGFONTW *safe)
{
    LOGFONTW *font = (LOGFONTW *)start;
    if (ii->arg->pre && TEST(SYSARG_WRITE, arg_flags)) {
        if (!report_memarg_type(ii, ordinal, arg_flags, start, size, "LOGFONTW",
                                DRSYS_TYPE_LOGFONTW, NULL))
            return;
    } else {
        size_t check_sz;
        if (size == 0) {
            /* i#873: existing code passes in 0 for the size, which violates
             * the MSDN docs, yet the kernel doesn't care and still returns
             * success.  Thus we don't report as an error and we make
             * it work.
             */
            size = sizeof(LOGFONTW);
        }
        check_sz = MIN(size - offsetof(LOGFONTW, lfFaceName), sizeof(font->lfFaceName));
        ASSERT(size >= offsetof(LOGFONTW, lfFaceName), "invalid size");
        if (!report_memarg_type(ii, ordinal, arg_flags, start,
                                offsetof(LOGFONTW, lfFaceName), "LOGFONTW",
                                DRSYS_TYPE_LOGFONTW, NULL))
            return;
        handle_cwstring(ii, "LOGFONTW.lfFaceName", (byte *)&font->lfFaceName, check_sz,
                        ordinal, arg_flags,
                        (safe == NULL) ? NULL : (wchar_t *)&safe->lfFaceName, true);
        if (ii->abort)
            return;
    }
}

static void
handle_nonclientmetrics(sysarg_iter_info_t *ii, byte *start, size_t size_specified,
                        int ordinal, uint arg_flags, NONCLIENTMETRICSW *safe)
{
    NONCLIENTMETRICSW *ptr_arg = (NONCLIENTMETRICSW *)start;
    NONCLIENTMETRICSW *ptr_safe;
    NONCLIENTMETRICSW ptr_local;
    size_t size;
    if (safe != NULL)
        ptr_safe = safe;
    else {
        if (!safe_read(start, sizeof(ptr_local), &ptr_local)) {
            WARN("WARNING: unable to read syscall param\n");
            return;
        }
        ptr_safe = &ptr_local;
    }
    /* Turns out that despite user32!SystemParametersInfoA requiring both uiParam
     * and cbSize, it turns around and calls NtUserSystemParametersInfo w/o
     * initializing cbSize!  Plus, it passes the A size instead of the W size!
     * Ditto on SET where it keeps the A size in the temp struct cbSize.
     * So we don't check that ptr_arg->cbSize is defined for pre-write
     * and we pretty much ignore the uiParam and cbSize values except
     * post-write (kernel puts in the right size).  Crazy.
     */
    LOG(ii->arg->drcontext, 2,
        "NONCLIENTMETRICSW %s: sizeof(NONCLIENTMETRICSW)=%x, cbSize=%x, uiParam=%x\n",
        TEST(SYSARG_WRITE, arg_flags) ? "write" : "read", sizeof(NONCLIENTMETRICSW),
        ptr_safe->cbSize, size_specified);
    /* win7 seems to set cbSize properly, always */
    if (win_ver.version >= DR_WINDOWS_VERSION_7 ||
        (!ii->arg->pre && TEST(SYSARG_WRITE, arg_flags)))
        size = ptr_safe->cbSize;
    else {
        /* MAX to handle future additions.  I don't think older versions
         * have smaller NONCLIENTMETRICSW than anywhere we're compiling.
         */
        size = MAX(sizeof(NONCLIENTMETRICSW), size_specified);
    }

    if (ii->arg->pre && TEST(SYSARG_WRITE, arg_flags)) {
        if (!report_memarg_type(ii, ordinal, arg_flags, start, size, "NONCLIENTMETRICSW",
                                DRSYS_TYPE_NONCLIENTMETRICSW, NULL))
            return;
    } else {
        size_t offs = 0;
        size_t check_sz = MIN(size, offsetof(NONCLIENTMETRICSW, lfCaptionFont));
        if (!report_memarg_type(ii, ordinal, arg_flags, start, check_sz,
                                "NONCLIENTMETRICSW A", DRSYS_TYPE_NONCLIENTMETRICSW,
                                NULL))
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfCaptionFont, check_sz, ordinal, arg_flags,
                       &ptr_safe->lfCaptionFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs,
                       offsetof(NONCLIENTMETRICSW, lfSmCaptionFont) -
                           offsetof(NONCLIENTMETRICSW, iSmCaptionWidth));
        if (!report_memarg_type(ii, ordinal, arg_flags, (byte *)&ptr_arg->iSmCaptionWidth,
                                check_sz, "NONCLIENTMETRICSW B",
                                DRSYS_TYPE_NONCLIENTMETRICSW, NULL))
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfSmCaptionFont, check_sz, ordinal,
                       arg_flags, &ptr_safe->lfSmCaptionFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs,
                       offsetof(NONCLIENTMETRICSW, lfMenuFont) -
                           offsetof(NONCLIENTMETRICSW, iMenuWidth));
        if (!report_memarg_type(ii, ordinal, arg_flags, (byte *)&ptr_arg->iMenuWidth,
                                check_sz, "NONCLIENTMETRICSW B",
                                DRSYS_TYPE_NONCLIENTMETRICSW, NULL))
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfMenuFont, check_sz, ordinal, arg_flags,
                       &ptr_safe->lfMenuFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfStatusFont, check_sz, ordinal, arg_flags,
                       &ptr_safe->lfStatusFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfMessageFont, check_sz, ordinal, arg_flags,
                       &ptr_safe->lfMessageFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        /* there is another field on Vista */
        check_sz = size - offs;
        if (!report_memarg_type(ii, ordinal, arg_flags, ((byte *)ptr_arg) + offs,
                                check_sz, "NONCLIENTMETRICSW C",
                                DRSYS_TYPE_NONCLIENTMETRICSW, NULL))
            return;
    }
}

static void
handle_iconmetrics(sysarg_iter_info_t *ii, byte *start, int ordinal, uint arg_flags,
                   ICONMETRICSW *safe)
{
    ICONMETRICSW *ptr_arg = (ICONMETRICSW *)start;
    ICONMETRICSW *ptr_safe;
    ICONMETRICSW ptr_local;
    size_t size;
    if (safe != NULL)
        ptr_safe = safe;
    else {
        if (!safe_read(start, sizeof(ptr_local), &ptr_local)) {
            WARN("WARNING: unable to read syscall param\n");
            return;
        }
        ptr_safe = &ptr_local;
    }
    size = ptr_safe->cbSize;

    if (ii->arg->pre && TEST(SYSARG_WRITE, arg_flags)) {
        if (!report_memarg_type(ii, ordinal, arg_flags, start, size, "ICONMETRICSW",
                                DRSYS_TYPE_ICONMETRICSW, NULL))
            return;
    } else {
        size_t offs = 0;
        size_t check_sz = MIN(size, offsetof(ICONMETRICSW, lfFont));
        if (!report_memarg_type(ii, ordinal, arg_flags, start, check_sz, "ICONMETRICSW A",
                                DRSYS_TYPE_ICONMETRICSW, NULL))
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        check_sz = MIN(size - offs, sizeof(LOGFONTW));
        handle_logfont(ii, (byte *)&ptr_arg->lfFont, check_sz, ordinal, arg_flags,
                       &ptr_safe->lfFont);
        if (ii->abort)
            return;
        offs += check_sz;
        if (offs >= size)
            return;

        /* currently no more args, but here for forward compat */
        check_sz = size - offs;
        if (!report_memarg_type(ii, ordinal, arg_flags, ((byte *)ptr_arg) + offs,
                                check_sz, "ICONMETRICSW B", DRSYS_TYPE_ICONMETRICSW,
                                NULL))
            return;
    }
}

static void
handle_serialkeys(sysarg_iter_info_t *ii, byte *start, int ordinal, uint arg_flags,
                  SERIALKEYSW *safe)
{
    SERIALKEYSW *ptr_safe;
    SERIALKEYSW ptr_local;
    size_t size;
    if (safe != NULL)
        ptr_safe = safe;
    else {
        if (!safe_read(start, sizeof(ptr_local), &ptr_local)) {
            WARN("WARNING: unable to read syscall param\n");
            return;
        }
        ptr_safe = &ptr_local;
    }
    size = ptr_safe->cbSize;
    if (!report_memarg_type(ii, ordinal, arg_flags, start, size, "SERIALKEYSW",
                            DRSYS_TYPE_SERIALKEYSW, NULL))
        return;
    handle_cwstring(ii, "SERIALKEYSW.lpszActivePort", (byte *)ptr_safe->lpszActivePort, 0,
                    ordinal, arg_flags, NULL, true);
    if (ii->abort)
        return;
    handle_cwstring(ii, "SERIALKEYSW.lpszPort", (byte *)ptr_safe->lpszPort, 0, ordinal,
                    arg_flags, NULL, true);
}

static void
handle_cwstring_field(sysarg_iter_info_t *ii, const char *id, int ordinal, uint arg_flags,
                      byte *struct_start, size_t struct_size, size_t cwstring_offs)
{
    wchar_t *ptr;
    if (struct_size <= cwstring_offs)
        return;
    if (!safe_read(struct_start + cwstring_offs, sizeof(ptr), &ptr)) {
        WARN("WARNING: unable to read syscall param\n");
        return;
    }
    handle_cwstring(ii, id, (byte *)ptr, 0, ordinal, arg_flags, NULL, true);
}

bool
wingdi_process_arg(sysarg_iter_info_t *iter_info, const sysinfo_arg_t *arg_info,
                   app_pc start, uint size)
{
    switch (arg_info->misc) {
    case SYSARG_TYPE_LARGE_STRING:
        return handle_large_string_access(iter_info, arg_info, start, size);
    case SYSARG_TYPE_DEVMODEW:
        return handle_devmodew_access(iter_info, arg_info, start, size);
    case SYSARG_TYPE_WNDCLASSEXW:
        return handle_wndclassexw_access(iter_info, arg_info, start, size);
    case SYSARG_TYPE_CLSMENUNAME:
        return handle_clsmenuname_access(iter_info, arg_info, start, size);
    case SYSARG_TYPE_MENUITEMINFOW:
        return handle_menuiteminfow_access(iter_info, arg_info, start, size);
    case SYSARG_TYPE_BITMAPINFO:
        return handle_bitmapinfo_access(iter_info, arg_info, start, size);
    }
    return false; /* not handled */
}

/***************************************************************************
 * CUSTOM SYSCALL HANDLING
 */

static void
handle_UserSystemParametersInfo(void *drcontext, cls_syscall_t *pt,
                                sysarg_iter_info_t *ii)
{
    UINT uiAction = (UINT)pt->sysarg[0];
    UINT uiParam = (UINT)pt->sysarg[1];
#define PV_PARAM_ORDINAL 2
    byte *pvParam = (byte *)pt->sysarg[PV_PARAM_ORDINAL];
    bool get = true;
    size_t sz = 0;
    bool uses_pvParam = false; /* also considered used if sz>0 */
    bool uses_uiParam = false;

    switch (uiAction) {
    case SPI_GETBEEP:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETBEEP:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSE:
        get = true;
        sz = 3 * sizeof(INT);
        break;
    case SPI_SETMOUSE:
        get = false;
        sz = 3 * sizeof(INT);
        break;
    case SPI_GETBORDER:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETBORDER:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETKEYBOARDSPEED:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETKEYBOARDSPEED:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSCREENSAVETIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETSCREENSAVETIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSCREENSAVEACTIVE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSCREENSAVEACTIVE:
        get = false;
        uses_uiParam = true;
        break;
    /* XXX: no official docs for these 2: */
    case SPI_GETGRIDGRANULARITY:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETGRIDGRANULARITY:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETDESKWALLPAPER: {
        /* uiParam is size in characters */
        handle_cwstring(ii, "pvParam", pvParam, uiParam * sizeof(wchar_t),
                        PV_PARAM_ORDINAL, SYSARG_WRITE, NULL, true);
        if (ii->abort)
            return;
        get = true;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETDESKWALLPAPER: {
        sysinfo_arg_t arg = { PV_PARAM_ORDINAL, sizeof(UNICODE_STRING),
                              SYSARG_READ | SYSARG_COMPLEX_TYPE,
                              SYSARG_TYPE_UNICODE_STRING };
        handle_unicode_string_access(ii, &arg, pvParam, sizeof(UNICODE_STRING), false);
        if (ii->abort)
            return;
        get = false;
        uses_pvParam = true;
        break;
    }
    case SPI_SETDESKPATTERN: get = false; break;
    case SPI_GETKEYBOARDDELAY:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETKEYBOARDDELAY:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_ICONHORIZONTALSPACING: {
        if (pvParam != NULL) {
            get = true;
            sz = sizeof(int);
        } else {
            get = false;
            uses_uiParam = true;
        }
        break;
    }
    case SPI_ICONVERTICALSPACING: {
        if (pvParam != NULL) {
            get = true;
            sz = sizeof(int);
        } else {
            get = false;
            uses_uiParam = true;
        }
        break;
    }
    case SPI_GETICONTITLEWRAP:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETICONTITLEWRAP:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMENUDROPALIGNMENT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETMENUDROPALIGNMENT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETDOUBLECLKWIDTH:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETDOUBLECLKHEIGHT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETICONTITLELOGFONT: {
        handle_logfont(ii, pvParam, uiParam, PV_PARAM_ORDINAL, SYSARG_WRITE, NULL);
        if (ii->abort)
            return;
        get = true;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETICONTITLELOGFONT: {
        handle_logfont(ii, pvParam, uiParam, PV_PARAM_ORDINAL, SYSARG_READ, NULL);
        if (ii->abort)
            return;
        get = false;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETDOUBLECLICKTIME:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETMOUSEBUTTONSWAP:
        get = false;
        uses_uiParam = true;
        break;
    /* XXX: no official docs: */
    case SPI_GETFASTTASKSWITCH:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_GETDRAGFULLWINDOWS:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETDRAGFULLWINDOWS:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETNONCLIENTMETRICS: {
        handle_nonclientmetrics(ii, pvParam, uiParam, PV_PARAM_ORDINAL, SYSARG_WRITE,
                                NULL);
        if (ii->abort)
            return;
        get = true;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETNONCLIENTMETRICS: {
        handle_nonclientmetrics(ii, pvParam, uiParam, PV_PARAM_ORDINAL, SYSARG_READ,
                                NULL);
        if (ii->abort)
            return;
        get = false;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_GETMINIMIZEDMETRICS:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETMINIMIZEDMETRICS:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETICONMETRICS: {
        handle_iconmetrics(ii, pvParam, PV_PARAM_ORDINAL, SYSARG_WRITE, NULL);
        if (ii->abort)
            return;
        get = true;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETICONMETRICS: {
        handle_iconmetrics(ii, pvParam, PV_PARAM_ORDINAL, SYSARG_READ, NULL);
        if (ii->abort)
            return;
        get = false;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_GETWORKAREA:
        get = true;
        sz = sizeof(RECT);
        break;
    case SPI_SETWORKAREA:
        get = false;
        sz = sizeof(RECT);
        break;
    case SPI_GETFILTERKEYS:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETFILTERKEYS:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETTOGGLEKEYS:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETTOGGLEKEYS:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETMOUSEKEYS:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETMOUSEKEYS:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETSHOWSOUNDS:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSHOWSOUNDS:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSTICKYKEYS:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETSTICKYKEYS:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETACCESSTIMEOUT:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETACCESSTIMEOUT:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETSERIALKEYS: {
        handle_serialkeys(ii, pvParam, PV_PARAM_ORDINAL, SYSARG_WRITE, NULL);
        if (ii->abort)
            return;
        get = true;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_SETSERIALKEYS: {
        handle_serialkeys(ii, pvParam, PV_PARAM_ORDINAL, SYSARG_READ, NULL);
        if (ii->abort)
            return;
        get = false;
        uses_uiParam = true;
        uses_pvParam = true;
        break;
    }
    case SPI_GETSOUNDSENTRY: {
        handle_cwstring_field(ii, "SOUNDSENTRYW.lpszWindowsEffectDLL", PV_PARAM_ORDINAL,
                              SYSARG_WRITE, pvParam, uiParam,
                              offsetof(SOUNDSENTRYW, lpszWindowsEffectDLL));
        if (ii->abort)
            return;
        /* rest of struct handled through pvParam check below */
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    }
    case SPI_SETSOUNDSENTRY: {
        handle_cwstring_field(ii, "SOUNDSENTRYW.lpszWindowsEffectDLL", PV_PARAM_ORDINAL,
                              SYSARG_READ, pvParam, uiParam,
                              offsetof(SOUNDSENTRYW, lpszWindowsEffectDLL));
        if (ii->abort)
            return;
        /* rest of struct handled through pvParam check below */
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    }
    case SPI_GETHIGHCONTRAST: {
        handle_cwstring_field(ii, "HIGHCONTRASTW.lpszDefaultScheme", PV_PARAM_ORDINAL,
                              SYSARG_WRITE, pvParam, uiParam,
                              offsetof(HIGHCONTRASTW, lpszDefaultScheme));
        if (ii->abort)
            return;
        /* rest of struct handled through pvParam check below */
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    }
    case SPI_SETHIGHCONTRAST: {
        handle_cwstring_field(ii, "HIGHCONTRASTW.lpszDefaultScheme", PV_PARAM_ORDINAL,
                              SYSARG_READ, pvParam, uiParam,
                              offsetof(HIGHCONTRASTW, lpszDefaultScheme));
        if (ii->abort)
            return;
        /* rest of struct handled through pvParam check below */
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    }
    case SPI_GETKEYBOARDPREF:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETKEYBOARDPREF:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSCREENREADER:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSCREENREADER:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETANIMATION:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_SETANIMATION:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETFONTSMOOTHING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETFONTSMOOTHING:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETDRAGWIDTH:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETDRAGHEIGHT:
        get = false;
        uses_uiParam = true;
        break;
    /* XXX: no official docs: */
    case SPI_SETHANDHELD:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETLOWPOWERTIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_GETPOWEROFFTIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETLOWPOWERTIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETPOWEROFFTIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETLOWPOWERACTIVE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_GETPOWEROFFACTIVE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETLOWPOWERACTIVE:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_SETPOWEROFFACTIVE:
        get = false;
        uses_uiParam = true;
        break;
    /* XXX: docs say to set uiParam=0 and pvParam=NULL; we don't check init */
    case SPI_SETCURSORS: get = false; break;
    case SPI_SETICONS: get = false; break;
    case SPI_GETDEFAULTINPUTLANG:
        get = true;
        sz = sizeof(HKL);
        break;
    case SPI_SETDEFAULTINPUTLANG:
        get = false;
        sz = sizeof(HKL);
        break;
    case SPI_SETLANGTOGGLE: get = false; break;
    case SPI_GETMOUSETRAILS:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETMOUSETRAILS:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSNAPTODEFBUTTON:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSNAPTODEFBUTTON:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSEHOVERWIDTH:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETMOUSEHOVERWIDTH:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSEHOVERHEIGHT:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETMOUSEHOVERHEIGHT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSEHOVERTIME:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETMOUSEHOVERTIME:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETWHEELSCROLLLINES:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETWHEELSCROLLLINES:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMENUSHOWDELAY:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETMENUSHOWDELAY:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETWHEELSCROLLCHARS:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETWHEELSCROLLCHARS:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSHOWIMEUI:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSHOWIMEUI:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSESPEED:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETMOUSESPEED:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSCREENSAVERRUNNING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSCREENSAVERRUNNING:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETAUDIODESCRIPTION:
        get = true;
        uses_uiParam = true;
        sz = uiParam;
        break;
    /* XXX: docs don't actually say to set uiParam: I'm assuming for symmetry */
    case SPI_SETAUDIODESCRIPTION:
        get = false;
        uses_uiParam = true;
        sz = uiParam;
        break;
    case SPI_GETSCREENSAVESECURE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSCREENSAVESECURE:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETHUNGAPPTIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETHUNGAPPTIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETWAITTOKILLTIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETWAITTOKILLTIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETWAITTOKILLSERVICETIMEOUT:
        get = true;
        sz = sizeof(int);
        break;
    case SPI_SETWAITTOKILLSERVICETIMEOUT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSEDOCKTHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    /* Note that many of the sets below use pvParam as either an inlined BOOL
     * or a pointer to a DWORD (why not inlined?), instead of using uiParam
     */
    case SPI_SETMOUSEDOCKTHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    /* XXX: docs don't say it writes to pvParam: ret val instead? */
    case SPI_GETPENDOCKTHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETPENDOCKTHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    case SPI_GETWINARRANGING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETWINARRANGING:
        get = false;
        uses_pvParam = true;
        break;
    /* XXX: docs don't say it writes to pvParam: ret val instead? */
    case SPI_GETMOUSEDRAGOUTTHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETMOUSEDRAGOUTTHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    /* XXX: docs don't say it writes to pvParam: ret val instead? */
    case SPI_GETPENDRAGOUTTHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETPENDRAGOUTTHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    /* XXX: docs don't say it writes to pvParam: ret val instead? */
    case SPI_GETMOUSESIDEMOVETHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETMOUSESIDEMOVETHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    /* XXX: docs don't say it writes to pvParam: ret val instead? */
    case SPI_GETPENSIDEMOVETHRESHOLD:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETPENSIDEMOVETHRESHOLD:
        get = false;
        sz = sizeof(DWORD);
        break;
    case SPI_GETDRAGFROMMAXIMIZE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETDRAGFROMMAXIMIZE:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETSNAPSIZING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSNAPSIZING:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETDOCKMOVING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETDOCKMOVING:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETACTIVEWINDOWTRACKING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETACTIVEWINDOWTRACKING:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMENUANIMATION:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETMENUANIMATION:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETCOMBOBOXANIMATION:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETCOMBOBOXANIMATION:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETLISTBOXSMOOTHSCROLLING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETLISTBOXSMOOTHSCROLLING:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETGRADIENTCAPTIONS:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETGRADIENTCAPTIONS:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETKEYBOARDCUES:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETKEYBOARDCUES:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETACTIVEWNDTRKZORDER:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETACTIVEWNDTRKZORDER:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETHOTTRACKING:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETHOTTRACKING:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMENUFADE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETMENUFADE:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETSELECTIONFADE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSELECTIONFADE:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETTOOLTIPANIMATION:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETTOOLTIPANIMATION:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETTOOLTIPFADE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETTOOLTIPFADE:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETCURSORSHADOW:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETCURSORSHADOW:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMOUSESONAR:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETMOUSESONAR:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETMOUSECLICKLOCK:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETMOUSECLICKLOCK:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMOUSEVANISH:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETMOUSEVANISH:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETFLATMENU:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETFLATMENU:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETDROPSHADOW:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETDROPSHADOW:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETBLOCKSENDINPUTRESETS:
        get = true;
        sz = sizeof(BOOL);
        break;
    /* yes this is uiParam in the midst of many pvParams */
    case SPI_SETBLOCKSENDINPUTRESETS:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETUIEFFECTS:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETUIEFFECTS:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETDISABLEOVERLAPPEDCONTENT:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETDISABLEOVERLAPPEDCONTENT:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETCLIENTAREAANIMATION:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETCLIENTAREAANIMATION:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETCLEARTYPE:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETCLEARTYPE:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETSPEECHRECOGNITION:
        get = true;
        sz = sizeof(BOOL);
        break;
    case SPI_SETSPEECHRECOGNITION:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETFOREGROUNDLOCKTIMEOUT:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETFOREGROUNDLOCKTIMEOUT:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETACTIVEWNDTRKTIMEOUT:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETACTIVEWNDTRKTIMEOUT:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETFOREGROUNDFLASHCOUNT:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETFOREGROUNDFLASHCOUNT:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETCARETWIDTH:
        get = true;
        sz = sizeof(DWORD);
        break;
    case SPI_SETCARETWIDTH:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMOUSECLICKLOCKTIME:
        get = true;
        sz = sizeof(DWORD);
        break;
    /* yes this is uiParam in the midst of many pvParams */
    case SPI_SETMOUSECLICKLOCKTIME:
        get = false;
        uses_uiParam = true;
        break;
    case SPI_GETFONTSMOOTHINGTYPE:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETFONTSMOOTHINGTYPE:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETFONTSMOOTHINGCONTRAST:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETFONTSMOOTHINGCONTRAST:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETFOCUSBORDERWIDTH:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETFOCUSBORDERWIDTH:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETFOCUSBORDERHEIGHT:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETFOCUSBORDERHEIGHT:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETFONTSMOOTHINGORIENTATION:
        get = true;
        sz = sizeof(UINT);
        break;
    case SPI_SETFONTSMOOTHINGORIENTATION:
        get = false;
        uses_pvParam = true;
        break;
    case SPI_GETMESSAGEDURATION:
        get = true;
        sz = sizeof(ULONG);
        break;
    case SPI_SETMESSAGEDURATION:
        get = false;
        uses_pvParam = true;
        break;

    /* XXX: unknown behavior */
    case SPI_LANGDRIVER:
    case SPI_SETFASTTASKSWITCH:
    case SPI_SETPENWINDOWS:
    case SPI_GETWINDOWSEXTENSION:
    default:
        WARN("WARNING: unhandled UserSystemParametersInfo uiAction 0x%x\n", uiAction);
    }

    /* table entry only checked uiAction for definedness */
    if (uses_uiParam && ii->arg->pre) {
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
    }
    if (sz > 0 || uses_pvParam) { /* pvParam is used */
        if (ii->arg->pre) {
            if (!report_sysarg(ii, 2, get ? SYSARG_WRITE : SYSARG_READ))
                return;
        }
        if (get && sz > 0) {
            if (!report_memarg_type(
                    ii, PV_PARAM_ORDINAL, SYSARG_WRITE, pvParam, sz, "pvParam",
                    sz == sizeof(int) ? DRSYS_TYPE_INT : DRSYS_TYPE_STRUCT, NULL))
                return;
        } else if (ii->arg->pre && sz > 0) {
            if (!report_memarg_type(
                    ii, PV_PARAM_ORDINAL, SYSARG_READ, pvParam, sz, "pvParam",
                    sz == sizeof(int) ? DRSYS_TYPE_INT : DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    }
    if (!get && ii->arg->pre) /* fWinIni used for all SET codes */
        report_sysarg(ii, 3, SYSARG_READ);
}

static void
handle_UserMenuInfo(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* 3rd param is bool saying whether it's Set or Get */
    BOOL set = (BOOL)pt->sysarg[2];
    MENUINFO info;
    /* user must set cbSize for set or get */
    if (ii->arg->pre) {
        if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)pt->sysarg[1],
                                sizeof(info.cbSize), "MENUINFOW.cbSize", DRSYS_TYPE_INT,
                                NULL))
            return;
    }
    if (ii->arg->pre || !set) {
        if (safe_read((byte *)pt->sysarg[1], sizeof(info), &info)) {
            if (!report_memarg_type(ii, 1, set ? SYSARG_READ : SYSARG_WRITE,
                                    (byte *)pt->sysarg[1], info.cbSize, "MENUINFOW",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        } else
            WARN("WARNING: unable to read syscall param\n");
    }
}

static void
handle_UserMenuItemInfo(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* 4th param is bool saying whether it's Set or Get */
    BOOL set = (BOOL)pt->sysarg[4];
    sysinfo_arg_t arg = { 3, 0, (set ? SYSARG_READ : SYSARG_WRITE) | SYSARG_COMPLEX_TYPE,
                          SYSARG_TYPE_MENUITEMINFOW };
    handle_menuiteminfow_access(ii, &arg, (byte *)pt->sysarg[3], 0);
}

static void
handle_UserGetAltTabInfo(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* buffer is ansi or unicode depending on arg 5; size (arg 4) is in chars */
    BOOL ansi = (BOOL)pt->sysarg[5];
    UINT count = (UINT)pt->sysarg[4];
    report_memarg_type(ii, 3, SYSARG_WRITE, (byte *)pt->sysarg[3],
                       count * (ansi ? sizeof(char) : sizeof(wchar_t)), "pszItemText",
                       ansi ? DRSYS_TYPE_CARRAY : DRSYS_TYPE_CWARRAY, NULL);
    report_sysarg_type(ii, 3, SYSARG_READ,
                       count * (ansi ? sizeof(char) : sizeof(wchar_t)),
                       ansi ? DRSYS_TYPE_CARRAY : DRSYS_TYPE_CWARRAY, "pszItemText");
}

static void
handle_UserGetRawInputBuffer(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    byte *buf = (byte *)pt->sysarg[0];
    UINT size;
    if (buf == NULL) {
        /* writes out total buffer size needed in bytes to param #1 */
        if (!report_memarg_type(ii, 1, SYSARG_WRITE, (byte *)pt->sysarg[1], sizeof(UINT),
                                "pcbSize", DRSYS_TYPE_INT, NULL))
            return;
    } else {
        if (ii->arg->pre) {
            /* XXX i#485: we don't know the number of array entries so we
             * can't check addressability pre-syscall: comes from a prior
             * buf==NULL call
             */
        } else if (safe_read((byte *)pt->sysarg[1], sizeof(size), &size)) {
            /* param #1 holds size of each RAWINPUT array entry */
            size = (size * dr_syscall_get_result(drcontext)) +
                /* param #2 holds header size */
                (UINT)pt->sysarg[2];
            report_sysarg_type(ii, 0, SYSARG_READ, size, DRSYS_TYPE_STRUCT, "pData");
            if (!report_memarg_type(ii, 0, SYSARG_WRITE, buf, size, "pData",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        } else
            WARN("WARNING: unable to read syscall param\n");
    }
}

static void
handle_UserGetRawInputData(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    byte *buf = (byte *)pt->sysarg[2];
    /* arg #3 is either R or W.  when W buf must be NULL and the 2,-3,WI entry
     * will do a safe_read but won't do a check so no false pos.
     */
    if (buf == NULL || ii->arg->pre) {
        uint flags = ((buf == NULL) ? SYSARG_WRITE : SYSARG_READ);
        report_memarg_type(ii, 3, flags, (byte *)pt->sysarg[3], sizeof(UINT), "pcbSize",
                           DRSYS_TYPE_INT, NULL);
    }
}

static void
handle_UserGetRawInputDeviceInfo(void *drcontext, cls_syscall_t *pt,
                                 sysarg_iter_info_t *ii)
{
    UINT uiCommand = (UINT)pt->sysarg[1];
    UINT size;
    if (safe_read((byte *)pt->sysarg[3], sizeof(size), &size)) {
        /* for uiCommand == RIDI_DEVICEINFO we assume pcbSize (3rd param)
         * will be set and we don't bother to check RID_DEVICE_INFO.cbSize
         */
        if (uiCommand == RIDI_DEVICENAME) {
            /* output is a string and size is in chars
             * XXX: I'm assuming a wide string!
             */
            size *= sizeof(wchar_t);
        }
        report_sysarg_type(ii, 2, SYSARG_READ, size, DRSYS_TYPE_STRUCT, "pData");
        if (!report_memarg_type(ii, 2, SYSARG_WRITE, (byte *)pt->sysarg[2], size, "pData",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (pt->sysarg[2] == 0) {
            /* XXX i#486: if buffer is not large enough, returns -1 but still
             * sets *pcbSize
             */
            if (!report_memarg_type(ii, 3, SYSARG_WRITE, (byte *)pt->sysarg[3],
                                    sizeof(UINT), "pData", DRSYS_TYPE_INT, NULL))
                return;
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
}

static void
handle_UserTrackMouseEvent(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    TRACKMOUSEEVENT *safe;
    byte buf[offsetof(TRACKMOUSEEVENT, dwFlags) + sizeof(safe->dwFlags)];
    /* user must set cbSize and dwFlags */
    if (ii->arg->pre) {
        report_sysarg_type(ii, 0, SYSARG_READ,
                           offsetof(TRACKMOUSEEVENT, dwFlags) + sizeof(safe->dwFlags),
                           DRSYS_TYPE_STRUCT, "TRACKMOUSEEVENT cbSize+dwFlags");
        if (!report_memarg_type(
                ii, 0, SYSARG_READ, (byte *)pt->sysarg[0],
                offsetof(TRACKMOUSEEVENT, dwFlags) + sizeof(safe->dwFlags),
                "TRACKMOUSEEVENT cbSize+dwFlags", DRSYS_TYPE_STRUCT, NULL))
            return;
    }
    if (safe_read((byte *)pt->sysarg[0], BUFFER_SIZE_BYTES(buf), buf)) {
        uint flags;
        safe = (TRACKMOUSEEVENT *)buf;
        /* XXX: for non-TME_QUERY are the other fields read? */
        flags = TEST(TME_QUERY, safe->dwFlags) ? SYSARG_WRITE : SYSARG_READ;
        if ((flags == SYSARG_WRITE || ii->arg->pre) &&
            safe->cbSize > BUFFER_SIZE_BYTES(buf)) {
            report_sysarg_type(ii, 0, SYSARG_READ, safe->cbSize - BUFFER_SIZE_BYTES(buf),
                               DRSYS_TYPE_STRUCT, "TRACKMOUSEEVENT cbSize+dwFlags");
            if (!report_memarg_type(
                    ii, 0, flags, ((byte *)pt->sysarg[0]) + BUFFER_SIZE_BYTES(buf),
                    safe->cbSize - BUFFER_SIZE_BYTES(buf), "TRACKMOUSEEVENT post-dwFlags",
                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    } else
        WARN("WARNING: unable to read syscall param\n");
}

static void
handle_UserMessageCall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* i#1249: behavior depends on both 2nd param (WM_* and other message codes)
     * and 6th param (major action requested: FNID_* codes).
     * See comments in table: if enough of these turn out to be different we
     * might want a secondary table(s) instead.
     */
#define ORD_WPARAM 2
#define ORD_LPARAM 3
#define ORD_RESULT 4
    UINT msg = (DWORD)pt->sysarg[1];
    WPARAM wparam = (WPARAM)pt->sysarg[ORD_WPARAM];
    LPARAM lparam = (LPARAM)pt->sysarg[ORD_LPARAM];
    ULONG_PTR result = (ULONG_PTR)pt->sysarg[ORD_RESULT];
    DWORD type = (DWORD)pt->sysarg[5];
    BOOL ansi = (BOOL)pt->sysarg[6];
    bool result_written = true;

    /* First, handle result param: whether read or written.
     * XXX i#1752: the return value of the syscall is actually the LRESULT, so
     * it's not clear whether this param #4 is really used as an OUT param.
     * It's NULL in all instances of the syscall observed so far.
     */
    if (type == FNID_SENDMESSAGECALLBACK || type == FNID_SENDMESSAGEFF ||
        type == FNID_SENDMESSAGEWTOOPTION)
        result_written = false;
    if (!report_memarg_type(ii, ORD_RESULT, result_written ? SYSARG_WRITE : SYSARG_READ,
                            (byte *)result, sizeof(result), "ResultInfo",
                            DRSYS_TYPE_UNSIGNED_INT, "ULONG_PTR"))
        return;

    /* Now handle memory params in the msg code.  We assume all FNID_* take in
     * codes in the same namespace and that we can ignore "type" here.
     * Some will fail on these codes (e.g., FNID_SCROLLBAR won't accept WM_GETTEXT)
     * but we'll live with doing the wrong unaddr check pre-syscall for that.
     */
    switch (msg) {
    case WM_COPYDATA: {
        COPYDATASTRUCT safe;
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ, (byte *)lparam,
                                sizeof(COPYDATASTRUCT), "WM_COPYDATA", DRSYS_TYPE_STRUCT,
                                "COPYDATASTRUCT"))
            return;
        if (safe_read((byte *)lparam, sizeof(safe), &safe) &&
            !report_memarg_type(ii, ORD_LPARAM, SYSARG_READ, (byte *)safe.lpData,
                                safe.cbData, "COPYDATASTRUCT.lpData", DRSYS_TYPE_VOID,
                                NULL))
            return;
        break;
    }
    /* XXX: I'm assuming WM_CREATE and WM_NCCREATE are only passed from the
     * kernel to the app and never the other way so I'm not handling here
     * (CREATESTRUCT is complex to handle).
     */
    case WM_GETMINMAXINFO: {
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ | SYSARG_WRITE,
                                (byte *)lparam, sizeof(MINMAXINFO), "WM_GETMINMAXINFO",
                                DRSYS_TYPE_STRUCT, "MINMAXINFO"))
            return;
        break;
    }
    case WM_GETTEXT: {
        if (ansi) {
            handle_cstring(ii, ORD_LPARAM, SYSARG_WRITE, "WM_GETTEXT buffer",
                           (byte *)lparam, wparam, NULL, true);
        } else {
            handle_cwstring(ii, "WM_GETTEXT buffer", (byte *)lparam,
                            wparam * sizeof(wchar_t), ORD_LPARAM, SYSARG_WRITE, NULL,
                            true);
        }
        if (ii->abort)
            return;
        break;
    }
    case WM_SETTEXT: {
        if (ansi) {
            handle_cstring(ii, ORD_LPARAM, SYSARG_READ, "WM_SETTEXT string",
                           (byte *)lparam, 0, NULL, true);
        } else {
            handle_cwstring(ii, "WM_GETTEXT string", (byte *)lparam, 0, ORD_LPARAM,
                            SYSARG_READ, NULL, true);
        }
        if (ii->abort)
            return;
        break;
    }
    case WM_NCCALCSIZE: {
        BOOL complex = (BOOL)wparam;
        if (complex) {
            NCCALCSIZE_PARAMS safe;
            if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ | SYSARG_WRITE,
                                    (byte *)lparam, sizeof(NCCALCSIZE_PARAMS),
                                    "WM_NCCALCSIZE", DRSYS_TYPE_STRUCT,
                                    "NCCALCSIZE_PARAMS"))
                return;
            if (safe_read((byte *)lparam, sizeof(safe), &safe) &&
                !report_memarg_type(ii, ORD_LPARAM, SYSARG_WRITE, (byte *)safe.lppos,
                                    sizeof(WINDOWPOS), "NCCALCSIZE_PARAMS.lppos",
                                    DRSYS_TYPE_STRUCT, "WINDOWPOS"))
                return;
        } else {
            if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ | SYSARG_WRITE,
                                    (byte *)lparam, sizeof(RECT), "WM_NCCALCSIZE",
                                    DRSYS_TYPE_STRUCT, "RECT"))
                return;
        }
        break;
    }
    case WM_STYLECHANGED: {
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ, (byte *)lparam,
                                sizeof(STYLESTRUCT), "WM_STYLECHANGED", DRSYS_TYPE_STRUCT,
                                "STYLESTRUCT"))
            return;
        break;
    }
    case WM_STYLECHANGING: {
        /* XXX: only some fields are written */
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ | SYSARG_WRITE,
                                (byte *)lparam, sizeof(STYLESTRUCT), "WM_STYLECHANGING",
                                DRSYS_TYPE_STRUCT, "STYLESTRUCT"))
            return;
        break;
    }
    case WM_WINDOWPOSCHANGED: {
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ, (byte *)lparam,
                                sizeof(WINDOWPOS), "WM_WINDOWPOSCHANGED",
                                DRSYS_TYPE_STRUCT, "WINDOWPOS"))
            return;
        break;
    }
    case WM_WINDOWPOSCHANGING: {
        /* XXX: only some fields are written */
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_READ | SYSARG_WRITE,
                                (byte *)lparam, sizeof(WINDOWPOS), "WM_WINDOWPOSCHANGING",
                                DRSYS_TYPE_STRUCT, "WINDOWPOS"))
            return;
        break;
    }

    /* Edit control messages:
     *
     * For now we only have handling for writes by the kernel for EM_GET*.
     * XXX i#1752: add checking of reads, and go through the rest of the EM_*
     * types and find other writes.
     * XXX i#1752: add tests for these.
     */
    case EM_GETSEL: {
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_WRITE, (byte *)lparam,
                                sizeof(DWORD), "EM_GETSEL", DRSYS_TYPE_UNSIGNED_INT,
                                NULL))
            return;
        if (!report_memarg_type(ii, ORD_WPARAM, SYSARG_WRITE, (byte *)wparam,
                                sizeof(DWORD), "EM_GETSEL", DRSYS_TYPE_UNSIGNED_INT,
                                NULL))
            return;
        break;
    }
    case EM_GETRECT: {
        if (!report_memarg_type(ii, ORD_LPARAM, SYSARG_WRITE, (byte *)lparam,
                                sizeof(RECT), "EM_GETRECT", DRSYS_TYPE_STRUCT, "RECT"))
            return;
        break;
    }
    case EM_GETLINE: {
        /* 1st WORD in buf holds # chars */
        WORD chars;
        if (safe_read((WORD *)lparam, sizeof(chars), &chars)) {
            if (ansi) {
                handle_cstring(ii, ORD_LPARAM, SYSARG_WRITE, "EM_GETLINE buffer",
                               (byte *)lparam, chars * sizeof(char), NULL, true);
            } else {
                handle_cwstring(ii, "EM_GETLINE buffer", (byte *)lparam,
                                chars * sizeof(wchar_t), ORD_LPARAM, SYSARG_WRITE, NULL,
                                true);
            }
            if (ii->abort)
                return;
        }
        break;
    }
    case EM_GETCUEBANNER: {
        handle_cwstring(ii, "EM_GETCUEBANNER buffer", (byte *)wparam,
                        lparam * sizeof(wchar_t), ORD_WPARAM, SYSARG_WRITE, NULL, true);
        if (ii->abort)
            return;
        break;
    }

    default: {
        DO_ONCE({ WARN("WARNING: unhandled NtUserMessageCall types found\n"); });
        LOG(drcontext, SYSCALL_VERBOSE,
            "WARNING: unhandled NtUserMessageCall message type 0x%x\n", msg);
        break;
    }
    }
#undef ORD_WPARAM
#undef ORD_LPARAM
#undef ORD_RESULT
}

static void
handle_accel_array(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
                   ACCEL *array, ULONG count, uint arg_flags)
{
    ULONG i;
    /* first field is BYTE followed by WORD so we have padding to skip */
    for (i = 0; i < count; i++) {
        if (!report_memarg_ex(ii, 0, mode_from_flags(arg_flags), (byte *)&array[i].fVirt,
                              sizeof(array[i].fVirt), "ACCEL.fVirt",
                              DRSYS_TYPE_UNSIGNED_INT, NULL, DRSYS_TYPE_STRUCT))
            return;
        if (!report_memarg_ex(ii, 0, mode_from_flags(arg_flags), (byte *)&array[i].key,
                              sizeof(array[i].key), "ACCEL.key", DRSYS_TYPE_SIGNED_INT,
                              NULL, DRSYS_TYPE_STRUCT))
            return;
        if (!report_memarg_ex(ii, 0, mode_from_flags(arg_flags), (byte *)&array[i].cmd,
                              sizeof(array[i].cmd), "ACCEL.cmd", DRSYS_TYPE_SIGNED_INT,
                              NULL, DRSYS_TYPE_STRUCT))
            return;
    }
}

static void
handle_UserCreateAcceleratorTable(void *drcontext, cls_syscall_t *pt,
                                  sysarg_iter_info_t *ii)
{
    ACCEL *array = (ACCEL *)pt->sysarg[0];
    ULONG count = (ULONG)pt->sysarg[1];
    handle_accel_array(drcontext, pt, ii, array, count, SYSARG_READ);
}

static void
handle_UserCopyAcceleratorTable(void *drcontext, cls_syscall_t *pt,
                                sysarg_iter_info_t *ii)
{
    ACCEL *array = (ACCEL *)pt->sysarg[1];
    ULONG count = (ULONG)pt->sysarg[2];
    handle_accel_array(drcontext, pt, ii, array, count, SYSARG_WRITE);
}

static void
handle_UserSetScrollInfo(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Special-cased b/c some fields are ignored (i#1299) */
    SCROLLINFO *si = (SCROLLINFO *)pt->sysarg[2];
    SCROLLINFO safe;
    if (!ii->arg->pre)
        return;
    /* User must set cbSize and fMask */
    if (!report_memarg_type(ii, 0, SYSARG_READ, (byte *)si,
                            offsetof(SCROLLINFO, fMask) + sizeof(si->fMask),
                            "SCROLLINFO cbSize+fMask", DRSYS_TYPE_STRUCT, "SCROLLINFO"))
        return;
    if (safe_read((byte *)si, sizeof(safe), &safe)) {
        if (TEST(SIF_RANGE, safe.fMask) && safe.cbSize >= offsetof(SCROLLINFO, nPage)) {
            if (!report_memarg_type(ii, 0, SYSARG_READ, (byte *)&si->nMin,
                                    sizeof(si->nMin) + sizeof(si->nMax),
                                    "SCROLLINFO nMin+nMax", DRSYS_TYPE_STRUCT,
                                    "SCROLLINFO"))
                return;
        }
        if (TEST(SIF_PAGE, safe.fMask) && safe.cbSize >= offsetof(SCROLLINFO, nPos)) {
            if (!report_memarg_type(ii, 0, SYSARG_READ, (byte *)&si->nPage,
                                    sizeof(si->nPage), "SCROLLINFO.nPage",
                                    DRSYS_TYPE_STRUCT, "SCROLLINFO"))
                return;
        }
        if (TEST(SIF_POS, safe.fMask) && safe.cbSize >= offsetof(SCROLLINFO, nTrackPos)) {
            if (!report_memarg_type(ii, 0, SYSARG_READ, (byte *)&si->nPos,
                                    sizeof(si->nPos), "SCROLLINFO.nPos",
                                    DRSYS_TYPE_STRUCT, "SCROLLINFO"))
                return;
        }
        /* nTrackPos is ignored on setting, even if SIF_TRACKPOS is set */
    }
}

static void
handle_GdiHfontCreate(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ENUMLOGFONTEXDVW dvw;
    ENUMLOGFONTEXDVW *real_dvw = (ENUMLOGFONTEXDVW *)pt->sysarg[0];
    if (ii->arg->pre && safe_read((byte *)pt->sysarg[0], sizeof(dvw), &dvw)) {
        uint i;
        byte *start = (byte *)pt->sysarg[0];
        ULONG total_size = (ULONG)pt->sysarg[1];
        /* Would be: {0,-1,R,}
         * Except not all fields need to be defined.
         * If any other syscall turns out to have this param type should
         * turn this into a type handler and not a syscall handler.
         */
        if (!report_memarg_ex(ii, 0, DRSYS_PARAM_BOUNDS, start, total_size,
                              "ENUMLOGFONTEXDVW", DRSYS_TYPE_STRUCT, NULL,
                              DRSYS_TYPE_INVALID))
            return;

        ASSERT(offsetof(ENUMLOGFONTEXDVW, elfEnumLogfontEx) == 0 &&
                   offsetof(ENUMLOGFONTEXW, elfLogFont) == 0,
               "logfont structs changed");
        handle_logfont(ii, start, sizeof(LOGFONTW), 0, SYSARG_READ,
                       &dvw.elfEnumLogfontEx.elfLogFont);
        if (ii->abort)
            return;

        start = (byte *)&real_dvw->elfEnumLogfontEx.elfFullName;
        for (i = 0; i < sizeof(dvw.elfEnumLogfontEx.elfFullName) / sizeof(wchar_t) &&
             dvw.elfEnumLogfontEx.elfFullName[i] != L'\0';
             i++)
            ; /* nothing */
        if (!report_memarg_type(ii, 0, SYSARG_READ, start, i * sizeof(wchar_t),
                                "ENUMLOGFONTEXW.elfFullName", DRSYS_TYPE_CWARRAY, NULL))
            return;

        start = (byte *)&real_dvw->elfEnumLogfontEx.elfStyle;
        for (i = 0; i < sizeof(dvw.elfEnumLogfontEx.elfStyle) / sizeof(wchar_t) &&
             dvw.elfEnumLogfontEx.elfStyle[i] != L'\0';
             i++)
            ; /* nothing */
        if (!report_memarg_type(ii, 0, SYSARG_READ, start, i * sizeof(wchar_t),
                                "ENUMLOGFONTEXW.elfStyle", DRSYS_TYPE_CWARRAY, NULL))
            return;

        start = (byte *)&real_dvw->elfEnumLogfontEx.elfScript;
        for (i = 0; i < sizeof(dvw.elfEnumLogfontEx.elfScript) / sizeof(wchar_t) &&
             dvw.elfEnumLogfontEx.elfScript[i] != L'\0';
             i++)
            ; /* nothing */
        if (!report_memarg_type(ii, 0, SYSARG_READ, start, i * sizeof(wchar_t),
                                "ENUMLOGFONTEXW.elfScript", DRSYS_TYPE_CWARRAY, NULL))
            return;

        /* the dvValues of DESIGNVECTOR are optional: from 0 to 64 bytes */
        start = (byte *)&real_dvw->elfDesignVector;
        if (dvw.elfDesignVector.dvNumAxes > MM_MAX_NUMAXES) {
            dvw.elfDesignVector.dvNumAxes = MM_MAX_NUMAXES;
            WARN("WARNING: NtGdiHfontCreate design vector larger than max\n");
        }
        if ((start + offsetof(DESIGNVECTOR, dvValues) +
             dvw.elfDesignVector.dvNumAxes * sizeof(LONG)) -
                (byte *)pt->sysarg[0] !=
            total_size) {
            WARN("WARNING: NtGdiHfontCreate total size doesn't match\n");
        }
        if (!report_memarg_type(ii, 0, SYSARG_READ, start,
                                offsetof(DESIGNVECTOR, dvValues) +
                                    dvw.elfDesignVector.dvNumAxes * sizeof(LONG),
                                "DESIGNVECTOR", DRSYS_TYPE_STRUCT, NULL))
            return;
    } else if (ii->arg->pre)
        WARN("WARNING: unable to read NtGdiHfontCreate param\n");
}

static void
handle_GdiDoPalette(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* Entry would read: {3,-2,R|SYSARG_SIZE_IN_ELEMENTS,sizeof(PALETTEENTRY)}
     * But pPalEntries is an OUT param if !bInbound.
     * It's a convenient arg: else would have to look at iFunc.
     */
    WORD cEntries = (WORD)pt->sysarg[2];
    PALETTEENTRY *pPalEntries = (PALETTEENTRY *)pt->sysarg[3];
    bool bInbound = (bool)pt->sysarg[5];
    if (bInbound && ii->arg->pre) {
        if (!report_memarg_type(ii, 3, SYSARG_READ, (byte *)pPalEntries,
                                cEntries * sizeof(PALETTEENTRY), "pPalEntries",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
    } else if (!bInbound) {
        if (!report_memarg_type(ii, 3, SYSARG_WRITE, (byte *)pPalEntries,
                                cEntries * sizeof(PALETTEENTRY), "pPalEntries",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
    }
}

/* Params 0 and 1 and the return type vary */
static void
handle_GdiPolyPolyDraw(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ULONG *counts = (ULONG *)pt->sysarg[2];
    ULONG num_counts = (ULONG)pt->sysarg[3];
    int ifunc = (int)pt->sysarg[4];
    ULONG num_points = 0;
    ULONG i;
    if (ifunc == GdiPolyPolyRgn) {
        /* Param 0 == fill mode enum value:
         *   {0, sizeof(POLYFUNCTYPE), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT}
         */
        report_sysarg_type(ii, 0, SYSARG_READ, sizeof(POLYFUNCTYPE),
                           DRSYS_TYPE_SIGNED_INT, "POLYFUNCTYPE");
    } else {
        /* Param 0 == HDC:
         *   {0, sizeof(HDC), SYSARG_INLINED, DRSYS_TYPE_HANDLE}
         */
        report_sysarg_type(ii, 0, SYSARG_READ, sizeof(HDC), DRSYS_TYPE_HANDLE, "HDC");
    }
    /* The length of the POINT array has to be dynamically computed */
    for (i = 0; i < num_counts; i++) {
        ULONG count;
        if (safe_read(&counts[i], sizeof(count), &count)) {
            num_points += count;
        }
    }
    /* Param 1 == POINT*.
     * XXX: how indicate an array of structs?
     */
    report_sysarg_type(ii, 1, SYSARG_READ, sizeof(PPOINT), DRSYS_TYPE_STRUCT, "POINT");
    if (!report_memarg_type(ii, 1, SYSARG_READ, (byte *)pt->sysarg[1],
                            num_points * sizeof(POINT), "PPOINT", DRSYS_TYPE_STRUCT,
                            "POINT"))
        return;

    switch (ifunc) {
    case GdiPolyBezier:
    case GdiPolyLineTo:
    case GdiPolyBezierTo:
        if (num_counts != 1)
            WARN("WARNING: NtGdiPolyPolyDraw: expected 1 count for single polygons\n");
        break;
    case GdiPolyPolygon:
    case GdiPolyPolyLine:
    case GdiPolyPolyRgn: break;
    default: WARN("WARNING: NtGdiPolyPolyDraw: unknown ifunc %d\n", ifunc);
    }

    if (ifunc == GdiPolyPolyRgn) {
        /* Returns HRGN */
        report_sysarg_return(drcontext, ii, sizeof(HRGN), DRSYS_TYPE_HANDLE, "HRGN");
    } else {
        /* Returns BOOL */
        report_sysarg_return(drcontext, ii, sizeof(BOOL), DRSYS_TYPE_BOOL, NULL);
    }
}

void
wingdi_shadow_process_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* handlers here do not check for success so we check up front */
    if (!ii->arg->pre) {
        if (!os_syscall_succeeded(ii->arg->sysnum, pt->sysinfo, ii->pt))
            return;
    }
    if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserSystemParametersInfo)) {
        handle_UserSystemParametersInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserMenuInfo)) {
        handle_UserMenuInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserMenuItemInfo)) {
        handle_UserMenuItemInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserGetAltTabInfo)) {
        handle_UserGetAltTabInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserGetRawInputBuffer)) {
        handle_UserGetRawInputBuffer(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserGetRawInputData)) {
        handle_UserGetRawInputData(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserGetRawInputDeviceInfo)) {
        handle_UserGetRawInputDeviceInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserTrackMouseEvent)) {
        handle_UserTrackMouseEvent(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserCreateWindowStation) ||
               drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserLoadKeyboardLayoutEx)) {
        /* Vista SP1 added one arg (both were 7, now 8)
         * XXX i#487: figure out what it is and whether we need to process it
         * for each of the two syscalls.
         * Also check whether it's defined after first deciding whether
         * we're on SP1: use core's method of checking for export?
         */
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserMessageCall)) {
        handle_UserMessageCall(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum,
                                   &sysnum_UserCreateAcceleratorTable)) {
        handle_UserCreateAcceleratorTable(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserCopyAcceleratorTable)) {
        handle_UserCopyAcceleratorTable(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_UserSetScrollInfo)) {
        handle_UserSetScrollInfo(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiCreatePaletteInternal)) {
        /* Entry would read: {0, cEntries * 4 + 4, R,} but see comment in ntgdi.h */
        if (ii->arg->pre) {
            UINT cEntries = (UINT)pt->sysarg[1];
            report_memarg_type(ii, 0, SYSARG_READ, (byte *)pt->sysarg[0],
                               sizeof(LOGPALETTE) - sizeof(PALETTEENTRY) +
                                   sizeof(PALETTEENTRY) * cEntries,
                               "pLogPal", DRSYS_TYPE_STRUCT, NULL);
            report_sysarg_type(ii, 0, SYSARG_READ,
                               sizeof(LOGPALETTE) - sizeof(PALETTEENTRY) +
                                   sizeof(PALETTEENTRY) * cEntries,
                               DRSYS_TYPE_STRUCT, "pLogPal");
        }
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiCheckBitmapBits)) {
        /* Entry would read: {7,dwWidth * dwHeight,W,} */
        DWORD dwWidth = (DWORD)pt->sysarg[4];
        DWORD dwHeight = (DWORD)pt->sysarg[5];
        report_memarg_type(ii, 7, SYSARG_WRITE, (byte *)pt->sysarg[7], dwWidth * dwHeight,
                           "paResults", DRSYS_TYPE_STRUCT, NULL);
        report_sysarg_type(ii, 7, SYSARG_READ, dwWidth * dwHeight, DRSYS_TYPE_STRUCT,
                           "paResults");
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiHfontCreate)) {
        handle_GdiHfontCreate(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiDoPalette)) {
        handle_GdiDoPalette(drcontext, pt, ii);
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiExtTextOutW)) {
        UINT fuOptions = (UINT)pt->sysarg[3];
        int cwc = (int)pt->sysarg[6];
        INT *pdx = (INT *)pt->sysarg[7];
        if (ii->arg->pre && TEST(ETO_PDY, fuOptions)) {
            /* pdx contains pairs of INTs.  regular entry already checked
             * size of singletons of INTs so here we check the extra size.
             */
            report_memarg_type(ii, 7, SYSARG_READ, ((byte *)pdx) + cwc * sizeof(INT),
                               cwc * sizeof(INT), "pdx extra size from ETO_PDY",
                               DRSYS_TYPE_STRUCT, NULL);
        }
    } else if (drsys_sysnums_equal(&ii->arg->sysnum, &sysnum_GdiPolyPolyDraw)) {
        handle_GdiPolyPolyDraw(drcontext, pt, ii);
    }
}

bool
wingdi_syscall_succeeded(drsys_sysnum_t sysnum, syscall_info_t *info, ptr_int_t res,
                         bool *success DR_PARAM_OUT)
{
    /* Custom success criteria */
    if (drsys_sysnums_equal(&sysnum, &sysnum_GdiDescribePixelFormat)) {
        *success = (res > 0);
        return true;
    } else if (drsys_sysnums_equal(&sysnum, &sysnum_GdiGetRasterizerCaps)) {
        *success = (res == 1);
        return true;
    }
    /* XXX: should all uint return types have SYSINFO_RET_ZERO_FAIL? */
    return false;
}
