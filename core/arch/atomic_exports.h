/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* file "atomic_exports.h" -- arch-specific synchronization declarations
 *
 * Note that ATOMIC_*_READ/WRITE() macros follow acquire-release semantics.
 */

#ifndef _ATOMIC_EXPORTS_H_
#define _ATOMIC_EXPORTS_H_ 1

/* We use this to ensure that linking and unlinking is atomic with respect
 * to a thread in the cache, this is needed for our current flushing
 * implementation.  Note that linking and unlinking are only atomic with
 * respect to a thread in the cache not with respect to a thread in dynamorio
 * (which can see linking flags etc.)
 */
/* see bug 524 for additional notes, reproduced here :
 * there is no way to do a locked mov, have to use an xchg or similar which is
 * a larger performance penalty (not really an issue), note that xchg implies
 * lock, so no need for the lock prefix below
 *
 * further Intel's documentation is a little weird on the issue of
 * cross-modifying code (see IA32 volume 3 7-2 through 7-7), "Locked
 * instructions should not be used to insure that data written can be fetched
 * as instructions" and "Locked operations are atomic with respect to all other
 * memory operations and all externally visible events.  Only instruction fetch
 * and page table access can pass locked instructions", (pass?) however it does
 * note that the current versions of P6 family, pentium 4, xeon, pentium and
 * 486 allow data written by locked instructions to be fetched as instructions.
 * In the cross-modifying code section, however, it gives a (horrible for us)
 * algorithm to ensure cross-modifying code is compliant with current and
 * future versions of IA-32 then says that "the use of this option is not
 * required for programs intended to run on the 486, but is recommended to
 * insure compatibility with pentium 4, xeon, P6 family and pentium
 * processors", so my take home is that it works now, but don't have any
 * expectations for the future - FIXME - */
/* Ref case 3628, case 4397, empirically this only works for code where the
 * entire offset being written is within a cache line, so we can't use a locked
 * instruction to ensure atomicity */
#define PAD_JMPS_ALIGNMENT                         \
    (INTERNAL_OPTION(pad_jmps_set_alignment) != 0  \
         ? INTERNAL_OPTION(pad_jmps_set_alignment) \
         : proc_get_cache_line_size())
#ifdef DEBUG
#    define CHECK_JMP_TARGET_ALIGNMENT(target, size, hot_patch)                     \
        do {                                                                        \
            if (hot_patch && CROSSES_ALIGNMENT(target, size, PAD_JMPS_ALIGNMENT)) { \
                STATS_INC(unaligned_patches);                                       \
                ASSERT(!DYNAMO_OPTION(pad_jmps));                                   \
            }                                                                       \
        } while (0)
#else
#    define CHECK_JMP_TARGET_ALIGNMENT(target, size, hot_patch)
#endif

#ifdef WINDOWS
/* note that the microsoft compiler will not enregister variables across asm
 * blocks that touch those registers, so don't need to worry about clobbering
 * eax and ebx */
#    define ATOMIC_1BYTE_READ(addr_src, addr_res)               \
        do {                                                    \
            *(BYTE *)(addr_res) = *(volatile BYTE *)(addr_src); \
        } while (0)
#    define ATOMIC_1BYTE_WRITE(target, value, hot_patch)                      \
        do {                                                                  \
            ASSERT(sizeof(*target) == 1);                                     \
            /* No alignment check necessary, hot_patch parameter provided for \
             * consistency.                                                   \
             */                                                               \
            _InterlockedExchange8((volatile CHAR *)(target), (CHAR)(value));  \
        } while (0)
#    define ATOMIC_4BYTE_WRITE(target, value, hot_patch)                    \
        do {                                                                \
            ASSERT(sizeof(value) == 4);                                     \
            /* test that we aren't crossing a cache line boundary */        \
            CHECK_JMP_TARGET_ALIGNMENT(target, 4, hot_patch);               \
            /* we use xchgl instead of mov for non-4-byte-aligned writes */ \
            _InterlockedExchange((volatile LONG *)(target), (LONG)(value)); \
        } while (0)
#    define ATOMIC_4BYTE_ALIGNED_WRITE(target, value, hot_patch) \
        do {                                                     \
            ASSERT(sizeof(value) == 4);                          \
            ASSERT(ALIGNED(target, 4));                          \
            *(volatile LONG *)(target) = (LONG)(value);          \
        } while (0)
#    define ATOMIC_4BYTE_ALIGNED_READ(addr_src, addr_res)       \
        do {                                                    \
            ASSERT(ALIGNED(addr_src, 4));                       \
            *(LONG *)(addr_res) = *(volatile LONG *)(addr_src); \
        } while (0)
#    ifdef X64
#        define ATOMIC_8BYTE_WRITE(target, value, hot_patch)                        \
            do {                                                                    \
                ASSERT(sizeof(value) == 8);                                         \
                /* Not currently used to write code */                              \
                ASSERT_CURIOSITY(!hot_patch);                                       \
                /* test that we aren't crossing a cache line boundary */            \
                CHECK_JMP_TARGET_ALIGNMENT(target, 8, hot_patch);                   \
                /* we use xchgl instead of mov for non-4-byte-aligned writes */     \
                _InterlockedExchange64((volatile __int64 *)target, (__int64)value); \
            } while (0)
#        define ATOMIC_8BYTE_ALIGNED_WRITE(target, value, hot_patch) \
            do {                                                     \
                ASSERT(sizeof(value) == 8);                          \
                ASSERT(ALIGNED(target, 8));                          \
                /* Not currently used to write code */               \
                ASSERT_CURIOSITY(!hot_patch);                        \
                *(volatile __int64 *)(target) = (__int64)(value);    \
            } while (0)
#        define ATOMIC_8BYTE_ALIGNED_READ(addr_src, addr_res)             \
            do {                                                          \
                ASSERT(ALIGNED(addr_src, 8));                             \
                *(__int64 *)(addr_res) = *(volatile __int64 *)(addr_src); \
            } while (0)
#    endif

/* We use intrinsics since they eliminated inline asm support for x64.
 * FIXME: these intrinsics all use xadd even when no return value is needed!
 *   We assume these aren't performance-critical enough to care.
 *   If we do change to not have return value, need to change static inlines below.
 * Even if these turn into callouts, they should be reentrant.
 */
#    define ATOMIC_INC_int(var) _InterlockedIncrement((volatile LONG *)&(var))
#    ifdef X64 /* 64-bit intrinsics only avail on x64 */
#        define ATOMIC_INC_int64(var) _InterlockedIncrement64((volatile __int64 *)&(var))
#    endif
#    define ATOMIC_INC(type, var) ATOMIC_INC_##type(var)
#    define ATOMIC_DEC_int(var) _InterlockedDecrement((volatile LONG *)&(var))
#    ifdef X64 /* 64-bit intrinsics only avail on x64 */
#        define ATOMIC_DEC_int64(var) _InterlockedDecrement64((volatile __int64 *)&(var))
#    endif
#    define ATOMIC_DEC(type, var) ATOMIC_DEC_##type(var)
/* Note that there is no x86/x64 _InterlockedAdd: only for IA64 */
#    define ATOMIC_ADD_int(var, value) \
        _InterlockedExchangeAdd((volatile LONG *)&(var), value)
