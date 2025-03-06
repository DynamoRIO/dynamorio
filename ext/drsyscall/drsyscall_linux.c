/* **********************************************************
 * Copyright (c) 2010-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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
#include "sysnum_linux.h"
#include "heap.h"
#include "asm_utils.h"
#include <string.h> /* for strcmp */
#include <stddef.h> /* for offsetof */
#include "linux_defines.h"
#ifdef X86
# include <asm/prctl.h>
#endif
#include <sys/prctl.h>

/* used to read entire ioctl arg at once */
union ioctl_data {
    struct ipmi_req req;
    struct ipmi_req_settime reqs;
    struct ipmi_recv recv;
};

/***************************************************************************
 * SYSTEM CALLS FOR LINUX
 */

/* 64-bit vs 32-bit and mixed-mode strategy:
 *
 * We could avoid a hashtable lookup and always use an array deref in
 * syscall_lookup() while still sharing data for syscalls that are
 * identical between the two modes if we generated a static table from
 * macros.  But macros are a little ugly with commas which our nested
 * structs are full of.  So we go ahead and pay the cost of a
 * hashtable lookup.  We could list in x86 order and avoid the
 * hashtable lookup there except we want to eventually support
 * mixed-mode and thus we want both x64 and x86 entries in the same
 * list.  We assume syscall numbers easily fit in 16 bits and pack the
 * numbers for the two platforms together via PACKNUM.
 *
 * For mixed-mode, the plan is to have the static table be x64 and to copy
 * it into the heap for x86.  While walking it we'll construct a table
 * mapping x64 numbers to their equivalent x86 numbers, allowing us to
 * use something like is_sysnum(num, SYS_mmap) (where SYS_mmap is the x64
 * number from sysnum_linux.h) in syscall dispatch (we'll have to replace
 * the switch statements with if-else).
 * XXX i#1013: for all the sizeof(struct) entries we'll have to make two entries
 * and define our own 32-bit version of the struct.
 */
/* PACKNUM is defined in table_defines.h */
/* the cast is for sign extension for -1 sentinel */
#ifdef X86
# define UNPACK_X64(packed) ((int)(short)((packed) >> 16))
# define UNPACK_X86(packed) ((int)(short)((packed) & 0xffff))
# ifdef X64
#  define UNPACK_NATIVE UNPACK_X64
# else
#  define UNPACK_NATIVE UNPACK_X86
# endif
#elif defined(ARM)
/* single number as we only support 32-bit for now */
# ifdef X64
#  error NYI i#1569
# else
#  define UNPACK_NATIVE(packed) packed
# endif
#elif defined(AARCH64)
#  define UNPACK_NATIVE(packed) packed
#endif

/* Table that maps system call number to a syscall_info_t* */
#define SYSTABLE_HASH_BITS 9 /* ~2x the # of entries */
hashtable_t systable;

/* Additional table that maps ioctl system call number to a syscall_info_t* */
#define SECONDARY_SYSTABLE_HASH_BITS 9 /* ~ 440 # of entries */
hashtable_t secondary_systable;

/* The tables are in separate files as they are quite large */
extern syscall_info_t syscall_info[];
extern size_t count_syscall_info;

extern syscall_info_t syscall_ioctl_info[];
extern size_t count_syscall_ioctl_info;

/***************************************************************************
 * TOP-LEVEL
 */

/* Table that maps syscall names to numbers.  Payload points at num in syscall_info[]. */
#define NAME2NUM_TABLE_HASH_BITS 10 /* <500 of them */
static hashtable_t name2num_table;

