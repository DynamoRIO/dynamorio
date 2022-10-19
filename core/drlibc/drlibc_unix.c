/* *******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
 * os.c - Linux specific routines
 */

#include <sys/mman.h>
#include <fcntl.h>

/* Avoid pulling in deps from instr_inline.h included from globals.h */
#define DR_NO_FAST_IR
#include "../globals.h"

#ifdef LINUX
#    include "include/syscall.h" /* our own local copy */
#else
#    include <sys/syscall.h>
#endif

#include <errno.h>
/* Just like in os.c we avoid the errno variable. */
#if !defined(STANDALONE_UNIT_TEST) && !defined(MACOS)
#    undef errno
#endif

#include "drlibc_unix.h"

#ifdef LINUX
#    include "module_private.h" /* for ELF_AUXV_TYPE and AT_PAGESZ */
#endif

process_id_t
get_process_id()
{
    return dynamorio_syscall(SYS_getpid, 0);
}

/* translate permission string to platform independent protection bits */
uint
permstr_to_memprot(const char *const perm)
{
    uint mem_prot = 0;
    if (perm == NULL || *perm == '\0')
        return mem_prot;
    if (perm[2] == 'x')
        mem_prot |= MEMPROT_EXEC;
    if (perm[1] == 'w')
        mem_prot |= MEMPROT_WRITE;
    if (perm[0] == 'r')
        mem_prot |= MEMPROT_READ;
    return mem_prot;
}

/* translate platform independent protection bits to native flags */
uint
memprot_to_osprot(uint prot)
{
    uint mmap_prot = 0;
    if (TEST(MEMPROT_EXEC, prot))
        mmap_prot |= PROT_EXEC;
    if (TEST(MEMPROT_READ, prot))
        mmap_prot |= PROT_READ;
    if (TEST(MEMPROT_WRITE, prot))
        mmap_prot |= PROT_WRITE;
    return mmap_prot;
}

bool
mmap_syscall_succeeded(byte *retval)
{
    ptr_int_t result = (ptr_int_t)retval;
    /* libc interprets up to -PAGE_SIZE as an error, and you never know if
     * some weird errno will be used by say vmkernel (xref PR 365331)
     */
    bool fail = (result < 0 && result >= -PAGE_SIZE);
    ASSERT_CURIOSITY(!fail ||
                     IF_VMX86(result == -ENOENT ||) IF_VMX86(result == -ENOSPC ||)
                             result == -EBADF ||
                     result == -EACCES || result == -EINVAL || result == -ETXTBSY ||
                     result == -EAGAIN || result == -ENOMEM || result == -ENODEV ||
                     result == -EFAULT || result == -EPERM || result == -EEXIST);
    return !fail;
}

/* N.B.: offs should be in pages for 32-bit Linux */
byte *
mmap_syscall(byte *addr, size_t len, ulong prot, ulong flags, ulong fd, ulong offs)
{
#if defined(MACOS) && !defined(X64)
    return (byte *)(ptr_int_t)dynamorio_syscall(
        SYS_mmap, 7, addr, len, prot, flags, fd,
        /* represent 64-bit arg as 2 32-bit args */
        offs, 0);
#else
    return (byte *)(ptr_int_t)dynamorio_syscall(
        IF_MACOS_ELSE(SYS_mmap, IF_X64_ELSE(SYS_mmap, SYS_mmap2)), 6, addr, len, prot,
        flags, fd, offs);
#endif
}

long
munmap_syscall(byte *addr, size_t len)
{
    return dynamorio_syscall(SYS_munmap, 2, addr, len);
}

/* FIXME - not available in 2.0 or earlier kernels, not really an issue since no one
 * should be running anything that old. */
static int
llseek_syscall(int fd, int64 offset, int origin, int64 *result)
{
#if defined(X64) || defined(MACOS)
#    ifndef X64
    /* 2 slots for 64-bit arg */
    *result = dynamorio_syscall(SYS_lseek, 4, fd, (uint)(offset & 0xFFFFFFFF),
                                (uint)((offset >> 32) & 0xFFFFFFFF), origin);
#    else
    *result = dynamorio_syscall(SYS_lseek, 3, fd, offset, origin);
#    endif
    return ((*result > 0) ? 0 : (int)*result);
#else
    return dynamorio_syscall(SYS__llseek, 5, fd, (uint)((offset >> 32) & 0xFFFFFFFF),
                             (uint)(offset & 0xFFFFFFFF), result, origin);
#endif
}

