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

#ifndef _UTILS_H_
#define _UTILS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WINDOWS
/* DRi#1424: avoid pulling in features from recent versions to keep compatibility */
# ifndef _WIN32_WINNT
#  ifdef X64
#   define _WIN32_WINNT _WIN32_WINNT_WIN2003
#  else
#   define _WIN32_WINNT _WIN32_WINNT_WINXP
#  endif
# endif
#endif

#include "hashtable.h"
#include "dr_config.h"  /* for DR_MAX_OPTIONS_LENGTH */
#include "dr_tools.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drsyscall.h"

#include <limits.h>

#ifdef WINDOWS
# define IF_WINDOWS(x) x
# define IF_WINDOWS_(x) x,
# define _IF_WINDOWS(x) , x
# define IF_WINDOWS_ELSE(x,y) x
# define IF_UNIX(x)
# define IF_UNIX_ELSE(x,y) y
# define IF_LINUX(x)
# define IF_LINUX_ELSE(x,y) y
# define IF_UNIX_(x)
#else
# define IF_WINDOWS(x)
# define IF_WINDOWS_(x)
# define _IF_WINDOWS(x)
# define IF_WINDOWS_ELSE(x,y) y
# define IF_UNIX(x) x
# define IF_UNIX_ELSE(x,y) x
# define IF_UNIX_(x) x,
# ifdef LINUX
#  define IF_LINUX(x) x
#  define IF_LINUX_ELSE(x,y) x
#  define IF_LINUX_(x) x,
# else
#  define IF_LINUX(x)
#  define IF_LINUX_ELSE(x,y) y
#  define IF_LINUX_(x)
# endif
#endif

#ifdef MACOS
# define IF_MACOS(x) x
# define IF_MACOS_ELSE(x,y) x
# define IF_MACOS_(x) x,
#else
# define IF_MACOS(x)
# define IF_MACOS_ELSE(x,y) y
# define IF_MACOS_(x)
#endif

#ifdef ANDROID
# define IF_ANDROID(x) x
# define IF_ANDROID_ELSE(x,y) x
#else
# define IF_ANDROID(x)
# define IF_ANDROID_ELSE(x,y) y
#endif

#ifndef IF_X64
# ifdef X64
#  define IF_X64(x) x
# else
#  define IF_X64(x)
# endif
#endif

#ifndef IF_NOT_X64
# ifdef X64
#  define IF_NOT_X64(x)
# else
#  define IF_NOT_X64(x) x
# endif
#endif

#ifndef IF_NOT_X64_
# ifdef X64
#  define IF_NOT_X64_(x)
# else
#  define IF_NOT_X64_(x) x,
# endif
#endif

#ifdef X86
# ifndef IF_X86
#  define IF_X86(x) x
#  define IF_X86_ELSE(x, y) x
# endif
# ifdef X64
#  ifndef IF_X86_32
#   define IF_X86_32(x)
#  endif
#  define IF_X86_32_(x)
# else
#  ifndef IF_X86_32
#   define IF_X86_32(x) x
#  endif
#  define IF_X86_32_(x) x,
# endif
#else
# ifndef IF_X86
#  define IF_X86(x)
#  define IF_X86_ELSE(x, y) y
# endif
# define IF_X86_32_(x)
#endif

#ifndef IF_ARM
# ifdef ARM
#  define IF_ARM(x) x
#  define IF_ARM_ELSE(x, y) x
# else
#  define IF_ARM(x)
#  define IF_ARM_ELSE(x, y) y
# endif
#endif

#ifdef DEBUG
# define IF_DEBUG(x) x
# define IF_DEBUG_ELSE(x,y) x
# define _IF_DEBUG(x) , x
#else
# define IF_DEBUG(x)
# define IF_DEBUG_ELSE(x,y) y
# define _IF_DEBUG(x)
#endif

#ifdef VMX86_SERVER
# define IF_VMX86(x) x
#else
# define IF_VMX86(x)
#endif

#ifdef TOOL_DR_MEMORY
# define IF_DRMEM(x) x
# define IF_DRHEAP(x)
# define IF_DRMEM_ELSE(x, y) x
#else
# define IF_DRMEM(x)
# define IF_DRHEAP(x) x
# define IF_DRMEM_ELSE(x, y) y
#endif

#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)
#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_MOD(addr, size, alignment) \
    ((((ptr_uint_t)addr)+(size)-1) & ((alignment)-1))
#define CROSSES_ALIGNMENT(addr, size, alignment) \
    (ALIGN_MOD(addr, size, alignment) < (size)-1)

#ifndef TESTANY
# define TEST(mask, var) (((mask) & (var)) != 0)
# define TESTANY TEST
# define TESTALL(mask, var) (((mask) & (var)) == (mask))
# define TESTONE(mask, var) test_one_bit_set((mask) & (var))
#endif