drmf_status_t
drsyscall_os_init(void *drcontext)
{
    uint i;
    hashtable_init_ex(&systable, SYSTABLE_HASH_BITS, HASH_INTPTR, false/*!strdup*/,
                      false/*!synch*/, NULL, sysnum_hash, sysnum_cmp);

    hashtable_init_ex(&secondary_systable, SECONDARY_SYSTABLE_HASH_BITS,
                      HASH_INTPTR, false/*!strdup*/, false/*!synch*/, NULL,
                      sysnum_hash, sysnum_cmp);

    hashtable_init(&name2num_table, NAME2NUM_TABLE_HASH_BITS, HASH_STRING,
                   false/*!strdup*/);

    dr_recurlock_lock(systable_lock);
    for (i = 0; i < count_syscall_info; i++) {
        syscall_info[i].num.number = UNPACK_NATIVE(syscall_info[i].num.number);
        if (syscall_info[i].num.number != -1) {
            IF_DEBUG(bool ok =)
                hashtable_add(&systable, (void *) &syscall_info[i].num,
                              (void *) &syscall_info[i]);
            ASSERT(ok, "no dups");

            IF_DEBUG(ok =)
                hashtable_add(&name2num_table, (void *) syscall_info[i].name,
                              (void *) &syscall_info[i].num);
            ASSERT(ok || strcmp(syscall_info[i].name, "ni_syscall") == 0, "no dups");
        }
    }
    /* i#1549: We place ioctl secondary syscalls into separate hashtable
     * to be in sync with our solution for Windows.
     */
    for (i = 0; i < count_syscall_ioctl_info; i++) {
        syscall_info_t *info = &syscall_ioctl_info[i];
        info->num.number = UNPACK_NATIVE(info->num.number);
        IF_DEBUG(bool ok =)
            hashtable_add(&secondary_systable, (void *) &info->num, (void *) info);
        ASSERT(ok, "no dups");
        IF_DEBUG(ok =)
            hashtable_add(&name2num_table, (void *) info->name,
                          (void *) &info->num);
        ASSERT(ok, "no dups");
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

static inline reg_id_t
sysparam_reg(uint argnum)
{
#ifdef X64
    switch (argnum) {
    case 0: return IF_X86_ELSE(REG_RDI, DR_REG_R0);
    case 1: return IF_X86_ELSE(REG_RSI, DR_REG_R1);
    case 2: return IF_X86_ELSE(REG_RDX, DR_REG_R2);
    case 3: return IF_X86_ELSE(REG_R10, DR_REG_R3); /* rcx = retaddr for OP_syscall */
    case 4: return IF_X86_ELSE(REG_R8,  DR_REG_R4);
    case 5: return IF_X86_ELSE(REG_R9,  DR_REG_R5);
    default: ASSERT(false, "invalid syscall argnum");
    }
#else
    switch (argnum) {
    case 0: return IF_ARM_ELSE(DR_REG_R0, DR_REG_EBX);
    case 1: return IF_ARM_ELSE(DR_REG_R1, DR_REG_ECX);
    case 2: return IF_ARM_ELSE(DR_REG_R2, DR_REG_EDX);
    case 3: return IF_ARM_ELSE(DR_REG_R3, DR_REG_ESI);
    case 4: return IF_ARM_ELSE(DR_REG_R4, DR_REG_EDI);
    case 5:
        /* for vsyscall, value is instead on stack */
        return IF_ARM_ELSE(DR_REG_R5, DR_REG_EBP);
    default:
        ASSERT(false, "invalid syscall argnum");
    }
#endif
    return REG_NULL;
}

/* Either sets arg->reg to DR_REG_NULL and sets arg->start_addr, or sets arg->reg
 * to non-DR_REG_NULL
 */
void
drsyscall_os_get_sysparam_location(cls_syscall_t *pt, uint argnum, drsys_arg_t *arg)
{
    reg_id_t reg = sysparam_reg(argnum);
    /* DR's syscall events don't tell us if this was vsyscall so we compare
     * values to find out
     */
    if (IF_X86_ELSE(reg == DR_REG_EBP &&
                    reg_get_value(reg, arg->mc) != pt->sysarg[argnum], false)) {
        /* must be vsyscall */
        ASSERT(!is_using_sysint(), "vsyscall incorrect assumption");
        arg->reg = DR_REG_NULL;
        arg->start_addr = (app_pc) arg->mc->xsp;
    } else {
        arg->reg = reg;
        arg->start_addr = NULL;
    }
}

drmf_status_t
drsys_syscall_type(drsys_syscall_t *syscall, drsys_syscall_type_t *type DR_PARAM_OUT)
{
    if (syscall == NULL || type == NULL)
        return DRMF_ERROR_INVALID_PARAMETER;
    *type = DRSYS_SYSCALL_TYPE_KERNEL;
    return DRMF_SUCCESS;
}

/***************************************************************************
 * PER-SYSCALL HANDLING
 */

static void
handle_clone(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint flags = (uint) pt->sysarg[0];

    /* PR 426162: pre-2.5.32-kernel, only 2 args.  Later glibc clone() has 3
     * optional args.  It blindly copies the 3 added args into registers, but
     * the kernel ignores them unless selected by appropriate flags.
     * We check the writes here to avoid races (xref PR 408540).
     */
    if (TEST(CLONE_PARENT_SETTID, flags)) {
        pid_t *ptid = SYSARG_AS_PTR(pt, 2, pid_t *);
        if (!report_sysarg(ii, 2, SYSARG_WRITE))
            return;
        if (ptid != NULL) {
            if (!report_memarg_type(ii, 2, SYSARG_WRITE, (app_pc) ptid, sizeof(*ptid),
                                    NULL, DRSYS_TYPE_INT, NULL))
                return;
        }
    }
    if (TEST(CLONE_SETTLS, flags)) {
#ifdef X86_32
        /* handle differences in type name */
# ifdef _LINUX_LDT_H
        typedef struct modify_ldt_ldt_s user_desc_t;
# else
        typedef struct user_desc user_desc_t;
# endif
        user_desc_t *tls = SYSARG_AS_PTR(pt, 3, user_desc_t *);
#endif
        if (!report_sysarg(ii, 3, SYSARG_READ))
            return;
#ifdef X86_32 /* for X64 or for ARM, inlined value */
        if (tls != NULL) {
            if (!report_memarg_type(ii, 3, SYSARG_READ, (app_pc) tls, sizeof(*tls), NULL,
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
#endif
    }
    if (TESTANY(CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID, flags)) {
        /* Even though CLEARTID is not used until child exit, and the address
         * can be changed later with set_tid_address(), and at one time glibc
         * didn't support the param but the kernel did, the kernel will store
         * this address so we should complain.
         */
        pid_t *ptid = SYSARG_AS_PTR(pt, 4, pid_t *);
        if (!report_sysarg(ii, 4, SYSARG_WRITE))
            return;
        if (ptid != NULL) {
            if (!report_memarg_type(ii, 4, SYSARG_WRITE, (app_pc) ptid, sizeof(*ptid),
                                    NULL, DRSYS_TYPE_INT, NULL))
                return;
        }
    }
}

static ssize_t
ipmi_addr_len_adjust(struct ipmi_addr * addr)
{
    /* Some types have the final byte as padding and when initialized
     * field-by-field with no memset we complain about uninit on that byte.
     * FIXME: this is a general problem w/ syscall param checking!
     */
    if (addr->addr_type == IPMI_SYSTEM_INTERFACE_ADDR_TYPE ||
        addr->addr_type == IPMI_LAN_ADDR_TYPE)
        return -1;
    return 0;
}

#define IOCTL_BUF_ARGNUM 2

static void
handle_pre_ioctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ptr_uint_t request = (ptr_uint_t) pt->sysarg[1];
    void *arg = SYSARG_AS_PTR(pt, IOCTL_BUF_ARGNUM, void *);
    if (arg == NULL)
        return;
    /* easier to safe_read the whole thing at once
     * N.B.: be careful about large structs that don't all have to be set
     * causing us to fail to read when really syscall would work fine
     */
    union ioctl_data data;
    /* shorter, easier-to-read code */
#define CHECK_DEF(ii, ptr, sz, id) do {                                       \
    if (!report_memarg_type(ii, IOCTL_BUF_ARGNUM, SYSARG_READ, (byte*)ptr,    \
                                sz, id, DRSYS_TYPE_STRUCT, NULL))             \
        return;                                                               \
} while (0)
#define CHECK_ADDR(ii, ptr, sz, id) do {                                       \
    if (!report_memarg_type(ii, IOCTL_BUF_ARGNUM, SYSARG_WRITE, (byte*)ptr,    \
                                sz, id, DRSYS_TYPE_STRUCT, NULL))              \
        return;                                                                \
} while (0)

    /* From "man ioctl_list".  This switch handles the special cases we've hit
     * so far, but the full table above has unhandled ioctls marked with
     * "FIXME: more".
     */
    switch (request) {
    // <include/linux/sockios.h>
    case SIOCGIFCONF: {
        struct ifconf input;
        CHECK_DEF(ii, arg, sizeof(struct ifconf), NULL);
        if (safe_read((void *)arg, sizeof(input), &input))
            CHECK_ADDR(ii, input.ifc_buf, input.ifc_len, "SIOCGIFCONF ifc_buf");
        return;
    }

    /* include <linux/ipmi.h> PR 531644 */
    case IPMICTL_SEND_COMMAND:
        CHECK_DEF(ii, arg, sizeof(struct ipmi_req), NULL); /*no id == arg itself*/
        if (safe_read((void *)arg, sizeof(struct ipmi_req), &data.req)) {
            CHECK_DEF(ii, data.req.addr, data.req.addr_len +
                      ipmi_addr_len_adjust((struct ipmi_addr *)data.req.addr),
                      "IPMICTL_SEND_COMMAND addr");
            CHECK_DEF(ii, data.req.msg.data, data.req.msg.data_len,
                      "IPMICTL_SEND_COMMAND msg.data");
        }
        return;
    case IPMICTL_SEND_COMMAND_SETTIME:
        CHECK_DEF(ii, arg, sizeof(struct ipmi_req_settime), NULL); /*no id == arg*/
        if (safe_read((void *)arg, sizeof(struct ipmi_req_settime), &data.reqs)) {
            CHECK_DEF(ii, data.reqs.req.addr, data.reqs.req.addr_len +
                      ipmi_addr_len_adjust((struct ipmi_addr *)data.reqs.req.addr),
                      "IPMICTL_SEND_COMMAND_SETTIME addr");
            CHECK_DEF(ii, data.reqs.req.msg.data, data.reqs.req.msg.data_len,
                      "IPMICTL_SEND_COMMAND_SETTIME msg.data");
        }
        return;
    case IPMICTL_RECEIVE_MSG:
    case IPMICTL_RECEIVE_MSG_TRUNC: {
        struct ipmi_recv *recv = (struct ipmi_recv *) arg;
        CHECK_ADDR(ii, arg, sizeof(struct ipmi_recv), NULL); /*no id == arg*/
        /* some fields are purely OUT so we must check the IN ones separately */
        CHECK_DEF(ii, &recv->addr, sizeof(recv->addr), NULL);
        CHECK_DEF(ii, &recv->addr_len, sizeof(recv->addr_len), NULL);
        CHECK_DEF(ii, &recv->msg.data, sizeof(recv->msg.data), NULL);
        CHECK_DEF(ii, &recv->msg.data_len, sizeof(recv->msg.data_len), NULL);
        if (safe_read((void *)arg, sizeof(struct ipmi_recv), &data.recv)) {
            CHECK_ADDR(ii, data.recv.addr, data.recv.addr_len,
                      "IPMICTL_RECEIVE_MSG* addr");
            CHECK_ADDR(ii, data.recv.msg.data, data.recv.msg.data_len,
                      "IPMICTL_RECEIVE_MSG* msg.data");
        }
        return;
    }
    }

#undef CHECK_DEF
#undef CHECK_ADDR
}

static void
handle_post_ioctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    ptr_uint_t request = (ptr_uint_t) pt->sysarg[1];
    void *arg = SYSARG_AS_PTR(pt, 2, ptr_uint_t *);
    ptr_int_t result = dr_syscall_get_result(drcontext);
    if (arg == NULL)
        return;
    if (result < 0)
        return;
    /* easier to safe_read the whole thing at once
     * to save space we could use a different union that only has the
     * structs needed in post: though currently it's the same set,
     * and most likely the larger ones will be in post.
     */
    union ioctl_data data;
    /* shorter, easier-to-read code */
#define MARK_WRITE(ii, ptr, sz, id) do {                                       \
    if (!report_memarg_type(ii, IOCTL_BUF_ARGNUM, SYSARG_WRITE, (byte*)ptr,    \
                                sz, id, DRSYS_TYPE_STRUCT, NULL))              \
        return;                                                                \
} while (0)
    switch (request) {
    case SIOCGIFCONF: {
        struct ifconf output;
        if (safe_read((void *)arg, sizeof(output), &output))
            MARK_WRITE(ii, output.ifc_buf, output.ifc_len, "SIOCGIFCONF ifc_buf");
        return;
    }
    case IPMICTL_RECEIVE_MSG:
    case IPMICTL_RECEIVE_MSG_TRUNC:
        if (safe_read((void *)arg, sizeof(struct ipmi_recv), &data.recv)) {
            MARK_WRITE(ii, data.recv.addr, data.recv.addr_len,
                       "IPMICTL_RECEIVE_MSG* addr");
            MARK_WRITE(ii, data.recv.msg.data, data.recv.msg.data_len,
                       "IPMICTL_RECEIVE_MSG* msg.data");
        }
        return;
    }
#undef MARK_WRITE
}

/* struct sockaddr is large but the meaningful portions vary by family.
 * This routine stores the socklen passed in pre-syscall and uses it to
 * take a MIN(pre,post) in post.
 * It performs all checks including on whole struct.
 */
static void
check_sockaddr(cls_syscall_t *pt, sysarg_iter_info_t *ii,
               byte *ptr, socklen_t socklen, int ordinal, uint arg_flags, const char *id)
{
    ASSERT(sizeof(socklen_t) <= sizeof(size_t), "shared code size type sanity check");
    handle_sockaddr(pt, ii, ptr, socklen, ordinal, arg_flags, id);
}

