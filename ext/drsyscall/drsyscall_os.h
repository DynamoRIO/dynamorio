/* **********************************************************
 * Copyright (c) 2010-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DRSYSCALL_OS_H_
#define _DRSYSCALL_OS_H_ 1

#include "utils.h"

#ifdef WINDOWS
/* We need extra room for dup entries for diff in vs out size to writes. */
# define MAX_ARGS_IN_ENTRY 18 /* 17 is max known */
#elif defined(MACOS) && !defined(X64)
# define MAX_ARGS_IN_ENTRY 8
#else
# define MAX_ARGS_IN_ENTRY 6 /* 6 is max on Linux */
#endif

#define SYSCALL_NUM_ARG_STORE MAX_ARGS_IN_ENTRY

#define SYSCALL_NUM_ARG_TRACK IF_WINDOWS_ELSE(26, 6)

/* Flag indicates last entry in the additional syscall table.
 * drsys_num_t number field is used for this value.
 */
#define SECONDARY_TABLE_ENTRY_MAX_NUMBER -1
/* There are enumeration classes with gaps (e.g. 0-2-4-8). We use
 * special flag to handle such cases. drsys_num_t number field is
 * used for this value.
 */
#define SECONDARY_TABLE_SKIP_ENTRY -2
/* We place base entry in the secondary table under -1 index to fall
 * back to the base entry if the app calls syscall with unknown
 * structure number.
 */
#define BASE_ENTRY_INDEX -1

/* for diagnostics: eventually provide some runtime option,
 * -logmask or something: for now have to modify this constant
 */
#ifdef TOOL_DR_MEMORY
# define SYSCALL_VERBOSE 2
#else
# define SYSCALL_VERBOSE 3
#endif

/* syscall numbers are most natural in decimal on linux but hex on windows */
#ifdef UNIX
# define SYSNUM_FMT "%d"
#else
# define SYSNUM_FMT PIFX
#endif

/* With the widening to 64-bit, 32-bit code needs a lot of ugly casts so we try
 * to simplify via this macro:
 */
#define SYSARG_AS_PTR(pt, ord, type) (type)(ptr_uint_t)(pt)->sysarg[(ord)]

#define ARGFMT "0x" HEX64_FORMAT_STRING

/* extra_info slot usage */
enum {
    /* The size computed by SYSARG_SIZE_IN_FIELD is saved here across the
     * syscall.  We only support one such parameter per syscall.
     */
    EXTRA_INFO_SIZE_FROM_FIELD,
    EXTRA_INFO_SOCKADDR,
#ifdef UNIX
    EXTRA_INFO_MSG_CONTROL,
    EXTRA_INFO_MSG_CONTROLLEN,
#endif
    EXTRA_INFO_MAX,
};

extern int cls_idx_drsys;

extern drsys_options_t drsys_ops;

extern const char * const param_type_names[];

enum {
    /*****************************************/
    /* sysinfo_arg_t.flags */
    SYSARG_READ                = 0x00000001,
    SYSARG_WRITE               = 0x00000002,
    /* The data structure type has pointers or uninitialized fields
     * or padding and needs special processing according to the
     * SYSARG_TYPE_* code stored in sysinfo_arg_t.misc.
     */
    SYSARG_COMPLEX_TYPE        = 0x00000004,
    /* the size points at the IO_STATUS_BLOCK param */
    SYSARG_POST_SIZE_IO_STATUS = 0x00000008,
    /* (available)             = 0x00000010, */
    /* The param holding the size is a pointer b/c it's an IN OUT var.
     * This can be used in one of two ways:
     * 1) A single entry exists for the buffer/struct.  In this case, the param
     *    holding the size must be both read and write (R|W).
     * 2) A duplicate entry exists for the buffer/struct.  Then the second can
     *    be SYSARG_LENGTH_INOUT (often "WI" in the tables) and the size param
     *    can just be written and not read (W).
     */
    SYSARG_LENGTH_INOUT        = 0x00000020,
    /* The size is not in bytes but in elements where the size of
     * each element is in the misc field.  The misc field can
     * contain <= in which case the element size is stored in that
     * parameter number.
     * This flag trumps SYSARG_COMPLEX_TYPE, so if there is an
     * overlap then special handling must be done for the type.
     */
    SYSARG_SIZE_IN_ELEMENTS    = 0x00000040,
    /* A non-memory argument (i.e., entire value is in parameter slot). */
    SYSARG_INLINED             = 0x00000080,
    /* for SYSARG_POST_SIZE_RETVAL on a duplicate entry, nothing is
     * written if the count, given in the first entry, is zero,
     * regardless of the buffer pointer value.
     */
    SYSARG_NO_WRITE_IF_COUNT_0 = 0x00000100,
    /* Contains a type specifier */
    SYSARG_HAS_TYPE            = 0x00000200,
    /* i#502-c#5 the arg should be ignored if the next arg is null */
    SYSARG_IGNORE_IF_NEXT_NULL = 0x00000400,
    /* Ignore this arg if the previous arg is NULL.
     * XXX: Unify this with SYSARG_IGNORE_IF_NEXT_NULL by storing the arg# in
     * misc once that doesn't conflict with type info.
     */
    SYSARG_IGNORE_IF_PREV_NULL = 0x00000800,
    /* Indicates that this entry is for non-memarg iteration only:
     * this memory parameter is complex and for memarg iteration
     * it relies on handler code.
     */
    SYSARG_NON_MEMARG          = 0x00001000,
    /* Indicates that the size value from an output parameter is one smaller
     * than the actual written size (typically, a string's terminating null).
     */
    SYSARG_SIZE_PLUS_1         = 0x00002000,

