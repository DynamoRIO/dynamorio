/* **********************************************************
 * Copyright (c) 2023 Arm Limited. All rights reserved.
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

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <sys/prctl.h>
#include <vector>

#include "tools.h"

namespace {

/*
 * Tests are specified assuming 128-bit vectors. If we run on hardware with a
 * higher VL then vector values are made up to the correct size by duplicating
 * the first 128-bits.
 */
constexpr size_t TEST_VL_BYTES = 16;

constexpr size_t NUM_Z_REGS = 32;
constexpr size_t NUM_P_REGS = 16;

using vector_reg_value128_t = std::array<uint8_t, TEST_VL_BYTES>;
using predicate_reg_value128_t = uint16_t;

constexpr vector_reg_value128_t UNINITIALIZED_VECTOR {
    0xAD, 0xDE, 0xAD, 0xDE, 0xAD, 0xDE, 0xAD, 0xDE,
    0xAD, 0xDE, 0xAD, 0xDE, 0xAD, 0xDE, 0xAD, 0xDE,
};
constexpr predicate_reg_value128_t UNINITIALIZED_PREDICATE = 0xDEAD;

enum class element_size_t {
    BYTE = 1,
    HALF = 2,
    SINGLE = 4,
    DOUBLE = 8,
};

/* Exhaustive lists of valid 128-bit vl predicate register values for single or double
 * sized elements.
 */
const std::map<element_size_t, std::vector<predicate_reg_value128_t>> ALL_PREDICATES {
    { element_size_t::SINGLE,
      { 0x0000, 0x0001, 0x0010, 0x0011, 0x0100, 0x0101, 0x0110, 0x0111, 0x1000, 0x1001,
        0x1010, 0x1011, 0x1100, 0x1101, 0x1110, 0x1111 } },
    { element_size_t::DOUBLE, { 0x0000, 0x0001, 0x0100, 0x0101 } },
};

enum test_result_t {
    FAIL,
    PASS,
};

bool
element_is_active(size_t element, predicate_reg_value128_t mask,
                  element_size_t element_size)
{
    const auto element_size_bytes = static_cast<size_t>(element_size);
    const auto element_flag = 1 << (element_size_bytes * element);
    return TESTALL(element_flag, mask);
}

/*
 * Create a copy of the data vector with all the elements that are inactive in the mask
 * set to 0.
 */
vector_reg_value128_t
apply_predicate_mask(vector_reg_value128_t data, predicate_reg_value128_t mask,
                     element_size_t element_size)
{
    const auto element_size_bytes = static_cast<size_t>(element_size);
    const auto num_elements = data.size() / element_size_bytes;
    for (size_t i = 0; i < num_elements; i++) {
        if (!element_is_active(i, mask, element_size)) {
            // Element is inactive, set it to 0.
            memset(&data[element_size_bytes * i], 0, element_size_bytes);
        }
    }

    return data;
}

size_t
get_vl_bytes()
{
    static const auto vl_bytes = []() {
        const int returned_value = prctl(PR_SVE_GET_VL);
        if (returned_value < 0) {
            perror("prctl(PR_SVE_GET_VL) failed");
            exit(1);
        }

        return static_cast<size_t>(returned_value & PR_SVE_VL_LEN_MASK);
    }();
    return vl_bytes;
}

struct scalable_reg_value_t {
    const uint8_t *data;
    size_t size;

    bool
    operator==(const scalable_reg_value_t &other) const
    {
        return (other.size == size) && (memcmp(data, other.data, size) == 0);
    }

    bool
    operator!=(const scalable_reg_value_t &other) const
    {
        return !(*this == other);
    }
};

void
print_vector(const scalable_reg_value_t &value)
{
    print("0x");
    for (size_t i = 0; i < value.size; i++) {
        print("%02x", value.data[i]);
    }
}

/*
 * Print a predicate register value as a binary number. Each bit is printed with a space
 * in between so that the bit will line up vertically with the corresponding byte of a
 * vector register printed on an adjacent line.
 *     vec:  0x12345678
 *     pred: 0b 0 1 0 1
 */
void
print_predicate(const scalable_reg_value_t &value)
{
    print("0b");
    for (size_t byte_i = 0; byte_i < value.size; byte_i++) {
        for (unsigned bit = 0; bit < 8; bit++) {
            if (TESTALL(1 << bit, value.data[byte_i]))
                print(" 1");
            else
                print(" 0");
        }
    }
}

struct sve_register_file_t {
    std::vector<uint8_t> z;
    std::vector<uint8_t> p;

    sve_register_file_t()
    {
        const auto vl_bytes = get_vl_bytes();
        const auto pl_bytes = vl_bytes / 8;
        z.resize(NUM_Z_REGS * vl_bytes);
        p.resize(NUM_P_REGS * pl_bytes);
    }

    scalable_reg_value_t
    get_z_register_value(size_t reg_num) const
    {
        assert(reg_num < NUM_Z_REGS);
        const auto vl_bytes = get_vl_bytes();
        return { &z[vl_bytes * reg_num], vl_bytes };
    }