#    ifdef X64 /* 64-bit intrinsics only avail on x64 */
#        define ATOMIC_ADD_int64(var, value) \
            _InterlockedExchangeAdd64((volatile __int64 *)&(var), value)
#    endif
#    define ATOMIC_COMPARE_EXCHANGE_int(var, compare, exchange) \
        _InterlockedCompareExchange((volatile LONG *)&(var), exchange, compare)
#    define ATOMIC_COMPARE_EXCHANGE_int64(var, compare, exchange) \
        _InterlockedCompareExchange64((volatile __int64 *)&(var), exchange, compare)
#    define ATOMIC_COMPARE_EXCHANGE ATOMIC_COMPARE_EXCHANGE_int
#    define ATOMIC_ADD(type, var, val) ATOMIC_ADD_##type(var, val)
#    ifdef X64
#        define ATOMIC_ADD_PTR(type, var, val) ATOMIC_ADD_int64(var, val)
#        define ATOMIC_COMPARE_EXCHANGE_PTR ATOMIC_COMPARE_EXCHANGE_int64
#    else
#        define ATOMIC_ADD_PTR(type, var, val) ATOMIC_ADD_int(var, val)
#        define ATOMIC_COMPARE_EXCHANGE_PTR ATOMIC_COMPARE_EXCHANGE_int
#    endif
#    define MEMORY_STORE_BARRIER()       /* not needed on x86 */
#    define SPINLOCK_PAUSE() _mm_pause() /* PAUSE = 0xf3 0x90 = repz nop */
#    define SERIALIZE_INSTRUCTIONS()     \
        do {                             \
            int cpuid_res_local[4];      \
            __cpuid(cpuid_res_local, 0); \
        }

/* returns true if result value is zero */
static inline bool
atomic_inc_and_test(volatile int *var)
{
    return (ATOMIC_INC(int, *(var)) == 0);
}
/* returns true if initial value was zero */
static inline bool
atomic_dec_and_test(volatile int *var)
{
    return (ATOMIC_DEC(int, *(var)) == -1);
}
/* returns true if result value is zero */
static inline bool
atomic_dec_becomes_zero(volatile int *var)
{
    return (ATOMIC_DEC(int, *(var)) == 0);
}
/* returns true if var was equal to compare */
static inline bool
atomic_compare_exchange_int(volatile int *var, int compare, int exchange)
{
    return (ATOMIC_COMPARE_EXCHANGE_int(*(var), compare, exchange) == (compare));
}
static inline bool
atomic_compare_exchange_int64(volatile int64 *var, int64 compare, int64 exchange)
{
    return ((ptr_int_t)ATOMIC_COMPARE_EXCHANGE_int64(*(var), compare, exchange) ==
            (compare));
}
/* atomically adds value to memory location var and returns the sum */
static inline int
atomic_add_exchange_int(volatile int *var, int value)
{
    return ((value) + ATOMIC_ADD(int, *(var), value));
}
#    ifdef X64 /* 64-bit intrinsics only avail on x64 */
static inline int64
atomic_add_exchange_int64(volatile int64 *var, int64 value)
{
    return ((value) + ATOMIC_ADD(int64, *(var), value));
}
#    endif
#    define atomic_add_exchange atomic_add_exchange_int

#else /* UNIX */
#    ifdef DR_HOST_X86
/* IA-32 vol 3 7.1.4: processor will internally suppress the bus lock
 * if target is within cache line.
 */
#        define ATOMIC_1BYTE_READ(addr_src, addr_res)               \
            do {                                                    \
                __asm__ __volatile__("movb %1, %%al; movb %%al, %0" \
                                     : "=m"(*(byte *)(addr_res))    \
                                     : "m"(*(byte *)(addr_src))     \
                                     : "al");                       \
            } while (0)
#        define ATOMIC_1BYTE_WRITE(target, value, hot_patch)                       \
            do {                                                                   \
                /* allow a constant to be passed in by supplying our own lvalue */ \
                char _myval = value;                                               \
                ASSERT(sizeof(*target) == 1);                                      \
                /* No alignment check necessary, hot_patch parameter provided for  \
                 * consistency.                                                    \
                 */                                                                \
                __asm__ __volatile__("xchgb %0, %1"                                \
                                     : "+m"(*(char *)(target)), "+r"(_myval)       \
                                     :);                                           \
            } while (0)
#        define ATOMIC_4BYTE_WRITE(target, value, hot_patch)                       \
            do {                                                                   \
                /* allow a constant to be passed in by supplying our own lvalue */ \
                int _myval = value;                                                \
                ASSERT(sizeof(value) == 4);                                        \
                /* test that we aren't crossing a cache line boundary */           \
                CHECK_JMP_TARGET_ALIGNMENT(target, 4, hot_patch);                  \
                /* we use xchgl instead of mov for non-4-byte-aligned writes */    \
                /* i#1805: both operands must be outputs to ensure proper compiler \
                 * behavior */                                                     \
                __asm__ __volatile__("xchgl %0, %1"                                \
                                     : "+m"(*(int *)(target)), "+r"(_myval)        \
                                     :);                                           \
            } while (0)
#        define ATOMIC_4BYTE_ALIGNED_WRITE(target, value, hot_patch)               \
            do {                                                                   \
                /* allow a constant to be passed in by supplying our own lvalue */ \
                int _myval = value;                                                \
                ASSERT(sizeof(value) == 4);                                        \
                ASSERT(ALIGNED(target, 4));                                        \
                __asm__ __volatile__("movl %1, %0"                                 \
                                     : "=m"(*(int *)(target))                      \
                                     : "r"(_myval));                               \
            } while (0)
#        define ATOMIC_4BYTE_ALIGNED_READ(addr_src, addr_res)         \
            do {                                                      \
                ASSERT(ALIGNED(addr_src, 4));                         \
                __asm__ __volatile__("movl %1, %%eax; movl %%eax, %0" \
                                     : "=m"(*(int *)(addr_res))       \
                                     : "m"(*(int *)(addr_src))        \
                                     : "eax");                        \
            } while (0)
#        ifdef X64
#            define ATOMIC_8BYTE_WRITE(target, value, hot_patch)                       \
                do {                                                                   \
                    /* allow a constant to be passed in by supplying our own lvalue */ \
                    int64 _myval = value;                                              \
                    ASSERT(sizeof(value) == 8);                                        \
                    /* Not currently used to write code */                             \
                    ASSERT_CURIOSITY(!hot_patch);                                      \
                    /* test that we aren't crossing a cache line boundary */           \
                    CHECK_JMP_TARGET_ALIGNMENT(target, 8, hot_patch);                  \
                    /* i#1805: both operands must be outputs to ensure proper compiler \
                     * behavior */                                                     \
                    __asm__ __volatile__("xchgq %0, %1"                                \
                                         : "+m"(*(int64 *)(target)), "+r"(_myval)      \
                                         :);                                           \
                } while (0)
