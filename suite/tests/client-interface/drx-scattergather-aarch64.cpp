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

#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstddef>
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

/* DynamoRIO supports vector lengths up to 512-bits */
constexpr size_t MAX_SUPPORTED_VL_BYTES = 64;

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

/* Lists of valid 128-bit vl predicate register values for different element sizes.
 * The list for double are exhaustive but exhaustive lists for the other sizes would be
 * too long so we have a cherry-picked subset that should give us good coverage.
 */
const std::map<element_size_t, std::vector<predicate_reg_value128_t>> ALL_PREDICATES {
    { element_size_t::BYTE,
      {
          0x0000,         // All inactive
          0xFFFF,         // All active
          0x5555, 0xaaaa, // Checkerboard pattern
      } },
    { element_size_t::HALF,
      {
          0x0000,         // All inactive
          0x5555,         // All active
          0x1111, 0x4444, // Checkerboard pattern
      } },
    { element_size_t::SINGLE,
      {
          0x0000,         // All inactive
          0x1111,         // All active
          0x0101, 0x1010, // Checkerboard pattern
      } },
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
 * Set all the elements of data that are inactive in the mask to 0.
 */
void
apply_predicate_mask(std::vector<uint8_t> &data, predicate_reg_value128_t mask,
                     element_size_t element_size)
{
    const auto element_size_bytes = static_cast<size_t>(element_size);
    const auto num_vector_elements = data.size() / element_size_bytes;
    const auto num_mask_elements = TEST_VL_BYTES / element_size_bytes;
    for (size_t i = 0; i < num_vector_elements; i++) {
        if (!element_is_active(i % num_mask_elements, mask, element_size)) {
            // Element is inactive, set it to 0.
            memset(&data[element_size_bytes * i], 0, element_size_bytes);
        }
    }
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
    assert(static_cast<size_t>(static_cast<int>(value.size)) == value.size);
    for (int i = static_cast<int>(value.size) - 1; i >= 0; i--) {
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
    assert(static_cast<size_t>(static_cast<int>(value.size)) == value.size);
    for (int byte_i = static_cast<int>(value.size) - 1; byte_i >= 0; byte_i--) {
        for (int bit = 7; bit >= 0; bit--) {
            if (TESTALL(1 << bit, value.data[byte_i]))
                print(" 1");
            else
                print(" 0");
        }
    }
}

/*
 * Overloaded functions for printing scalar values from template functions.
 */
void
print_scalar(uint8_t value)
{
    print("0x%02" PRIx8, value);
}

void
print_scalar(uint16_t value)
{
    print("0x%04" PRIx16, value);
}

void
print_scalar(uint32_t value)
{
    print("0x%08" PRIx32, value);
}

void
print_scalar(uint64_t value)
{
    print("0x%016" PRIx64, value);
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

template <typename TEST_PTRS_T> struct test_case_base_t {
    std::string name_; // Unique name for this test printed when the test is run.

    using test_ptrs_t = TEST_PTRS_T;
    using test_func_t = std::function<void(test_ptrs_t &)>;
    test_func_t run_test_;

    element_size_t element_size_;
    unsigned governing_p_reg_;

    test_result_t test_status_;

    void
    test_failed()
    {
        if (test_status_ == PASS) {
            test_status_ = FAIL;
            print("FAIL\n");
        }
    }

    test_case_base_t(std::string name, test_func_t func, unsigned governing_p_reg,
                     element_size_t element_size)
        : name_(std::move(name))
        , run_test_(std::move(func))
        , governing_p_reg_(governing_p_reg)
        , element_size_(element_size)
    {
        assert(governing_p_reg_ < NUM_P_REGS);
    }

    // Set the values of the SVE registers before the test function is run.
    virtual void
    setup(sve_register_file_t &register_values) = 0;

    virtual void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) = 0;

    virtual test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) = 0;

    test_result_t
    run_test_case()
    {
        print("%s: ", name_.c_str());
        test_status_ = PASS;

        test_register_data_t register_data;
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            register_data.before.set_z_register_value(i, UNINITIALIZED_VECTOR);
        }
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            register_data.before.set_p_register_value(i, UNINITIALIZED_PREDICATE);
        }

        test_ptrs_t ptrs = create_test_ptrs(register_data);

        const size_t num_elements = TEST_VL_BYTES / static_cast<size_t>(element_size_);

        const auto &predicates = ALL_PREDICATES.at(element_size_);
        for (const auto &pred : predicates) {
            /* TODO i#5036: Test faulting behavior. */

            register_data.before.set_p_register_value(governing_p_reg_, pred);
            setup(register_data.before);

            run_test_(ptrs);

            check_output(pred, register_data);
        }
        if (test_status_ == PASS)
            print("PASS\n");

        return test_status_;
    }

    void
    check_z_reg(unsigned reg_num, const test_register_data_t &register_data)
    {
        const auto before = register_data.before.get_z_register_value(reg_num);
        const auto after = register_data.after.get_z_register_value(reg_num);
        if (before != after) {
            test_failed();
            print("z%u has been corrupted:\n", reg_num);
            print("before: ");
            print_vector(before);
            print("\nafter:  ");
            print_vector(after);
            print("\n");
        }
    }

    void
    check_p_reg(unsigned reg_num, const test_register_data_t &register_data)
    {
        const auto before = register_data.before.get_p_register_value(reg_num);
        const auto after = register_data.after.get_p_register_value(reg_num);
        if (before != after) {
            test_failed();
            print("p%u has been corrupted:\n", reg_num);
            print("before: ");
            print_predicate(before);
            print("\nafter:  ");
            print_predicate(after);
            print("\n");
        }
    }

    // Captures an expected memory output value of a stored element so we can check that
    // the store was performed correctly.
    template <typename VALUE_T> struct expected_value_t {
        std::ptrdiff_t offset; // Offset from the base pointer. Might be negative.
        VALUE_T value;
    };
    union expected_values_t {
        std::array<expected_value_t<uint8_t>, 2> u8x2;
        std::array<expected_value_t<uint8_t>, 4> u8x4;
        std::array<expected_value_t<uint16_t>, 2> u16x2;
        std::array<expected_value_t<uint16_t>, 4> u16x4;
        std::array<expected_value_t<uint32_t>, 2> u32x2;
        std::array<expected_value_t<uint32_t>, 4> u32x4;
        std::array<expected_value_t<uint64_t>, 2> u64x2;

        template <typename OFFSET_T>
        expected_values_t(std::array<OFFSET_T, 2> offsets, element_size_t value_size)
        {
            // We can predict the expected value for each offset because the src register
            // is always set to the same value before we execute the store instruction.
            // The value that these stores write is the lower part of a 64-bit vector
            // element.
            // Src register value: ||15|14|13|12|11|10|09|08||07|06|05|04|03|02|01|00||
            // Byte values         ||                     AA||                     BB||
            // Half values         ||                  AA|AA||                  BB|BB||
            // Word values         ||            AA|AA|AA|AA||            BB|BB|BB|BB||
            // Double values       ||AA|AA|AA|AA|AA|AA|AA|AA||BB|BB|BB|BB|BB|BB|BB|BB||
            switch (value_size) {
#define CASE(size, member, val0, val1)                                      \
    case element_size_t::size:                                              \
        member = { { { static_cast<std::ptrdiff_t>(offsets[0]), val0 },     \
                     { static_cast<std::ptrdiff_t>(offsets[1]), val1 } } }; \
        break
                CASE(BYTE, u8x2, 0x00, 0x08);
                CASE(HALF, u16x2, 0x0100, 0x0908);
                CASE(SINGLE, u32x2, 0x03020100, 0x11100908);
                CASE(DOUBLE, u64x2, 0x0706050403020100, 0x1514131211100908);
#undef CASE
            }
        }

        template <typename OFFSET_T>
        expected_values_t(std::array<OFFSET_T, 4> offsets, element_size_t value_size)
        {
            // We can predict the expected value for each offset because the src register
            // is always set to the same value before we execute the store instruction.
            // The value that these stores write is the lower part of a 32-bit vector
            // element.
            // Src register value: ||15|14|13|12||11|10|09|08||07|06|05|04||03|02|01|00||
            // Byte values         ||         AA||         BB||         CC||         DD||
            // Half values         ||      AA|AA||      BB|BB||      CC|CC||      DD|DD||
            // Word values         ||AA|AA|AA|AA||BB|BB|BB|BB||CC|CC|CC|CC||DD|DD|DD|DD||
            assert(value_size != element_size_t::DOUBLE);
            switch (value_size) {
#define CASE(size, member, val0, val1, val2, val3)                          \
    case element_size_t::size:                                              \
        member = { { { static_cast<std::ptrdiff_t>(offsets[0]), val0 },     \
                     { static_cast<std::ptrdiff_t>(offsets[1]), val1 },     \
                     { static_cast<std::ptrdiff_t>(offsets[2]), val2 },     \
                     { static_cast<std::ptrdiff_t>(offsets[3]), val3 } } }; \
        break
                CASE(BYTE, u8x4, 0x00, 0x04, 0x08, 0x12);
                CASE(HALF, u16x4, 0x0100, 0x0504, 0x0908, 0x1312);
                CASE(SINGLE, u32x4, 0x03020100, 0x07060504, 0x11100908, 0x15141312);
#undef CASE
            }
        }
    };

    template <typename VALUE_T>
    const void *
    get_scaled_offset(const void *base_ptr, std::ptrdiff_t offset)
    {
        return &static_cast<const VALUE_T *>(base_ptr)[offset];
    }

    template <typename VALUE_T, size_t NUM_ELEMENTS>
    void
    check_expected_values(
        const std::array<expected_value_t<VALUE_T>, NUM_ELEMENTS> &expectations,
        predicate_reg_value128_t mask,
        const std::array<const void *, NUM_ELEMENTS> base_ptrs, bool scaled)
    {
        for (size_t element = 0; element < expectations.size(); element++) {
            const auto &expectation = expectations[element];
            const void *base_ptr = base_ptrs[element];

            VALUE_T value;
            if (scaled) {
                value = *static_cast<const VALUE_T *>(
                    get_scaled_offset<VALUE_T>(base_ptr, expectation.offset));
            } else {
                value = *static_cast<const VALUE_T *>(
                    get_scaled_offset<uint8_t>(base_ptr, expectation.offset));
            }

            const bool is_active = element_is_active(element, mask, element_size_);

            const auto expected_value =
                is_active ? expectation.value : static_cast<VALUE_T>(0xABABABABABABABAB);

            if (expected_value != value) {
                // If any offsets alias then the value from the highest active element is
                // written, so if we find a mismatch we need to make sure there isn't
                // another element writing to the same location before we declare it a
                // failure.
                bool written_by_another_element = false;

                // First we check whether there are any active higher elements that have
                // the same offset.
                for (size_t higher_element = element + 1;
                     higher_element < expectations.size(); higher_element++) {
                    if (expectations[higher_element].offset == expectation.offset &&
                        element_is_active(higher_element, mask, element_size_)) {
                        written_by_another_element = true;
                        break;
                    }
                }

                // Second we check if this element is inactive, was there an active lower
                // element with the same offset.
                if (!is_active && !written_by_another_element) {
                    for (size_t lower_element = 0; lower_element < element;
                         lower_element++) {
                        if (expectations[lower_element].offset == expectation.offset &&
                            element_is_active(lower_element, mask, element_size_)) {
                            written_by_another_element = true;
                            break;
                        }
                    }
                }

                if (!written_by_another_element) {
                    test_failed();
                    print("\nat offset: %i", expectation.offset);
                    print("\nexpected:  ");
                    print_scalar(expected_value);
                    print("\nactual:    ");
                    print_scalar(value);
                    print("\n");
                }
            }
        }
    }
};

struct basic_test_ptrs_t {
    const void *z_restore_base; // Base address for initializing Z registers.
    const void *p_restore_base; // Base address for initializing P registers.
    void *z_save_base;          // Base address to save Z registers to after test
                                // instruction
    void *p_save_base;          // Base address to save P registers to after test
                                // instruction
};

struct test_ptrs_with_base_ptr_t : public basic_test_ptrs_t {
    void *base; // Base address used for the test instruction.

    test_ptrs_with_base_ptr_t(void *base_, const void *z_restore_base_,
                              const void *p_restore_base_, void *z_save_base_,
                              void *p_save_base_)
        : basic_test_ptrs_t { z_restore_base_, p_restore_base_, z_save_base_,
                              p_save_base_ }
        , base(base_)
    {
    }
};

struct scalar_plus_vector_test_case_base_t
    : public test_case_base_t<test_ptrs_with_base_ptr_t> {
    void *base_ptr_;

    scalar_plus_vector_test_case_base_t(std::string name, test_func_t func,
                                        unsigned governing_p_reg,
                                        element_size_t element_size, void *base_ptr)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func), governing_p_reg,
                                        element_size)
        , base_ptr_(base_ptr)
    {
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            base_ptr_,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

struct scalar_plus_vector_load_test_case_t : public scalar_plus_vector_test_case_base_t {
    vector_reg_value128_t reference_data_;
    vector_reg_value128_t offset_data__;

    struct registers_used_t {
        unsigned dest_z;
        unsigned governing_p;
        unsigned index_z;
    };
    registers_used_t registers_used_;

    template <typename ELEMENT_T, typename OFFSET_T>
    scalar_plus_vector_load_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<ELEMENT_T, TEST_VL_BYTES / sizeof(ELEMENT_T)> reference_data,
        std::array<OFFSET_T, TEST_VL_BYTES / sizeof(OFFSET_T)> offsets, void *base_ptr)
        : scalar_plus_vector_test_case_base_t(
              std::move(name), std::move(func), registers_used.governing_p,
              static_cast<element_size_t>(sizeof(ELEMENT_T)), base_ptr)
        , registers_used_(registers_used)
    {
        std::memcpy(reference_data_.data(), reference_data.data(),
                    reference_data_.size());
        std::memcpy(offset_data__.data(), offsets.data(), offset_data__.size());
    }

    virtual void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the offset register.
        register_values.set_z_register_value(registers_used_.index_z, offset_data__);
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        const auto vl_bytes = get_vl_bytes();

        std::vector<uint8_t> expected_output_data;
        expected_output_data.resize(vl_bytes);

        assert(reference_data_.size() == TEST_VL_BYTES);
        for (size_t i = 0; i < vl_bytes / TEST_VL_BYTES; i++) {
            memcpy(&expected_output_data[TEST_VL_BYTES * i], reference_data_.data(),
                   TEST_VL_BYTES);
        }
        apply_predicate_mask(expected_output_data, pred, element_size_);
        const scalable_reg_value_t expected_output {
            expected_output_data.data(),
            vl_bytes,
        };

        const auto output_value =
            register_data.after.get_z_register_value(registers_used_.dest_z);

        if (output_value != expected_output) {
            test_failed();
            print("predicate: ");
            print_predicate(
                register_data.before.get_p_register_value(registers_used_.governing_p));
            print("\nexpected:  ");
            print_vector(expected_output);
            print("\nactual:    ");
            print_vector(output_value);
            print("\n");
        }

        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            if (i == registers_used_.dest_z)
                continue;
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }
    }
};

template <typename TEST_CASE_T>
test_result_t
run_tests(std::vector<TEST_CASE_T> tests)
{
    test_result_t overall_status = PASS;

    for (auto &instr_test : tests) {
        if (instr_test.run_test_case() == FAIL) {
            overall_status = FAIL;
        }
    }

    return overall_status;
}

class test_memory_t {
public:
    static const size_t CHUNK_SIZE = 64 * 1024;
    static const size_t DATA_SIZE = 3 * CHUNK_SIZE;
    static const size_t REGION_SIZE = 16 * 1024;