    void
    set_z_register_value(size_t reg_num, vector_reg_value128_t value)
    {
        const auto vl_bytes = get_vl_bytes();
        const auto reg_offset = vl_bytes * reg_num;
        for (size_t i = 0; i < vl_bytes / TEST_VL_BYTES; i++) {
            const auto slice_offset = reg_offset + (TEST_VL_BYTES * i);
            memcpy(&z[slice_offset], value.data(), TEST_VL_BYTES);
        }
    }

    scalable_reg_value_t
    get_p_register_value(size_t reg_num) const
    {
        assert(reg_num < NUM_P_REGS);
        const auto pl_bytes = get_vl_bytes() / 8;
        return { &p[pl_bytes * reg_num], pl_bytes };
    }

    void
    set_p_register_value(size_t reg_num, predicate_reg_value128_t value)
    {
        const auto pl_bytes = get_vl_bytes() / 8;
        const auto reg_offset = pl_bytes * reg_num;
        for (size_t i = 0; i < pl_bytes / sizeof(value); i++) {
            const auto slice_offset = reg_offset + (sizeof(value) * i);
            memcpy(&p[slice_offset], &value, sizeof(value));
        }
    }
};

struct test_register_data_t {
    sve_register_file_t before; // Values the registers will be set to before the test.
    sve_register_file_t after;  // Values of the registers after the test instruction.
};

struct scalar_plus_vector_test_case_t {
    std::string name; // Unique name for this test printed when the test is run.

    struct test_ptrs_t {
        const void *base;           // Base address used for the test instruction.
        const void *z_restore_base; // Base address for initializing Z registers.
        const void *p_restore_base; // Base address for initializing P registers.
        void *z_save_base;          // Base address to save Z registers to after test
                                    // instruction
        void *p_save_base;          // Base address to save P registers to after test
                                    // instruction
    };

    using test_func_t = std::function<void(test_ptrs_t &)>;
    test_func_t run_test;

    vector_reg_value128_t reference_data;
    vector_reg_value128_t offset_data;
    element_size_t element_size;
    const void *input_data;

    struct registers_used_t {
        unsigned dest_z;
        unsigned governing_p;
        unsigned index_z;
    };
    registers_used_t registers_used;

    template <typename ELEMENT_T, typename OFFSET_T>
    scalar_plus_vector_test_case_t(
        std::string name_, test_func_t func_, registers_used_t registers_used_,
        std::array<ELEMENT_T, TEST_VL_BYTES / sizeof(ELEMENT_T)> reference_data_,
        std::array<OFFSET_T, TEST_VL_BYTES / sizeof(OFFSET_T)> offsets,
        const void *input_data_)
        : name(std::move(name_))
        , run_test(std::move(func_))
        , registers_used(registers_used_)
        , element_size(static_cast<element_size_t>(sizeof(ELEMENT_T)))
        , input_data(input_data_)
    {
        std::memcpy(reference_data.data(), reference_data_.data(), reference_data.size());
        std::memcpy(offset_data.data(), offsets.data(), offset_data.size());
    }

    test_result_t
    run_test_case() const
    {
        test_result_t status = PASS;
        const auto test_failed = [&status]() {
            if (status == PASS) {
                status = FAIL;
                print("FAIL\n");
            }
        };
        print("%s: ", name.c_str());

        test_register_data_t register_data;
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            register_data.before.set_z_register_value(i, UNINITIALIZED_VECTOR);
        }
        register_data.before.set_z_register_value(registers_used.index_z, offset_data);
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            register_data.before.set_p_register_value(i, UNINITIALIZED_PREDICATE);
        }

        test_ptrs_t ptrs {
            input_data,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
        const size_t num_elements =
            offset_data.size() / static_cast<size_t>(element_size);

        const auto vl_bytes = get_vl_bytes();
        std::vector<uint8_t> expected_output_data;
        expected_output_data.resize(vl_bytes);

        const auto &predicates = ALL_PREDICATES.at(element_size);
        for (const auto &pred : predicates) {
            /* TODO i#5036: Test faulting behavior. */

            const auto expected_output128 =
                apply_predicate_mask(reference_data, pred, element_size);
            for (size_t i = 0; i < vl_bytes / TEST_VL_BYTES; i++) {
                memcpy(&expected_output_data[TEST_VL_BYTES * i],
                       expected_output128.data(), TEST_VL_BYTES);
            }
            const scalable_reg_value_t expected_output {
                expected_output_data.data(),
                vl_bytes,
            };

            register_data.before.set_p_register_value(registers_used.governing_p, pred);

            run_test(ptrs);

            const auto output_value =
                register_data.after.get_z_register_value(registers_used.dest_z);

            if (output_value != expected_output) {
                test_failed();
                print("predicate: ");
                print_predicate(register_data.before.get_p_register_value(
                    registers_used.governing_p));
                print("\nexpected:  ");
                print_vector(expected_output);
                print("\nactual:    ");
                print_vector(output_value);
                print("\n");
            }

            // Check that the values of the other Z registers have been preserved.
            for (size_t i = 0; i < NUM_Z_REGS; i++) {
                if (i == registers_used.dest_z)
                    continue;
                const auto before = register_data.before.get_z_register_value(i);
                const auto after = register_data.after.get_z_register_value(i);
                if (before != after) {
                    test_failed();
                    print("z%u has been corrupted:\n", i);
                    print("before: ");
                    print_vector(before);
                    print("\nafter:  ");
                    print_vector(after);
                    print("\n");
                }
            }
            // Check that the values of the P registers have been preserved.
            for (size_t i = 0; i < NUM_P_REGS; i++) {
                const auto before = register_data.before.get_p_register_value(i);
                const auto after = register_data.after.get_p_register_value(i);
                if (before != after) {
                    test_failed();
                    print("p%u has been corrupted:\n", i);
                    print("before: ");
                    print_predicate(before);
                    print("\nafter:  ");
                    print_predicate(after);
                    print("\n");
                }
            }
        }
        if (status == PASS)
            print("PASS\n");

        return status;
#undef TEST_FAILED
    }
};