#define IS_POWER_OF_2(x) ((x) != 0 && ((x) & ((x)-1)) == 0)

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

#ifdef WINDOWS
# define inline __inline
# define INLINE_FORCED __forceinline
/* Use special C99 operator _Pragma to generate a pragma from a macro */
# if _MSC_VER <= 1200 /* XXX: __pragma may work w/ vc6: then don't need #if */
#  define ACTUAL_PRAGMA(p) _Pragma ( #p )
# else
#   define ACTUAL_PRAGMA(p) __pragma ( p )
# endif
# define DO_NOT_OPTIMIZE ACTUAL_PRAGMA( optimize("g", off) )
# define END_DO_NOT_OPTIMIZE ACTUAL_PRAGMA( optimize("g", on) )
/* START_PACKED_STRUCTURE can't be used after typedef (b/c MSVC compiler
 * has bug where it accepts #pragma but not __pragma there) and thus the
 * struct have to be typedef-ed in two steps.
 * see example struct _packed_frame_t at common/callstack.c
 */
# define START_PACKED_STRUCTURE ACTUAL_PRAGMA( pack(push,1) )
# define END_PACKED_STRUCTURE ACTUAL_PRAGMA( pack(pop) )
#else /* UNIX */
# define inline __inline__
# define INLINE_FORCED inline
# if 0 /* only available in gcc 4.4+ so not using: XXX: add HAVE_OPTIMIZE_ATTRIBUTE */
#   define DO_NOT_OPTIMIZE __attribute__((optimize("O0")))
# else
#   define DO_NOT_OPTIMIZE /* nothing */
# endif
# define END_DO_NOT_OPTIMIZE /* nothing */
# define START_PACKED_STRUCTURE /* nothing */
# define END_PACKED_STRUCTURE __attribute__ ((__packed__))
#endif
#define INLINE_ONCE inline

#ifdef UNIX
# define DIRSEP '/'
# define ALT_DIRSEP '/'
# define NL "\n"
#else
/* We can pick which we want for usability: the auto-launch of notepad
 * converts to backslash regardless (i#1123).  Backslash works in all
 * Windows apps, while forward works in most and in cygwin (though
 * still not first-class there as it has a drive letter) but not in
 * File Open dialogs or older notepad Save As (or as executable path
 * when launching).  We could consider making this a runtime option
 * or auto-picked if in cygwin but for now we're going with backslash.
 */
# define DIRSEP '\\'
# define ALT_DIRSEP '/'
# define NL "\r\n"
#endif

/* Names meant for use in text_matches_pattern() where wildcards are supported */
#define DYNAMORIO_LIBNAME IF_WINDOWS_ELSE("dynamorio.dll", \
    IF_MACOS_ELSE("libdynamorio.*dylib*", "libdynamorio.so*"))
/* CLIENT_LIBNAME is assumed to be set as a preprocessor define */
#define CLIENT_LIBNAME_BASE STRINGIFY(CLIENT_LIBNAME)
#define DRMEMORY_LIBNAME \
    IF_WINDOWS_ELSE(CLIENT_LIBNAME_BASE ".dll", "lib" CLIENT_LIBNAME_BASE \
                    IF_MACOS_ELSE("*.dylib*", ".so*"))

#define MAX_INSTR_SIZE 17

#define sscanf  DO_NOT_USE_sscanf_directly_see_issue_344

#define INVALID_THREAD_ID 0

#define ASSERT_NOT_IMPLEMENTED() ASSERT(false, "Not Yet Implemented")
#define ASSERT_NOT_REACHED() ASSERT(false, "Shouldn't be reached")

#define CHECK_TRUNCATE_RANGE_uint(val)   ((val) >= 0 && (val) <= UINT_MAX)
#define CHECK_TRUNCATE_RANGE_int(val)    ((val) <= INT_MAX && ((int64)(val)) >= INT_MIN)
#ifdef WINDOWS
# define CHECK_TRUNCATE_RANGE_ULONG(val) CHECK_TRUNCATE_RANGE_uint(val)
#endif
#define CHECK_TRUNCATE_RANGE_ushort(val)   ((val) >= 0 && (val) <= USHRT_MAX)
#define ASSERT_TRUNCATE_TYPE(var, type) ASSERT(sizeof(var) == sizeof(type), \
                                               "mismatch "#var" and "#type)
/* check no precision lose on typecast from val to var.
 * var = (type) val; should always be preceded by a call to ASSERT_TRUNCATE
 */
#define ASSERT_TRUNCATE(var, type, val) do {        \
    ASSERT_TRUNCATE_TYPE(var, type);                \
    ASSERT(CHECK_TRUNCATE_RANGE_##type(val),        \
           "truncating value to ("#type")"#var);    \
} while (0)

