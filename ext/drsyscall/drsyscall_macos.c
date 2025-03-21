/* **********************************************************
 * Copyright (c) 2014-2024 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"

#include <sys/syscall.h>
#include <string.h>
#include <fcntl.h>

/* FIXME i#1440: finish porting Dr. Syscall to Mac OSX */

/***************************************************************************
 * SYSTEM CALLS FOR MAC
 */

/* 64-bit and 32-bit have the same numbers, which is convenient */

/* Table that maps system call number to a syscall_info_t* */
#define SYSTABLE_HASH_BITS 9 /* ~2x the # of entries */
hashtable_t systable;

/* Table that maps secondary system call number to a syscall_info_t*
 * We leave this table empty to be in sync with Windows & Linux solutions.
 * xref i#1438 i#1549.
 */
#define SECONDARY_SYSTABLE_HASH_BITS 1 /* now we haven't any entries */
hashtable_t secondary_systable;

/* The tables are in separate files as they are quite large */

/* BSD syscalls */
extern syscall_info_t syscall_info_bsd[];
extern size_t count_syscall_info_bsd;

/* FIXME i#1440: add mach syscall table */
#define SYSCALL_NUM_MARKER_MACH      0x1000000
#define SYSCALL_NUM_MARKER_BSD       0x2000000 /* x64 only */
#define SYSCALL_NUM_MARKER_MACHDEP   0x3000000

/* FIXME i#1440: add machdep syscall table */

/***************************************************************************
 * PER-SYSCALL HANDLING
 */

void
os_handle_pre_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    switch (ii->arg->sysnum.number) {
    case SYS_open:
    case SYS_open_nocancel: {
        /* 3rd arg is only required for O_CREAT */
        int flags = (int) pt->sysarg[1];
        if (TEST(O_CREAT, flags)) {
            if (!report_sysarg_type(ii, 2, SYSARG_READ, sizeof(int),
                                    DRSYS_TYPE_SIGNED_INT, NULL))
                return;
        }
        break;
    }
    }
    /* If you add any handling here: need to check ii->abort first */
}

void
os_handle_post_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* each handler checks result for success */
    switch (ii->arg->sysnum.number) {
        /* FIXME i#1440: add handling */
    }
    /* If you add any handling here: need to check ii->abort first */
}

/***************************************************************************
 * SHADOW PER-ARG-TYPE HANDLING
 */

/* XXX i#1440: share w/ Linux */
static bool
handle_cstring_access(sysarg_iter_info_t *ii,
                      const sysinfo_arg_t *arg_info,
                      app_pc start, uint size/*in bytes*/)
{
    return handle_cstring(ii, arg_info->param, arg_info->flags,
                          NULL, start, size, NULL,
                          /* let normal check ensure full size is addressable */
                          false);
}

/* XXX i#1440: share w/ Linux */
static void
check_strarray(sysarg_iter_info_t *ii, char **array, int ordinal, const char *id)
{
    char *str;
    int i = 0;
#   define STR_ARRAY_MAX_ITER 64*1024 /* safety check */
    while (safe_read(&array[i], sizeof(str), &str) && str != NULL
           && i < STR_ARRAY_MAX_ITER) {
        handle_cstring(ii, ordinal, SYSARG_READ, id, (app_pc)str, 0, NULL, false);
        i++;
    }
}

/* XXX i#1440: share w/ Linux */
static bool
handle_strarray_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                       app_pc start, uint size)
{
    char id[16];
    dr_snprintf(id, BUFFER_SIZE_ELEMENTS(id), "%s%d", "parameter #", arg_info->param);
    NULL_TERMINATE_BUFFER(id);
    check_strarray(ii, (char **)start, arg_info->param, id);
    return true; /* check_strarray checks whole array */
}

static bool
os_handle_syscall_arg_access(sysarg_iter_info_t *ii,
                             const sysinfo_arg_t *arg_info,
                             app_pc start, uint size)
{
    if (!TEST(SYSARG_COMPLEX_TYPE, arg_info->flags))
        return false;

    switch (arg_info->misc) {
    case SYSARG_TYPE_CSTRING:
        return handle_cstring_access(ii, arg_info, start, size);
    case DRSYS_TYPE_CSTRARRAY:
        return handle_strarray_access(ii, arg_info, start, size);
    /* FIXME i#1440: add more handling -- probably also want
     * SYSARG_TYPE_SOCKADDR?  Share w/ Linux?
     */
    }
    return false;
}

bool
os_handle_pre_syscall_arg_access(sysarg_iter_info_t *ii,
                                 const sysinfo_arg_t *arg_info,
                                 app_pc start, uint size)
{
    return os_handle_syscall_arg_access(ii, arg_info, start, size);
}

bool
os_handle_post_syscall_arg_access(sysarg_iter_info_t *ii,
                                  const sysinfo_arg_t *arg_info,
                                  app_pc start, uint size)
{
    return os_handle_syscall_arg_access(ii, arg_info, start, size);
}

/***************************************************************************
 * TOP_LEVEL
 */