test_result_t
run_tests(const std::vector<scalar_plus_vector_test_case_t> &tests)
{
    test_result_t overall_status = PASS;

    for (const auto &instr_test : tests) {
        if (instr_test.run_test_case() == FAIL) {
            overall_status = FAIL;
        }
    }

    return overall_status;
}

class input_data_t {
public:
    const size_t DATA_SIZE = 9 * 4096; // 9 4KiB pages
    input_data_t()
        : data(mmap(nullptr, DATA_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
    {
        /*
         * We set up nine pages of memory to use as input data for load instruction
         * tests. The first page contains input data of different sizes and the rest
         * of the pages are set up to fault when they are accessed. The tests use
         * one of the first 4 regions as their base pointer, so when we want to
         * force an instruction to fault (to test faulting behaviour) we can set the
         * offset to 4096 which will always land in one of the faulting pages.
         * +=====================================================+
         * | Page   | Byte off | Region off |                    |
         * +=====================================================+
         * | page 0 |        0 |          0 | 8-bit input data   |
         * |        |----------+------------+--------------------+
         * |        |     1024 |          1 | 16-bit input data  |
         * |        |----------+------------+--------------------+
         * |        |     2048 |          2 | 32-bit input data  |
         * |        |----------+------------+--------------------+
         * |        |     3072 |          3 | 64-bit input data  |
         * +--------+----------+------------+--------------------+
         * | page 1 |     4096 |          4 | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +~~~~~~~~+~~~~~~~~~~+~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~+
         * | pg 2-7 |      ... |       ...  | All accesses fault |
         * +~~~~~~~~+~~~~~~~~~~+~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~+
         * | page 8 |    32768 |         32 | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +--------+----------+------------+--------------------+
         *
         */

        // Write 8, 16, 32, and 64-bit input data to the first page.
        // Each region contains 40 values. We set the base address to the 9th
        // element in the array so we can use 8 negative offsets and 32 positive
        // offsets.
        write_input_data(0,
                         std::array<uint8_t, 40> {
                             0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x00, 0x01,
                             0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                             0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                             0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0xff,
                         });
        write_input_data(
            1, std::array<uint16_t, 40> { 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3,
                                          0xfff2, 0xfff1, 0x0000, 0x0001, 0x0002, 0x0003,
                                          0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009,
                                          0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                                          0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021,
                                          0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
                                          0x0028, 0x0029, 0x0030, 0xffff });
        write_input_data(2,
                         std::array<uint32_t, 40> {
                             0xfffffff8, 0xfffffff7, 0xfffffff6, 0xfffffff5, 0xfffffff4,
                             0xfffffff3, 0xfffffff2, 0xfffffff1, 0x00000000, 0x00000001,
                             0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000006,
                             0x00000007, 0x00000008, 0x00000009, 0x00000010, 0x00000011,
                             0x00000012, 0x00000013, 0x00000014, 0x00000015, 0x00000016,
                             0x00000017, 0x00000018, 0x00000019, 0x00000020, 0x00000021,
                             0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000026,
                             0x00000027, 0x00000028, 0x00000029, 0x00000030, 0xffffffff,
                         });
        write_input_data(3,
                         std::array<uint64_t, 40> {
                             0xfffffffffffffff8, 0xfffffffffffffff7, 0xfffffffffffffff6,
                             0xfffffffffffffff5, 0xfffffffffffffff4, 0xfffffffffffffff3,
                             0xfffffffffffffff2, 0xfffffffffffffff1, 0x0000000000000000,
                             0x0000000000000001, 0x0000000000000002, 0x0000000000000003,
                             0x0000000000000004, 0x0000000000000005, 0x0000000000000006,
                             0x0000000000000007, 0x0000000000000008, 0x0000000000000009,
                             0x0000000000000010, 0x0000000000000011, 0x0000000000000012,
                             0x0000000000000013, 0x0000000000000014, 0x0000000000000015,
                             0x0000000000000016, 0x0000000000000017, 0x0000000000000018,
                             0x0000000000000019, 0x0000000000000020, 0x0000000000000021,
                             0x0000000000000022, 0x0000000000000023, 0x0000000000000024,
                             0x0000000000000025, 0x0000000000000026, 0x0000000000000027,
                             0x0000000000000028, 0x0000000000000029, 0x0000000000000030,
                             0xffffffffffffffff,
                         });
        // Change the permissions of the second page so that any accesses to it will
        // fault.
        mmap(region_start_addr(4), 8 * 4096, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    }

    ~input_data_t()
    {
        munmap(data, DATA_SIZE);
    }

    const void *
    region_start_addr(size_t offset) const
    {
        return &static_cast<char *>(data)[1024 * offset];
    }

    void *
    region_start_addr(size_t offset)
    {
        return &static_cast<char *>(data)[1024 * offset];
    }

    const void *
    base_addr_for_data_size(element_size_t element_size) const
    {
        return &static_cast<char *>(data)[base_offset_for_data_size(element_size)];
    }

    void *
    base_addr_for_data_size(element_size_t element_size)
    {
        return &static_cast<char *>(data)[base_offset_for_data_size(element_size)];
    }

private:
    static size_t
    base_offset_for_data_size(element_size_t element_size)
    {
        size_t offset = 0;
        switch (element_size) {
        case element_size_t::BYTE: offset = 0; break;
        case element_size_t::HALF: offset = 1; break;
        case element_size_t::SINGLE: offset = 2; break;
        case element_size_t::DOUBLE: offset = 3; break;
        }
        // The base address is set to the 8th element in the region.
        return (1024 * offset) + (static_cast<size_t>(element_size) * 8);
    }

    template <typename T>
    void
    write_input_data(size_t offset, const std::array<T, 40> &input_data)
    {
        memcpy(region_start_addr(offset), input_data.data(),
               input_data.size() * sizeof(T));
    }

    void *data;
};

#if defined(__ARM_FEATURE_SVE)
/*
 * Expands to a string literal containing assembly code that can be included in
 * an asm {} statement using string literal concatenation.
 *
 * For example SAVE_OR_RESTORE_SINGLE_REGISTER(ldr, z, 5, mem_base_ptr)
 * produces: "ldr z5, [%mem_base_ptr], #5, mul vl]\n" The empty string at the
 * beginning "" is necessary to stop clang-format treating #op as a preprocessor
 * directive.
 */
#    define SAVE_OR_RESTORE_SINGLE_REGISTER(op, reg_type, num, base) \
        "" #op " " #reg_type #num ", [%[" #base "], #" #num ", mul vl]\n"

#    define SAVE_OR_RESTORE_Z_REGISTERS(op, base)        \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 0, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 1, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 2, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 3, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 4, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 5, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 6, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 7, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 8, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 9, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 10, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 11, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 12, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 13, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 14, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 15, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 16, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 17, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 18, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 19, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 20, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 21, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 22, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 23, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 24, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 25, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 26, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 27, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 28, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 29, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 30, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, z, 31, base)

#    define SAVE_OR_RESTORE_P_REGISTERS(op, base)        \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 0, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 1, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 2, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 3, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 4, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 5, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 6, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 7, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 8, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 9, base)  \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 10, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 11, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 12, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 13, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 14, base) \
        SAVE_OR_RESTORE_SINGLE_REGISTER(op, p, 15, base)

#    define SAVE_Z_REGISTERS(base) SAVE_OR_RESTORE_Z_REGISTERS(str, base)
#    define RESTORE_Z_REGISTERS(base) SAVE_OR_RESTORE_Z_REGISTERS(ldr, base)

#    define SAVE_P_REGISTERS(base) SAVE_OR_RESTORE_P_REGISTERS(str, base)
#    define RESTORE_P_REGISTERS(base) SAVE_OR_RESTORE_P_REGISTERS(ldr, base)

// Handy short hand to list all Z registers in an asm {} statment clobber list.
#    define ALL_Z_REGS                                                                   \
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "z9", "z10", "z11", "z12", \
            "z13", "z14", "z15", "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23", \
            "z24", "z25", "z26", "z27", "z28", "z29", "z30", "z31"

// Handy short hand to list all P registers in an asm {} statment clobber list.
#    define ALL_P_REGS                                                                   \
        "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11", "p12", \
            "p13", "p14", "p15"

test_result_t
test_ld1_scalar_plus_vector()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_vector_test_case_t::test_ptrs_t &ptrs) {                     \
            asm(/* clang-format off */                                                  \
            RESTORE_Z_REGISTERS(z_restore_base)                                     \
            RESTORE_P_REGISTERS(p_restore_base)                                     \
            ld_instruction "\n"                                                     \
            SAVE_Z_REGISTERS(z_save_base)                                           \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */         \
                :                                                                   \
                : [base] "r"(ptrs.base), [z_restore_base] "r"(ptrs.z_restore_base), \
                  [z_save_base] "r"(ptrs.z_save_base),                              \
                  [p_restore_base] "r"(ptrs.p_restore_base),                        \
                  [p_save_base] "r"(ptrs.p_save_base)                               \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                                \
        }

    input_data_t input_data;
    return run_tests({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zm},
         *     Expected output data,
         *     Offset data (value for zm),
         *     Base pointer (value for Xn),
         * },
         */
        // LD1B instructions.
        {
            "ld1b 32bit unscaled offset uxtw",
            TEST_FUNC("ld1b z0.s, p7/z, [%[base], z31.s, uxtw]"),
            { /*zt=*/0, /*pg=*/7, /*zm=*/31 },
            std::array<uint32_t, 4> { 0x00, 0x01, 0x07, 0x10 },
            std::array<uint32_t, 4> { 0, 1, 7, 10 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b 32bit unscaled offset sxtw",
            TEST_FUNC("ld1b z1.s, p6/z, [%[base], z30.s, sxtw]"),
            { /*zt=*/1, /*pg=*/6, /*zm=*/30 },
            std::array<uint32_t, 4> { 0x00, 0xF1, 0x18, 0x27 },
            std::array<int32_t, 4> { 0, -1, 18, 27 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1b z2.d, p5/z, [%[base], z29.d, uxtw]"),
            { /*zt=*/2, /*pg=*/5, /*zm=*/29 },
            std::array<uint64_t, 2> { 0x01, 0x22 },
            std::array<uint64_t, 2> { 1, 22 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1b z3.d, p4/z, [%[base], z28.d, sxtw]"),
            { /*zt=*/3, /*pg=*/4, /*zm=*/28 },
            std::array<uint64_t, 2> { 0xF2, 0x29 },
            std::array<int64_t, 2> { -2, 29 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b 64bit unscaled offset",
            TEST_FUNC("ld1b z4.d, p3/z, [%[base], z27.d]"),
            { /*zt=*/4, /*pg=*/3, /*zm=*/27 },
            std::array<uint64_t, 2> { 0x09, 0x28 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1b z30.d, p3/z, [%[base], z30.d]"),
            { /*zt=*/30, /*pg=*/3, /*zm=*/30 },
            std::array<uint64_t, 2> { 0x09, 0x28 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SB instructions.
        {
            "ld1sb 32bit unscaled offset uxtw",
            TEST_FUNC("ld1sb z5.s, p2/z, [%[base], z26.s, uxtw]"),
            { /*zt=*/5, /*pg=*/2, /*zm=*/26 },
            std::array<int32_t, 4> { 0x00, -1, 0x23, 0x30 },
            std::array<uint32_t, 4> { 0, 31, 23, 30 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb 32bit unscaled offset sxtw",
            TEST_FUNC("ld1sb z6.s, p1/z, [%[base], z25.s, sxtw]"),
            { /*zt=*/6, /*pg=*/1, /*zm=*/25 },
            std::array<int32_t, 4> { 0x01, -15, 0x11, 0x24 },
            std::array<int32_t, 4> { 1, -1, 11, 24 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sb z7.d, p0/z, [%[base], z24.d, uxtw]"),
            { /*zt=*/7, /*pg=*/0, /*zm=*/24 },
            std::array<int64_t, 2> { 0x01, -1 },
            std::array<uint64_t, 2> { 1, 31 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sb z8.d, p1/z, [%[base], z23.d, sxtw]"),
            { /*zt=*/8, /*pg=*/1, /*zm=*/23 },
            std::array<int64_t, 2> { -14, 0x29 },
            std::array<int64_t, 2> { -2, 29 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb 64bit unscaled offset",
            TEST_FUNC("ld1sb z9.d, p2/z, [%[base], z22.d]"),
            { /*zt=*/9, /*pg=*/2, /*zm=*/22 },
            std::array<int64_t, 2> { -1, 0x09 },
            std::array<uint64_t, 2> { 31, 9 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb 64bit unscaled offset",
            TEST_FUNC("ld1sb z17.d, p7/z, [%[base], z17.d]"),
            { /*zt=*/17, /*pg=*/7, /*zm=*/17 },
            std::array<int64_t, 2> { -1, 0x09 },
            std::array<uint64_t, 2> { 31, 9 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1H instructions.
        {
            "ld1h 32bit scaled offset uxtw",
            TEST_FUNC("ld1h z10.s, p3/z, [%[base], z21.s, uxtw #1]"),
            { /*zt=*/10, /*pg=*/3, /*zm=*/21 },
            std::array<uint32_t, 4> { 0x01, 0x10, 0x23, 0x26 },
            std::array<uint32_t, 4> { 1, 10, 23, 26 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 32bit scaled offset sxtw",
            TEST_FUNC("ld1h z11.s, p4/z, [%[base], z20.s, sxtw #1]"),
            { /*zt=*/11, /*pg=*/4, /*zm=*/20 },
            std::array<uint32_t, 4> { 0xFFF3, 0x07, 0x16, 0x30 },
            std::array<int32_t, 4> { -3, 7, 16, 30 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1h z12.d, p5/z, [%[base], z19.d, uxtw #1]"),
            { /*zt=*/12, /*pg=*/5, /*zm=*/19 },
            std::array<uint64_t, 2> { 0x08, 0x28 },
            std::array<uint64_t, 2> { 8, 28 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1h z13.d, p6/z, [%[base], z18.d, sxtw #1]"),
            { /*zt=*/13, /*pg=*/6, /*zm=*/18 },
            std::array<uint64_t, 2> { 0xFFF4, 0x24 },
            std::array<int64_t, 2> { -4, 24 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1h z14.d, p7/z, [%[base], z17.d, uxtw]"),
            { /*zt=*/14, /*pg=*/7, /*zm=*/17 },
            std::array<uint64_t, 2> { 0x0403, 0x2322 },
            std::array<uint64_t, 2> { 3, 22 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1h z15.d, p6/z, [%[base], z16.d, sxtw]"),
            { /*zt=*/15, /*pg=*/6, /*zm=*/16 },
            std::array<uint64_t, 2> { 0x0100, 0xF4F5 },
            std::array<int64_t, 2> { 0, -5 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h 32bit unscaled offset uxtw",
            TEST_FUNC("ld1h z16.s, p5/z, [%[base], z15.s, uxtw #1]"),
            { /*zt=*/16, /*pg=*/5, /*zm=*/15 },
            std::array<uint32_t, 4> { 0x01, 0x10, 0x23, 0x30 },
            std::array<uint32_t, 4> { 1, 10, 23, 30 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 32bit unscaled offset sxtw",
            TEST_FUNC("ld1h z17.s, p4/z, [%[base], z14.s, sxtw #1]"),
            { /*zt=*/17, /*pg=*/4, /*zm=*/14 },
            std::array<uint32_t, 4> { 0x00, 0xFFF6, 0x18, 0x27 },
            std::array<int32_t, 4> { 0, -6, 18, 27 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 64bit scaled offset",
            TEST_FUNC("ld1h z18.d, p3/z, [%[base], z13.d, lsl #1]"),
            { /*zt=*/18, /*pg=*/3, /*zm=*/13 },
            std::array<uint64_t, 2> { 0x03, 0x14 },
            std::array<uint64_t, 2> { 3, 14 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h 64bit unscaled offset",
            TEST_FUNC("ld1h z19.d, p2/z, [%[base], z12.d]"),
            { /*zt=*/19, /*pg=*/2, /*zm=*/12 },
            std::array<uint64_t, 2> { 0x1009, 0x2928 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1h z25.d, p5/z, [%[base], z25.d]"),
            { /*zt=*/25, /*pg=*/5, /*zm=*/25 },
            std::array<uint64_t, 2> { 0x1009, 0x2928 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SH instructions.
        {
            "ld1sh 32bit scaled offset uxtw",
            TEST_FUNC("ld1sh z20.s, p1/z, [%[base], z11.s, uxtw #1]"),
            { /*zt=*/20, /*pg=*/1, /*zm=*/11 },
            std::array<int32_t, 4> { 0x00, 0x07, 0x16, -1 },
            std::array<uint32_t, 4> { 0, 7, 16, 31 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 32bit scaled offset sxtw",
            TEST_FUNC("ld1sh z21.s, p0/z, [%[base], z10.s, sxtw #1]"),
            { /*zt=*/21, /*pg=*/0, /*zm=*/10 },
            std::array<int32_t, 4> { -13, 0x01, 0x10, 0x30 },
            std::array<int32_t, 4> { -3, 1, 10, 30 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1sh z22.d, p1/z, [%[base], z9.d, uxtw #1]"),
            { /*zt=*/22, /*pg=*/1, /*zm=*/9 },
            std::array<int64_t, 2> { 0x00, -1 },
            std::array<uint64_t, 2> { 0, 31 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1sh z23.d, p2/z, [%[base], z8.d, sxtw #1]"),
            { /*zt=*/23, /*pg=*/2, /*zm=*/8 },
            std::array<int64_t, 2> { -12, 0x14 },
            std::array<int64_t, 2> { -4, 14 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sh z24.d, p3/z, [%[base], z7.d, uxtw]"),
            { /*zt=*/24, /*pg=*/3, /*zm=*/7 },
            std::array<int64_t, 2> { 0x0201, -208 },
            std::array<uint64_t, 2> { 1, 30 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sh z25.d, p4/z, [%[base], z6.d, sxtw]"),
            { /*zt=*/25, /*pg=*/4, /*zm=*/6 },
            std::array<int64_t, 2> { -2827, 0x3029 },
            std::array<int64_t, 2> { -5, 29 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh 32bit unscaled offset uxtw",
            TEST_FUNC("ld1sh z26.s, p5/z, [%[base], z5.s, uxtw #1]"),
            { /*zt=*/26, /*pg=*/5, /*zm=*/5 },
            std::array<int32_t, 4> { 0x05, 0x15, 0x25, -1 },
            std::array<uint32_t, 4> { 5, 15, 25, 31 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 32bit unscaled offset sxtw",
            TEST_FUNC("ld1sh z27.s, p6/z, [%[base], z4.s, sxtw #1]"),
            { /*zt=*/27, /*pg=*/6, /*zm=*/4 },
            std::array<int32_t, 4> { 0x06, 0x16, -10, 0x26 },
            std::array<int32_t, 4> { 6, 16, -6, 26 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 64bit scaled offset",
            TEST_FUNC("ld1sh z28.d, p7/z, [%[base], z3.d, lsl #1]"),
            { /*zt=*/28, /*pg=*/7, /*zm=*/3 },
            std::array<int64_t, 2> { 0x09, -1 },
            std::array<uint64_t, 2> { 9, 31 },
            input_data.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh 64bit unscaled offset",
            TEST_FUNC("ld1sh z29.d, p6/z, [%[base], z2.d]"),
            { /*zt=*/29, /*pg=*/6, /*zm=*/2 },
            std::array<int64_t, 2> { 0x0403, -208 },
            std::array<uint64_t, 2> { 3, 30 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1sh z0.d, p0/z, [%[base], z0.d]"),
            { /*zt=*/0, /*pg=*/0, /*zm=*/0 },
            std::array<int64_t, 2> { 0x0403, -208 },
            std::array<uint64_t, 2> { 3, 30 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1W instructions.
        {
            "ld1w 32bit scaled offset uxtw",
            TEST_FUNC("ld1w z30.s, p5/z, [%[base], z1.s, uxtw #2]"),
            { /*zt=*/30, /*pg=*/5, /*zm=*/1 },
            std::array<uint32_t, 4> { 0x00, 0x07, 0x17, 0x27 },
            std::array<uint32_t, 4> { 0, 7, 17, 27 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w 32bit scaled offset sxtw",
            TEST_FUNC("ld1w z31.s, p4/z, [%[base], z0.s, sxtw #2]"),
            { /*zt=*/31, /*pg=*/4, /*zm=*/0 },
            std::array<uint32_t, 4> { 0xFFFFFFF7, 0x07, 0x17, 0x27 },
            std::array<int32_t, 4> { -7, 7, 17, 27 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1w z0.d, p3/z, [%[base], z1.d, uxtw #2]"),
            { /*zt=*/0, /*pg=*/3, /*zm=*/1 },
            std::array<uint64_t, 2> { 0x18, 0x28 },
            std::array<uint64_t, 2> { 18, 28 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1w z2.d, p2/z, [%[base], z3.d, sxtw #2]"),
            { /*zt=*/2, /*pg=*/2, /*zm=*/3 },
            std::array<uint64_t, 2> { 0xFFFFFFF8, 0x08 },
            std::array<int64_t, 2> { -8, 8 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1w z4.d, p1/z, [%[base], z5.d, uxtw]"),
            { /*zt=*/4, /*pg=*/1, /*zm=*/5 },
            std::array<uint64_t, 2> { 0x04030201, 0x25242322 },
            std::array<uint64_t, 2> { 1, 22 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1w z6.d, p0/z, [%[base], z7.d, sxtw]"),
            { /*zt=*/6, /*pg=*/0, /*zm=*/7 },
            std::array<uint64_t, 2> { 0x020100F1, 0x30292827 },
            std::array<int64_t, 2> { -1, 27 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w 32bit unscaled offset uxtw",
            TEST_FUNC("ld1w z8.s, p1/z, [%[base], z9.s, uxtw]"),
            { /*zt=*/8, /*pg=*/1, /*zm=*/9 },
            std::array<uint32_t, 4> { 0x03020100, 0x05040302, 0x15141312, 0x25242322 },
            std::array<int32_t, 4> { 0, 2, 12, 22 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w 32bit unscaled offset sxtw",
            TEST_FUNC("ld1w z10.s, p2/z, [%[base], z11.s, sxtw]"),
            { /*zt=*/10, /*pg=*/2, /*zm=*/11 },
            std::array<uint32_t, 4> { 0x0100F1F2, 0x05040302, 0x15141312, 0x25242322 },
            std::array<int32_t, 4> { -2, 2, 12, 22 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w 64bit scaled offset",
            TEST_FUNC("ld1w z12.d, p3/z, [%[base], z13.d, lsl #2]"),
            { /*zt=*/12, /*pg=*/3, /*zm=*/13 },
            std::array<uint64_t, 2> { 0x03, 0x14 },
            std::array<uint64_t, 2> { 3, 14 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w 64bit unscaled offset",
            TEST_FUNC("ld1w z14.d, p4/z, [%[base], z15.d]"),
            { /*zt=*/14, /*pg=*/4, /*zm=*/15 },
            std::array<uint64_t, 2> { 0x06050403, 0x17161514 },
            std::array<uint64_t, 2> { 3, 14 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1w z5.d, p5/z, [%[base], z5.d]"),
            { /*zt=*/5, /*pg=*/5, /*zm=*/5 },
            std::array<uint64_t, 2> { 0x06050403, 0x17161514 },
            std::array<uint64_t, 2> { 3, 14 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SW instructions.
        {
            "ld1sw 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1sw z16.d, p5/z, [%[base], z17.d, uxtw #2]"),
            { /*zt=*/16, /*pg=*/5, /*zm=*/17 },
            std::array<int64_t, 2> { -1, 0x10 },
            std::array<uint64_t, 2> { 31, 10 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1sw z18.d, p6/z, [%[base], z19.d, sxtw #2]"),
            { /*zt=*/18, /*pg=*/6, /*zm=*/19 },
            std::array<int64_t, 2> { -8, 0x16 },
            std::array<int64_t, 2> { -8, 16 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sw z20.d, p7/z, [%[base], z21.d, uxtw]"),
            { /*zt=*/20, /*pg=*/7, /*zm=*/21 },
            std::array<int64_t, 2> { 0x04030201, -13620952 },
            std::array<uint64_t, 2> { 1, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sw z22.d, p6/z, [%[base], z23.d, sxtw]"),
            { /*zt=*/22, /*pg=*/6, /*zm=*/23 },
            std::array<int64_t, 2> { 0x11100908, -168364040 },
            std::array<int64_t, 2> { 8, -8 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw 64bit scaled offset",
            TEST_FUNC("ld1sw z24.d, p5/z, [%[base], z25.d, lsl #2]"),
            { /*zt=*/24, /*pg=*/5, /*zm=*/25 },
            std::array<int64_t, 2> { -1, 0x28 },
            std::array<uint64_t, 2> { 31, 28 },
            input_data.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw 64bit unscaled offset",
            TEST_FUNC("ld1sw z26.d, p4/z, [%[base], z27.d]"),
            { /*zt=*/26, /*pg=*/4, /*zm=*/27 },
            std::array<int64_t, 2> { 0x12111009, -13620952 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1sw z10.d, p5/z, [%[base], z10.d]"),
            { /*zt=*/10, /*pg=*/5, /*zm=*/10 },
            std::array<int64_t, 2> { 0x12111009, -13620952 },
            std::array<uint64_t, 2> { 9, 28 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1D
        {
            "ld1d 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1d z28.d, p3/z, [%[base], z29.d, uxtw #3]"),
            { /*zt=*/28, /*pg=*/3, /*zm=*/29 },
            std::array<uint64_t, 2> { 0x15, 0x25 },
            std::array<uint64_t, 2> { 15, 25 },
            input_data.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1d z30.d, p2/z, [%[base], z31.d, sxtw #3]"),
            { /*zt=*/30, /*pg=*/2, /*zm=*/31 },
            std::array<uint64_t, 2> { 0x08, 0xFFFFFFFFFFFFFFF3 },
            std::array<int64_t, 2> { 8, -3 },
            input_data.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1d z31.d, p1/z, [%[base], z30.d, uxtw]"),
            { /*zt=*/31, /*pg=*/1, /*zm=*/30 },
            std::array<uint64_t, 2> { 0x2019181716151413, 0x3029282726252423 },
            std::array<uint64_t, 2> { 13, 23 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1d z29.d, p0/z, [%[base], z28.d, sxtw]"),
            { /*zt=*/29, /*pg=*/0, /*zm=*/28 },
            std::array<uint64_t, 2> { 0x2120191817161514, 0x03020100F1F2F3F4 },
            std::array<int64_t, 2> { 14, -4 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d 64bit scaled offset",
            TEST_FUNC("ld1d z27.d, p1/z, [%[base], z26.d, lsl #3]"),
            { /*zt=*/27, /*pg=*/1, /*zm=*/26 },
            std::array<uint64_t, 2> { 0x00, 0x10 },
            std::array<uint64_t, 2> { 0, 10 },
            input_data.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d 64bit unscaled offset",
            TEST_FUNC("ld1d z25.d, p2/z, [%[base], z24.d]"),
            { /*zt=*/25, /*pg=*/2, /*zm=*/24 },
            std::array<uint64_t, 2> { 0x020100F1F2F3F4F5, 0x1716151413121110 },
            std::array<int64_t, 2> { -5, 10 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1d z15.d, p5/z, [%[base], z15.d]"),
            { /*zt=*/15, /*pg=*/5, /*zm=*/15 },
            std::array<uint64_t, 2> { 0x020100F1F2F3F4F5, 0x1716151413121110 },
            std::array<int64_t, 2> { -5, 10 },
            input_data.base_addr_for_data_size(element_size_t::BYTE),
        },
    });
#    undef TEST_FUNC
}
#endif // defined(__ARM_FEATURE_SVE)

} // namespace

int
main(int argc, char **argv)
{
#if defined(__ARM_FEATURE_SVE)
    test_ld1_scalar_plus_vector();
#endif

    return 0;
}