#ifdef X86
# define MC_RET_REG(mc) (mc)->xax
# define MC_FP_REG(mc)  (mc)->xbp
# define MC_SP_REG(mc)  (mc)->xsp
#elif defined(ARM)
# define MC_RET_REG(mc) (mc)->r0
# define MC_FP_REG(mc)  (mc)->r11
# define MC_SP_REG(mc)  (mc)->sp
#endif

#define DR_REG_PTR_RETURN IF_X86_ELSE(DR_REG_XAX, DR_REG_R0)

/* globals that affect NOTIFY* and *LOG* macros */
extern bool op_print_stderr;
extern int op_verbose_level;
extern bool op_pause_at_assert;
extern bool op_pause_via_loop;
extern bool op_ignore_asserts;
extern uint op_prefix_style;
extern file_t f_global;
extern int reported_disk_error;
#ifdef TOOL_DR_MEMORY
extern file_t f_results;
#endif
extern bool op_use_symcache;

/* Workarounds for i#261 where DR can't write to cmd console.
 *
 * 1) Use private kernel32!WriteFile to write to the
 * console.  This could be unsafe to do in the middle of app operations but our
 * notifications are usually at startup and shutdown only.  Writing to the
 * console involves sending a message to csrss which we hope we can treat like a
 * system call wrt atomicity so long as the message crafting via private
 * kernel32.dll is safe.
 *
 * 2) For drag-and-drop where the cmd window will disappear, we use
 * msgboxes (rather than "wait for keypress") for error messages,
 * which are good to highlight anyway (and thus we do not try to
 * distinguish existing cmd from new cmd: but we do not use msgbox for
 * rxvt or xterm or other shells since we assume that's a savvy user).
 * We only pop up msgbox for NOTIFY_ERROR() to avoid too many
 * msgboxes, esp for things like options list (for that we'd want to
 * bundle into one buffer anyway: but that exceeds dr_messagebox's
 * buffer size).
 */
#ifdef WIN32
# define IN_CMD (dr_using_console())
# define USE_MSGBOX (op_print_stderr && IN_CMD)
#else
/* For non-USE_DRSYMS Windows we just don't support cmd: unfortunately this
 * includes cygwin in cmd.  With PR 561181 we'll get cygwin into USE_DRSYMS.
 */
# define USE_MSGBOX (false)
#endif
/* dr_fprintf() now prints to the console after dr_enable_console_printing() */
#define PRINT_CONSOLE(...) dr_fprintf(STDERR, __VA_ARGS__)

/* for notifying user
 * XXX: should add messagebox, controlled by option
 */
enum {
    PREFIX_STYLE_DEAULT,
    PREFIX_STYLE_NONE,
    PREFIX_STYLE_BLANK,
};

#ifdef TOOL_DR_MEMORY
# define PREFIX_DEFAULT_MAIN_THREAD "~~Dr.M~~ "
#else
# define PREFIX_DEFAULT_MAIN_THREAD "~~Dr.H~~ "
#endif
#define PREFIX_BLANK                "         "

void
print_prefix_to_buffer(char *buf, size_t bufsz, size_t *sofar);

void
print_prefix_to_console(void);

#define NOTIFY(...) do { \
    ELOG(0, __VA_ARGS__); \
    if (op_print_stderr) { \
        print_prefix_to_console(); \
        PRINT_CONSOLE(__VA_ARGS__); \
    }                                         \
} while (0)
#define NOTIFY_ERROR(...) do { \
    NOTIFY(__VA_ARGS__); \
    IF_DRMEM(ELOGF(0, f_results, __VA_ARGS__)); \
    IF_WINDOWS({if (USE_MSGBOX) dr_messagebox(__VA_ARGS__);})   \
} while (0)
#define NOTIFY_COND(cond, f, ...) do { \
    ELOGF(0, f, __VA_ARGS__); \
    if ((cond) && op_print_stderr) { \
        print_prefix_to_console(); \
        PRINT_CONSOLE(__VA_ARGS__); \
    }                                         \
} while (0)
#define NOTIFY_VERBOSE(level, ...) do { \
    ELOG(0, __VA_ARGS__); \
    if (op_verbose_level >= level && op_print_stderr) { \
        print_prefix_to_console(); \
        PRINT_CONSOLE(__VA_ARGS__); \
    }                                         \
} while (0)
#define NOTIFY_NO_PREFIX(...) do { \
    ELOG(0, __VA_ARGS__); \
    if (op_print_stderr) { \
        PRINT_CONSOLE(__VA_ARGS__); \
    }                                         \
} while (0)

/***************************************************************************
 * for warning/error reporting to logfile
 */

/* Per-thread data shared across callbacks and all modules */
typedef struct _tls_util_t {
    file_t f;  /* logfile */
} tls_util_t;

extern int tls_idx_util;

#define LOGFILE(pt) ((pt) == NULL ? f_global : (pt)->f)
#define PT_GET(dc) \
    (((dc) == NULL) ? NULL : (tls_util_t *)drmgr_get_tls_field(dc, tls_idx_util))
