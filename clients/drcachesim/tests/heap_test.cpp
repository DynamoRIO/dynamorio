/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

#include "tools.h"
#include <new>
#include <stdio.h>

namespace {

static void
test_operators()
{
    const int iters = 99;
    for (int i = 0; i < iters; ++i) {
        // Invoke all of the global operator news and deletes.
        // We do not call destructors for our explicit operator calls: we are
        // not allocating anything that needs it.

        // void *operator new(std::size_t count);
        int *int_ptr = new int;
        // void operator delete(void *ptr) noexcept;
        delete int_ptr;

        // void *operator new[](std::size_t count);
        // We use a different variable to avoid false positives in clang's check
        // for new..delete[].
        int *int_arr = new int[42];
        // void operator delete[](void *ptr) noexcept;
        delete[] int_arr;

        // void *operator new(std::size_t count, const std::nothrow_t &tag);
        int_ptr = new (std::nothrow) int;
        // void operator delete(void *ptr, const std::nothrow_t &tag) noexcept;
        ::operator delete(int_ptr, std::nothrow);
        // void *operator new[](std::size_t count, const std::nothrow_t &tag);
        int_arr = new (std::nothrow) int[42];
        // void operator delete[](void *ptr, const std::nothrow_t &tag) noexcept;
        ::operator delete[](int_arr, std::nothrow);

#if defined(__cpp_aligned_new)
        // void *operator new(std::size_t count, std::align_val_t al);
        class alignas(64) align64 {
        public: // Public to avoid warnings about unused field.
            int x = 0;
        };
        auto *aligned_class = new align64;
        int *aligned_int = new (std::align_val_t { 64 }) int;
        // void operator delete(void *ptr, std::align_val_t al) noexcept;
        delete aligned_class;
        // Some compilers (MSVC) will complain with just "delete".
        ::operator delete (aligned_int, std::align_val_t { 64 });

        // void *operator new[](std::size_t count, std::align_val_t al);
        auto *class_array = new align64[4];
        int *aligned_arr = new (std::align_val_t { 64 }) int[42];
        // void operator delete[](void *ptr, std::align_val_t al) noexcept;
        delete[] class_array;
        // Some compilers (MSVC) will complain with just "delete[]".
        ::operator delete[](aligned_arr, std::align_val_t { 64 });

        // void *operator new(std::size_t count, std::align_val_t al,
        //                    const std::nothrow_t &);
        aligned_class = new (std::nothrow) align64;
        aligned_int = new (std::align_val_t { 64 }, std::nothrow) int;
        // void operator delete(void *ptr, std::align_val_t al,
        //                      const std::nothrow_t &tag) noexcept;
        ::operator delete(aligned_class, std::nothrow);
        ::operator delete (aligned_int, std::align_val_t { 64 }, std::nothrow);
        // void *operator new[](std::size_t count, std::align_val_t al,
        //                     const std::nothrow_t &);
        class_array = new (std::nothrow) align64[4];
        aligned_arr = new (std::align_val_t { 64 }, std::nothrow) int[42];
        // void operator delete[](void *ptr, std::align_val_t al,
        //                        const std::nothrow_t &tag) noexcept;
        ::operator delete[](class_array, std::nothrow);
        ::operator delete[](aligned_arr, std::align_val_t { 64 }, std::nothrow);
#endif
#if defined(__cpp_sized_deallocation)
        // void operator delete(void *ptr, std::size_t sz) noexcept;
        int_ptr = new int;
        ::operator delete(int_ptr, sizeof(*int_ptr));
        // void operator delete[](void *ptr, std::size_t sz) noexcept;
        int_arr = new int[42];
        ::operator delete[](int_arr, 42 * sizeof(*int_arr));
#endif
#if defined(__cpp_aligned_new) && defined(__cpp_sized_deallocation)
        aligned_class = new align64;
        aligned_int = new (std::align_val_t { 64 }) int;
        // void operator delete(void *ptr, std::size_t sz, std::align_val_t al) noexcept;
        ::operator delete (aligned_class, sizeof(*aligned_class),
                           std::align_val_t { alignof(*aligned_class) });
        ::operator delete (aligned_int, sizeof(*aligned_int), std::align_val_t { 64 });

        class_array = new align64[4];
        aligned_arr = new (std::align_val_t { 64 }) int[42];
        // void operator delete[](void *ptr, std::size_t sz, std::align_val_t al)
        // noexcept;
        ::operator delete[](class_array, 4 * sizeof(*class_array),
                            std::align_val_t { alignof(*class_array) });
        ::operator delete[](aligned_arr, 42 * sizeof(*aligned_arr),
                            std::align_val_t { 64 });
#endif
        // void *operator new(std::size_t count, void *ptr);
        void *buf = malloc(sizeof(*int_ptr));
        int_ptr = new (buf) int;
        // void operator delete(void *ptr, void *place) noexcept;
        ::operator delete(int_ptr, buf);
        free(buf);

        // void *operator new[](std::size_t count, void *ptr);
        buf = malloc(42 * sizeof(*int_arr));
        int_arr = new (buf) int[42];
        // void operator delete[](void *ptr, void *place) noexcept;
        ::operator delete[](int_arr, buf);
        free(buf);
    }
}

} // namespace

int
main()
{
    test_operators();
    print("All done.\n");
    return 0;
}
