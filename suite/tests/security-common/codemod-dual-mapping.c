/* **********************************************************
 * Copyright (c) 2026 Arm Limited. All rights reserved.
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
 * * Neither the name of Arm Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Exercise code modified through a separate writable alias. */
#include "tools.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define CHECK_RESULT(expr, message)                         \
    do {                                                    \
        if ((expr) != 0) {                                  \
            print("%s:%u %s", __FILE__, __LINE__, message); \
            return 1;                                       \
        }                                                   \
    } while (0)

#define EXPECT(lhs_expr, rhs_expr)                                                \
    do {                                                                          \
        uint16_t lhs = lhs_expr;                                                  \
        uint16_t rhs = rhs_expr;                                                  \
        if (!((lhs) == (rhs))) {                                                  \
            print("%s:%u Expected " #lhs_expr " == " #rhs_expr " but %u != %u\n", \
                  __FILE__, __LINE__, lhs, rhs);                                  \
            return 1;                                                             \
        }                                                                         \
    } while (0)

/* Generate code that returns `value` and write it to `buf`. */
static void
write_code(byte *buf, uint16_t value)
{
#ifdef AARCH64
    uint code[] = {
        0x52800000 | (value << 5), /* mov w0, #value */
        0xd65f03c0,                /* ret */
    };
    memcpy(buf, code, sizeof(code));
    tools_clear_icache(buf, buf + sizeof(code));
#else
#    error "Target architecture not supported."
#endif
}

static int
test_dual_mapping(int fd, byte *rw)
{
    byte *rx = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    assert(rx != MAP_FAILED);
    uint16_t (*func)(void) = (uint16_t (*)(void))rx;

    /* Write code to rw and run it through rx to prime the code cache. */
    write_code(rw, 1);
    EXPECT(func(), 1);

    /* Modify the code and run again. We should see the update. */
    write_code(rw, 5);
    EXPECT(func(), 5);

    /* Run the code enough times to cross the s2ro threshold.
     * Shared pages should not be put into ro-mode because making rx read-only will not
     * detect writes to rw. Shared mappings should stay in sandbox mode.
     */
    for (unsigned i = 0; i < 100; i++) {
        EXPECT(func(), 5);
    }

    /* Modify the code again to make sure we are still in sandbox mode and can still
     * detect changes.
     */
    write_code(rw, 7);
    EXPECT(func(), 7);

    /* Changing protections on either shared alias must not lose track of the
     * SHARED attribute.
     */
    CHECK_RESULT(mprotect(rw, PAGE_SIZE, PROT_READ), "mprotect(rw, R) failed.");
    EXPECT(func(), 7);
    CHECK_RESULT(mprotect(rw, PAGE_SIZE, PROT_READ | PROT_WRITE),
                 "mprotect(rw, RW) failed.");
    write_code(rw, 9);
    EXPECT(func(), 9);

    CHECK_RESULT(mprotect(rx, PAGE_SIZE, PROT_READ), "mprotect(rx, R) failed.");
    CHECK_RESULT(mprotect(rx, PAGE_SIZE, PROT_READ | PROT_EXEC),
                 "mprotect(rx, RX) failed.");
    EXPECT(func(), 9);
    write_code(rw, 11);
    EXPECT(func(), 11);

    /* Making the executable alias writable and non-executable removes its
     * executable-area record. Restoring RX must retain the shared-mapping flag
     * when that record is recreated, so later writes through rw are detected.
     */
    CHECK_RESULT(mprotect(rx, PAGE_SIZE, PROT_READ | PROT_WRITE),
                 "mprotect(rx, RW) failed.");
    CHECK_RESULT(mprotect(rx, PAGE_SIZE, PROT_READ | PROT_EXEC),
                 "mprotect(rx, RX) failed.");
    EXPECT(func(), 11);
    write_code(rw, 13);
    EXPECT(func(), 13);

    CHECK_RESULT(munmap(rx, PAGE_SIZE), "munmap(rx) failed.");
    return 0;
}

static int
test_mixed_mprotect(int fd, byte *rw)
{
    /* A single protection change can span both private and shared mappings.
     * Keep the first page private and replace the second page with a shared alias.
     * Checking only the first page's sharing attribute would leave the second page
     * unsandboxed when both pages are made executable.
     */
    byte *mixed = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mixed != MAP_FAILED);
    byte *rx = mmap(mixed + PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED, fd, 0);
    assert(rx != MAP_FAILED);
    uint16_t (*func)(void) = (uint16_t (*)(void))rx;

    write_code(rw, 15);
    CHECK_RESULT(mprotect(mixed, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC),
                 "mprotect(mixed, RX) failed.");
    EXPECT(func(), 15);
    write_code(rw, 17);
    EXPECT(func(), 17);

    /* Also cover replacing existing writable executable areas with read-only ones.
     * Execute both pages while writable so both have executable-area records.
     */
    CHECK_RESULT(mprotect(mixed, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC),
                 "mprotect(mixed, RWX) failed.");
    write_code(mixed, 19);
    uint16_t (*private_func)(void) = (uint16_t (*)(void))mixed;
    EXPECT(private_func(), 19);
    EXPECT(func(), 17);
    CHECK_RESULT(mprotect(mixed, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC),
                 "mprotect(mixed, RX) failed.");
    EXPECT(func(), 17);
    write_code(rw, 21);
    EXPECT(func(), 21);

    CHECK_RESULT(munmap(mixed, 2 * PAGE_SIZE), "munmap(mixed) failed.");
    return 0;
}