#define LOGFILE_GET(dc) LOGFILE(PT_GET(dc))
#define PT_LOOKUP() PT_GET(dr_get_current_drcontext())
#define LOGFILE_LOOKUP() LOGFILE(PT_LOOKUP())
/* we require a ,fmt arg but C99 requires one+ argument which we just strip */
#define ELOGF(level, f, ...) do {   \
    if (op_verbose_level >= (level) && (f) != INVALID_FILE) {\
        if (dr_fprintf(f, __VA_ARGS__) < 0) \
            REPORT_DISK_ERROR(); \
    } \
} while (0)
#define ELOGPT(level, pt, ...) \
    ELOGF(level, LOGFILE(pt), __VA_ARGS__)
#define ELOG(level, ...) do {   \
    if (op_verbose_level >= (level)) { /* avoid unnec PT_GET */ \
        ELOGPT(level, PT_LOOKUP(), __VA_ARGS__); \
    } \
} while (0)
/* DR's fprintf has a size limit */
#define ELOG_LARGE_F(level, f, s) do {   \
    if (op_verbose_level >= (level) && (f) != INVALID_FILE) \
        dr_write_file(f, s, strlen(s)); \
} while (0)
#define ELOG_LARGE_PT(level, pt, s) \
    ELOG_LARGE_F(level, LOGFILE(pt), s)
#define ELOG_LARGE(level, s) do {   \
    if (op_verbose_level >= (level)) { /* avoid unnec PT_GET */ \
        ELOG_LARGE_PT(level, PT_LOOKUP(), s); \
    } \
} while (0)

#define WARN(...) ELOGF(0, f_global, __VA_ARGS__)

#define REPORT_DISK_ERROR() do { \
    /* this implements a DO_ONCE with multiple instantiations */ \
    int report_count = dr_atomic_add32_return_sum(&reported_disk_error, 1); \
    if (report_count == 1) { \
        if (op_print_stderr) { \
            print_prefix_to_console(); \
            PRINT_CONSOLE("WARNING: Unable to write to the disk.  "\
                "Ensure that you have enough space and permissions.\n"); \
        } \
        if (USE_MSGBOX) { \
            IF_WINDOWS(dr_messagebox("Unable to write to the disk.  "\
                "Ensure that you have enough space and permissions.\n")); \
        } \
    } \
} while(0)

/* PR 427074: asserts should go to the log and not just stderr.
 * Since we don't have a vsnprintf() (i#168) we can't make this an
 * expression (w/o duplicating NOTIFY as an expression) but we only
 * use it as a statement anyway.
 */
#ifdef DEBUG
# define ASSERT(x, msg) do { \
    if (!(x)) { \
        NOTIFY_ERROR("ASSERT FAILURE (thread " TIDFMT "): %s:%d: %s (%s)" NL,  \
                     (dr_get_current_drcontext() == NULL ? 0 :      \
                      dr_get_thread_id(dr_get_current_drcontext())),\
                     __FILE__,  __LINE__, #x, msg); \
        if (!op_ignore_asserts) drmemory_abort(); \
    } \
} while (0)
# define ASSERT_NOT_TESTED(msg) do { \
    static int assert_not_tested_printed = 0; \
    if (!assert_not_tested_printed) { \
        /* This singleton-like implementation has a data race. \
         * However, in the worst case this will result in printing the message \
         * twice which isn't a big deal \
         */ \
        assert_not_tested_printed = 1; \
        NOTIFY("Not tested - %s @%s:%d" NL, msg, __FILE__, __LINE__); \
    } \
} while (0)
#else
# define ASSERT(x, msg) /* nothing */
# define ASSERT_NOT_TESTED(msg) /* nothing */
#endif

#ifdef BUILD_UNIT_TESTS
/* Don't use ASSERT as it will crash trying to get TLS fields.
 * We should move these to a shared header once we had tests elsewhere.
 */
# define EXPECT(x) do { \
    if (!(x)) { \
        dr_fprintf(STDERR, "CHECK FAILED: %s:%d: %s", __FILE__,  __LINE__, #x); \
        drmemory_abort(); \
    } \
} while (0)
#endif

#ifdef DEBUG
# define LOGF ELOGF
# define LOGPT ELOGPT
# define LOG ELOG
# define LOG_LARGE_F ELOG_LARGE_F
# define LOG_LARGE_PT ELOG_LARGE_PT
# define LOG_LARGE ELOG_LARGE
# define DOLOG(level, stmt)  do {   \
    if (op_verbose_level >= (level)) \
        stmt                        \
} while (0)
# define DODEBUG(stmt)  do {   \
    stmt                       \
} while (0)
#else
# define LOGF(level, pt, fmt, ...) /* nothing */
# define LOGPT(level, pt, fmt, ...) /* nothing */
# define LOG(level, fmt, ...) /* nothing */
# define LOG_LARGE_F(level, pt, fmt, ...) /* nothing */
# define LOG_LARGE_PT(level, pt, fmt, ...) /* nothing */
# define LOG_LARGE(level, fmt, ...) /* nothing */
# define DOLOG(level, stmt) /* nothing */
# define DODEBUG(stmt) /* nothing */
#endif