/* scatter-gather buffer vector handling.
 * loops until bytes checked == bytes_read
 */
static void
check_iov(cls_syscall_t *pt, sysarg_iter_info_t *ii,
          struct iovec *iov, size_t iov_len, size_t bytes_read,
          int ordinal, uint arg_flags, const char *id)
{
    uint i;
    size_t bytes_so_far = 0;
    bool done = false;
    struct iovec iov_copy;
    if (iov == NULL || iov_len == 0)
        return;
    if (!report_memarg_type(ii, ordinal, arg_flags, (app_pc)iov,
                            iov_len * sizeof(struct iovec), id, DRSYS_TYPE_STRUCT, NULL))
        return;
    for (i = 0; i < iov_len; i++) {
        if (safe_read(&iov[i], sizeof(iov_copy), &iov_copy)) {
            if (bytes_so_far + iov_copy.iov_len > bytes_read) {
                done = true;
                iov_copy.iov_len = (bytes_read - bytes_so_far);
            }
            bytes_so_far += iov_copy.iov_len;
            LOG(3, "check_iov: iov entry %d, buf="PFX", len="PIFX"\n",
                i, iov_copy.iov_base, iov_copy.iov_len);
            if (iov_copy.iov_len > 0 &&
                !report_memarg_type(ii, ordinal, arg_flags, (app_pc)iov_copy.iov_base,
                                    iov_copy.iov_len, id, DRSYS_TYPE_STRUCT, NULL))
                return;
            if (done)
                break;
        }
    }
}

/* checks an array of cstrings */
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

/* checks entire struct so caller need do nothing */
static void
check_msghdr(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
             byte *ptr, size_t len, int ordinal, uint arg_flags)
{
    bool sendmsg = TEST(SYSARG_READ, arg_flags); /* else, recvmsg */
    struct msghdr *msg = (struct msghdr *) ptr;
    byte *ptr1, *ptr2;
    size_t val_socklen;
    if (ii->arg->pre) {
        /* pre-syscall */
        size_t len = sendmsg ? sizeof(struct msghdr) :
            /* msg_flags is an out param */
            offsetof(struct msghdr, msg_flags);
        LOG(3, "\tmsg="PFX", name="PFX", iov="PFX", control="PFX"\n",
            msg, msg->msg_name, msg->msg_iov, msg->msg_control);/*unsafe reads*/
        if (!report_memarg_type(ii, ordinal, arg_flags, (app_pc)msg, len,
                                sendmsg ? "sendmsg msg" : "recvmsg msg",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (!sendmsg) {
            if (!report_memarg_type(ii, ordinal, arg_flags,
                                    (app_pc)&msg->msg_flags, sizeof(msg->msg_flags),
                                    "recvmsg msg_flags", DRSYS_TYPE_INT, NULL))
                return;
        }
        if (safe_read(&msg->msg_name, sizeof(msg->msg_name), &ptr2) &&
            safe_read(&msg->msg_namelen, sizeof(msg->msg_namelen), &val_socklen) &&
            ptr2 != NULL) {
            if (sendmsg) {
                check_sockaddr(pt, ii, ptr2, val_socklen, ordinal, SYSARG_READ,
                               "sendmsg addr");
                if (ii->abort)
                    return;
            } else {
                if (!report_memarg_type(ii, ordinal, arg_flags, ptr2, val_socklen,
                                        "recvmsg addr", DRSYS_TYPE_STRUCT, NULL))
                return;
            }
        }
        if (safe_read(&msg->msg_iov, sizeof(msg->msg_iov), &ptr1) &&
            safe_read(&msg->msg_iovlen, sizeof(msg->msg_iovlen), &len) &&
            ptr1 != NULL) {
            check_iov(pt, ii, (struct iovec *) ptr1, len, 0, ordinal, arg_flags,
                      sendmsg ? "sendmsg iov" : "recvmsg iov");
            if (ii->abort)
                return;
        }
        if (safe_read(&msg->msg_control, sizeof(msg->msg_control), &ptr2) &&
            safe_read(&msg->msg_controllen, sizeof(msg->msg_controllen),
                      &val_socklen)) {
            if (pt->first_iter) {
                store_extra_info(pt, EXTRA_INFO_MSG_CONTROL, (ptr_int_t) ptr2);
                store_extra_info(pt, EXTRA_INFO_MSG_CONTROLLEN, val_socklen);
            }
            if (ptr2 != NULL) {
                if (!report_memarg_type(ii, ordinal, arg_flags, ptr2, val_socklen,
                                        sendmsg ? "sendmsg msg_control" :
                                        "recvmsg msg_control", DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        } else if (pt->first_iter) {
            store_extra_info(pt, EXTRA_INFO_MSG_CONTROL, 0);
            store_extra_info(pt, EXTRA_INFO_MSG_CONTROLLEN, 0);
        }
    } else {
        /* post-syscall: thus must be recvmsg */
        ptr_int_t result = dr_syscall_get_result(drcontext);
        struct iovec *iov;
        size_t len;
        /* we saved this in pre-syscall */
        void *pre_control = (void *) read_extra_info(pt, EXTRA_INFO_MSG_CONTROL);
        size_t pre_controllen = (size_t) read_extra_info(pt, EXTRA_INFO_MSG_CONTROLLEN);
        ASSERT(!sendmsg, "logic error"); /* currently axiomatic but just in case */
        if (!report_memarg_type(ii, ordinal, arg_flags, (app_pc)&msg->msg_flags,
                                sizeof(msg->msg_flags), "recvmsg msg_flags",
                                DRSYS_TYPE_INT, NULL))
            return;
        if (safe_read(&msg->msg_iov, sizeof(msg->msg_iov), &iov) &&
            safe_read(&msg->msg_iovlen, sizeof(msg->msg_iovlen), &len) &&
            iov != NULL) {
            check_iov(pt, ii, iov, len, result, ordinal, arg_flags, "recvmsg iov");
            if (ii->abort)
                return;
        }
        if (safe_read(&msg->msg_name, sizeof(msg->msg_name), &ptr2) &&
            safe_read(&msg->msg_namelen, sizeof(msg->msg_namelen), &val_socklen) &&
            ptr2 != NULL) {
            check_sockaddr(pt, ii, (app_pc)ptr2, val_socklen, ordinal, arg_flags,
                           "recvmsg addr");
            if (ii->abort)
                return;
        }
        /* re-read to see size returned by kernel */
        if (safe_read(&msg->msg_controllen, sizeof(msg->msg_controllen), &val_socklen)) {
            /* Not sure what kernel does on truncation so being safe */
            size_t len = (val_socklen <= pre_controllen) ? val_socklen : pre_controllen;
            if (!report_memarg_type(ii, ordinal, arg_flags, (app_pc)&msg->msg_controllen,
                                    sizeof(msg->msg_controllen), "recvmsg msg_controllen",
                                    DRSYS_TYPE_INT, NULL))
                return;
            if (pre_control != NULL && len > 0) {
                if (!report_memarg_type(ii, ordinal, arg_flags, pre_control, len,
                                        "recvmsg msg_control", DRSYS_TYPE_STRUCT, NULL))
                    return;
            } else
                ASSERT(len == 0, "msg w/ no data can't have non-zero len!");
        }
    }
}

#ifndef X64 /* XXX i#1013: for mixed-mode we'll need to indirect SYS_socketcall, etc. */
static void
handle_pre_socketcall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    /* The first sysparam is an array of args of varying length */
#   define SOCK_ARRAY_ARG 1
    ptr_uint_t *arg = SYSARG_AS_PTR(pt, SOCK_ARRAY_ARG, ptr_uint_t *);
    app_pc ptr1, ptr2;
    socklen_t val_socklen;
    size_t val_size;
    const char *id = NULL;
    /* we store some values for post-syscall on successful safe_read using
     * these array values beyond our 2 params
     */
    if (pt->first_iter) {
        pt->sysarg[2] = 0;
        pt->sysarg[3] = 0;
        pt->sysarg[4] = 0;
        pt->sysarg[5] = 0;
    }
    LOG(2, "pre-sys_socketcall request=%d arg="PFX"\n", request, arg);
    LOG(3, "\targs: 0="PFX", 2="PFX", 3="PFX", 4="PFX"\n",
        arg[0], arg[1], arg[2], arg[3], arg[4]);/*unsafe reads*/
    if (arg == NULL)
        return;
    /* XXX: could use SYSINFO_SECONDARY_TABLE instead */
    switch (request) {
    case SYS_SOCKET:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                3*sizeof(ptr_uint_t), "socket", DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    case SYS_BIND:
        id = "bind";
    case SYS_CONNECT:
        id = (id == NULL) ? "connect" : id;
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                3*sizeof(ptr_uint_t), id, DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[2], sizeof(val_socklen), &val_socklen) &&
            safe_read((void *)&arg[1], sizeof(arg[1]), &ptr1)) {
            check_sockaddr(pt, ii, ptr1, val_socklen, SOCK_ARRAY_ARG, SYSARG_READ, id);
            if (ii->abort)
                return;
        }
        break;
    case SYS_SHUTDOWN:
        id = "shutdown";
    case SYS_LISTEN:
        id = (id == NULL) ? "listen" : id;
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                2*sizeof(ptr_uint_t), id, DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    case SYS_ACCEPT:
        id = "accept";
    case SYS_GETSOCKNAME:
        id = (id == NULL) ? "getsockname" : id;
    case SYS_GETPEERNAME:
        id = (id == NULL) ? "getpeername" : id;
    case SYS_ACCEPT4:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                3*sizeof(ptr_uint_t), id, DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[2], sizeof(arg[2]), &ptr2) &&
            safe_read(ptr2, sizeof(val_socklen), &val_socklen) &&
            safe_read((void *)&arg[1], sizeof(arg[1]), &ptr1)) {
            /* the size is an in-out arg */
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, ptr2,
                                    sizeof(socklen_t), id, DRSYS_TYPE_INT, NULL))
                return;
            if (pt->first_iter) {
                pt->sysarg[2] = (ptr_int_t) ptr1;
                pt->sysarg[3] = val_socklen;
            }
            if (ptr1 != NULL) { /* ok to be NULL for SYS_ACCEPT at least */
                check_sockaddr(pt, ii, ptr1, val_socklen, SOCK_ARRAY_ARG, SYSARG_WRITE,
                               id);
                if (ii->abort)
                    return;
            }
        }
        break;
    case SYS_SOCKETPAIR:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                4*sizeof(ptr_uint_t), "socketpair",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[3], sizeof(arg[3]), &ptr1)) {
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_WRITE, ptr1,
                                    2*sizeof(int), "socketpair", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    case SYS_SEND:
        id = "send";
    case SYS_RECV:
        id = (id == NULL) ? "recv" : id;
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                4*sizeof(ptr_uint_t), id, DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[2], sizeof(arg[2]), &val_size) &&
            safe_read((void *)&arg[1], sizeof(arg[1]), &ptr1)) {
            if (pt->first_iter) {
                pt->sysarg[4] = (ptr_int_t) ptr1;
                pt->sysarg[5] = val_size;
            }
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG,
                                    request == SYS_SEND ? SYSARG_READ :
                                    SYSARG_WRITE, ptr1, val_size, id,
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    case SYS_SENDTO:
    case SYS_RECVFROM:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                6*sizeof(ptr_uint_t),
                                (request == SYS_SENDTO) ? "sendto args" : "recvfrom args",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[2], sizeof(arg[2]), &val_size) &&
            safe_read((void *)&arg[1], sizeof(arg[1]), &ptr1)) {
            if (pt->first_iter) {
                pt->sysarg[4] = (ptr_int_t) ptr1;
                pt->sysarg[5] = val_size;
            }
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG,
                                    request == SYS_SENDTO ? SYSARG_READ : SYSARG_WRITE,
                                    ptr1, val_size, (request == SYS_SENDTO) ?
                                    "sendto buf" : "recvfrom buf",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        if (((request == SYS_SENDTO &&
              safe_read((void *)&arg[5], sizeof(val_socklen), &val_socklen)) ||
             (request == SYS_RECVFROM &&
              safe_read((void *)&arg[5], sizeof(arg[5]), &ptr2) &&
              safe_read(ptr2, sizeof(val_socklen), &val_socklen))) &&
            safe_read((void *)&arg[4], sizeof(arg[4]), &ptr1)) {
            if (pt->first_iter) {
                pt->sysarg[2] = (ptr_int_t) ptr1;
                pt->sysarg[3] = val_socklen;
            }
            if (ptr1 == NULL)
                break;  /* sockaddr is optional to both sendto and recvfrom */
            if (request == SYS_SENDTO) {
                check_sockaddr(pt, ii, ptr1, val_socklen, SOCK_ARRAY_ARG,
                               SYSARG_READ, "sendto addr");
                if (ii->abort)
                    return;
            } else {
                /* XXX: save socklen for post recvfrom() handline.
                 * check_sockaddr() would store socklen for us, but it reads
                 * sa_family, which is uninitialized for recvfrom().
                 */
                if (pt->first_iter)
                    store_extra_info(pt, EXTRA_INFO_SOCKADDR, val_socklen);
                if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_WRITE, ptr1,
                                        val_socklen, "recvfrom addr",
                                        DRSYS_TYPE_STRUCT, NULL))
                    return;
                if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ|SYSARG_WRITE,
                                        ptr2, sizeof(socklen_t), "recvfrom socklen",
                                        DRSYS_TYPE_UNSIGNED_INT, NULL))
                    return;
            }
        }
        break;
    case SYS_SETSOCKOPT:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                5*sizeof(ptr_uint_t), "setsockopt args",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[4], sizeof(val_socklen), &val_socklen) &&
            safe_read((void *)&arg[3], sizeof(arg[3]), &ptr1)) {
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, ptr1, val_socklen,
                                    "setsockopt optval", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    case SYS_GETSOCKOPT:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                5*sizeof(ptr_uint_t), "getsockopt args",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[4], sizeof(arg[4]), &ptr2) &&
            safe_read(ptr2, sizeof(val_socklen), &val_socklen) &&
            safe_read((void *)&arg[3], sizeof(arg[3]), &ptr1) &&
            ptr1 != NULL/*optional*/) {
            /* in-out arg */
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, ptr2,
                                    sizeof(socklen_t), "getsockopt optval",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (pt->first_iter) {
                pt->sysarg[2] = (ptr_int_t) ptr1;
                pt->sysarg[3] = val_socklen;
            }
            if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_WRITE, ptr1, val_socklen,
                                    "getsockopt optlen", DRSYS_TYPE_INT, NULL))
                return;
        }
        break;
    case SYS_SENDMSG:
    case SYS_RECVMSG:
        if (!report_memarg_type(ii, SOCK_ARRAY_ARG, SYSARG_READ, (app_pc) arg,
                                3*sizeof(ptr_uint_t), (request == SYS_SENDMSG) ?
                                "sendmsg args" : "recvmsg args", DRSYS_TYPE_STRUCT, NULL))
            return;
        if (safe_read((void *)&arg[1], sizeof(arg[1]), &ptr1)) {
            if (pt->first_iter)
                pt->sysarg[2] = (ptr_int_t) ptr1; /* struct msghdr* */
            check_msghdr(drcontext, pt, ii, ptr1, sizeof(struct msghdr),
                         SOCK_ARRAY_ARG, (request == SYS_SENDMSG) ? SYSARG_READ :
                         SYSARG_WRITE);
            if (ii->abort)
                return;
        }
        break;
    default:
        ELOGF(0, f_global, "WARNING: unknown socketcall request %d\n", request);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
}