/* Table that maps syscall names to numbers.  Payload points at num in syscall_info_*
 * tables.
 */
#define NAME2NUM_TABLE_HASH_BITS 10 /* <500 of them */
static hashtable_t name2num_table;

drmf_status_t
drsyscall_os_init(void *drcontext)
{
    uint i;
    hashtable_init_ex(&systable, SYSTABLE_HASH_BITS, HASH_INTPTR, false/*!strdup*/,
                      false/*!synch*/, NULL, sysnum_hash, sysnum_cmp);
    /* We initialize & leave secondary_systable empty to be in sync with our
     * Windows & Linux solutions. Xref i#1438 i#1549.
     */
    hashtable_init_ex(&secondary_systable, SECONDARY_SYSTABLE_HASH_BITS,
                      HASH_INTPTR, false/*!strdup*/, false/*!synch*/, NULL,
                      sysnum_hash, sysnum_cmp);

    hashtable_init(&name2num_table, NAME2NUM_TABLE_HASH_BITS, HASH_STRING,
                   false/*!strdup*/);

    dr_recurlock_lock(systable_lock);
    for (i = 0; i < count_syscall_info_bsd; i++) {
#if defined(MACOS) && defined(X64)
        /* We want to use SYS_ enums in the table so we could add the BSD marker
         * here via:
         *   syscall_info_bsd[i].num.number |= SYSCALL_NUM_MARKER_BSD;
         * however, DR is removing the marker from the numbers we see, so we leave it
         * off.  XXX: Will a user look at raw syscalls and include it?
         */
#endif
        IF_DEBUG(bool ok =)
            hashtable_add(&systable, (void *) &syscall_info_bsd[i].num,
                          (void *) &syscall_info_bsd[i]);
        ASSERT(ok, "no dups");

        IF_DEBUG(ok =)
            hashtable_add(&name2num_table, (void *) syscall_info_bsd[i].name,
                          (void *) &syscall_info_bsd[i].num);
        ASSERT(ok || strcmp(syscall_info_bsd[i].name, "ni_syscall") == 0, "no dups");
    }
    dr_recurlock_unlock(systable_lock);
    return DRMF_SUCCESS;
}

void
drsyscall_os_exit(void)
{
    hashtable_delete(&systable);
    hashtable_delete(&secondary_systable);
    hashtable_delete(&name2num_table);
}

void
drsyscall_os_thread_init(void *drcontext)
{
}

void
drsyscall_os_thread_exit(void *drcontext)
{
}

void
drsyscall_os_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
}

bool
os_syscall_get_num(const char *name, drsys_sysnum_t *num_out DR_PARAM_OUT)
{
    drsys_sysnum_t *num = (drsys_sysnum_t *)
        hashtable_lookup(&name2num_table, (void *)name);
    if (num != NULL) {
        *num_out = *num;
        return true;
    }
    return false;
}

/* Either sets arg->reg to DR_REG_NULL and sets arg->start_addr, or sets arg->reg
 * to non-DR_REG_NULL
 */
void
drsyscall_os_get_sysparam_location(cls_syscall_t *pt, uint argnum, drsys_arg_t *arg)
{
#ifdef X64
    switch (argnum) {
    case 0: arg->reg = DR_REG_RDI;
    case 1: arg->reg = DR_REG_RSI;
    case 2: arg->reg = DR_REG_RDX;
    case 3: arg->reg = DR_REG_R10; /* rcx = retaddr for OP_syscall */
    case 4: arg->reg = DR_REG_R8;
    case 5: arg->reg = DR_REG_R9;
    default: arg->reg = DR_REG_NULL; /* error */
    }
    arg->start_addr = NULL;
#else
    /* Args are on stack, past retaddr from syscall wrapper */
    arg->reg = DR_REG_NULL;
    arg->start_addr = (app_pc) (((reg_t *)arg->mc->esp) + 1/*retaddr*/ + argnum);
#endif
}

drmf_status_t
drsys_syscall_type(drsys_syscall_t *syscall, drsys_syscall_type_t *type DR_PARAM_OUT)
{
    if (syscall == NULL || type == NULL)
        return DRMF_ERROR_INVALID_PARAMETER;
    *type = DRSYS_SYSCALL_TYPE_KERNEL;
    return DRMF_SUCCESS;
}

bool
os_syscall_succeeded(drsys_sysnum_t sysnum, syscall_info_t *info, cls_syscall_t *pt)
{
    if (TEST(SYSCALL_NUM_MARKER_MACH, sysnum.number)) {
        /* FIXME i#1440: Mach syscalls vary (for some KERN_SUCCESS=0 is success,
         * for others that return mach_port_t 0 is failure (I think?).
         */
        return ((ptr_int_t)pt->mc.xax >= 0);
    } else
        return !TEST(EFLAGS_CF, pt->mc.xflags);
}

bool
os_syscall_succeeded_custom(drsys_sysnum_t sysnum, syscall_info_t *info,
                            cls_syscall_t *pt)
{
    return false;
}