/* For printing to a buffer.
 * Usage: have a size_t variable "sofar" that counts the chars used so far.
 * We take in "len" to avoid repeated locals, which some compilers won't
 * combine (grrr: xref some PR).
 * If we had i#168 dr_vsnprintf this wouldn't have to be a macro.
 */
#define BUFPRINT_NO_ASSERT(buf, bufsz, sofar, len, ...) do { \
    len = dr_snprintf((buf)+(sofar), (bufsz)-(sofar), __VA_ARGS__); \
    sofar += (len == -1 ? ((bufsz)-(sofar)) : (len < 0 ? 0 : len)); \
    /* be paranoid: though usually many calls in a row and could delay until end */ \
    (buf)[(bufsz)-1] = '\0';                                 \
} while (0)

#define BUFPRINT(buf, bufsz, sofar, len, ...) do { \
    BUFPRINT_NO_ASSERT(buf, bufsz, sofar, len, __VA_ARGS__); \
    ASSERT((bufsz) > (sofar), "buffer size miscalculation"); \
} while (0)

/* Buffered file write marcos, to improve performance.  See PR 551841. */
#define FLUSH_BUFFER(fd, buf, sofar) do { \
    if ((sofar) > 0) \
        dr_write_file(fd, buf, sofar); \
    (sofar) = 0; \
} while (0)

#define BUFFERED_WRITE(fd, buf, bufsz, sofar, len, ...) do { \
    int old_sofar = sofar; \
    BUFPRINT_NO_ASSERT(buf, bufsz, sofar, len, __VA_ARGS__); \
 \
    /* If the buffer overflows, \
     * flush the buffer to the file and reprint to the buffer. \
     * We must treat the buffer length being hit exactly as an overflow \
     * b/c the NULL already clobbered our data. \
     */ \
    if ((sofar) >= (bufsz)) { \
        ASSERT((bufsz) > old_sofar, "unexpected overflow"); \
        (buf)[old_sofar] = '\0';  /* not needed, strictly speaking; be safe */ \
        (sofar) = old_sofar; \
        FLUSH_BUFFER(fd, buf, sofar); /* pass sofar to get it zeroed */ \
        BUFPRINT_NO_ASSERT(buf, bufsz, sofar, len, __VA_ARGS__); \
        ASSERT((bufsz) > (sofar), "single write can't overflow buffer"); \
    } \
} while (0)

#ifdef STATISTICS
# define STATS_INC(stat) ATOMIC_INC32(stat)
# define STATS_DEC(stat) ATOMIC_DEC32(stat)
# define STATS_ADD(stat, val) ATOMIC_ADD32(stat, val)
# define STATS_PEAK(stat) do {               \
    uint stats_peak_local_val_ = stat;       \
    if (stats_peak_local_val_ > peak_##stat) \
        peak_##stat = stats_peak_local_val_; \
} while (0)
# define DOSTATS(x) x
# define _IF_STATS(x) , x
#else
# define STATS_INC(stat) /* nothing */
# define STATS_DEC(stat) /* nothing */
# define STATS_ADD(stat, val) /* nothing */
# define STATS_PEAK(stat) /* nothing */
# define DOSTATS(x) /* nothing */
# define _IF_STATS(x) /* nothing */
#endif

#define PRE instrlist_meta_preinsert
#define PREXL8 instrlist_preinsert
#define POST instrlist_meta_postinsert
#define POSTXL8 instrlist_postinsert

#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#define DWORD2BYTE(v, n) (((v) & (0xff << 8*(n))) >> 8*(n))

#ifdef X64
# define POINTER_MAX ULLONG_MAX
# define XSP_SZ 8
#else
# define POINTER_MAX UINT_MAX
# define XSP_SZ 4
#endif

/* C standard has pointer overflow as undefined so cast to unsigned (i#302) */
#define POINTER_OVERFLOW_ON_ADD(ptr, add) \
    (((ptr_uint_t)(ptr)) + (add) < ((ptr_uint_t)(ptr)))
#define POINTER_UNDERFLOW_ON_SUB(ptr, sub) \
    (((ptr_uint_t)(ptr)) - (sub) > ((ptr_uint_t)(ptr)))