#            define ATOMIC_8BYTE_ALIGNED_WRITE(target, value, hot_patch)               \
                do {                                                                   \
                    /* allow a constant to be passed in by supplying our own lvalue */ \
                    int64 _myval = value;                                              \
                    ASSERT(sizeof(value) == 8);                                        \
                    /* Not currently used to write code */                             \
                    ASSERT_CURIOSITY(!hot_patch);                                      \
                    ASSERT(ALIGNED(target, 8));                                        \
                    __asm__ __volatile__("movq %1, %0"                                 \
                                         : "=m"(*(int64 *)(target))                    \
                                         : "r"(_myval));                               \
                } while (0)
#            define ATOMIC_8BYTE_ALIGNED_READ(addr_src, addr_res)         \
                do {                                                      \
                    ASSERT(ALIGNED(addr_src, 8));                         \
                    __asm__ __volatile__("movq %1, %%rax; movq %%rax, %0" \
                                         : "=m"(*(int64 *)(addr_res))     \
                                         : "m"(*(int64 *)addr_src)        \
                                         : "rax");                        \
                } while (0)
#        endif /* X64 */
#        define ATOMIC_INC_suffix(suffix, var) \
            __asm__ __volatile__("lock inc" suffix " %0" : "=m"(var) : : "cc", "memory")
#        define ATOMIC_INC_int(var) ATOMIC_INC_suffix("l", var)
#        define ATOMIC_INC_int64(var) ATOMIC_INC_suffix("q", var)
#        define ATOMIC_INC(type, var) ATOMIC_INC_##type(var)
#        define ATOMIC_DEC_suffix(suffix, var) \
            __asm__ __volatile__("lock dec" suffix " %0" : "=m"(var) : : "cc", "memory")
#        define ATOMIC_DEC_int(var) ATOMIC_DEC_suffix("l", var)
#        define ATOMIC_DEC_int64(var) ATOMIC_DEC_suffix("q", var)
#        define ATOMIC_DEC(type, var) ATOMIC_DEC_##type(var)
/* with just "r" gcc will put $0 from PROBE_WRITE_PC into %eax
 * and then complain that "lock addq" can't take %eax!
 * so we use "ri":
 */
#        define ATOMIC_ADD_suffix(suffix, var, value)        \
            __asm__ __volatile__("lock add" suffix " %1, %0" \
                                 : "=m"(var)                 \
                                 : "ri"(value)               \
                                 : "cc", "memory")
#        define ATOMIC_ADD_int(var, val) ATOMIC_ADD_suffix("l", var, val)
#        define ATOMIC_ADD_int64(var, val) ATOMIC_ADD_suffix("q", var, val)
#        define ATOMIC_ADD(type, var, val) ATOMIC_ADD_##type(var, val)
/* Not safe for general use, just for atomic_add_exchange(), undefed below */
#        define ATOMIC_ADD_EXCHANGE_suffix(suffix, var, value, result) \
            __asm__ __volatile__("lock xadd" suffix " %1, %0"          \
                                 : "=m"(*var), "=r"(result)            \
                                 : "1"(value)                          \
                                 : "cc", "memory")
#        define ATOMIC_ADD_EXCHANGE_int(var, val, res) \
            ATOMIC_ADD_EXCHANGE_suffix("l", var, val, res)
#        define ATOMIC_ADD_EXCHANGE_int64(var, val, res) \
            ATOMIC_ADD_EXCHANGE_suffix("q", var, val, res)
#        define ATOMIC_COMPARE_EXCHANGE_suffix(suffix, var, compare, exchange) \
            __asm__ __volatile__("lock cmpxchg" suffix " %2,%0"                \
                                 : "=m"(var)                                   \
                                 : "a"(compare), "r"(exchange)                 \
                                 : "cc", "memory")
#        define ATOMIC_COMPARE_EXCHANGE_int(var, compare, exchange) \
            ATOMIC_COMPARE_EXCHANGE_suffix("l", var, compare, exchange)
#        define ATOMIC_COMPARE_EXCHANGE_int64(var, compare, exchange) \
            ATOMIC_COMPARE_EXCHANGE_suffix("q", var, compare, exchange)
#        define ATOMIC_EXCHANGE(var, newval, result)   \
            __asm __volatile("xchgl %0, %1"            \
                             : "=r"(result), "=m"(var) \
                             : "0"(newval), "m"(var))

#        define MEMORY_STORE_BARRIER() /* not needed on x86 */
#        define SPINLOCK_PAUSE() __asm__ __volatile__("pause")
#        define SERIALIZE_INSTRUCTIONS()                   \
            __asm__ __volatile__("xor %%eax, %%eax; cpuid" \
                                 :                         \
                                 :                         \
                                 : "eax", "ebx", "ecx", "edx");