ptr_int_t
dr_stat_syscall(const char *fname, struct stat64 *st)
{
#ifdef SYSNUM_STAT
    return dynamorio_syscall(SYSNUM_STAT, 2, fname, st);
#else
    return dynamorio_syscall(SYS_fstatat, 4, AT_FDCWD, fname, st, 0);
#endif
}

bool
os_file_exists(const char *fname, bool is_dir)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dr_stat_syscall(fname, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    return (!is_dir || S_ISDIR(st.st_mode));
}

/* Returns true if two paths point to the same file.  Follows symlinks.
 */
bool
os_files_same(const char *path1, const char *path2)
{
    struct stat64 st1, st2;
    ptr_int_t res = dr_stat_syscall(path1, &st1);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    res = dr_stat_syscall(path2, &st2);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    return st1.st_ino == st2.st_ino;
}

bool
os_get_file_size(const char *file, uint64 *size)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dr_stat_syscall(file, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    ASSERT(size != NULL);
    *size = st.st_size;
    return true;
}

bool
os_get_file_size_by_handle(file_t fd, uint64 *size)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dynamorio_syscall(SYSNUM_FSTAT, 2, fd, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    ASSERT(size != NULL);
    *size = st.st_size;
    return true;
}

/* created directory will be owned by effective uid,
 * Note a symbolic link will never be followed.
 */
bool
os_create_dir(const char *fname, create_directory_flags_t create_dir_flags)
{
    bool require_new = TEST(CREATE_DIR_REQUIRE_NEW, create_dir_flags);
#ifdef SYS_mkdir
    int rc = dynamorio_syscall(SYS_mkdir, 2, fname, S_IRWXU | S_IRWXG);
#else
    int rc = dynamorio_syscall(SYS_mkdirat, 3, AT_FDCWD, fname, S_IRWXU | S_IRWXG);
#endif
    ASSERT(create_dir_flags == CREATE_DIR_REQUIRE_NEW ||
           create_dir_flags == CREATE_DIR_ALLOW_EXISTING);
    return (rc == 0 || (!require_new && rc == -EEXIST));
}

bool
os_delete_dir(const char *name)
{
#ifdef SYS_rmdir
    return (dynamorio_syscall(SYS_rmdir, 1, name) == 0);
#else
    return (dynamorio_syscall(SYS_unlinkat, 3, AT_FDCWD, name, AT_REMOVEDIR) == 0);
#endif
}

int
open_syscall(const char *file, int flags, int mode)
{
    ASSERT(file != NULL);
#ifdef SYS_open
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_open), 3, file, flags, mode);
#else
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_openat), 4, AT_FDCWD, file, flags,
                             mode);
#endif
}

int
close_syscall(int fd)
{
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_close), 1, fd);
}

int
dup_syscall(int fd)
{
    return dynamorio_syscall(SYS_dup, 1, fd);
}

ssize_t
read_syscall(int fd, void *buf, size_t nbytes)
{
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_read), 3, fd, buf, nbytes);
}

ssize_t
write_syscall(int fd, const void *buf, size_t nbytes)
{
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_write), 3, fd, buf, nbytes);
}

/* not easily accessible in header files */
#ifndef O_LARGEFILE
#    ifdef X64
/* not needed */
#        define O_LARGEFILE 0
#    else
#        define O_LARGEFILE 0100000
#    endif
#endif

/* we assume that opening for writing wants to create file.
 * we also assume that nobody calling this is creating a persistent
 * file: for that, use os_open_protected() to avoid leaking on exec
 * and to separate from the app's files.
 */