static int
test_map_fixed(int fd, byte *rw)
{
    /* MAP_FIXED replaces the mapping even when the permissions do not change.
     * Cover both an unexecuted private RX page and one with cached fragments,
     * while retaining executable private pages on either side of the replacement.
     * The last case starts with writable code that DR can make read-only.
     */
    enum { UNEXECUTED_RX, EXECUTED_RX, EXECUTED_RWX };
    for (int mode = UNEXECUTED_RX; mode <= EXECUTED_RWX; ++mode) {
        byte *private_pages = mmap(NULL, 3 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(private_pages != MAP_FAILED);
        write_code(private_pages, 23);
        write_code(private_pages + PAGE_SIZE, 25);
        write_code(private_pages + 2 * PAGE_SIZE, 27);
        CHECK_RESULT(
            mprotect(private_pages, 3 * PAGE_SIZE,
                     PROT_READ | PROT_EXEC | (mode == EXECUTED_RWX ? PROT_WRITE : 0)),
            "mprotect(private) failed.");
        uint16_t (*before)(void) = (uint16_t (*)(void))private_pages;
        uint16_t (*after)(void) = (uint16_t (*)(void))(private_pages + 2 * PAGE_SIZE);
        uint16_t (*func)(void) = (uint16_t (*)(void))(private_pages + PAGE_SIZE);
        if (mode != UNEXECUTED_RX) {
            EXPECT(before(), 23);
            EXPECT(func(), 25);
            EXPECT(after(), 27);
        }

        write_code(rw, 29);
        /* mmap rounds the replacement length up to a whole page. */
        byte *rx = mmap(private_pages + PAGE_SIZE, PAGE_SIZE - 1, PROT_READ | PROT_EXEC,
                        MAP_SHARED | MAP_FIXED, fd, 0);
        assert(rx == private_pages + PAGE_SIZE);
        EXPECT(func(), 29);
        EXPECT(before(), 23);
        EXPECT(after(), 27);
        for (unsigned i = 0; i < 100; ++i)
            EXPECT(func(), 29);
        write_code(rw, 31);
        EXPECT(func(), 31);

        /* A failed replacement must leave the original code usable, including
         * detecting later writes through its shared alias.
         */
        byte *failed =
            mmap(rx, PAGE_SIZE, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, -1, 0);
        assert(failed == MAP_FAILED);
        EXPECT(func(), 31);
        write_code(rw, 33);
        EXPECT(func(), 33);

        CHECK_RESULT(munmap(private_pages, 3 * PAGE_SIZE), "munmap(private) failed.");
    }
    return 0;
}

static int
test_mremap_fixed(int fd, byte *rw)
{
    /* MREMAP_FIXED replaces its destination just like MAP_FIXED.  A failed remap
     * must retain the destination's executable-area state, while a successful
     * private-to-shared replacement must recreate it with the shared flag.
     */
    byte *remap_source = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    assert(remap_source != MAP_FAILED);
    byte *remap_target =
        mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(remap_target != MAP_FAILED);
    write_code(remap_target, 35);
    CHECK_RESULT(mprotect(remap_target, PAGE_SIZE, PROT_READ | PROT_EXEC),
                 "mprotect(remap target, RX) failed.");
    uint16_t (*func)(void) = (uint16_t (*)(void))remap_target;
    EXPECT(func(), 35);

    write_code(rw, 37);
    byte *rx = mremap(remap_source, PAGE_SIZE, PAGE_SIZE, MREMAP_FIXED, remap_target);
    assert(rx == MAP_FAILED);
    EXPECT(func(), 35);

    rx = mremap(remap_source, PAGE_SIZE, PAGE_SIZE, MREMAP_MAYMOVE | MREMAP_FIXED,
                remap_target);
    assert(rx == remap_target);
    EXPECT(func(), 37);
    for (unsigned i = 0; i < 100; ++i)
        EXPECT(func(), 37);
    write_code(rw, 39);
    EXPECT(func(), 39);
    CHECK_RESULT(munmap(remap_target, PAGE_SIZE), "munmap(remap target) failed.");
    return 0;
}

static int
test_map_fixed_range(int fd)
{
    /* Only the second page changes sharing in the final replacement.  Check the
     * whole range, including shared-to-private transitions, not just its start.
     */
    byte *mixed = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mixed != MAP_FAILED);
    byte *rx = mmap(mixed + PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_EXEC,
                    MAP_SHARED | MAP_FIXED, fd, 0);
    assert(rx == mixed + PAGE_SIZE);
    byte *replacement = mmap(mixed, 2 * PAGE_SIZE, PROT_READ | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(replacement == mixed);
    CHECK_RESULT(munmap(mixed, 2 * PAGE_SIZE), "munmap(mixed) failed.");
    return 0;
}

int
main(void)
{
    int fd = memfd_create("dual_mapping", /*flags=*/0);
    assert(fd >= 0);

    CHECK_RESULT(ftruncate(fd, PAGE_SIZE), "ftruncate() failed.");

    byte *rw = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rw != MAP_FAILED);

    CHECK_RESULT(test_dual_mapping(fd, rw), "test_dual_mapping() failed.");
    CHECK_RESULT(test_mixed_mprotect(fd, rw), "test_mixed_mprotect() failed.");
    CHECK_RESULT(test_map_fixed(fd, rw), "test_map_fixed() failed.");
    CHECK_RESULT(test_mremap_fixed(fd, rw), "test_mremap_fixed() failed.");
    CHECK_RESULT(test_map_fixed_range(fd), "test_map_fixed_range() failed.");

    CHECK_RESULT(munmap(rw, PAGE_SIZE), "munmap(rw) failed.");

    CHECK_RESULT(close(fd), "Failed to close fd.");

    print("Test passed\n");
    return 0;
}