    /*****************************************/
    /* sysinfo_arg_t.size, using values that cannot be mistaken for
     * a parameter reference.
     */
    /* <available>            = -100, */
    /* used in repeated sysinfo_arg_t entry for post-syscall size */
    SYSARG_POST_SIZE_RETVAL   = -101,
    /* Size is stored as a field of size 4 bytes with an offset given by
     * sysinfo_arg_t.misc.  Can only be used by one arg per syscall.
     */
    SYSARG_SIZE_IN_FIELD      = -102,

    /*****************************************/
    /* sysinfo_arg_t.misc when flags has SYSARG_COMPLEX_TYPE */
    /* These occupy the same number-space as DRSYS_TYPE_*.
     * We have duplicate labels here for legacy code.
     * We have a separate namespace so we can use our own types
     * internally w/o exposing in the public header.
     */
    /* The following flags are used on Windows. */
    SYSARG_TYPE_CSTRING                 = DRSYS_TYPE_CSTRING, /* Linux too */
    SYSARG_TYPE_CSTRING_WIDE            = DRSYS_TYPE_CWSTRING,
    SYSARG_TYPE_PORT_MESSAGE            = DRSYS_TYPE_PORT_MESSAGE,
    SYSARG_TYPE_CONTEXT                 = DRSYS_TYPE_CONTEXT,
    SYSARG_TYPE_EXCEPTION_RECORD        = DRSYS_TYPE_EXCEPTION_RECORD,
    SYSARG_TYPE_SECURITY_QOS            = DRSYS_TYPE_SECURITY_QOS,
    SYSARG_TYPE_SECURITY_DESCRIPTOR     = DRSYS_TYPE_SECURITY_DESCRIPTOR,
    SYSARG_TYPE_UNICODE_STRING          = DRSYS_TYPE_UNICODE_STRING,
    SYSARG_TYPE_OBJECT_ATTRIBUTES       = DRSYS_TYPE_OBJECT_ATTRIBUTES,
    SYSARG_TYPE_LARGE_STRING            = DRSYS_TYPE_LARGE_STRING,
    SYSARG_TYPE_DEVMODEW                = DRSYS_TYPE_DEVMODEW,
    SYSARG_TYPE_WNDCLASSEXW             = DRSYS_TYPE_WNDCLASSEXW,
    SYSARG_TYPE_CLSMENUNAME             = DRSYS_TYPE_CLSMENUNAME,
    SYSARG_TYPE_MENUITEMINFOW           = DRSYS_TYPE_MENUITEMINFOW,
    SYSARG_TYPE_ALPC_PORT_ATTRIBUTES    = DRSYS_TYPE_ALPC_PORT_ATTRIBUTES,
    SYSARG_TYPE_ALPC_SECURITY_ATTRIBUTES= DRSYS_TYPE_ALPC_SECURITY_ATTRIBUTES,
    SYSARG_TYPE_BITMAPINFO              = DRSYS_TYPE_BITMAPINFO,
    SYSARG_TYPE_ALPC_CONTEXT_ATTRIBUTES = DRSYS_TYPE_ALPC_CONTEXT_ATTRIBUTES,
    SYSARG_TYPE_ALPC_MESSAGE_ATTRIBUTES = DRSYS_TYPE_ALPC_MESSAGE_ATTRIBUTES,
    SYSARG_TYPE_T2_SET_PARAMETERS       = DRSYS_TYPE_T2_SET_PARAMETERS,
    /* These are Linux-specific */
    SYSARG_TYPE_SOCKADDR                = DRSYS_TYPE_SOCKADDR,
    SYSARG_TYPE_MSGHDR                  = DRSYS_TYPE_MSGHDR,
    SYSARG_TYPE_MSGBUF                  = DRSYS_TYPE_MSGBUF,
    /* Types that we map to other types.  These need unique number separate
     * from the DRSYS_TYPE_* numbers so we sequentially number from here:
     */
    SYSARG_TYPE_UNICODE_STRING_NOLEN    = DRSYS_TYPE_LAST + 1,
    /* These are used to encode type+size into return_type field */
    SYSARG_TYPE_SINT32,
    SYSARG_TYPE_UINT32,
    SYSARG_TYPE_SINT16,
    SYSARG_TYPE_UINT16,
    SYSARG_TYPE_BOOL32,
    SYSARG_TYPE_BOOL8,
    /* Be sure to update map_to_exported_type() when adding here */
};