file_t
os_open(const char *fname, int os_open_flags)
{
    int res;
    int flags = 0;
    if (TEST(OS_OPEN_ALLOW_LARGE, os_open_flags))
        flags |= O_LARGEFILE;
    if (TEST(OS_OPEN_WRITE_ONLY, os_open_flags))
        res = open_syscall(fname, flags | O_WRONLY, 0);
    else if (!TEST(OS_OPEN_WRITE, os_open_flags))
        res = open_syscall(fname, flags | O_RDONLY, 0);
    else {
        res = open_syscall(
            fname,
            flags | O_RDWR | O_CREAT |
                (TEST(OS_OPEN_APPEND, os_open_flags)
                     ?
                     /* Currently we only support either appending
                      * or truncating, just like Windows and the client
                      * interface.  If we end up w/ a use case that wants
                      * neither it could open append and then seek; if we do
                      * add OS_TRUNCATE or sthg we'll need to add it to
                      * any current writers who don't set OS_OPEN_REQUIRE_NEW.
                      */
                     O_APPEND
                     : O_TRUNC) |
                (TEST(OS_OPEN_REQUIRE_NEW, os_open_flags) ? O_EXCL : 0),
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }
    if (res < 0)
        return INVALID_FILE;

    return res;
}

file_t
os_open_directory(const char *fname, int os_open_flags)
{
    /* no special handling */
    return os_open(fname, os_open_flags);
}

void
os_close(file_t f)
{
    close_syscall(f);
}

/* no os_write() so drinject can use drdecode's copy */

ssize_t
os_read(file_t f, void *buf, size_t count)
{
    return read_syscall(f, buf, count);
}

void
os_flush(file_t f)
{
    /* we're not using FILE*, so there is no buffering */
}

/* seek current file position to offset bytes from origin, return true if successful */
bool
os_seek(file_t f, int64 offset, int origin)
{
    int64 result;
    int ret = 0;

    ret = llseek_syscall(f, offset, origin, &result);

    return (ret == 0);
}

/* return the current file position, -1 on failure */
int64
os_tell(file_t f)
{
    int64 result = -1;
    int ret = 0;

    ret = llseek_syscall(f, 0, SEEK_CUR, &result);

    if (ret != 0)
        return -1;

    return result;
}

bool
os_delete_file(const char *name)
{
#ifdef SYS_unlink
    return (dynamorio_syscall(SYS_unlink, 1, name) == 0);
#else
    return (dynamorio_syscall(SYS_unlinkat, 3, AT_FDCWD, name, 0) == 0);
#endif
}

bool
os_rename_file(const char *orig_name, const char *new_name, bool replace)
{
    ptr_int_t res;
    if (!replace) {
        /* SYS_rename replaces so we must test beforehand => could have race */
        /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
        struct stat64 st;
        ptr_int_t res = dr_stat_syscall(new_name, &st);
        if (res == 0)
            return false;
        else if (res != -ENOENT) {
            LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s stat failed: " PIFX "\n", __func__, res);
            return false;
        }
    }
#ifdef SYS_rename
    res = dynamorio_syscall(SYS_rename, 2, orig_name, new_name);
#else
    res = dynamorio_syscall(SYS_renameat, 4, AT_FDCWD, orig_name, AT_FDCWD, new_name);
#endif
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s \"%s\" to \"%s\" failed: " PIFX "\n",
            __func__, orig_name, new_name, res);
    }
    return (res == 0);
}

bool
os_delete_mapped_file(const char *filename)
{
    return os_delete_file(filename);
}

/* We mark as weak so the core can override with its more complex version. */
WEAK byte *
os_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
            map_flags_t map_flags)
{
    int flags;
    byte *map;
#if defined(LINUX) && !defined(X64)
    uint pg_offs;
    ASSERT_TRUNCATE(pg_offs, uint, offs / PAGE_SIZE);
    pg_offs = (uint)(offs / PAGE_SIZE);
#endif
#ifdef VMX86_SERVER
    flags = MAP_PRIVATE; /* MAP_SHARED not supported yet */
#else
    flags = TEST(MAP_FILE_COPY_ON_WRITE, map_flags) ? MAP_PRIVATE : MAP_SHARED;
#endif
    /* Allows memory request instead of mapping a file,
     * so we can request memory from a particular address with fixed argument */
    if (f == -1)
        flags |= MAP_ANONYMOUS;
    if (TEST(MAP_FILE_FIXED, map_flags))
        flags |= MAP_FIXED;
    map = mmap_syscall(addr, *size, memprot_to_osprot(prot), flags, f,
                       /* x86 Linux mmap uses offset in pages */
                       IF_LINUX_ELSE(IF_X64_ELSE(offs, pg_offs), offs));
    if (!mmap_syscall_succeeded(map)) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, map);
        map = NULL;
    }
    return map;
}

WEAK bool
os_unmap_file(byte *map, size_t size)
{
    long res = munmap_syscall(map, size);
    return (res == 0);
}

const reg_id_t syscall_regparms[MAX_SYSCALL_ARGS] = {
#ifdef X86
#    ifdef X64
    DR_REG_RDI, DR_REG_RSI,
    DR_REG_RDX, DR_REG_R10, /* RCX goes here in normal x64 calling contention. */
    DR_REG_R8,  DR_REG_R9
#    else
    DR_REG_EBX, DR_REG_ECX, DR_REG_EDX, DR_REG_ESI, DR_REG_EDI, DR_REG_EBP
#    endif /* 64/32-bit */
#elif defined(AARCHXX)
    DR_REG_R0, DR_REG_R1, DR_REG_R2, DR_REG_R3, DR_REG_R4, DR_REG_R5,
#elif defined(RISCV64)
    DR_REG_A0, DR_REG_A1, DR_REG_A2, DR_REG_A3, DR_REG_A4, DR_REG_A5,
#endif /* X86/ARM */
};