#        define SET_FLAG(cc, flag) \
            __asm__ __volatile__("set" #cc " %0" : "=qm"(flag) : : "cc", "memory")
#        define SET_IF_NOT_ZERO(flag) SET_FLAG(nz, flag)
#        define SET_IF_NOT_LESS(flag) SET_FLAG(nl, flag)

#    elif defined(DR_HOST_AARCH64)

#        define ATOMIC_1BYTE_READ(addr_src, addr_res)               \
            do {                                                    \
                /* We use "load-acquire" to add a barrier. */       \
                __asm__ __volatile__("ldarb w0, [%0]\n\t"           \
                                     "strb w0, [%1]"                \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "w0", "memory");             \
            } while (0)
#        define ATOMIC_1BYTE_WRITE(target, value, hot_patch)   \
            do {                                               \
                ASSERT(sizeof(*target) == 1);                  \
                /* Not currently used to write code */         \
                ASSERT_CURIOSITY(!hot_patch);                  \
                __asm__ __volatile__("stlrb %w0, [%1]"         \
                                     :                         \
                                     : "r"(value), "r"(target) \
                                     : "memory");              \
            } while (0)
#        define ATOMIC_4BYTE_WRITE(target, value, hot_patch)                    \
            do {                                                                \
                ASSERT(sizeof(value) == 4);                                     \
                /* Not currently used to write code */                          \
                ASSERT_CURIOSITY(!hot_patch);                                   \
                /* We use "store-release" to add a barrier to make the store */ \
                /* visible promptly and not just atomic as in untorn which */   \
                /* alignment gives us by itself. */                             \
                ASSERT(ALIGNED(target, 4));                                     \
                __asm__ __volatile__("stlr %w0, [%1]"                           \
                                     :                                          \
                                     : "r"(value), "r"(target)                  \
                                     : "memory");                               \
            } while (0)
#        define ATOMIC_4BYTE_ALIGNED_WRITE ATOMIC_4BYTE_WRITE
#        define ATOMIC_4BYTE_ALIGNED_READ(addr_src, addr_res)       \
            do {                                                    \
                ASSERT(ALIGNED(addr_src, 4));                       \
                /* We use "load-acquire" to add a barrier. */       \
                __asm__ __volatile__("ldar w0, [%0]\n\t"            \
                                     "str w0, [%1]"                 \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "w0", "memory");             \
            } while (0)
#        define ATOMIC_8BYTE_WRITE(target, value, hot_patch)   \
            do {                                               \
                ASSERT(sizeof(value) == 8);                    \
                /* Not currently used to write code */         \
                ASSERT_CURIOSITY(!hot_patch);                  \
                ASSERT(ALIGNED(target, 8));                    \
                __asm__ __volatile__("stlr %0, [%1]"           \
                                     :                         \
                                     : "r"(value), "r"(target) \
                                     : "memory");              \
            } while (0)
#        define ATOMIC_8BYTE_ALIGNED_WRITE ATOMIC_8BYTE_WRITE
#        define ATOMIC_8BYTE_ALIGNED_READ(addr_src, addr_res)       \
            do {                                                    \
                ASSERT(ALIGNED(addr_src, 8));                       \
                /* We use "load-acquire" to add a barrier. */       \
                __asm__ __volatile__("ldar x0, [%0]\n\t"            \
                                     "str x0, [%1]"                 \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "x0", "memory");             \
            } while (0)

#        define DEF_ATOMIC_incdec(fname, type, r, op)                             \
            static inline void fname(volatile type *var)                          \
            {                                                                     \
                type tmp1;                                                        \
                int tmp2;                                                         \
                __asm__ __volatile__("1: ldaxr  %" r "0, [%x2]           \n\t"    \
                                     "  " op "   %" r "0, %" r "0, #1       \n\t" \
                                     "   stlxr  %w1, %" r "0, [%x2]      \n\t"    \
                                     "   cbnz  %w1, 1b                \n\t"       \
                                     : "=&r"(tmp1), "=&r"(tmp2)                   \
                                     : "r"(var));                                 \
            }
DEF_ATOMIC_incdec(ATOMIC_INC_int, int, "w", "add") DEF_ATOMIC_incdec(ATOMIC_INC_int64,
                                                                     int64, "x", "add")
    DEF_ATOMIC_incdec(ATOMIC_DEC_int, int, "w", "sub")
        DEF_ATOMIC_incdec(ATOMIC_DEC_int64, int64, "x", "sub")
#        undef DEF_ATOMIC_incdec

#        define ATOMIC_INC(type, var) ATOMIC_INC_##type(&var)
#        define ATOMIC_DEC(type, var) ATOMIC_DEC_##type(&var)

#        define DEF_ATOMIC_ADD(fname, type, r)                                    \
            static inline void fname(volatile type *var, type val)                \
            {                                                                     \
                type tmp1;                                                        \
                int tmp2;                                                         \
                __asm__ __volatile__("1: ldaxr  %" r "0, [%x2]           \n\t"    \
                                     "   add   %" r "0, %" r "0, %" r "3    \n\t" \
                                     "   stlxr  %w1, %" r "0, [%x2]      \n\t"    \
                                     "   cbnz  %w1, 1b                \n\t"       \
                                     : "=&r"(tmp1), "=&r"(tmp2)                   \
                                     : "r"(var), "r"(val));                       \
            }
            DEF_ATOMIC_ADD(ATOMIC_ADD_int, int, "w") DEF_ATOMIC_ADD(ATOMIC_ADD_int64,
                                                                    int64, "x")
#        undef DEF_ATOMIC_ADD

#        define ATOMIC_ADD(type, var, val) ATOMIC_ADD_##type(&var, val)

#        define DEF_ATOMIC_ADD_EXCHANGE(fname, type, reg)                             \
            static inline type fname(volatile type *var, type val)                    \
            {                                                                         \
                type ret;                                                             \
                int tmp;                                                              \
                __asm__ __volatile__("1: ldaxr  %" reg "1, [%x2]             \n\t"    \
                                     "   add   %" reg "1, %" reg "1, %" reg "3  \n\t" \
                                     "   stlxr  %w0, %" reg "1, [%x2]        \n\t"    \
                                     "   cbnz  %w0, 1b                    \n\t"       \
                                     : "=&r"(tmp), "=&r"(ret)                         \
                                     : "r"(var), "r"(val));                           \
                return ret;                                                           \
            }
                DEF_ATOMIC_ADD_EXCHANGE(atomic_add_exchange_int, int, "w")
                    DEF_ATOMIC_ADD_EXCHANGE(atomic_add_exchange_int64, int64, "x")
#        undef DEF_ATOMIC_ADD_EXCHANGE

#        define atomic_add_exchange atomic_add_exchange_int

#        define DEF_atomic_compare_exchange(fname, type, r)                           \
            static inline bool fname(volatile type *var, type compare, type exchange) \
            {                                                                         \
                type tmp1;                                                            \
                int tmp2;                                                             \
                bool ret;                                                             \
                __asm__ __volatile__("1: ldaxr  %" r "0, [%x3]           \n\t"        \
                                     "   cmp   %" r "0, %" r "4           \n\t"       \
                                     "   b.ne  2f                     \n\t"           \
                                     "   stlxr  %w1, %" r "5, [%x3]      \n\t"        \
                                     "   cbnz  %w1, 1b                \n\t"           \
                                     "   cmp   %" r "0, %" r "4           \n\t"       \
                                     "2: clrex                        \n\t"           \
                                     "   cset  %w2, eq                \n\t"           \
                                     : "=&r"(tmp1), "=&r"(tmp2), "=r"(ret)            \
                                     : "r"(var), "r"(compare), "r"(exchange));        \
                return ret;                                                           \
            }
                        DEF_atomic_compare_exchange(atomic_compare_exchange_int, int, "w")
                            DEF_atomic_compare_exchange(atomic_compare_exchange_int64,
                                                        int64, "x")
#        undef DEF_atomic_compare_exchange

                                static inline int atomic_exchange_int(volatile int *var,
                                                                      int newval)
{
    int tmp, ret;
    __asm__ __volatile__("1: ldaxr  %w0, [%x2]             \n\t"
                         "   stlxr  %w1, %w3, [%x2]        \n\t"
                         "   cbnz  %w1, 1b                \n\t"
                         : "=&r"(ret), "=&r"(tmp)
                         : "r"(var), "r"(newval));
    return ret;
}

#        define MEMORY_STORE_BARRIER() __asm__ __volatile__("dmb st")
/* i#4719: QEMU crashes on "wfi" so we use the superset "wfe".
 * XXX: Consider issuing "sev" on lock release?
 */
#        define SPINLOCK_PAUSE()             \
            do {                             \
                __asm__ __volatile__("wfe"); \
            } while (0) /* wait for interrupt */

static inline bool
atomic_inc_and_test(volatile int *var)
{
    return atomic_add_exchange_int(var, 1) == 0;
}

static inline bool
atomic_dec_and_test(volatile int *var)
{
    return atomic_add_exchange_int(var, -1) == -1;
}

static inline bool
atomic_dec_becomes_zero(volatile int *var)
{
    return atomic_add_exchange_int(var, -1) == 0;
}

#    elif defined(DR_HOST_ARM)

#        define ATOMIC_1BYTE_READ(addr_src, addr_res)                        \
            do {                                                             \
                __asm__ __volatile__("ldrb r0, [%0]; dmb ish; strb r0, [%1]" \
                                     :                                       \
                                     : "r"(addr_src), "r"(addr_res)          \
                                     : "r0", "memory");                      \
            } while (0)
#        define ATOMIC_1BYTE_WRITE(target, value, hot_patch)   \
            do {                                               \
                ASSERT(sizeof(*target) == 1);                  \
                __asm__ __volatile__("dmb ish; strb %0, [%1]"  \
                                     :                         \
                                     : "r"(value), "r"(target) \
                                     : "memory");              \
            } while (0)
#        define ATOMIC_4BYTE_WRITE(target, value, hot_patch)                     \
            do {                                                                 \
                ASSERT(sizeof(value) == 4);                                      \
                /* Load and store instructions are atomic on ARM if aligned. */  \
                /* FIXME i#1551: we need patch the whole instruction instead. */ \
                ASSERT(ALIGNED(target, 4));                                      \
                __asm__ __volatile__("dmb ish; str %0, [%1]"                     \
                                     :                                           \
                                     : "r"(value), "r"(target)                   \
                                     : "memory");                                \
            } while (0)
#        define ATOMIC_4BYTE_ALIGNED_WRITE ATOMIC_4BYTE_WRITE
#        define ATOMIC_4BYTE_ALIGNED_READ(addr_src, addr_res)              \
            do {                                                           \
                ASSERT(ALIGNED(addr_src, 4));                              \
                __asm__ __volatile__("ldr r0, [%0]; dmb ish; str r0, [%1]" \
                                     :                                     \
                                     : "r"(addr_src), "r"(addr_res)        \
                                     : "r0", "memory");                    \
            } while (0)

/* OP_swp is deprecated and OP_ldrex and OP_strex are introduced in
 * ARMv6 for ARM synchronization primitives
 */
/* The manual says "If SCTLR.A and SCTLR.U are both 0,
 * a non word-aligned memory address causes UNPREDICTABLE behavior.",
 * so we require alignment here.
 */
/* FIXME i#1551: should we allow infinite loops for those ATOMIC ops */
#        define ATOMIC_INC_suffix(suffix, var)                                     \
            __asm__ __volatile__("   dmb ish                        \n\t"          \
                                 "1: ldrex" suffix " r2, %0         \n\t"          \
                                 "   add" suffix " r2, r2, #1     \n\t"            \
                                 "   strex" suffix " r3, r2, %0     \n\t"          \
                                 "   cmp   r3, #0                   \n\t"          \
                                 "   bne   1b                       \n\t"          \
                                 "   cmp   r2, #0" /* for possible SET_FLAG use */ \
                                 : "=Q"(var)       /* no offset for ARM mode */    \
                                 :                                                 \
                                 : "cc", "memory", "r2", "r3");
#        define ATOMIC_INC_int(var) ATOMIC_INC_suffix("", var)
#        define ATOMIC_INC_int64(var) ATOMIC_INC_suffix("d", var)
#        define ATOMIC_INC(type, var) ATOMIC_INC_##type(var)
#        define ATOMIC_DEC_suffix(suffix, var)                                     \
            __asm__ __volatile__("   dmb ish                        \n\t"          \
                                 "1: ldrex" suffix " r2, %0         \n\t"          \
                                 "   sub" suffix " r2, r2, #1     \n\t"            \
                                 "   strex" suffix " r3, r2, %0     \n\t"          \
                                 "   cmp   r3, #0                   \n\t"          \
                                 "   bne   1b                       \n\t"          \
                                 "   cmp   r2, #0" /* for possible SET_FLAG use */ \
                                 : "=Q"(var)       /* no offset for ARM mode */    \
                                 :                                                 \
                                 : "cc", "memory", "r2", "r3");
#        define ATOMIC_DEC_int(var) ATOMIC_DEC_suffix("", var)
#        define ATOMIC_DEC_int64(var) ATOMIC_DEC_suffix("d", var)
#        define ATOMIC_DEC(type, var) ATOMIC_DEC_##type(var)
#        define ATOMIC_ADD_suffix(suffix, var, value)                              \
            __asm__ __volatile__("   dmb ish                        \n\t"          \
                                 "1: ldrex" suffix " r2, %0         \n\t"          \
                                 "   add" suffix " r2, r2, %1     \n\t"            \
                                 "   strex" suffix " r3, r2, %0     \n\t"          \
                                 "   cmp   r3, #0                   \n\t"          \
                                 "   bne   1b                       \n\t"          \
                                 "   cmp   r2, #0" /* for possible SET_FLAG use */ \
                                 : "=Q"(var)       /* no offset for ARM mode */    \
                                 : "r"(value)                                      \
                                 : "cc", "memory", "r2", "r3");
#        define ATOMIC_ADD_int(var, val) ATOMIC_ADD_suffix("", var, val)
#        define ATOMIC_ADD_int64(var, val) ATOMIC_ADD_suffix("q", var, val)
#        define ATOMIC_ADD(type, var, val) ATOMIC_ADD_##type(var, val)
/* Not safe for general use, just for atomic_add_exchange(), undefed below */
#        define ATOMIC_ADD_EXCHANGE_suffix(suffix, var, value, result)    \
            __asm__ __volatile__("   dmb ish                        \n\t" \
                                 "1: ldrex" suffix " r2, %0         \n\t" \
                                 "   add" suffix " r2, r2, %2     \n\t"   \
                                 "   strex" suffix " r3, r2, %0     \n\t" \
                                 "   cmp   r3, #0                   \n\t" \
                                 "   bne   1b                       \n\t" \
                                 "   sub" suffix " r2, r2, %2     \n\t"   \
                                 "   str" suffix " r2, %1"                \
                                 : "=Q"(*var), "=m"(result)               \
                                 : "r"(value)                             \
                                 : "cc", "memory", "r2", "r3");
#        define ATOMIC_ADD_EXCHANGE_int(var, val, res) \
            ATOMIC_ADD_EXCHANGE_suffix("", var, val, res)
#        define ATOMIC_ADD_EXCHANGE_int64(var, val, res) \
            ATOMIC_ADD_EXCHANGE_suffix("d", var, val, res)
#        define ATOMIC_COMPARE_EXCHANGE_suffix(suffix, var, compare, exchange)           \
            __asm__ __volatile__("   dmb ish                      \n\t"                  \
                                 "2: ldrex" suffix " r2, %0       \n\t"                  \
                                 "   cmp" suffix " r2, %1       \n\t"                    \
                                 "   bne    1f                    \n\t"                  \
                                 "   strex" suffix " r3, %2, %0   \n\t"                  \
                                 "   cmp   r3, #0                 \n\t"                  \
                                 "   bne   2b                     \n\t"                  \
                                 "   cmp" suffix " r2, %1       \n\t" /* for SET_FLAG */ \
                                 "1: clrex                        \n\t"                  \
                                 : "=Q"(var) /* no offset for ARM mode */                \
                                 : "r"(compare), "r"(exchange)                           \
                                 : "cc", "memory", "r2", "r3");
#        define ATOMIC_COMPARE_EXCHANGE_int(var, compare, exchange) \
            ATOMIC_COMPARE_EXCHANGE_suffix("", var, compare, exchange)
#        define ATOMIC_COMPARE_EXCHANGE_int64(var, compare, exchange) \
            ATOMIC_COMPARE_EXCHANGE_suffix("d", var, compare, exchange)
#        define ATOMIC_EXCHANGE(var, newval, result)            \
            __asm__ __volatile__("   dmb ish              \n\t" \
                                 "1: ldrex r2, %0         \n\t" \
                                 "   strex r3, %2, %0     \n\t" \
                                 "   cmp   r3, #0         \n\t" \
                                 "   bne   1b             \n\t" \
                                 "   str   r2, %1"              \
                                 : "=Q"(var), "=m"(result)      \
                                 : "r"(newval)                  \
                                 : "cc", "memory", "r2", "r3");

#        define MEMORY_STORE_BARRIER() __asm__ __volatile__("dmb st")
/* QEMU crashes on "wfi" so we use the superset "wfe".
 * XXX: Consider issuing "sev" on lock release?
 */
#        define SPINLOCK_PAUSE() __asm__ __volatile__("wfe") /* wait for interrupt */
#        define SERIALIZE_INSTRUCTIONS() __asm__ __volatile__("clrex");
/* assuming flag is unsigned char */
#        define SET_FLAG(cc, flag)                            \
            __asm__ __volatile__("   mov       r2, #1  \n\t"  \
                                 "   b" #cc "   1f      \n\t" \
                                 "   mov       r2, #0  \n\t"  \
                                 "1: strb r2, %0"             \
                                 : "=m"(flag)                 \
                                 :                            \
                                 : "r2")
#        define SET_IF_NOT_ZERO(flag) SET_FLAG(ne, flag)
#        define SET_IF_NOT_LESS(flag) SET_FLAG(ge, flag)
#    elif defined(DR_HOST_RISCV64)
#        define ATOMIC_1BYTE_READ(addr_src, addr_res)               \
            do {                                                    \
                /* Fence for acquire semantics. */                  \
                __asm__ __volatile__("lbu t0, 0(%0)\n\t"            \
                                     "sb t0, 0(%1)\n\t"             \
                                     "fence iorw,iorw"              \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "memory");                   \
            } while (0)
#        define ATOMIC_1BYTE_WRITE(target, value, hot_patch)   \
            do {                                               \
                /* Not currently used to write code */         \
                ASSERT_CURIOSITY(!hot_patch);                  \
                /* Fence for release semantics. */             \
                __asm__ __volatile__("fence iorw,iorw\n\t"     \
                                     "sb %0, 0(%1)\n\t"        \
                                     :                         \
                                     : "r"(value), "r"(target) \
                                     : "memory");              \
            } while (0)
#        define ATOMIC_4BYTE_ALIGNED_WRITE(target, value, hot_patch) \
            do {                                                     \
                /* Page 25 of Volume I: RISC-V Unprivileged ISA      \
                 * V20191213 states that aligned loads/stores are    \
                 * atomic. Alternatively we could use                \
                 * amoswap.w zero %0, 0(%1) but that would incur     \
                 * an extra read we don't need and also requires     \
                 * alignment.                                        \
                 */                                                  \
                ASSERT(ALIGNED(target, 4));                          \
                /* Not currently used to write code. */              \
                ASSERT_CURIOSITY(!hot_patch);                        \
                /* Fence for release semantics. */                   \
                __asm__ __volatile__("fence iorw,iorw\n\t"           \
                                     "sw %0, 0(%1)\n\t"              \
                                     :                               \
                                     : "r"(value), "r"(target)       \
                                     : "memory");                    \
            } while (0)
#        define ATOMIC_4BYTE_WRITE ATOMIC_4BYTE_ALIGNED_WRITE
#        define ATOMIC_4BYTE_ALIGNED_READ(addr_src, addr_res)       \
            do {                                                    \
                /* Page 25 of Volume I: RISC-V Unprivileged ISA     \
                 * V20191213 states that aligned loads/stores are   \
                 * atomic.                                          \
                 */                                                 \
                ASSERT(ALIGNED(addr_src, 4));                       \
                ASSERT(ALIGNED(addr_res, 4));                       \
                /* Fence for acquire semantics. */                  \
                __asm__ __volatile__("lw t0, 0(%0)\n\t"             \
                                     "sw t0, 0(%1) \n\t"            \
                                     "fence iorw,iorw"              \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "memory");                   \
            } while (0)
#        define ATOMIC_8BYTE_ALIGNED_WRITE(target, value, hot_patch) \
            do {                                                     \
                /* Page 25 of Volume I: RISC-V Unprivileged ISA      \
                 * V20191213 states that aligned loads/stores are    \
                 * atomic. Alternatively we could use                \
                 * amoswap.w zero %0, 0(%1) but that would incur     \
                 * an extra read we don't need and also requires     \
                 * alignment.                                        \
                 */                                                  \
                ASSERT(ALIGNED(target, 8));                          \
                /* Not currently used to write code */               \
                ASSERT_CURIOSITY(!hot_patch);                        \
                /* Fence for release semantics. */                   \
                __asm__ __volatile__("fence iorw,iorw\n\t"           \
                                     "sd %1, 0(%0)\n\t"              \
                                     :                               \
                                     : "r"(target), "r"(value)       \
                                     : "memory");                    \
            } while (0)
#        define ATOMIC_8BYTE_WRITE ATOMIC_8BYTE_ALIGNED_WRITE
#        define ATOMIC_8BYTE_ALIGNED_READ(addr_src, addr_res)       \
            do {                                                    \
                /* Page 25 of Volume I: RISC-V Unprivileged ISA     \
                 * V20191213 states that aligned loads/stores are   \
                 * atomic.                                          \
                 */                                                 \
                ASSERT(ALIGNED(addr_src, 8));                       \
                ASSERT(ALIGNED(addr_res, 8));                       \
                __asm__ __volatile__("ld t0, 0(%0)\n\t"             \
                                     "sd t0, 0(%1)\n\t"             \
                                     "fence iorw,iorw"              \
                                     :                              \
                                     : "r"(addr_src), "r"(addr_res) \
                                     : "memory");                   \
            } while (0)

#        define ADDI_suffix(sfx, var, val, align)                              \
            do {                                                               \
                /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213      \
                 * requires that var address is naturally aligned.             \
                 */                                                            \
                ASSERT(ALIGNED(var, align));                                   \
                __asm__ __volatile__("li t0, " #val "\n\t"                     \
                                     "amoadd." sfx ".aqrl zero,  t0, (%0)\n\t" \
                                     :                                         \
                                     : "r"(var)                                \
                                     : "t0", "memory");                        \
            } while (0)

static inline void
ATOMIC_INC_int(volatile int *var)
{
    ADDI_suffix("w", var, 1, 4);
}

static inline void
ATOMIC_INC_int64(volatile int64 *var)
{
    ADDI_suffix("d", var, 1, 8);
}

static inline void
ATOMIC_DEC_int(volatile int *var)
{
    ADDI_suffix("w", var, -1, 4);
}

static inline void
ATOMIC_DEC_int64(volatile int64 *var)
{
    ADDI_suffix("d", var, -1, 8);
}

#        define ATOMIC_INC(type, var) ATOMIC_INC_##type(&var)
#        define ATOMIC_DEC(type, var) ATOMIC_DEC_##type(&var)

#        undef ADDI_suffix

static inline void
ATOMIC_ADD_int(volatile int *var, int val)
{
    /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 4));
    __asm__ __volatile__("amoadd.w.aqrl zero, %1, (%0)\n\t"
                         :
                         : "r"(var), "r"(val)
                         : "memory");
}