#ifdef UNIX
# ifdef X64
#  define ASM_XAX "rax"
#  define ASM_XDX "rdx"
#  define ASM_XSP "rsp"
#  define ASM_SEG "gs"
#  define ASM_SYSARG1 "rdi"
#  define ASM_SYSARG2 "rsi"
#  define ASM_SYSARG3 "rdx"
#  define ASM_SYSARG4 "r10"
#  define ASM_SYSARG5 "r8"
#  define ASM_SYSARG6 "r9"
/* int 0x80 returns different value from syscall in x64.
 * For example, brk(0) returns 0xfffffffffffffff2 using int 0x80
 */
#  define ASM_SYSCALL "syscall"
# else
#  define ASM_XAX "eax"
#  define ASM_XDX "edx"
#  define ASM_XSP "esp"
#  define ASM_SEG "fs"
#  define ASM_SYSARG1 "ebx"
#  define ASM_SYSARG2 "ecx"
#  define ASM_SYSARG3 "edx"
#  define ASM_SYSARG4 "esi"
#  define ASM_SYSARG5 "edi"
#  define ASM_SYSARG6 "ebp"
#  define ASM_SYSCALL "int $0x80"
# endif
#endif

#ifdef UNIX
# ifdef X86
#  define ATOMIC_INC32(x) __asm__ __volatile__("lock incl %0" : "=m" (x) : : "memory")
#  define ATOMIC_DEC32(x) __asm__ __volatile__("lock decl %0" : "=m" (x) : : "memory")
#  define ATOMIC_ADD32(x, val) \
    __asm__ __volatile__("lock addl %1, %0" : "=m" (x) : "r" (val) : "memory")

static inline int
atomic_add32_return_sum(volatile int *x, int val)
{
    int cur;
    __asm__ __volatile__("lock xaddl %1, %0" : "=m" (*x), "=r" (cur)
                         : "1" (val) : "memory");
    return (cur + val);
}
# elif defined(ARM)
/* XXX: should DR export these for us? */
#  define ATOMIC_INC32(x)                                   \
     __asm__ __volatile__(                                  \
       "1: ldrex r2, %0         \n\t"                       \
       "   add   r2, r2, #1     \n\t"                       \
       "   strex r3, r2, %0     \n\t"                       \
       "   cmp   r3, #0         \n\t"                       \
       "   bne   1b             \n\t"                       \
       "   cmp   r2, #0" /* for possible SET_FLAG use */    \
       : "=Q" (x) /* no offset for ARM mode */              \
       : : "cc", "memory", "r2", "r3")
#  define ATOMIC_DEC32(x)                                   \
     __asm__ __volatile__(                                  \
       "1: ldrex r2, %0         \n\t"                       \
       "   sub   r2, r2, #1     \n\t"                       \
       "   strex r3, r2, %0     \n\t"                       \
       "   cmp   r3, #0         \n\t"                       \
       "   bne   1b             \n\t"                       \
       "   cmp   r2, #0" /* for possible SET_FLAG use */    \
       : "=Q" (x) /* no offset for ARM mode */              \
       : : "cc", "memory", "r2", "r3")
#  define ATOMIC_ADD32(x, val)                              \
     __asm__ __volatile__(                                  \
       "1: ldrex r2, %0         \n\t"                       \
       "   add   r2, r2, %1     \n\t"                       \
       "   strex r3, r2, %0     \n\t"                       \
       "   cmp   r3, #0         \n\t"                       \
       "   bne   1b             \n\t"                       \
       "   cmp   r2, #0" /* for possible SET_FLAG use */    \
       : "=Q" (x) /* no offset for ARM mode */              \
       : "r"  (val)                                         \
       : "cc", "memory", "r2", "r3")
#  define ATOMIC_ADD_EXCHANGE32(x, val, result)             \
     __asm__ __volatile__(                                  \
       "1: ldrex r2, %0         \n\t"                       \
       "   add   r2, r2, %2     \n\t"                       \
       "   strex r3, r2, %0     \n\t"                       \
       "   cmp   r3, #0         \n\t"                       \
       "   bne   1b             \n\t"                       \
       "   sub   r2, r2, %2     \n\t"                       \
       "   str   r2, %1"                                    \
       : "=Q" (*x), "=m" (result)                           \
       : "r"  (val)                                         \
       : "cc", "memory", "r2", "r3")

static inline int
atomic_add32_return_sum(volatile int *x, int val)
{
    int temp;
    ATOMIC_ADD_EXCHANGE32(x, val, temp);
    return (temp + val);
}
# endif
#else
# define ATOMIC_INC32(x) _InterlockedIncrement((volatile LONG *)&(x))
# define ATOMIC_DEC32(x) _InterlockedDecrement((volatile LONG *)&(x))
# define ATOMIC_ADD32(x, val) _InterlockedExchangeAdd((volatile LONG *)&(x), val)

static inline int
atomic_add32_return_sum(volatile int *x, int val)
{
    return (ATOMIC_ADD32(*x, val) + val);
}
#endif