/* We encode the actual size of a write, if it can differ from the
 * requested size, as a subsequent sysinfo_arg_t entry with the same
 * param#.  A negative size there refers to a parameter that should be
 * de-referenced to obtain the actual write size.  That parameter to be
 * de-referenced must have its own entry which indicates its size.
 */
typedef struct _sysinfo_arg_t {
    int param; /* ordinal of parameter */
    int size; /* >0 = abs size; <=0 = -param that holds size */
    uint flags; /* SYSARG_ flags */
    /* Meaning depends on flags.  I'd use a union but that would make
     * the syscall tables ugly w/ a ton of braces.
     * Currently used for:
     * - SYSARG_COMPLEX_TYPE: holds SYSARG_TYPE_* enum value
     * - SYSARG_SIZE_IN_ELEMENTS: holds size of array entry
     * - SYSARG_SIZE_IN_FIELD: holds offset of 4-byte size field
     * - SYSARG_INLINED: holds SYSARG_TYPE_* enum value
     * - SYSARG_HAS_TYPE: holds SYSARG_TYPE_* enum value
     */
    int misc;
    const char *type_name; /* Symbolic name of the arg type. */
} sysinfo_arg_t;

#define SYSARG_MISC_HAS_TYPE(flags) \
    (TESTANY(SYSARG_COMPLEX_TYPE|SYSARG_INLINED|SYSARG_HAS_TYPE, (flags)))

enum {
    /* If not set, automated param comparison is used to find writes */
    SYSINFO_ALL_PARAMS_KNOWN    = 0x00000001,
    /* When checking the sysnum vs a wrapper function, do not consider
     * removing the prefix
     */
    SYSINFO_REQUIRES_PREFIX     = 0x00000002,
    /* NtUser syscall wrappers are spread across user32.dll and imm32.dll */
    SYSINFO_IMM32_DLL           = 0x00000004,
    /* Return value indicates failure only when zero */
    SYSINFO_RET_ZERO_FAIL       = 0x00000008,
    /* Return value of STATUS_BUFFER_TOO_SMALL (i#486),
     * STATUS_BUFFER_OVERFLOW (i#531), or STATUS_INFO_LENGTH_MISMATCH
     * (i#932) writes final arg but no others.
     * If it turns out some syscalls distinguish between the ret values
     * we can split the flag up but seems safer to combine.
     */
    SYSINFO_RET_SMALL_WRITE_LAST= 0x00000010,
    /* System call takes a code from one of its params that is in essence
     * a new system call number in a new sub-space.
     * The num_out field contains a pointer to a new syscall_info_t
     * array to use with the first param's code.
     * The first argument field indicates which param contains the code.
     * Any other argument fields in the initial entry are ignored.
     */
    SYSINFO_SECONDARY_TABLE     = 0x00000020,
    /* Return value indicates failure only when -1 */
    SYSINFO_RET_MINUS1_FAIL     = 0x00000040,
    /* Return type varies dynamically and handler must call report_sysarg_return() */
    SYSINFO_RET_TYPE_VARIES     = 0x00000080,
    /* Return value is 64-bit even in 32-bit mode */
    SYSINFO_RET_64BIT           = 0x00000100,
};