static inline void
ATOMIC_ADD_int64(volatile int64 *var, int64 val)
{
    /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 8));
    __asm__ __volatile__("amoadd.d.aqrl zero, %1, (%0)\n\t"
                         :
                         : "r"(var), "r"(val)
                         : "memory");
}

#        define ATOMIC_ADD(type, var, val) ATOMIC_ADD_##type(&var, val)

static inline int
atomic_add_exchange_int(volatile int *var, int val)
{
    int res = 0;
    /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 4));
    __asm__ __volatile__("amoadd.w.aqrl %0, %2, (%1)\n\t"
                         : "=r"(res)
                         : "r"(var), "r"(val)
                         : "memory");
    return res + val;
}

static inline int64
atomic_add_exchange_int64(volatile int64 *var, int64 val)
{
    int64 res = 0;
    /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 8));
    __asm__ __volatile__("amoadd.d.aqrl %0, %2, (%1)\n\t"
                         : "=r"(res)
                         : "r"(var), "r"(val)
                         : "memory");
    return res + val;
}

#        define atomic_add_exchange atomic_add_exchange_int

static inline bool
atomic_compare_exchange_int(volatile int *var, int compare, int exchange)
{
    bool res = 1;
    /* Page 49 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 4));
    /* This code is based on GCC's atomic_compare_exchange_strong().
     * The fence ensures no re-ordering beyond the start of the lr/sc loop.
     * Provides acquire/release semantics for LR/SC sequences by setting the
     * aq and rl bits on lr and sc respectively.
     */
    __asm__ __volatile__("fence   iorw,ow\n\t"
                         "1: lr.w.aq    t0, (%1)\n\t"
                         "   bne        t0, %2, 2f\n\t"
                         "   sc.w.rl    %0, %3, (%1)\n\t"
                         "   bnez       %0, 1b\n\t"
                         "2:\n\t"
                         : "+r"(res)
                         : "r"(var), "r"(compare), "r"(exchange)
                         : "t0", "memory");
    return res == 0;
}