/* racy: should be used only for diagnostics */
#define DO_ONCE(stmt) {     \
    static int do_once = 0; \
    if (!do_once) {         \
        do_once = 1;        \
        stmt;               \
    }                       \
}

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* IS_ASCII excludes null char */
#define IS_ASCII(c) ((byte)(c) < 0x80 && (byte)(c) != 0)
#define IS_WCHAR_AT(ptr) \
    (IS_ASCII(*((byte*)(ptr))) && *(((byte*)(ptr))+1) == 0)
#define IS_WCHARx2_AT(ptr) \
    (IS_WCHAR_AT(ptr) && IS_WCHAR_AT(((wchar_t*)(ptr))+1))

/***************************************************************************
 * UTILITY ROUTINES
 */

#ifdef WINDOWS
# define strcasecmp _stricmp
#endif

#ifdef memset
# undef memset
#endif

void *
memset(void *dst, int val, size_t size);

void
wait_for_user(const char *message);

void
drmemory_abort(void);

bool
safe_read(void *base, size_t size, void *out_buf);

/* if returns false, calls instr_free() on inst first */
bool
safe_decode(void *drcontext, app_pc pc, instr_t *inst, app_pc *next_pc /*OPTIONAL OUT*/);

#ifdef STATISTICS
extern uint symbol_lookups;
extern uint symbol_searches;
extern uint symbol_lookup_cache_hits;
extern uint symbol_search_cache_hits;
extern uint symbol_address_lookups;
#endif
bool
lookup_has_fast_search(const module_data_t *mod);

app_pc
lookup_symbol(const module_data_t *mod, const char *symname);

app_pc
lookup_internal_symbol(const module_data_t *mod, const char *symname);

/* Iterates over symbols matching modname!sym_pattern until
 * callback returns false.
 * N.B.: if you add a call to this routine, or modify an existing call,
 * bump SYMCACHE_VERSION and add symcache checks.
 */
bool
lookup_all_symbols(const module_data_t *mod, const char *sym_pattern, bool full,
                   drsym_enumerate_ex_cb callback, void *data);

bool
module_has_debug_info(const module_data_t *mod);

#ifdef DEBUG
void
print_mcontext(file_t f, dr_mcontext_t *mc);
#endif

#ifdef WINDOWS
# ifdef DEBUG
/* check that peb isolation is consistently applied (xref i#324) */
bool
using_private_peb(void);
# endif

HANDLE
get_private_heap_handle(void);
#endif /* WINDOWS */

void
hashtable_delete_with_stats(hashtable_t *table, const char *name);

#ifdef STATISTICS
void
hashtable_cluster_stats(hashtable_t *table, const char *name);
#endif

bool
unsigned_multiply_will_overflow(size_t m, size_t n);

void
crash_process(void);

static inline bool
test_one_bit_set(uint x)
{
    return (x > 0) && (x & (x-1)) == 0;
}

static inline generic_func_t
cast_to_func(void *p)
{
#ifdef WINDOWS
#  pragma warning(push)
#  pragma warning(disable : 4055)
#endif
    return (generic_func_t) p;
#ifdef WINDOWS
#  pragma warning(pop)
#endif
}

/***************************************************************************
 * WINDOWS SYSCALLS
 */

#ifdef WINDOWS
# include "windefs.h"

/* i#1908: we support loading numbers from a file */
# define SYSNUM_FILE IF_X64_ELSE("syscalls_x64.txt", "syscalls_x86.txt")
# define SYSNUM_FILE_WOW64 "syscalls_wow64.txt"

# ifndef STATUS_INVALID_KERNEL_INFO_VERSION
#  define STATUS_INVALID_KERNEL_INFO_VERSION 0xc000a004
# endif

TEB *
get_TEB(void);

TEB *
get_TEB_from_handle(HANDLE h);

thread_id_t
get_tid_from_handle(HANDLE h);

TEB *
get_TEB_from_tid(thread_id_t tid);

void
set_app_error_code(void *drcontext, uint val);

PEB *
get_app_PEB(void);

HANDLE
get_process_heap_handle(void);

bool
is_current_process(HANDLE h);

bool
is_wow64_process(void);

const wchar_t *
get_app_commandline(void);

/* returns just the primary number */
int
sysnum_from_name(const char *name);

bool
get_sysnum(const char *name, drsys_sysnum_t *var, bool ok_to_fail);

bool
running_on_Win7_or_later(void);

bool
running_on_Win7SP1_or_later(void);

bool
running_on_Vista_or_later(void);

dr_os_version_t
get_windows_version(void);

void
get_windows_version_string(char *buf DR_PARAM_OUT, size_t bufsz);

app_pc
get_highest_user_address(void);

/* set *base to preferred value, or NULL for none */
bool
virtual_alloc(void **base, size_t size, uint memtype, uint prot);

bool
virtual_free(void *base);

bool
module_imports_from_msvc(const module_data_t *mod);

#endif /* WINDOWS */