    test_memory_t()
        : data(mmap(nullptr, DATA_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
    {
        assert(DATA_SIZE % sysconf(_SC_PAGE_SIZE) == 0);
        reset();

        // Change the permissions of chunks 0 and 2 so that any accesses to them will
        // fault.
        mmap(chunk_start_addr(0), CHUNK_SIZE, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        mmap(chunk_start_addr(2), CHUNK_SIZE, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    }

    ~test_memory_t()
    {
        munmap(data, DATA_SIZE);
    }

    void
    reset()
    {
        static constexpr uint8_t POISON_VALUE = 0xAB;
        memset(chunk_start_addr(1), POISON_VALUE, CHUNK_SIZE);
    }

    const void *
    chunk_start_addr(size_t chunk_offset) const
    {
        return &static_cast<char *>(data)[CHUNK_SIZE * chunk_offset];
    }

    void *
    chunk_start_addr(size_t chunk_offset)
    {
        return &static_cast<char *>(data)[CHUNK_SIZE * chunk_offset];
    }

    const char *
    region_start_addr(size_t region_offset) const
    {
        const auto byte_offset = CHUNK_SIZE + (REGION_SIZE * region_offset);
        return &static_cast<char *>(data)[byte_offset];
    }

    char *
    region_start_addr(size_t region_offset)
    {
        const auto byte_offset = CHUNK_SIZE + (REGION_SIZE * region_offset);
        return &static_cast<char *>(data)[byte_offset];
    }

protected:
    void *data;
};

class input_data_t : public test_memory_t {
public:
    input_data_t()
        : test_memory_t()
    {
        /*
         * We set up 3 64KiB chunks of memory to use as input data for load instruction
         * tests. The first and last chunks are set to fault when accessed, and the middle
         * chunk contains input data of different sizes.
         * +=====================================================+
         * | Chunk  | Byte off | Region off |                    |
         * +=====================================================+
         * | 0      |  0x00000 |        n/a | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +--------+----------+------------+--------------------+
         * | 1      |  0x10000 |          0 | 8-bit input data   |
         * |        |----------+------------+--------------------+
         * |        |  0x14000 |          1 | 16-bit input data  |
         * |        |----------+------------+--------------------+
         * |        |  0x18000 |          2 | 32-bit input data  |
         * |        |----------+------------+--------------------+
         * |        |  0x1C000 |          3 | 64-bit input data  |
         * +--------+----------+------------+--------------------+
         * | 2      |  0x20000 |        n/a | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +--------+----------+------------+--------------------+
         */

        write_input_data(0,
                         std::array<uint8_t, 32> {
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                             0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                             0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
                             0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1,
                         });
        write_input_data(1,
                         std::array<uint16_t, 32> {
                             0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006,
                             0x0007, 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013,
                             0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x0020,
                             0x0021, 0x0022, 0x0023, 0xfff8, 0xfff7, 0xfff6, 0xfff5,
                             0xfff4, 0xfff3, 0xfff2, 0xfff1,
                         });
        write_input_data(2,
                         std::array<uint32_t, 32> {
                             0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004,
                             0x00000005, 0x00000006, 0x00000007, 0x00000008, 0x00000009,
                             0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014,
                             0x00000015, 0x00000016, 0x00000017, 0x00000018, 0x00000019,
                             0x00000020, 0x00000021, 0x00000022, 0x00000023, 0xfffffff8,
                             0xfffffff7, 0xfffffff6, 0xfffffff5, 0xfffffff4, 0xfffffff3,
                             0xfffffff2, 0xfffffff1,
                         });
        write_input_data(3,
                         std::array<uint64_t, 32> {
                             0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                             0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                             0x0000000000000006, 0x0000000000000007, 0x0000000000000008,
                             0x0000000000000009, 0x0000000000000010, 0x0000000000000011,
                             0x0000000000000012, 0x0000000000000013, 0x0000000000000014,
                             0x0000000000000015, 0x0000000000000016, 0x0000000000000017,
                             0x0000000000000018, 0x0000000000000019, 0x0000000000000020,
                             0x0000000000000021, 0x0000000000000022, 0x0000000000000023,
                             0xfffffffffffffff8, 0xfffffffffffffff7, 0xfffffffffffffff6,
                             0xfffffffffffffff5, 0xfffffffffffffff4, 0xfffffffffffffff3,
                             0xfffffffffffffff2, 0xfffffffffffffff1,
                         });
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
    template <typename T, size_t N>
    void
    write_input_data(size_t offset, const std::array<T, N> &input_data)
    {
        // Repeat the supplied pattern through the selected region.
        const auto data_size = input_data.size() * sizeof(T);
        const auto num_repetitions = REGION_SIZE / data_size;
        assert(REGION_SIZE % num_repetitions == 0);
        for (size_t i = 0; i < num_repetitions; i++) {
            memcpy(region_start_addr(offset) + (data_size * i), input_data.data(),
                   data_size);
        }
    }

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
        // The base address is set to the middle of the region.
        return CHUNK_SIZE + (REGION_SIZE * offset) + REGION_SIZE / 2;
    }
} INPUT_DATA;

class output_data_t : public test_memory_t {
public:
    output_data_t()
        : test_memory_t()
    {
        /*
         * We set up 3 64KiB chunks of memory to use as output memory for store
         * instruction tests. The first and last chunks are set to fault when accessed,
         * and the middle chunk is used for tests to store values to.
         * The tests use the midpoint (region 2, 0x1800 bytes) as the base pointer and
         * tests have a +/-32KiB range to store to.
         * +=====================================================+
         * | Chunk  | Byte off | Region off |                    |
         * +=====================================================+
         * | 0      |  0x00000 |        n/a | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +--------+----------+------------+--------------------+
         * | 1      |  0x10000 |          0 | -ve offset data    |
         * |        |----------+------------+--------------------+
         * |        |  0x18000 |          2 | +ve offset data    |
         * +--------+----------+------------+--------------------+
         * | 2      |  0x20000 |        n/a | All accesses fault |
         * |        |          |            |                    |
         * |        |          |            |                    |
         * +--------+----------+------------+--------------------+
         */
    }

    void *
    base_addr()
    {
        return region_start_addr(2);
    }
} OUTPUT_DATA;

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
        [](scalar_plus_vector_load_test_case_t::test_ptrs_t &ptrs) {                \
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

    return run_tests<scalar_plus_vector_load_test_case_t>({
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
            "ld1b scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("ld1b z0.s, p7/z, [%[base], z31.s, uxtw]"),
            { /*zt=*/0, /*pg=*/7, /*zm=*/31 },
            std::array<uint32_t, 4> { 0x00, 0x01, 0x07, 0x10 },
            std::array<uint32_t, 4> { 0, 1, 7, 10 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("ld1b z1.s, p6/z, [%[base], z30.s, sxtw]"),
            { /*zt=*/1, /*pg=*/6, /*zm=*/30 },
            std::array<uint32_t, 4> { 0x00, 0xF1, 0x18, 0xF5 },
            std::array<int32_t, 4> { 0, -1, 18, 27 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1b z2.d, p5/z, [%[base], z29.d, uxtw]"),
            { /*zt=*/2, /*pg=*/5, /*zm=*/29 },
            std::array<uint64_t, 2> { 0x01, 0x22 },
            std::array<uint64_t, 2> { 1, 22 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1b z3.d, p4/z, [%[base], z28.d, sxtw]"),
            { /*zt=*/3, /*pg=*/4, /*zm=*/28 },
            std::array<uint64_t, 2> { 0xF2, 0xF3 },
            std::array<int64_t, 2> { -2, 29 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1b z4.d, p3/z, [%[base], z27.d]"),
            { /*zt=*/4, /*pg=*/3, /*zm=*/27 },
            std::array<uint64_t, 2> { 0x09, 0xF4 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1b z30.d, p3/z, [%[base], z30.d]"),
            { /*zt=*/30, /*pg=*/3, /*zm=*/30 },
            std::array<uint64_t, 2> { 0x09, 0xF4 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SB instructions.
        {
            "ld1sb scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("ld1sb z5.s, p2/z, [%[base], z26.s, uxtw]"),
            { /*zt=*/5, /*pg=*/2, /*zm=*/26 },
            std::array<int32_t, 4> { 0x00, -15, 0x23, -14 },
            std::array<uint32_t, 4> { 0, 31, 23, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("ld1sb z6.s, p1/z, [%[base], z25.s, sxtw]"),
            { /*zt=*/6, /*pg=*/1, /*zm=*/25 },
            std::array<int32_t, 4> { 0x01, -15, 0x11, -8 },
            std::array<int32_t, 4> { 1, -1, 11, 24 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sb z7.d, p0/z, [%[base], z24.d, uxtw]"),
            { /*zt=*/7, /*pg=*/0, /*zm=*/24 },
            std::array<int64_t, 2> { 0x01, -15 },
            std::array<uint64_t, 2> { 1, 31 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sb z8.d, p1/z, [%[base], z23.d, sxtw]"),
            { /*zt=*/8, /*pg=*/1, /*zm=*/23 },
            std::array<int64_t, 2> { -14, -13 },
            std::array<int64_t, 2> { -2, 29 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1sb z9.d, p2/z, [%[base], z22.d]"),
            { /*zt=*/9, /*pg=*/2, /*zm=*/22 },
            std::array<int64_t, 2> { -15, 0x09 },
            std::array<uint64_t, 2> { 31, 9 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1sb z17.d, p7/z, [%[base], z17.d]"),
            { /*zt=*/17, /*pg=*/7, /*zm=*/17 },
            std::array<int64_t, 2> { -15, 0x09 },
            std::array<uint64_t, 2> { 31, 9 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1H instructions.
        {
            "ld1h scalar+vector 32bit scaled offset uxtw",
            TEST_FUNC("ld1h z10.s, p3/z, [%[base], z21.s, uxtw #1]"),
            { /*zt=*/10, /*pg=*/3, /*zm=*/21 },
            std::array<uint32_t, 4> { 0x01, 0x10, 0x23, 0xFFF6 },
            std::array<uint32_t, 4> { 1, 10, 23, 26 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 32bit scaled offset sxtw",
            TEST_FUNC("ld1h z11.s, p4/z, [%[base], z20.s, sxtw #1]"),
            { /*zt=*/11, /*pg=*/4, /*zm=*/20 },
            std::array<uint32_t, 4> { 0xFFF3, 0x07, 0x16, 0xFFF2 },
            std::array<int32_t, 4> { -3, 7, 16, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1h z12.d, p5/z, [%[base], z19.d, uxtw #1]"),
            { /*zt=*/12, /*pg=*/5, /*zm=*/19 },
            std::array<uint64_t, 2> { 0x08, 0xFFF4 },
            std::array<uint64_t, 2> { 8, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1h z13.d, p6/z, [%[base], z18.d, sxtw #1]"),
            { /*zt=*/13, /*pg=*/6, /*zm=*/18 },
            std::array<uint64_t, 2> { 0xFFF4, 0xFFF8 },
            std::array<int64_t, 2> { -4, 24 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1h z14.d, p7/z, [%[base], z17.d, uxtw]"),
            { /*zt=*/14, /*pg=*/7, /*zm=*/17 },
            std::array<uint64_t, 2> { 0x0403, 0x2322 },
            std::array<uint64_t, 2> { 3, 22 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1h z15.d, p6/z, [%[base], z16.d, sxtw]"),
            { /*zt=*/15, /*pg=*/6, /*zm=*/16 },
            std::array<uint64_t, 2> { 0x0100, 0xF4F5 },
            std::array<int64_t, 2> { 0, -5 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("ld1h z16.s, p5/z, [%[base], z15.s, uxtw #1]"),
            { /*zt=*/16, /*pg=*/5, /*zm=*/15 },
            std::array<uint32_t, 4> { 0x01, 0x10, 0x23, 0xFFF2 },
            std::array<uint32_t, 4> { 1, 10, 23, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("ld1h z17.s, p4/z, [%[base], z14.s, sxtw #1]"),
            { /*zt=*/17, /*pg=*/4, /*zm=*/14 },
            std::array<uint32_t, 4> { 0x00, 0xFFF6, 0x18, 0xFFF5 },
            std::array<int32_t, 4> { 0, -6, 18, 27 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 64bit scaled offset",
            TEST_FUNC("ld1h z18.d, p3/z, [%[base], z13.d, lsl #1]"),
            { /*zt=*/18, /*pg=*/3, /*zm=*/13 },
            std::array<uint64_t, 2> { 0x03, 0x14 },
            std::array<uint64_t, 2> { 3, 14 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1h z19.d, p2/z, [%[base], z12.d]"),
            { /*zt=*/19, /*pg=*/2, /*zm=*/12 },
            std::array<uint64_t, 2> { 0x1009, 0xF3F4 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1h scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1h z25.d, p5/z, [%[base], z25.d]"),
            { /*zt=*/25, /*pg=*/5, /*zm=*/25 },
            std::array<uint64_t, 2> { 0x1009, 0xF3F4 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SH instructions.
        {
            "ld1sh scalar+vector 32bit scaled offset uxtw",
            TEST_FUNC("ld1sh z20.s, p1/z, [%[base], z11.s, uxtw #1]"),
            { /*zt=*/20, /*pg=*/1, /*zm=*/11 },
            std::array<int32_t, 4> { 0x00, 0x07, 0x16, -15 },
            std::array<uint32_t, 4> { 0, 7, 16, 31 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 32bit scaled offset sxtw",
            TEST_FUNC("ld1sh z21.s, p0/z, [%[base], z10.s, sxtw #1]"),
            { /*zt=*/21, /*pg=*/0, /*zm=*/10 },
            std::array<int32_t, 4> { -13, 0x01, 0x10, -14 },
            std::array<int32_t, 4> { -3, 1, 10, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1sh z22.d, p1/z, [%[base], z9.d, uxtw #1]"),
            { /*zt=*/22, /*pg=*/1, /*zm=*/9 },
            std::array<int64_t, 2> { 0x00, -15 },
            std::array<uint64_t, 2> { 0, 31 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1sh z23.d, p2/z, [%[base], z8.d, sxtw #1]"),
            { /*zt=*/23, /*pg=*/2, /*zm=*/8 },
            std::array<int64_t, 2> { -12, 0x14 },
            std::array<int64_t, 2> { -4, 14 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sh z24.d, p3/z, [%[base], z7.d, uxtw]"),
            { /*zt=*/24, /*pg=*/3, /*zm=*/7 },
            std::array<int64_t, 2> { 0x0201, -3598 },
            std::array<uint64_t, 2> { 1, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sh z25.d, p4/z, [%[base], z6.d, sxtw]"),
            { /*zt=*/25, /*pg=*/4, /*zm=*/6 },
            std::array<int64_t, 2> { -2827, -3341 },
            std::array<int64_t, 2> { -5, 29 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("ld1sh z26.s, p5/z, [%[base], z5.s, uxtw #1]"),
            { /*zt=*/26, /*pg=*/5, /*zm=*/5 },
            std::array<int32_t, 4> { 0x05, 0x15, -9, -15 },
            std::array<uint32_t, 4> { 5, 15, 25, 31 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("ld1sh z27.s, p6/z, [%[base], z4.s, sxtw #1]"),
            { /*zt=*/27, /*pg=*/6, /*zm=*/4 },
            std::array<int32_t, 4> { 0x06, 0x16, -10, -10 },
            std::array<int32_t, 4> { 6, 16, -6, 26 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 64bit scaled offset",
            TEST_FUNC("ld1sh z28.d, p7/z, [%[base], z3.d, lsl #1]"),
            { /*zt=*/28, /*pg=*/7, /*zm=*/3 },
            std::array<int64_t, 2> { 0x09, -15 },
            std::array<uint64_t, 2> { 9, 31 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1sh z29.d, p6/z, [%[base], z2.d]"),
            { /*zt=*/29, /*pg=*/6, /*zm=*/2 },
            std::array<int64_t, 2> { 0x0403, -3598 },
            std::array<uint64_t, 2> { 3, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sh scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1sh z0.d, p0/z, [%[base], z0.d]"),
            { /*zt=*/0, /*pg=*/0, /*zm=*/0 },
            std::array<int64_t, 2> { 0x0403, -3598 },
            std::array<uint64_t, 2> { 3, 30 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1W instructions.
        {
            "ld1w scalar+vector 32bit scaled offset uxtw",
            TEST_FUNC("ld1w z30.s, p5/z, [%[base], z1.s, uxtw #2]"),
            { /*zt=*/30, /*pg=*/5, /*zm=*/1 },
            std::array<uint32_t, 4> { 0x00, 0x07, 0x17, 0xFFFFFFF5 },
            std::array<uint32_t, 4> { 0, 7, 17, 27 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+vector 32bit scaled offset sxtw",
            TEST_FUNC("ld1w z31.s, p4/z, [%[base], z0.s, sxtw #2]"),
            { /*zt=*/31, /*pg=*/4, /*zm=*/0 },
            std::array<uint32_t, 4> { 0xFFFFFFF7, 0x07, 0x17, 0xFFFFFFF5 },
            std::array<int32_t, 4> { -7, 7, 17, 27 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1w z0.d, p3/z, [%[base], z1.d, uxtw #2]"),
            { /*zt=*/0, /*pg=*/3, /*zm=*/1 },
            std::array<uint64_t, 2> { 0x18, 0xFFFFFFF4 },
            std::array<uint64_t, 2> { 18, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1w z2.d, p2/z, [%[base], z3.d, sxtw #2]"),
            { /*zt=*/2, /*pg=*/2, /*zm=*/3 },
            std::array<uint64_t, 2> { 0xFFFFFFF8, 0x08 },
            std::array<int64_t, 2> { -8, 8 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1w z4.d, p1/z, [%[base], z5.d, uxtw]"),
            { /*zt=*/4, /*pg=*/1, /*zm=*/5 },
            std::array<uint64_t, 2> { 0x04030201, 0xF7F82322 },
            std::array<uint64_t, 2> { 1, 22 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1w z6.d, p0/z, [%[base], z7.d, sxtw]"),
            { /*zt=*/6, /*pg=*/0, /*zm=*/7 },
            std::array<uint64_t, 2> { 0x020100F1, 0xF2F3F4F5 },
            std::array<int64_t, 2> { -1, 27 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("ld1w z8.s, p1/z, [%[base], z9.s, uxtw]"),
            { /*zt=*/8, /*pg=*/1, /*zm=*/9 },
            std::array<uint32_t, 4> { 0x03020100, 0x05040302, 0x15141312, 0xF7F82322 },
            std::array<int32_t, 4> { 0, 2, 12, 22 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("ld1w z10.s, p2/z, [%[base], z11.s, sxtw]"),
            { /*zt=*/10, /*pg=*/2, /*zm=*/11 },
            std::array<uint32_t, 4> { 0x0100F1F2, 0x05040302, 0x15141312, 0xF7F82322 },
            std::array<int32_t, 4> { -2, 2, 12, 22 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w scalar+vector 64bit scaled offset",
            TEST_FUNC("ld1w z12.d, p3/z, [%[base], z13.d, lsl #2]"),
            { /*zt=*/12, /*pg=*/3, /*zm=*/13 },
            std::array<uint64_t, 2> { 0x03, 0x14 },
            std::array<uint64_t, 2> { 3, 14 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1w z14.d, p4/z, [%[base], z15.d]"),
            { /*zt=*/14, /*pg=*/4, /*zm=*/15 },
            std::array<uint64_t, 2> { 0x06050403, 0x17161514 },
            std::array<uint64_t, 2> { 3, 14 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1w scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1w z5.d, p5/z, [%[base], z5.d]"),
            { /*zt=*/5, /*pg=*/5, /*zm=*/5 },
            std::array<uint64_t, 2> { 0x06050403, 0x17161514 },
            std::array<uint64_t, 2> { 3, 14 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SW instructions.
        {
            "ld1sw scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1sw z16.d, p5/z, [%[base], z17.d, uxtw #2]"),
            { /*zt=*/16, /*pg=*/5, /*zm=*/17 },
            std::array<int64_t, 2> { -15, 0x10 },
            std::array<uint64_t, 2> { 31, 10 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1sw z18.d, p6/z, [%[base], z19.d, sxtw #2]"),
            { /*zt=*/18, /*pg=*/6, /*zm=*/19 },
            std::array<int64_t, 2> { -8, 0x16 },
            std::array<int64_t, 2> { -8, 16 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1sw z20.d, p7/z, [%[base], z21.d, uxtw]"),
            { /*zt=*/20, /*pg=*/7, /*zm=*/21 },
            std::array<int64_t, 2> { 0x04030201, -235736076 },
            std::array<uint64_t, 2> { 1, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1sw z22.d, p6/z, [%[base], z23.d, sxtw]"),
            { /*zt=*/22, /*pg=*/6, /*zm=*/23 },
            std::array<int64_t, 2> { 0x11100908, -168364040 },
            std::array<int64_t, 2> { 8, -8 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw scalar+vector 64bit scaled offset",
            TEST_FUNC("ld1sw z24.d, p5/z, [%[base], z25.d, lsl #2]"),
            { /*zt=*/24, /*pg=*/5, /*zm=*/25 },
            std::array<int64_t, 2> { -15, -12 },
            std::array<uint64_t, 2> { 31, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1sw z26.d, p4/z, [%[base], z27.d]"),
            { /*zt=*/26, /*pg=*/4, /*zm=*/27 },
            std::array<int64_t, 2> { 0x12111009, -235736076 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sw scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1sw z10.d, p5/z, [%[base], z10.d]"),
            { /*zt=*/10, /*pg=*/5, /*zm=*/10 },
            std::array<int64_t, 2> { 0x12111009, -235736076 },
            std::array<uint64_t, 2> { 9, 28 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1D
        {
            "ld1d scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("ld1d z28.d, p3/z, [%[base], z29.d, uxtw #3]"),
            { /*zt=*/28, /*pg=*/3, /*zm=*/29 },
            std::array<uint64_t, 2> { 0x15, 0xFFFFFFFFFFFFFFF7 },
            std::array<uint64_t, 2> { 15, 25 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("ld1d z30.d, p2/z, [%[base], z31.d, sxtw #3]"),
            { /*zt=*/30, /*pg=*/2, /*zm=*/31 },
            std::array<uint64_t, 2> { 0x08, 0xFFFFFFFFFFFFFFF3 },
            std::array<int64_t, 2> { 8, -3 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("ld1d z31.d, p1/z, [%[base], z30.d, uxtw]"),
            { /*zt=*/31, /*pg=*/1, /*zm=*/30 },
            std::array<uint64_t, 2> { 0x2019181716151413, 0xF2F3F4F5F6F7F823 },
            std::array<uint64_t, 2> { 13, 23 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("ld1d z29.d, p0/z, [%[base], z28.d, sxtw]"),
            { /*zt=*/29, /*pg=*/0, /*zm=*/28 },
            std::array<uint64_t, 2> { 0x2120191817161514, 0x03020100F1F2F3F4 },
            std::array<int64_t, 2> { 14, -4 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d scalar+vector 64bit scaled offset",
            TEST_FUNC("ld1d z27.d, p1/z, [%[base], z26.d, lsl #3]"),
            { /*zt=*/27, /*pg=*/1, /*zm=*/26 },
            std::array<uint64_t, 2> { 0x00, 0x10 },
            std::array<uint64_t, 2> { 0, 10 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d scalar+vector 64bit unscaled offset",
            TEST_FUNC("ld1d z25.d, p2/z, [%[base], z24.d]"),
            { /*zt=*/25, /*pg=*/2, /*zm=*/24 },
            std::array<uint64_t, 2> { 0x020100F1F2F3F4F5, 0x1716151413121110 },
            std::array<int64_t, 2> { -5, 10 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1d scalar+vector 64bit unscaled offset Zt==Zm",
            TEST_FUNC("ld1d z15.d, p5/z, [%[base], z15.d]"),
            { /*zt=*/15, /*pg=*/5, /*zm=*/15 },
            std::array<uint64_t, 2> { 0x020100F1F2F3F4F5, 0x1716151413121110 },
            std::array<int64_t, 2> { -5, 10 },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
    });
#    undef TEST_FUNC
}

struct scalar_plus_vector_store_test_case_t : public scalar_plus_vector_test_case_base_t {
    vector_reg_value128_t offset_data_;

    struct registers_used_t {
        unsigned src_z;
        unsigned governing_p;
        unsigned index_z;
    };
    registers_used_t registers_used_;

    element_size_t stored_value_size_;

    bool scaled_;

    expected_values_t expected_values_;

    template <typename OFFSET_T>
    scalar_plus_vector_store_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<OFFSET_T, TEST_VL_BYTES / sizeof(OFFSET_T)> offsets,
        element_size_t stored_value_size, bool scaled)
        : scalar_plus_vector_test_case_base_t(
              std::move(name), std::move(func), registers_used.governing_p,
              static_cast<element_size_t>(sizeof(OFFSET_T)),
              /*base_ptr=*/OUTPUT_DATA.base_addr())
        , registers_used_(registers_used)
        , stored_value_size_(stored_value_size)
        , scaled_(scaled)
        , expected_values_(offsets, stored_value_size)
    {
        std::memcpy(offset_data_.data(), offsets.data(), offset_data_.size());
    }

    virtual void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the offset register.
        register_values.set_z_register_value(registers_used_.index_z, offset_data_);

        register_values.set_z_register_value(registers_used_.src_z,
                                             { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                               0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13,
                                               0x14, 0x15 });
        OUTPUT_DATA.reset();
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }

        switch (element_size_) {
        case element_size_t::SINGLE: {
            std::array<const void *, 4> base_ptrs { base_ptr_, base_ptr_, base_ptr_,
                                                    base_ptr_ };
            switch (stored_value_size_) {
            case element_size_t::BYTE:
                check_expected_values(expected_values_.u8x4, pred, base_ptrs, scaled_);
                break;
            case element_size_t::HALF:
                check_expected_values(expected_values_.u16x4, pred, base_ptrs, scaled_);
                break;
            case element_size_t::SINGLE:
                check_expected_values(expected_values_.u32x4, pred, base_ptrs, scaled_);
                break;
            }
        }
        case element_size_t::DOUBLE: {
            std::array<const void *, 2> base_ptrs { base_ptr_, base_ptr_ };
            switch (stored_value_size_) {
            case element_size_t::BYTE:
                check_expected_values(expected_values_.u8x2, pred, base_ptrs, scaled_);
                break;
            case element_size_t::HALF:
                check_expected_values(expected_values_.u16x2, pred, base_ptrs, scaled_);
                break;
            case element_size_t::SINGLE:
                check_expected_values(expected_values_.u32x2, pred, base_ptrs, scaled_);
                break;
            case element_size_t::DOUBLE:
                check_expected_values(expected_values_.u64x2, pred, base_ptrs, scaled_);
                break;
            }
        }
        }
    }
};

test_result_t
test_st1_scalar_plus_vector()
{
#    define TEST_FUNC(st_instruction)                                               \
        [](scalar_plus_vector_store_test_case_t::test_ptrs_t &ptrs) {               \
            asm(/* clang-format off */                                              \
            RESTORE_Z_REGISTERS(z_restore_base)                                     \
            RESTORE_P_REGISTERS(p_restore_base)                                     \
            st_instruction "\n"                                                     \
            SAVE_Z_REGISTERS(z_save_base)                                           \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */         \
                :                                                                   \
                : [base] "r"(ptrs.base), [z_restore_base] "r"(ptrs.z_restore_base), \
                  [z_save_base] "r"(ptrs.z_save_base),                              \
                  [p_restore_base] "r"(ptrs.p_restore_base),                        \
                  [p_save_base] "r"(ptrs.p_save_base)                               \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                                \
        }

    return run_tests<scalar_plus_vector_store_test_case_t>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zm},
         *     Offset data (value for zm),
         *     Stored value size,
         *     Is the index scaled,
         * },
         */
        // ST1B instructions.
        {
            "st1b scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("st1b z0.d, p0, [%[base], z31.d, uxtw]"),
            { /*zt=*/0, /*pg=*/0, /*zm=*/31 },
            std::array<uint64_t, 2> { 0, 100 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("st1b z1.d, p1, [%[base], z30.d, sxtw]"),
            { /*zt=*/1, /*pg=*/1, /*zm=*/30 },
            std::array<int64_t, 2> { -1, 101 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("st1b z2.s, p2, [%[base], z29.s, uxtw]"),
            { /*zt=*/2, /*pg=*/2, /*zm=*/29 },
            std::array<uint32_t, 4> { 2, 102, 3, 103 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("st1b z3.s, p3, [%[base], z28.s, sxtw]"),
            { /*zt=*/3, /*pg=*/3, /*zm=*/28 },
            std::array<int32_t, 4> { -3, -103, 4, 104 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 32bit unscaled offset sxtw (repeated offset)",
            TEST_FUNC("st1b z3.s, p3, [%[base], z28.s, sxtw]"),
            { /*zt=*/3, /*pg=*/3, /*zm=*/28 },
            std::array<int32_t, 4> { -4, -4, 5, 5 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 64bit unscaled offset",
            TEST_FUNC("st1b z4.d, p4, [%[base], z27.d]"),
            { /*zt=*/4, /*pg=*/4, /*zm=*/27 },
            std::array<uint64_t, 2> { 5, 104 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        {
            "st1b scalar+vector 64bit unscaled offset (repeated offset)",
            TEST_FUNC("st1b z4.d, p4, [%[base], z27.d]"),
            { /*zt=*/4, /*pg=*/4, /*zm=*/27 },
            std::array<uint64_t, 2> { 6, 6 },
            element_size_t::BYTE,
            /*scaled=*/false,
        },
        // ST1H instructions.
        {
            "st1h scalar+vector 32bit scaled offset uxtw",
            TEST_FUNC("st1h z5.s, p5, [%[base], z26.s, uxtw #1]"),
            { /*zt=*/5, /*pg=*/5, /*zm=*/26 },
            std::array<uint32_t, 4> { 7, 105, 9, 107 },
            element_size_t::HALF,
            /*scaled=*/true,
        },
        {
            "st1h scalar+vector 32bit scaled offset sxtw",
            TEST_FUNC("st1h z6.s, p6, [%[base], z25.s, sxtw #1]"),
            { /*zt=*/6, /*pg=*/6, /*zm=*/25 },
            std::array<int32_t, 4> { -8, -106, 10, 108 },
            element_size_t::HALF,
            /*scaled=*/true,
        },
        {
            "st1h scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("st1h z7.d, p7, [%[base], z24.d, uxtw #1]"),
            { /*zt=*/7, /*pg=*/7, /*zm=*/24 },
            std::array<uint64_t, 2> { 9, 107 },
            element_size_t::HALF,
            /*scaled=*/true,
        },
        {
            "st1h scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("st1h z8.d, p0, [%[base], z23.d, sxtw #1]"),
            { /*zt=*/8, /*pg=*/0, /*zm=*/23 },
            std::array<int64_t, 2> { -10, 108 },
            element_size_t::HALF,
            /*scaled=*/true,
        },
        {
            "st1h scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("st1h z9.d, p1, [%[base], z22.d, uxtw]"),
            { /*zt=*/9, /*pg=*/1, /*zm=*/22 },
            std::array<uint64_t, 2> { 11, 109 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("st1h z10.d, p2, [%[base], z21.d, sxtw]"),
            { /*zt=*/10, /*pg=*/2, /*zm=*/21 },
            std::array<int64_t, 2> { -12, 110 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("st1h z11.s, p3, [%[base], z20.s, uxtw]"),
            { /*zt=*/11, /*pg=*/3, /*zm=*/20 },
            std::array<uint32_t, 4> { 13, 111, 15, 113 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("st1h z12.s, p4, [%[base], z19.s, sxtw]"),
            { /*zt=*/12, /*pg=*/4, /*zm=*/19 },
            std::array<int32_t, 4> { -14, -112, 16, 114 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("st1h z12.s, p4, [%[base], z19.s, sxtw]"),
            { /*zt=*/12, /*pg=*/4, /*zm=*/19 },
            std::array<int32_t, 4> { -14, -112, 16, 114 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 32bit unscaled offset sxtw (repeated offset)",
            TEST_FUNC("st1h z12.s, p4, [%[base], z19.s, sxtw]"),
            { /*zt=*/12, /*pg=*/4, /*zm=*/19 },
            std::array<int32_t, 4> { 15, 15, 17, 17 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 64bit scaled offset",
            TEST_FUNC("st1h z13.d, p5, [%[base], z18.d, lsl #1]"),
            { /*zt=*/13, /*pg=*/5, /*zm=*/18 },
            std::array<uint64_t, 2> { 16, 113 },
            element_size_t::HALF,
            /*scaled=*/true,
        },
        {
            "st1h scalar+vector 64bit unscaled offset",
            TEST_FUNC("st1h z14.d, p6, [%[base], z17.d]"),
            { /*zt=*/14, /*pg=*/6, /*zm=*/17 },
            std::array<uint64_t, 2> { 17, 114 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        {
            "st1h scalar+vector 64bit unscaled offset (repeated offset)",
            TEST_FUNC("st1h z14.d, p6, [%[base], z17.d]"),
            { /*zt=*/14, /*pg=*/6, /*zm=*/17 },
            std::array<uint64_t, 2> { 18, 18 },
            element_size_t::HALF,
            /*scaled=*/false,
        },
        // ST1W instructions.
        {
            "st1w scalar+vector 32bit scaled offset uxtw",
            TEST_FUNC("st1w z15.s, p7, [%[base], z16.s, uxtw #2]"),
            { /*zt=*/15, /*pg=*/7, /*zm=*/16 },
            std::array<uint32_t, 4> { 19, 115, 23, 119 },
            element_size_t::SINGLE,
            /*scaled=*/true,
        },
        {
            "st1w scalar+vector 32bit scaled offset sxtw",
            TEST_FUNC("st1w z16.s, p0, [%[base], z15.s, sxtw #2]"),
            { /*zt=*/16, /*pg=*/0, /*zm=*/15 },
            std::array<int32_t, 4> { -20, -116, 24, 120 },
            element_size_t::SINGLE,
            /*scaled=*/true,
        },
        {
            "st1w scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("st1w z17.d, p1, [%[base], z14.d, uxtw #2]"),
            { /*zt=*/17, /*pg=*/1, /*zm=*/14 },
            std::array<uint64_t, 2> { 21, 117 },
            element_size_t::SINGLE,
            /*scaled=*/true,
        },
        {
            "st1w scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("st1w z18.d, p2, [%[base], z13.d, sxtw #2]"),
            { /*zt=*/18, /*pg=*/2, /*zm=*/13 },
            std::array<int64_t, 2> { -22, 118 },
            element_size_t::SINGLE,
            /*scaled=*/true,
        },
        {
            "st1w scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("st1w z19.d, p3, [%[base], z12.d, uxtw]"),
            { /*zt=*/19, /*pg=*/3, /*zm=*/12 },
            std::array<uint64_t, 2> { 23, 119 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("st1w z20.d, p4, [%[base], z11.d, sxtw]"),
            { /*zt=*/20, /*pg=*/4, /*zm=*/11 },
            std::array<int64_t, 2> { -24, 120 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 32bit unscaled offset uxtw",
            TEST_FUNC("st1w z21.s, p5, [%[base], z10.s, uxtw]"),
            { /*zt=*/21, /*pg=*/5, /*zm=*/10 },
            std::array<uint32_t, 4> { 25, 121, 29, 125 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 32bit unscaled offset sxtw",
            TEST_FUNC("st1w z22.s, p6, [%[base], z9.s, sxtw]"),
            { /*zt=*/22, /*pg=*/6, /*zm=*/9 },
            std::array<int32_t, 4> { -26, -122, 30, 126 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 32bit unscaled offset sxtw (repeated offset)",
            TEST_FUNC("st1w z22.s, p6, [%[base], z9.s, sxtw]"),
            { /*zt=*/22, /*pg=*/6, /*zm=*/9 },
            std::array<int32_t, 4> { -27, -27, 30, 30 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 64bit scaled offset",
            TEST_FUNC("st1w z23.d, p7, [%[base], z8.d, lsl #2]"),
            { /*zt=*/23, /*pg=*/7, /*zm=*/8 },
            std::array<uint64_t, 2> { 28, 123 },
            element_size_t::SINGLE,
            /*scaled=*/true,
        },
        {
            "st1w scalar+vector 64bit unscaled offset",
            TEST_FUNC("st1w z24.d, p0, [%[base], z7.d]"),
            { /*zt=*/24, /*pg=*/0, /*zm=*/7 },
            std::array<uint64_t, 2> { 29, 124 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        {
            "st1w scalar+vector 64bit unscaled offset (repeated offset)",
            TEST_FUNC("st1w z24.d, p0, [%[base], z7.d]"),
            { /*zt=*/24, /*pg=*/0, /*zm=*/7 },
            std::array<uint64_t, 2> { 30, 30 },
            element_size_t::SINGLE,
            /*scaled=*/false,
        },
        // ST1D instructions.
        {
            "st1d scalar+vector 32bit unpacked scaled offset uxtw",
            TEST_FUNC("st1d z25.d, p1, [%[base], z6.d, uxtw #3]"),
            { /*zt=*/25, /*pg=*/1, /*zm=*/6 },
            std::array<uint64_t, 2> { 31, 125 },
            element_size_t::DOUBLE,
            /*scaled=*/true,
        },
        {
            "st1d scalar+vector 32bit unpacked scaled offset sxtw",
            TEST_FUNC("st1d z26.d, p2, [%[base], z5.d, sxtw #3]"),
            { /*zt=*/26, /*pg=*/2, /*zm=*/5 },
            std::array<int64_t, 2> { -32, 126 },
            element_size_t::DOUBLE,
            /*scaled=*/true,
        },
        {
            "st1d scalar+vector 32bit unpacked unscaled offset uxtw",
            TEST_FUNC("st1d z27.d, p3, [%[base], z4.d, uxtw]"),
            { /*zt=*/27, /*pg=*/3, /*zm=*/4 },
            std::array<uint64_t, 2> { 33, 127 },
            element_size_t::DOUBLE,
            /*scaled=*/false,
        },
        {
            "st1d scalar+vector 32bit unpacked unscaled offset sxtw",
            TEST_FUNC("st1d z28.d, p4, [%[base], z3.d, sxtw]"),
            { /*zt=*/28, /*pg=*/4, /*zm=*/3 },
            std::array<int64_t, 2> { -34, 128 },
            element_size_t::DOUBLE,
            /*scaled=*/false,
        },
        {
            "st1d scalar+vector 64bit scaled offset",
            TEST_FUNC("st1d z29.d, p5, [%[base], z2.d, lsl #3]"),
            { /*zt=*/29, /*pg=*/5, /*zm=*/2 },
            std::array<uint64_t, 2> { 36, 129 },
            element_size_t::DOUBLE,
            /*scaled=*/true,
        },
        {
            "st1d scalar+vector 64bit unscaled offset",
            TEST_FUNC("st1d z30.d, p6, [%[base], z1.d]"),
            { /*zt=*/30, /*pg=*/6, /*zm=*/1 },
            std::array<uint64_t, 2> { 37, 130 },
            element_size_t::DOUBLE,
            /*scaled=*/false,
        },
        {
            "st1d scalar+vector 64bit unscaled offset (repeated offset)",
            TEST_FUNC("st1d z30.d, p6, [%[base], z1.d]"),
            { /*zt=*/30, /*pg=*/6, /*zm=*/1 },
            std::array<uint64_t, 2> { 38, 38 },
            element_size_t::DOUBLE,
            /*scaled=*/false,
        },
    });
#    undef TEST_FUNC
}

struct vector_plus_immediate_load_test_case_t
    : public test_case_base_t<basic_test_ptrs_t> {
    vector_reg_value128_t reference_data_;
    vector_reg_value128_t base_data_;

    struct registers_used_t {
        unsigned dest_z;
        unsigned governing_p;
        unsigned base_z;
    } registers_used_;

    template <typename ELEMENT_T, typename BASE_T>
    vector_plus_immediate_load_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<ELEMENT_T, TEST_VL_BYTES / sizeof(ELEMENT_T)> reference_data,
        std::array<BASE_T, TEST_VL_BYTES / sizeof(BASE_T)> base)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        static_cast<element_size_t>(sizeof(BASE_T)))
        , registers_used_(registers_used)

    {
        std::memcpy(reference_data_.data(), reference_data.data(),
                    reference_data_.size());
        std::memcpy(base_data_.data(), base.data(), base_data_.size());
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the base vector register.
        register_values.set_z_register_value(registers_used_.base_z, base_data_);
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        const auto vl_bytes = get_vl_bytes();

        std::vector<uint8_t> expected_output_data;
        expected_output_data.resize(vl_bytes);

        assert(reference_data_.size() == TEST_VL_BYTES);
        for (size_t i = 0; i < vl_bytes / TEST_VL_BYTES; i++) {
            memcpy(&expected_output_data[TEST_VL_BYTES * i], reference_data_.data(),
                   TEST_VL_BYTES);
        }
        apply_predicate_mask(expected_output_data, pred, element_size_);
        const scalable_reg_value_t expected_output {
            expected_output_data.data(),
            vl_bytes,
        };

        const auto output_value =
            register_data.after.get_z_register_value(registers_used_.dest_z);

        if (output_value != expected_output) {
            test_failed();
            print("predicate: ");
            print_predicate(
                register_data.before.get_p_register_value(registers_used_.governing_p));
            print("\nexpected:  ");
            print_vector(expected_output);
            print("\nactual:    ");
            print_vector(output_value);
            print("\n");
        }

        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            if (i == registers_used_.dest_z)
                continue;
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_ld1_vector_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](vector_plus_immediate_load_test_case_t::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    const auto get_base_ptr = [&](element_size_t element_size, size_t offset) {
        void *start = INPUT_DATA.base_addr_for_data_size(element_size);
        switch (element_size) {
        case element_size_t::BYTE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint8_t *>(start)[offset]);
        case element_size_t::HALF:
            return reinterpret_cast<uintptr_t>(&static_cast<uint16_t *>(start)[offset]);
        case element_size_t::SINGLE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint32_t *>(start)[offset]);
        case element_size_t::DOUBLE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint64_t *>(start)[offset]);
        }
        assert(false); // unreachable
        return uintptr_t(0);
    };
    return run_tests<vector_plus_immediate_load_test_case_t>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zn},
         *     Expected output data,
         *     Base data (value for zn),
         * },
         */
        /* TODO i#5036: Add tests for 32-bit element variants.
         *              For example: ld1b z0.s, p0/z, [z31.s, #0].
         *              These instructions require 32-bit base pointers and I'm not sure
         *              how we can reliably and portably guarantee that allocated memory
         *              has an address that fits into 32-bits.
         */
        {
            "ld1b vector+immediate 64bit element",
            TEST_FUNC("ld1b z0.d, p0/z, [z31.d, #0]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<uint64_t, 2> { 0x00, 0x16 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 0),
                get_base_ptr(element_size_t::BYTE, 16),
            },
        },
        {
            "ld1b vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1b z0.d, p0/z, [z31.d, #31]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<uint64_t, 2> { 0xf1, 0xf1 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 0),
                get_base_ptr(element_size_t::BYTE, 0),
            },
        },
        {
            "ld1sb vector+immediate 64bit element",
            TEST_FUNC("ld1sb z3.d, p1/z, [z27.d, #1]"),
            { /*zt=*/3, /*pg=*/1, /*zn=*/27 },
            std::array<int64_t, 2> { 0x02, -15 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 1),
                get_base_ptr(element_size_t::BYTE, 30),
            },
        },
        {
            "ld1sb vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1sb z3.d, p1/z, [z27.d, #31]"),
            { /*zt=*/3, /*pg=*/1, /*zn=*/27 },
            std::array<int64_t, 2> { -15, -15 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 0),
                get_base_ptr(element_size_t::BYTE, 0),
            },
        },
        {
            "ld1h vector+immediate 64bit element",
            TEST_FUNC("ld1h z7.d, p2/z, [z23.d, #4]"),
            { /*zt=*/7, /*pg=*/2, /*zn=*/23 },
            std::array<uint64_t, 2> { 0x04, 0x20 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 2),
                get_base_ptr(element_size_t::HALF, 18),
            },
        },
        {
            "ld1h vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1h z7.d, p2/z, [z23.d, #62]"),
            { /*zt=*/7, /*pg=*/2, /*zn=*/23 },
            std::array<uint64_t, 2> { 0xfff1, 0xfff1 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 0),
                get_base_ptr(element_size_t::HALF, 0),
            },
        },
        {
            "ld1sh vector+immediate 64bit element",
            TEST_FUNC("ld1sh z11.d, p3/z, [z19.d, #6]"),
            { /*zt=*/11, /*pg=*/3, /*zn=*/19 },
            std::array<int64_t, 2> { 0x06, -15 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 3),
                get_base_ptr(element_size_t::HALF, 28),
            },
        },
        {
            "ld1sh vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1sh z11.d, p3/z, [z19.d, #62]"),
            { /*zt=*/11, /*pg=*/3, /*zn=*/19 },
            std::array<int64_t, 2> { -15, -14 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 0),
                get_base_ptr(element_size_t::HALF, -1),
            },
        },
        {
            "ld1w vector+immediate 64bit element",
            TEST_FUNC("ld1w z15.d, p4/z, [z15.d, #16]"),
            { /*zt=*/15, /*pg=*/4, /*zn=*/15 },
            std::array<uint64_t, 2> { 0x08, 0xfffffff8 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 4),
                get_base_ptr(element_size_t::SINGLE, 20),
            },
        },
        {
            "ld1w vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1w z15.d, p4/z, [z15.d, #124]"),
            { /*zt=*/15, /*pg=*/4, /*zn=*/15 },
            std::array<uint64_t, 2> { 0xfffffff1, 0xfffffff3 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 0),
                get_base_ptr(element_size_t::SINGLE, -2),
            },
        },
        {
            "ld1sw vector+immediate 64bit element",
            TEST_FUNC("ld1sw z19.d, p5/z, [z11.d, #20]"),
            { /*zt=*/19, /*pg=*/5, /*zn=*/11 },
            std::array<int64_t, 2> { 0x10, -14 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 5),
                get_base_ptr(element_size_t::SINGLE, 25),
            },
        },
        {
            "ld1sw vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1sw z19.d, p5/z, [z11.d, #124]"),
            { /*zt=*/19, /*pg=*/5, /*zn=*/11 },
            std::array<int64_t, 2> { -9, -10 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 26),
                get_base_ptr(element_size_t::SINGLE, -5),
            },
        },
        {
            "ld1d vector+immediate 64bit element",
            TEST_FUNC("ld1d z23.d, p6/z, [z7.d, #48]"),
            { /*zt=*/23, /*pg=*/6, /*zn=*/7 },
            std::array<uint64_t, 2> { 0x12, 0xfffffffffffffff4 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::DOUBLE, 6),
                get_base_ptr(element_size_t::DOUBLE, 22),
            },
        },
        {
            "ld1d vector+immediate 64bit element (max index)",
            TEST_FUNC("ld1d z23.d, p6/z, [z7.d, #248]"),
            { /*zt=*/23, /*pg=*/6, /*zn=*/7 },
            std::array<uint64_t, 2> { 0xfffffffffffffff1, 0xfffffffffffffff7 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::DOUBLE, 0),
                get_base_ptr(element_size_t::DOUBLE, -6),
            },
        },
        {
            "ld1d vector+immediate 64bit element Zt==Zn",
            TEST_FUNC("ld1d z27.d, p7/z, [z3.d, #0]"),
            { /*zt=*/27, /*pg=*/7, /*zn=*/3 },
            std::array<uint64_t, 2> { 0x07, 0x23 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::DOUBLE, 7),
                get_base_ptr(element_size_t::DOUBLE, 23),
            },
        },
    });
#    undef TEST_FUNC
}

struct vector_plus_immediate_store_test_case_t
    : public test_case_base_t<basic_test_ptrs_t> {
    vector_reg_value128_t base_data_;
    std::array<const void *, 2> base_ptrs_;

    struct registers_used_t {
        unsigned src_z;
        unsigned governing_p;
        unsigned base_z;
    } registers_used_;

    element_size_t stored_value_size_;

    expected_values_t expected_values_;

    vector_plus_immediate_store_test_case_t(std::string name, test_func_t func,
                                            registers_used_t registers_used,
                                            std::array<std::ptrdiff_t, 2> base_offsets,
                                            element_size_t stored_value_size,
                                            std::ptrdiff_t immediate_offset)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        element_size_t::DOUBLE)
        , registers_used_(registers_used)
        , stored_value_size_(stored_value_size)
        , expected_values_(
              std::array<std::ptrdiff_t, 2> { immediate_offset, immediate_offset },
              stored_value_size)
    {
        base_ptrs_[0] =
            static_cast<const uint8_t *>(OUTPUT_DATA.base_addr()) + base_offsets[0];
        base_ptrs_[1] =
            static_cast<const uint8_t *>(OUTPUT_DATA.base_addr()) + base_offsets[1];
        std::memcpy(base_data_.data(), base_ptrs_.data(), base_data_.size());
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the base register.
        register_values.set_z_register_value(registers_used_.base_z, base_data_);

        register_values.set_z_register_value(registers_used_.src_z,
                                             { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                               0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13,
                                               0x14, 0x15 });
        OUTPUT_DATA.reset();
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        // Check that the values of the Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }

        const bool scaled = false;
        assert(element_size_ == element_size_t::DOUBLE);

        switch (stored_value_size_) {
        case element_size_t::BYTE:
            check_expected_values(expected_values_.u8x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::HALF:
            check_expected_values(expected_values_.u16x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::SINGLE:
            check_expected_values(expected_values_.u32x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::DOUBLE:
            check_expected_values(expected_values_.u64x2, pred, base_ptrs_, scaled);
            break;
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_st1_vector_plus_immediate()
{
#    define TEST_FUNC(st_instruction)                                       \
        [](vector_plus_immediate_load_test_case_t::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            st_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<vector_plus_immediate_store_test_case_t>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zn},
         *     Offsets
         *     Stored value size
         *     #imm index value
         * },
         */
        /* TODO i#5036: Add tests for 32-bit element variants.
         *              For example: st1b z0.s, p0/z, [z31.s, #0].
         *              These instructions require 32-bit base pointers and I'm not sure
         *              how we can reliably and portably guarantee that allocated memory
         *              has an address that fits into 32-bits.
         */
        {
            "st1b vector+immediate 64bit element",
            TEST_FUNC("st1b z0.d, p0, [z31.d, #0]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<std::ptrdiff_t, 2> { 0, 16 },
            element_size_t::BYTE,
            0,
        },
        {
            "st1b vector+immediate 64bit element (max index)",
            TEST_FUNC("st1b z0.d, p0, [z31.d, #31]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<std::ptrdiff_t, 2> { 0, 16 },
            element_size_t::BYTE,
            31,
        },
        {
            "st1b vector+immediate 64bit element (repeated base)",
            TEST_FUNC("st1b z0.d, p0, [z31.d, #0]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<std::ptrdiff_t, 2> { 0, 0 },
            element_size_t::BYTE,
            0,
        },
        {
            "st1h vector+immediate 64bit element",
            TEST_FUNC("st1h z7.d, p2, [z23.d, #4]"),
            { /*zt=*/7, /*pg=*/2, /*zn=*/23 },
            std::array<std::ptrdiff_t, 2> { 2, 18 },
            element_size_t::HALF,
            4,
        },
        {
            "st1h vector+immediate 64bit element (max index)",
            TEST_FUNC("st1h z7.d, p2, [z23.d, #62]"),
            { /*zt=*/7, /*pg=*/2, /*zn=*/23 },
            std::array<std::ptrdiff_t, 2> { 2, 18 },
            element_size_t::HALF,
            62,
        },
        {
            "st1h vector+immediate 64bit element (repeated base)",
            TEST_FUNC("st1h z7.d, p2, [z23.d, #4]"),
            { /*zt=*/7, /*pg=*/2, /*zn=*/23 },
            std::array<std::ptrdiff_t, 2> { 19, 19 },
            element_size_t::HALF,
            4,
        },
        {
            "st1w vector+immediate 64bit element",
            TEST_FUNC("st1w z15.d, p4, [z16.d, #16]"),
            { /*zt=*/15, /*pg=*/4, /*zn=*/16 },
            std::array<std::ptrdiff_t, 2> { 4, 20 },
            element_size_t::SINGLE,
            16,
        },
        {
            "st1w vector+immediate 64bit element (max index)",
            TEST_FUNC("st1w z15.d, p4, [z16.d, #124]"),
            { /*zt=*/15, /*pg=*/4, /*zn=*/16 },
            std::array<std::ptrdiff_t, 2> { 4, 20 },
            element_size_t::SINGLE,
            124,
        },
        {
            "st1w vector+immediate 64bit element (repeated base)",
            TEST_FUNC("st1w z15.d, p4, [z16.d, #16]"),
            { /*zt=*/15, /*pg=*/4, /*zn=*/16 },
            std::array<std::ptrdiff_t, 2> { 21, 21 },
            element_size_t::SINGLE,
            16,
        },
        {
            "st1d vector+immediate 64bit element",
            TEST_FUNC("st1d z23.d, p6, [z7.d, #48]"),
            { /*zt=*/23, /*pg=*/6, /*zn=*/7 },
            std::array<std::ptrdiff_t, 2> { 6, 22 },
            element_size_t::DOUBLE,
            48,
        },
        {
            "st1d vector+immediate 64bit element (max index)",
            TEST_FUNC("st1d z23.d, p6, [z7.d, #248]"),
            { /*zt=*/23, /*pg=*/6, /*zn=*/7 },
            std::array<std::ptrdiff_t, 2> { 6, 22 },
            element_size_t::DOUBLE,
            248,
        },
        {
            "st1d vector+immediate 64bit element (repeated base)",
            TEST_FUNC("st1d z23.d, p6, [z7.d, #48]"),
            { /*zt=*/23, /*pg=*/6, /*zn=*/7 },
            std::array<std::ptrdiff_t, 2> { 23, 23 },
            element_size_t::DOUBLE,
            48,
        },
    });
#    undef TEST_FUNC
}

struct scalar_plus_scalar_test_ptrs_t : public basic_test_ptrs_t {
    void *base;    // Value used for the scalar base pointer.
    int64_t index; // Value used for the scalar index value.

    scalar_plus_scalar_test_ptrs_t(void *base_, int64_t index_,
                                   const void *z_restore_base_,
                                   const void *p_restore_base_, void *z_save_base_,
                                   void *p_save_base_)
        : basic_test_ptrs_t { z_restore_base_, p_restore_base_, z_save_base_,
                              p_save_base_ }
        , base(base_)
        , index(index_)
    {
    }
};

template <size_t NUM_ZT>
struct scalar_plus_scalar_load_test_case_t
    : public test_case_base_t<scalar_plus_scalar_test_ptrs_t> {
    std::array<std::vector<uint8_t>, NUM_ZT> reference_data_;

    struct registers_used_t {
        std::array<unsigned, NUM_ZT> dest_z;
        unsigned governing_p;
    } registers_used_;

    void *base_;
    int64_t index_;

    template <typename ELEMENT_T>
    scalar_plus_scalar_load_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<std::array<ELEMENT_T, MAX_SUPPORTED_VL_BYTES / sizeof(ELEMENT_T)>,
                   NUM_ZT>
            reference_data,
        void *base, int64_t index)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        static_cast<element_size_t>(sizeof(ELEMENT_T)))
        , registers_used_(registers_used)
        , base_(base)
        , index_(index)
    {
        const auto vl_bytes = get_vl_bytes();
        static constexpr size_t REG_DATA_SIZE =
            MAX_SUPPORTED_VL_BYTES / sizeof(ELEMENT_T);
        for (size_t i = 0; i < NUM_ZT; i++) {
            reference_data_[i].resize(vl_bytes);
            memcpy(reference_data_[i].data(), reference_data[i].data(), vl_bytes);
        }
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // No Z/P registers to set up. The base and index are passed to the test function
        // in the scalar_plus_scalar_test_ptrs_t object.
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        for (size_t i = 0; i < NUM_ZT; i++) {
            std::vector<uint8_t> expected_output_data(reference_data_[i]);
            apply_predicate_mask(expected_output_data, pred, element_size_);
            const scalable_reg_value_t expected_output {
                expected_output_data.data(),
                expected_output_data.size(),
            };

            const auto output_value =
                register_data.after.get_z_register_value(registers_used_.dest_z[i]);

            if (output_value != expected_output) {
                test_failed();
                if (NUM_ZT > 1)
                    print("Zt%u:\n", (unsigned)i);
                print("predicate: ");
                print_predicate(register_data.before.get_p_register_value(
                    registers_used_.governing_p));
                print("\nexpected:  ");
                print_vector(expected_output);
                print("\nactual:    ");
                print_vector(output_value);
                print("\n");
            }
        }

        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            if (std::find(registers_used_.dest_z.begin(), registers_used_.dest_z.end(),
                          i) == registers_used_.dest_z.end())
                check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            base_,
            index_,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_ld1_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_load_test_case_t<1>::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_load_test_case_t<1>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zm},
         *     Expected output data,
         *     Base pointer (value for Xn),
         *     Index (value for Xm),
         * },
         */
        // LD1B instructions.
        {
            "ld1b scalar+scalar 8bit element",
            TEST_FUNC("ld1b z4.b, p7/z, [%[base], %[index]]"),
            { /*zt=*/ { 4 }, /*pg=*/7 },
            std::array<std::array<uint8_t, 64>, 1> { {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x00,
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
                0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1,
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/0,
        },
        {
            "ld1b scalar+scalar 16bit element",
            TEST_FUNC("ld1b z8.h, p6/z, [%[base], %[index]]"),
            { /*zt=*/ { 8 }, /*pg=*/6 },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x00f1, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006,
                  0x0007, 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014,
                  0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022,
                  0x0023, 0x00f8, 0x00f7, 0x00f6, 0x00f5, 0x00f4, 0x00f3, 0x00f2 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/-1,
        },
        {
            "ld1b scalar+scalar 32bit element",
            TEST_FUNC("ld1b z12.s, p5/z, [%[base], %[index]]"),
            { /*zt=*/ { 12 }, /*pg=*/5 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x000005, 0x000006, 0x000007, 0x000008, 0x000009, 0x000010, 0x000011,
                  0x000012, 0x000013, 0x000014, 0x000015, 0x000016, 0x000017, 0x000018,
                  0x000019, 0x000020 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/5,
        },
        {
            "ld1b scalar+scalar 64bit element",
            TEST_FUNC("ld1b z16.d, p4/z, [%[base], %[index]]"),
            { /*zt=*/ { 16 }, /*pg=*/4 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x00000000000009, 0x00000000000010, 0x00000000000011, 0x00000000000012,
                  0x00000000000013, 0x00000000000014, 0x00000000000015,
                  0x00000000000016 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/9,
        },
        {
            "ldnt1b scalar+scalar",
            TEST_FUNC("ldnt1b z20.b, p3/z, [%[base], %[index]]"),
            { /*zt=*/ { 20 }, /*pg=*/3 },
            std::array<std::array<uint8_t, 64>, 1> {
                { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x00,
                  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                  0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
                  0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/0,
        },
        // LD1SB
        {
            "ld1sb scalar+scalar 16bit element",
            TEST_FUNC("ld1sb z24.h, p2/z, [%[base], %[index]]"),
            { /*zt=*/ { 24 }, /*pg=*/2 },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0xfff3, 0xfff2, 0xfff1, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004,
                  0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x0010, 0x0011, 0x0012,
                  0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x0020,
                  0x0021, 0x0022, 0x0023, 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/-3,
        },
        {
            "ld1sb scalar+scalar 32bit element",
            TEST_FUNC("ld1sb z28.s, p1/z, [%[base], %[index]]"),
            { /*zt=*/ { 28 }, /*pg=*/1 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x000005, 0x000006, 0x000007, 0x000008, 0x000009, 0x000010, 0x000011,
                  0x000012, 0x000013, 0x000014, 0x000015, 0x000016, 0x000017, 0x000018,
                  0x000019, 0x000020 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/5,
        },
        {
            "ld1sb scalar+scalar 64bit element",
            TEST_FUNC("ld1sb z31.d, p0/z, [%[base], %[index]]"),
            { /*zt=*/ { 31 }, /*pg=*/0 },
            std::array<std::array<int64_t, 8>, 1> { { -12, -13, -14, -15, 0, 1, 2, 3 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/28,
        },
        // LD1H
        {
            "ld1h scalar+scalar 16bit element",
            TEST_FUNC("ld1h z27.h, p1/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 27 }, /*pg=*/1 },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0006, 0x0007, 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013,
                  0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021,
                  0x0022, 0x0023, 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3,
                  0xfff2, 0xfff1, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/6,
        },
        {
            "ld1h scalar+scalar 32bit element",
            TEST_FUNC("ld1h z23.s, p2/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 23 }, /*pg=*/2 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000009, 0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014,
                  0x00000015, 0x00000016, 0x00000017, 0x00000018, 0x00000019, 0x00000020,
                  0x00000021, 0x00000022, 0x00000023, 0x0000fff8 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/9,
        },
        {
            "ld1h scalar+scalar 64bit element",
            TEST_FUNC("ld1h z19.d, p3/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 19 }, /*pg=*/3 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x000000000000fff2, 0x000000000000fff1, 0x0000000000000000,
                  0x0000000000000001, 0x0000000000000002, 0x0000000000000003,
                  0x0000000000000004, 0x0000000000000005 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/-2,
        },
        {
            "ldnt1h scalar+scalar",
            TEST_FUNC("ldnt1h z15.h, p4/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 15 }, /*pg=*/4 },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0006, 0x0007, 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013,
                  0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021,
                  0x0022, 0x0023, 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3,
                  0xfff2, 0xfff1, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/6,
        },
        // LD1SH
        {
            "ld1sh scalar+scalar 32bit element",
            TEST_FUNC("ld1sh z11.s, p5/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 11 }, /*pg=*/5 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000009, 0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014,
                  0x00000015, 0x00000016, 0x00000017, 0x00000018, 0x00000019, 0x00000020,
                  0x00000021, 0x00000022, 0x00000023, 0xfffffff8 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/9,
        },
        {
            "ld1sh scalar+scalar 64bit element",
            TEST_FUNC("ld1sh z7.d, p6/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 7 }, /*pg=*/6 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0xfffffffffffffff2, 0xfffffffffffffff1, 0x0000000000000000,
                  0x0000000000000001, 0x0000000000000002, 0x0000000000000003,
                  0x0000000000000004, 0x0000000000000005 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/-2,
        },
        // LD1W
        {
            "ld1w scalar+scalar 32bit element",
            TEST_FUNC("ld1w z3.s, p7/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 3 }, /*pg=*/7 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000017, 0x00000018, 0x00000019, 0x00000020, 0x00000021, 0x00000022,
                  0x00000023, 0xfffffff8, 0xfffffff7, 0xfffffff6, 0xfffffff5, 0xfffffff4,
                  0xfffffff3, 0xfffffff2, 0xfffffff1, 0x00000000 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/17,
        },
        {
            "ld1w scalar+scalar 64bit element",
            TEST_FUNC("ld1w z1.d, p6/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 1 }, /*pg=*/6 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x00000000fffffff1, 0x0000000000000000, 0x0000000000000001,
                  0x0000000000000002, 0x0000000000000003, 0x0000000000000004,
                  0x0000000000000005, 0x0000000000000006 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/-1,
        },
        {
            "ldnt1w scalar+scalar",
            TEST_FUNC("ldnt1w z5.s, p5/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 5 }, /*pg=*/5 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000018, 0x00000019, 0x00000020, 0x00000021, 0x00000022, 0x00000023,
                  0xfffffff8, 0xfffffff7, 0xfffffff6, 0xfffffff5, 0xfffffff4, 0xfffffff3,
                  0xfffffff2, 0xfffffff1, 0x00000000, 0x00000001 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/18,
        },
        // LD1SW
        {
            "ld1sw scalar+scalar",
            TEST_FUNC("ld1sw z9.d, p4/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 9 }, /*pg=*/4 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0xfffffffffffffff1, 0x0000000000000000, 0x0000000000000001,
                  0x0000000000000002, 0x0000000000000003, 0x0000000000000004,
                  0x0000000000000005, 0x0000000000000006 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/-1,
        },
        // LD1D
        {
            "ld1d scalar+scalar",
            TEST_FUNC("ld1d z13.d, p3/z, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 13 }, /*pg=*/3 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000008, 0x0000000000000009, 0x0000000000000010,
                  0x0000000000000011, 0x0000000000000012, 0x0000000000000013,
                  0x0000000000000014, 0x0000000000000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/8,
        },
        {
            "ldnt1d scalar+scalar",
            TEST_FUNC("ldnt1d z17.d, p2/z, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 17 }, /*pg=*/2 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000002, 0x0000000000000003, 0x0000000000000004,
                  0x0000000000000005, 0x0000000000000006, 0x0000000000000007,
                  0x0000000000000008, 0x0000000000000009 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/2,
        },
        // Load and replicate instructions
        {
            "ld1rqb scalar+scalar",
            TEST_FUNC("ld1rqb z21.b, p1/z, [%[base], %[index]]"),
            { /*zt=*/ { 21 }, /*pg=*/1 },
            std::array<std::array<uint8_t, 64>, 1> {
                { 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                  0x17, 0x18, 0x19, 0x20, 0x21, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                  0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x06,
                  0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                  0x18, 0x19, 0x20, 0x21, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12,
                  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/6,
        },
        {
            "ld1rqh scalar+scalar",
            TEST_FUNC("ld1rqh z25.h, p0/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 25 }, /*pg=*/0 },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019,
                  0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019,
                  0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019,
                  0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/12,
        },
        {
            "ld1rqw scalar+scalar",
            TEST_FUNC("ld1rqw z29.s, p1/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 29 }, /*pg=*/1 },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000020, 0x00000021, 0x00000022, 0x00000023, 0x00000020, 0x00000021,
                  0x00000022, 0x00000023, 0x00000020, 0x00000021, 0x00000022, 0x00000023,
                  0x00000020, 0x00000021, 0x00000022, 0x00000023 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/-12,
        },
        {
            "ld1rqd scalar+scalar",
            TEST_FUNC("ld1rqd z31.d, p2/z, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 31 }, /*pg=*/2 },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0xfffffffffffffff6, 0xfffffffffffffff5, 0xfffffffffffffff6,
                  0xfffffffffffffff5, 0xfffffffffffffff6, 0xfffffffffffffff5,
                  0xfffffffffffffff6, 0xfffffffffffffff5 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/-6,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld2_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_load_test_case_t<2>::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_load_test_case_t<2>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2}, pg},
         *     Expected output data,
         *     Base pointer (value for Xn),
         *     Index (value for Xm),
         * },
         */
        {
            "ld2b scalar+scalar",
            TEST_FUNC("ld2b {z4.b, z5.b}, p7/z, [%[base], %[index]]"),
            { /*zt1=*/ { 4, 5 }, /*pg=*/7 },
            std::array<std::array<uint8_t, 64>, 2> {
                { { // Zt1 data
                    0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20,
                    0x22, 0xf8, 0xf6, 0xf4, 0xf2, 0x00, 0x02, 0x04, 0x06, 0x08, 0x10,
                    0x12, 0x14, 0x16, 0x18, 0x20, 0x22, 0xf8, 0xf6, 0xf4, 0xf2, 0x00,
                    0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20, 0x22,
                    0xf8, 0xf6, 0xf4, 0xf2, 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12,
                    0x14, 0x16, 0x18, 0x20, 0x22, 0xf8, 0xf6, 0xf4, 0xf2 },
                  { // Zt2 data
                    0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21,
                    0x23, 0xf7, 0xf5, 0xf3, 0xf1, 0x01, 0x03, 0x05, 0x07, 0x09, 0x11,
                    0x13, 0x15, 0x17, 0x19, 0x21, 0x23, 0xf7, 0xf5, 0xf3, 0xf1, 0x01,
                    0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21, 0x23,
                    0xf7, 0xf5, 0xf3, 0xf1, 0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13,
                    0x15, 0x17, 0x19, 0x21, 0x23, 0xf7, 0xf5, 0xf3, 0xf1 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/0,
        },
        {
            "ld2h scalar+scalar",
            TEST_FUNC("ld2h {z12.h, z13.h}, p6/z, [%[base], %[index], lsl #1]"),
            { /*zt1=*/ { 12, 13 }, /*pg=*/6 },
            std::array<std::array<uint16_t, 32>, 2> {
                { { // Zt1 data
                    0x0016, 0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2,
                    0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x0010, 0x0012, 0x0014,
                    0x0016, 0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2,
                    0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x0010, 0x0012, 0x0014 },
                  { // Zt2 data
                    0x0017, 0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1,
                    0x0001, 0x0003, 0x0005, 0x0007, 0x0009, 0x0011, 0x0013, 0x0015,
                    0x0017, 0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1,
                    0x0001, 0x0003, 0x0005, 0x0007, 0x0009, 0x0011, 0x0013, 0x0015 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/-16,
        },
        {
            "ld2w scalar+scalar",
            TEST_FUNC("ld2w {z24.s, z25.s}, p5/z, [%[base], %[index], lsl #2]"),
            { /*zt1=*/ { 24, 25 }, /*pg=*/5 },
            std::array<std::array<uint32_t, 16>, 2> {
                { { // Zt1 data
                    0x00000008, 0x00000010, 0x00000012, 0x00000014, 0x00000016,
                    0x00000018, 0x00000020, 0x00000022, 0xfffffff8, 0xfffffff6,
                    0xfffffff4, 0xfffffff2, 0x00000000, 0x00000002, 0x00000004,
                    0x00000006 },
                  { // Zt2 data
                    0x00000009, 0x00000011, 0x00000013, 0x00000015, 0x00000017,
                    0x00000019, 0x00000021, 0x00000023, 0xfffffff7, 0xfffffff5,
                    0xfffffff3, 0xfffffff1, 0x00000001, 0x00000003, 0x00000005,
                    0x00000007 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/8,
        },
        {
            "ld2d scalar+scalar",
            TEST_FUNC("ld2d {z31.d, z0.d}, p4/z, [%[base], %[index], lsl #3]"),
            { /*zt1=*/ { 31, 0 }, /*pg=*/4 },
            std::array<std::array<uint64_t, 8>, 2> {
                { { // Zt1 data
                    0xfffffffffffffff7, 0xfffffffffffffff5, 0xfffffffffffffff3,
                    0xfffffffffffffff1, 0x0000000000000001, 0x0000000000000003,
                    0x0000000000000005, 0x0000000000000007 },
                  { // Zt2 data
                    0xfffffffffffffff6, 0xfffffffffffffff4, 0xfffffffffffffff2,
                    0x0000000000000000, 0x0000000000000002, 0x0000000000000004,
                    0x0000000000000006, 0x0000000000000008 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/25,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld3_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_load_test_case_t<3>::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_load_test_case_t<3>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3}, pg},
         *     Expected output data,
         *     Base pointer (value for Xn),
         *     Index (value for Xm),
         * },
         */
        {
            "ld3b scalar+scalar",
            TEST_FUNC("ld3b {z4.b, z5.b, z6.b}, p3/z, [%[base], %[index]]"),
            { /*zt=*/ { 4, 5, 6 }, /*pg=*/3 },
            std::array<std::array<uint8_t, 64>, 3> {
                { { // Zt1 data
                    0x00, 0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2,
                    0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                    0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                    0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                    0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                    0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3 },
                  { // Zt2 data
                    0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                    0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                    0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                    0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                    0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00, 0x03,
                    0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2 },
                  { // Z3 data
                    0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                    0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                    0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                    0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00, 0x03,
                    0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01, 0x04,
                    0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1 }

                } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/0,
        },
        {
            "ld3h scalar+scalar",
            TEST_FUNC("ld3h {z12.h, z13.h, z14.h}, p2/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 12, 13, 14 }, /*pg=*/2 },
            std::array<std::array<uint16_t, 32>, 3> {
                { { // Zt1 data
                    0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005,
                    0x0008, 0x0011, 0x0014, 0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3,
                    0x0000, 0x0003, 0x0006, 0x0009, 0x0012, 0x0015, 0x0018, 0x0021,
                    0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007, 0x0010, 0x0013 },
                  { // Zt2 data
                    0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006,
                    0x0009, 0x0012, 0x0015, 0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2,
                    0x0001, 0x0004, 0x0007, 0x0010, 0x0013, 0x0016, 0x0019, 0x0022,
                    0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005, 0x0008, 0x0011, 0x0014 },
                  { // Zt3 data
                    0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007,
                    0x0010, 0x0013, 0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1,
                    0x0002, 0x0005, 0x0008, 0x0011, 0x0014, 0x0017, 0x0020, 0x0023,
                    0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006, 0x0009, 0x0012, 0x0015

                  } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/-16,
        },
        {
            "ld3w scalar+scalar",
            TEST_FUNC("ld3w {z24.s, z25.s, z26.s}, p1/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 24, 25, 26 }, /*pg=*/1 },
            std::array<std::array<uint32_t, 16>, 3> {
                { { // Zt1 data
                    0x00000008, 0x00000011, 0x00000014, 0x00000017, 0x00000020,
                    0x00000023, 0xfffffff6, 0xfffffff3, 0x00000000, 0x00000003,
                    0x00000006, 0x00000009, 0x00000012, 0x00000015, 0x00000018,
                    0x00000021 },
                  { // Zt2 data
                    0x00000009, 0x00000012, 0x00000015, 0x00000018, 0x00000021,
                    0xfffffff8, 0xfffffff5, 0xfffffff2, 0x00000001, 0x00000004,
                    0x00000007, 0x00000010, 0x00000013, 0x00000016, 0x00000019, 0x00000022

                  },
                  { // Zt3 data
                    0x00000010, 0x00000013, 0x00000016, 0x00000019, 0x00000022,
                    0xfffffff7, 0xfffffff4, 0xfffffff1, 0x00000002, 0x00000005,
                    0x00000008, 0x00000011, 0x00000014, 0x00000017, 0x00000020,
                    0x00000023 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/8,
        },
        {
            "ld3d scalar+scalar",
            TEST_FUNC("ld3d {z30.d, z31.d, z0.d}, p0/z, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 30, 31, 0 }, /*pg=*/0 },
            std::array<std::array<uint64_t, 8>, 3> {
                { { // Zt1 data
                    0xfffffffffffffff7, 0xfffffffffffffff4, 0xfffffffffffffff1,
                    0x0000000000000002, 0x0000000000000005, 0x0000000000000008,
                    0x0000000000000011, 0x0000000000000014 },
                  { // Zt2 data
                    0xfffffffffffffff6, 0xfffffffffffffff3, 0x0000000000000000,
                    0x0000000000000003, 0x0000000000000006, 0x0000000000000009,
                    0x0000000000000012, 0x0000000000000015 },
                  { // Zt3 data
                    0xfffffffffffffff5, 0xfffffffffffffff2, 0x0000000000000001,
                    0x0000000000000004, 0x0000000000000007, 0x0000000000000010,
                    0x0000000000000013, 0x0000000000000016 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/25,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld4_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_load_test_case_t<4>::test_ptrs_t &ptrs) {     \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_load_test_case_t<4>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3, zt4}, pg},
         *     Expected output data,
         *     Base pointer (value for Xn),
         *     Index (value for Xm),
         * },
         */
        {
            "ld4b scalar+scalar",
            TEST_FUNC("ld4b {z4.b, z5.b, z6.b, z7.b}, p7/z, [%[base], %[index]]"),
            { /*zt=*/ { 4, 5, 6, 7 }, /*pg=*/7 },
            std::array<std::array<uint8_t, 64>, 4> {
                { { // Zt1 data
                    0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08,
                    0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20,
                    0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00,
                    0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12,
                    0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8,
                    0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4 },
                  { // Zt2 data
                    0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09,
                    0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21,
                    0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01,
                    0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13,
                    0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7,
                    0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3 },
                  { // Z3 data
                    0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10,
                    0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22,
                    0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02,
                    0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14,
                    0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6,
                    0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2 },
                  { // Z4 data
                    0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11,
                    0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23,
                    0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03,
                    0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15,
                    0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5,
                    0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1 }

                } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
            /*index=*/0,
        },
        {
            "ld4h scalar+scalar",
            TEST_FUNC(
                "ld4h {z12.h, z13.h, z14.h, z15.h}, p5/z, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 12, 13, 14, 15 }, /*pg=*/5 },
            std::array<std::array<uint16_t, 32>, 4> {
                { { // Zt1 data
                    0x0016, 0x0020, 0xfff8, 0xfff4, 0x0000, 0x0004, 0x0008, 0x0012,
                    0x0016, 0x0020, 0xfff8, 0xfff4, 0x0000, 0x0004, 0x0008, 0x0012,
                    0x0016, 0x0020, 0xfff8, 0xfff4, 0x0000, 0x0004, 0x0008, 0x0012,
                    0x0016, 0x0020, 0xfff8, 0xfff4, 0x0000, 0x0004, 0x0008, 0x0012 },
                  { // Zt2 data
                    0x0017, 0x0021, 0xfff7, 0xfff3, 0x0001, 0x0005, 0x0009, 0x0013,
                    0x0017, 0x0021, 0xfff7, 0xfff3, 0x0001, 0x0005, 0x0009, 0x0013,
                    0x0017, 0x0021, 0xfff7, 0xfff3, 0x0001, 0x0005, 0x0009, 0x0013,
                    0x0017, 0x0021, 0xfff7, 0xfff3, 0x0001, 0x0005, 0x0009, 0x0013

                  },
                  { // Zt3 data
                    0x0018, 0x0022, 0xfff6, 0xfff2, 0x0002, 0x0006, 0x0010, 0x0014,
                    0x0018, 0x0022, 0xfff6, 0xfff2, 0x0002, 0x0006, 0x0010, 0x0014,
                    0x0018, 0x0022, 0xfff6, 0xfff2, 0x0002, 0x0006, 0x0010, 0x0014,
                    0x0018, 0x0022, 0xfff6, 0xfff2, 0x0002, 0x0006, 0x0010, 0x0014 },
                  { // Zt4 data
                    0x0019, 0x0023, 0xfff5, 0xfff1, 0x0003, 0x0007, 0x0011, 0x0015,
                    0x0019, 0x0023, 0xfff5, 0xfff1, 0x0003, 0x0007, 0x0011, 0x0015,
                    0x0019, 0x0023, 0xfff5, 0xfff1, 0x0003, 0x0007, 0x0011, 0x0015,
                    0x0019, 0x0023, 0xfff5, 0xfff1, 0x0003, 0x0007, 0x0011, 0x0015 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
            /*index=*/-16,
        },
        {
            "ld4w scalar+scalar",
            TEST_FUNC(
                "ld4w {z24.s, z25.s, z26.s, z27.s}, p3/z, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 24, 25, 26, 27 }, /*pg=*/3 },
            std::array<std::array<uint32_t, 16>, 4> {
                { { // Zt1 data
                    0x00000008, 0x00000012, 0x00000016, 0x00000020, 0xfffffff8,
                    0xfffffff4, 0x00000000, 0x00000004, 0x00000008, 0x00000012,
                    0x00000016, 0x00000020, 0xfffffff8, 0xfffffff4, 0x00000000,
                    0x00000004 },
                  { // Zt2 data
                    0x00000009, 0x00000013, 0x00000017, 0x00000021, 0xfffffff7,
                    0xfffffff3, 0x00000001, 0x00000005, 0x00000009, 0x00000013,
                    0x00000017, 0x00000021, 0xfffffff7, 0xfffffff3, 0x00000001,
                    0x00000005 },
                  { // Zt3 data
                    0x00000010, 0x00000014, 0x00000018, 0x00000022, 0xfffffff6,
                    0xfffffff2, 0x00000002, 0x00000006, 0x00000010, 0x00000014,
                    0x00000018, 0x00000022, 0xfffffff6, 0xfffffff2, 0x00000002,
                    0x00000006 },
                  { // Zt4 data
                    0x00000011, 0x00000015, 0x00000019, 0x00000023, 0xfffffff5,
                    0xfffffff1, 0x00000003, 0x00000007, 0x00000011, 0x00000015,
                    0x00000019, 0x00000023, 0xfffffff5, 0xfffffff1, 0x00000003, 0x00000007

                  } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
            /*index=*/8,
        },
        {
            "ld4d scalar+scalar",
            TEST_FUNC(
                "ld4d {z30.d, z31.d, z0.d, z1.d}, p1/z, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 30, 31, 0, 1 }, /*pg=*/1 },
            std::array<std::array<uint64_t, 8>, 4> {
                { { // Zt1 data
                    0xfffffffffffffff7, 0xfffffffffffffff3, 0x0000000000000001,
                    0x0000000000000005, 0x0000000000000009, 0x0000000000000013,
                    0x0000000000000017, 0x0000000000000021 },
                  { // Zt2 data
                    0xfffffffffffffff6, 0xfffffffffffffff2, 0x0000000000000002,
                    0x0000000000000006, 0x0000000000000010, 0x0000000000000014,
                    0x0000000000000018, 0x0000000000000022 },
                  { // Zt3 data
                    0xfffffffffffffff5, 0xfffffffffffffff1, 0x0000000000000003,
                    0x0000000000000007, 0x0000000000000011, 0x0000000000000015,
                    0x0000000000000019, 0x0000000000000023 },
                  { // Zt4 data
                    0xfffffffffffffff4, 0x0000000000000000, 0x0000000000000004,
                    0x0000000000000008, 0x0000000000000012, 0x0000000000000016,
                    0x0000000000000020, 0xfffffffffffffff8 } } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
            /*index=*/25,
        },
    });
#    undef TEST_FUNC
}

template <size_t NUM_ZT>
struct scalar_plus_scalar_store_test_case_t
    : public test_case_base_t<scalar_plus_scalar_test_ptrs_t> {
    std::vector<uint8_t> reference_data_;
    struct registers_used_t {
        std::array<unsigned, NUM_ZT> src_z;
        unsigned governing_p;
    } registers_used_;

    void *base_;
    int64_t index_;

    element_size_t stored_value_size_;

    template <typename VALUE_T, size_t NUM_VALUES>
    scalar_plus_scalar_store_test_case_t(std::string name, test_func_t func,
                                         registers_used_t registers_used,
                                         std::array<VALUE_T, NUM_VALUES> reference_data,
                                         int64_t index)
        : test_case_base_t<test_ptrs_t>(
              std::move(name), std::move(func), registers_used.governing_p,
              static_cast<element_size_t>(TEST_VL_BYTES / (NUM_VALUES / NUM_ZT)))
        , registers_used_(registers_used)
        , base_(OUTPUT_DATA.base_addr())
        , index_(index)
        , stored_value_size_(static_cast<element_size_t>(sizeof(VALUE_T)))
    {
        const auto num_copies = get_vl_bytes() / TEST_VL_BYTES;
        const auto copy_length = sizeof(VALUE_T) * reference_data.size();
        reference_data_.resize(copy_length * num_copies);
        for (size_t i = 0; i < num_copies; i++) {
            std::memcpy(&reference_data_[i * copy_length], reference_data.data(),
                        copy_length);
        }
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        static constexpr std::array<vector_reg_value128_t, 4> DATA_TO_WRITE { {
            { // Zt1 data
              0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
              0x12, 0x13, 0x14, 0x15 },
            { // Zt2 data
              0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
              0x28, 0x29, 0x30, 0x31 },
            { // Zt3 data
              0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43,
              0x44, 0x45, 0x46, 0x47 },
            { // Zt4 data
              0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
              0x60, 0x61, 0x62, 0x63 },
        } };
        assert(NUM_ZT <= DATA_TO_WRITE.size());

        for (size_t zt = 0; zt < NUM_ZT; zt++) {
            register_values.set_z_register_value(registers_used_.src_z[zt],
                                                 DATA_TO_WRITE[zt]);
        }

        if (test_status_ == test_result_t::PASS) {
            const auto stored_value_bytes = static_cast<size_t>(stored_value_size_);
            memset(static_cast<uint8_t *>(base_) + (index_ * stored_value_bytes), 0xAB,
                   reference_data_.size());
        } else {
            OUTPUT_DATA.reset();
        }
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        // Check that the values of the Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }

        const auto vl_bytes = get_vl_bytes();

        std::vector<uint8_t> expected_output_data(reference_data_);

        const auto stored_value_bytes = static_cast<size_t>(stored_value_size_);
        const auto element_size_bytes = static_cast<size_t>(element_size_);

        const auto num_vector_elements = vl_bytes / element_size_bytes;
        const auto num_mask_elements = TEST_VL_BYTES / element_size_bytes;
        for (size_t i = 0; i < num_vector_elements; i++) {
            if (!element_is_active(i % num_mask_elements, pred, element_size_)) {
                // Element is inactive, set it to the poison value.
                memset(&expected_output_data[NUM_ZT * stored_value_bytes * i], 0xAB,
                       NUM_ZT * stored_value_bytes);
            }
        }

        const scalable_reg_value_t expected_output {
            expected_output_data.data(),
            expected_output_data.size(),
        };

        const scalable_reg_value_t output_value {
            static_cast<uint8_t *>(base_) + (index_ * stored_value_bytes),
            expected_output_data.size(),
        };

        if (output_value != expected_output) {
            test_failed();
            print("predicate: ");
            print_predicate(
                register_data.before.get_p_register_value(registers_used_.governing_p));
            print("\nexpected:  ");
            print_vector(expected_output);
            print("\nactual:    ");
            print_vector(output_value);
            print("\n");
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            base_,
            index_,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_st1_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_store_test_case_t<1>::test_ptrs_t &ptrs) {    \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_store_test_case_t<1>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        // ST1B instructions.
        {
            "st1b scalar+scalar 8bit element",
            TEST_FUNC("st1b z4.b, p7, [%[base], %[index]]"),
            { /*zt=*/ { 4 }, /*pg=*/7 },
            std::array<uint8_t, 16> { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                      0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 },
            /*index=*/0,
        },
        {
            "st1b scalar+scalar 16bit element",
            TEST_FUNC("st1b z8.h, p6, [%[base], %[index]]"),
            { /*zt=*/ { 8 }, /*pg=*/6 },
            std::array<uint8_t, 8> { 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14 },
            /*index=*/-1,
        },
        {
            "st1b scalar+scalar 32bit element",
            TEST_FUNC("st1b z12.s, p5, [%[base], %[index]]"),
            { /*zt=*/ { 12 }, /*pg=*/5 },
            std::array<uint8_t, 4> { 0x00, 0x04, 0x08, 0x12 },
            /*index=*/5,
        },
        {
            "st1b scalar+scalar 64bit element",
            TEST_FUNC("st1b z16.d, p4, [%[base], %[index]]"),
            { /*zt=*/ { 16 }, /*pg=*/4 },
            std::array<uint8_t, 2> { 0x0, 0x8 },
            /*index=*/9,
        },
        // LD1H
        {
            "st1h scalar+scalar 16bit element",
            TEST_FUNC("st1h z31.h, p0, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 31 }, /*pg=*/0 },
            std::array<uint16_t, 8> { 0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x1110,
                                      0x1312, 0x1514 },
            /*index=*/6,
        },
        {
            "st1h scalar+scalar 32bit element",
            TEST_FUNC("st1h z27.s, p1, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 27 }, /*pg=*/1 },
            std::array<uint16_t, 4> { 0x0100, 0x0504, 0x0908, 0x1312 },
            /*index=*/9,
        },
        {
            "st1h scalar+scalar 64bit element",
            TEST_FUNC("st1h z23.d, p2, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 23 }, /*pg=*/2 },
            std::array<uint16_t, 2> { 0x0100, 0x0908 },
            /*index=*/-2,
        },
        // ST1W
        {
            "st1w scalar+scalar 32bit element",
            TEST_FUNC("st1w z11.s, p5, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 11 }, /*pg=*/5 },
            std::array<uint32_t, 4> { 0x03020100, 0x07060504, 0x11100908, 0x15141312 },
            /*index=*/16,
        },
        {
            "st1w scalar+scalar 64bit element",
            TEST_FUNC("st1w z7.d, p6, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 7 }, /*pg=*/6 },
            std::array<uint32_t, 2> { 0x03020100, 0x11100908 },
            /*index=*/-1,
        },
        // ST1D
        {
            "st1d scalar+scalar",
            TEST_FUNC("st1d z1.d, p7, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 1 }, /*pg=*/7 },
            std::array<uint64_t, 2> { 0x0706050403020100, 0x1514131211100908 },
            /*index=*/8,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st2_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_store_test_case_t<2>::test_ptrs_t &ptrs) {    \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_store_test_case_t<2>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        {
            "st2b scalar+scalar",
            TEST_FUNC("st2b {z4.b, z5.b}, p7, [%[base], %[index]]"),
            { /*zt=*/ { 4, 5 }, /*pg=*/7 },
            std::array<uint8_t, 32> { 0x00, 0x16, 0x01, 0x17, 0x02, 0x18, 0x03, 0x19,
                                      0x04, 0x20, 0x05, 0x21, 0x06, 0x22, 0x07, 0x23,
                                      0x08, 0x24, 0x09, 0x25, 0x10, 0x26, 0x11, 0x27,
                                      0x12, 0x28, 0x13, 0x29, 0x14, 0x30, 0x15, 0x31 },
            /*index=*/0,
        },
        {
            "st2h scalar+scalar",
            TEST_FUNC("st2h {z7.h, z8.h}, p6, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 7, 8 }, /*pg=*/6 },
            std::array<uint16_t, 16> { 0x0100, 0x1716, 0x0302, 0x1918, 0x0504, 0x2120,
                                       0x0706, 0x2322, 0x0908, 0x2524, 0x1110, 0x2726,
                                       0x1312, 0x2928, 0x1514, 0x3130 },
            /*index=*/7,
        },
        {
            "st2w scalar+scalar",
            TEST_FUNC("st2w {z31.s, z0.s}, p5, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 31, 0 }, /*pg=*/5 },
            std::array<uint32_t, 8> { 0x03020100, 0x19181716, 0x07060504, 0x23222120,
                                      0x11100908, 0x27262524, 0x15141312, 0x31302928 },
            /*index=*/7,
        },
        {
            "st2d scalar+scalar",
            TEST_FUNC("st2d {z17.d, z18.d}, p4, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 17, 18 }, /*pg=*/4 },
            std::array<uint64_t, 4> { 0x0706050403020100, 0x2322212019181716,
                                      0x1514131211100908, 0x3130292827262524 },
            /*index=*/7,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st3_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_store_test_case_t<3>::test_ptrs_t &ptrs) {    \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_store_test_case_t<3>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        {
            "st3b scalar+scalar",
            TEST_FUNC("st3b {z4.b, z5.b, z6.b}, p3, [%[base], %[index]]"),
            { /*zt=*/ { 4, 5, 6 }, /*pg=*/3 },
            std::array<uint8_t, 48> {
                0x00, 0x16, 0x32, 0x01, 0x17, 0x33, 0x02, 0x18, 0x34, 0x03, 0x19, 0x35,
                0x04, 0x20, 0x36, 0x05, 0x21, 0x37, 0x06, 0x22, 0x38, 0x07, 0x23, 0x39,
                0x08, 0x24, 0x40, 0x09, 0x25, 0x41, 0x10, 0x26, 0x42, 0x11, 0x27, 0x43,
                0x12, 0x28, 0x44, 0x13, 0x29, 0x45, 0x14, 0x30, 0x46, 0x15, 0x31, 0x47 },
            /*index=*/0,
        },
        {
            "st3h scalar+scalar",
            TEST_FUNC("st3h {z7.h, z8.h, z9.h}, p2, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 7, 8, 9 }, /*pg=*/2 },
            std::array<uint16_t, 24> { 0x0100, 0x1716, 0x3332, 0x0302, 0x1918, 0x3534,
                                       0x0504, 0x2120, 0x3736, 0x0706, 0x2322, 0x3938,
                                       0x0908, 0x2524, 0x4140, 0x1110, 0x2726, 0x4342,
                                       0x1312, 0x2928, 0x4544, 0x1514, 0x3130, 0x4746 },
            /*index=*/17,
        },
        {
            "st3w scalar+scalar",
            TEST_FUNC("st3w {z31.s, z0.s, z1.s}, p1, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 31, 0, 1 }, /*pg=*/1 },
            std::array<uint32_t, 12> { 0x03020100, 0x19181716, 0x35343332, 0x07060504,
                                       0x23222120, 0x39383736, 0x11100908, 0x27262524,
                                       0x43424140, 0x15141312, 0x31302928, 0x47464544 },
            /*index=*/-17,
        },
        {
            "st3d scalar+scalar",
            TEST_FUNC("st3d {z17.d, z18.d, z19.d}, p0, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 17, 18, 19 }, /*pg=*/0 },
            std::array<uint64_t, 6> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x1514131211100908,
                                      0x3130292827262524, 0x4746454443424140 },
            /*index=*/16,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st4_scalar_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                       \
        [](scalar_plus_scalar_store_test_case_t<4>::test_ptrs_t &ptrs) {    \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */ \
                :                                                           \
                : [base] "r"(ptrs.base), [index] "r"(ptrs.index),           \
                  [z_restore_base] "r"(ptrs.z_restore_base),                \
                  [z_save_base] "r"(ptrs.z_save_base),                      \
                  [p_restore_base] "r"(ptrs.p_restore_base),                \
                  [p_save_base] "r"(ptrs.p_save_base)                       \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                        \
        }

    return run_tests<scalar_plus_scalar_store_test_case_t<4>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3, zt4}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        {
            "st4b scalar+scalar",
            TEST_FUNC("st4b {z4.b, z5.b, z6.b, z7.b}, p0, [%[base], %[index]]"),
            { /*zt=*/ { 4, 5, 6, 7 }, /*pg=*/0 },
            std::array<uint8_t, 64> {
                0x00, 0x16, 0x32, 0x48, 0x01, 0x17, 0x33, 0x49, 0x02, 0x18, 0x34,
                0x50, 0x03, 0x19, 0x35, 0x51, 0x04, 0x20, 0x36, 0x52, 0x05, 0x21,
                0x37, 0x53, 0x06, 0x22, 0x38, 0x54, 0x07, 0x23, 0x39, 0x55, 0x08,
                0x24, 0x40, 0x56, 0x09, 0x25, 0x41, 0x57, 0x10, 0x26, 0x42, 0x58,
                0x11, 0x27, 0x43, 0x59, 0x12, 0x28, 0x44, 0x60, 0x13, 0x29, 0x45,
                0x61, 0x14, 0x30, 0x46, 0x62, 0x15, 0x31, 0x47, 0x63 },
            /*index=*/0,
        },
        {
            "st4h scalar+scalar",
            TEST_FUNC("st4h {z7.h, z8.h, z9.h, z10.h}, p2, [%[base], %[index], lsl #1]"),
            { /*zt=*/ { 7, 8, 9, 10 }, /*pg=*/2 },
            std::array<uint16_t, 32> {
                0x0100, 0x1716, 0x3332, 0x4948, 0x0302, 0x1918, 0x3534, 0x5150,
                0x0504, 0x2120, 0x3736, 0x5352, 0x0706, 0x2322, 0x3938, 0x5554,
                0x0908, 0x2524, 0x4140, 0x5756, 0x1110, 0x2726, 0x4342, 0x5958,
                0x1312, 0x2928, 0x4544, 0x6160, 0x1514, 0x3130, 0x4746, 0x6362 },
            /*index=*/20,
        },
        {
            "st4w scalar+scalar",
            TEST_FUNC("st4w {z30.s, z31.s, z0.s, z1.s}, p4, [%[base], %[index], lsl #2]"),
            { /*zt=*/ { 30, 31, 0, 1 }, /*pg=*/4 },
            std::array<uint32_t, 16> { 0x03020100, 0x19181716, 0x35343332, 0x51504948,
                                       0x07060504, 0x23222120, 0x39383736, 0x55545352,
                                       0x11100908, 0x27262524, 0x43424140, 0x59585756,
                                       0x15141312, 0x31302928, 0x47464544, 0x63626160 },
            /*index=*/-20,
        },
        {
            "st4d scalar+scalar",
            TEST_FUNC(
                "st4d {z17.d, z18.d, z19.d, z20.d}, p6, [%[base], %[index], lsl #3]"),
            { /*zt=*/ { 17, 18, 19, 20 }, /*pg=*/6 },
            std::array<uint64_t, 8> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x5554535251504948,
                                      0x1514131211100908, 0x3130292827262524,
                                      0x4746454443424140, 0x6362616059585756 },
            /*index=*/9,
        },
    });
#    undef TEST_FUNC
}

template <size_t NUM_ZT>
struct scalar_plus_immediate_load_test_case_t
    : public test_case_base_t<test_ptrs_with_base_ptr_t> {
    std::array<std::vector<uint8_t>, NUM_ZT> reference_data_;

    struct registers_used_t {
        std::array<unsigned, NUM_ZT> dest_z;
        unsigned governing_p;
    } registers_used_;

    void *base_;

    template <typename ELEMENT_T>
    scalar_plus_immediate_load_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<std::array<ELEMENT_T, 16 / sizeof(ELEMENT_T)>, NUM_ZT>
            reference_data_128,
        std::array<std::array<ELEMENT_T, 32 / sizeof(ELEMENT_T)>, NUM_ZT>
            reference_data_256,
        std::array<std::array<ELEMENT_T, 64 / sizeof(ELEMENT_T)>, NUM_ZT>
            reference_data_512,
        void *base)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        static_cast<element_size_t>(sizeof(ELEMENT_T)))
        , registers_used_(registers_used)
        , base_(base)
    {
        const auto vl_bytes = get_vl_bytes();

        for (size_t zt = 0; zt < NUM_ZT; zt++) {
            reference_data_.at(zt).resize(vl_bytes);
            switch (vl_bytes) {
            case 16:
                assert(reference_data_128.at(zt).size() * sizeof(ELEMENT_T) == vl_bytes);
                memcpy(reference_data_.at(zt).data(), reference_data_128.at(zt).data(),
                       vl_bytes);
                break;
            case 32:
                assert(reference_data_256.at(zt).size() * sizeof(ELEMENT_T) == vl_bytes);
                memcpy(reference_data_.at(zt).data(), reference_data_256.at(zt).data(),
                       vl_bytes);
                break;
            case 64:
                assert(reference_data_512.at(zt).size() * sizeof(ELEMENT_T) == vl_bytes);
                memcpy(reference_data_.at(zt).data(), reference_data_512.at(zt).data(),
                       vl_bytes);
                break;
            default: print("Unsupported vector length: %lu\n", vl_bytes); exit(1);
            }
        }
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // No Z/P registers to set up. The base is passed to the test function
        // in the test_ptrs_t object.
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        for (size_t zt = 0; zt < NUM_ZT; zt++) {
            std::vector<uint8_t> expected_output_data(reference_data_.at(zt));
            apply_predicate_mask(expected_output_data, pred, element_size_);
            const scalable_reg_value_t expected_output {
                expected_output_data.data(),
                expected_output_data.size(),
            };

            const auto output_value =
                register_data.after.get_z_register_value(registers_used_.dest_z.at(zt));

            if (output_value != expected_output) {
                test_failed();
                if (NUM_ZT > 1)
                    print("Zt%u:\n", (unsigned)zt);
                print("predicate: ");
                print_predicate(register_data.before.get_p_register_value(
                    registers_used_.governing_p));
                print("\nexpected:  ");
                print_vector(expected_output);
                print("\nactual:    ");
                print_vector(output_value);
                print("\n");
            }
        }

        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            if (std::find(registers_used_.dest_z.begin(), registers_used_.dest_z.end(),
                          i) == registers_used_.dest_z.end())
                check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            base_,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_ld1_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_load_test_case_t<1>::test_ptrs_t &ptrs) {          \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_load_test_case_t<1>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt}, pg},
         *     Expected output data (128-bit vl),
         *     Expected output data (256-bit vl),
         *     Expected output data (512-bit vl),
         *     Base pointer (value for Xn),
         * },
         */
        // LD1B instructions
        {
            "ld1b scalar+immediate 8bit element",
            TEST_FUNC("ld1b z0.b, p7/z, [%[base], #0, mul vl]"),
            { /*zt=*/ { 0 }, /*pg=*/7 },
            std::array<std::array<uint8_t, 16>, 1> { { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                                       0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                                                       0x12, 0x13, 0x14, 0x15 } },
            std::array<std::array<uint8_t, 32>, 1> {
                { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            std::array<std::array<uint8_t, 64>, 1> {
                { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x00,
                  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                  0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
                  0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+immediate 16bit element",
            TEST_FUNC("ld1b z3.h, p4/z, [%[base], #1, mul vl]"),
            { /*zt=*/3, /*pg=*/4 },
            std::array<std::array<uint16_t, 8>, 1> {
                { 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015 } },
            std::array<std::array<uint16_t, 16>, 1> {
                { 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023, 0x00f8,
                  0x00f7, 0x00f6, 0x00f5, 0x00f4, 0x00f3, 0x00f2, 0x00f1 } },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
                  0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                  0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023,
                  0x00f8, 0x00f7, 0x00f6, 0x00f5, 0x00f4, 0x00f3, 0x00f2, 0x00f1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+immediate 32bit element",
            TEST_FUNC("ld1b z6.s, p1/z, [%[base], #2, mul vl]"),
            { /*zt=*/6, /*pg=*/1 },
            std::array<std::array<uint32_t, 4>, 1> {
                { 0x00000008, 0x00000009, 0x00000010, 0x00000011 } },
            std::array<std::array<uint32_t, 8>, 1> { { 0x00000016, 0x00000017, 0x00000018,
                                                       0x00000019, 0x00000020, 0x00000021,
                                                       0x00000022, 0x00000023 } },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
                  0x00000006, 0x00000007, 0x00000008, 0x00000009, 0x00000010, 0x00000011,
                  0x00000012, 0x00000013, 0x00000014, 0x00000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+immediate 64bit element",
            TEST_FUNC("ld1b z9.d, p2/z, [%[base], #3, mul vl]"),
            { /*zt=*/9, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000006, 0x0000000000000007 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000012, 0x0000000000000013, 0x0000000000000014,
                  0x0000000000000015 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x00000000000000f8, 0x00000000000000f7, 0x00000000000000f6,
                  0x00000000000000f5, 0x00000000000000f4, 0x00000000000000f3,
                  0x00000000000000f2, 0x00000000000000f1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1b z10.d, p3/z, [%[base], #-8, mul vl]"),
            { /*zt=*/10, /*pg=*/3 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1b scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1b z11.d, p4/z, [%[base], #7, mul vl]"),
            { /*zt=*/11, /*pg=*/4 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x00000000000000f4, 0x00000000000000f3, 0x00000000000000f2,
                  0x00000000000000f1 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x00000000000000f8, 0x00000000000000f7, 0x00000000000000f6,
                  0x00000000000000f5, 0x00000000000000f4, 0x00000000000000f3,
                  0x00000000000000f2, 0x00000000000000f1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ldnt1b scalar+immediate 8bit element",
            TEST_FUNC("ldnt1b z12.b, p5/z, [%[base], #4, mul vl]"),
            { /*zt=*/12, /*pg=*/5 },
            std::array<std::array<uint8_t, 16>, 1> { { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                                       0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                                                       0x12, 0x13, 0x14, 0x15 } },
            std::array<std::array<uint8_t, 32>, 1> {
                { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            std::array<std::array<uint8_t, 64>, 1> {
                { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x00,
                  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
                  0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
                  0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1SB instructions
        {
            "ld1sb scalar+immediate 16bit element",
            TEST_FUNC("ld1sb z15.h, p6/z, [%[base], #5, mul vl]"),
            { /*zt=*/15, /*pg=*/6 },
            std::array<std::array<int16_t, 8>, 1> {
                { 0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015 } },
            std::array<std::array<int16_t, 16>, 1> {
                { 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023, -8, -9,
                  -10, -11, -12, -13, -14, -15 } },
            std::array<std::array<int16_t, 32>, 1> {
                { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
                  0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                  0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023,
                  -8,     -9,     -10,    -11,    -12,    -13,    -14,    -15 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+immediate 32bit element",
            TEST_FUNC("ld1sb z18.s, p3/z, [%[base], #6, mul vl]"),
            { /*zt=*/18, /*pg=*/3 },
            std::array<std::array<int32_t, 4>, 1> { { -8, -9, -10, -11 } },
            std::array<std::array<int32_t, 8>, 1> { { 0x00000016, 0x00000017, 0x00000018,
                                                      0x00000019, 0x00000020, 0x00000021,
                                                      0x00000022, 0x00000023 } },
            std::array<std::array<int32_t, 16>, 1> {
                { 0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
                  0x00000006, 0x00000007, 0x00000008, 0x00000009, 0x00000010, 0x00000011,
                  0x00000012, 0x00000013, 0x00000014, 0x00000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+immediate 64bit element",
            TEST_FUNC("ld1sb z21.d, p0/z, [%[base], #-6, mul vl]"),
            { /*zt=*/21, /*pg=*/0 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000020, 0x0000000000000021 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000008, 0x0000000000000009, 0x0000000000000010,
                  0x0000000000000011 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000018,
                  0x0000000000000019, 0x0000000000000020, 0x0000000000000021,
                  0x0000000000000022, 0x0000000000000023 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1sb z22.d, p1/z, [%[base], #-8, mul vl]"),
            { /*zt=*/22, /*pg=*/1 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1sb scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1sb z23.d, p2/z, [%[base], #7, mul vl]"),
            { /*zt=*/23, /*pg=*/2 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<int64_t, 4>, 1> { { -12, -13, -14, -15 } },
            std::array<std::array<int64_t, 8>, 1> {
                { -8, -9, -10, -11, -12, -13, -14, -15 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD1H instructions
        {
            "ld1h scalar+immediate 16bit element",
            TEST_FUNC("ld1h z24.h, p3/z, [%[base], #-5, mul vl]"),
            { /*zt=*/24, /*pg=*/3 },
            std::array<std::array<uint16_t, 8>, 1> {
                { 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            std::array<std::array<uint16_t, 16>, 1> {
                { 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023, 0xfff8,
                  0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
                  0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                  0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023,
                  0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+immediate 32bit element",
            TEST_FUNC("ld1h z27.s, p6/z, [%[base], #-4, mul vl]"),
            { /*zt=*/27, /*pg=*/6 },
            std::array<std::array<uint32_t, 4>, 1> {
                { 0x00000016, 0x00000017, 0x00000018, 0x00000019 } },
            std::array<std::array<uint32_t, 8>, 1> { { 0x00000000, 0x00000001, 0x00000002,
                                                       0x00000003, 0x00000004, 0x00000005,
                                                       0x00000006, 0x00000007 } },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
                  0x00000006, 0x00000007, 0x00000008, 0x00000009, 0x00000010, 0x00000011,
                  0x00000012, 0x00000013, 0x00000014, 0x00000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+immediate 64bit element",
            TEST_FUNC("ld1h z30.d, p5/z, [%[base], #-3, mul vl]"),
            { /*zt=*/30, /*pg=*/5 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x000000000000fff6, 0x000000000000fff5 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000020, 0x0000000000000021, 0x0000000000000022,
                  0x0000000000000023 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000008, 0x0000000000000009, 0x0000000000000010,
                  0x0000000000000011, 0x0000000000000012, 0x0000000000000013,
                  0x0000000000000014, 0x0000000000000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1h z31.d, p4/z, [%[base], #-8, mul vl]"),
            { /*zt=*/31, /*pg=*/4 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1h scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1h z0.d, p3/z, [%[base], #7, mul vl]"),
            { /*zt=*/0, /*pg=*/3 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x000000000000fff4, 0x000000000000fff3, 0x000000000000fff2,
                  0x000000000000fff1 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x000000000000fff8, 0x000000000000fff7, 0x000000000000fff6,
                  0x000000000000fff5, 0x000000000000fff4, 0x000000000000fff3,
                  0x000000000000fff2, 0x000000000000fff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ldnt1h scalar+immediate 16bit element",
            TEST_FUNC("ldnt1h z1.h, p2/z, [%[base], #-2, mul vl]"),
            { /*zt=*/1, /*pg=*/2 },
            std::array<std::array<uint16_t, 8>, 1> {
                { 0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023 } },
            std::array<std::array<uint16_t, 16>, 1> {
                { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
                  0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015 } },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
                  0x0008, 0x0009, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                  0x0016, 0x0017, 0x0018, 0x0019, 0x0020, 0x0021, 0x0022, 0x0023,
                  0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        // LD1SH instructions
        {
            "ld1sh scalar+immediate 32bit element",
            TEST_FUNC("ld1sh z4.s, p1/z, [%[base], #-1, mul vl]"),
            { /*zt=*/4, /*pg=*/1 },
            std::array<std::array<int32_t, 4>, 1> { { -12, -13, -14, -15 } },
            std::array<std::array<int32_t, 8>, 1> {
                { -8, -9, -10, -11, -12, -13, -14, -15 } },
            std::array<std::array<int32_t, 16>, 1> {
                { 0x00000016, 0x00000017, 0x00000018, 0x00000019, 0x00000020, 0x00000021,
                  0x00000022, 0x00000023, -8, -9, -10, -11, -12, -13, -14, -15 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+immediate 64bit element",
            TEST_FUNC("ld1sh z7.d, p4/z, [%[base], #0, mul vl]"),
            { /*zt=*/7, /*pg=*/4 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000000, 0x0000000000000001 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1sh z8.d, p5/z, [%[base], #-8, mul vl]"),
            { /*zt=*/8, /*pg=*/5 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1sh scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1sh z9.d, p6/z, [%[base], #7, mul vl]"),
            { /*zt=*/9, /*pg=*/6 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<int64_t, 4>, 1> { { -12, -13, -14, -15 } },
            std::array<std::array<int64_t, 8>, 1> {
                { -8, -9, -10, -11, -12, -13, -14, -15 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        // LD1W instructions
        {
            "ld1w scalar+immediate 32bit element",
            TEST_FUNC("ld1w z10.s, p7/z, [%[base], #1, mul vl]"),
            { /*zt=*/10, /*pg=*/7 },
            std::array<std::array<uint32_t, 4>, 1> {
                { 0x00000004, 0x00000005, 0x00000006, 0x00000007 } },
            std::array<std::array<uint32_t, 8>, 1> { { 0x00000008, 0x00000009, 0x00000010,
                                                       0x00000011, 0x00000012, 0x00000013,
                                                       0x00000014, 0x00000015 } },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000016, 0x00000017, 0x00000018, 0x00000019, 0x00000020, 0x00000021,
                  0x00000022, 0x00000023, 0xfffffff8, 0xfffffff7, 0xfffffff6, 0xfffffff5,
                  0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+immediate 64bit element",
            TEST_FUNC("ld1w z13.d, p4/z, [%[base], #2, mul vl]"),
            { /*zt=*/13, /*pg=*/4 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000004, 0x0000000000000005 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000008, 0x0000000000000009, 0x0000000000000010,
                  0x0000000000000011 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000018,
                  0x0000000000000019, 0x0000000000000020, 0x0000000000000021,
                  0x0000000000000022, 0x0000000000000023 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1w z14.d, p3/z, [%[base], #-8, mul vl]"),
            { /*zt=*/14, /*pg=*/3 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1w scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1w z15.d, p2/z, [%[base], #7, mul vl]"),
            { /*zt=*/15, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x00000000fffffff4, 0x00000000fffffff3, 0x00000000fffffff2,
                  0x00000000fffffff1 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x00000000fffffff8, 0x00000000fffffff7, 0x00000000fffffff6,
                  0x00000000fffffff5, 0x00000000fffffff4, 0x00000000fffffff3,
                  0x00000000fffffff2, 0x00000000fffffff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ldnt1w scalar+immediate 32bit element",
            TEST_FUNC("ldnt1w z16.s, p1/z, [%[base], #3, mul vl]"),
            { /*zt=*/16, /*pg=*/1 },
            std::array<std::array<uint32_t, 4>, 1> {
                { 0x00000012, 0x00000013, 0x00000014, 0x00000015 } },
            std::array<std::array<uint32_t, 8>, 1> { { 0xfffffff8, 0xfffffff7, 0xfffffff6,
                                                       0xfffffff5, 0xfffffff4, 0xfffffff3,
                                                       0xfffffff2, 0xfffffff1 } },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0x00000016, 0x00000017, 0x00000018, 0x00000019, 0x00000020, 0x00000021,
                  0x00000022, 0x00000023, 0xfffffff8, 0xfffffff7, 0xfffffff6, 0xfffffff5,
                  0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        // LD1SW instructions
        {
            "ld1sw scalar+immediate 64bit element",
            TEST_FUNC("ld1sw z19.d, p2/z, [%[base], #4, mul vl]"),
            { /*zt=*/19, /*pg=*/2 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000008, 0x0000000000000009 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000018,
                  0x0000000000000019 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1sw z20.d, p3/z, [%[base], #-8, mul vl]"),
            { /*zt=*/20, /*pg=*/3 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<int64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<int64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1sw scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1sw z21.d, p4/z, [%[base], #7, mul vl]"),
            { /*zt=*/21, /*pg=*/4 },
            std::array<std::array<int64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<int64_t, 4>, 1> { { -12, -13, -14, -15 } },
            std::array<std::array<int64_t, 8>, 1> {
                { -8, -9, -10, -11, -12, -13, -14, -15 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },

        // LD1D instructions
        {
            "ld1d scalar+immediate 64bit element",
            TEST_FUNC("ld1d z22.d, p5/z, [%[base], #5, mul vl]"),
            { /*zt=*/22, /*pg=*/5 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000010, 0x0000000000000011 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000020, 0x0000000000000021, 0x0000000000000022,
                  0x0000000000000023 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000008, 0x0000000000000009, 0x0000000000000010,
                  0x0000000000000011, 0x0000000000000012, 0x0000000000000013,
                  0x0000000000000014, 0x0000000000000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d scalar+immediate 64bit element (min index)",
            TEST_FUNC("ld1d z23.d, p6/z, [%[base], #-8, mul vl]"),
            { /*zt=*/23, /*pg=*/6 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000000, 0x0000000000000001, 0x0000000000000002,
                  0x0000000000000003, 0x0000000000000004, 0x0000000000000005,
                  0x0000000000000006, 0x0000000000000007 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1d scalar+immediate 64bit element (max index)",
            TEST_FUNC("ld1d z24.d, p7/z, [%[base], #7, mul vl]"),
            { /*zt=*/24, /*pg=*/7 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0xfffffffffffffff4, 0xfffffffffffffff3, 0xfffffffffffffff2,
                  0xfffffffffffffff1 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0xfffffffffffffff8, 0xfffffffffffffff7, 0xfffffffffffffff6,
                  0xfffffffffffffff5, 0xfffffffffffffff4, 0xfffffffffffffff3,
                  0xfffffffffffffff2, 0xfffffffffffffff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ldnt1d scalar+immediate 64bit element",
            TEST_FUNC("ldnt1d z25.d, p6/z, [%[base], #6, mul vl]"),
            { /*zt=*/25, /*pg=*/6 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000012, 0x0000000000000013 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0xfffffffffffffff8, 0xfffffffffffffff7, 0xfffffffffffffff6,
                  0xfffffffffffffff5 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000018,
                  0x0000000000000019, 0x0000000000000020, 0x0000000000000021,
                  0x0000000000000022, 0x0000000000000023 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        // Load and replicate instructions
        {
            "ld1rqb scalar+immediate",
            TEST_FUNC("ld1rqb z26.b, p5/z, [%[base], #80]"),
            { /*zt=*/26, /*pg=*/5 },
            std::array<std::array<uint8_t, 16>, 1> { { 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                                                       0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5,
                                                       0xf4, 0xf3, 0xf2, 0xf1 } },
            std::array<std::array<uint8_t, 32>, 1> {
                { 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0xf8, 0xf7, 0xf6,
                  0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            std::array<std::array<uint8_t, 64>, 1> {
                { 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0xf8, 0xf7, 0xf6,
                  0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
                  0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0x16,
                  0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0xf8, 0xf7, 0xf6, 0xf5,
                  0xf4, 0xf3, 0xf2, 0xf1, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
                  0x23, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        {
            "ld1rqh scalar+immediate",
            TEST_FUNC("ld1rqh z27.h, p4/z, [%[base], #48]"),
            { /*zt=*/27, /*pg=*/4 },
            std::array<std::array<uint16_t, 8>, 1> {
                { 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            std::array<std::array<uint16_t, 16>, 1> { {

                0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1, 0xfff8,
                0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            std::array<std::array<uint16_t, 32>, 1> {
                { 0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1,
                  0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1,
                  0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1,
                  0xfff8, 0xfff7, 0xfff6, 0xfff5, 0xfff4, 0xfff3, 0xfff2, 0xfff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        {
            "ld1rqw scalar+immediate",
            TEST_FUNC("ld1rqw z28.s, p3/z, [%[base], #-16]"),
            { /*zt=*/28, /*pg=*/3 },
            std::array<std::array<uint32_t, 4>, 1> {
                { 0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1 } },
            std::array<std::array<uint32_t, 8>, 1> { { 0xfffffff4, 0xfffffff3, 0xfffffff2,
                                                       0xfffffff1, 0xfffffff4, 0xfffffff3,
                                                       0xfffffff2, 0xfffffff1 } },
            std::array<std::array<uint32_t, 16>, 1> {
                { 0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1, 0xfffffff4, 0xfffffff3,
                  0xfffffff2, 0xfffffff1, 0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1,
                  0xfffffff4, 0xfffffff3, 0xfffffff2, 0xfffffff1 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        {
            "ld1rqd scalar+immediate",
            TEST_FUNC("ld1rqd z29.d, p2/z, [%[base], #-32]"),
            { /*zt=*/29, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0xfffffffffffffff4, 0xfffffffffffffff3 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0xfffffffffffffff4, 0xfffffffffffffff3, 0xfffffffffffffff4,
                  0xfffffffffffffff3 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0xfffffffffffffff4, 0xfffffffffffffff3, 0xfffffffffffffff4,
                  0xfffffffffffffff3, 0xfffffffffffffff4, 0xfffffffffffffff3,
                  0xfffffffffffffff4, 0xfffffffffffffff3 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1rqd scalar+immediate (min index)",
            TEST_FUNC("ld1rqd z30.d, p1/z, [%[base], #-128]"),
            { /*zt=*/30, /*pg=*/1 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000016, 0x0000000000000017 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000016,
                  0x0000000000000017 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000016, 0x0000000000000017, 0x0000000000000016,
                  0x0000000000000017, 0x0000000000000016, 0x0000000000000017,
                  0x0000000000000016, 0x0000000000000017 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld1rqd scalar+immediate (max index)",
            TEST_FUNC("ld1rqd z31.d, p0/z, [%[base], #112]"),
            { /*zt=*/31, /*pg=*/0 },
            std::array<std::array<uint64_t, 2>, 1> {
                { 0x0000000000000014, 0x0000000000000015 } },
            std::array<std::array<uint64_t, 4>, 1> {
                { 0x0000000000000014, 0x0000000000000015, 0x0000000000000014,
                  0x0000000000000015 } },
            std::array<std::array<uint64_t, 8>, 1> {
                { 0x0000000000000014, 0x0000000000000015, 0x0000000000000014,
                  0x0000000000000015, 0x0000000000000014, 0x0000000000000015,
                  0x0000000000000014, 0x0000000000000015 } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld2_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_load_test_case_t<2>::test_ptrs_t &ptrs) {          \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_load_test_case_t<2>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2}, pg},
         *     Expected output data (128-bit vl),
         *     Expected output data (256-bit vl),
         *     Expected output data (512-bit vl),
         *     Base pointer (value for Xn),
         * },
         */
        // LD2B instructions
        {
            "ld2b scalar+immediate",
            TEST_FUNC("ld2b { z0.b, z1.b }, p7/z, [%[base], #0, mul vl]"),
            { /*zt=*/ { 0, 1 }, /*pg=*/7 },
            std::array<std::array<uint8_t, 16>, 2> { {
                { 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20, 0x22,
                  0xf8, 0xf6, 0xf4, 0xf2 },
                { 0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21, 0x23,
                  0xf7, 0xf5, 0xf3, 0xf1 },
            } },
            std::array<std::array<uint8_t, 32>, 2> { {
                { 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20,
                  0x22, 0xf8, 0xf6, 0xf4, 0xf2, 0x00, 0x02, 0x04, 0x06, 0x08, 0x10,
                  0x12, 0x14, 0x16, 0x18, 0x20, 0x22, 0xf8, 0xf6, 0xf4, 0xf2 },
                { 0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21,
                  0x23, 0xf7, 0xf5, 0xf3, 0xf1, 0x01, 0x03, 0x05, 0x07, 0x09, 0x11,
                  0x13, 0x15, 0x17, 0x19, 0x21, 0x23, 0xf7, 0xf5, 0xf3, 0xf1 },
            } },
            std::array<std::array<uint8_t, 64>, 2> { {
                { 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20,
                  0x22, 0xf8, 0xf6, 0xf4, 0xf2, 0x00, 0x02, 0x04, 0x06, 0x08, 0x10,
                  0x12, 0x14, 0x16, 0x18, 0x20, 0x22, 0xf8, 0xf6, 0xf4, 0xf2, 0x00,
                  0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14, 0x16, 0x18, 0x20, 0x22,
                  0xf8, 0xf6, 0xf4, 0xf2, 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12,
                  0x14, 0x16, 0x18, 0x20, 0x22, 0xf8, 0xf6, 0xf4, 0xf2 },
                { 0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21,
                  0x23, 0xf7, 0xf5, 0xf3, 0xf1, 0x01, 0x03, 0x05, 0x07, 0x09, 0x11,
                  0x13, 0x15, 0x17, 0x19, 0x21, 0x23, 0xf7, 0xf5, 0xf3, 0xf1, 0x01,
                  0x03, 0x05, 0x07, 0x09, 0x11, 0x13, 0x15, 0x17, 0x19, 0x21, 0x23,
                  0xf7, 0xf5, 0xf3, 0xf1, 0x01, 0x03, 0x05, 0x07, 0x09, 0x11, 0x13,
                  0x15, 0x17, 0x19, 0x21, 0x23, 0xf7, 0xf5, 0xf3, 0xf1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD2H instructions
        {
            "ld2h scalar+immediate",
            TEST_FUNC("ld2h { z3.h, z4.h }, p4/z, [%[base], #2, mul vl]"),
            { /*zt=*/ { 3, 4 }, /*pg=*/4 },
            std::array<std::array<uint16_t, 8>, 2> { {
                { 0x0016, 0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2 },
                { 0x0017, 0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1 },
            } },
            std::array<std::array<uint16_t, 16>, 2> { {
                { 0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x0010, 0x0012, 0x0014, 0x0016,
                  0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2 },
                { 0x0001, 0x0003, 0x0005, 0x0007, 0x0009, 0x0011, 0x0013, 0x0015, 0x0017,
                  0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1 },
            } },
            std::array<std::array<uint16_t, 32>, 2> { {
                { 0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x0010, 0x0012, 0x0014,
                  0x0016, 0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2,
                  0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x0010, 0x0012, 0x0014,
                  0x0016, 0x0018, 0x0020, 0x0022, 0xfff8, 0xfff6, 0xfff4, 0xfff2 },
                { 0x0001, 0x0003, 0x0005, 0x0007, 0x0009, 0x0011, 0x0013, 0x0015,
                  0x0017, 0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1,
                  0x0001, 0x0003, 0x0005, 0x0007, 0x0009, 0x0011, 0x0013, 0x0015,
                  0x0017, 0x0019, 0x0021, 0x0023, 0xfff7, 0xfff5, 0xfff3, 0xfff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        // LD2W instructions
        {
            "ld2w scalar+immediate",
            TEST_FUNC("ld2w { z6.s, z7.s }, p1/z, [%[base], #4, mul vl]"),
            { /*zt=*/ { 6, 7 }, /*pg=*/1 },
            std::array<std::array<uint32_t, 4>, 2> { {
                { 0x00000016, 0x00000018, 0x00000020, 0x00000022 },
                { 0x00000017, 0x00000019, 0x00000021, 0x00000023 },
            } },
            std::array<std::array<uint32_t, 8>, 2> { {
                { 0x00000000, 0x00000002, 0x00000004, 0x00000006, 0x00000008, 0x00000010,
                  0x00000012, 0x00000014 },
                { 0x00000001, 0x00000003, 0x00000005, 0x00000007, 0x00000009, 0x00000011,
                  0x00000013, 0x00000015 },
            } },
            std::array<std::array<uint32_t, 16>, 2> { {
                { 0x00000000, 0x00000002, 0x00000004, 0x00000006, 0x00000008, 0x00000010,
                  0x00000012, 0x00000014, 0x00000016, 0x00000018, 0x00000020, 0x00000022,
                  0xfffffff8, 0xfffffff6, 0xfffffff4, 0xfffffff2 },
                { 0x00000001, 0x00000003, 0x00000005, 0x00000007, 0x00000009, 0x00000011,
                  0x00000013, 0x00000015, 0x00000017, 0x00000019, 0x00000021, 0x00000023,
                  0xfffffff7, 0xfffffff5, 0xfffffff3, 0xfffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        // LD2D instructions
        {
            "ld2d scalar+immediate",
            TEST_FUNC("ld2d { z9.d, z10.d }, p2/z, [%[base], #6, mul vl]"),
            { /*zt=*/ { 9, 10 }, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 2> { {
                { 0x0000000000000012, 0x0000000000000014 },
                { 0x0000000000000013, 0x0000000000000015 },
            } },
            std::array<std::array<uint64_t, 4>, 2> { {
                { 0xfffffffffffffff8, 0xfffffffffffffff6, 0xfffffffffffffff4,
                  0xfffffffffffffff2 },
                { 0xfffffffffffffff7, 0xfffffffffffffff5, 0xfffffffffffffff3,
                  0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 8>, 2> { {
                { 0x0000000000000016, 0x0000000000000018, 0x0000000000000020,
                  0x0000000000000022, 0xfffffffffffffff8, 0xfffffffffffffff6,
                  0xfffffffffffffff4, 0xfffffffffffffff2 },
                { 0x0000000000000017, 0x0000000000000019, 0x0000000000000021,
                  0x0000000000000023, 0xfffffffffffffff7, 0xfffffffffffffff5,
                  0xfffffffffffffff3, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld2d scalar+immediate (min index)",
            TEST_FUNC("ld2d { z10.d, z11.d }, p3/z, [%[base], #-16, mul vl]"),
            { /*zt=*/ { 10, 11 }, /*pg=*/3 },
            std::array<std::array<uint64_t, 2>, 2> { {
                { 0x0000000000000000, 0x0000000000000002 },
                { 0x0000000000000001, 0x0000000000000003 },
            } },
            std::array<std::array<uint64_t, 4>, 2> { {
                { 0x0000000000000000, 0x0000000000000002, 0x0000000000000004,
                  0x0000000000000006 },
                { 0x0000000000000001, 0x0000000000000003, 0x0000000000000005,
                  0x0000000000000007 },
            } },
            std::array<std::array<uint64_t, 8>, 2> { {
                { 0x0000000000000000, 0x0000000000000002, 0x0000000000000004,
                  0x0000000000000006, 0x0000000000000008, 0x0000000000000010,
                  0x0000000000000012, 0x0000000000000014 },
                { 0x0000000000000001, 0x0000000000000003, 0x0000000000000005,
                  0x0000000000000007, 0x0000000000000009, 0x0000000000000011,
                  0x0000000000000013, 0x0000000000000015 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld2d scalar+immediate (max index)",
            TEST_FUNC("ld2d { z11.d, z12.d }, p4/z, [%[base], #14, mul vl]"),
            { /*zt=*/ { 11, 12 }, /*pg=*/4 },
            std::array<std::array<uint64_t, 2>, 2> { {
                { 0xfffffffffffffff4, 0xfffffffffffffff2 },
                { 0xfffffffffffffff3, 0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 4>, 2> { {
                { 0xfffffffffffffff8, 0xfffffffffffffff6, 0xfffffffffffffff4,
                  0xfffffffffffffff2 },
                { 0xfffffffffffffff7, 0xfffffffffffffff5, 0xfffffffffffffff3,
                  0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 8>, 2> { {
                { 0x0000000000000016, 0x0000000000000018, 0x0000000000000020,
                  0x0000000000000022, 0xfffffffffffffff8, 0xfffffffffffffff6,
                  0xfffffffffffffff4, 0xfffffffffffffff2 },
                { 0x0000000000000017, 0x0000000000000019, 0x0000000000000021,
                  0x0000000000000023, 0xfffffffffffffff7, 0xfffffffffffffff5,
                  0xfffffffffffffff3, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld3_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_load_test_case_t<3>::test_ptrs_t &ptrs) {          \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_load_test_case_t<3>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3}, pg},
         *     Expected output data (128-bit vl),
         *     Expected output data (256-bit vl),
         *     Expected output data (512-bit vl),
         *     Base pointer (value for Xn),
         * },
         */
        // LD3B instructions
        {
            "ld3b scalar+immediate",
            TEST_FUNC("ld3b { z12.b, z13.b, z14.b }, p5/z, [%[base], #12, mul vl]"),
            { /*zt=*/ { 12, 13, 14 }, /*pg=*/5 },
            std::array<std::array<uint8_t, 16>, 3> { {
                { 0x00, 0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                  0x04, 0x07, 0x10, 0x13 },
                { 0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                  0x05, 0x08, 0x11, 0x14 },
                { 0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00, 0x03,
                  0x06, 0x09, 0x12, 0x15 },
            } },
            std::array<std::array<uint8_t, 32>, 3> { {
                { 0x00, 0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2,
                  0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                  0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3 },
                { 0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                  0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                  0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2 },
                { 0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                  0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                  0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1 },
            } },
            std::array<std::array<uint8_t, 64>, 3> { {
                { 0x00, 0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2,
                  0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                  0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                  0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                  0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                  0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3 },
                { 0x01, 0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1,
                  0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                  0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                  0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                  0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00, 0x03,
                  0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2 },
                { 0x02, 0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00,
                  0x03, 0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01,
                  0x04, 0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1, 0x02,
                  0x05, 0x08, 0x11, 0x14, 0x17, 0x20, 0x23, 0xf6, 0xf3, 0x00, 0x03,
                  0x06, 0x09, 0x12, 0x15, 0x18, 0x21, 0xf8, 0xf5, 0xf2, 0x01, 0x04,
                  0x07, 0x10, 0x13, 0x16, 0x19, 0x22, 0xf7, 0xf4, 0xf1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD3H instructions
        {
            "ld3h scalar+immediate",
            TEST_FUNC("ld3h { z15.h, z16.h, z17.h }, p6/z, [%[base], #15, mul vl]"),
            { /*zt=*/ { 15, 16, 17 }, /*pg=*/6 },
            std::array<std::array<uint16_t, 8>, 3> { {
                { 0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007, 0x0010, 0x0013 },
                { 0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005, 0x0008, 0x0011, 0x0014 },
                { 0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006, 0x0009, 0x0012, 0x0015 },
            } },
            std::array<std::array<uint16_t, 16>, 3> { {
                { 0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005, 0x0008,
                  0x0011, 0x0014, 0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3 },
                { 0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006, 0x0009,
                  0x0012, 0x0015, 0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2 },
                { 0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007, 0x0010,
                  0x0013, 0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1 },
            } },
            std::array<std::array<uint16_t, 32>, 3> { {
                { 0x0000, 0x0003, 0x0006, 0x0009, 0x0012, 0x0015, 0x0018, 0x0021,
                  0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007, 0x0010, 0x0013,
                  0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005,
                  0x0008, 0x0011, 0x0014, 0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3 },
                { 0x0001, 0x0004, 0x0007, 0x0010, 0x0013, 0x0016, 0x0019, 0x0022,
                  0xfff7, 0xfff4, 0xfff1, 0x0002, 0x0005, 0x0008, 0x0011, 0x0014,
                  0x0017, 0x0020, 0x0023, 0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006,
                  0x0009, 0x0012, 0x0015, 0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2 },
                { 0x0002, 0x0005, 0x0008, 0x0011, 0x0014, 0x0017, 0x0020, 0x0023,
                  0xfff6, 0xfff3, 0x0000, 0x0003, 0x0006, 0x0009, 0x0012, 0x0015,
                  0x0018, 0x0021, 0xfff8, 0xfff5, 0xfff2, 0x0001, 0x0004, 0x0007,
                  0x0010, 0x0013, 0x0016, 0x0019, 0x0022, 0xfff7, 0xfff4, 0xfff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        // LD3W instructions
        {
            "ld3w scalar+immediate",
            TEST_FUNC("ld3w { z18.s, z19.s, z20.s }, p3/z, [%[base], #18, mul vl]"),
            { /*zt=*/ { 18, 19, 20 }, /*pg=*/3 },
            std::array<std::array<uint32_t, 4>, 3> { {
                { 0x00000008, 0x00000011, 0x00000014, 0x00000017 },
                { 0x00000009, 0x00000012, 0x00000015, 0x00000018 },
                { 0x00000010, 0x00000013, 0x00000016, 0x00000019 },
            } },
            std::array<std::array<uint32_t, 8>, 3> { {
                { 0x00000016, 0x00000019, 0x00000022, 0xfffffff7, 0xfffffff4, 0xfffffff1,
                  0x00000002, 0x00000005 },
                { 0x00000017, 0x00000020, 0x00000023, 0xfffffff6, 0xfffffff3, 0x00000000,
                  0x00000003, 0x00000006 },
                { 0x00000018, 0x00000021, 0xfffffff8, 0xfffffff5, 0xfffffff2, 0x00000001,
                  0x00000004, 0x00000007 },
            } },
            std::array<std::array<uint32_t, 16>, 3> { {
                { 0x00000000, 0x00000003, 0x00000006, 0x00000009, 0x00000012, 0x00000015,
                  0x00000018, 0x00000021, 0xfffffff8, 0xfffffff5, 0xfffffff2, 0x00000001,
                  0x00000004, 0x00000007, 0x00000010, 0x00000013 },
                { 0x00000001, 0x00000004, 0x00000007, 0x00000010, 0x00000013, 0x00000016,
                  0x00000019, 0x00000022, 0xfffffff7, 0xfffffff4, 0xfffffff1, 0x00000002,
                  0x00000005, 0x00000008, 0x00000011, 0x00000014 },
                { 0x00000002, 0x00000005, 0x00000008, 0x00000011, 0x00000014, 0x00000017,
                  0x00000020, 0x00000023, 0xfffffff6, 0xfffffff3, 0x00000000, 0x00000003,
                  0x00000006, 0x00000009, 0x00000012, 0x00000015 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        // LD3D instructions
        {
            "ld3d scalar+immediate",
            TEST_FUNC("ld3d { z21.d, z22.d, z23.d }, p0/z, [%[base], #-18, mul vl]"),
            { /*zt=*/ { 21, 22, 23 }, /*pg=*/0 },
            std::array<std::array<uint64_t, 2>, 3> { {
                { 0xfffffffffffffff4, 0xfffffffffffffff1 },
                { 0xfffffffffffffff3, 0x0000000000000000 },
                { 0xfffffffffffffff2, 0x0000000000000001 },
            } },
            std::array<std::array<uint64_t, 4>, 3> { {
                { 0xfffffffffffffff8, 0xfffffffffffffff5, 0xfffffffffffffff2,
                  0x0000000000000001 },
                { 0xfffffffffffffff7, 0xfffffffffffffff4, 0xfffffffffffffff1,
                  0x0000000000000002 },
                { 0xfffffffffffffff6, 0xfffffffffffffff3, 0x0000000000000000,
                  0x0000000000000003 },
            } },
            std::array<std::array<uint64_t, 8>, 3> { {
                { 0x0000000000000016, 0x0000000000000019, 0x0000000000000022,
                  0xfffffffffffffff7, 0xfffffffffffffff4, 0xfffffffffffffff1,
                  0x0000000000000002, 0x0000000000000005 },
                { 0x0000000000000017, 0x0000000000000020, 0x0000000000000023,
                  0xfffffffffffffff6, 0xfffffffffffffff3, 0x0000000000000000,
                  0x0000000000000003, 0x0000000000000006 },
                { 0x0000000000000018, 0x0000000000000021, 0xfffffffffffffff8,
                  0xfffffffffffffff5, 0xfffffffffffffff2, 0x0000000000000001,
                  0x0000000000000004, 0x0000000000000007 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld3d scalar+immediate (min index)",
            TEST_FUNC("ld3d { z22.d, z23.d, z24.d }, p1/z, [%[base], #-24, mul vl]"),
            { /*zt=*/ { 22, 23, 24 }, /*pg=*/1 },
            std::array<std::array<uint64_t, 2>, 3> { {
                { 0x0000000000000016, 0x0000000000000019 },
                { 0x0000000000000017, 0x0000000000000020 },
                { 0x0000000000000018, 0x0000000000000021 },
            } },
            std::array<std::array<uint64_t, 4>, 3> { {
                { 0x0000000000000000, 0x0000000000000003, 0x0000000000000006,
                  0x0000000000000009 },
                { 0x0000000000000001, 0x0000000000000004, 0x0000000000000007,
                  0x0000000000000010 },
                { 0x0000000000000002, 0x0000000000000005, 0x0000000000000008,
                  0x0000000000000011 },
            } },
            std::array<std::array<uint64_t, 8>, 3> { {
                { 0x0000000000000000, 0x0000000000000003, 0x0000000000000006,
                  0x0000000000000009, 0x0000000000000012, 0x0000000000000015,
                  0x0000000000000018, 0x0000000000000021 },
                { 0x0000000000000001, 0x0000000000000004, 0x0000000000000007,
                  0x0000000000000010, 0x0000000000000013, 0x0000000000000016,
                  0x0000000000000019, 0x0000000000000022 },
                { 0x0000000000000002, 0x0000000000000005, 0x0000000000000008,
                  0x0000000000000011, 0x0000000000000014, 0x0000000000000017,
                  0x0000000000000020, 0x0000000000000023 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld3d scalar+immediate (max index)",
            TEST_FUNC("ld3d { z23.d, z24.d, z25.d }, p2/z, [%[base], #21, mul vl]"),
            { /*zt=*/ { 23, 24, 25 }, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 3> { {
                { 0x0000000000000010, 0x0000000000000013 },
                { 0x0000000000000011, 0x0000000000000014 },
                { 0x0000000000000012, 0x0000000000000015 },
            } },
            std::array<std::array<uint64_t, 4>, 3> { {
                { 0x0000000000000020, 0x0000000000000023, 0xfffffffffffffff6,
                  0xfffffffffffffff3 },
                { 0x0000000000000021, 0xfffffffffffffff8, 0xfffffffffffffff5,
                  0xfffffffffffffff2 },
                { 0x0000000000000022, 0xfffffffffffffff7, 0xfffffffffffffff4,
                  0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 8>, 3> { {
                { 0x0000000000000008, 0x0000000000000011, 0x0000000000000014,
                  0x0000000000000017, 0x0000000000000020, 0x0000000000000023,
                  0xfffffffffffffff6, 0xfffffffffffffff3 },
                { 0x0000000000000009, 0x0000000000000012, 0x0000000000000015,
                  0x0000000000000018, 0x0000000000000021, 0xfffffffffffffff8,
                  0xfffffffffffffff5, 0xfffffffffffffff2 },
                { 0x0000000000000010, 0x0000000000000013, 0x0000000000000016,
                  0x0000000000000019, 0x0000000000000022, 0xfffffffffffffff7,
                  0xfffffffffffffff4, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_ld4_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_load_test_case_t<4>::test_ptrs_t &ptrs) {          \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_load_test_case_t<4>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3, zt4}, pg},
         *     Expected output data (128-bit vl),
         *     Expected output data (256-bit vl),
         *     Expected output data (512-bit vl),
         *     Base pointer (value for Xn),
         * },
         */
        // LD4B instructions
        {
            "ld4b scalar+immediate",
            TEST_FUNC("ld4b { z24.b, z25.b, z26.b, z27.b }, p3/z, [%[base], #-20, "
                      "mul vl]"),
            { /*zt=*/ { 24, 25, 26, 27 }, /*pg=*/3 },
            std::array<std::array<uint8_t, 16>, 4> { {
                { 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12,
                  0x16, 0x20, 0xf8, 0xf4 },
                { 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13,
                  0x17, 0x21, 0xf7, 0xf3 },
                { 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14,
                  0x18, 0x22, 0xf6, 0xf2 },
                { 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15,
                  0x19, 0x23, 0xf5, 0xf1 },
            } },
            std::array<std::array<uint8_t, 32>, 4> { {
                { 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08,
                  0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20,
                  0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4 },
                { 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09,
                  0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21,
                  0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3 },
                { 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10,
                  0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22,
                  0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2 },
                { 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11,
                  0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23,
                  0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1 },
            } },
            std::array<std::array<uint8_t, 64>, 4> { {
                { 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08,
                  0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20,
                  0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00,
                  0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12,
                  0x16, 0x20, 0xf8, 0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8,
                  0xf4, 0x00, 0x04, 0x08, 0x12, 0x16, 0x20, 0xf8, 0xf4 },
                { 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09,
                  0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21,
                  0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01,
                  0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13,
                  0x17, 0x21, 0xf7, 0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7,
                  0xf3, 0x01, 0x05, 0x09, 0x13, 0x17, 0x21, 0xf7, 0xf3 },
                { 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10,
                  0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22,
                  0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02,
                  0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14,
                  0x18, 0x22, 0xf6, 0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6,
                  0xf2, 0x02, 0x06, 0x10, 0x14, 0x18, 0x22, 0xf6, 0xf2 },
                { 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11,
                  0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23,
                  0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03,
                  0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15,
                  0x19, 0x23, 0xf5, 0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5,
                  0xf1, 0x03, 0x07, 0x11, 0x15, 0x19, 0x23, 0xf5, 0xf1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::BYTE),
        },
        // LD4H instructions
        {
            "ld4h scalar+immediate",
            TEST_FUNC("ld4h { z27.h, z28.h, z29.h, z30.h }, p6/z, [%[base], #-16, "
                      "mul vl]"),
            { /*zt=*/ { 27, 28, 29, 30 }, /*pg=*/6 },
            std::array<std::array<uint16_t, 8>, 4> { {
                { 0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4 },
                { 0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3 },
                { 0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2 },
                { 0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1 },
            } },
            std::array<std::array<uint16_t, 16>, 4> { {
                { 0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4, 0x0000,
                  0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4 },
                { 0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3, 0x0001,
                  0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3 },
                { 0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2, 0x0002,
                  0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2 },
                { 0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1, 0x0003,
                  0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1 },
            } },
            std::array<std::array<uint16_t, 32>, 4> { {
                { 0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4,
                  0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4,
                  0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4,
                  0x0000, 0x0004, 0x0008, 0x0012, 0x0016, 0x0020, 0xfff8, 0xfff4 },
                { 0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3,
                  0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3,
                  0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3,
                  0x0001, 0x0005, 0x0009, 0x0013, 0x0017, 0x0021, 0xfff7, 0xfff3 },
                { 0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2,
                  0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2,
                  0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2,
                  0x0002, 0x0006, 0x0010, 0x0014, 0x0018, 0x0022, 0xfff6, 0xfff2 },
                { 0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1,
                  0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1,
                  0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1,
                  0x0003, 0x0007, 0x0011, 0x0015, 0x0019, 0x0023, 0xfff5, 0xfff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::HALF),
        },
        // LD4W instructions
        {
            "ld4w scalar+immediate",
            TEST_FUNC("ld4w { z30.s, z31.s, z0.s, z1.s }, p5/z, [%[base], #-12, mul vl]"),
            { /*zt=*/ { 30, 31, 0, 1 }, /*pg=*/5 },
            std::array<std::array<uint32_t, 4>, 4> { {
                { 0x00000016, 0x00000020, 0xfffffff8, 0xfffffff4 },
                { 0x00000017, 0x00000021, 0xfffffff7, 0xfffffff3 },
                { 0x00000018, 0x00000022, 0xfffffff6, 0xfffffff2 },
                { 0x00000019, 0x00000023, 0xfffffff5, 0xfffffff1 },
            } },
            std::array<std::array<uint32_t, 8>, 4> { {
                { 0x00000000, 0x00000004, 0x00000008, 0x00000012, 0x00000016, 0x00000020,
                  0xfffffff8, 0xfffffff4 },
                { 0x00000001, 0x00000005, 0x00000009, 0x00000013, 0x00000017, 0x00000021,
                  0xfffffff7, 0xfffffff3 },
                { 0x00000002, 0x00000006, 0x00000010, 0x00000014, 0x00000018, 0x00000022,
                  0xfffffff6, 0xfffffff2 },
                { 0x00000003, 0x00000007, 0x00000011, 0x00000015, 0x00000019, 0x00000023,
                  0xfffffff5, 0xfffffff1 },
            } },
            std::array<std::array<uint32_t, 16>, 4> { {
                { 0x00000000, 0x00000004, 0x00000008, 0x00000012, 0x00000016, 0x00000020,
                  0xfffffff8, 0xfffffff4, 0x00000000, 0x00000004, 0x00000008, 0x00000012,
                  0x00000016, 0x00000020, 0xfffffff8, 0xfffffff4 },
                { 0x00000001, 0x00000005, 0x00000009, 0x00000013, 0x00000017, 0x00000021,
                  0xfffffff7, 0xfffffff3, 0x00000001, 0x00000005, 0x00000009, 0x00000013,
                  0x00000017, 0x00000021, 0xfffffff7, 0xfffffff3 },
                { 0x00000002, 0x00000006, 0x00000010, 0x00000014, 0x00000018, 0x00000022,
                  0xfffffff6, 0xfffffff2, 0x00000002, 0x00000006, 0x00000010, 0x00000014,
                  0x00000018, 0x00000022, 0xfffffff6, 0xfffffff2 },
                { 0x00000003, 0x00000007, 0x00000011, 0x00000015, 0x00000019, 0x00000023,
                  0xfffffff5, 0xfffffff1, 0x00000003, 0x00000007, 0x00000011, 0x00000015,
                  0x00000019, 0x00000023, 0xfffffff5, 0xfffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::SINGLE),
        },
        // LD4D instructions
        {
            "ld4d scalar+immediate",
            TEST_FUNC("ld4d { z1.d, z2.d, z3.d, z4.d }, p2/z, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 1, 2, 3, 4 }, /*pg=*/2 },
            std::array<std::array<uint64_t, 2>, 4> { {
                { 0x0000000000000016, 0x0000000000000020 },
                { 0x0000000000000017, 0x0000000000000021 },
                { 0x0000000000000018, 0x0000000000000022 },
                { 0x0000000000000019, 0x0000000000000023 },
            } },
            std::array<std::array<uint64_t, 4>, 4> { {
                { 0x0000000000000000, 0x0000000000000004, 0x0000000000000008,
                  0x0000000000000012 },
                { 0x0000000000000001, 0x0000000000000005, 0x0000000000000009,
                  0x0000000000000013 },
                { 0x0000000000000002, 0x0000000000000006, 0x0000000000000010,
                  0x0000000000000014 },
                { 0x0000000000000003, 0x0000000000000007, 0x0000000000000011,
                  0x0000000000000015 },
            } },
            std::array<std::array<uint64_t, 8>, 4> { {
                { 0x0000000000000000, 0x0000000000000004, 0x0000000000000008,
                  0x0000000000000012, 0x0000000000000016, 0x0000000000000020,
                  0xfffffffffffffff8, 0xfffffffffffffff4 },
                { 0x0000000000000001, 0x0000000000000005, 0x0000000000000009,
                  0x0000000000000013, 0x0000000000000017, 0x0000000000000021,
                  0xfffffffffffffff7, 0xfffffffffffffff3 },
                { 0x0000000000000002, 0x0000000000000006, 0x0000000000000010,
                  0x0000000000000014, 0x0000000000000018, 0x0000000000000022,
                  0xfffffffffffffff6, 0xfffffffffffffff2 },
                { 0x0000000000000003, 0x0000000000000007, 0x0000000000000011,
                  0x0000000000000015, 0x0000000000000019, 0x0000000000000023,
                  0xfffffffffffffff5, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld4d scalar+immediate (min index)",
            TEST_FUNC("ld4d { z2.d, z3.d, z4.d, z5.d }, p1/z, [%[base], #-32, mul vl]"),
            { /*zt=*/ { 2, 3, 4, 5 }, /*pg=*/1 },
            std::array<std::array<uint64_t, 2>, 4> { {
                { 0x0000000000000000, 0x0000000000000004 },
                { 0x0000000000000001, 0x0000000000000005 },
                { 0x0000000000000002, 0x0000000000000006 },
                { 0x0000000000000003, 0x0000000000000007 },
            } },
            std::array<std::array<uint64_t, 4>, 4> { {
                { 0x0000000000000000, 0x0000000000000004, 0x0000000000000008,
                  0x0000000000000012 },
                { 0x0000000000000001, 0x0000000000000005, 0x0000000000000009,
                  0x0000000000000013 },
                { 0x0000000000000002, 0x0000000000000006, 0x0000000000000010,
                  0x0000000000000014 },
                { 0x0000000000000003, 0x0000000000000007, 0x0000000000000011,
                  0x0000000000000015 },
            } },
            std::array<std::array<uint64_t, 8>, 4> { {
                { 0x0000000000000000, 0x0000000000000004, 0x0000000000000008,
                  0x0000000000000012, 0x0000000000000016, 0x0000000000000020,
                  0xfffffffffffffff8, 0xfffffffffffffff4 },
                { 0x0000000000000001, 0x0000000000000005, 0x0000000000000009,
                  0x0000000000000013, 0x0000000000000017, 0x0000000000000021,
                  0xfffffffffffffff7, 0xfffffffffffffff3 },
                { 0x0000000000000002, 0x0000000000000006, 0x0000000000000010,
                  0x0000000000000014, 0x0000000000000018, 0x0000000000000022,
                  0xfffffffffffffff6, 0xfffffffffffffff2 },
                { 0x0000000000000003, 0x0000000000000007, 0x0000000000000011,
                  0x0000000000000015, 0x0000000000000019, 0x0000000000000023,
                  0xfffffffffffffff5, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
        {
            "ld4d scalar+immediate (max index)",
            TEST_FUNC("ld4d { z3.d, z4.d, z5.d, z6.d }, p0/z, [%[base], #28, mul vl]"),
            { /*zt=*/ { 3, 4, 5, 6 }, /*pg=*/0 },
            std::array<std::array<uint64_t, 2>, 4> { {
                { 0xfffffffffffffff8, 0xfffffffffffffff4 },
                { 0xfffffffffffffff7, 0xfffffffffffffff3 },
                { 0xfffffffffffffff6, 0xfffffffffffffff2 },
                { 0xfffffffffffffff5, 0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 4>, 4> { {
                { 0x0000000000000016, 0x0000000000000020, 0xfffffffffffffff8,
                  0xfffffffffffffff4 },
                { 0x0000000000000017, 0x0000000000000021, 0xfffffffffffffff7,
                  0xfffffffffffffff3 },
                { 0x0000000000000018, 0x0000000000000022, 0xfffffffffffffff6,
                  0xfffffffffffffff2 },
                { 0x0000000000000019, 0x0000000000000023, 0xfffffffffffffff5,
                  0xfffffffffffffff1 },
            } },
            std::array<std::array<uint64_t, 8>, 4> { {
                { 0x0000000000000000, 0x0000000000000004, 0x0000000000000008,
                  0x0000000000000012, 0x0000000000000016, 0x0000000000000020,
                  0xfffffffffffffff8, 0xfffffffffffffff4 },
                { 0x0000000000000001, 0x0000000000000005, 0x0000000000000009,
                  0x0000000000000013, 0x0000000000000017, 0x0000000000000021,
                  0xfffffffffffffff7, 0xfffffffffffffff3 },
                { 0x0000000000000002, 0x0000000000000006, 0x0000000000000010,
                  0x0000000000000014, 0x0000000000000018, 0x0000000000000022,
                  0xfffffffffffffff6, 0xfffffffffffffff2 },
                { 0x0000000000000003, 0x0000000000000007, 0x0000000000000011,
                  0x0000000000000015, 0x0000000000000019, 0x0000000000000023,
                  0xfffffffffffffff5, 0xfffffffffffffff1 },
            } },
            INPUT_DATA.base_addr_for_data_size(element_size_t::DOUBLE),
        },
    });
#    undef TEST_FUNC
}

template <size_t NUM_ZT>
struct scalar_plus_immediate_store_test_case_t
    : public test_case_base_t<test_ptrs_with_base_ptr_t> {
    std::vector<uint8_t> reference_data_;
    struct registers_used_t {
        std::array<unsigned, NUM_ZT> src_z;
        unsigned governing_p;
    } registers_used_;

    void *base_;
    int64_t index_;

    element_size_t stored_value_size_;

    template <typename VALUE_T, size_t NUM_VALUES>
    scalar_plus_immediate_store_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<VALUE_T, NUM_VALUES> reference_data, int64_t index)
        : test_case_base_t<test_ptrs_t>(
              std::move(name), std::move(func), registers_used.governing_p,
              static_cast<element_size_t>(TEST_VL_BYTES / (NUM_VALUES / NUM_ZT)))
        , registers_used_(registers_used)
        , base_(OUTPUT_DATA.base_addr())
        , index_(index)
        , stored_value_size_(static_cast<element_size_t>(sizeof(VALUE_T)))
    {
        const auto num_copies = get_vl_bytes() / TEST_VL_BYTES;
        const auto copy_length = sizeof(VALUE_T) * reference_data.size();
        reference_data_.resize(copy_length * num_copies);
        for (size_t i = 0; i < num_copies; i++) {
            std::memcpy(&reference_data_[i * copy_length], reference_data.data(),
                        copy_length);
        }
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        static constexpr std::array<vector_reg_value128_t, 4> DATA_TO_WRITE { {
            { // Zt1 data
              0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
              0x12, 0x13, 0x14, 0x15 },
            { // Zt2 data
              0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
              0x28, 0x29, 0x30, 0x31 },
            { // Zt3 data
              0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43,
              0x44, 0x45, 0x46, 0x47 },
            { // Zt4 data
              0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
              0x60, 0x61, 0x62, 0x63 },
        } };
        assert(NUM_ZT <= DATA_TO_WRITE.size());

        for (size_t zt = 0; zt < NUM_ZT; zt++) {
            register_values.set_z_register_value(registers_used_.src_z[zt],
                                                 DATA_TO_WRITE[zt]);
        }

        if (test_status_ == test_result_t::PASS) {
            const auto vl_bytes = get_vl_bytes();
            const auto stored_value_bytes = static_cast<size_t>(stored_value_size_);
            const auto element_size_bytes = static_cast<size_t>(element_size_);
            const auto num_vector_elements = vl_bytes / element_size_bytes;
            memset(static_cast<uint8_t *>(base_) +
                       (index_ * num_vector_elements * stored_value_bytes),
                   0xAB, reference_data_.size());
        } else {
            OUTPUT_DATA.reset();
        }
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        // Check that the values of the Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }

        const auto vl_bytes = get_vl_bytes();

        std::vector<uint8_t> expected_output_data(reference_data_);

        const auto stored_value_bytes = static_cast<size_t>(stored_value_size_);
        const auto element_size_bytes = static_cast<size_t>(element_size_);

        const auto num_vector_elements = vl_bytes / element_size_bytes;
        const auto num_mask_elements = TEST_VL_BYTES / element_size_bytes;
        for (size_t i = 0; i < num_vector_elements; i++) {
            if (!element_is_active(i % num_mask_elements, pred, element_size_)) {
                // Element is inactive, set it to the poison value.
                memset(&expected_output_data[NUM_ZT * stored_value_bytes * i], 0xAB,
                       NUM_ZT * stored_value_bytes);
            }
        }

        const scalable_reg_value_t expected_output {
            expected_output_data.data(),
            expected_output_data.size(),
        };

        const scalable_reg_value_t output_value {
            static_cast<uint8_t *>(base_) +
                (index_ * num_vector_elements * stored_value_bytes),
            expected_output_data.size(),
        };

        if (output_value != expected_output) {
            test_failed();
            print("predicate: ");
            print_predicate(
                register_data.before.get_p_register_value(registers_used_.governing_p));
            print("\nexpected:  ");
            print_vector(expected_output);
            print("\nactual:    ");
            print_vector(output_value);
            print("\n");
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            base_,
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
        };
    }
};

test_result_t
test_st1_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_store_test_case_t<1>::test_ptrs_t &ptrs) {         \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_store_test_case_t<1>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        // ST1B instructions.
        {
            "st1b scalar+immediate 8bit element",
            TEST_FUNC("st1b z4.b, p7, [%[base], #0, mul vl]"),
            { /*zt=*/ { 4 }, /*pg=*/7 },
            std::array<uint8_t, 16> { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                      0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 },
            /*index=*/0,
        },
        {
            "st1b scalar+immediate 16bit element",
            TEST_FUNC("st1b z3.h, p4, [%[base], #1, mul vl]"),
            { /*zt=*/ { 3 }, /*pg=*/4 },
            std::array<uint8_t, 8> { 0x00, 0x02, 0x04, 0x06, 0x08, 0x10, 0x12, 0x14 },
            /*index=*/1,
        },
        {
            "st1b scalar+immediate 32bit element",
            TEST_FUNC("st1b z6.s, p1, [%[base], #2, mul vl]"),
            { /*zt=*/ { 6 }, /*pg=*/1 },
            std::array<uint8_t, 4> { 0x00, 0x04, 0x08, 0x12 },
            /*index=*/2,
        },
        {
            "st1b scalar+immediate 64bit element",
            TEST_FUNC("st1b z9.d, p2, [%[base], #3, mul vl]"),
            { /*zt=*/ { 9 }, /*pg=*/2 },
            std::array<uint8_t, 2> { 0x00, 0x08 },
            /*index=*/3,
        },
        {
            "st1b scalar+immediate 64bit element (min index)",
            TEST_FUNC("st1b z10.d, p3, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 10 }, /*pg=*/3 },
            std::array<uint8_t, 2> { 0x00, 0x08 },
            /*index=*/-8,
        },
        {
            "st1b scalar+immediate 64bit element (max index)",
            TEST_FUNC("st1b z11.d, p4, [%[base], #7, mul vl]"),
            { /*zt=*/ { 11 }, /*pg=*/4 },
            std::array<uint8_t, 2> { 0x00, 0x08 },
            /*index=*/7,
        },
        // ST1H instructions
        {
            "st1h scalar+immediate 16bit element",
            TEST_FUNC("st1h z12.h, p5, [%[base], #4, mul vl]"),
            { /*zt=*/ { 12 }, /*pg=*/5 },
            std::array<uint16_t, 8> { 0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x1110,
                                      0x1312, 0x1514 },
            /*index=*/4,
        },
        {
            "st1h scalar+immediate 32bit element",
            TEST_FUNC("st1h z15.s, p6, [%[base], #5, mul vl]"),
            { /*zt=*/ { 15 }, /*pg=*/6 },
            std::array<uint16_t, 4> { 0x0100, 0x0504, 0x0908, 0x1312 },
            /*index=*/5,
        },
        {
            "st1h scalar+immediate 64bit element",
            TEST_FUNC("st1h z18.d, p3, [%[base], #6, mul vl]"),
            { /*zt=*/ { 18 }, /*pg=*/3 },
            std::array<uint16_t, 2> { 0x0100, 0x0908 },
            /*index=*/6,
        },
        {
            "st1h scalar+immediate 64bit element (min index)",
            TEST_FUNC("st1h z19.d, p2, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 19 }, /*pg=*/2 },
            std::array<uint16_t, 2> { 0x0100, 0x0908 },
            /*index=*/-8,
        },
        {
            "st1h scalar+immediate 64bit element (max index)",
            TEST_FUNC("st1h z20.d, p1, [%[base], #7, mul vl]"),
            { /*zt=*/ { 20 }, /*pg=*/1 },
            std::array<uint16_t, 2> { 0x0100, 0x0908 },
            /*index=*/7,
        },
        // ST1W instructions
        {
            "st1w scalar+immediate 32bit element",
            TEST_FUNC("st1w z21.s, p0, [%[base], #-6, mul vl]"),
            { /*zt=*/ { 21 }, /*pg=*/0 },
            std::array<uint32_t, 4> { 0x03020100, 0x07060504, 0x11100908, 0x15141312 },
            /*index=*/-6,
        },
        {
            "st1w scalar+immediate 64bit element",
            TEST_FUNC("st1w z24.d, p3, [%[base], #-5, mul vl]"),
            { /*zt=*/ { 24 }, /*pg=*/3 },
            std::array<uint32_t, 2> { 0x03020100, 0x11100908 },
            /*index=*/-5,
        },
        {
            "st1w scalar+immediate 64bit element (min index)",
            TEST_FUNC("st1w z25.d, p4, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 25 }, /*pg=*/4 },
            std::array<uint32_t, 2> { 0x03020100, 0x11100908 },
            /*index=*/-8,
        },
        {
            "st1w scalar+immediate 64bit element (max index)",
            TEST_FUNC("st1w z26.d, p5, [%[base], #7, mul vl]"),
            { /*zt=*/ { 26 }, /*pg=*/5 },
            std::array<uint32_t, 2> { 0x03020100, 0x11100908 },
            /*index=*/7,
        },
        // ST1D instructions
        {
            "st1d scalar+immediate 64bit element",
            TEST_FUNC("st1d z27.d, p6, [%[base], #-4, mul vl]"),
            { /*zt=*/ { 27 }, /*pg=*/6 },
            std::array<uint64_t, 2> { 0x0706050403020100, 0x1514131211100908 },
            /*index=*/-4,
        },
        {
            "st1d scalar+immediate 64bit element (min index)",
            TEST_FUNC("st1d z28.d, p7, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 28 }, /*pg=*/7 },
            std::array<uint64_t, 2> { 0x0706050403020100, 0x1514131211100908 },
            /*index=*/-8,
        },
        {
            "st1d scalar+immediate 64bit element (max index)",
            TEST_FUNC("st1d z29.d, p6, [%[base], #7, mul vl]"),
            { /*zt=*/ { 29 }, /*pg=*/6 },
            std::array<uint64_t, 2> { 0x0706050403020100, 0x1514131211100908 },
            /*index=*/7,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st2_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_store_test_case_t<2>::test_ptrs_t &ptrs) {         \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_store_test_case_t<2>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        // ST2B instructions
        {
            "st2b scalar+immediate",
            TEST_FUNC("st2b { z0.b, z1.b }, p7, [%[base], #0, mul vl]"),
            { /*zt=*/ { 0, 1 }, /*pg=*/7 },
            std::array<uint8_t, 32> { 0x00, 0x16, 0x01, 0x17, 0x02, 0x18, 0x03, 0x19,
                                      0x04, 0x20, 0x05, 0x21, 0x06, 0x22, 0x07, 0x23,
                                      0x08, 0x24, 0x09, 0x25, 0x10, 0x26, 0x11, 0x27,
                                      0x12, 0x28, 0x13, 0x29, 0x14, 0x30, 0x15, 0x31 },
            /*index=*/0,
        },
        // ST2H instructions
        {
            "st2h scalar+immediate",
            TEST_FUNC("st2h { z3.h, z4.h }, p4, [%[base], #2, mul vl]"),
            { /*zt=*/ { 3, 4 }, /*pg=*/4 },
            std::array<uint16_t, 16> { 0x0100, 0x1716, 0x0302, 0x1918, 0x0504, 0x2120,
                                       0x0706, 0x2322, 0x0908, 0x2524, 0x1110, 0x2726,
                                       0x1312, 0x2928, 0x1514, 0x3130 },
            /*index=*/2,
        },
        // ST2W instructions
        {
            "st2w scalar+immediate",
            TEST_FUNC("st2w { z6.s, z7.s }, p1, [%[base], #4, mul vl]"),
            { /*zt=*/ { 6, 7 }, /*pg=*/1 },
            std::array<uint32_t, 8> { 0x03020100, 0x19181716, 0x07060504, 0x23222120,
                                      0x11100908, 0x27262524, 0x15141312, 0x31302928 },
            /*index=*/4,
        },
        // ST2D instructions
        {
            "st2d scalar+immediate",
            TEST_FUNC("st2d { z9.d, z10.d }, p2, [%[base], #6, mul vl]"),
            { /*zt=*/ { 9, 10 }, /*pg=*/2 },
            std::array<uint64_t, 4> { 0x0706050403020100, 0x2322212019181716,
                                      0x1514131211100908, 0x3130292827262524 },
            /*index=*/6,
        },
        {
            "st2d scalar+immediate (min index)",
            TEST_FUNC("st2d { z10.d, z11.d }, p3, [%[base], #-16, mul vl]"),
            { /*zt=*/ { 10, 11 }, /*pg=*/3 },
            std::array<uint64_t, 4> { 0x0706050403020100, 0x2322212019181716,
                                      0x1514131211100908, 0x3130292827262524 },
            /*index=*/-16,
        },
        {
            "st2d scalar+immediate (max index)",
            TEST_FUNC("st2d { z11.d, z12.d }, p4, [%[base], #14, mul vl]"),
            { /*zt=*/ { 11, 12 }, /*pg=*/4 },
            std::array<uint64_t, 4> { 0x0706050403020100, 0x2322212019181716,
                                      0x1514131211100908, 0x3130292827262524 },
            /*index=*/14,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st3_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_store_test_case_t<3>::test_ptrs_t &ptrs) {         \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_store_test_case_t<3>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        // ST3B instructions
        {
            "st3b scalar+immediate",
            TEST_FUNC("st3b { z12.b, z13.b, z14.b }, p5, [%[base], #12, mul vl]"),
            { /*zt=*/ { 12, 13, 14 }, /*pg=*/5 },
            std::array<uint8_t, 48> {
                0x00, 0x16, 0x32, 0x01, 0x17, 0x33, 0x02, 0x18, 0x34, 0x03, 0x19, 0x35,
                0x04, 0x20, 0x36, 0x05, 0x21, 0x37, 0x06, 0x22, 0x38, 0x07, 0x23, 0x39,
                0x08, 0x24, 0x40, 0x09, 0x25, 0x41, 0x10, 0x26, 0x42, 0x11, 0x27, 0x43,
                0x12, 0x28, 0x44, 0x13, 0x29, 0x45, 0x14, 0x30, 0x46, 0x15, 0x31, 0x47 },
            /*index=*/12,
        },
        // ST3H instructions
        {
            "st3h scalar+immediate",
            TEST_FUNC("st3h { z15.h, z16.h, z17.h }, p6, [%[base], #15, mul vl]"),
            { /*zt=*/ { 15, 16, 17 }, /*pg=*/6 },
            std::array<uint16_t, 24> { 0x0100, 0x1716, 0x3332, 0x0302, 0x1918, 0x3534,
                                       0x0504, 0x2120, 0x3736, 0x0706, 0x2322, 0x3938,
                                       0x0908, 0x2524, 0x4140, 0x1110, 0x2726, 0x4342,
                                       0x1312, 0x2928, 0x4544, 0x1514, 0x3130, 0x4746 },
            /*index=*/15,
        },
        // ST3W instructions
        {
            "st3w scalar+immediate",
            TEST_FUNC("st3w { z18.s, z19.s, z20.s }, p3, [%[base], #18, mul vl]"),
            { /*zt=*/ { 18, 19, 20 }, /*pg=*/3 },
            std::array<uint32_t, 12> { 0x03020100, 0x19181716, 0x35343332, 0x07060504,
                                       0x23222120, 0x39383736, 0x11100908, 0x27262524,
                                       0x43424140, 0x15141312, 0x31302928, 0x47464544 },
            /*index=*/18,
        },
        // ST3D instructions
        {
            "st3d scalar+immediate",
            TEST_FUNC("st3d { z21.d, z22.d, z23.d }, p0, [%[base], #-18, mul vl]"),
            { /*zt=*/ { 21, 22, 23 }, /*pg=*/0 },
            std::array<uint64_t, 6> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x1514131211100908,
                                      0x3130292827262524, 0x4746454443424140 },
            /*index=*/-18,
        },
        {
            "st3d scalar+immediate (min index)",
            TEST_FUNC("st3d { z22.d, z23.d, z24.d }, p1, [%[base], #-24, mul vl]"),
            { /*zt=*/ { 22, 23, 24 }, /*pg=*/1 },
            std::array<uint64_t, 6> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x1514131211100908,
                                      0x3130292827262524, 0x4746454443424140 },
            /*index=*/-24,
        },
        {
            "st3d scalar+immediate (max index)",
            TEST_FUNC("st3d { z23.d, z24.d, z25.d }, p2, [%[base], #21, mul vl]"),
            { /*zt=*/ { 23, 24, 25 }, /*pg=*/2 },
            std::array<uint64_t, 6> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x1514131211100908,
                                      0x3130292827262524, 0x4746454443424140 },
            /*index=*/21,
        },
    });
#    undef TEST_FUNC
}

test_result_t
test_st4_scalar_plus_immediate()
{
#    define TEST_FUNC(ld_instruction)                                               \
        [](scalar_plus_immediate_store_test_case_t<4>::test_ptrs_t &ptrs) {         \
            asm(/* clang-format off */                                              \
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

    return run_tests<scalar_plus_immediate_store_test_case_t<4>>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {{zt1, zt2, zt3, zt4}, pg, zm},
         *     Expected output data,
         *     Index (value for Xm),
         * },
         */
        // ST4B instructions
        {
            "st4b scalar+immediate",
            TEST_FUNC("st4b { z24.b, z25.b, z26.b, z27.b }, p3, [%[base], #-20, mul vl]"),
            { /*zt=*/ { 24, 25, 26, 27 }, /*pg=*/3 },
            std::array<uint8_t, 64> {
                0x00, 0x16, 0x32, 0x48, 0x01, 0x17, 0x33, 0x49, 0x02, 0x18, 0x34,
                0x50, 0x03, 0x19, 0x35, 0x51, 0x04, 0x20, 0x36, 0x52, 0x05, 0x21,
                0x37, 0x53, 0x06, 0x22, 0x38, 0x54, 0x07, 0x23, 0x39, 0x55, 0x08,
                0x24, 0x40, 0x56, 0x09, 0x25, 0x41, 0x57, 0x10, 0x26, 0x42, 0x58,
                0x11, 0x27, 0x43, 0x59, 0x12, 0x28, 0x44, 0x60, 0x13, 0x29, 0x45,
                0x61, 0x14, 0x30, 0x46, 0x62, 0x15, 0x31, 0x47, 0x63 },
            /*index=*/-20,
        },
        // ST4H instructions
        {
            "st4h scalar+immediate",
            TEST_FUNC("st4h { z27.h, z28.h, z29.h, z30.h }, p6, [%[base], #-16, mul vl]"),
            { /*zt=*/ { 27, 28, 29, 30 }, /*pg=*/6 },
            std::array<uint16_t, 32> {
                0x0100, 0x1716, 0x3332, 0x4948, 0x0302, 0x1918, 0x3534, 0x5150,
                0x0504, 0x2120, 0x3736, 0x5352, 0x0706, 0x2322, 0x3938, 0x5554,
                0x0908, 0x2524, 0x4140, 0x5756, 0x1110, 0x2726, 0x4342, 0x5958,
                0x1312, 0x2928, 0x4544, 0x6160, 0x1514, 0x3130, 0x4746, 0x6362 },
            /*index=*/-16,
        },
        // ST4W instructions
        {
            "st4w scalar+immediate",
            TEST_FUNC("st4w { z30.s, z31.s, z0.s, z1.s }, p5, [%[base], #-12, mul vl]"),
            { /*zt=*/ { 30, 31, 0, 1 }, /*pg=*/5 },
            std::array<uint32_t, 16> { 0x03020100, 0x19181716, 0x35343332, 0x51504948,
                                       0x07060504, 0x23222120, 0x39383736, 0x55545352,
                                       0x11100908, 0x27262524, 0x43424140, 0x59585756,
                                       0x15141312, 0x31302928, 0x47464544, 0x63626160 },
            /*index=*/-12,
        },
        // ST4D instructions
        {
            "st4d scalar+immediate",
            TEST_FUNC("st4d { z1.d, z2.d, z3.d, z4.d }, p2, [%[base], #-8, mul vl]"),
            { /*zt=*/ { 1, 2, 3, 4 }, /*pg=*/2 },
            std::array<uint64_t, 8> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x5554535251504948,
                                      0x1514131211100908, 0x3130292827262524,
                                      0x4746454443424140, 0x6362616059585756 },
            /*index=*/-8,
        },
        {
            "st4d scalar+immediate (min index)",
            TEST_FUNC("st4d { z2.d, z3.d, z4.d, z5.d }, p1, [%[base], #-32, mul vl]"),
            { /*zt=*/ { 2, 3, 4, 5 }, /*pg=*/1 },
            std::array<uint64_t, 8> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x5554535251504948,
                                      0x1514131211100908, 0x3130292827262524,
                                      0x4746454443424140, 0x6362616059585756 },
            /*index=*/-32,
        },
        {
            "st4d scalar+immediate (max index)",
            TEST_FUNC("st4d { z3.d, z4.d, z5.d, z6.d }, p0, [%[base], #28, mul vl]"),
            { /*zt=*/ { 3, 4, 5, 6 }, /*pg=*/0 },
            std::array<uint64_t, 8> { 0x0706050403020100, 0x2322212019181716,
                                      0x3938373635343332, 0x5554535251504948,
                                      0x1514131211100908, 0x3130292827262524,
                                      0x4746454443424140, 0x6362616059585756 },
            /*index=*/28,
        },

    });
#    undef TEST_FUNC
}

#endif // defined(__ARM_FEATURE_SVE)

#if defined(__ARM_FEATURE_SVE2)

struct test_ptrs_with_index_t : public basic_test_ptrs_t {
    int64_t index; // Scalar index used for the test instruction.

    test_ptrs_with_index_t(const void *z_restore_base_, const void *p_restore_base_,
                           void *z_save_base_, void *p_save_base_, int64_t index_)
        : basic_test_ptrs_t { z_restore_base_, p_restore_base_, z_save_base_,
                              p_save_base_ }
        , index(index_)
    {
    }
};

struct vector_plus_scalar_load_test_case_t
    : public test_case_base_t<test_ptrs_with_index_t> {
    vector_reg_value128_t reference_data_;
    vector_reg_value128_t base_data_;

    struct registers_used_t {
        unsigned dest_z;
        unsigned governing_p;
        unsigned base_z;
    } registers_used_;

    int64_t index_; // The scalar index used for the test instruction.
                    // This gets copied to the test_ptrs_t object to pass to the test
                    // function.

    template <typename ELEMENT_T, typename BASE_T>
    vector_plus_scalar_load_test_case_t(
        std::string name, test_func_t func, registers_used_t registers_used,
        std::array<ELEMENT_T, TEST_VL_BYTES / sizeof(ELEMENT_T)> reference_data,
        std::array<BASE_T, TEST_VL_BYTES / sizeof(BASE_T)> base, int64_t index)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        static_cast<element_size_t>(sizeof(BASE_T)))
        , registers_used_(registers_used)
        , index_(index)

    {
        std::memcpy(reference_data_.data(), reference_data.data(),
                    reference_data_.size());
        std::memcpy(base_data_.data(), base.data(), base_data_.size());
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the base vector register.
        register_values.set_z_register_value(registers_used_.base_z, base_data_);
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        const auto vl_bytes = get_vl_bytes();

        std::vector<uint8_t> expected_output_data;
        expected_output_data.resize(vl_bytes);

        assert(reference_data_.size() == TEST_VL_BYTES);
        for (size_t i = 0; i < vl_bytes / TEST_VL_BYTES; i++) {
            memcpy(&expected_output_data[TEST_VL_BYTES * i], reference_data_.data(),
                   TEST_VL_BYTES);
        }
        apply_predicate_mask(expected_output_data, pred, element_size_);
        const scalable_reg_value_t expected_output {
            expected_output_data.data(),
            vl_bytes,
        };

        const auto output_value =
            register_data.after.get_z_register_value(registers_used_.dest_z);

        if (output_value != expected_output) {
            test_failed();
            print("predicate: ");
            print_predicate(
                register_data.before.get_p_register_value(registers_used_.governing_p));
            print("\nexpected:  ");
            print_vector(expected_output);
            print("\nactual:    ");
            print_vector(output_value);
            print("\n");
        }

        // Check that the values of the other Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            if (i == registers_used_.dest_z)
                continue;
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
            index_,
        };
    }
};

test_result_t
test_ld1_vector_plus_scalar()
{
#    define TEST_FUNC(ld_instruction)                                          \
        [](vector_plus_scalar_load_test_case_t::test_ptrs_t &ptrs) {           \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            ld_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */    \
                :                                                              \
                : [z_restore_base] "r"(ptrs.z_restore_base),                   \
                  [z_save_base] "r"(ptrs.z_save_base),                         \
                  [p_restore_base] "r"(ptrs.p_restore_base),                   \
                  [p_save_base] "r"(ptrs.p_save_base), [index] "r"(ptrs.index) \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                           \
        }

    const auto get_base_ptr = [&](element_size_t element_size, size_t offset) {
        void *start = INPUT_DATA.base_addr_for_data_size(element_size);
        switch (element_size) {
        case element_size_t::BYTE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint8_t *>(start)[offset]);
        case element_size_t::HALF:
            return reinterpret_cast<uintptr_t>(&static_cast<uint16_t *>(start)[offset]);
        case element_size_t::SINGLE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint32_t *>(start)[offset]);
        case element_size_t::DOUBLE:
            return reinterpret_cast<uintptr_t>(&static_cast<uint64_t *>(start)[offset]);
        }
        assert(false); // unreachable
        return uintptr_t(0);
    };
    return run_tests<vector_plus_scalar_load_test_case_t>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zn},
         *     Expected output data,
         *     Base data (value for zn),
         *     Index value,
         * },
         */
        /* TODO i#5036: Add tests for 32-bit element variants.
         *              For example: ldnt1b z0.s, p0/z, [z31.s, x2].
         *              These instructions require 32-bit base pointers and I'm not sure
         *              how we can reliably and portably guarantee that allocated memory
         *              has an address that fits into 32-bits.
         */
        {
            "ldnt1b vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1b z0.d, p0/z, [z31.d, %[index]]"),
            { /*zt=*/0, /*pg=*/0, /*zn=*/31 },
            std::array<uint64_t, 2> { 0x00, 0x16 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 0),
                get_base_ptr(element_size_t::BYTE, 16),
            },
            0,
        },
        {
            "ldnt1sb vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1sb z7.d, p1/z, [z24.d, %[index]]"),
            { /*zt=*/7, /*pg=*/1, /*zn=*/24 },
            std::array<int64_t, 2> { -15, 0x15 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::BYTE, 0),
                get_base_ptr(element_size_t::BYTE, 16),
            },
            -1,
        },
        {
            "ldnt1h vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1h z14.d, p2/z, [z17.d, %[index]]"),
            { /*zt=*/14, /*pg=*/2, /*zn=*/17 },
            std::array<uint64_t, 2> { 0x12, 0x14 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 8),
                get_base_ptr(element_size_t::HALF, 10),
            },
            8,
        },
        {
            "ldnt1sh vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1sh z21.d, p3/z, [z10.d, %[index]]"),
            { /*zt=*/21, /*pg=*/3, /*zn=*/10 },
            std::array<int64_t, 2> { -15, 0x17 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::HALF, 2),
                get_base_ptr(element_size_t::HALF, 20),
            },
            -6,
        },
        {
            "ldnt1w vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1w z28.d, p4/z, [z3.d, %[index]]"),
            { /*zt=*/28, /*pg=*/4, /*zn=*/3 },
            std::array<uint64_t, 2> { 0xfffffff4, 0xfffffff3 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 4),
                get_base_ptr(element_size_t::SINGLE, 5),
            },
            -32,
        },
        {
            "ldnt1sw vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1sw z29.d, p5/z, [z4.d, %[index]]"),
            { /*zt=*/29, /*pg=*/5, /*zn=*/4 },
            std::array<int64_t, 2> { -12, -13 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::SINGLE, 4),
                get_base_ptr(element_size_t::SINGLE, 5),
            },
            -32,
        },
        {
            "ldnt1d vector+scalar 64bit unscaled offset",
            TEST_FUNC("ldnt1d z22.d, p6/z, [z11.d, %[index]]"),
            { /*zt=*/22, /*pg=*/6, /*zn=*/11 },
            std::array<uint64_t, 2> { 0x03, 0x19 },
            std::array<uintptr_t, 2> {
                get_base_ptr(element_size_t::DOUBLE, 0),
                get_base_ptr(element_size_t::DOUBLE, 16),
            },
            24,
        },
    });
#    undef TEST_FUNC
}

struct vector_plus_scalar_store_test_case_t
    : public test_case_base_t<test_ptrs_with_index_t> {
    vector_reg_value128_t base_data_;
    std::array<const void *, 2> base_ptrs_;

    struct registers_used_t {
        unsigned src_z;
        unsigned governing_p;
        unsigned base_z;
    } registers_used_;

    element_size_t stored_value_size_;

    expected_values_t expected_values_;

    int64_t index_; // The scalar index used for the test instruction.
                    // This gets copied to the test_ptrs_t object to pass to the test
                    // function.

    vector_plus_scalar_store_test_case_t(std::string name, test_func_t func,
                                         registers_used_t registers_used,
                                         std::array<std::ptrdiff_t, 2> base_offsets,
                                         element_size_t stored_value_size,
                                         std::ptrdiff_t offset)
        : test_case_base_t<test_ptrs_t>(std::move(name), std::move(func),
                                        registers_used.governing_p,
                                        element_size_t::DOUBLE)
        , registers_used_(registers_used)
        , stored_value_size_(stored_value_size)
        , expected_values_(std::array<std::ptrdiff_t, 2> { offset, offset },
                           stored_value_size)
        , index_(static_cast<int64_t>(offset))
    {
        base_ptrs_[0] =
            static_cast<const uint8_t *>(OUTPUT_DATA.base_addr()) + base_offsets[0];
        base_ptrs_[1] =
            static_cast<const uint8_t *>(OUTPUT_DATA.base_addr()) + base_offsets[1];
        std::memcpy(base_data_.data(), base_ptrs_.data(), base_data_.size());
    }

    void
    setup(sve_register_file_t &register_values) override
    {
        // Set the value for the base register.
        register_values.set_z_register_value(registers_used_.base_z, base_data_);

        register_values.set_z_register_value(registers_used_.src_z,
                                             { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                               0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13,
                                               0x14, 0x15 });
        OUTPUT_DATA.reset();
    }

    void
    check_output(predicate_reg_value128_t pred,
                 const test_register_data_t &register_data) override
    {
        // Check that the values of the Z registers have been preserved.
        for (size_t i = 0; i < NUM_Z_REGS; i++) {
            check_z_reg(i, register_data);
        }
        // Check that the values of the P registers have been preserved.
        for (size_t i = 0; i < NUM_P_REGS; i++) {
            check_p_reg(i, register_data);
        }

        const bool scaled = false;
        assert(element_size_ == element_size_t::DOUBLE);

        switch (stored_value_size_) {
        case element_size_t::BYTE:
            check_expected_values(expected_values_.u8x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::HALF:
            check_expected_values(expected_values_.u16x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::SINGLE:
            check_expected_values(expected_values_.u32x2, pred, base_ptrs_, scaled);
            break;
        case element_size_t::DOUBLE:
            check_expected_values(expected_values_.u64x2, pred, base_ptrs_, scaled);
            break;
        }
    }

    test_ptrs_t
    create_test_ptrs(test_register_data_t &register_data) override
    {
        return {
            register_data.before.z.data(),
            register_data.before.p.data(),
            register_data.after.z.data(),
            register_data.after.p.data(),
            index_,
        };
    }
#    undef TEST_FUNC
};

test_result_t
test_st1_vector_plus_scalar()
{
#    define TEST_FUNC(st_instruction)                                          \
        [](vector_plus_scalar_load_test_case_t::test_ptrs_t &ptrs) {           \
            asm(/* clang-format off */                                      \
            RESTORE_Z_REGISTERS(z_restore_base)                             \
            RESTORE_P_REGISTERS(p_restore_base)                             \
            st_instruction "\n"                                             \
            SAVE_Z_REGISTERS(z_save_base)                                   \
            SAVE_P_REGISTERS(p_save_base) /* clang-format on */    \
                :                                                              \
                : [z_restore_base] "r"(ptrs.z_restore_base),                   \
                  [z_save_base] "r"(ptrs.z_save_base),                         \
                  [p_restore_base] "r"(ptrs.p_restore_base),                   \
                  [p_save_base] "r"(ptrs.p_save_base), [index] "r"(ptrs.index) \
                : ALL_Z_REGS, ALL_P_REGS, "memory");                           \
        }

    return run_tests<vector_plus_scalar_store_test_case_t>({
        /* {
         *     Test name,
         *     Function that executes the test instruction,
         *     Registers used {zt, pg, zn},
         *     Offsets
         *     Stored value size
         *     index value
         * },
         */
        /* TODO i#5036: Add tests for 32-bit element variants.
         *              For example: stnt1b z0.s, p0/z, [z31.s, x5].
         *              These instructions require 32-bit base pointers and I'm not sure
         *              how we can reliably and portably guarantee that allocated memory
         *              has an address that fits into 32-bits.
         */
        {
            "stnt1b vector+scalar 64bit unscaled offset",
            TEST_FUNC("stnt1b z0.d, p7, [z28.d, %[index]]"),
            { /*zt=*/0, /*pg=*/7, /*zn=*/28 },
            std::array<std::ptrdiff_t, 2> { 0, 16 },
            element_size_t::BYTE,
            0,
        },
        {
            "stnt1b vector+scalar 64bit unscaled offset (repeated base)",
            TEST_FUNC("stnt1b z3.d, p6, [z24.d, %[index]]"),
            { /*zt=*/3, /*pg=*/6, /*zn=*/24 },
            std::array<std::ptrdiff_t, 2> { 7, 7 },
            element_size_t::BYTE,
            0,
        },
        {
            "stnt1h vector+scalar 64bit unscaled offset",
            TEST_FUNC("stnt1h z7.d, p5, [z20.d, %[index]]"),
            { /*zt=*/7, /*pg=*/5, /*zn=*/20 },
            std::array<std::ptrdiff_t, 2> { -32, -16 },
            element_size_t::HALF,
            -10,
        },
        {
            "stnt1h vector+scalar 64bit unscaled offset (repeated base)",
            TEST_FUNC("stnt1h z11.d, p4, [z16.d, %[index]]"),
            { /*zt=*/11, /*pg=*/4, /*zn=*/16 },
            std::array<std::ptrdiff_t, 2> { -32, -32 },
            element_size_t::HALF,
            -10,
        },
        {
            "stnt1w vector+scalar 64bit unscaled offset",
            TEST_FUNC("stnt1w z15.d, p3, [z12.d, %[index]]"),
            { /*zt=*/15, /*pg=*/3, /*zn=*/12 },
            std::array<std::ptrdiff_t, 2> { 14, 100 },
            element_size_t::SINGLE,
            32,
        },
        {
            "stnt1w vector+scalar 64bit unscaled offset (repeated base)",
            TEST_FUNC("stnt1w z19.d, p2, [z8.d, %[index]]"),
            { /*zt=*/19, /*pg=*/2, /*zn=*/8 },
            std::array<std::ptrdiff_t, 2> { 14, 14 },
            element_size_t::SINGLE,
            32,
        },
        {
            "stnt1d vector+scalar 64bit unscaled offset",
            TEST_FUNC("stnt1d z23.d, p1, [z4.d, %[index]]"),
            { /*zt=*/23, /*pg=*/1, /*zn=*/4 },
            std::array<std::ptrdiff_t, 2> { -16, 16 },
            element_size_t::DOUBLE,
            50,
        },
        {
            "stnt1d vector+scalar 64bit unscaled offset (repeated base)",
            TEST_FUNC("stnt1d z27.d, p0, [z0.d, %[index]]"),
            { /*zt=*/27, /*pg=*/0, /*zn=*/0 },
            std::array<std::ptrdiff_t, 2> { -16, 16 },
            element_size_t::DOUBLE,
            50,
        },
    });
}

#endif // defined(__ARM_FEATURE_SVE2)
} // namespace

int
main(int argc, char **argv)
{
    test_result_t status = PASS;
#if defined(__ARM_FEATURE_SVE)
    if (test_ld1_scalar_plus_vector() == FAIL)
        status = FAIL;
    if (test_st1_scalar_plus_vector() == FAIL)
        status = FAIL;
    if (test_ld1_vector_plus_immediate() == FAIL)
        status = FAIL;
    if (test_st1_vector_plus_immediate() == FAIL)
        status = FAIL;
    if (test_ld1_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_ld2_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_ld3_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_ld4_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_st1_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_st2_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_st3_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_st4_scalar_plus_scalar() == FAIL)
        status = FAIL;
    if (test_ld1_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_ld2_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_ld3_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_ld4_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_st1_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_st2_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_st3_scalar_plus_immediate() == FAIL)
        status = FAIL;
    if (test_st4_scalar_plus_immediate() == FAIL)
        status = FAIL;
#endif
#if defined(__ARM_FEATURE_SVE2)
    if (test_ld1_vector_plus_scalar() == FAIL)
        status = FAIL;
    if (test_st1_vector_plus_scalar() == FAIL)
        status = FAIL;
#endif

    return status == PASS ? 0 : 1;
}