static inline bool
atomic_compare_exchange_int64(volatile int64 *var, int64 compare, int64 exchange)
{
    bool res = 1;
    /* Page 49 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address is
     * naturally aligned.
     */
    ASSERT(ALIGNED(var, 8));
    /* This code is based on GCC's atomic_compare_exchange_strong().
     * The fence ensures no re-ordering beyond the start of the lr/sc loop.
     * Provides acquire/release semantics for LR/SC sequences by setting the
     * aq and rl bits on lr and sc respectively.
     */
    __asm__ __volatile__("fence   iorw,ow\n\t"
                         "1: lr.d.aq    t0, (%1)\n\t"
                         "   bne        t0, %2, 2f\n\t"
                         "   sc.d.rl    %0, %3, (%1)\n\t"
                         "   bnez       %0, 1b\n\t"
                         "2:\n\t"
                         : "+r"(res)
                         : "r"(var), "r"(compare), "r"(exchange)
                         : "t0", "memory");
    return res == 0;
}

#        define ATOMIC_COMPARE_EXCHANGE_int(var, compare, exchange) \
            atomic_compare_exchange_int(var, compare, exchange)
#        define ATOMIC_COMPARE_EXCHANGE_int64(var, compare, exchange) \
            atomic_compare_exchange_int64(var, compare, exchange)