#define SYSCALL_ARG_TRACK_MAX_SZ 2048

typedef struct _syscall_info_t {
    /* System call number: filled in dynamically, allowing us to use the static
     * fields to indicate underlying version reliance.  We read the static
     * fields before we decide to use an entry and clobber the static value with
     * the actual dynamic number.  Statically, the number field, if non-zero,
     * indicates the minimum Windows version at which this entry is valid; the
     * secondary field, if non-zero, indicates the maximum Windows version at
     * which this entry is valid.  I'd use a union but then we'd need even more
     * {} in the tables.
     */
    drsys_sysnum_t num;
    const char *name;
    uint flags; /* SYSINFO_ flags */
    uint return_type; /* not drsys_param_type_t so we can use extended SYSARG_TYPE_* */
    int arg_count;
    /* list of args that are not inlined: though we now store inlined too for drstrace */
    sysinfo_arg_t arg[MAX_ARGS_IN_ENTRY];
    /* For custom handling w/o separate number lookup.
     * If SYSINFO_SECONDARY_TABLE is set in flags, this is instead
     * a pointer to a new syscall_info_t table.
     * (I'd use a union but that makes syscall table initializers uglier)
     */
    drsys_sysnum_t *num_out;
} syscall_info_t;

typedef struct _cls_syscall_t {
    /* the interface keeps state for API simplicity and for performance */
    drsys_sysnum_t sysnum;
    syscall_info_t *sysinfo;
    dr_mcontext_t mc;
    bool pre;

    /* For recording args so post-syscall can examine.
     * To maintain ordinals for 32-bit Mac, where syscalls can take 64-bit
     * args, we have to widen what we store for 64-bit.
     */
    uint64 sysarg[SYSCALL_NUM_ARG_STORE];
#ifdef WINDOWS
    reg_t param_base;
#endif

    /* for recording additional info for particular arg types */
    ptr_int_t extra_info[EXTRA_INFO_MAX];
#ifdef DEBUG
    /* We should be able to statically share extra_info[].  This helps find errors. */
    bool extra_inuse[SYSCALL_NUM_ARG_STORE];
#endif
    /* We need to store the size in pre for use in post (for i#1119)
     * and we can't share sysarg_sz as some syscalls are both known
     * and unknown.
     */
    size_t sysarg_known_sz[SYSCALL_NUM_ARG_STORE];
    bool first_iter;
    bool first_iter_generic_loop; /* just for sysarg_get_size */
    bool memargs_iterated; /* to enforce that post requires pre */

    /* for comparing memory across unknown system calls */
    bool known;
    app_pc sysarg_ptr[SYSCALL_NUM_ARG_TRACK];
    size_t sysarg_sz[SYSCALL_NUM_ARG_TRACK];
    /* dynamically allocated */
    size_t sysarg_val_bytes[SYSCALL_NUM_ARG_TRACK];
    byte *sysarg_val[SYSCALL_NUM_ARG_TRACK];

    /* for a writable info struct so we can set the sysnum */
    syscall_info_t unknown_info;
} cls_syscall_t;

/* used for simpler arg passing among syscall arg handlers */
typedef struct _sysarg_iter_info_t {
    drsys_arg_t *arg;
    drsys_iter_cb_t cb_mem;
    drsys_iter_cb_t cb_arg;
    void *user_data;
    cls_syscall_t *pt;
    bool abort;
} sysarg_iter_info_t;

/* Type of callback for routine which returns secondary number. */
typedef uint (*drsys_get_secnum_cb_t)(const char *name, uint primary_number);

/* Hashtable maintained in os-specific code that maps drsys_sysnum_t to
 * syscall_info_t*.
 * To gain efficiency and merge static and dynamic queries, our API
 * hands out an opaque copy of the syscall_info_t pointers that are
 * stored in this table to the client.
 * We assume the source tables pointed into are set at process init
 * and never changed afterward.
 */
extern hashtable_t systable;

/* i#1549: We use separate hashtable to handle syscalls with secondary
 * components.
 */
extern hashtable_t secondary_systable;

/* lock for systable & secondary_systable, maintained in drsyscall.c */
extern void *systable_lock;

drmf_status_t
drsyscall_os_init(void *drcontext);

void
drsyscall_os_exit(void);

syscall_info_t *
syscall_lookup(drsys_sysnum_t num, bool resolve_secondary);

