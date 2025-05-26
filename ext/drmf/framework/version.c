/* **********************************************************
 * Copyright (c) 2012-2025 Google, Inc.  All rights reserved.
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

/* Dr. Memory Framework shared code */

#include "dr_api.h"
#include "drmemory_framework.h"
#include "utils.h"

drmf_status_t
drmf_check_version(void *drcontext, client_id_t client_id)
{
    byte *base;
    int *drmf_ver_used;

    /* Support being called from multiple extensions at init time.
     * Execution model guarantees no races at init time.
     */
    static drmf_status_t res = DRMF_ERROR_NOT_IMPLEMENTED;
    if (res != DRMF_ERROR_NOT_IMPLEMENTED)
        return res;

    base = dr_get_client_base(client_id);
    drmf_ver_used =
        (int *)dr_get_proc_address((module_handle_t)base, DRMF_VERSION_USED_NAME);
    LOG(drcontext, 1, "%s: lib ver=%d-%d vs client version %d" NL, __FUNCTION__,
        DRMF_VERSION_COMPAT, DRMF_VERSION_CUR,
        (drmf_ver_used == NULL) ? -1 : *drmf_ver_used);
    if (drmf_ver_used == NULL || *drmf_ver_used < DRMF_VERSION_COMPAT ||
        *drmf_ver_used > DRMF_VERSION_CUR) {
        NOTIFY_ERROR("Version %d-%d mismatch with client version %d-%d" NL,
                     DRMF_VERSION_COMPAT, DRMF_VERSION_CUR,
                     (drmf_ver_used == NULL) ? -1 : *drmf_ver_used);
        res = DRMF_ERROR_INCOMPATIBLE_VERSION;
    } else
        res = DRMF_SUCCESS;
    ASSERT(res != DRMF_ERROR_NOT_IMPLEMENTED, "need to update sentinel");
    return res;
}