/* exchanges *var with newval and returns original *var */
static inline int
atomic_exchange_int(volatile int *var, int newval)
{
    int res;
    /* Page 53 of Volume I: RISC-V Unprivileged ISA V20191213 requires that var address
     * is naturally aligned.
     */
    ASSERT(ALIGNED(var, 4));
    __asm__ __volatile__("amoswap.w.aqrl %0, %2, (%1)\n\t"
                         : "=r"(res)
                         : "r"(var), "r"(newval)
                         : "memory");
    return res;
}

/* Ensure no store reordering for normal memory writes. */
#        define MEMORY_STORE_BARRIER() __asm__ __volatile__("fence w,w")

/* Insert pause hint directly to be compatible with old compilers. This
 * will work even on platforms without Zihintpause extension because this
 * is a FENCE hint instruction which evaluates to NOP then.
 */
#        define SPINLOCK_PAUSE() __asm__ __volatile__(".int 0x0100000F");

static inline bool
atomic_inc_and_test(volatile int *var)
{
    return atomic_add_exchange_int(var, 1) == 0;
}

static inline bool
atomic_dec_and_test(volatile int *var)
{
    return atomic_add_exchange_int(var, -1) == -1;
}

static inline bool
atomic_dec_becomes_zero(volatile int *var)
{
    return atomic_add_exchange_int(var, -1) == 0;
}
#    endif /* RISCV64 */

#    ifdef X64
#        define ATOMIC_ADD_PTR(type, var, val) ATOMIC_ADD_int64(var, val)
#    else
#        define ATOMIC_ADD_PTR(type, var, val) ATOMIC_ADD_int(var, val)
#    endif
#    define ATOMIC_COMPARE_EXCHANGE ATOMIC_COMPARE_EXCHANGE_int
#    ifdef X64
#        define ATOMIC_COMPARE_EXCHANGE_PTR ATOMIC_COMPARE_EXCHANGE_int64
#    else
#        define ATOMIC_COMPARE_EXCHANGE_PTR ATOMIC_COMPARE_EXCHANGE
#    endif

#    if !defined(DR_HOST_AARCH64) && !defined(DR_HOST_RISCV64)
/* Atomically increments *var by 1
 * Returns true if the resulting value is zero, otherwise returns false
 */
static inline bool
atomic_inc_and_test(volatile int *var)
{
    unsigned char c;

    ATOMIC_INC(int, *var);
    /* flags should be set according to resulting value, now we convert that back to C */
    SET_IF_NOT_ZERO(c);
    /* FIXME: we add an extra memory reference to a local,
       although we could put the return value in EAX ourselves */
    return c == 0;
}