static void
handle_post_socketcall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    ptr_uint_t *arg = SYSARG_AS_PTR(pt, SOCK_ARRAY_ARG, ptr_uint_t *);
    ptr_int_t result = dr_syscall_get_result(drcontext);
    app_pc ptr2;
    socklen_t val_socklen;
    const char *id = NULL;
    LOG(2, "post-sys_socketcall result="PIFX"\n", result);
    if (result < 0)
        return;
    switch (request) {
    case SYS_ACCEPT:
        id = "accept";
    case SYS_GETSOCKNAME:
        id = (id == NULL) ? "getsockname" : id;
    case SYS_GETPEERNAME:
        id = (id == NULL) ? "getpeername" : id;
    case SYS_ACCEPT4:
        if (pt->sysarg[3]/*pre-addrlen*/ > 0 && pt->sysarg[2]/*sockaddr*/ != 0 &&
            /* re-read to see size returned by kernel */
            safe_read((void *)&arg[2], sizeof(arg[2]), &ptr2) &&
            safe_read(ptr2, sizeof(val_socklen), &val_socklen)) {
            check_sockaddr(pt, ii, SYSARG_AS_PTR(pt, 2, app_pc), val_socklen, SOCK_ARRAY_ARG,
                           SYSARG_WRITE, id);
            if (ii->abort)
                return;
        }
        break;
    case SYS_RECV:
        if (pt->sysarg[4]/*buf*/ != 0) {
            /* Not sure what kernel does on truncation so being safe */
            size_t len = (result <= pt->sysarg[5]/*buflen*/) ? result : pt->sysarg[5];
            if (len > 0) {
                if (!report_memarg_type(ii, 4, SYSARG_WRITE, SYSARG_AS_PTR(pt, 4, app_pc),
                                        len, "recv", DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    case SYS_RECVFROM:
        if (pt->sysarg[4]/*buf*/ != 0) {
            /* Not sure what kernel does on truncation so being safe */
            size_t len = (result <= pt->sysarg[5]/*buflen*/) ? result : pt->sysarg[5];
            if (len > 0) {
                if (!report_memarg_type(ii, 4, SYSARG_WRITE, SYSARG_AS_PTR(pt, 4, app_pc),
                                        len, "recvfrom buf", DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        if (pt->sysarg[3]/*pre-addrlen*/ > 0 && pt->sysarg[2]/*sockaddr*/ != 0 &&
            /* re-read to see size returned by kernel */
            safe_read((void *)&arg[5], sizeof(arg[5]), &ptr2) &&
            safe_read(ptr2, sizeof(val_socklen), &val_socklen) &&
            val_socklen > 0) {
            check_sockaddr(pt, ii, SYSARG_AS_PTR(pt, 2, app_pc), val_socklen, 2, SYSARG_WRITE,
                           "recvfrom addr");
            if (ii->abort)
                return;
        }
        break;
    case SYS_GETSOCKOPT:
        if (pt->sysarg[3]/*pre-optlen*/ > 0 && pt->sysarg[2]/*optval*/ != 0 &&
            /* re-read to see size returned by kernel */
            safe_read((void *)&arg[4], sizeof(arg[4]), &ptr2) &&
            safe_read(ptr2, sizeof(val_socklen), &val_socklen)) {
            /* Not sure what kernel does on truncation so being safe */
            size_t len = (val_socklen <= pt->sysarg[3]) ? val_socklen : pt->sysarg[3];
            if (!report_memarg_type(ii, 2, SYSARG_WRITE, SYSARG_AS_PTR(pt, 2, app_pc),
                                    len, "getsockopt", DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    case SYS_RECVMSG: {
        if (pt->sysarg[2] != 0) { /* if 0, error on safe_read in pre */
            check_msghdr(drcontext, pt, ii, SYSARG_AS_PTR(pt, 2, byte *), sizeof(struct msghdr),
                         SOCK_ARRAY_ARG, SYSARG_WRITE);
            if (ii->abort)
                return;
        }
        break;
    }
    }
}
#endif /* !X64 */

static uint
ipc_sem_len(int semid)
{
    struct semid_ds ds;
    union semun ctlarg;
    ctlarg.buf = &ds;
    /* FIXME PR 519781: not tested! */
    if (
#ifdef X64
        raw_syscall(SYS_semctl, 4, semid, 0, IPC_STAT, (ptr_int_t)&ctlarg)
#else
        raw_syscall(SYS_ipc, 5, SEMCTL, semid, 0, IPC_STAT, (ptr_int_t)&ctlarg)
#endif
        < 0)
        return 0;
    else
        return ds.sem_nsems;
}

/* Note that we can't use a SYSINFO_SECONDARY_TABLE for this b/c some params
 * are not always used
 */
static void
handle_semctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
              /* shifted by one for 32-bit so we take in the base */
              int argnum_semid)
{
    /* int semctl(int semid, int semnum, int cmd, union semun arg) */
    uint cmd;
    ptr_int_t arg_val;
    union semun arg;
    int semid;
    ASSERT(argnum_semid + 3 < SYSCALL_NUM_ARG_STORE, "index too high");
    cmd = (uint) pt->sysarg[argnum_semid + 2];
    arg_val = (ptr_int_t) pt->sysarg[argnum_semid + 3];
    arg = *(union semun *) &arg_val;
    semid = (int) pt->sysarg[argnum_semid];
    if (!ii->arg->pre && (ptr_int_t)dr_syscall_get_result(drcontext) < 0)
        return;
    /* strip out the version flag or-ed in by libc */
    cmd &= (~IPC_64);
    if (ii->arg->pre) {
        if (!report_sysarg(ii, argnum_semid, SYSARG_READ))
            return;
        if (!report_sysarg(ii, argnum_semid + 2/*cmd*/, SYSARG_READ))
            return;
    }
    switch (cmd) {
    case IPC_SET:
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
            if (!report_memarg_type(ii, argnum_semid + 3, SYSARG_READ, (app_pc) arg.buf,
                                    sizeof(struct semid_ds), "semctl.IPC_SET",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    case IPC_STAT:
    case SEM_STAT:
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
        }
        if (!report_memarg_type(ii, argnum_semid + 3, SYSARG_WRITE, (app_pc) arg.buf,
                                sizeof(struct semid_ds),
                                (cmd == IPC_STAT) ? "semctl.IPC_STAT" : "semctl.SEM_STAT",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    case IPC_RMID: /* nothing further */
        break;
    case IPC_INFO:
    case SEM_INFO:
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
        }
        if (!report_memarg_type(ii,argnum_semid + 3,  SYSARG_WRITE, (app_pc) arg.__buf,
                                sizeof(struct seminfo),
                                (cmd == IPC_INFO) ? "semctl.IPC_INFO" : "semctl.SEM_INFO",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    case GETALL: {
        /* we must query to get the length of arg.array */
        uint semlen = ipc_sem_len(semid);
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
        }
        if (!report_memarg_type(ii, argnum_semid + 3, SYSARG_WRITE, (app_pc) arg.array,
                                semlen*sizeof(short), "semctl.GETALL",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case SETALL: {
        if (ii->arg->pre) {
            /* we must query to get the length of arg.array */
            uint semlen = ipc_sem_len(semid);
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
            if (!report_memarg_type(ii, argnum_semid + 3, SYSARG_READ, (app_pc) arg.array,
                                    semlen*sizeof(short), "semctl.SETALL",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    }
    case GETNCNT:
    case GETZCNT:
    case GETPID:
    case GETVAL:
        if (ii->arg->pre)
            if (!report_sysarg(ii, argnum_semid + 1/*semnum*/,SYSARG_READ))
                return;
        break;
    case SETVAL:
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_semid + 1/*semnun*/,SYSARG_READ))
                return;
            if (!report_sysarg(ii, argnum_semid + 3/*semun*/,SYSARG_READ))
                return;
        }
        break;
    default:
        ELOGF(0, f_global, "WARNING: unknown SEMCTL request %d\n", cmd);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
}

/* Note that we can't use a SYSINFO_SECONDARY_TABLE for this b/c some params
 * are not always used
 */
static void
handle_msgctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
              /* arg numbers vary for 32-bit vs 64-bit so we take them in */
              int argnum_msqid, int argnum_cmd, int argnum_buf)
{
    uint cmd = (uint) pt->sysarg[argnum_cmd];
    byte *ptr = SYSARG_AS_PTR(pt, argnum_buf, byte *);
    if (!ii->arg->pre && (ptr_int_t)dr_syscall_get_result(drcontext) < 0)
        return;
    if (ii->arg->pre) {
        if (!report_sysarg(ii, argnum_msqid, SYSARG_READ))
            return;
        if (!report_sysarg(ii, argnum_cmd, SYSARG_READ))
            return;
    }
    switch (cmd) {
    case IPC_INFO:
    case MSG_INFO: {
        struct msginfo *buf = (struct msginfo *) ptr;
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                return;
        }
        /* not all fields are set but we simplify */
        if (!report_memarg_type(ii, argnum_buf, SYSARG_WRITE, (app_pc) buf, sizeof(*buf),
                                (cmd == IPC_INFO) ? "msgctl ipc_info" : "msgctl msg_info",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case IPC_STAT:
    case MSG_STAT: {
        struct msqid_ds *buf = (struct msqid_ds *) ptr;
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                return;
        }
        if (!report_memarg_type(ii, argnum_buf, SYSARG_WRITE, (app_pc) buf, sizeof(*buf),
                                (cmd == IPC_STAT) ?  "msgctl ipc_stat" : "msgctl msg_stat",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case IPC_SET: {
        if (ii->arg->pre) {
            struct msqid_ds *buf = (struct msqid_ds *) ptr;
            if (ii->arg->pre) {
                if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                    return;
            }
            /* not all fields are read but we simplify */
            if (!report_memarg_type(ii, argnum_buf, SYSARG_READ, (app_pc) buf,
                                    sizeof(*buf), "msgctl ipc_set",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
        break;
    }
    case IPC_RMID: /* nothing further to do */
        break;
    default:
        ELOGF(0, f_global, "WARNING: unknown MSGCTL request %d\n", cmd);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
}

/* Note that we can't use a SYSINFO_SECONDARY_TABLE for this b/c some params
 * are not always used
 */
static void
handle_shmctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
              /* arg numbers vary for 32-bit vs 64-bit so we take them in */
              int argnum_shmid, int argnum_cmd, int argnum_buf)
{
    uint cmd = (uint) pt->sysarg[argnum_cmd];
    byte *ptr = SYSARG_AS_PTR(pt, argnum_buf, byte *);
    if (!ii->arg->pre && (ptr_int_t)dr_syscall_get_result(drcontext) < 0)
        return;
    if (ii->arg->pre) {
        if (!report_sysarg(ii, argnum_shmid, SYSARG_READ))
            return;
        if (!report_sysarg(ii, argnum_cmd, SYSARG_READ))
            return;
    }
    switch (cmd) {
    case IPC_INFO:
    case SHM_INFO: {
        struct shminfo *buf = (struct shminfo *) ptr;
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                return;
        }
        /* not all fields are set but we simplify */
        if (!report_memarg_type(ii, argnum_buf, SYSARG_WRITE, (app_pc) buf, sizeof(*buf),
                                "shmctl ipc_info", DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case IPC_STAT:
    case SHM_STAT: {
        struct shmid_ds *buf = (struct shmid_ds *) ptr;
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                return;
        }
        if (!report_memarg_type(ii, argnum_buf, SYSARG_WRITE, (app_pc) buf, sizeof(*buf),
                                (cmd == IPC_STAT) ? "shmctl ipc_stat" : "shmctl shm_stat",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case IPC_SET: {
        struct shmid_ds *buf = (struct shmid_ds *) ptr;
        if (ii->arg->pre) {
            if (!report_sysarg(ii, argnum_buf, SYSARG_READ))
                return;
        }
        /* not all fields are read but we simplify */
        if (!report_memarg_type(ii, argnum_buf, ii->arg->pre ? SYSARG_WRITE : SYSARG_READ,
                                (app_pc) buf, sizeof(*buf), "shmctl ipc_set",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    }
    case IPC_RMID: /* nothing further to do */
        break;
    default:
        ELOGF(0, f_global, "WARNING: unknown SHMCTL request %d\n", cmd);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
}

static void
handle_pre_process_vm_readv_writev(void *drcontext, sysarg_iter_info_t *ii)
{
    int arg_flags_local;
    if(!strcmp(ii->pt->sysinfo->name, "process_vm_readv")) {
        arg_flags_local = SYSARG_WRITE;

        pid_t pid = (pid_t)(ii->pt->sysarg[0]);
        if (pid == dr_get_process_id()) {
            struct iovec *riov;
            riov = SYSARG_AS_PTR(ii->pt, 3, struct iovec *);
            unsigned long riovcnt = ii->pt->sysarg[4];

            /* size_t-1 is the max size_t value */
            check_iov(ii->pt, ii, riov, (size_t)riovcnt, (size_t)-1, 3, SYSARG_READ,
                    "remote_iov");
        }
    } else { /* process_vm_writev */
        arg_flags_local = SYSARG_READ;
    }

    struct iovec *liov;
    liov = SYSARG_AS_PTR(ii->pt, 1, struct iovec *);
    unsigned long liovcnt = ii->pt->sysarg[2];

    /* XXX: Passing (size_t)-1 (max size_t val) we check every member of the array,
     * but we are unable to know the true size of it. The liovcnt parameter
     * of the syscall holds the size but it can still be out of array bounds.
     */
    check_iov(ii->pt, ii, liov, (size_t)liovcnt, (size_t)-1, 1, arg_flags_local,
              "local_iov");
}

static void
handle_post_process_vm_readv(void *drcontext, sysarg_iter_info_t *ii)
{
    long res = dr_syscall_get_result(drcontext);

    if (res > 0) {
        struct iovec *liov;
        liov = SYSARG_AS_PTR(ii->pt, 1, struct iovec *);
        unsigned long liovcnt = ii->pt->sysarg[2];

        check_iov(ii->pt, ii, liov, (size_t)liovcnt, res, 1, SYSARG_WRITE,
                  "local_iov");
    }
}

static void
handle_post_process_vm_writev(void *drcontext, sysarg_iter_info_t *ii)
{
    long res = dr_syscall_get_result(drcontext);
    pid_t pid = (pid_t)(ii->pt->sysarg[0]);

    if (res > 0 && pid == dr_get_process_id()) {
        struct iovec *riov;
        riov = SYSARG_AS_PTR(ii->pt, 3, struct iovec *);
        unsigned long riovcnt = ii->pt->sysarg[4];

        check_iov(ii->pt, ii, riov, (size_t)riovcnt, res, 3, SYSARG_WRITE,
                  "remote_iov");
    }
}

static void
check_msgbuf(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii,
             byte *ptr, size_t len, int ordinal, uint arg_flags)
{
    bool msgsnd = TEST(SYSARG_READ, arg_flags); /* else, msgrcv */
    struct msgbuf *buf = (struct msgbuf *) ptr;
    if (!ii->arg->pre) {
        if (msgsnd)
            return;
        else
            len = (size_t) dr_syscall_get_result(drcontext);
    }
    if (!report_memarg_type(ii, ordinal, arg_flags, (app_pc) &buf->mtype,
                            sizeof(buf->mtype), msgsnd? "msgsnd mtype" : "msgrcv mtype",
                            DRSYS_TYPE_INT, NULL))
        return;
    report_memarg_type(ii, ordinal, arg_flags, (app_pc) &buf->mtext, len,
                       msgsnd? "msgsnd mtext" : "msgrcv mtext",
                       DRSYS_TYPE_STRUCT, NULL);
}

#ifndef X64 /* XXX i#1013: for mixed-mode we'll need to indirect SYS_socketcall, etc. */
static void
handle_pre_ipc(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    int arg2 = (int) pt->sysarg[2];
    ptr_uint_t *ptr = SYSARG_AS_PTR(pt, 4, ptr_uint_t *);
    ptr_int_t arg5 = (int) pt->sysarg[5];
    /* They all use param #0, which is checked via table specifying 1 arg */
    /* Note that we can't easily use SYSINFO_SECONDARY_TABLE for these
     * b/c they don't require all their params to be defined.
     */
    switch (request) {
    case SEMTIMEDOP:
        /* int semtimedop(int semid, struct sembuf *sops, unsigned nsops,
         *                struct timespec *timeout)
         */
        if (!report_sysarg(ii, 5, SYSARG_READ))
            return;
        if (!report_memarg_type(ii, 5, SYSARG_READ, (app_pc) arg5,
                                sizeof(struct timespec), "semtimedop",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        /* fall-through */
    case SEMOP:
        /* int semop(int semid, struct sembuf *sops, unsigned nsops) */
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 4, SYSARG_READ))
            return;
        if (!report_memarg_type(ii, 4, SYSARG_READ, (app_pc) ptr,
                                arg2*sizeof(struct sembuf), "semop",
                                DRSYS_TYPE_STRUCT, NULL))
            return;
        break;
    case SEMGET: /* nothing */
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 3, SYSARG_READ))
            return;
        break;
    case SEMCTL: {
        /* int semctl(int semid, int semnum, int cmd, ...) */
        handle_semctl(drcontext, pt, ii, 1);
        if (ii->abort)
            return;
        break;
    }
    case MSGSND: {
        /* int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg) */
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return; /* msqid */
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return; /* msgsz */
        if (!report_sysarg(ii, 3, SYSARG_READ))
            return; /* msgflg */
        if (!report_sysarg(ii, 4, SYSARG_READ))
            return; /* msgp */
        check_msgbuf(drcontext, pt, ii, (byte *) ptr, arg2, 2, SYSARG_READ);
        if (ii->abort)
            return;
        break;
    }
    case MSGRCV: {
        /* ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,
         *                int msgflg)
         */
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return; /* msqid */
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return; /* msgsz */
        if (!report_sysarg(ii, 3, SYSARG_READ))
            return; /* msgflg */
        if (!report_sysarg(ii, 4, SYSARG_READ))
            return; /* msgp */
        if (!report_sysarg(ii, 5, SYSARG_READ))
            return; /* msgtyp */
        check_msgbuf(drcontext, pt, ii, (byte *) ptr, arg2, 2, SYSARG_WRITE);
        break;
    }
    case MSGGET:
        /* int msgget(key_t key, int msgflg) */
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return;
        break;
    case MSGCTL: {
        handle_msgctl(drcontext, pt, ii, 1, 2, 4);
        break;
    }
    case SHMAT:
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 4, SYSARG_READ))
            return;
        /* FIXME: this should be treated as a new mmap by DR? */
        break;
    case SHMDT:
        if (!report_sysarg(ii, 4, SYSARG_READ))
            return;
        break;
    case SHMGET:
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 2, SYSARG_READ))
            return;
        if (!report_sysarg(ii, 3, SYSARG_READ))
            return;
        break;
    case SHMCTL: {
        handle_shmctl(drcontext, pt, ii, 1, 2, 4);
        break;
    }
    default:
        ELOGF(0, f_global, "WARNING: unknown ipc request %d\n", request);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
    /* If you add any handling here: need to check ii->abort first */
}

static void
handle_post_ipc(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    ptr_uint_t *ptr = SYSARG_AS_PTR(pt, 4, ptr_uint_t *);
    ptr_int_t result = dr_syscall_get_result(drcontext);
    switch (request) {
    case SEMCTL: {
        handle_semctl(drcontext, pt, ii, 1);
        break;
    }
    case MSGRCV: {
        if (result >= 0) {
            check_msgbuf(drcontext, pt, ii, (byte *) ptr, (size_t) result,
                         4, SYSARG_WRITE);
        }
        break;
    }
    case MSGCTL: {
        handle_msgctl(drcontext, pt, ii, 1, 2, 4);
        break;
    }
    case SHMCTL: {
        handle_shmctl(drcontext, pt, ii, 1, 2, 4);
        break;
    }
    }
    /* If you add any handling here: need to check ii->abort first */
}
#endif /* !X64 */

/* handles both select and pselect6 */
static void
handle_pre_select(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    int nfds = (int) pt->sysarg[0];
    /* Only special-cased b/c the size is special: one bit each.
     * No post-syscall action needed b/c no writes to previously-undef mem.
     */
    size_t sz = nfds / 8; /* 8 bits per byte, size is in bytes */
    app_pc ptr = SYSARG_AS_PTR(pt, 1, app_pc);
    if (ptr != NULL) {
        if (!report_memarg_type(ii, 1, SYSARG_READ, ptr, sz,
                                "select readfds", DRSYS_TYPE_STRUCT, NULL))
            return;
    }
    ptr = SYSARG_AS_PTR(pt, 2, app_pc);
    if (ptr != NULL) {
        if (!report_memarg_type(ii, 2, SYSARG_READ, ptr, sz,
                                "select writefds", DRSYS_TYPE_STRUCT, NULL))
            return;
    }
    ptr = SYSARG_AS_PTR(pt, 3, app_pc);
    if (ptr != NULL) {
        if (!report_memarg_type(ii, 3, SYSARG_READ, ptr, sz,
                                "select exceptfds", DRSYS_TYPE_STRUCT, NULL))
            return;
    }
    ptr = SYSARG_AS_PTR(pt, 4, app_pc);
    if (ptr != NULL) {
        if (!report_memarg_type(ii, 4, SYSARG_READ, ptr,
                                (ii->arg->sysnum.number == SYS_select ?
                                 sizeof(struct timeval) : sizeof(struct timespec)),
                                "select timeout", DRSYS_TYPE_STRUCT, NULL))
            return;
    }
    if (ii->arg->sysnum.number == SYS_pselect6) {
        ptr = SYSARG_AS_PTR(pt, 5, app_pc);
        if (ptr != NULL) {
            if (!report_memarg_type(ii, 5, SYSARG_READ, ptr,
                                    sizeof(kernel_sigset_t), "pselect sigmask",
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
        }
    }
}

#define PRCTL_NAME_SZ 16 /* from man page */

static void
handle_pre_prctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    ptr_int_t arg1 = (ptr_int_t) pt->sysarg[1];
    /* They all use param #0, which is checked via table specifying 1 arg.
     * Officially it's a 5-arg syscall but so far nothing using beyond 2 args.
     */
    /* XXX: could use SYSINFO_SECONDARY_TABLE instead */
    switch (request) {
    case PR_SET_PDEATHSIG:
    case PR_SET_UNALIGN:
    case PR_SET_FPEMU:
    case PR_SET_FPEXC:
    case PR_SET_DUMPABLE:
    case PR_SET_TIMING:
    case PR_SET_TSC:
    case PR_SET_SECUREBITS:
    case PR_SET_SECCOMP:
    case PR_SET_KEEPCAPS:
    case PR_SET_ENDIAN:
    case PR_SET_TIMERSLACK:
    case PR_CAPBSET_READ:
    case PR_CAPBSET_DROP:
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        break;
    case PR_GET_PDEATHSIG:
    case PR_GET_UNALIGN:
    case PR_GET_FPEMU:
    case PR_GET_FPEXC:
    case PR_GET_TSC:
    case PR_GET_ENDIAN:
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_memarg_type(ii, 1, SYSARG_WRITE, (app_pc) arg1, sizeof(int), NULL,
                                DRSYS_TYPE_INT, NULL))
            return;
        break;
    case PR_GET_DUMPABLE:
    case PR_GET_TIMING:
    case PR_GET_SECUREBITS:
    case PR_GET_SECCOMP:
    case PR_GET_KEEPCAPS:
    case PR_GET_TIMERSLACK:
        /* returned data is just syscall return value */
        break;
    case PR_SET_NAME:
    case PR_GET_NAME:
        if (!report_sysarg(ii, 1, SYSARG_READ))
            return;
        if (!report_memarg_type(ii, 1, (request == PR_GET_NAME) ? SYSARG_WRITE :
                                SYSARG_READ, (app_pc) arg1, PRCTL_NAME_SZ, NULL,
                                DRSYS_TYPE_CARRAY, NULL))
            return;
        break;
    default:
        ELOGF(0, f_global, "WARNING: unknown prctl request %d\n", request);
        IF_DEBUG(report_callstack(ii->arg->drcontext, ii->arg->mc);)
        break;
    }
}

static void
handle_post_prctl(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    uint request = (uint) pt->sysarg[0];
    ptr_int_t result = dr_syscall_get_result(drcontext);
    switch (request) {
    case PR_GET_PDEATHSIG:
    case PR_GET_UNALIGN:
    case PR_GET_FPEMU:
    case PR_GET_FPEXC:
    case PR_GET_TSC:
    case PR_GET_ENDIAN:
        if (result >= 0) {
            if (!report_memarg_type(ii, 1, SYSARG_WRITE, SYSARG_AS_PTR(pt, 1, app_pc),
                                    sizeof(int), NULL, DRSYS_TYPE_INT, NULL))
                return;
        }
        break;
    case PR_GET_NAME:
        /* FIXME PR 408539: actually only writes up to null char */
        if (!report_memarg_type(ii, 1, SYSARG_WRITE, SYSARG_AS_PTR(pt, 1, app_pc),
                                PRCTL_NAME_SZ, NULL, DRSYS_TYPE_CARRAY, NULL))
            return;
        break;
    }
}

void
os_handle_pre_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    switch (ii->arg->sysnum.number) {
    case SYS_clone:
        handle_clone(drcontext, pt, ii);
        break;
#ifdef X86
    case SYS__sysctl: {
        struct __sysctl_args *args = SYSARG_AS_PTR(pt, 0, struct __sysctl_args *);
        if (args != NULL) {
            /* just doing reads here: writes in post */
            if (!report_memarg_type(ii, 0, SYSARG_READ, (app_pc) args->name,
                                    args->nlen*sizeof(int), NULL,
                                    DRSYS_TYPE_STRUCT, NULL))
                return;
            if (args->newval != NULL) {
                if (!report_memarg_type(ii, 0, SYSARG_READ, (app_pc) args->newval,
                                        args->newlen, NULL, DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    }
#endif
    case SYS_mremap: {
        /* 5th arg is conditionally valid */
        int flags = (int) pt->sysarg[3];
        if (TEST(MREMAP_FIXED, flags)) {
            if (!report_sysarg(ii, 4, SYSARG_READ))
                return;
        }
        break;
    }
    case SYS_open: {
        /* 3rd arg is sometimes required.  glibc open() wrapper passes
         * a constant 0 as mode if no O_CREAT, but opendir() bypasses
         * that wrapper (PR 488597).
         */
        int flags = (int) pt->sysarg[1];
        if (TEST(O_CREAT, flags)) {
            if (!report_sysarg(ii, 2, SYSARG_READ))
                return;
        }
        break;
    }
    case SYS_fcntl:
#ifndef X64
    case SYS_fcntl64:
#endif
        {
        /* 3rd arg is sometimes required.  Note that SYS_open has a similar
         * situation but we don't yet bother to special-case b/c glibc passes
         * a constant 0 as mode if no O_CREAT: yet fcntl glibc routine
         * blindly reads 3rd arg regardless of 2nd.
         */
            int cmd = (int) pt->sysarg[1];
            /* Some kernels add custom cmds, so error on side of false pos
             * rather than false neg via negative checks
             */
            if (cmd != F_GETFD && cmd != F_GETFL && cmd != F_GETOWN
#ifdef __USE_GNU
                && cmd != F_GETSIG && cmd != F_GETLEASE
#endif
                ) {
                if (!report_sysarg(ii, 2, SYSARG_READ))
                    return;
            }
        }
        break;
    case SYS_ioctl:
        handle_pre_ioctl(drcontext, pt, ii);
        break;
#ifdef X64
    case SYS_semctl:
        handle_semctl(drcontext, pt, ii, 0);
        break;
    case SYS_msgctl:
        handle_msgctl(drcontext, pt, ii, 0, 1, 2);
        break;
    case SYS_shmctl:
        handle_shmctl(drcontext, pt, ii, 0, 1, 2);
        break;
#else
    /* XXX i#1013: for mixed-mode we'll need is_sysnum() for access to these */
    case SYS_socketcall:
        handle_pre_socketcall(drcontext, pt, ii);
        break;
    case SYS_ipc:
        handle_pre_ipc(drcontext, pt, ii);
        break;
#endif
    case SYS_select: /* fall-through */
    case SYS_pselect6:
        handle_pre_select(drcontext, pt, ii);
        break;
    case SYS_poll: {
        struct pollfd *fds = SYSARG_AS_PTR(pt, 0, struct pollfd *);
        nfds_t nfds = (nfds_t) pt->sysarg[1];
        if (fds != NULL) {
            int i;
            for (i = 0; i < nfds; i++) {
                /* First fields are inputs, last is output */
                if (!report_memarg_type(ii, 0, SYSARG_READ, (app_pc) &fds[i],
                                        offsetof(struct pollfd, revents), NULL,
                                        DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    }
    case SYS_prctl:
        handle_pre_prctl(drcontext, pt, ii);
        break;
    case SYS_rt_sigaction: {
        /* restorer field not always filled in.  we ignore the old (pre-2.1.68)
         * kernel sigaction struct layout.
         */
        kernel_sigaction_t *sa = SYSARG_AS_PTR(pt, 1, kernel_sigaction_t *);
        if (sa != NULL) {
            if (TEST(SA_RESTORER, sa->flags)) {
                if (!report_memarg_type(ii, 1, SYSARG_READ, (app_pc) sa, sizeof(*sa),
                                        NULL, DRSYS_TYPE_STRUCT, NULL))
                    return;
            } else {
                if (!report_memarg_type(ii, 1, SYSARG_READ, (app_pc) sa,
                                        offsetof(kernel_sigaction_t, restorer), NULL,
                                        DRSYS_TYPE_STRUCT, NULL))
                    return;
                /* skip restorer field */
                if (!report_memarg_type(ii, 1, SYSARG_READ, (app_pc) &sa->mask,
                                        sizeof(*sa) - offsetof(kernel_sigaction_t, mask),
                                        NULL, DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    }
    case SYS_futex: {
        /* PR 479107: later args are optional */
        int op = (int) pt->sysarg[1];
        if (op == FUTEX_WAKE || op == FUTEX_FD) {
            /* just the 3 params */
        } else if (op == FUTEX_WAIT) {
            struct timespec *timeout = SYSARG_AS_PTR(pt, 3, struct timespec *);
            if (!report_sysarg(ii, 3, SYSARG_READ))
                return;
            if (timeout != NULL) {
                if (!report_memarg_type(ii, 3, SYSARG_READ, (app_pc) timeout,
                                        sizeof(*timeout), NULL, DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        } else if (op == FUTEX_REQUEUE || op == FUTEX_CMP_REQUEUE) {
            if (!report_sysarg(ii, 4, SYSARG_READ))
                return;
            if (op == FUTEX_CMP_REQUEUE) {
                if (!report_sysarg(ii, 5, SYSARG_READ))
                    return;
            }
            if (!report_memarg_type(ii, 4, SYSARG_READ, SYSARG_AS_PTR(pt, 4, app_pc),
                                    sizeof(uint), NULL, DRSYS_TYPE_INT, NULL))
                return;
        }
        break;
    }
    case SYS_process_vm_readv:
    case SYS_process_vm_writev:
        handle_pre_process_vm_readv_writev(drcontext, ii);
        break;
#ifdef X64
#ifdef X86
    case SYS_arch_prctl: {
        int code = (int) pt->sysarg[0];
        unsigned long addr = (unsigned long) pt->sysarg[1];
        if (code == ARCH_GET_FS || code == ARCH_SET_FS) {
            if (!report_memarg_type(ii, 1, SYSARG_WRITE, (app_pc) addr,
                                    sizeof(addr), NULL, DRSYS_TYPE_UNSIGNED_INT, NULL))
                return;
        } /* else, inlined */
        break;
    }
#endif
#endif
    }
    /* If you add any handling here: need to check ii->abort first */
}

void
os_handle_post_syscall(void *drcontext, cls_syscall_t *pt, sysarg_iter_info_t *ii)
{
    /* each handler checks result for success */
    switch (ii->arg->sysnum.number) {
#ifdef X86
    case SYS__sysctl: {
        struct __sysctl_args *args = SYSARG_AS_PTR(pt, 0, struct __sysctl_args *);
        size_t len;
        if (dr_syscall_get_result(drcontext) == 0 && args != NULL) {
            /* xref PR 408540: here we wait until post so we can use the
             * actual written size.  There could be races but they're
             * app errors, which we should report, right?
             */
            if (args->oldval != NULL && safe_read(args->oldlenp, sizeof(len), &len)) {
                if (!report_memarg_type(ii, 0, SYSARG_WRITE, (app_pc) args->oldval, len,
                                        NULL, DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    }
#endif
    case SYS_ioctl:
        handle_post_ioctl(drcontext, pt, ii);
        break;
#ifdef X64
    case SYS_semctl:
        handle_semctl(drcontext, pt, ii, 0);
        break;
    case SYS_msgctl:
        handle_msgctl(drcontext, pt, ii, 0, 1, 2);
        break;
    case SYS_shmctl:
        handle_shmctl(drcontext, pt, ii, 0, 1, 2);
        break;
#else
    case SYS_socketcall:
        handle_post_socketcall(drcontext, pt, ii);
        break;
    case SYS_ipc:
        handle_post_ipc(drcontext, pt, ii);
        break;
#endif
    case SYS_poll: {
        struct pollfd *fds = SYSARG_AS_PTR(pt, 0, struct pollfd *);
        nfds_t nfds = (nfds_t) pt->sysarg[1];
        if (fds != NULL) {
            int i;
            for (i = 0; i < nfds; i++) {
                /* First fields are inputs, last is output */
                if (!report_memarg_type(ii, 0, SYSARG_WRITE, (app_pc) &fds[i].revents,
                                        sizeof(fds[i].revents), NULL,
                                        DRSYS_TYPE_STRUCT, NULL))
                    return;
            }
        }
        break;
    }
    case SYS_prctl: {
        handle_post_prctl(drcontext, pt, ii);
        break;
    }
    case SYS_process_vm_readv: {
        handle_post_process_vm_readv(drcontext, ii);
        break;
    }
    case SYS_process_vm_writev:
        handle_post_process_vm_writev(drcontext, ii);
        break;
#ifdef X64
#ifdef X86
    case SYS_arch_prctl: {
        int code = (int) pt->sysarg[0];
        unsigned long addr = (unsigned long) pt->sysarg[1];
        if (code == ARCH_GET_FS || code == ARCH_SET_FS) {
            if (!report_memarg_type(ii, 1, SYSARG_WRITE, (app_pc) addr,
                                    sizeof(addr), NULL, DRSYS_TYPE_UNSIGNED_INT, NULL))
                return;
        } /* else, inlined */
        break;
    }
#endif
#endif
    }
    /* If you add any handling here: need to check ii->abort first */
}

/***************************************************************************
 * SHADOW PER-ARG-TYPE HANDLING
 */

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

static bool
handle_sockaddr_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                       app_pc start, uint size)
{
    cls_syscall_t *pt = (cls_syscall_t *)
        drmgr_get_cls_field(ii->arg->drcontext, cls_idx_drsys);
    check_sockaddr(pt, ii, start, (socklen_t) size, arg_info->param,
                   arg_info->flags, NULL);
    return true; /* check_sockaddr did all checking */
}

static bool
handle_msghdr_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                       app_pc start, uint size)
{
    cls_syscall_t *pt = (cls_syscall_t *)
        drmgr_get_cls_field(ii->arg->drcontext, cls_idx_drsys);
    check_msghdr(ii->arg->drcontext, pt, ii, start, (socklen_t) size,
                 arg_info->param, arg_info->flags);
    return true; /* check_msghdr checks whole struct */
}

static bool
handle_msgbuf_access(sysarg_iter_info_t *ii, const sysinfo_arg_t *arg_info,
                     app_pc start, uint size)
{
    cls_syscall_t *pt = (cls_syscall_t *)
        drmgr_get_cls_field(ii->arg->drcontext, cls_idx_drsys);
    check_msgbuf(ii->arg->drcontext, pt, ii, start, size,
                 arg_info->param, arg_info->flags);
    return true; /* check_msgbuf checks whole struct */
}

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
    case SYSARG_TYPE_SOCKADDR:
        return handle_sockaddr_access(ii, arg_info, start, size);
    case SYSARG_TYPE_MSGHDR:
        return handle_msghdr_access(ii, arg_info, start, size);
    case SYSARG_TYPE_MSGBUF:
        return handle_msgbuf_access(ii, arg_info, start, size);
    case DRSYS_TYPE_CSTRARRAY:
        return handle_strarray_access(ii, arg_info, start, size);
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

bool
os_syscall_succeeded(drsys_sysnum_t sysnum, syscall_info_t *info, cls_syscall_t *pt)
{
    ptr_int_t res = (ptr_int_t) pt->mc.IF_X86_ELSE(xax,r0);
    if (sysnum.number == SYS_mmap || IF_NOT_X64(sysnum.number == SYS_mmap2 ||)
        sysnum.number == SYS_mremap)
        return (res >= 0 || res < -PAGE_SIZE);
    else
        return (res >= 0);
}

bool
os_syscall_succeeded_custom(drsys_sysnum_t sysnum, syscall_info_t *info,
                            cls_syscall_t *pt)
{
    return false;
}