reg_t
syscall_get_param(void *drcontext, uint num);

/***************************************************************************
 * HEAP WITH STATS
 */

/* PR 423757: heap accounting */
typedef enum {
    HEAPSTAT_SHADOW,
    HEAPSTAT_PERBB,
#ifdef TOOL_DR_HEAPSTAT
    HEAPSTAT_SNAPSHOT,
    HEAPSTAT_STALENESS,
#endif
    HEAPSTAT_CALLSTACK,
    HEAPSTAT_HASHTABLE,
    HEAPSTAT_GENCODE,
    HEAPSTAT_RBTREE,
    HEAPSTAT_REPORT,
    HEAPSTAT_WRAP,
    HEAPSTAT_MISC,
    /* when you add here, add to heapstat_names in utils.c */
    HEAPSTAT_NUMTYPES,
} heapstat_t;

/* wrappers around dr_ versions to track heap accounting stats */
void *
global_alloc(size_t size, heapstat_t type);

void
global_free(void *p, size_t size, heapstat_t type);

void *
thread_alloc(void *drcontext, size_t size, heapstat_t type);

void
thread_free(void *drcontext, void *p, size_t size, heapstat_t type);

void *
nonheap_alloc(size_t size, uint prot, heapstat_t type);

void
nonheap_free(void *p, size_t size, heapstat_t type);

void
heap_dump_stats(file_t f);

#define dr_global_alloc DO_NOT_USE_use_global_alloc
#define dr_global_free  DO_NOT_USE_use_global_free
#define dr_thread_alloc DO_NOT_USE_use_thread_alloc
#define dr_thread_free  DO_NOT_USE_use_thread_free
#define dr_nonheap_alloc DO_NOT_USE_use_nonheap_alloc
#define dr_nonheap_free  DO_NOT_USE_use_nonheap_free

char *
drmem_strdup(const char *src, heapstat_t type);

/* note: Guarantees that even if src overflows max, the allocated buffer will be
 * large enough for max characters plus the null-terminator.
 */
char *
drmem_strndup(const char *src, size_t max, heapstat_t type);

/***************************************************************************
 * STRINGS
 */

#define END_MARKER "\terror end" NL

/* DR_MAX_OPTIONS_LENGTH is the maximum client options string length that DR
 * will give us.  by making each individual option buffer this long, we won't
 * have truncation issues.
 */
#define MAX_OPTION_LEN DR_MAX_OPTIONS_LENGTH

#if !defined(MACOS) && !defined(ANDROID) && !defined(NOLINK_STRCASESTR)
char *
strcasestr(const char *text, const char *pattern);
#endif

char *
strnchr(const char *str, int find, size_t max);

#define FILESYS_CASELESS IF_WINDOWS_ELSE(true, false)

bool
text_matches_pattern(const char *text, const char *pattern, bool ignore_case);

/* patterns is a null-separated, double-null-terminated list of strings */
bool
text_matches_any_pattern(const char *text, const char *patterns, bool ignore_case);

/* patterns is a null-separated, double-null-terminated list of strings */
const char *
text_contains_any_string(const char *text, const char *patterns, bool ignore_case,
                         const char **matched);

/* For parsing an mmapped file into lines: returns the start of the next line.
 * Optionally returns the start of this line after skipping whitespace (if skip_ws)
 * in "sol" and the end of the line (prior to any whitespace, if skip_ws) in "eol".
 */
const char *
find_next_line(const char *start, const char *eof, const char **sol DR_PARAM_OUT,
               const char **eol DR_PARAM_OUT, bool skip_ws);

/***************************************************************************
 * REGISTER CONVERSION UTILITIES
 */

reg_id_t
reg_ptrsz_to_32(reg_id_t reg);

reg_id_t
reg_ptrsz_to_16(reg_id_t reg);

reg_id_t
reg_ptrsz_to_8(reg_id_t reg);

#ifdef X86
reg_id_t
reg_ptrsz_to_8h(reg_id_t reg);
#endif

reg_id_t
reg_to_size(reg_id_t reg, opnd_size_t size);

/***************************************************************************
 * APP INSTRS
 */

instr_t *
instr_get_prev_app_instr(instr_t *instr);

instr_t *
instr_get_next_app_instr(instr_t *instr);

instr_t *
instrlist_first_app_instr(instrlist_t *ilist);

instr_t *
instrlist_last_app_instr(instrlist_t *ilist);

/***************************************************************************
 * HASHTABLE
 *
 * hashtable was moved and generalized so we need to initialize it
 */

/* must be called before drmgr or drwrap */
void
utils_early_init(void);

void
utils_init(void);

void
utils_exit(void);

void
utils_thread_init(void *drcontext);

void
utils_thread_exit(void *drcontext);

void
utils_thread_set_file(void *drcontext, file_t f);

#ifdef __cplusplus
}
#endif

#endif /* _UTILS_H_ */