void
drsyscall_os_thread_init(void *drcontext);

void
drsyscall_os_thread_exit(void *drcontext);

void
drsyscall_os_module_load(void *drcontext, const module_data_t *info, bool loaded);

bool
is_using_sysenter(void);

bool
is_using_sysint(void);

#ifdef WINDOWS
bool
is_using_wow64(void);
#endif

/* Either sets arg->reg to DR_REG_NULL and sets arg->start_addr, or sets arg->reg
 * to non-DR_REG_NULL
 */
void
drsyscall_os_get_sysparam_location(cls_syscall_t *pt, uint argnum, drsys_arg_t *arg);

/* check syscall param at pre-syscall only */
void
check_sysparam(uint sysnum, uint argnum, dr_mcontext_t *mc, size_t argsz);

/* for memory shadowing checks */
void
os_handle_pre_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii);

void
os_handle_post_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii);

/* returns true if the given argument was processed in a non-standard way
 * (e.g. OS-specific structures) and we should skip the standard check
 */
bool
os_handle_pre_syscall_arg_access(sysarg_iter_info_t *ii,
                                 const sysinfo_arg_t *arg_info,
                                 app_pc start, uint size);

/* returns true if the given argument was processed in a non-standard way
 * (e.g. OS-specific structures) and we should skip the standard check
 */
bool
os_handle_post_syscall_arg_access(sysarg_iter_info_t *ii,
                                  const sysinfo_arg_t *arg_info,
                                  app_pc start, uint size);

bool
os_syscall_succeeded(drsys_sysnum_t sysnum, syscall_info_t *info, cls_syscall_t *pt);

/* Returns true if successful, yet we should skip automated table output
 * params as we need custom output handling.
 */
bool
os_syscall_succeeded_custom(drsys_sysnum_t sysnum, syscall_info_t *info,
                            cls_syscall_t *pt);

#ifdef WINDOWS
bool
os_syscall_ret_small_write_last(syscall_info_t *info, ptr_int_t res);
#endif

bool
os_syscall_get_num(const char *name, drsys_sysnum_t *num DR_PARAM_OUT);

uint
sysnum_hash(void *val);

bool
sysnum_cmp(void *v1, void *v2);

bool
sysarg_invalid(sysinfo_arg_t *arg);

void
store_extra_info(cls_syscall_t *pt, int index, ptr_int_t value);

ptr_int_t
read_extra_info(cls_syscall_t *pt, int index);

bool
report_memarg_ex(sysarg_iter_info_t *iter_info,
                 int ordinal, drsys_param_mode_t mode,
                 app_pc ptr, size_t sz, const char *id,
                 drsys_param_type_t type, const char *type_name,
                 drsys_param_type_t containing_type);

bool
report_memarg_type(sysarg_iter_info_t *iter_info,
                   int ordinal, uint arg_flags,
                   app_pc ptr, size_t sz, const char *id,
                   drsys_param_type_t type, const char *type_name);

bool
report_memarg_field(sysarg_iter_info_t *ii,
                    const sysinfo_arg_t *arg_info,
                    app_pc ptr, size_t sz, const char *id,
                    drsys_param_type_t type, const char *type_name);

bool
report_memarg(sysarg_iter_info_t *iter_info,
              const sysinfo_arg_t *arg_info,
              app_pc ptr, size_t sz, const char *id);

bool
report_sysarg_return(void *drcontext, sysarg_iter_info_t *ii,
                     size_t sz, drsys_param_type_t type, const char *type_name);

bool
report_sysarg_type(sysarg_iter_info_t *ii, int ordinal, uint arg_flags,
                   size_t sz, drsys_param_type_t type, const char *type_name);

bool
report_sysarg(sysarg_iter_info_t *iter_info, int ordinal, uint arg_flags);

drsys_param_mode_t
mode_from_flags(uint arg_flags);

bool
handle_cstring(sysarg_iter_info_t *ii, int ordinal, uint arg_flags, const char *id,
               byte *start, size_t size/*in bytes*/, char *safe, bool check_addr);

bool
handle_sockaddr(cls_syscall_t *pt, sysarg_iter_info_t *ii, byte *ptr,
                size_t len, int ordinal, uint arg_flags, const char *id);

#if DEBUG
void
report_callstack(void *drcontext, dr_mcontext_t *mc);
#endif /* DEBUG */

#endif /* _DRSYSCALL_OS_H_ */