/****************************************************************************
 * Page size discovery and query
 */

/* This variable is only used by os_set_page_size and os_page_size, but those
 * functions may be called before libdynamorio.so has been relocated. So check
 * the disassembly of those functions: there should be no relocations.
 */
static size_t page_size = 0;

static size_t auxv_minsigstksz = 0;

/* Return true if size is a multiple of the page size.
 * XXX: This function may be called when DynamoRIO is in a fragile state, or not
 * yet relocated, so keep this self-contained and do not use global variables or
 * logging.
 */
static bool
os_try_page_size(size_t size)
{
    byte *addr =
        mmap_syscall(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((ptr_uint_t)addr >= (ptr_uint_t)-4096) /* mmap failed: should not happen */
        return false;
    if (munmap_syscall(addr + size, size) == 0) {
        /* munmap of top half succeeded: munmap bottom half and return true */
        munmap_syscall(addr, size);
        return true;
    }
    /* munmap of top half failed: munmap whole region and return false */
    munmap_syscall(addr, size * 2);
    return false;
}

/* Directly determine the granularity of memory allocation using mmap and munmap.
 * This is used as a last resort if the page size is required before it has been
 * discovered in any other way, such as from AT_PAGESZ.
 * XXX: This function may be called when DynamoRIO is in a fragile state, or not
 * yet relocated, so keep this self-contained and do not use global variables or
 * logging.
 */
static size_t
os_find_page_size(void)
{
    size_t size = 4096;
    if (os_try_page_size(size)) {
        /* Try smaller sizes. */
        for (size /= 2; size > 0; size /= 2) {
            if (!os_try_page_size(size))
                return size * 2;
        }
    } else {
        /* Try larger sizes. */
        for (size *= 2; size * 2 > 0; size *= 2) {
            if (os_try_page_size(size))
                return size;
        }
    }
    /* Something went wrong... */
    return 4096;
}

static void
os_set_page_size(size_t size)
{
    page_size = size; /* atomic write */
}

size_t
os_page_size(void)
{
    size_t size = page_size; /* atomic read */
    if (size == 0) {
        /* XXX: On Mac OSX we should use sysctl_query on hw.pagesize. */
        size = os_find_page_size();
        os_set_page_size(size);
    }
    return size;
}

/* With SIGSTKSZ now in sysconf and an auxv var AT_MINSIGSTKSZ we avoid
 * using the defines and try to lookup the min value in os_page_size_init().
 */
size_t
os_minsigstksz(void)
{
#ifdef AARCH64
#    define MINSIGSTKSZ_DEFAULT 5120
#else
#    define MINSIGSTKSZ_DEFAULT 2048
#endif
    if (auxv_minsigstksz == 0)
        return MINSIGSTKSZ_DEFAULT;
    return auxv_minsigstksz;
}

void
os_page_size_init(const char **env, bool env_followed_by_auxv)
{
#ifdef LINUX
    /* On Linux we get the page size from the auxiliary vector, which is what
     * the C library typically does for implementing sysconf(_SC_PAGESIZE).
     * However, for STATIC_LIBRARY, our_environ is not guaranteed to point
     * at the stack as we're so late, so we do not try to read off the end of it
     * (i#2122).
     */
    if (!env_followed_by_auxv)
        return;
    size_t size = page_size; /* atomic read */
    if (size == 0) {
        ELF_AUXV_TYPE *auxv;
        /* Skip environment. */
        while (*env != 0)
            ++env;
        /* Look for AT_PAGESZ and other values in the auxiliary vector. */
        for (auxv = (ELF_AUXV_TYPE *)(env + 1); auxv->a_type != AT_NULL; auxv++) {
            if (auxv->a_type == AT_PAGESZ) {
                os_set_page_size(auxv->a_un.a_val);
                break;
            }
#    ifdef AT_MINSIGSTKSZ
            else if (auxv->a_type == AT_MINSIGSTKSZ) {
                auxv_minsigstksz = auxv->a_un.a_val;
                break;
            }
#    endif
        }
    }
#endif /* LINUX */
}