/* Atomically decrements *var by 1
 * Returns true if the initial value was zero, otherwise returns false
 */
static inline bool
atomic_dec_and_test(volatile int *var)
{
    unsigned char c;

    ATOMIC_DEC(int, *var);
    /* result should be set according to value before change, now we
     * convert that back to C.
     */
    SET_IF_NOT_LESS(c);
    /* FIXME: we add an extra memory reference to a local,
       although we could put the return value in EAX ourselves */
    return c == 0;
}

/* Atomically decrements *var by 1
 * Returns true if the resulting value is zero, otherwise returns false
 */
static inline bool
atomic_dec_becomes_zero(volatile int *var)
{
    unsigned char c;

    ATOMIC_DEC(int, *var);
    /* result should be set according to value after change, now we
     * convert that back to C.
     */
    SET_IF_NOT_ZERO(c);
    /* FIXME: we add an extra memory reference to a local,
       although we could put the return value in EAX ourselves */
    return c == 0;
}

/* returns true if var was equal to compare, and now is equal to exchange,
   otherwise returns false
 */
static inline bool
atomic_compare_exchange_int(volatile int *var, int compare, int exchange)
{
    unsigned char c;
    ATOMIC_COMPARE_EXCHANGE(*var, compare, exchange);
    /* ZF is set if matched, all other flags are as if a normal compare happened */
    /* we convert ZF value back to C */
    SET_IF_NOT_ZERO(c);
    /* FIXME: we add an extra memory reference to a local,
       although we could put the return value in EAX ourselves */
    return c == 0;
}

/* exchanges *var with newval and returns original *var */
static inline int
atomic_exchange_int(volatile int *var, int newval)
{
    int result;
    ATOMIC_EXCHANGE(*var, newval, result);
    return result;
}

#        ifdef X64
/* returns true if var was equal to compare, and now is equal to exchange,
   otherwise returns false
 */
static inline bool
atomic_compare_exchange_int64(volatile int64 *var, int64 compare, int64 exchange)
{
    unsigned char c;
    ATOMIC_COMPARE_EXCHANGE_int64(*var, compare, exchange);
    /* ZF is set if matched, all other flags are as if a normal compare happened */
    /* we convert ZF value back to C */
    SET_IF_NOT_ZERO(c);
    /* FIXME: we add an extra memory reference to a local,
       although we could put the return value in EAX ourselves */
    return c == 0;
}
#        endif

/* atomically adds value to memory location var and returns the sum */
static inline int
atomic_add_exchange_int(volatile int *var, int value)
{
    int temp;
    ATOMIC_ADD_EXCHANGE_int(var, value, temp);
    return (temp + value);
}
static inline int64
atomic_add_exchange_int64(volatile int64 *var, int64 value)
{
    int64 temp;
    ATOMIC_ADD_EXCHANGE_int64(var, value, temp);
    return (temp + value);
}
#        define atomic_add_exchange atomic_add_exchange_int
#        undef ATOMIC_ADD_EXCHANGE_suffix
#        undef ATOMIC_ADD_EXCHANGE_int
#        undef ATOMIC_ADD_EXCHANGE_int64
#    endif /* !AARCH64 */

#endif /* UNIX */

static inline int
atomic_aligned_read_int(volatile int *var)
{
    int temp = 0;
    ATOMIC_4BYTE_ALIGNED_READ(var, &temp);
    return temp;
}

static inline bool
atomic_read_bool(volatile bool *var)
{
    bool temp = 0;
    ATOMIC_1BYTE_READ(var, &temp);
    return temp;
}

#ifdef X64
static inline int64
atomic_aligned_read_int64(volatile int64 *var)
{
    int64 temp = 0;
    ATOMIC_8BYTE_ALIGNED_READ(var, &temp);
    return temp;
}
#endif

#define atomic_compare_exchange atomic_compare_exchange_int
#ifdef X64
#    define atomic_compare_exchange_ptr(v, c, e) \
        atomic_compare_exchange_int64((volatile int64 *)(v), (int64)(c), (int64)(e))
#    define ATOMIC_ADDR_WRITE ATOMIC_8BYTE_WRITE
#else
#    define atomic_compare_exchange_ptr(v, c, e) \
        atomic_compare_exchange_int((volatile int *)(v), (int)(c), (int)(e))
#    define ATOMIC_ADDR_WRITE ATOMIC_4BYTE_WRITE
#endif

#define ATOMIC_MAX_int(type, maxvar, curvar)                                        \
    do {                                                                            \
        type atomic_max__maxval;                                                    \
        type atomic_max__curval = (curvar);                                         \
        ASSERT(sizeof(int) == sizeof(maxvar));                                      \
        ASSERT(sizeof(type) == sizeof(maxvar));                                     \
        ASSERT(sizeof(type) == sizeof(curvar));                                     \
        do {                                                                        \
            atomic_max__maxval = atomic_aligned_read_int((int *)&(maxvar));         \
        } while (atomic_max__maxval < atomic_max__curval &&                         \
                 !atomic_compare_exchange_int((int *)&(maxvar), atomic_max__maxval, \
                                              atomic_max__curval));                 \
    } while (0)

#ifdef X64 /* 64-bit intrinsics only avail on x64 */
#    define ATOMIC_MAX_int64(type, maxvar, curvar)                                     \
        do {                                                                           \
            type atomic_max__maxval;                                                   \
            type atomic_max__curval = (curvar);                                        \
            ASSERT(sizeof(int64) == sizeof(maxvar));                                   \
            ASSERT(sizeof(type) == sizeof(maxvar));                                    \
            ASSERT(sizeof(type) == sizeof(curvar));                                    \
            do {                                                                       \
                atomic_max__maxval = atomic_aligned_read_int64((int64 *)&(maxvar));    \
            } while (atomic_max__maxval < atomic_max__curval &&                        \
                     !atomic_compare_exchange_int64(                                   \
                         (int64 *)&(maxvar), atomic_max__maxval, atomic_max__curval)); \
        } while (0)
#endif

/* Our ATOMIC_* macros target release-acquire semantics.  ATOMIC_PTRSZ_ALIGNED_WRITE
 * is a Store-Release and ensures prior stores in program order in this thread
 * are not observed by another thread after this store.
 */
#ifdef X64
#    define ATOMIC_PTRSZ_ALIGNED_WRITE(target, value, hot_patch) \
        ATOMIC_8BYTE_ALIGNED_WRITE(target, value, hot_patch)
#else
#    define ATOMIC_PTRSZ_ALIGNED_WRITE(target, value, hot_patch) \
        ATOMIC_4BYTE_ALIGNED_WRITE(target, value, hot_patch)
#endif

#define ATOMIC_MAX(type, maxvar, curvar) ATOMIC_MAX_##type(type, maxvar, curvar)

#endif /* _ATOMIC_EXPORTS_H_ */
