/* *******************************************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

/*
 * memquery_test_data.h - A collection of table-test data used in unit tests for
 * the unix memquery logic; it is included directly into memquery_test.h.
 */

fake_memquery_result results_0[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464e4000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071000000,
                         .vm_end = (app_pc)0x0000000071380000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x00000000715fc000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000000007157f000,
                         .vm_end = (app_pc)0x00000000715a7000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000715a7000,
                         .vm_end = (app_pc)0x00000000715c5000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000715c5000,
                         .vm_end = (app_pc)0x00000000715fc000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a688000,
                         .vm_end = (app_pc)0x00007f770a6a0000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a6a0000,
                         .vm_end = (app_pc)0x00007f770a89f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a89f000,
                         .vm_end = (app_pc)0x00007f770a8a0000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a0000,
                         .vm_end = (app_pc)0x00007f770a8a1000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a1000,
                         .vm_end = (app_pc)0x00007f770a8a5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a5000,
                         .vm_end = (app_pc)0x00007f770a9a8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a9a8000,
                         .vm_end = (app_pc)0x00007f770aba7000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba7000,
                         .vm_end = (app_pc)0x00007f770aba8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba8000,
                         .vm_end = (app_pc)0x00007f770aba9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba9000,
                         .vm_end = (app_pc)0x00007f770abac000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770abac000,
                         .vm_end = (app_pc)0x00007f770adab000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adab000,
                         .vm_end = (app_pc)0x00007f770adac000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adac000,
                         .vm_end = (app_pc)0x00007f770adad000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adad000,
                         .vm_end = (app_pc)0x00007f770af42000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770af42000,
                         .vm_end = (app_pc)0x00007f770b142000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b142000,
                         .vm_end = (app_pc)0x00007f770b146000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b146000,
                         .vm_end = (app_pc)0x00007f770b148000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b148000,
                         .vm_end = (app_pc)0x00007f770b14c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b14c000,
                         .vm_end = (app_pc)0x00007f770b16f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b340000,
                         .vm_end = (app_pc)0x00007f770b342000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b36c000,
                         .vm_end = (app_pc)0x00007f770b36f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b36f000,
                         .vm_end = (app_pc)0x00007f770b370000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b370000,
                         .vm_end = (app_pc)0x00007f770b371000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b371000,
                         .vm_end = (app_pc)0x00007f770b372000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b68b000,
                         .vm_end = (app_pc)0x00007ffe5b6ad000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b71d000,
                         .vm_end = (app_pc)0x00007ffe5b720000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b720000,
                         .vm_end = (app_pc)0x00007ffe5b722000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_0 = {
    .iters = results_0,
    .iters_count = 40,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/bin64/unit_tests",
    .want_return = 4,
    .want_start = (app_pc)0x0000000071000000,
    .want_end = (app_pc)0x00000000715fc000,
};

fake_memquery_result results_1[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464e5000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071000000,
                         .vm_end = (app_pc)0x0000000071380000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x00000000715fc000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000000007157f000,
                         .vm_end = (app_pc)0x00000000715a7000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000715a7000,
                         .vm_end = (app_pc)0x00000000715c5000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "bin64/unit_tests" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000715c5000,
                         .vm_end = (app_pc)0x00000000715fc000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a688000,
                         .vm_end = (app_pc)0x00007f770a6a0000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a6a0000,
                         .vm_end = (app_pc)0x00007f770a89f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a89f000,
                         .vm_end = (app_pc)0x00007f770a8a0000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a0000,
                         .vm_end = (app_pc)0x00007f770a8a1000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a1000,
                         .vm_end = (app_pc)0x00007f770a8a5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a8a5000,
                         .vm_end = (app_pc)0x00007f770a9a8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770a9a8000,
                         .vm_end = (app_pc)0x00007f770aba7000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba7000,
                         .vm_end = (app_pc)0x00007f770aba8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba8000,
                         .vm_end = (app_pc)0x00007f770aba9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770aba9000,
                         .vm_end = (app_pc)0x00007f770abac000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770abac000,
                         .vm_end = (app_pc)0x00007f770adab000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adab000,
                         .vm_end = (app_pc)0x00007f770adac000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adac000,
                         .vm_end = (app_pc)0x00007f770adad000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770adad000,
                         .vm_end = (app_pc)0x00007f770af42000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770af42000,
                         .vm_end = (app_pc)0x00007f770b142000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b142000,
                         .vm_end = (app_pc)0x00007f770b146000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b146000,
                         .vm_end = (app_pc)0x00007f770b148000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b148000,
                         .vm_end = (app_pc)0x00007f770b14c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b14c000,
                         .vm_end = (app_pc)0x00007f770b16f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b340000,
                         .vm_end = (app_pc)0x00007f770b342000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b36c000,
                         .vm_end = (app_pc)0x00007f770b36f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b36f000,
                         .vm_end = (app_pc)0x00007f770b370000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b370000,
                         .vm_end = (app_pc)0x00007f770b371000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f770b371000,
                         .vm_end = (app_pc)0x00007f770b372000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b68b000,
                         .vm_end = (app_pc)0x00007ffe5b6ad000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b71d000,
                         .vm_end = (app_pc)0x00007ffe5b720000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe5b720000,
                         .vm_end = (app_pc)0x00007ffe5b722000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_1 = {
    .iters = results_1,
    .iters_count = 40,
    .in_start = (app_pc)0x0000000071000000,
    .want_return = 4,
    .want_start = (app_pc)0x0000000071000000,
    .want_end = (app_pc)0x00000000715fc000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/bin64/",
    .want_filename = "unit_tests",
};

fake_memquery_result results_4[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f0d1000,
                         .vm_end = (app_pc)0x00007ff59f467000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f666000,
                         .vm_end = (app_pc)0x00007ff59f6ac000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f6ac000,
                         .vm_end = (app_pc)0x00007ff59f6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe480d000,
                         .vm_end = (app_pc)0x00007fffe482e000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe499c000,
                         .vm_end = (app_pc)0x00007fffe499f000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe499f000,
                         .vm_end = (app_pc)0x00007fffe49a1000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_4 = {
    .iters = results_4,
    .iters_count = 6,
    .in_start = (app_pc)0x00007ff59f0d1000,
    .want_return = 3,
    .want_start = (app_pc)0x00007ff59f0d1000,
    .want_end = (app_pc)0x00007ff59f6e3000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_5[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59aecb000,
                         .vm_end = (app_pc)0x00007ff59aece000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.execve-sub64" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000205000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59aece000,
                         .vm_end = (app_pc)0x00007ff59b0ce000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59b0ce000,
                         .vm_end = (app_pc)0x00007ff59b0d0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.execve-sub64" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59b0d0000,
                         .vm_end = (app_pc)0x00007ff59b0d1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59b0d1000,
                         .vm_end = (app_pc)0x00007ff59b0d2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59eeaa000,
                         .vm_end = (app_pc)0x00007ff59eecd000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59eecd000,
                         .vm_end = (app_pc)0x00007ff59f0cd000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f0cd000,
                         .vm_end = (app_pc)0x00007ff59f0cf000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f0cf000,
                         .vm_end = (app_pc)0x00007ff59f0d0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f0d0000,
                         .vm_end = (app_pc)0x00007ff59f0d1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f0d1000,
                         .vm_end = (app_pc)0x00007ff59f467000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f467000,
                         .vm_end = (app_pc)0x00007ff59f666000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f666000,
                         .vm_end = (app_pc)0x00007ff59f6ac000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff59f6ac000,
                         .vm_end = (app_pc)0x00007ff59f6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe480d000,
                         .vm_end = (app_pc)0x00007fffe482e000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe499c000,
                         .vm_end = (app_pc)0x00007fffe499f000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffe499f000,
                         .vm_end = (app_pc)0x00007fffe49a1000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_5 = {
    .iters = results_5,
    .iters_count = 17,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "linux.execve-sub64",
    .want_return = 3,
    .want_start = (app_pc)0x00007ff59aecb000,
    .want_end = (app_pc)0x00007ff59b0d0000,
};

fake_memquery_result results_6[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04f99000,
                         .vm_end = (app_pc)0x00007f0c0532f000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c0552e000,
                         .vm_end = (app_pc)0x00007f0c05574000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c05574000,
                         .vm_end = (app_pc)0x00007f0c055ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe457ea000,
                         .vm_end = (app_pc)0x00007ffe4580c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe45874000,
                         .vm_end = (app_pc)0x00007ffe45877000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe45877000,
                         .vm_end = (app_pc)0x00007ffe45879000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_6 = {
    .iters = results_6,
    .iters_count = 6,
    .in_start = (app_pc)0x00007f0c04f99000,
    .want_return = 3,
    .want_start = (app_pc)0x00007f0c04f99000,
    .want_end = (app_pc)0x00007f0c055ab000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_7[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c00d93000,
                         .vm_end = (app_pc)0x00007f0c00d97000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.execve64" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000205000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c00d97000,
                         .vm_end = (app_pc)0x00007f0c00f96000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c00f96000,
                         .vm_end = (app_pc)0x00007f0c00f98000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.execve64" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c00f98000,
                         .vm_end = (app_pc)0x00007f0c00f99000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c00f99000,
                         .vm_end = (app_pc)0x00007f0c00f9a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04d72000,
                         .vm_end = (app_pc)0x00007f0c04d95000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04d95000,
                         .vm_end = (app_pc)0x00007f0c04f95000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04f95000,
                         .vm_end = (app_pc)0x00007f0c04f97000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04f97000,
                         .vm_end = (app_pc)0x00007f0c04f98000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04f98000,
                         .vm_end = (app_pc)0x00007f0c04f99000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c04f99000,
                         .vm_end = (app_pc)0x00007f0c0532f000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c0532f000,
                         .vm_end = (app_pc)0x00007f0c0552e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c0552e000,
                         .vm_end = (app_pc)0x00007f0c05574000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f0c05574000,
                         .vm_end = (app_pc)0x00007f0c055ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe457ea000,
                         .vm_end = (app_pc)0x00007ffe4580c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe45874000,
                         .vm_end = (app_pc)0x00007ffe45877000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe45877000,
                         .vm_end = (app_pc)0x00007ffe45879000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_7 = {
    .iters = results_7,
    .iters_count = 17,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/linux.execve64",
    .want_return = 3,
    .want_start = (app_pc)0x00007f0c00d93000,
    .want_end = (app_pc)0x00007f0c00f98000,
};

fake_memquery_result results_189[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000400000,
                         .vm_end = (app_pc)0x00000000004b4000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.fib-static" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000400000,
        .mod_end = (app_pc)0x00000000006b8000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000004b4000,
                         .vm_end = (app_pc)0x00000000006b3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000006b3000,
                         .vm_end = (app_pc)0x00000000006b6000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/linux.fib-static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000006b6000,
                         .vm_end = (app_pc)0x00000000006b8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000006b8000,
                         .vm_end = (app_pc)0x00000000006b9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000006b9000,
                         .vm_end = (app_pc)0x00000000006ba000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fec19fbd000,
                         .vm_end = (app_pc)0x00007fec1a353000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fec1a353000,
                         .vm_end = (app_pc)0x00007fec1a552000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fec1a552000,
                         .vm_end = (app_pc)0x00007fec1a598000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fec1a598000,
                         .vm_end = (app_pc)0x00007fec1a5cf000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff49078000,
                         .vm_end = (app_pc)0x00007fff4909a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff49119000,
                         .vm_end = (app_pc)0x00007fff4911c000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4911c000,
                         .vm_end = (app_pc)0x00007fff4911e000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_189 = {
    .iters = results_189,
    .iters_count = 13,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/linux.fib-static",
    .want_return = 4,
    .want_start = (app_pc)0x0000000000400000,
    .want_end = (app_pc)0x00000000006b8000,
};

fake_memquery_result results_489[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x000055d337d5d000,
                         .vm_end = (app_pc)0x000055d33818c000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d33838b000,
                         .vm_end = (app_pc)0x000055d3383b4000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383b4000,
                         .vm_end = (app_pc)0x000055d3383d3000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383d3000,
                         .vm_end = (app_pc)0x000055d33840e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d338e29000,
                         .vm_end = (app_pc)0x000055d338e5b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6670000,
                         .vm_end = (app_pc)0x00007f6ba6805000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6805000,
                         .vm_end = (app_pc)0x00007f6ba6a05000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a05000,
                         .vm_end = (app_pc)0x00007f6ba6a09000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a09000,
                         .vm_end = (app_pc)0x00007f6ba6a0b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0b000,
                         .vm_end = (app_pc)0x00007f6ba6a0f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0f000,
                         .vm_end = (app_pc)0x00007f6ba6a26000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a26000,
                         .vm_end = (app_pc)0x00007f6ba6c25000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c25000,
                         .vm_end = (app_pc)0x00007f6ba6c26000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c26000,
                         .vm_end = (app_pc)0x00007f6ba6c27000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c27000,
                         .vm_end = (app_pc)0x00007f6ba6d2a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6d2a000,
                         .vm_end = (app_pc)0x00007f6ba6f29000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f29000,
                         .vm_end = (app_pc)0x00007f6ba6f2a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2a000,
                         .vm_end = (app_pc)0x00007f6ba6f2b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2b000,
                         .vm_end = (app_pc)0x00007f6ba70a1000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba70a1000,
                         .vm_end = (app_pc)0x00007f6ba72a1000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72a1000,
                         .vm_end = (app_pc)0x00007f6ba72ab000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ab000,
                         .vm_end = (app_pc)0x00007f6ba72ad000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ad000,
                         .vm_end = (app_pc)0x00007f6ba72b0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b0000,
                         .vm_end = (app_pc)0x00007f6ba72b3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b3000,
                         .vm_end = (app_pc)0x00007f6ba74b2000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b2000,
                         .vm_end = (app_pc)0x00007f6ba74b3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b3000,
                         .vm_end = (app_pc)0x00007f6ba74b4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b4000,
                         .vm_end = (app_pc)0x00007f6ba74d7000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76a5000,
                         .vm_end = (app_pc)0x00007f6ba76aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d4000,
                         .vm_end = (app_pc)0x00007f6ba76d7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d7000,
                         .vm_end = (app_pc)0x00007f6ba76d8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d8000,
                         .vm_end = (app_pc)0x00007f6ba76d9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d9000,
                         .vm_end = (app_pc)0x00007f6ba76da000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f099000,
                         .vm_end = (app_pc)0x00007fff0f0bb000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11b000,
                         .vm_end = (app_pc)0x00007fff0f11e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11e000,
                         .vm_end = (app_pc)0x00007fff0f120000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_489 = {

    .iters = results_489,
    .iters_count = 36,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/clients/bin64/"
               "tool.drcacheoff.burst_static",
    .want_return = 4,
    .want_start = (app_pc)0x000055d337d5d000,
    .want_end = (app_pc)0x000055d33840e000,

};

fake_memquery_result results_490[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d5c000,
                         .vm_end = (app_pc)0x000055d337d5d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d337d5d000,
                         .vm_end = (app_pc)0x000055d33818c000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d33838b000,
                         .vm_end = (app_pc)0x000055d3383b4000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383b4000,
                         .vm_end = (app_pc)0x000055d3383d3000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383d3000,
                         .vm_end = (app_pc)0x000055d33840e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d338e29000,
                         .vm_end = (app_pc)0x000055d338e5b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6670000,
                         .vm_end = (app_pc)0x00007f6ba6805000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6805000,
                         .vm_end = (app_pc)0x00007f6ba6a05000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a05000,
                         .vm_end = (app_pc)0x00007f6ba6a09000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a09000,
                         .vm_end = (app_pc)0x00007f6ba6a0b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0b000,
                         .vm_end = (app_pc)0x00007f6ba6a0f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0f000,
                         .vm_end = (app_pc)0x00007f6ba6a26000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a26000,
                         .vm_end = (app_pc)0x00007f6ba6c25000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c25000,
                         .vm_end = (app_pc)0x00007f6ba6c26000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c26000,
                         .vm_end = (app_pc)0x00007f6ba6c27000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c27000,
                         .vm_end = (app_pc)0x00007f6ba6d2a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6d2a000,
                         .vm_end = (app_pc)0x00007f6ba6f29000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f29000,
                         .vm_end = (app_pc)0x00007f6ba6f2a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2a000,
                         .vm_end = (app_pc)0x00007f6ba6f2b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2b000,
                         .vm_end = (app_pc)0x00007f6ba70a1000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba70a1000,
                         .vm_end = (app_pc)0x00007f6ba72a1000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72a1000,
                         .vm_end = (app_pc)0x00007f6ba72ab000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ab000,
                         .vm_end = (app_pc)0x00007f6ba72ad000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ad000,
                         .vm_end = (app_pc)0x00007f6ba72b0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b0000,
                         .vm_end = (app_pc)0x00007f6ba72b3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b3000,
                         .vm_end = (app_pc)0x00007f6ba74b2000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b2000,
                         .vm_end = (app_pc)0x00007f6ba74b3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b3000,
                         .vm_end = (app_pc)0x00007f6ba74b4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b4000,
                         .vm_end = (app_pc)0x00007f6ba74d7000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76a5000,
                         .vm_end = (app_pc)0x00007f6ba76aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d4000,
                         .vm_end = (app_pc)0x00007f6ba76d7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d7000,
                         .vm_end = (app_pc)0x00007f6ba76d8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d8000,
                         .vm_end = (app_pc)0x00007f6ba76d9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d9000,
                         .vm_end = (app_pc)0x00007f6ba76da000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f099000,
                         .vm_end = (app_pc)0x00007fff0f0bb000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11b000,
                         .vm_end = (app_pc)0x00007fff0f11e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11e000,
                         .vm_end = (app_pc)0x00007fff0f120000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_490 = {

    .iters = results_490,
    .iters_count = 37,
    .in_start = (app_pc)0x000055d3383b5010,
    .want_return = 2,
    .want_start = (app_pc)0x000055d337d5d000,
    .want_end = (app_pc)0x000055d33840e000,

};

fake_memquery_result results_491[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d5c000,
                         .vm_end = (app_pc)0x000055d327d5d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d5d000,
                         .vm_end = (app_pc)0x000055d327d63000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d63000,
                         .vm_end = (app_pc)0x000055d327d65000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d65000,
                         .vm_end = (app_pc)0x000055d327d6f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d6f000,
                         .vm_end = (app_pc)0x000055d327d75000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d75000,
                         .vm_end = (app_pc)0x000055d327d76000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d76000,
                         .vm_end = (app_pc)0x000055d327d7d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d7d000,
                         .vm_end = (app_pc)0x000055d327d7e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d7e000,
                         .vm_end = (app_pc)0x000055d337d5d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d337d5d000,
                         .vm_end = (app_pc)0x000055d33818c000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d33838b000,
                         .vm_end = (app_pc)0x000055d3383b4000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383b4000,
                         .vm_end = (app_pc)0x000055d3383d3000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383d3000,
                         .vm_end = (app_pc)0x000055d33840e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d338e29000,
                         .vm_end = (app_pc)0x000055d338e5b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6670000,
                         .vm_end = (app_pc)0x00007f6ba6805000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6805000,
                         .vm_end = (app_pc)0x00007f6ba6a05000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a05000,
                         .vm_end = (app_pc)0x00007f6ba6a09000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a09000,
                         .vm_end = (app_pc)0x00007f6ba6a0b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0b000,
                         .vm_end = (app_pc)0x00007f6ba6a0f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0f000,
                         .vm_end = (app_pc)0x00007f6ba6a26000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a26000,
                         .vm_end = (app_pc)0x00007f6ba6c25000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c25000,
                         .vm_end = (app_pc)0x00007f6ba6c26000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c26000,
                         .vm_end = (app_pc)0x00007f6ba6c27000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c27000,
                         .vm_end = (app_pc)0x00007f6ba6d2a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6d2a000,
                         .vm_end = (app_pc)0x00007f6ba6f29000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f29000,
                         .vm_end = (app_pc)0x00007f6ba6f2a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2a000,
                         .vm_end = (app_pc)0x00007f6ba6f2b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2b000,
                         .vm_end = (app_pc)0x00007f6ba70a1000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba70a1000,
                         .vm_end = (app_pc)0x00007f6ba72a1000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72a1000,
                         .vm_end = (app_pc)0x00007f6ba72ab000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ab000,
                         .vm_end = (app_pc)0x00007f6ba72ad000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ad000,
                         .vm_end = (app_pc)0x00007f6ba72b0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b0000,
                         .vm_end = (app_pc)0x00007f6ba72b3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b3000,
                         .vm_end = (app_pc)0x00007f6ba74b2000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b2000,
                         .vm_end = (app_pc)0x00007f6ba74b3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b3000,
                         .vm_end = (app_pc)0x00007f6ba74b4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b4000,
                         .vm_end = (app_pc)0x00007f6ba74d7000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76a5000,
                         .vm_end = (app_pc)0x00007f6ba76aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d4000,
                         .vm_end = (app_pc)0x00007f6ba76d7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d7000,
                         .vm_end = (app_pc)0x00007f6ba76d8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d8000,
                         .vm_end = (app_pc)0x00007f6ba76d9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d9000,
                         .vm_end = (app_pc)0x00007f6ba76da000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f099000,
                         .vm_end = (app_pc)0x00007fff0f0bb000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11b000,
                         .vm_end = (app_pc)0x00007fff0f11e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11e000,
                         .vm_end = (app_pc)0x00007fff0f120000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_491 = {

    .iters = results_491,
    .iters_count = 45,
    .in_start = (app_pc)0x000055d338019d20,
    .want_return = 4,
    .want_start = (app_pc)0x000055d337d5d000,
    .want_end = (app_pc)0x000055d33840e000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/clients/bin64/",
    .want_filename = "tool.drcacheoff.burst_static",

};

fake_memquery_result results_492[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x000055d327d5b000,
                         .vm_end = (app_pc)0x000055d337d5d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d337d5d000,
                         .vm_end = (app_pc)0x000055d33818c000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d33838b000,
                         .vm_end = (app_pc)0x000055d3383b4000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383b4000,
                         .vm_end = (app_pc)0x000055d3383d3000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_static" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d3383d3000,
                         .vm_end = (app_pc)0x000055d33840e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055d338e29000,
                         .vm_end = (app_pc)0x000055d338e5b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6670000,
                         .vm_end = (app_pc)0x00007f6ba6805000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6805000,
                         .vm_end = (app_pc)0x00007f6ba6a05000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a05000,
                         .vm_end = (app_pc)0x00007f6ba6a09000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a09000,
                         .vm_end = (app_pc)0x00007f6ba6a0b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0b000,
                         .vm_end = (app_pc)0x00007f6ba6a0f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a0f000,
                         .vm_end = (app_pc)0x00007f6ba6a26000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6a26000,
                         .vm_end = (app_pc)0x00007f6ba6c25000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c25000,
                         .vm_end = (app_pc)0x00007f6ba6c26000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c26000,
                         .vm_end = (app_pc)0x00007f6ba6c27000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6c27000,
                         .vm_end = (app_pc)0x00007f6ba6d2a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6d2a000,
                         .vm_end = (app_pc)0x00007f6ba6f29000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f29000,
                         .vm_end = (app_pc)0x00007f6ba6f2a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2a000,
                         .vm_end = (app_pc)0x00007f6ba6f2b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba6f2b000,
                         .vm_end = (app_pc)0x00007f6ba70a1000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba70a1000,
                         .vm_end = (app_pc)0x00007f6ba72a1000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72a1000,
                         .vm_end = (app_pc)0x00007f6ba72ab000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ab000,
                         .vm_end = (app_pc)0x00007f6ba72ad000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72ad000,
                         .vm_end = (app_pc)0x00007f6ba72b0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b0000,
                         .vm_end = (app_pc)0x00007f6ba72b3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba72b3000,
                         .vm_end = (app_pc)0x00007f6ba74b2000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b2000,
                         .vm_end = (app_pc)0x00007f6ba74b3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b3000,
                         .vm_end = (app_pc)0x00007f6ba74b4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba74b4000,
                         .vm_end = (app_pc)0x00007f6ba74d7000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76a5000,
                         .vm_end = (app_pc)0x00007f6ba76aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d4000,
                         .vm_end = (app_pc)0x00007f6ba76d7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d7000,
                         .vm_end = (app_pc)0x00007f6ba76d8000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d8000,
                         .vm_end = (app_pc)0x00007f6ba76d9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f6ba76d9000,
                         .vm_end = (app_pc)0x00007f6ba76da000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f099000,
                         .vm_end = (app_pc)0x00007fff0f0bb000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11b000,
                         .vm_end = (app_pc)0x00007fff0f11e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fff0f11e000,
                         .vm_end = (app_pc)0x00007fff0f120000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_492 = {

    .iters = results_492,
    .iters_count = 37,
    .in_start = (app_pc)0x000055d3383b5010,
    .want_return = 2,
    .want_start = (app_pc)0x000055d337d5d000,
    .want_end = (app_pc)0x000055d33840e000,

};

fake_memquery_result results_504[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x0000561a2294a000,
                         .vm_end = (app_pc)0x0000561a22d79000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22f78000,
                         .vm_end = (app_pc)0x0000561a22fa1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fa1000,
                         .vm_end = (app_pc)0x0000561a22fc0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fc0000,
                         .vm_end = (app_pc)0x0000561a22ffb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a24936000,
                         .vm_end = (app_pc)0x0000561aa4968000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19023c000,
                         .vm_end = (app_pc)0x00007fe1903d1000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1903d1000,
                         .vm_end = (app_pc)0x00007fe1905d1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d1000,
                         .vm_end = (app_pc)0x00007fe1905d5000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d5000,
                         .vm_end = (app_pc)0x00007fe1905d7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d7000,
                         .vm_end = (app_pc)0x00007fe1905db000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905db000,
                         .vm_end = (app_pc)0x00007fe1905f2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905f2000,
                         .vm_end = (app_pc)0x00007fe1907f1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f1000,
                         .vm_end = (app_pc)0x00007fe1907f2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f2000,
                         .vm_end = (app_pc)0x00007fe1907f3000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f3000,
                         .vm_end = (app_pc)0x00007fe1908f6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1908f6000,
                         .vm_end = (app_pc)0x00007fe190af5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af5000,
                         .vm_end = (app_pc)0x00007fe190af6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af6000,
                         .vm_end = (app_pc)0x00007fe190af7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af7000,
                         .vm_end = (app_pc)0x00007fe190c6d000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190c6d000,
                         .vm_end = (app_pc)0x00007fe190e6d000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e6d000,
                         .vm_end = (app_pc)0x00007fe190e77000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e77000,
                         .vm_end = (app_pc)0x00007fe190e79000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e79000,
                         .vm_end = (app_pc)0x00007fe190e7c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7c000,
                         .vm_end = (app_pc)0x00007fe190e7f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7f000,
                         .vm_end = (app_pc)0x00007fe19107e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107e000,
                         .vm_end = (app_pc)0x00007fe19107f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107f000,
                         .vm_end = (app_pc)0x00007fe191080000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191080000,
                         .vm_end = (app_pc)0x00007fe1910a3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191271000,
                         .vm_end = (app_pc)0x00007fe191276000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a0000,
                         .vm_end = (app_pc)0x00007fe1912a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a3000,
                         .vm_end = (app_pc)0x00007fe1912a4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a4000,
                         .vm_end = (app_pc)0x00007fe1912a5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a5000,
                         .vm_end = (app_pc)0x00007fe1912a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bde0000,
                         .vm_end = (app_pc)0x00007ffc3be02000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfce000,
                         .vm_end = (app_pc)0x00007ffc3bfd1000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfd1000,
                         .vm_end = (app_pc)0x00007ffc3bfd3000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_504 = {

    .iters = results_504,
    .iters_count = 36,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/clients/bin64/"
               "tool.drcacheoff.burst_noreach",
    .want_return = 4,
    .want_start = (app_pc)0x0000561a2294a000,
    .want_end = (app_pc)0x0000561a22ffb000,

};

fake_memquery_result results_505[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02949000,
                         .vm_end = (app_pc)0x0000561a2294a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a2294a000,
                         .vm_end = (app_pc)0x0000561a22d79000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22f78000,
                         .vm_end = (app_pc)0x0000561a22fa1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fa1000,
                         .vm_end = (app_pc)0x0000561a22fc0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fc0000,
                         .vm_end = (app_pc)0x0000561a22ffb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a24936000,
                         .vm_end = (app_pc)0x0000561aa4968000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19023c000,
                         .vm_end = (app_pc)0x00007fe1903d1000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1903d1000,
                         .vm_end = (app_pc)0x00007fe1905d1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d1000,
                         .vm_end = (app_pc)0x00007fe1905d5000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d5000,
                         .vm_end = (app_pc)0x00007fe1905d7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d7000,
                         .vm_end = (app_pc)0x00007fe1905db000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905db000,
                         .vm_end = (app_pc)0x00007fe1905f2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905f2000,
                         .vm_end = (app_pc)0x00007fe1907f1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f1000,
                         .vm_end = (app_pc)0x00007fe1907f2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f2000,
                         .vm_end = (app_pc)0x00007fe1907f3000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f3000,
                         .vm_end = (app_pc)0x00007fe1908f6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1908f6000,
                         .vm_end = (app_pc)0x00007fe190af5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af5000,
                         .vm_end = (app_pc)0x00007fe190af6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af6000,
                         .vm_end = (app_pc)0x00007fe190af7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af7000,
                         .vm_end = (app_pc)0x00007fe190c6d000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190c6d000,
                         .vm_end = (app_pc)0x00007fe190e6d000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e6d000,
                         .vm_end = (app_pc)0x00007fe190e77000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e77000,
                         .vm_end = (app_pc)0x00007fe190e79000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e79000,
                         .vm_end = (app_pc)0x00007fe190e7c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7c000,
                         .vm_end = (app_pc)0x00007fe190e7f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7f000,
                         .vm_end = (app_pc)0x00007fe19107e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107e000,
                         .vm_end = (app_pc)0x00007fe19107f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107f000,
                         .vm_end = (app_pc)0x00007fe191080000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191080000,
                         .vm_end = (app_pc)0x00007fe1910a3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191271000,
                         .vm_end = (app_pc)0x00007fe191276000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a0000,
                         .vm_end = (app_pc)0x00007fe1912a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a3000,
                         .vm_end = (app_pc)0x00007fe1912a4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a4000,
                         .vm_end = (app_pc)0x00007fe1912a5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a5000,
                         .vm_end = (app_pc)0x00007fe1912a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bde0000,
                         .vm_end = (app_pc)0x00007ffc3be02000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfce000,
                         .vm_end = (app_pc)0x00007ffc3bfd1000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfd1000,
                         .vm_end = (app_pc)0x00007ffc3bfd3000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_505 = {

    .iters = results_505,
    .iters_count = 37,
    .in_start = (app_pc)0x0000561a22fa2010,
    .want_return = 2,
    .want_start = (app_pc)0x0000561a2294a000,
    .want_end = (app_pc)0x0000561a22ffb000,

};

fake_memquery_result results_506[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02949000,
                         .vm_end = (app_pc)0x0000561a0294a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a0294a000,
                         .vm_end = (app_pc)0x0000561a02950000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02950000,
                         .vm_end = (app_pc)0x0000561a02952000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02952000,
                         .vm_end = (app_pc)0x0000561a0295c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a0295c000,
                         .vm_end = (app_pc)0x0000561a02962000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02962000,
                         .vm_end = (app_pc)0x0000561a02963000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02963000,
                         .vm_end = (app_pc)0x0000561a0296a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a0296a000,
                         .vm_end = (app_pc)0x0000561a0296b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a0296b000,
                         .vm_end = (app_pc)0x0000561a2294a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a2294a000,
                         .vm_end = (app_pc)0x0000561a22d79000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22f78000,
                         .vm_end = (app_pc)0x0000561a22fa1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fa1000,
                         .vm_end = (app_pc)0x0000561a22fc0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fc0000,
                         .vm_end = (app_pc)0x0000561a22ffb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a24936000,
                         .vm_end = (app_pc)0x0000561aa4968000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19023c000,
                         .vm_end = (app_pc)0x00007fe1903d1000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1903d1000,
                         .vm_end = (app_pc)0x00007fe1905d1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d1000,
                         .vm_end = (app_pc)0x00007fe1905d5000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d5000,
                         .vm_end = (app_pc)0x00007fe1905d7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d7000,
                         .vm_end = (app_pc)0x00007fe1905db000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905db000,
                         .vm_end = (app_pc)0x00007fe1905f2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905f2000,
                         .vm_end = (app_pc)0x00007fe1907f1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f1000,
                         .vm_end = (app_pc)0x00007fe1907f2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f2000,
                         .vm_end = (app_pc)0x00007fe1907f3000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f3000,
                         .vm_end = (app_pc)0x00007fe1908f6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1908f6000,
                         .vm_end = (app_pc)0x00007fe190af5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af5000,
                         .vm_end = (app_pc)0x00007fe190af6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af6000,
                         .vm_end = (app_pc)0x00007fe190af7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af7000,
                         .vm_end = (app_pc)0x00007fe190c6d000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190c6d000,
                         .vm_end = (app_pc)0x00007fe190e6d000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e6d000,
                         .vm_end = (app_pc)0x00007fe190e77000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e77000,
                         .vm_end = (app_pc)0x00007fe190e79000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e79000,
                         .vm_end = (app_pc)0x00007fe190e7c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7c000,
                         .vm_end = (app_pc)0x00007fe190e7f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7f000,
                         .vm_end = (app_pc)0x00007fe19107e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107e000,
                         .vm_end = (app_pc)0x00007fe19107f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107f000,
                         .vm_end = (app_pc)0x00007fe191080000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191080000,
                         .vm_end = (app_pc)0x00007fe1910a3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191271000,
                         .vm_end = (app_pc)0x00007fe191276000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a0000,
                         .vm_end = (app_pc)0x00007fe1912a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a3000,
                         .vm_end = (app_pc)0x00007fe1912a4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a4000,
                         .vm_end = (app_pc)0x00007fe1912a5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a5000,
                         .vm_end = (app_pc)0x00007fe1912a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bde0000,
                         .vm_end = (app_pc)0x00007ffc3be02000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfce000,
                         .vm_end = (app_pc)0x00007ffc3bfd1000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfd1000,
                         .vm_end = (app_pc)0x00007ffc3bfd3000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_506 = {

    .iters = results_506,
    .iters_count = 45,
    .in_start = (app_pc)0x0000561a22c06ded,
    .want_return = 4,
    .want_start = (app_pc)0x0000561a2294a000,
    .want_end = (app_pc)0x0000561a22ffb000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/clients/bin64/",
    .want_filename = "tool.drcacheoff.burst_noreach",

};

fake_memquery_result results_507[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02948000,
                         .vm_end = (app_pc)0x0000561a2294a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a2294a000,
                         .vm_end = (app_pc)0x0000561a22d79000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22f78000,
                         .vm_end = (app_pc)0x0000561a22fa1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fa1000,
                         .vm_end = (app_pc)0x0000561a22fc0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fc0000,
                         .vm_end = (app_pc)0x0000561a22ffb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a24936000,
                         .vm_end = (app_pc)0x0000561aa4968000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19023c000,
                         .vm_end = (app_pc)0x00007fe1903d1000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1903d1000,
                         .vm_end = (app_pc)0x00007fe1905d1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d1000,
                         .vm_end = (app_pc)0x00007fe1905d5000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d5000,
                         .vm_end = (app_pc)0x00007fe1905d7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d7000,
                         .vm_end = (app_pc)0x00007fe1905db000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905db000,
                         .vm_end = (app_pc)0x00007fe1905f2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905f2000,
                         .vm_end = (app_pc)0x00007fe1907f1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f1000,
                         .vm_end = (app_pc)0x00007fe1907f2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f2000,
                         .vm_end = (app_pc)0x00007fe1907f3000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f3000,
                         .vm_end = (app_pc)0x00007fe1908f6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1908f6000,
                         .vm_end = (app_pc)0x00007fe190af5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af5000,
                         .vm_end = (app_pc)0x00007fe190af6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af6000,
                         .vm_end = (app_pc)0x00007fe190af7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af7000,
                         .vm_end = (app_pc)0x00007fe190c6d000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190c6d000,
                         .vm_end = (app_pc)0x00007fe190e6d000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e6d000,
                         .vm_end = (app_pc)0x00007fe190e77000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e77000,
                         .vm_end = (app_pc)0x00007fe190e79000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e79000,
                         .vm_end = (app_pc)0x00007fe190e7c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7c000,
                         .vm_end = (app_pc)0x00007fe190e7f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7f000,
                         .vm_end = (app_pc)0x00007fe19107e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107e000,
                         .vm_end = (app_pc)0x00007fe19107f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107f000,
                         .vm_end = (app_pc)0x00007fe191080000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191080000,
                         .vm_end = (app_pc)0x00007fe1910a3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191271000,
                         .vm_end = (app_pc)0x00007fe191276000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a0000,
                         .vm_end = (app_pc)0x00007fe1912a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a3000,
                         .vm_end = (app_pc)0x00007fe1912a4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a4000,
                         .vm_end = (app_pc)0x00007fe1912a5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a5000,
                         .vm_end = (app_pc)0x00007fe1912a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bde0000,
                         .vm_end = (app_pc)0x00007ffc3be02000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfce000,
                         .vm_end = (app_pc)0x00007ffc3bfd1000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfd1000,
                         .vm_end = (app_pc)0x00007ffc3bfd3000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_507 = {

    .iters = results_507,
    .iters_count = 37,
    .in_start = (app_pc)0x0000561a22fa2010,
    .want_return = 2,
    .want_start = (app_pc)0x0000561a2294a000,
    .want_end = (app_pc)0x0000561a22ffb000,

};

fake_memquery_result results_508[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x0000561a02947000,
                         .vm_end = (app_pc)0x0000561a2294a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a2294a000,
                         .vm_end = (app_pc)0x0000561a22d79000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b1000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22f78000,
                         .vm_end = (app_pc)0x0000561a22fa1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fa1000,
                         .vm_end = (app_pc)0x0000561a22fc0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_noreach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a22fc0000,
                         .vm_end = (app_pc)0x0000561a22ffb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x0000561a24936000,
                         .vm_end = (app_pc)0x0000561aa4968000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19023c000,
                         .vm_end = (app_pc)0x00007fe1903d1000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1903d1000,
                         .vm_end = (app_pc)0x00007fe1905d1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d1000,
                         .vm_end = (app_pc)0x00007fe1905d5000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d5000,
                         .vm_end = (app_pc)0x00007fe1905d7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905d7000,
                         .vm_end = (app_pc)0x00007fe1905db000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905db000,
                         .vm_end = (app_pc)0x00007fe1905f2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1905f2000,
                         .vm_end = (app_pc)0x00007fe1907f1000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f1000,
                         .vm_end = (app_pc)0x00007fe1907f2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f2000,
                         .vm_end = (app_pc)0x00007fe1907f3000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1907f3000,
                         .vm_end = (app_pc)0x00007fe1908f6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1908f6000,
                         .vm_end = (app_pc)0x00007fe190af5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af5000,
                         .vm_end = (app_pc)0x00007fe190af6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af6000,
                         .vm_end = (app_pc)0x00007fe190af7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190af7000,
                         .vm_end = (app_pc)0x00007fe190c6d000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190c6d000,
                         .vm_end = (app_pc)0x00007fe190e6d000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e6d000,
                         .vm_end = (app_pc)0x00007fe190e77000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e77000,
                         .vm_end = (app_pc)0x00007fe190e79000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e79000,
                         .vm_end = (app_pc)0x00007fe190e7c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7c000,
                         .vm_end = (app_pc)0x00007fe190e7f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe190e7f000,
                         .vm_end = (app_pc)0x00007fe19107e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107e000,
                         .vm_end = (app_pc)0x00007fe19107f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe19107f000,
                         .vm_end = (app_pc)0x00007fe191080000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191080000,
                         .vm_end = (app_pc)0x00007fe1910a3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe191271000,
                         .vm_end = (app_pc)0x00007fe191276000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a0000,
                         .vm_end = (app_pc)0x00007fe1912a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a3000,
                         .vm_end = (app_pc)0x00007fe1912a4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a4000,
                         .vm_end = (app_pc)0x00007fe1912a5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fe1912a5000,
                         .vm_end = (app_pc)0x00007fe1912a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bde0000,
                         .vm_end = (app_pc)0x00007ffc3be02000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfce000,
                         .vm_end = (app_pc)0x00007ffc3bfd1000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc3bfd1000,
                         .vm_end = (app_pc)0x00007ffc3bfd3000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_508 = {

    .iters = results_508,
    .iters_count = 37,
    .in_start = (app_pc)0x0000561a22fa2010,
    .want_return = 2,
    .want_start = (app_pc)0x0000561a2294a000,
    .want_end = (app_pc)0x0000561a22ffb000,

};

fake_memquery_result results_509[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464e4000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f0756d2000,
                         .vm_end = (app_pc)0x000055f075872000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000003d7000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a71000,
                         .vm_end = (app_pc)0x000055f075a97000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a97000,
                         .vm_end = (app_pc)0x000055f075a98000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a98000,
                         .vm_end = (app_pc)0x000055f075aa9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f07728f000,
                         .vm_end = (app_pc)0x000055f0776e1000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deac7f000,
                         .vm_end = (app_pc)0x00007f2deae14000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deae14000,
                         .vm_end = (app_pc)0x00007f2deb014000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb014000,
                         .vm_end = (app_pc)0x00007f2deb018000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb018000,
                         .vm_end = (app_pc)0x00007f2deb01a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb01a000,
                         .vm_end = (app_pc)0x00007f2deb01e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb01e000,
                         .vm_end = (app_pc)0x00007f2deb035000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb035000,
                         .vm_end = (app_pc)0x00007f2deb234000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb234000,
                         .vm_end = (app_pc)0x00007f2deb235000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb235000,
                         .vm_end = (app_pc)0x00007f2deb236000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb236000,
                         .vm_end = (app_pc)0x00007f2deb339000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb339000,
                         .vm_end = (app_pc)0x00007f2deb538000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb538000,
                         .vm_end = (app_pc)0x00007f2deb539000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb539000,
                         .vm_end = (app_pc)0x00007f2deb53a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb53a000,
                         .vm_end = (app_pc)0x00007f2deb6b0000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb6b0000,
                         .vm_end = (app_pc)0x00007f2deb8b0000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8b0000,
                         .vm_end = (app_pc)0x00007f2deb8ba000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8ba000,
                         .vm_end = (app_pc)0x00007f2deb8bc000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8bc000,
                         .vm_end = (app_pc)0x00007f2deb8bf000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8bf000,
                         .vm_end = (app_pc)0x00007f2debc55000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debc55000,
                         .vm_end = (app_pc)0x00007f2debe54000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe54000,
                         .vm_end = (app_pc)0x00007f2debe7c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe7c000,
                         .vm_end = (app_pc)0x00007f2debe9a000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe9a000,
                         .vm_end = (app_pc)0x00007f2debed1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debed1000,
                         .vm_end = (app_pc)0x00007f2debee9000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debee9000,
                         .vm_end = (app_pc)0x00007f2dec0e8000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0e8000,
                         .vm_end = (app_pc)0x00007f2dec0e9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0e9000,
                         .vm_end = (app_pc)0x00007f2dec0ea000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0ea000,
                         .vm_end = (app_pc)0x00007f2dec0ee000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0ee000,
                         .vm_end = (app_pc)0x00007f2dec107000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021a000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec107000,
                         .vm_end = (app_pc)0x00007f2dec306000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec306000,
                         .vm_end = (app_pc)0x00007f2dec307000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec307000,
                         .vm_end = (app_pc)0x00007f2dec308000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec308000,
                         .vm_end = (app_pc)0x00007f2dec32b000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec3f8000,
                         .vm_end = (app_pc)0x00007f2dec4fe000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec528000,
                         .vm_end = (app_pc)0x00007f2dec52b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52b000,
                         .vm_end = (app_pc)0x00007f2dec52c000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52c000,
                         .vm_end = (app_pc)0x00007f2dec52d000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52d000,
                         .vm_end = (app_pc)0x00007f2dec52e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf847000,
                         .vm_end = (app_pc)0x00007fffcf869000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf977000,
                         .vm_end = (app_pc)0x00007fffcf97a000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf97a000,
                         .vm_end = (app_pc)0x00007fffcf97c000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_509 = {

    .iters = results_509,
    .iters_count = 55,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/clients/bin64/drcachesim",
    .want_return = 4,
    .want_start = (app_pc)0x000055f0756d2000,
    .want_end = (app_pc)0x000055f075aa9000,

};

fake_memquery_result results_510[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464e5000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f0756d2000,
                         .vm_end = (app_pc)0x000055f075872000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000003d7000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a71000,
                         .vm_end = (app_pc)0x000055f075a97000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a97000,
                         .vm_end = (app_pc)0x000055f075a98000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/drcachesim" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f075a98000,
                         .vm_end = (app_pc)0x000055f075aa9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055f07728f000,
                         .vm_end = (app_pc)0x000055f0776e1000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deac7f000,
                         .vm_end = (app_pc)0x00007f2deae14000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deae14000,
                         .vm_end = (app_pc)0x00007f2deb014000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb014000,
                         .vm_end = (app_pc)0x00007f2deb018000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb018000,
                         .vm_end = (app_pc)0x00007f2deb01a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb01a000,
                         .vm_end = (app_pc)0x00007f2deb01e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb01e000,
                         .vm_end = (app_pc)0x00007f2deb035000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb035000,
                         .vm_end = (app_pc)0x00007f2deb234000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb234000,
                         .vm_end = (app_pc)0x00007f2deb235000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb235000,
                         .vm_end = (app_pc)0x00007f2deb236000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb236000,
                         .vm_end = (app_pc)0x00007f2deb339000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb339000,
                         .vm_end = (app_pc)0x00007f2deb538000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb538000,
                         .vm_end = (app_pc)0x00007f2deb539000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb539000,
                         .vm_end = (app_pc)0x00007f2deb53a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb53a000,
                         .vm_end = (app_pc)0x00007f2deb6b0000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb6b0000,
                         .vm_end = (app_pc)0x00007f2deb8b0000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8b0000,
                         .vm_end = (app_pc)0x00007f2deb8ba000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8ba000,
                         .vm_end = (app_pc)0x00007f2deb8bc000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8bc000,
                         .vm_end = (app_pc)0x00007f2deb8bf000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2deb8bf000,
                         .vm_end = (app_pc)0x00007f2debc55000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debc55000,
                         .vm_end = (app_pc)0x00007f2debe54000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe54000,
                         .vm_end = (app_pc)0x00007f2debe7c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe7c000,
                         .vm_end = (app_pc)0x00007f2debe9a000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debe9a000,
                         .vm_end = (app_pc)0x00007f2debed1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debed1000,
                         .vm_end = (app_pc)0x00007f2debee9000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2debee9000,
                         .vm_end = (app_pc)0x00007f2dec0e8000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0e8000,
                         .vm_end = (app_pc)0x00007f2dec0e9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0e9000,
                         .vm_end = (app_pc)0x00007f2dec0ea000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0ea000,
                         .vm_end = (app_pc)0x00007f2dec0ee000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec0ee000,
                         .vm_end = (app_pc)0x00007f2dec107000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021a000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec107000,
                         .vm_end = (app_pc)0x00007f2dec306000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec306000,
                         .vm_end = (app_pc)0x00007f2dec307000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec307000,
                         .vm_end = (app_pc)0x00007f2dec308000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libz.so.1.2.8" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec308000,
                         .vm_end = (app_pc)0x00007f2dec32b000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec3f8000,
                         .vm_end = (app_pc)0x00007f2dec4fe000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec528000,
                         .vm_end = (app_pc)0x00007f2dec52b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52b000,
                         .vm_end = (app_pc)0x00007f2dec52c000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52c000,
                         .vm_end = (app_pc)0x00007f2dec52d000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f2dec52d000,
                         .vm_end = (app_pc)0x00007f2dec52e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf847000,
                         .vm_end = (app_pc)0x00007fffcf869000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf977000,
                         .vm_end = (app_pc)0x00007fffcf97a000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007fffcf97a000,
                         .vm_end = (app_pc)0x00007fffcf97c000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

memquery_library_bounds_test test_510 = {

    .iters = results_510,
    .iters_count = 55,
    .in_start = (app_pc)0x00007f2deb8bf000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f2deb8bf000,
    .want_end = (app_pc)0x00007f2debed1000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",

};

fake_memquery_result results_511[100] = {
    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d654000,
                         .vm_end = (app_pc)0x000055ea3d655000,
                         .prot = 5,
                         .comment = "" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x00000000006b2000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d655000,
                         .vm_end = (app_pc)0x000055ea3d658000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_maps" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d658000,
                         .vm_end = (app_pc)0x000055ea3d659000,
                         .prot = 5,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d659000,
                         .vm_end = (app_pc)0x000055ea3d65c000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_maps" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d65c000,
                         .vm_end = (app_pc)0x000055ea3d65d000,
                         .prot = 5,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3d65d000,
                         .vm_end = (app_pc)0x000055ea3da83000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_maps" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3dc83000,
                         .vm_end = (app_pc)0x000055ea3dcac000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_maps" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3dcac000,
                         .vm_end = (app_pc)0x000055ea3dccb000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "clients/bin64/tool.drcacheoff.burst_maps" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3dccb000,
                         .vm_end = (app_pc)0x000055ea3dd06000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x000055ea3e7f4000,
                         .vm_end = (app_pc)0x000055ea3e826000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238889000,
                         .vm_end = (app_pc)0x00007f5238a1e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238a1e000,
                         .vm_end = (app_pc)0x00007f5238c1e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238c1e000,
                         .vm_end = (app_pc)0x00007f5238c22000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238c22000,
                         .vm_end = (app_pc)0x00007f5238c24000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238c24000,
                         .vm_end = (app_pc)0x00007f5238c28000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238c28000,
                         .vm_end = (app_pc)0x00007f5238c3f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000218000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238c3f000,
                         .vm_end = (app_pc)0x00007f5238e3e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238e3e000,
                         .vm_end = (app_pc)0x00007f5238e3f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238e3f000,
                         .vm_end = (app_pc)0x00007f5238e40000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libgcc_s.so.1" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238e40000,
                         .vm_end = (app_pc)0x00007f5238f43000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5238f43000,
                         .vm_end = (app_pc)0x00007f5239142000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5239142000,
                         .vm_end = (app_pc)0x00007f5239143000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5239143000,
                         .vm_end = (app_pc)0x00007f5239144000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f5239144000,
                         .vm_end = (app_pc)0x00007f52392ba000,
                         .prot = 5,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000385000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52392ba000,
                         .vm_end = (app_pc)0x00007f52394ba000,
                         .prot = 0,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52394ba000,
                         .vm_end = (app_pc)0x00007f52394c4000,
                         .prot = 1,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52394c4000,
                         .vm_end = (app_pc)0x00007f52394c6000,
                         .prot = 3,
                         .comment = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.25" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52394c6000,
                         .vm_end = (app_pc)0x00007f52394c9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52394c9000,
                         .vm_end = (app_pc)0x00007f52394cc000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52394cc000,
                         .vm_end = (app_pc)0x00007f52396cb000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52396cb000,
                         .vm_end = (app_pc)0x00007f52396cc000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52396cc000,
                         .vm_end = (app_pc)0x00007f52396cd000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52396cd000,
                         .vm_end = (app_pc)0x00007f52396f0000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52398be000,
                         .vm_end = (app_pc)0x00007f52398c3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52398ed000,
                         .vm_end = (app_pc)0x00007f52398f0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52398f0000,
                         .vm_end = (app_pc)0x00007f52398f1000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52398f1000,
                         .vm_end = (app_pc)0x00007f52398f2000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007f52398f2000,
                         .vm_end = (app_pc)0x00007f52398f3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc56248000,
                         .vm_end = (app_pc)0x00007ffc5626a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc563a5000,
                         .vm_end = (app_pc)0x00007ffc563a8000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,

    },

    {

        .iter_result = { .vm_start = (app_pc)0x00007ffc563a8000,
                         .vm_end = (app_pc)0x00007ffc563aa000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,

    },

};

fake_memquery_result results_562[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000400000,
                         .vm_end = (app_pc)0x0000000000401000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/tool.sse41" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000400000,
        .mod_end = (app_pc)0x0000000000401000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000401000,
                         .vm_end = (app_pc)0x0000000000402000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000402000,
                         .vm_end = (app_pc)0x0000000000403000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071000000,
                         .vm_end = (app_pc)0x0000000071396000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071396000,
                         .vm_end = (app_pc)0x0000000071595000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071595000,
                         .vm_end = (app_pc)0x00000000715db000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000715db000,
                         .vm_end = (app_pc)0x0000000071612000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000071612000,
                         .vm_end = (app_pc)0x0000000071613000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f67a6c0b000,
                         .vm_end = (app_pc)0x00007f67a6c0e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f67a6c0e000,
                         .vm_end = (app_pc)0x00007f67a6c10000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffddfb54000,
                         .vm_end = (app_pc)0x00007ffddfb76000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },

};

memquery_library_bounds_test test_562 = {
    .iters = results_562,
    .iters_count = 11,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/tool.sse41",
    .want_return = 1,
    .want_start = (app_pc)0x0000000000400000,
    .want_end = (app_pc)0x0000000000401000,
};

fake_memquery_result results_563[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bd6fb000,
                         .vm_end = (app_pc)0x00007f02bda91000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bdc90000,
                         .vm_end = (app_pc)0x00007f02bdcd6000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bdcd6000,
                         .vm_end = (app_pc)0x00007f02bdd0d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36b10000,
                         .vm_end = (app_pc)0x00007ffc36b32000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36bf3000,
                         .vm_end = (app_pc)0x00007ffc36bf6000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36bf6000,
                         .vm_end = (app_pc)0x00007ffc36bf8000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_563 = {
    .iters = results_563,
    .iters_count = 6,
    .in_start = (app_pc)0x00007f02bd6fb000,
    .want_return = 3,
    .want_start = (app_pc)0x00007f02bd6fb000,
    .want_end = (app_pc)0x00007f02bdd0d000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_564[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000400000,
                         .vm_end = (app_pc)0x0000000000401000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/tool.sse42" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000400000,
        .mod_end = (app_pc)0x0000000000401000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000401000,
                         .vm_end = (app_pc)0x0000000000402000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000000000402000,
                         .vm_end = (app_pc)0x0000000000403000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bd6fb000,
                         .vm_end = (app_pc)0x00007f02bda91000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bda91000,
                         .vm_end = (app_pc)0x00007f02bdc90000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bdc90000,
                         .vm_end = (app_pc)0x00007f02bdcd6000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f02bdcd6000,
                         .vm_end = (app_pc)0x00007f02bdd0d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36b10000,
                         .vm_end = (app_pc)0x00007ffc36b32000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36bf3000,
                         .vm_end = (app_pc)0x00007ffc36bf6000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc36bf6000,
                         .vm_end = (app_pc)0x00007ffc36bf8000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_564 = {
    .iters = results_564,
    .iters_count = 10,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/tool.sse42",
    .want_return = 1,
    .want_start = (app_pc)0x0000000000400000,
    .want_end = (app_pc)0x0000000000401000,
};

fake_memquery_result results_565[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1b04000,
                         .vm_end = (app_pc)0x00007fbff1e9a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff2099000,
                         .vm_end = (app_pc)0x00007fbff20df000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff20df000,
                         .vm_end = (app_pc)0x00007fbff2116000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe83388000,
                         .vm_end = (app_pc)0x00007ffe833aa000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe833dd000,
                         .vm_end = (app_pc)0x00007ffe833e0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe833e0000,
                         .vm_end = (app_pc)0x00007ffe833e2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_565 = {
    .iters = results_565,
    .iters_count = 6,
    .in_start = (app_pc)0x00007fbff1b04000,
    .want_return = 3,
    .want_start = (app_pc)0x00007fbff1b04000,
    .want_end = (app_pc)0x00007fbff2116000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_566[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbfed8fe000,
                         .vm_end = (app_pc)0x00007fbfed902000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/tool.cpuid" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000205000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbfed902000,
                         .vm_end = (app_pc)0x00007fbfedb01000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbfedb01000,
                         .vm_end = (app_pc)0x00007fbfedb03000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/tool.cpuid" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbfedb03000,
                         .vm_end = (app_pc)0x00007fbfedb04000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbfedb04000,
                         .vm_end = (app_pc)0x00007fbfedb05000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff18dd000,
                         .vm_end = (app_pc)0x00007fbff1900000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1900000,
                         .vm_end = (app_pc)0x00007fbff1b00000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1b00000,
                         .vm_end = (app_pc)0x00007fbff1b02000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1b02000,
                         .vm_end = (app_pc)0x00007fbff1b03000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1b03000,
                         .vm_end = (app_pc)0x00007fbff1b04000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1b04000,
                         .vm_end = (app_pc)0x00007fbff1e9a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff1e9a000,
                         .vm_end = (app_pc)0x00007fbff2099000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff2099000,
                         .vm_end = (app_pc)0x00007fbff20df000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fbff20df000,
                         .vm_end = (app_pc)0x00007fbff2116000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe83388000,
                         .vm_end = (app_pc)0x00007ffe833aa000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe833dd000,
                         .vm_end = (app_pc)0x00007ffe833e0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe833e0000,
                         .vm_end = (app_pc)0x00007ffe833e2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_566 = {
    .iters = results_566,
    .iters_count = 17,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/tool.cpuid",
    .want_return = 3,
    .want_start = (app_pc)0x00007fbfed8fe000,
    .want_end = (app_pc)0x00007fbfedb03000,
};

fake_memquery_result results_567[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007f3d7335c000,
                         .vm_end = (app_pc)0x00007f3d736f2000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f3d738f1000,
                         .vm_end = (app_pc)0x00007f3d73937000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f3d73937000,
                         .vm_end = (app_pc)0x00007f3d7396e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffd98a7a000,
                         .vm_end = (app_pc)0x00007ffd98a9c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffd98bf1000,
                         .vm_end = (app_pc)0x00007ffd98bf4000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffd98bf4000,
                         .vm_end = (app_pc)0x00007ffd98bf6000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

fake_memquery_result results_577[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b918d000,
                         .vm_end = (app_pc)0x00007ff6b9523000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9722000,
                         .vm_end = (app_pc)0x00007ff6b9768000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9768000,
                         .vm_end = (app_pc)0x00007ff6b979f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa7567000,
                         .vm_end = (app_pc)0x00007fffa7589000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa75c2000,
                         .vm_end = (app_pc)0x00007fffa75c5000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa75c5000,
                         .vm_end = (app_pc)0x00007fffa75c7000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_577 = {
    .iters = results_577,
    .iters_count = 6,
    .in_start = (app_pc)0x00007ff6b918d000,
    .want_return = 3,
    .want_start = (app_pc)0x00007ff6b918d000,
    .want_end = (app_pc)0x00007ff6b979f000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_578[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b4f83000,
                         .vm_end = (app_pc)0x00007ff6b4f8b000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/security-common.selfmod" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000209000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b4f8b000,
                         .vm_end = (app_pc)0x00007ff6b518a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b518a000,
                         .vm_end = (app_pc)0x00007ff6b518c000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/security-common.selfmod" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b518c000,
                         .vm_end = (app_pc)0x00007ff6b518d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b518d000,
                         .vm_end = (app_pc)0x00007ff6b518e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b8f66000,
                         .vm_end = (app_pc)0x00007ff6b8f89000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b8f89000,
                         .vm_end = (app_pc)0x00007ff6b9189000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9189000,
                         .vm_end = (app_pc)0x00007ff6b918b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b918b000,
                         .vm_end = (app_pc)0x00007ff6b918c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b918c000,
                         .vm_end = (app_pc)0x00007ff6b918d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b918d000,
                         .vm_end = (app_pc)0x00007ff6b9523000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9523000,
                         .vm_end = (app_pc)0x00007ff6b9722000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9722000,
                         .vm_end = (app_pc)0x00007ff6b9768000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ff6b9768000,
                         .vm_end = (app_pc)0x00007ff6b979f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa7567000,
                         .vm_end = (app_pc)0x00007fffa7589000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa75c2000,
                         .vm_end = (app_pc)0x00007fffa75c5000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffa75c5000,
                         .vm_end = (app_pc)0x00007fffa75c7000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_578 = {
    .iters = results_578,
    .iters_count = 17,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "security-common.selfmod",
    .want_return = 3,
    .want_start = (app_pc)0x00007ff6b4f83000,
    .want_end = (app_pc)0x00007ff6b518c000,
};

fake_memquery_result results_579[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddb2d000,
                         .vm_end = (app_pc)0x00007fb9ddec3000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9de0c2000,
                         .vm_end = (app_pc)0x00007fb9de108000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9de108000,
                         .vm_end = (app_pc)0x00007fb9de13f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931c8000,
                         .vm_end = (app_pc)0x00007ffc931ea000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931f8000,
                         .vm_end = (app_pc)0x00007ffc931fb000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931fb000,
                         .vm_end = (app_pc)0x00007ffc931fd000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_579 = {
    .iters = results_579,
    .iters_count = 6,
    .in_start = (app_pc)0x00007fb9ddb2d000,
    .want_return = 3,
    .want_start = (app_pc)0x00007fb9ddb2d000,
    .want_end = (app_pc)0x00007fb9de13f000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_580[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000002ff94718000,
                         .vm_end = (app_pc)0x000002ff94719000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/libclient.dr_options.dll.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000072000000,
        .mod_end = (app_pc)0x0000000072203000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000002ff94719000,
                         .vm_end = (app_pc)0x000002ff94919000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000002ff94919000,
                         .vm_end = (app_pc)0x000002ff9491b000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/libclient.dr_options.dll.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000002ff9491b000,
                         .vm_end = (app_pc)0x000002ff9491c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004718000,
                         .vm_end = (app_pc)0x0000030004719000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004719000,
                         .vm_end = (app_pc)0x000003000471f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000003000471f000,
                         .vm_end = (app_pc)0x0000030004721000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004721000,
                         .vm_end = (app_pc)0x000003000472b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000003000472b000,
                         .vm_end = (app_pc)0x0000030004731000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004731000,
                         .vm_end = (app_pc)0x0000030004732000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004732000,
                         .vm_end = (app_pc)0x0000030004739000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000030004739000,
                         .vm_end = (app_pc)0x000003000473a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000003000473a000,
                         .vm_end = (app_pc)0x0000030014718000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9d9923000,
                         .vm_end = (app_pc)0x00007fb9d992b000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/security-common.selfmod" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000209000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9d992b000,
                         .vm_end = (app_pc)0x00007fb9d9b2a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9d9b2a000,
                         .vm_end = (app_pc)0x00007fb9d9b2c000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/security-common.selfmod" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9d9b2c000,
                         .vm_end = (app_pc)0x00007fb9d9b2d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9d9b2d000,
                         .vm_end = (app_pc)0x00007fb9d9b2e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9dd906000,
                         .vm_end = (app_pc)0x00007fb9dd929000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9dd929000,
                         .vm_end = (app_pc)0x00007fb9ddb29000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddb29000,
                         .vm_end = (app_pc)0x00007fb9ddb2b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddb2b000,
                         .vm_end = (app_pc)0x00007fb9ddb2c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddb2c000,
                         .vm_end = (app_pc)0x00007fb9ddb2d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddb2d000,
                         .vm_end = (app_pc)0x00007fb9ddec3000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9ddec3000,
                         .vm_end = (app_pc)0x00007fb9de0c2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9de0c2000,
                         .vm_end = (app_pc)0x00007fb9de108000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb9de108000,
                         .vm_end = (app_pc)0x00007fb9de13f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931c8000,
                         .vm_end = (app_pc)0x00007ffc931ea000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931f8000,
                         .vm_end = (app_pc)0x00007ffc931fb000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc931fb000,
                         .vm_end = (app_pc)0x00007ffc931fd000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_580 = {
    .iters = results_580,
    .iters_count = 30,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "security-common.selfmod",
    .want_return = 3,
    .want_start = (app_pc)0x00007fb9d9923000,
    .want_end = (app_pc)0x00007fb9d9b2c000,
};

fake_memquery_result results_581[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00007f46de1b9000,
                         .vm_end = (app_pc)0x00007f46de54f000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f46de74e000,
                         .vm_end = (app_pc)0x00007f46de794000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f46de794000,
                         .vm_end = (app_pc)0x00007f46de7cb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffff85e9000,
                         .vm_end = (app_pc)0x00007ffff860b000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffff868b000,
                         .vm_end = (app_pc)0x00007ffff868e000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffff868e000,
                         .vm_end = (app_pc)0x00007ffff8690000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

fake_memquery_result results_583[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464e4000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65036000,
                         .vm_end = (app_pc)0x000055ba65160000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000032d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba6535f000,
                         .vm_end = (app_pc)0x000055ba65360000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65360000,
                         .vm_end = (app_pc)0x000055ba65361000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65361000,
                         .vm_end = (app_pc)0x000055ba65363000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662998b000,
                         .vm_end = (app_pc)0x00007f6629b20000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629b20000,
                         .vm_end = (app_pc)0x00007f6629d20000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d20000,
                         .vm_end = (app_pc)0x00007f6629d24000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d24000,
                         .vm_end = (app_pc)0x00007f6629d26000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d26000,
                         .vm_end = (app_pc)0x00007f6629d2a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d2a000,
                         .vm_end = (app_pc)0x00007f662a0c0000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a0c0000,
                         .vm_end = (app_pc)0x00007f662a2bf000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a2bf000,
                         .vm_end = (app_pc)0x00007f662a2e7000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a2e7000,
                         .vm_end = (app_pc)0x00007f662a305000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a305000,
                         .vm_end = (app_pc)0x00007f662a33c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a33c000,
                         .vm_end = (app_pc)0x00007f662a33f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a33f000,
                         .vm_end = (app_pc)0x00007f662a53e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a53e000,
                         .vm_end = (app_pc)0x00007f662a53f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a53f000,
                         .vm_end = (app_pc)0x00007f662a540000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a540000,
                         .vm_end = (app_pc)0x00007f662a643000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a643000,
                         .vm_end = (app_pc)0x00007f662a842000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a842000,
                         .vm_end = (app_pc)0x00007f662a843000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a843000,
                         .vm_end = (app_pc)0x00007f662a844000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a844000,
                         .vm_end = (app_pc)0x00007f662a867000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa38000,
                         .vm_end = (app_pc)0x00007f662aa3a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa64000,
                         .vm_end = (app_pc)0x00007f662aa67000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa67000,
                         .vm_end = (app_pc)0x00007f662aa68000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa68000,
                         .vm_end = (app_pc)0x00007f662aa69000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa69000,
                         .vm_end = (app_pc)0x00007f662aa6a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2b93000,
                         .vm_end = (app_pc)0x00007fffb2bb5000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2bc0000,
                         .vm_end = (app_pc)0x00007fffb2bc3000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2bc3000,
                         .vm_end = (app_pc)0x00007fffb2bc5000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_583 = {
    .iters = results_583,
    .iters_count = 40,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/api.ir",
    .want_return = 4,
    .want_start = (app_pc)0x000055ba65036000,
    .want_end = (app_pc)0x000055ba65363000,
};

fake_memquery_result results_584[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d2000,
                         .vm_end = (app_pc)0x00000000464d3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d3000,
                         .vm_end = (app_pc)0x00000000464d9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464d9000,
                         .vm_end = (app_pc)0x00000000464db000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464db000,
                         .vm_end = (app_pc)0x00000000464e5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464e5000,
                         .vm_end = (app_pc)0x00000000464eb000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464eb000,
                         .vm_end = (app_pc)0x00000000464ec000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464ec000,
                         .vm_end = (app_pc)0x00000000464f3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f3000,
                         .vm_end = (app_pc)0x00000000464f4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00000000464f4000,
                         .vm_end = (app_pc)0x00000000564d2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65036000,
                         .vm_end = (app_pc)0x000055ba65160000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000032d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba6535f000,
                         .vm_end = (app_pc)0x000055ba65360000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65360000,
                         .vm_end = (app_pc)0x000055ba65361000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ir" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055ba65361000,
                         .vm_end = (app_pc)0x000055ba65363000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662998b000,
                         .vm_end = (app_pc)0x00007f6629b20000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629b20000,
                         .vm_end = (app_pc)0x00007f6629d20000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d20000,
                         .vm_end = (app_pc)0x00007f6629d24000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d24000,
                         .vm_end = (app_pc)0x00007f6629d26000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d26000,
                         .vm_end = (app_pc)0x00007f6629d2a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6629d2a000,
                         .vm_end = (app_pc)0x00007f662a0c0000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a0c0000,
                         .vm_end = (app_pc)0x00007f662a2bf000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a2bf000,
                         .vm_end = (app_pc)0x00007f662a2e7000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a2e7000,
                         .vm_end = (app_pc)0x00007f662a305000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a305000,
                         .vm_end = (app_pc)0x00007f662a33c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a33c000,
                         .vm_end = (app_pc)0x00007f662a33f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a33f000,
                         .vm_end = (app_pc)0x00007f662a53e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a53e000,
                         .vm_end = (app_pc)0x00007f662a53f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a53f000,
                         .vm_end = (app_pc)0x00007f662a540000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a540000,
                         .vm_end = (app_pc)0x00007f662a643000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a643000,
                         .vm_end = (app_pc)0x00007f662a842000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a842000,
                         .vm_end = (app_pc)0x00007f662a843000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a843000,
                         .vm_end = (app_pc)0x00007f662a844000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662a844000,
                         .vm_end = (app_pc)0x00007f662a867000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa38000,
                         .vm_end = (app_pc)0x00007f662aa3a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa64000,
                         .vm_end = (app_pc)0x00007f662aa67000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa67000,
                         .vm_end = (app_pc)0x00007f662aa68000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa68000,
                         .vm_end = (app_pc)0x00007f662aa69000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f662aa69000,
                         .vm_end = (app_pc)0x00007f662aa6a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2b93000,
                         .vm_end = (app_pc)0x00007fffb2bb5000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2bc0000,
                         .vm_end = (app_pc)0x00007fffb2bc3000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb2bc3000,
                         .vm_end = (app_pc)0x00007fffb2bc5000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_584 = {
    .iters = results_584,
    .iters_count = 40,
    .in_start = (app_pc)0x00007f6629d2a000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f6629d2a000,
    .want_end = (app_pc)0x00007f662a33c000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_585[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377be9000,
                         .vm_end = (app_pc)0x000055a377bee000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000206000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377ded000,
                         .vm_end = (app_pc)0x000055a377dee000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377dee000,
                         .vm_end = (app_pc)0x000055a377def000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a379d0a000,
                         .vm_end = (app_pc)0x000055a379d2b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62244a7000,
                         .vm_end = (app_pc)0x00007f62244a8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62244a8000,
                         .vm_end = (app_pc)0x00007f6224ca8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6224ca8000,
                         .vm_end = (app_pc)0x00007f6224ca9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6224ca9000,
                         .vm_end = (app_pc)0x00007f62254a9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62254a9000,
                         .vm_end = (app_pc)0x00007f62254aa000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62254aa000,
                         .vm_end = (app_pc)0x00007f6225caa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6225caa000,
                         .vm_end = (app_pc)0x00007f6225cab000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6225cab000,
                         .vm_end = (app_pc)0x00007f62264ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62264ab000,
                         .vm_end = (app_pc)0x00007f62264ac000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62264ac000,
                         .vm_end = (app_pc)0x00007f6226cac000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6226cac000,
                         .vm_end = (app_pc)0x00007f6226cad000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6226cad000,
                         .vm_end = (app_pc)0x00007f62274ad000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62274ad000,
                         .vm_end = (app_pc)0x00007f62274ae000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62274ae000,
                         .vm_end = (app_pc)0x00007f6227cae000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6227cae000,
                         .vm_end = (app_pc)0x00007f6227caf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6227caf000,
                         .vm_end = (app_pc)0x00007f62284af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62284af000,
                         .vm_end = (app_pc)0x00007f62284b0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62284b0000,
                         .vm_end = (app_pc)0x00007f6228cb0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6228cb0000,
                         .vm_end = (app_pc)0x00007f6228cb1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6228cb1000,
                         .vm_end = (app_pc)0x00007f62294b1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62294b1000,
                         .vm_end = (app_pc)0x00007f6229646000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229646000,
                         .vm_end = (app_pc)0x00007f6229846000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229846000,
                         .vm_end = (app_pc)0x00007f622984a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622984a000,
                         .vm_end = (app_pc)0x00007f622984c000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622984c000,
                         .vm_end = (app_pc)0x00007f6229850000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229850000,
                         .vm_end = (app_pc)0x00007f6229868000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229868000,
                         .vm_end = (app_pc)0x00007f6229a67000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a67000,
                         .vm_end = (app_pc)0x00007f6229a68000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a68000,
                         .vm_end = (app_pc)0x00007f6229a69000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a69000,
                         .vm_end = (app_pc)0x00007f6229a6d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a6d000,
                         .vm_end = (app_pc)0x00007f6229e03000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229e03000,
                         .vm_end = (app_pc)0x00007f622a002000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a002000,
                         .vm_end = (app_pc)0x00007f622a02a000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a02a000,
                         .vm_end = (app_pc)0x00007f622a048000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a048000,
                         .vm_end = (app_pc)0x00007f622a07f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a07f000,
                         .vm_end = (app_pc)0x00007f622a082000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a082000,
                         .vm_end = (app_pc)0x00007f622a281000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a281000,
                         .vm_end = (app_pc)0x00007f622a282000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a282000,
                         .vm_end = (app_pc)0x00007f622a283000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a283000,
                         .vm_end = (app_pc)0x00007f622a386000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a386000,
                         .vm_end = (app_pc)0x00007f622a585000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a585000,
                         .vm_end = (app_pc)0x00007f622a586000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a586000,
                         .vm_end = (app_pc)0x00007f622a587000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a587000,
                         .vm_end = (app_pc)0x00007f622a5aa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a77b000,
                         .vm_end = (app_pc)0x00007f622a77d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7a7000,
                         .vm_end = (app_pc)0x00007f622a7aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7aa000,
                         .vm_end = (app_pc)0x00007f622a7ab000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7ab000,
                         .vm_end = (app_pc)0x00007f622a7ac000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7ac000,
                         .vm_end = (app_pc)0x00007f622a7ad000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce00f6000,
                         .vm_end = (app_pc)0x00007ffce0118000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce01b0000,
                         .vm_end = (app_pc)0x00007ffce01b3000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce01b3000,
                         .vm_end = (app_pc)0x00007ffce01b5000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_585 = {
    .iters = results_585,
    .iters_count = 56,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/api.startstop",
    .want_return = 3,
    .want_start = (app_pc)0x000055a377be9000,
    .want_end = (app_pc)0x000055a377def000,
};

fake_memquery_result results_586[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367be8000,
                         .vm_end = (app_pc)0x000055a367be9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367be9000,
                         .vm_end = (app_pc)0x000055a367bef000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367bef000,
                         .vm_end = (app_pc)0x000055a367bf1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367bf1000,
                         .vm_end = (app_pc)0x000055a367bfb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367bfb000,
                         .vm_end = (app_pc)0x000055a367c01000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367c01000,
                         .vm_end = (app_pc)0x000055a367c02000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367c02000,
                         .vm_end = (app_pc)0x000055a367c09000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367c09000,
                         .vm_end = (app_pc)0x000055a367c0a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a367c0a000,
                         .vm_end = (app_pc)0x000055a377be9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377be9000,
                         .vm_end = (app_pc)0x000055a377bee000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000206000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377ded000,
                         .vm_end = (app_pc)0x000055a377dee000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a377dee000,
                         .vm_end = (app_pc)0x000055a377def000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055a379d0a000,
                         .vm_end = (app_pc)0x000055a379d2b000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62244a7000,
                         .vm_end = (app_pc)0x00007f62244a8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62244a8000,
                         .vm_end = (app_pc)0x00007f6224ca8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6224ca8000,
                         .vm_end = (app_pc)0x00007f6224ca9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6224ca9000,
                         .vm_end = (app_pc)0x00007f62254a9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62254a9000,
                         .vm_end = (app_pc)0x00007f62254aa000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62254aa000,
                         .vm_end = (app_pc)0x00007f6225caa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6225caa000,
                         .vm_end = (app_pc)0x00007f6225cab000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6225cab000,
                         .vm_end = (app_pc)0x00007f62264ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62264ab000,
                         .vm_end = (app_pc)0x00007f62264ac000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62264ac000,
                         .vm_end = (app_pc)0x00007f6226cac000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6226cac000,
                         .vm_end = (app_pc)0x00007f6226cad000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6226cad000,
                         .vm_end = (app_pc)0x00007f62274ad000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62274ad000,
                         .vm_end = (app_pc)0x00007f62274ae000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62274ae000,
                         .vm_end = (app_pc)0x00007f6227cae000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6227cae000,
                         .vm_end = (app_pc)0x00007f6227caf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6227caf000,
                         .vm_end = (app_pc)0x00007f62284af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62284af000,
                         .vm_end = (app_pc)0x00007f62284b0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62284b0000,
                         .vm_end = (app_pc)0x00007f6228cb0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6228cb0000,
                         .vm_end = (app_pc)0x00007f6228cb1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6228cb1000,
                         .vm_end = (app_pc)0x00007f62294b1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f62294b1000,
                         .vm_end = (app_pc)0x00007f6229646000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229646000,
                         .vm_end = (app_pc)0x00007f6229846000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229846000,
                         .vm_end = (app_pc)0x00007f622984a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622984a000,
                         .vm_end = (app_pc)0x00007f622984c000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622984c000,
                         .vm_end = (app_pc)0x00007f6229850000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229850000,
                         .vm_end = (app_pc)0x00007f6229868000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229868000,
                         .vm_end = (app_pc)0x00007f6229a67000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a67000,
                         .vm_end = (app_pc)0x00007f6229a68000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a68000,
                         .vm_end = (app_pc)0x00007f6229a69000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a69000,
                         .vm_end = (app_pc)0x00007f6229a6d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229a6d000,
                         .vm_end = (app_pc)0x00007f6229e03000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6229e03000,
                         .vm_end = (app_pc)0x00007f622a002000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a002000,
                         .vm_end = (app_pc)0x00007f622a02a000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a02a000,
                         .vm_end = (app_pc)0x00007f622a048000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a048000,
                         .vm_end = (app_pc)0x00007f622a07f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a07f000,
                         .vm_end = (app_pc)0x00007f622a082000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a082000,
                         .vm_end = (app_pc)0x00007f622a281000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a281000,
                         .vm_end = (app_pc)0x00007f622a282000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a282000,
                         .vm_end = (app_pc)0x00007f622a283000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a283000,
                         .vm_end = (app_pc)0x00007f622a386000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a386000,
                         .vm_end = (app_pc)0x00007f622a585000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a585000,
                         .vm_end = (app_pc)0x00007f622a586000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a586000,
                         .vm_end = (app_pc)0x00007f622a587000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a587000,
                         .vm_end = (app_pc)0x00007f622a5aa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a77b000,
                         .vm_end = (app_pc)0x00007f622a77d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7a7000,
                         .vm_end = (app_pc)0x00007f622a7aa000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7aa000,
                         .vm_end = (app_pc)0x00007f622a7ab000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7ab000,
                         .vm_end = (app_pc)0x00007f622a7ac000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f622a7ac000,
                         .vm_end = (app_pc)0x00007f622a7ad000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce00f6000,
                         .vm_end = (app_pc)0x00007ffce0118000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce01b0000,
                         .vm_end = (app_pc)0x00007ffce01b3000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffce01b3000,
                         .vm_end = (app_pc)0x00007ffce01b5000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_586 = {
    .iters = results_586,
    .iters_count = 65,
    .in_start = (app_pc)0x00007f6229a6d000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f6229a6d000,
    .want_end = (app_pc)0x00007f622a07f000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_587[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79515b000,
                         .vm_end = (app_pc)0x000055c795160000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000206000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79535f000,
                         .vm_end = (app_pc)0x000055c795360000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c795360000,
                         .vm_end = (app_pc)0x000055c795361000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79708d000,
                         .vm_end = (app_pc)0x000055c7970ae000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f63f9f000,
                         .vm_end = (app_pc)0x00007f4f63fa0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f63fa0000,
                         .vm_end = (app_pc)0x00007f4f647a0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f647a0000,
                         .vm_end = (app_pc)0x00007f4f647a1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f647a1000,
                         .vm_end = (app_pc)0x00007f4f64fa1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f64fa1000,
                         .vm_end = (app_pc)0x00007f4f64fa2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f64fa2000,
                         .vm_end = (app_pc)0x00007f4f657a2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f657a2000,
                         .vm_end = (app_pc)0x00007f4f657a3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f657a3000,
                         .vm_end = (app_pc)0x00007f4f65fa3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f65fa3000,
                         .vm_end = (app_pc)0x00007f4f65fa4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f65fa4000,
                         .vm_end = (app_pc)0x00007f4f667a4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f667a4000,
                         .vm_end = (app_pc)0x00007f4f667a5000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f667a5000,
                         .vm_end = (app_pc)0x00007f4f66fa5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f66fa5000,
                         .vm_end = (app_pc)0x00007f4f66fa6000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f66fa6000,
                         .vm_end = (app_pc)0x00007f4f677a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f677a6000,
                         .vm_end = (app_pc)0x00007f4f677a7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f677a7000,
                         .vm_end = (app_pc)0x00007f4f67fa7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f67fa7000,
                         .vm_end = (app_pc)0x00007f4f67fa8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f67fa8000,
                         .vm_end = (app_pc)0x00007f4f687a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f687a8000,
                         .vm_end = (app_pc)0x00007f4f687a9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f687a9000,
                         .vm_end = (app_pc)0x00007f4f68fa9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f68fa9000,
                         .vm_end = (app_pc)0x00007f4f6913e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6913e000,
                         .vm_end = (app_pc)0x00007f4f6933e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6933e000,
                         .vm_end = (app_pc)0x00007f4f69342000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69342000,
                         .vm_end = (app_pc)0x00007f4f69344000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69344000,
                         .vm_end = (app_pc)0x00007f4f69348000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69348000,
                         .vm_end = (app_pc)0x00007f4f69360000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69360000,
                         .vm_end = (app_pc)0x00007f4f6955f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6955f000,
                         .vm_end = (app_pc)0x00007f4f69560000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69560000,
                         .vm_end = (app_pc)0x00007f4f69561000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69561000,
                         .vm_end = (app_pc)0x00007f4f69565000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69565000,
                         .vm_end = (app_pc)0x00007f4f698fb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f698fb000,
                         .vm_end = (app_pc)0x00007f4f69afa000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69afa000,
                         .vm_end = (app_pc)0x00007f4f69b22000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b22000,
                         .vm_end = (app_pc)0x00007f4f69b40000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b40000,
                         .vm_end = (app_pc)0x00007f4f69b77000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b77000,
                         .vm_end = (app_pc)0x00007f4f69b7a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b7a000,
                         .vm_end = (app_pc)0x00007f4f69d79000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d79000,
                         .vm_end = (app_pc)0x00007f4f69d7a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d7a000,
                         .vm_end = (app_pc)0x00007f4f69d7b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d7b000,
                         .vm_end = (app_pc)0x00007f4f69e7e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69e7e000,
                         .vm_end = (app_pc)0x00007f4f6a07d000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07d000,
                         .vm_end = (app_pc)0x00007f4f6a07e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07e000,
                         .vm_end = (app_pc)0x00007f4f6a07f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07f000,
                         .vm_end = (app_pc)0x00007f4f6a0a2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a273000,
                         .vm_end = (app_pc)0x00007f4f6a275000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a29f000,
                         .vm_end = (app_pc)0x00007f4f6a2a2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a2000,
                         .vm_end = (app_pc)0x00007f4f6a2a3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a3000,
                         .vm_end = (app_pc)0x00007f4f6a2a4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a4000,
                         .vm_end = (app_pc)0x00007f4f6a2a5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc616b000,
                         .vm_end = (app_pc)0x00007fffc618d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc61df000,
                         .vm_end = (app_pc)0x00007fffc61e2000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc61e2000,
                         .vm_end = (app_pc)0x00007fffc61e4000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_587 = {
    .iters = results_587,
    .iters_count = 56,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/api.detach",
    .want_return = 3,
    .want_start = (app_pc)0x000055c79515b000,
    .want_end = (app_pc)0x000055c795361000,
};

fake_memquery_result results_588[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055c78515a000,
                         .vm_end = (app_pc)0x000055c78515b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c78515b000,
                         .vm_end = (app_pc)0x000055c785161000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c785161000,
                         .vm_end = (app_pc)0x000055c785163000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c785163000,
                         .vm_end = (app_pc)0x000055c78516d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c78516d000,
                         .vm_end = (app_pc)0x000055c785173000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c785173000,
                         .vm_end = (app_pc)0x000055c785174000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c785174000,
                         .vm_end = (app_pc)0x000055c78517b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c78517b000,
                         .vm_end = (app_pc)0x000055c78517c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c78517c000,
                         .vm_end = (app_pc)0x000055c79515b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79515b000,
                         .vm_end = (app_pc)0x000055c795160000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000206000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79535f000,
                         .vm_end = (app_pc)0x000055c795360000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c795360000,
                         .vm_end = (app_pc)0x000055c795361000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055c79708d000,
                         .vm_end = (app_pc)0x000055c7970ae000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f63f9f000,
                         .vm_end = (app_pc)0x00007f4f63fa0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f63fa0000,
                         .vm_end = (app_pc)0x00007f4f647a0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f647a0000,
                         .vm_end = (app_pc)0x00007f4f647a1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f647a1000,
                         .vm_end = (app_pc)0x00007f4f64fa1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f64fa1000,
                         .vm_end = (app_pc)0x00007f4f64fa2000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f64fa2000,
                         .vm_end = (app_pc)0x00007f4f657a2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f657a2000,
                         .vm_end = (app_pc)0x00007f4f657a3000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f657a3000,
                         .vm_end = (app_pc)0x00007f4f65fa3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f65fa3000,
                         .vm_end = (app_pc)0x00007f4f65fa4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f65fa4000,
                         .vm_end = (app_pc)0x00007f4f667a4000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f667a4000,
                         .vm_end = (app_pc)0x00007f4f667a5000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f667a5000,
                         .vm_end = (app_pc)0x00007f4f66fa5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f66fa5000,
                         .vm_end = (app_pc)0x00007f4f66fa6000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f66fa6000,
                         .vm_end = (app_pc)0x00007f4f677a6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f677a6000,
                         .vm_end = (app_pc)0x00007f4f677a7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f677a7000,
                         .vm_end = (app_pc)0x00007f4f67fa7000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f67fa7000,
                         .vm_end = (app_pc)0x00007f4f67fa8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f67fa8000,
                         .vm_end = (app_pc)0x00007f4f687a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f687a8000,
                         .vm_end = (app_pc)0x00007f4f687a9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f687a9000,
                         .vm_end = (app_pc)0x00007f4f68fa9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f68fa9000,
                         .vm_end = (app_pc)0x00007f4f6913e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6913e000,
                         .vm_end = (app_pc)0x00007f4f6933e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6933e000,
                         .vm_end = (app_pc)0x00007f4f69342000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69342000,
                         .vm_end = (app_pc)0x00007f4f69344000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69344000,
                         .vm_end = (app_pc)0x00007f4f69348000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69348000,
                         .vm_end = (app_pc)0x00007f4f69360000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69360000,
                         .vm_end = (app_pc)0x00007f4f6955f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6955f000,
                         .vm_end = (app_pc)0x00007f4f69560000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69560000,
                         .vm_end = (app_pc)0x00007f4f69561000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69561000,
                         .vm_end = (app_pc)0x00007f4f69565000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69565000,
                         .vm_end = (app_pc)0x00007f4f698fb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f698fb000,
                         .vm_end = (app_pc)0x00007f4f69afa000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69afa000,
                         .vm_end = (app_pc)0x00007f4f69b22000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b22000,
                         .vm_end = (app_pc)0x00007f4f69b40000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b40000,
                         .vm_end = (app_pc)0x00007f4f69b77000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b77000,
                         .vm_end = (app_pc)0x00007f4f69b7a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69b7a000,
                         .vm_end = (app_pc)0x00007f4f69d79000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d79000,
                         .vm_end = (app_pc)0x00007f4f69d7a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d7a000,
                         .vm_end = (app_pc)0x00007f4f69d7b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69d7b000,
                         .vm_end = (app_pc)0x00007f4f69e7e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f69e7e000,
                         .vm_end = (app_pc)0x00007f4f6a07d000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07d000,
                         .vm_end = (app_pc)0x00007f4f6a07e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07e000,
                         .vm_end = (app_pc)0x00007f4f6a07f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a07f000,
                         .vm_end = (app_pc)0x00007f4f6a0a2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a273000,
                         .vm_end = (app_pc)0x00007f4f6a275000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a29f000,
                         .vm_end = (app_pc)0x00007f4f6a2a2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a2000,
                         .vm_end = (app_pc)0x00007f4f6a2a3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a3000,
                         .vm_end = (app_pc)0x00007f4f6a2a4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4f6a2a4000,
                         .vm_end = (app_pc)0x00007f4f6a2a5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc616b000,
                         .vm_end = (app_pc)0x00007fffc618d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc61df000,
                         .vm_end = (app_pc)0x00007fffc61e2000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffc61e2000,
                         .vm_end = (app_pc)0x00007fffc61e4000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_588 = {
    .iters = results_588,
    .iters_count = 65,
    .in_start = (app_pc)0x00007f4f69565000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f4f69565000,
    .want_end = (app_pc)0x00007f4f69b77000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_589[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb2b0000,
                         .vm_end = (app_pc)0x000055d0bb2b6000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000207000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb4b5000,
                         .vm_end = (app_pc)0x000055d0bb4b6000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb4b6000,
                         .vm_end = (app_pc)0x000055d0bb4b7000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bd149000,
                         .vm_end = (app_pc)0x000055d0bd16a000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906443000,
                         .vm_end = (app_pc)0x00007f4906444000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906444000,
                         .vm_end = (app_pc)0x00007f4906c44000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906c44000,
                         .vm_end = (app_pc)0x00007f4906dd9000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906dd9000,
                         .vm_end = (app_pc)0x00007f4906fd9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fd9000,
                         .vm_end = (app_pc)0x00007f4906fdd000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fdd000,
                         .vm_end = (app_pc)0x00007f4906fdf000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fdf000,
                         .vm_end = (app_pc)0x00007f4906fe3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fe3000,
                         .vm_end = (app_pc)0x00007f4906ffb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906ffb000,
                         .vm_end = (app_pc)0x00007f49071fa000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fa000,
                         .vm_end = (app_pc)0x00007f49071fb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fb000,
                         .vm_end = (app_pc)0x00007f49071fc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fc000,
                         .vm_end = (app_pc)0x00007f4907200000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907200000,
                         .vm_end = (app_pc)0x00007f4907596000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907596000,
                         .vm_end = (app_pc)0x00007f4907795000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907795000,
                         .vm_end = (app_pc)0x00007f49077bd000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49077bd000,
                         .vm_end = (app_pc)0x00007f49077db000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49077db000,
                         .vm_end = (app_pc)0x00007f4907812000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907812000,
                         .vm_end = (app_pc)0x00007f4907815000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907815000,
                         .vm_end = (app_pc)0x00007f4907a14000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a14000,
                         .vm_end = (app_pc)0x00007f4907a15000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a15000,
                         .vm_end = (app_pc)0x00007f4907a16000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a16000,
                         .vm_end = (app_pc)0x00007f4907b19000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907b19000,
                         .vm_end = (app_pc)0x00007f4907d18000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d18000,
                         .vm_end = (app_pc)0x00007f4907d19000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d19000,
                         .vm_end = (app_pc)0x00007f4907d1a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d1a000,
                         .vm_end = (app_pc)0x00007f4907d3d000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f0e000,
                         .vm_end = (app_pc)0x00007f4907f10000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3a000,
                         .vm_end = (app_pc)0x00007f4907f3d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3d000,
                         .vm_end = (app_pc)0x00007f4907f3e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3e000,
                         .vm_end = (app_pc)0x00007f4907f3f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3f000,
                         .vm_end = (app_pc)0x00007f4907f40000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd6ea000,
                         .vm_end = (app_pc)0x00007ffcdd70c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd7aa000,
                         .vm_end = (app_pc)0x00007ffcdd7ad000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd7ad000,
                         .vm_end = (app_pc)0x00007ffcdd7af000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_589 = {
    .iters = results_589,
    .iters_count = 38,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/api.detach_state",
    .want_return = 3,
    .want_start = (app_pc)0x000055d0bb2b0000,
    .want_end = (app_pc)0x000055d0bb4b7000,
};

fake_memquery_result results_590[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2af000,
                         .vm_end = (app_pc)0x000055d0ab2b0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2b0000,
                         .vm_end = (app_pc)0x000055d0ab2b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2b6000,
                         .vm_end = (app_pc)0x000055d0ab2b8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2b8000,
                         .vm_end = (app_pc)0x000055d0ab2c2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2c2000,
                         .vm_end = (app_pc)0x000055d0ab2c8000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2c8000,
                         .vm_end = (app_pc)0x000055d0ab2c9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2c9000,
                         .vm_end = (app_pc)0x000055d0ab2d0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2d0000,
                         .vm_end = (app_pc)0x000055d0ab2d1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0ab2d1000,
                         .vm_end = (app_pc)0x000055d0bb2b0000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb2b0000,
                         .vm_end = (app_pc)0x000055d0bb2b6000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000207000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb4b5000,
                         .vm_end = (app_pc)0x000055d0bb4b6000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bb4b6000,
                         .vm_end = (app_pc)0x000055d0bb4b7000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.detach_state" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d0bd149000,
                         .vm_end = (app_pc)0x000055d0bd16a000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906443000,
                         .vm_end = (app_pc)0x00007f4906444000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906444000,
                         .vm_end = (app_pc)0x00007f4906c44000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906c44000,
                         .vm_end = (app_pc)0x00007f4906dd9000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906dd9000,
                         .vm_end = (app_pc)0x00007f4906fd9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fd9000,
                         .vm_end = (app_pc)0x00007f4906fdd000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fdd000,
                         .vm_end = (app_pc)0x00007f4906fdf000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fdf000,
                         .vm_end = (app_pc)0x00007f4906fe3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906fe3000,
                         .vm_end = (app_pc)0x00007f4906ffb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4906ffb000,
                         .vm_end = (app_pc)0x00007f49071fa000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fa000,
                         .vm_end = (app_pc)0x00007f49071fb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fb000,
                         .vm_end = (app_pc)0x00007f49071fc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49071fc000,
                         .vm_end = (app_pc)0x00007f4907200000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907200000,
                         .vm_end = (app_pc)0x00007f4907596000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907596000,
                         .vm_end = (app_pc)0x00007f4907795000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907795000,
                         .vm_end = (app_pc)0x00007f49077bd000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49077bd000,
                         .vm_end = (app_pc)0x00007f49077db000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f49077db000,
                         .vm_end = (app_pc)0x00007f4907812000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907812000,
                         .vm_end = (app_pc)0x00007f4907815000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907815000,
                         .vm_end = (app_pc)0x00007f4907a14000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a14000,
                         .vm_end = (app_pc)0x00007f4907a15000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a15000,
                         .vm_end = (app_pc)0x00007f4907a16000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907a16000,
                         .vm_end = (app_pc)0x00007f4907b19000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907b19000,
                         .vm_end = (app_pc)0x00007f4907d18000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d18000,
                         .vm_end = (app_pc)0x00007f4907d19000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d19000,
                         .vm_end = (app_pc)0x00007f4907d1a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907d1a000,
                         .vm_end = (app_pc)0x00007f4907d3d000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f0e000,
                         .vm_end = (app_pc)0x00007f4907f10000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3a000,
                         .vm_end = (app_pc)0x00007f4907f3d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3d000,
                         .vm_end = (app_pc)0x00007f4907f3e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3e000,
                         .vm_end = (app_pc)0x00007f4907f3f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f4907f3f000,
                         .vm_end = (app_pc)0x00007f4907f40000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd6ea000,
                         .vm_end = (app_pc)0x00007ffcdd70c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd7aa000,
                         .vm_end = (app_pc)0x00007ffcdd7ad000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcdd7ad000,
                         .vm_end = (app_pc)0x00007ffcdd7af000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_590 = {
    .iters = results_590,
    .iters_count = 47,
    .in_start = (app_pc)0x00007f4907200000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f4907200000,
    .want_end = (app_pc)0x00007f4907812000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_592[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f8e8000,
                         .vm_end = (app_pc)0x000056060f8e9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f8e9000,
                         .vm_end = (app_pc)0x000056060f8ef000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f8ef000,
                         .vm_end = (app_pc)0x000056060f8f1000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f8f1000,
                         .vm_end = (app_pc)0x000056060f8fb000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f8fb000,
                         .vm_end = (app_pc)0x000056060f901000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f901000,
                         .vm_end = (app_pc)0x000056060f902000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f902000,
                         .vm_end = (app_pc)0x000056060f909000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f909000,
                         .vm_end = (app_pc)0x000056060f90a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056060f90a000,
                         .vm_end = (app_pc)0x000056061f8e9000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056061f8e9000,
                         .vm_end = (app_pc)0x000056061f8ef000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ibl-stress" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000207000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056061faee000,
                         .vm_end = (app_pc)0x000056061faef000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ibl-stress" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056061faef000,
                         .vm_end = (app_pc)0x000056061faf0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.ibl-stress" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000560620576000,
                         .vm_end = (app_pc)0x0000560620597000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27150c9000,
                         .vm_end = (app_pc)0x00007f271525e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f271525e000,
                         .vm_end = (app_pc)0x00007f271545e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f271545e000,
                         .vm_end = (app_pc)0x00007f2715462000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715462000,
                         .vm_end = (app_pc)0x00007f2715464000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715464000,
                         .vm_end = (app_pc)0x00007f2715468000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715468000,
                         .vm_end = (app_pc)0x00007f27157fe000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000071000000,
        .mod_end = (app_pc)0x0000000071612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27157fe000,
                         .vm_end = (app_pc)0x00007f27159fd000,
                         .prot = 0,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27159fd000,
                         .vm_end = (app_pc)0x00007f2715a25000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715a25000,
                         .vm_end = (app_pc)0x00007f2715a43000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "lib64/debug/libdynamorio.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715a43000,
                         .vm_end = (app_pc)0x00007f2715a7a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715a7a000,
                         .vm_end = (app_pc)0x00007f2715a92000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715a92000,
                         .vm_end = (app_pc)0x00007f2715c91000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715c91000,
                         .vm_end = (app_pc)0x00007f2715c92000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715c92000,
                         .vm_end = (app_pc)0x00007f2715c93000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715c93000,
                         .vm_end = (app_pc)0x00007f2715c97000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715c97000,
                         .vm_end = (app_pc)0x00007f2715c9a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715c9a000,
                         .vm_end = (app_pc)0x00007f2715e99000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715e99000,
                         .vm_end = (app_pc)0x00007f2715e9a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715e9a000,
                         .vm_end = (app_pc)0x00007f2715e9b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715e9b000,
                         .vm_end = (app_pc)0x00007f2715f9e000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2715f9e000,
                         .vm_end = (app_pc)0x00007f271619d000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f271619d000,
                         .vm_end = (app_pc)0x00007f271619e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f271619e000,
                         .vm_end = (app_pc)0x00007f271619f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f271619f000,
                         .vm_end = (app_pc)0x00007f27161c2000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f2716393000,
                         .vm_end = (app_pc)0x00007f2716395000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27163bf000,
                         .vm_end = (app_pc)0x00007f27163c2000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27163c2000,
                         .vm_end = (app_pc)0x00007f27163c3000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27163c3000,
                         .vm_end = (app_pc)0x00007f27163c4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f27163c4000,
                         .vm_end = (app_pc)0x00007f27163c5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc4c0d0000,
                         .vm_end = (app_pc)0x00007ffc4c0f2000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc4c1e0000,
                         .vm_end = (app_pc)0x00007ffc4c1e3000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc4c1e3000,
                         .vm_end = (app_pc)0x00007ffc4c1e5000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_592 = {
    .iters = results_592,
    .iters_count = 45,
    .in_start = (app_pc)0x00007f2715468000,
    .want_return = 5,
    .want_start = (app_pc)0x00007f2715468000,
    .want_end = (app_pc)0x00007f2715a7a000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/lib64/debug/",
    .want_filename = "libdynamorio.so",
};

fake_memquery_result results_593[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056506efc4000,
                         .vm_end = (app_pc)0x000056506f35a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f559000,
                         .vm_end = (app_pc)0x000056506f581000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f581000,
                         .vm_end = (app_pc)0x000056506f5a0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f5a0000,
                         .vm_end = (app_pc)0x000056506f5d6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda290cf000,
                         .vm_end = (app_pc)0x00007fda29264000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29264000,
                         .vm_end = (app_pc)0x00007fda29464000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29464000,
                         .vm_end = (app_pc)0x00007fda29468000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29468000,
                         .vm_end = (app_pc)0x00007fda2946a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946a000,
                         .vm_end = (app_pc)0x00007fda2946e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946e000,
                         .vm_end = (app_pc)0x00007fda29471000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29471000,
                         .vm_end = (app_pc)0x00007fda29670000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29670000,
                         .vm_end = (app_pc)0x00007fda29671000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29671000,
                         .vm_end = (app_pc)0x00007fda29672000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29672000,
                         .vm_end = (app_pc)0x00007fda29775000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29775000,
                         .vm_end = (app_pc)0x00007fda29974000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29974000,
                         .vm_end = (app_pc)0x00007fda29975000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29975000,
                         .vm_end = (app_pc)0x00007fda29976000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29976000,
                         .vm_end = (app_pc)0x00007fda29999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b6a000,
                         .vm_end = (app_pc)0x00007fda29b6c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b96000,
                         .vm_end = (app_pc)0x00007fda29b99000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b99000,
                         .vm_end = (app_pc)0x00007fda29b9a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9a000,
                         .vm_end = (app_pc)0x00007fda29b9b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9b000,
                         .vm_end = (app_pc)0x00007fda29b9c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde26c000,
                         .vm_end = (app_pc)0x00007fffde28e000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde355000,
                         .vm_end = (app_pc)0x00007fffde358000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde358000,
                         .vm_end = (app_pc)0x00007fffde35a000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_593 = {
    .iters = results_593,
    .iters_count = 26,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_startstop",
    .want_return = 4,
    .want_start = (app_pc)0x000056506efc4000,
    .want_end = (app_pc)0x000056506f5d6000,
};

fake_memquery_result results_594[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efc3000,
                         .vm_end = (app_pc)0x000056506efc4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506efc4000,
                         .vm_end = (app_pc)0x000056506f35a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f559000,
                         .vm_end = (app_pc)0x000056506f581000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f581000,
                         .vm_end = (app_pc)0x000056506f5a0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f5a0000,
                         .vm_end = (app_pc)0x000056506f5d6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda290cf000,
                         .vm_end = (app_pc)0x00007fda29264000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29264000,
                         .vm_end = (app_pc)0x00007fda29464000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29464000,
                         .vm_end = (app_pc)0x00007fda29468000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29468000,
                         .vm_end = (app_pc)0x00007fda2946a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946a000,
                         .vm_end = (app_pc)0x00007fda2946e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946e000,
                         .vm_end = (app_pc)0x00007fda29471000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29471000,
                         .vm_end = (app_pc)0x00007fda29670000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29670000,
                         .vm_end = (app_pc)0x00007fda29671000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29671000,
                         .vm_end = (app_pc)0x00007fda29672000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29672000,
                         .vm_end = (app_pc)0x00007fda29775000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29775000,
                         .vm_end = (app_pc)0x00007fda29974000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29974000,
                         .vm_end = (app_pc)0x00007fda29975000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29975000,
                         .vm_end = (app_pc)0x00007fda29976000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29976000,
                         .vm_end = (app_pc)0x00007fda29999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b6a000,
                         .vm_end = (app_pc)0x00007fda29b6c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b96000,
                         .vm_end = (app_pc)0x00007fda29b99000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b99000,
                         .vm_end = (app_pc)0x00007fda29b9a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9a000,
                         .vm_end = (app_pc)0x00007fda29b9b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9b000,
                         .vm_end = (app_pc)0x00007fda29b9c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde26c000,
                         .vm_end = (app_pc)0x00007fffde28e000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde355000,
                         .vm_end = (app_pc)0x00007fffde358000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde358000,
                         .vm_end = (app_pc)0x00007fffde35a000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_594 = {
    .iters = results_594,
    .iters_count = 27,
    .in_start = (app_pc)0x000056506f582010,
    .want_return = 2,
    .want_start = (app_pc)0x000056506efc4000,
    .want_end = (app_pc)0x000056506f5d6000,
};

fake_memquery_result results_595[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efc3000,
                         .vm_end = (app_pc)0x000056505efc4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efc4000,
                         .vm_end = (app_pc)0x000056505efca000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efca000,
                         .vm_end = (app_pc)0x000056505efcc000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efcc000,
                         .vm_end = (app_pc)0x000056505efd6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efd6000,
                         .vm_end = (app_pc)0x000056505efdc000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efdc000,
                         .vm_end = (app_pc)0x000056505efdd000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efdd000,
                         .vm_end = (app_pc)0x000056505efe4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efe4000,
                         .vm_end = (app_pc)0x000056505efe5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056505efe5000,
                         .vm_end = (app_pc)0x000056506efc4000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506efc4000,
                         .vm_end = (app_pc)0x000056506f35a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f559000,
                         .vm_end = (app_pc)0x000056506f581000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f581000,
                         .vm_end = (app_pc)0x000056506f5a0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_startstop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056506f5a0000,
                         .vm_end = (app_pc)0x000056506f5d6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda290cf000,
                         .vm_end = (app_pc)0x00007fda29264000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29264000,
                         .vm_end = (app_pc)0x00007fda29464000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29464000,
                         .vm_end = (app_pc)0x00007fda29468000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29468000,
                         .vm_end = (app_pc)0x00007fda2946a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946a000,
                         .vm_end = (app_pc)0x00007fda2946e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda2946e000,
                         .vm_end = (app_pc)0x00007fda29471000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29471000,
                         .vm_end = (app_pc)0x00007fda29670000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29670000,
                         .vm_end = (app_pc)0x00007fda29671000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29671000,
                         .vm_end = (app_pc)0x00007fda29672000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29672000,
                         .vm_end = (app_pc)0x00007fda29775000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29775000,
                         .vm_end = (app_pc)0x00007fda29974000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29974000,
                         .vm_end = (app_pc)0x00007fda29975000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29975000,
                         .vm_end = (app_pc)0x00007fda29976000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29976000,
                         .vm_end = (app_pc)0x00007fda29999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b6a000,
                         .vm_end = (app_pc)0x00007fda29b6c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b96000,
                         .vm_end = (app_pc)0x00007fda29b99000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b99000,
                         .vm_end = (app_pc)0x00007fda29b9a000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9a000,
                         .vm_end = (app_pc)0x00007fda29b9b000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fda29b9b000,
                         .vm_end = (app_pc)0x00007fda29b9c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde26c000,
                         .vm_end = (app_pc)0x00007fffde28e000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde355000,
                         .vm_end = (app_pc)0x00007fffde358000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffde358000,
                         .vm_end = (app_pc)0x00007fffde35a000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_595 = {
    .iters = results_595,
    .iters_count = 35,
    .in_start = (app_pc)0x000056506f26ab88,
    .want_return = 4,
    .want_start = (app_pc)0x000056506efc4000,
    .want_end = (app_pc)0x000056506f5d6000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_startstop",
};

fake_memquery_result results_596[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a49b45000,
                         .vm_end = (app_pc)0x0000563a49edb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a0da000,
                         .vm_end = (app_pc)0x0000563a4a102000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a102000,
                         .vm_end = (app_pc)0x0000563a4a121000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a121000,
                         .vm_end = (app_pc)0x0000563a4a157000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60335de000,
                         .vm_end = (app_pc)0x00007f6033773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033773000,
                         .vm_end = (app_pc)0x00007f6033973000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033973000,
                         .vm_end = (app_pc)0x00007f6033977000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033977000,
                         .vm_end = (app_pc)0x00007f6033979000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033979000,
                         .vm_end = (app_pc)0x00007f603397d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f603397d000,
                         .vm_end = (app_pc)0x00007f6033980000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033980000,
                         .vm_end = (app_pc)0x00007f6033b7f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b7f000,
                         .vm_end = (app_pc)0x00007f6033b80000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b80000,
                         .vm_end = (app_pc)0x00007f6033b81000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b81000,
                         .vm_end = (app_pc)0x00007f6033c84000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033c84000,
                         .vm_end = (app_pc)0x00007f6033e83000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e83000,
                         .vm_end = (app_pc)0x00007f6033e84000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e84000,
                         .vm_end = (app_pc)0x00007f6033e85000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e85000,
                         .vm_end = (app_pc)0x00007f6033ea8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6034079000,
                         .vm_end = (app_pc)0x00007f603407b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a5000,
                         .vm_end = (app_pc)0x00007f60340a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a8000,
                         .vm_end = (app_pc)0x00007f60340a9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a9000,
                         .vm_end = (app_pc)0x00007f60340aa000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340aa000,
                         .vm_end = (app_pc)0x00007f60340ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0febb000,
                         .vm_end = (app_pc)0x00007ffc0fedd000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffcd000,
                         .vm_end = (app_pc)0x00007ffc0ffd0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffd0000,
                         .vm_end = (app_pc)0x00007ffc0ffd2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_596 = {
    .iters = results_596,
    .iters_count = 26,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_noclient",
    .want_return = 4,
    .want_start = (app_pc)0x0000563a49b45000,
    .want_end = (app_pc)0x0000563a4a157000,
};

fake_memquery_result results_597[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b44000,
                         .vm_end = (app_pc)0x0000563a49b45000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a49b45000,
                         .vm_end = (app_pc)0x0000563a49edb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a0da000,
                         .vm_end = (app_pc)0x0000563a4a102000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a102000,
                         .vm_end = (app_pc)0x0000563a4a121000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a121000,
                         .vm_end = (app_pc)0x0000563a4a157000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60335de000,
                         .vm_end = (app_pc)0x00007f6033773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033773000,
                         .vm_end = (app_pc)0x00007f6033973000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033973000,
                         .vm_end = (app_pc)0x00007f6033977000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033977000,
                         .vm_end = (app_pc)0x00007f6033979000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033979000,
                         .vm_end = (app_pc)0x00007f603397d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f603397d000,
                         .vm_end = (app_pc)0x00007f6033980000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033980000,
                         .vm_end = (app_pc)0x00007f6033b7f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b7f000,
                         .vm_end = (app_pc)0x00007f6033b80000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b80000,
                         .vm_end = (app_pc)0x00007f6033b81000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b81000,
                         .vm_end = (app_pc)0x00007f6033c84000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033c84000,
                         .vm_end = (app_pc)0x00007f6033e83000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e83000,
                         .vm_end = (app_pc)0x00007f6033e84000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e84000,
                         .vm_end = (app_pc)0x00007f6033e85000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e85000,
                         .vm_end = (app_pc)0x00007f6033ea8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6034079000,
                         .vm_end = (app_pc)0x00007f603407b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a5000,
                         .vm_end = (app_pc)0x00007f60340a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a8000,
                         .vm_end = (app_pc)0x00007f60340a9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a9000,
                         .vm_end = (app_pc)0x00007f60340aa000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340aa000,
                         .vm_end = (app_pc)0x00007f60340ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0febb000,
                         .vm_end = (app_pc)0x00007ffc0fedd000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffcd000,
                         .vm_end = (app_pc)0x00007ffc0ffd0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffd0000,
                         .vm_end = (app_pc)0x00007ffc0ffd2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_597 = {
    .iters = results_597,
    .iters_count = 27,
    .in_start = (app_pc)0x0000563a4a103010,
    .want_return = 2,
    .want_start = (app_pc)0x0000563a49b45000,
    .want_end = (app_pc)0x0000563a4a157000,
};

fake_memquery_result results_598[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b44000,
                         .vm_end = (app_pc)0x0000563a39b45000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b45000,
                         .vm_end = (app_pc)0x0000563a39b4b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b4b000,
                         .vm_end = (app_pc)0x0000563a39b4d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b4d000,
                         .vm_end = (app_pc)0x0000563a39b57000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b57000,
                         .vm_end = (app_pc)0x0000563a39b5d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b5d000,
                         .vm_end = (app_pc)0x0000563a39b5e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b5e000,
                         .vm_end = (app_pc)0x0000563a39b65000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b65000,
                         .vm_end = (app_pc)0x0000563a39b66000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b66000,
                         .vm_end = (app_pc)0x0000563a49b45000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a49b45000,
                         .vm_end = (app_pc)0x0000563a49edb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a0da000,
                         .vm_end = (app_pc)0x0000563a4a102000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a102000,
                         .vm_end = (app_pc)0x0000563a4a121000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a121000,
                         .vm_end = (app_pc)0x0000563a4a157000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60335de000,
                         .vm_end = (app_pc)0x00007f6033773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033773000,
                         .vm_end = (app_pc)0x00007f6033973000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033973000,
                         .vm_end = (app_pc)0x00007f6033977000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033977000,
                         .vm_end = (app_pc)0x00007f6033979000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033979000,
                         .vm_end = (app_pc)0x00007f603397d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f603397d000,
                         .vm_end = (app_pc)0x00007f6033980000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033980000,
                         .vm_end = (app_pc)0x00007f6033b7f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b7f000,
                         .vm_end = (app_pc)0x00007f6033b80000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b80000,
                         .vm_end = (app_pc)0x00007f6033b81000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b81000,
                         .vm_end = (app_pc)0x00007f6033c84000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033c84000,
                         .vm_end = (app_pc)0x00007f6033e83000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e83000,
                         .vm_end = (app_pc)0x00007f6033e84000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e84000,
                         .vm_end = (app_pc)0x00007f6033e85000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e85000,
                         .vm_end = (app_pc)0x00007f6033ea8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6034079000,
                         .vm_end = (app_pc)0x00007f603407b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a5000,
                         .vm_end = (app_pc)0x00007f60340a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a8000,
                         .vm_end = (app_pc)0x00007f60340a9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a9000,
                         .vm_end = (app_pc)0x00007f60340aa000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340aa000,
                         .vm_end = (app_pc)0x00007f60340ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0febb000,
                         .vm_end = (app_pc)0x00007ffc0fedd000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffcd000,
                         .vm_end = (app_pc)0x00007ffc0ffd0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffd0000,
                         .vm_end = (app_pc)0x00007ffc0ffd2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_598 = {
    .iters = results_598,
    .iters_count = 35,
    .in_start = (app_pc)0x0000563a49debb71,
    .want_return = 4,
    .want_start = (app_pc)0x0000563a49b45000,
    .want_end = (app_pc)0x0000563a4a157000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_noclient",
};

fake_memquery_result results_599[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a39b43000,
                         .vm_end = (app_pc)0x0000563a49b45000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a49b45000,
                         .vm_end = (app_pc)0x0000563a49edb000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a0da000,
                         .vm_end = (app_pc)0x0000563a4a102000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a102000,
                         .vm_end = (app_pc)0x0000563a4a121000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noclient" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4a121000,
                         .vm_end = (app_pc)0x0000563a4a157000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000563a4b8fb000,
                         .vm_end = (app_pc)0x0000563a4b91c000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60335de000,
                         .vm_end = (app_pc)0x00007f6033773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033773000,
                         .vm_end = (app_pc)0x00007f6033973000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033973000,
                         .vm_end = (app_pc)0x00007f6033977000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033977000,
                         .vm_end = (app_pc)0x00007f6033979000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033979000,
                         .vm_end = (app_pc)0x00007f603397d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f603397d000,
                         .vm_end = (app_pc)0x00007f6033980000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033980000,
                         .vm_end = (app_pc)0x00007f6033b7f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b7f000,
                         .vm_end = (app_pc)0x00007f6033b80000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b80000,
                         .vm_end = (app_pc)0x00007f6033b81000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033b81000,
                         .vm_end = (app_pc)0x00007f6033c84000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033c84000,
                         .vm_end = (app_pc)0x00007f6033e83000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e83000,
                         .vm_end = (app_pc)0x00007f6033e84000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e84000,
                         .vm_end = (app_pc)0x00007f6033e85000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6033e85000,
                         .vm_end = (app_pc)0x00007f6033ea8000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f6034079000,
                         .vm_end = (app_pc)0x00007f603407b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a5000,
                         .vm_end = (app_pc)0x00007f60340a8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a8000,
                         .vm_end = (app_pc)0x00007f60340a9000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340a9000,
                         .vm_end = (app_pc)0x00007f60340aa000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f60340aa000,
                         .vm_end = (app_pc)0x00007f60340ab000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0febb000,
                         .vm_end = (app_pc)0x00007ffc0fedd000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffcd000,
                         .vm_end = (app_pc)0x00007ffc0ffd0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffc0ffd0000,
                         .vm_end = (app_pc)0x00007ffc0ffd2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_599 = {
    .iters = results_599,
    .iters_count = 28,
    .in_start = (app_pc)0x0000563a4a103010,
    .want_return = 2,
    .want_start = (app_pc)0x0000563a49b45000,
    .want_end = (app_pc)0x0000563a4a157000,
};

fake_memquery_result results_600[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055e48474e000,
                         .vm_end = (app_pc)0x000055e484ae4000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484ce3000,
                         .vm_end = (app_pc)0x000055e484d0b000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d0b000,
                         .vm_end = (app_pc)0x000055e484d2a000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d2a000,
                         .vm_end = (app_pc)0x000055e484d60000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c2b6000,
                         .vm_end = (app_pc)0x00007f071c44b000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c44b000,
                         .vm_end = (app_pc)0x00007f071c64b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64b000,
                         .vm_end = (app_pc)0x00007f071c64f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64f000,
                         .vm_end = (app_pc)0x00007f071c651000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c651000,
                         .vm_end = (app_pc)0x00007f071c655000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c655000,
                         .vm_end = (app_pc)0x00007f071c658000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c658000,
                         .vm_end = (app_pc)0x00007f071c857000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c857000,
                         .vm_end = (app_pc)0x00007f071c858000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c858000,
                         .vm_end = (app_pc)0x00007f071c859000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c859000,
                         .vm_end = (app_pc)0x00007f071c95c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c95c000,
                         .vm_end = (app_pc)0x00007f071cb5b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5b000,
                         .vm_end = (app_pc)0x00007f071cb5c000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5c000,
                         .vm_end = (app_pc)0x00007f071cb5d000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5d000,
                         .vm_end = (app_pc)0x00007f071cb80000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd51000,
                         .vm_end = (app_pc)0x00007f071cd53000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd7d000,
                         .vm_end = (app_pc)0x00007f071cd80000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd80000,
                         .vm_end = (app_pc)0x00007f071cd81000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd81000,
                         .vm_end = (app_pc)0x00007f071cd82000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd82000,
                         .vm_end = (app_pc)0x00007f071cd83000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1afa000,
                         .vm_end = (app_pc)0x00007fffb1b1c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b62000,
                         .vm_end = (app_pc)0x00007fffb1b65000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b65000,
                         .vm_end = (app_pc)0x00007fffb1b67000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_600 = {
    .iters = results_600,
    .iters_count = 26,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_noinit",
    .want_return = 4,
    .want_start = (app_pc)0x000055e48474e000,
    .want_end = (app_pc)0x000055e484d60000,
};

fake_memquery_result results_601[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055e47474d000,
                         .vm_end = (app_pc)0x000055e48474e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e48474e000,
                         .vm_end = (app_pc)0x000055e484ae4000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484ce3000,
                         .vm_end = (app_pc)0x000055e484d0b000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d0b000,
                         .vm_end = (app_pc)0x000055e484d2a000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d2a000,
                         .vm_end = (app_pc)0x000055e484d60000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c2b6000,
                         .vm_end = (app_pc)0x00007f071c44b000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c44b000,
                         .vm_end = (app_pc)0x00007f071c64b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64b000,
                         .vm_end = (app_pc)0x00007f071c64f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64f000,
                         .vm_end = (app_pc)0x00007f071c651000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c651000,
                         .vm_end = (app_pc)0x00007f071c655000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c655000,
                         .vm_end = (app_pc)0x00007f071c658000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c658000,
                         .vm_end = (app_pc)0x00007f071c857000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c857000,
                         .vm_end = (app_pc)0x00007f071c858000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c858000,
                         .vm_end = (app_pc)0x00007f071c859000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c859000,
                         .vm_end = (app_pc)0x00007f071c95c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c95c000,
                         .vm_end = (app_pc)0x00007f071cb5b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5b000,
                         .vm_end = (app_pc)0x00007f071cb5c000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5c000,
                         .vm_end = (app_pc)0x00007f071cb5d000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5d000,
                         .vm_end = (app_pc)0x00007f071cb80000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd51000,
                         .vm_end = (app_pc)0x00007f071cd53000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd7d000,
                         .vm_end = (app_pc)0x00007f071cd80000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd80000,
                         .vm_end = (app_pc)0x00007f071cd81000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd81000,
                         .vm_end = (app_pc)0x00007f071cd82000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd82000,
                         .vm_end = (app_pc)0x00007f071cd83000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1afa000,
                         .vm_end = (app_pc)0x00007fffb1b1c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b62000,
                         .vm_end = (app_pc)0x00007fffb1b65000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b65000,
                         .vm_end = (app_pc)0x00007fffb1b67000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_601 = {
    .iters = results_601,
    .iters_count = 27,
    .in_start = (app_pc)0x000055e484d0c010,
    .want_return = 2,
    .want_start = (app_pc)0x000055e48474e000,
    .want_end = (app_pc)0x000055e484d60000,
};

fake_memquery_result results_602[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055e47474d000,
                         .vm_end = (app_pc)0x000055e47474e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e47474e000,
                         .vm_end = (app_pc)0x000055e474754000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e474754000,
                         .vm_end = (app_pc)0x000055e474756000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e474756000,
                         .vm_end = (app_pc)0x000055e474760000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e474760000,
                         .vm_end = (app_pc)0x000055e474766000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e474766000,
                         .vm_end = (app_pc)0x000055e474767000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e474767000,
                         .vm_end = (app_pc)0x000055e47476e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e47476e000,
                         .vm_end = (app_pc)0x000055e47476f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e47476f000,
                         .vm_end = (app_pc)0x000055e48474e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e48474e000,
                         .vm_end = (app_pc)0x000055e484ae4000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484ce3000,
                         .vm_end = (app_pc)0x000055e484d0b000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d0b000,
                         .vm_end = (app_pc)0x000055e484d2a000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_noinit" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055e484d2a000,
                         .vm_end = (app_pc)0x000055e484d60000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c2b6000,
                         .vm_end = (app_pc)0x00007f071c44b000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c44b000,
                         .vm_end = (app_pc)0x00007f071c64b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64b000,
                         .vm_end = (app_pc)0x00007f071c64f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c64f000,
                         .vm_end = (app_pc)0x00007f071c651000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c651000,
                         .vm_end = (app_pc)0x00007f071c655000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c655000,
                         .vm_end = (app_pc)0x00007f071c658000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c658000,
                         .vm_end = (app_pc)0x00007f071c857000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c857000,
                         .vm_end = (app_pc)0x00007f071c858000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c858000,
                         .vm_end = (app_pc)0x00007f071c859000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c859000,
                         .vm_end = (app_pc)0x00007f071c95c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071c95c000,
                         .vm_end = (app_pc)0x00007f071cb5b000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5b000,
                         .vm_end = (app_pc)0x00007f071cb5c000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5c000,
                         .vm_end = (app_pc)0x00007f071cb5d000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cb5d000,
                         .vm_end = (app_pc)0x00007f071cb80000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd51000,
                         .vm_end = (app_pc)0x00007f071cd53000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd7d000,
                         .vm_end = (app_pc)0x00007f071cd80000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd80000,
                         .vm_end = (app_pc)0x00007f071cd81000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd81000,
                         .vm_end = (app_pc)0x00007f071cd82000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f071cd82000,
                         .vm_end = (app_pc)0x00007f071cd83000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1afa000,
                         .vm_end = (app_pc)0x00007fffb1b1c000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b62000,
                         .vm_end = (app_pc)0x00007fffb1b65000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fffb1b65000,
                         .vm_end = (app_pc)0x00007fffb1b67000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_602 = {
    .iters = results_602,
    .iters_count = 35,
    .in_start = (app_pc)0x000055e4849f4b18,
    .want_return = 4,
    .want_start = (app_pc)0x000055e48474e000,
    .want_end = (app_pc)0x000055e484d60000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_noinit",
};

fake_memquery_result results_603[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cd4cf000,
                         .vm_end = (app_pc)0x000055f9cd865000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda64000,
                         .vm_end = (app_pc)0x000055f9cda8c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda8c000,
                         .vm_end = (app_pc)0x000055f9cdaab000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cdaab000,
                         .vm_end = (app_pc)0x000055f9cdae1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bc19000,
                         .vm_end = (app_pc)0x00007f223bdae000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bdae000,
                         .vm_end = (app_pc)0x00007f223bfae000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfae000,
                         .vm_end = (app_pc)0x00007f223bfb2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb2000,
                         .vm_end = (app_pc)0x00007f223bfb4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb4000,
                         .vm_end = (app_pc)0x00007f223bfb8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb8000,
                         .vm_end = (app_pc)0x00007f223bfbb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfbb000,
                         .vm_end = (app_pc)0x00007f223c1ba000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1ba000,
                         .vm_end = (app_pc)0x00007f223c1bb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bb000,
                         .vm_end = (app_pc)0x00007f223c1bc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bc000,
                         .vm_end = (app_pc)0x00007f223c2bf000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c2bf000,
                         .vm_end = (app_pc)0x00007f223c4be000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4be000,
                         .vm_end = (app_pc)0x00007f223c4bf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4bf000,
                         .vm_end = (app_pc)0x00007f223c4c0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4c0000,
                         .vm_end = (app_pc)0x00007f223c4e3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6b4000,
                         .vm_end = (app_pc)0x00007f223c6b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e0000,
                         .vm_end = (app_pc)0x00007f223c6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e3000,
                         .vm_end = (app_pc)0x00007f223c6e4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e4000,
                         .vm_end = (app_pc)0x00007f223c6e5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e5000,
                         .vm_end = (app_pc)0x00007f223c6e6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef975e000,
                         .vm_end = (app_pc)0x00007ffef9780000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef979d000,
                         .vm_end = (app_pc)0x00007ffef97a0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef97a0000,
                         .vm_end = (app_pc)0x00007ffef97a2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_603 = {
    .iters = results_603,
    .iters_count = 26,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_detach",
    .want_return = 4,
    .want_start = (app_pc)0x000055f9cd4cf000,
    .want_end = (app_pc)0x000055f9cdae1000,
};

fake_memquery_result results_604[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4ce000,
                         .vm_end = (app_pc)0x000055f9cd4cf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cd4cf000,
                         .vm_end = (app_pc)0x000055f9cd865000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda64000,
                         .vm_end = (app_pc)0x000055f9cda8c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda8c000,
                         .vm_end = (app_pc)0x000055f9cdaab000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cdaab000,
                         .vm_end = (app_pc)0x000055f9cdae1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bc19000,
                         .vm_end = (app_pc)0x00007f223bdae000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bdae000,
                         .vm_end = (app_pc)0x00007f223bfae000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfae000,
                         .vm_end = (app_pc)0x00007f223bfb2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb2000,
                         .vm_end = (app_pc)0x00007f223bfb4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb4000,
                         .vm_end = (app_pc)0x00007f223bfb8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb8000,
                         .vm_end = (app_pc)0x00007f223bfbb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfbb000,
                         .vm_end = (app_pc)0x00007f223c1ba000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1ba000,
                         .vm_end = (app_pc)0x00007f223c1bb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bb000,
                         .vm_end = (app_pc)0x00007f223c1bc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bc000,
                         .vm_end = (app_pc)0x00007f223c2bf000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c2bf000,
                         .vm_end = (app_pc)0x00007f223c4be000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4be000,
                         .vm_end = (app_pc)0x00007f223c4bf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4bf000,
                         .vm_end = (app_pc)0x00007f223c4c0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4c0000,
                         .vm_end = (app_pc)0x00007f223c4e3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6b4000,
                         .vm_end = (app_pc)0x00007f223c6b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e0000,
                         .vm_end = (app_pc)0x00007f223c6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e3000,
                         .vm_end = (app_pc)0x00007f223c6e4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e4000,
                         .vm_end = (app_pc)0x00007f223c6e5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e5000,
                         .vm_end = (app_pc)0x00007f223c6e6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef975e000,
                         .vm_end = (app_pc)0x00007ffef9780000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef979d000,
                         .vm_end = (app_pc)0x00007ffef97a0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef97a0000,
                         .vm_end = (app_pc)0x00007ffef97a2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_604 = {
    .iters = results_604,
    .iters_count = 27,
    .in_start = (app_pc)0x000055f9cda8d010,
    .want_return = 2,
    .want_start = (app_pc)0x000055f9cd4cf000,
    .want_end = (app_pc)0x000055f9cdae1000,
};

fake_memquery_result results_605[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4ce000,
                         .vm_end = (app_pc)0x000055f9bd4cf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4cf000,
                         .vm_end = (app_pc)0x000055f9bd4d5000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4d5000,
                         .vm_end = (app_pc)0x000055f9bd4d7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4d7000,
                         .vm_end = (app_pc)0x000055f9bd4e1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4e1000,
                         .vm_end = (app_pc)0x000055f9bd4e7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4e7000,
                         .vm_end = (app_pc)0x000055f9bd4e8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4e8000,
                         .vm_end = (app_pc)0x000055f9bd4ef000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4ef000,
                         .vm_end = (app_pc)0x000055f9bd4f0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4f0000,
                         .vm_end = (app_pc)0x000055f9cd4cf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cd4cf000,
                         .vm_end = (app_pc)0x000055f9cd865000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda64000,
                         .vm_end = (app_pc)0x000055f9cda8c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda8c000,
                         .vm_end = (app_pc)0x000055f9cdaab000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cdaab000,
                         .vm_end = (app_pc)0x000055f9cdae1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bc19000,
                         .vm_end = (app_pc)0x00007f223bdae000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bdae000,
                         .vm_end = (app_pc)0x00007f223bfae000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfae000,
                         .vm_end = (app_pc)0x00007f223bfb2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb2000,
                         .vm_end = (app_pc)0x00007f223bfb4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb4000,
                         .vm_end = (app_pc)0x00007f223bfb8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb8000,
                         .vm_end = (app_pc)0x00007f223bfbb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfbb000,
                         .vm_end = (app_pc)0x00007f223c1ba000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1ba000,
                         .vm_end = (app_pc)0x00007f223c1bb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bb000,
                         .vm_end = (app_pc)0x00007f223c1bc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bc000,
                         .vm_end = (app_pc)0x00007f223c2bf000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c2bf000,
                         .vm_end = (app_pc)0x00007f223c4be000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4be000,
                         .vm_end = (app_pc)0x00007f223c4bf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4bf000,
                         .vm_end = (app_pc)0x00007f223c4c0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4c0000,
                         .vm_end = (app_pc)0x00007f223c4e3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6b4000,
                         .vm_end = (app_pc)0x00007f223c6b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e0000,
                         .vm_end = (app_pc)0x00007f223c6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e3000,
                         .vm_end = (app_pc)0x00007f223c6e4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e4000,
                         .vm_end = (app_pc)0x00007f223c6e5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e5000,
                         .vm_end = (app_pc)0x00007f223c6e6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef975e000,
                         .vm_end = (app_pc)0x00007ffef9780000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef979d000,
                         .vm_end = (app_pc)0x00007ffef97a0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef97a0000,
                         .vm_end = (app_pc)0x00007ffef97a2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_605 = {
    .iters = results_605,
    .iters_count = 35,
    .in_start = (app_pc)0x000055f9cd775c09,
    .want_return = 4,
    .want_start = (app_pc)0x000055f9cd4cf000,
    .want_end = (app_pc)0x000055f9cdae1000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_detach",
};

fake_memquery_result results_606[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9bd4cd000,
                         .vm_end = (app_pc)0x000055f9cd4cf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cd4cf000,
                         .vm_end = (app_pc)0x000055f9cd865000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda64000,
                         .vm_end = (app_pc)0x000055f9cda8c000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cda8c000,
                         .vm_end = (app_pc)0x000055f9cdaab000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_detach" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cdaab000,
                         .vm_end = (app_pc)0x000055f9cdae1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055f9cef37000,
                         .vm_end = (app_pc)0x000055f9cef58000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bc19000,
                         .vm_end = (app_pc)0x00007f223bdae000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bdae000,
                         .vm_end = (app_pc)0x00007f223bfae000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfae000,
                         .vm_end = (app_pc)0x00007f223bfb2000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb2000,
                         .vm_end = (app_pc)0x00007f223bfb4000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb4000,
                         .vm_end = (app_pc)0x00007f223bfb8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfb8000,
                         .vm_end = (app_pc)0x00007f223bfbb000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223bfbb000,
                         .vm_end = (app_pc)0x00007f223c1ba000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1ba000,
                         .vm_end = (app_pc)0x00007f223c1bb000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bb000,
                         .vm_end = (app_pc)0x00007f223c1bc000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c1bc000,
                         .vm_end = (app_pc)0x00007f223c2bf000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c2bf000,
                         .vm_end = (app_pc)0x00007f223c4be000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4be000,
                         .vm_end = (app_pc)0x00007f223c4bf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4bf000,
                         .vm_end = (app_pc)0x00007f223c4c0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c4c0000,
                         .vm_end = (app_pc)0x00007f223c4e3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6b4000,
                         .vm_end = (app_pc)0x00007f223c6b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e0000,
                         .vm_end = (app_pc)0x00007f223c6e3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e3000,
                         .vm_end = (app_pc)0x00007f223c6e4000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e4000,
                         .vm_end = (app_pc)0x00007f223c6e5000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007f223c6e5000,
                         .vm_end = (app_pc)0x00007f223c6e6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef975e000,
                         .vm_end = (app_pc)0x00007ffef9780000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef979d000,
                         .vm_end = (app_pc)0x00007ffef97a0000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffef97a0000,
                         .vm_end = (app_pc)0x00007ffef97a2000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_606 = {
    .iters = results_606,
    .iters_count = 28,
    .in_start = (app_pc)0x000055f9cda8d010,
    .want_return = 2,
    .want_start = (app_pc)0x000055f9cd4cf000,
    .want_end = (app_pc)0x000055f9cdae1000,
};

fake_memquery_result results_607[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e104000,
                         .vm_end = (app_pc)0x000056373e49a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e699000,
                         .vm_end = (app_pc)0x000056373e6c1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6c1000,
                         .vm_end = (app_pc)0x000056373e6e0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6e0000,
                         .vm_end = (app_pc)0x000056373e716000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373f306000,
                         .vm_end = (app_pc)0x000056373f327000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150804000,
                         .vm_end = (app_pc)0x00007fb150999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150999000,
                         .vm_end = (app_pc)0x00007fb150b99000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b99000,
                         .vm_end = (app_pc)0x00007fb150b9d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9d000,
                         .vm_end = (app_pc)0x00007fb150b9f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9f000,
                         .vm_end = (app_pc)0x00007fb150ba3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba3000,
                         .vm_end = (app_pc)0x00007fb150ba6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba6000,
                         .vm_end = (app_pc)0x00007fb150da5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da5000,
                         .vm_end = (app_pc)0x00007fb150da6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da6000,
                         .vm_end = (app_pc)0x00007fb150da7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da7000,
                         .vm_end = (app_pc)0x00007fb150eaa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150eaa000,
                         .vm_end = (app_pc)0x00007fb1510a9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510a9000,
                         .vm_end = (app_pc)0x00007fb1510aa000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510aa000,
                         .vm_end = (app_pc)0x00007fb1510ab000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510ab000,
                         .vm_end = (app_pc)0x00007fb1510ce000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb15129f000,
                         .vm_end = (app_pc)0x00007fb1512a1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cb000,
                         .vm_end = (app_pc)0x00007fb1512ce000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512ce000,
                         .vm_end = (app_pc)0x00007fb1512cf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cf000,
                         .vm_end = (app_pc)0x00007fb1512d0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512d0000,
                         .vm_end = (app_pc)0x00007fb1512d1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3408000,
                         .vm_end = (app_pc)0x00007ffcf342a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3482000,
                         .vm_end = (app_pc)0x00007ffcf3485000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3485000,
                         .vm_end = (app_pc)0x00007ffcf3487000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_607 = {
    .iters = results_607,
    .iters_count = 27,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_prepop",
    .want_return = 4,
    .want_start = (app_pc)0x000056373e104000,
    .want_end = (app_pc)0x000056373e716000,
};

fake_memquery_result results_608[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e103000,
                         .vm_end = (app_pc)0x000056373e104000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e104000,
                         .vm_end = (app_pc)0x000056373e49a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e699000,
                         .vm_end = (app_pc)0x000056373e6c1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6c1000,
                         .vm_end = (app_pc)0x000056373e6e0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6e0000,
                         .vm_end = (app_pc)0x000056373e716000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373f306000,
                         .vm_end = (app_pc)0x000056373f327000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150804000,
                         .vm_end = (app_pc)0x00007fb150999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150999000,
                         .vm_end = (app_pc)0x00007fb150b99000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b99000,
                         .vm_end = (app_pc)0x00007fb150b9d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9d000,
                         .vm_end = (app_pc)0x00007fb150b9f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9f000,
                         .vm_end = (app_pc)0x00007fb150ba3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba3000,
                         .vm_end = (app_pc)0x00007fb150ba6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba6000,
                         .vm_end = (app_pc)0x00007fb150da5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da5000,
                         .vm_end = (app_pc)0x00007fb150da6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da6000,
                         .vm_end = (app_pc)0x00007fb150da7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da7000,
                         .vm_end = (app_pc)0x00007fb150eaa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150eaa000,
                         .vm_end = (app_pc)0x00007fb1510a9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510a9000,
                         .vm_end = (app_pc)0x00007fb1510aa000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510aa000,
                         .vm_end = (app_pc)0x00007fb1510ab000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510ab000,
                         .vm_end = (app_pc)0x00007fb1510ce000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb15129f000,
                         .vm_end = (app_pc)0x00007fb1512a1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cb000,
                         .vm_end = (app_pc)0x00007fb1512ce000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512ce000,
                         .vm_end = (app_pc)0x00007fb1512cf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cf000,
                         .vm_end = (app_pc)0x00007fb1512d0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512d0000,
                         .vm_end = (app_pc)0x00007fb1512d1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3408000,
                         .vm_end = (app_pc)0x00007ffcf342a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3482000,
                         .vm_end = (app_pc)0x00007ffcf3485000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3485000,
                         .vm_end = (app_pc)0x00007ffcf3487000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_608 = {
    .iters = results_608,
    .iters_count = 28,
    .in_start = (app_pc)0x000056373e6c2010,
    .want_return = 2,
    .want_start = (app_pc)0x000056373e104000,
    .want_end = (app_pc)0x000056373e716000,
};

fake_memquery_result results_609[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e103000,
                         .vm_end = (app_pc)0x000056372e104000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e104000,
                         .vm_end = (app_pc)0x000056372e10a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e10a000,
                         .vm_end = (app_pc)0x000056372e10c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e10c000,
                         .vm_end = (app_pc)0x000056372e116000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e116000,
                         .vm_end = (app_pc)0x000056372e11c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e11c000,
                         .vm_end = (app_pc)0x000056372e11d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e11d000,
                         .vm_end = (app_pc)0x000056372e124000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e124000,
                         .vm_end = (app_pc)0x000056372e125000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e125000,
                         .vm_end = (app_pc)0x000056373e104000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e104000,
                         .vm_end = (app_pc)0x000056373e49a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e699000,
                         .vm_end = (app_pc)0x000056373e6c1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6c1000,
                         .vm_end = (app_pc)0x000056373e6e0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6e0000,
                         .vm_end = (app_pc)0x000056373e716000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373f306000,
                         .vm_end = (app_pc)0x000056373f327000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150804000,
                         .vm_end = (app_pc)0x00007fb150999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150999000,
                         .vm_end = (app_pc)0x00007fb150b99000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b99000,
                         .vm_end = (app_pc)0x00007fb150b9d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9d000,
                         .vm_end = (app_pc)0x00007fb150b9f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9f000,
                         .vm_end = (app_pc)0x00007fb150ba3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba3000,
                         .vm_end = (app_pc)0x00007fb150ba6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba6000,
                         .vm_end = (app_pc)0x00007fb150da5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da5000,
                         .vm_end = (app_pc)0x00007fb150da6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da6000,
                         .vm_end = (app_pc)0x00007fb150da7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da7000,
                         .vm_end = (app_pc)0x00007fb150eaa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150eaa000,
                         .vm_end = (app_pc)0x00007fb1510a9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510a9000,
                         .vm_end = (app_pc)0x00007fb1510aa000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510aa000,
                         .vm_end = (app_pc)0x00007fb1510ab000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510ab000,
                         .vm_end = (app_pc)0x00007fb1510ce000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb15129f000,
                         .vm_end = (app_pc)0x00007fb1512a1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cb000,
                         .vm_end = (app_pc)0x00007fb1512ce000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512ce000,
                         .vm_end = (app_pc)0x00007fb1512cf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cf000,
                         .vm_end = (app_pc)0x00007fb1512d0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512d0000,
                         .vm_end = (app_pc)0x00007fb1512d1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3408000,
                         .vm_end = (app_pc)0x00007ffcf342a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3482000,
                         .vm_end = (app_pc)0x00007ffcf3485000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3485000,
                         .vm_end = (app_pc)0x00007ffcf3487000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_609 = {
    .iters = results_609,
    .iters_count = 36,
    .in_start = (app_pc)0x000056373e3aafcf,
    .want_return = 4,
    .want_start = (app_pc)0x000056373e104000,
    .want_end = (app_pc)0x000056373e716000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_prepop",
};

fake_memquery_result results_610[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000056372e102000,
                         .vm_end = (app_pc)0x000056373e104000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e104000,
                         .vm_end = (app_pc)0x000056373e49a000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e699000,
                         .vm_end = (app_pc)0x000056373e6c1000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6c1000,
                         .vm_end = (app_pc)0x000056373e6e0000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_prepop" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373e6e0000,
                         .vm_end = (app_pc)0x000056373e716000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000056373f306000,
                         .vm_end = (app_pc)0x000056373f327000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150804000,
                         .vm_end = (app_pc)0x00007fb150999000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150999000,
                         .vm_end = (app_pc)0x00007fb150b99000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b99000,
                         .vm_end = (app_pc)0x00007fb150b9d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9d000,
                         .vm_end = (app_pc)0x00007fb150b9f000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150b9f000,
                         .vm_end = (app_pc)0x00007fb150ba3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba3000,
                         .vm_end = (app_pc)0x00007fb150ba6000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150ba6000,
                         .vm_end = (app_pc)0x00007fb150da5000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da5000,
                         .vm_end = (app_pc)0x00007fb150da6000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da6000,
                         .vm_end = (app_pc)0x00007fb150da7000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150da7000,
                         .vm_end = (app_pc)0x00007fb150eaa000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb150eaa000,
                         .vm_end = (app_pc)0x00007fb1510a9000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510a9000,
                         .vm_end = (app_pc)0x00007fb1510aa000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510aa000,
                         .vm_end = (app_pc)0x00007fb1510ab000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1510ab000,
                         .vm_end = (app_pc)0x00007fb1510ce000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb15129f000,
                         .vm_end = (app_pc)0x00007fb1512a1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cb000,
                         .vm_end = (app_pc)0x00007fb1512ce000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512ce000,
                         .vm_end = (app_pc)0x00007fb1512cf000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512cf000,
                         .vm_end = (app_pc)0x00007fb1512d0000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb1512d0000,
                         .vm_end = (app_pc)0x00007fb1512d1000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3408000,
                         .vm_end = (app_pc)0x00007ffcf342a000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3482000,
                         .vm_end = (app_pc)0x00007ffcf3485000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffcf3485000,
                         .vm_end = (app_pc)0x00007ffcf3487000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_610 = {
    .iters = results_610,
    .iters_count = 28,
    .in_start = (app_pc)0x000056373e6c2010,
    .want_return = 2,
    .want_start = (app_pc)0x000056373e104000,
    .want_end = (app_pc)0x000056373e716000,
};

fake_memquery_result results_611[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055864880a000,
                         .vm_end = (app_pc)0x0000558648ba1000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000614000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648da0000,
                         .vm_end = (app_pc)0x0000558648dc8000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648dc8000,
                         .vm_end = (app_pc)0x0000558648de7000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648de7000,
                         .vm_end = (app_pc)0x0000558648e1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005586499f9000,
                         .vm_end = (app_pc)0x0000558649a1a000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644000000,
                         .vm_end = (app_pc)0x00007fd644025000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644025000,
                         .vm_end = (app_pc)0x00007fd648000000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd94000,
                         .vm_end = (app_pc)0x00007fd64bd95000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd95000,
                         .vm_end = (app_pc)0x00007fd64c595000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c595000,
                         .vm_end = (app_pc)0x00007fd64c72a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c72a000,
                         .vm_end = (app_pc)0x00007fd64c92a000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92a000,
                         .vm_end = (app_pc)0x00007fd64c92e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92e000,
                         .vm_end = (app_pc)0x00007fd64c930000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c930000,
                         .vm_end = (app_pc)0x00007fd64c934000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c934000,
                         .vm_end = (app_pc)0x00007fd64c937000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c937000,
                         .vm_end = (app_pc)0x00007fd64cb36000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb36000,
                         .vm_end = (app_pc)0x00007fd64cb37000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb37000,
                         .vm_end = (app_pc)0x00007fd64cb38000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb38000,
                         .vm_end = (app_pc)0x00007fd64cb50000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb50000,
                         .vm_end = (app_pc)0x00007fd64cd4f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd4f000,
                         .vm_end = (app_pc)0x00007fd64cd50000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd50000,
                         .vm_end = (app_pc)0x00007fd64cd51000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd51000,
                         .vm_end = (app_pc)0x00007fd64cd55000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd55000,
                         .vm_end = (app_pc)0x00007fd64ce58000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64ce58000,
                         .vm_end = (app_pc)0x00007fd64d057000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d057000,
                         .vm_end = (app_pc)0x00007fd64d058000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d058000,
                         .vm_end = (app_pc)0x00007fd64d059000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d059000,
                         .vm_end = (app_pc)0x00007fd64d07c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d24d000,
                         .vm_end = (app_pc)0x00007fd64d24f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d279000,
                         .vm_end = (app_pc)0x00007fd64d27c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27c000,
                         .vm_end = (app_pc)0x00007fd64d27d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27d000,
                         .vm_end = (app_pc)0x00007fd64d27e000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27e000,
                         .vm_end = (app_pc)0x00007fd64d27f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe6736b000,
                         .vm_end = (app_pc)0x00007ffe6738d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673b9000,
                         .vm_end = (app_pc)0x00007ffe673bc000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673bc000,
                         .vm_end = (app_pc)0x00007ffe673be000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_611 = {
    .iters = results_611,
    .iters_count = 36,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_signal",
    .want_return = 4,
    .want_start = (app_pc)0x000055864880a000,
    .want_end = (app_pc)0x0000558648e1e000,
};

fake_memquery_result results_612[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638809000,
                         .vm_end = (app_pc)0x000055864880a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055864880a000,
                         .vm_end = (app_pc)0x0000558648ba1000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000614000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648da0000,
                         .vm_end = (app_pc)0x0000558648dc8000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648dc8000,
                         .vm_end = (app_pc)0x0000558648de7000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648de7000,
                         .vm_end = (app_pc)0x0000558648e1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005586499f9000,
                         .vm_end = (app_pc)0x0000558649a1a000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644000000,
                         .vm_end = (app_pc)0x00007fd644025000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644025000,
                         .vm_end = (app_pc)0x00007fd648000000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd94000,
                         .vm_end = (app_pc)0x00007fd64bd95000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd95000,
                         .vm_end = (app_pc)0x00007fd64c595000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c595000,
                         .vm_end = (app_pc)0x00007fd64c72a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c72a000,
                         .vm_end = (app_pc)0x00007fd64c92a000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92a000,
                         .vm_end = (app_pc)0x00007fd64c92e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92e000,
                         .vm_end = (app_pc)0x00007fd64c930000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c930000,
                         .vm_end = (app_pc)0x00007fd64c934000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c934000,
                         .vm_end = (app_pc)0x00007fd64c937000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c937000,
                         .vm_end = (app_pc)0x00007fd64cb36000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb36000,
                         .vm_end = (app_pc)0x00007fd64cb37000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb37000,
                         .vm_end = (app_pc)0x00007fd64cb38000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb38000,
                         .vm_end = (app_pc)0x00007fd64cb50000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb50000,
                         .vm_end = (app_pc)0x00007fd64cd4f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd4f000,
                         .vm_end = (app_pc)0x00007fd64cd50000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd50000,
                         .vm_end = (app_pc)0x00007fd64cd51000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd51000,
                         .vm_end = (app_pc)0x00007fd64cd55000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd55000,
                         .vm_end = (app_pc)0x00007fd64ce58000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64ce58000,
                         .vm_end = (app_pc)0x00007fd64d057000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d057000,
                         .vm_end = (app_pc)0x00007fd64d058000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d058000,
                         .vm_end = (app_pc)0x00007fd64d059000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d059000,
                         .vm_end = (app_pc)0x00007fd64d07c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d24d000,
                         .vm_end = (app_pc)0x00007fd64d24f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d279000,
                         .vm_end = (app_pc)0x00007fd64d27c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27c000,
                         .vm_end = (app_pc)0x00007fd64d27d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27d000,
                         .vm_end = (app_pc)0x00007fd64d27e000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27e000,
                         .vm_end = (app_pc)0x00007fd64d27f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe6736b000,
                         .vm_end = (app_pc)0x00007ffe6738d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673b9000,
                         .vm_end = (app_pc)0x00007ffe673bc000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673bc000,
                         .vm_end = (app_pc)0x00007ffe673be000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_612 = {
    .iters = results_612,
    .iters_count = 37,
    .in_start = (app_pc)0x0000558648dc9010,
    .want_return = 2,
    .want_start = (app_pc)0x000055864880a000,
    .want_end = (app_pc)0x0000558648e1e000,
};

fake_memquery_result results_613[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638809000,
                         .vm_end = (app_pc)0x000055863880a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055863880a000,
                         .vm_end = (app_pc)0x0000558638810000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638810000,
                         .vm_end = (app_pc)0x0000558638812000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638812000,
                         .vm_end = (app_pc)0x000055863881c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055863881c000,
                         .vm_end = (app_pc)0x0000558638822000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638822000,
                         .vm_end = (app_pc)0x0000558638823000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558638823000,
                         .vm_end = (app_pc)0x000055863882a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055863882a000,
                         .vm_end = (app_pc)0x000055863882b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055863882b000,
                         .vm_end = (app_pc)0x000055864880a000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055864880a000,
                         .vm_end = (app_pc)0x0000558648ba1000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000614000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648da0000,
                         .vm_end = (app_pc)0x0000558648dc8000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648dc8000,
                         .vm_end = (app_pc)0x0000558648de7000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_signal" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x0000558648de7000,
                         .vm_end = (app_pc)0x0000558648e1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005586499f9000,
                         .vm_end = (app_pc)0x0000558649a1a000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644000000,
                         .vm_end = (app_pc)0x00007fd644025000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd644025000,
                         .vm_end = (app_pc)0x00007fd648000000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd94000,
                         .vm_end = (app_pc)0x00007fd64bd95000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64bd95000,
                         .vm_end = (app_pc)0x00007fd64c595000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c595000,
                         .vm_end = (app_pc)0x00007fd64c72a000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c72a000,
                         .vm_end = (app_pc)0x00007fd64c92a000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92a000,
                         .vm_end = (app_pc)0x00007fd64c92e000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c92e000,
                         .vm_end = (app_pc)0x00007fd64c930000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c930000,
                         .vm_end = (app_pc)0x00007fd64c934000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c934000,
                         .vm_end = (app_pc)0x00007fd64c937000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64c937000,
                         .vm_end = (app_pc)0x00007fd64cb36000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb36000,
                         .vm_end = (app_pc)0x00007fd64cb37000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb37000,
                         .vm_end = (app_pc)0x00007fd64cb38000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb38000,
                         .vm_end = (app_pc)0x00007fd64cb50000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cb50000,
                         .vm_end = (app_pc)0x00007fd64cd4f000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd4f000,
                         .vm_end = (app_pc)0x00007fd64cd50000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd50000,
                         .vm_end = (app_pc)0x00007fd64cd51000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd51000,
                         .vm_end = (app_pc)0x00007fd64cd55000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64cd55000,
                         .vm_end = (app_pc)0x00007fd64ce58000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64ce58000,
                         .vm_end = (app_pc)0x00007fd64d057000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d057000,
                         .vm_end = (app_pc)0x00007fd64d058000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d058000,
                         .vm_end = (app_pc)0x00007fd64d059000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d059000,
                         .vm_end = (app_pc)0x00007fd64d07c000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d24d000,
                         .vm_end = (app_pc)0x00007fd64d24f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d279000,
                         .vm_end = (app_pc)0x00007fd64d27c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27c000,
                         .vm_end = (app_pc)0x00007fd64d27d000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27d000,
                         .vm_end = (app_pc)0x00007fd64d27e000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fd64d27e000,
                         .vm_end = (app_pc)0x00007fd64d27f000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe6736b000,
                         .vm_end = (app_pc)0x00007ffe6738d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673b9000,
                         .vm_end = (app_pc)0x00007ffe673bc000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffe673bc000,
                         .vm_end = (app_pc)0x00007ffe673be000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_613 = {
    .iters = results_613,
    .iters_count = 45,
    .in_start = (app_pc)0x0000558648ab1a6b,
    .want_return = 4,
    .want_start = (app_pc)0x000055864880a000,
    .want_end = (app_pc)0x0000558648e1e000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_signal",
};

fake_memquery_result results_614[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f209d000,
                         .vm_end = (app_pc)0x000055d8f2433000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2632000,
                         .vm_end = (app_pc)0x000055d8f265a000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f265a000,
                         .vm_end = (app_pc)0x000055d8f2679000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2679000,
                         .vm_end = (app_pc)0x000055d8f26af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f90cd000,
                         .vm_end = (app_pc)0x00007fb3f9262000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9262000,
                         .vm_end = (app_pc)0x00007fb3f9462000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9462000,
                         .vm_end = (app_pc)0x00007fb3f9466000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9466000,
                         .vm_end = (app_pc)0x00007fb3f9468000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9468000,
                         .vm_end = (app_pc)0x00007fb3f946c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946c000,
                         .vm_end = (app_pc)0x00007fb3f946f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946f000,
                         .vm_end = (app_pc)0x00007fb3f966e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966e000,
                         .vm_end = (app_pc)0x00007fb3f966f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966f000,
                         .vm_end = (app_pc)0x00007fb3f9670000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9670000,
                         .vm_end = (app_pc)0x00007fb3f9773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9773000,
                         .vm_end = (app_pc)0x00007fb3f9972000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9972000,
                         .vm_end = (app_pc)0x00007fb3f9973000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9973000,
                         .vm_end = (app_pc)0x00007fb3f9974000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9974000,
                         .vm_end = (app_pc)0x00007fb3f9997000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b68000,
                         .vm_end = (app_pc)0x00007fb3f9b6a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b94000,
                         .vm_end = (app_pc)0x00007fb3f9b97000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b97000,
                         .vm_end = (app_pc)0x00007fb3f9b98000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b98000,
                         .vm_end = (app_pc)0x00007fb3f9b99000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b99000,
                         .vm_end = (app_pc)0x00007fb3f9b9a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d67f000,
                         .vm_end = (app_pc)0x00007fff4d6a1000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d1000,
                         .vm_end = (app_pc)0x00007fff4d6d4000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d4000,
                         .vm_end = (app_pc)0x00007fff4d6d6000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_614 = {
    .iters = results_614,
    .iters_count = 26,
    .in_start = (app_pc)0x0000000000000000,
    .in_name =
        "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/api.static_crash",
    .want_return = 4,
    .want_start = (app_pc)0x000055d8f209d000,
    .want_end = (app_pc)0x000055d8f26af000,
};

fake_memquery_result results_615[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e209c000,
                         .vm_end = (app_pc)0x000055d8f209d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f209d000,
                         .vm_end = (app_pc)0x000055d8f2433000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2632000,
                         .vm_end = (app_pc)0x000055d8f265a000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f265a000,
                         .vm_end = (app_pc)0x000055d8f2679000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2679000,
                         .vm_end = (app_pc)0x000055d8f26af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f90cd000,
                         .vm_end = (app_pc)0x00007fb3f9262000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9262000,
                         .vm_end = (app_pc)0x00007fb3f9462000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9462000,
                         .vm_end = (app_pc)0x00007fb3f9466000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9466000,
                         .vm_end = (app_pc)0x00007fb3f9468000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9468000,
                         .vm_end = (app_pc)0x00007fb3f946c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946c000,
                         .vm_end = (app_pc)0x00007fb3f946f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946f000,
                         .vm_end = (app_pc)0x00007fb3f966e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966e000,
                         .vm_end = (app_pc)0x00007fb3f966f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966f000,
                         .vm_end = (app_pc)0x00007fb3f9670000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9670000,
                         .vm_end = (app_pc)0x00007fb3f9773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9773000,
                         .vm_end = (app_pc)0x00007fb3f9972000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9972000,
                         .vm_end = (app_pc)0x00007fb3f9973000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9973000,
                         .vm_end = (app_pc)0x00007fb3f9974000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9974000,
                         .vm_end = (app_pc)0x00007fb3f9997000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b68000,
                         .vm_end = (app_pc)0x00007fb3f9b6a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b94000,
                         .vm_end = (app_pc)0x00007fb3f9b97000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b97000,
                         .vm_end = (app_pc)0x00007fb3f9b98000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b98000,
                         .vm_end = (app_pc)0x00007fb3f9b99000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b99000,
                         .vm_end = (app_pc)0x00007fb3f9b9a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d67f000,
                         .vm_end = (app_pc)0x00007fff4d6a1000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d1000,
                         .vm_end = (app_pc)0x00007fff4d6d4000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d4000,
                         .vm_end = (app_pc)0x00007fff4d6d6000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_615 = {
    .iters = results_615,
    .iters_count = 27,
    .in_start = (app_pc)0x000055d8f265b010,
    .want_return = 2,
    .want_start = (app_pc)0x000055d8f209d000,
    .want_end = (app_pc)0x000055d8f26af000,
};

fake_memquery_result results_616[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e209c000,
                         .vm_end = (app_pc)0x000055d8e209d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e209d000,
                         .vm_end = (app_pc)0x000055d8e20a3000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20a3000,
                         .vm_end = (app_pc)0x000055d8e20a5000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20a5000,
                         .vm_end = (app_pc)0x000055d8e20af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20af000,
                         .vm_end = (app_pc)0x000055d8e20b5000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20b5000,
                         .vm_end = (app_pc)0x000055d8e20b6000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20b6000,
                         .vm_end = (app_pc)0x000055d8e20bd000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20bd000,
                         .vm_end = (app_pc)0x000055d8e20be000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8e20be000,
                         .vm_end = (app_pc)0x000055d8f209d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f209d000,
                         .vm_end = (app_pc)0x000055d8f2433000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2632000,
                         .vm_end = (app_pc)0x000055d8f265a000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f265a000,
                         .vm_end = (app_pc)0x000055d8f2679000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_crash" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x000055d8f2679000,
                         .vm_end = (app_pc)0x000055d8f26af000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f90cd000,
                         .vm_end = (app_pc)0x00007fb3f9262000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9262000,
                         .vm_end = (app_pc)0x00007fb3f9462000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9462000,
                         .vm_end = (app_pc)0x00007fb3f9466000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9466000,
                         .vm_end = (app_pc)0x00007fb3f9468000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9468000,
                         .vm_end = (app_pc)0x00007fb3f946c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946c000,
                         .vm_end = (app_pc)0x00007fb3f946f000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f946f000,
                         .vm_end = (app_pc)0x00007fb3f966e000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966e000,
                         .vm_end = (app_pc)0x00007fb3f966f000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f966f000,
                         .vm_end = (app_pc)0x00007fb3f9670000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9670000,
                         .vm_end = (app_pc)0x00007fb3f9773000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9773000,
                         .vm_end = (app_pc)0x00007fb3f9972000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9972000,
                         .vm_end = (app_pc)0x00007fb3f9973000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9973000,
                         .vm_end = (app_pc)0x00007fb3f9974000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9974000,
                         .vm_end = (app_pc)0x00007fb3f9997000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b68000,
                         .vm_end = (app_pc)0x00007fb3f9b6a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b94000,
                         .vm_end = (app_pc)0x00007fb3f9b97000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b97000,
                         .vm_end = (app_pc)0x00007fb3f9b98000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b98000,
                         .vm_end = (app_pc)0x00007fb3f9b99000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fb3f9b99000,
                         .vm_end = (app_pc)0x00007fb3f9b9a000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d67f000,
                         .vm_end = (app_pc)0x00007fff4d6a1000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d1000,
                         .vm_end = (app_pc)0x00007fff4d6d4000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007fff4d6d4000,
                         .vm_end = (app_pc)0x00007fff4d6d6000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },

};

memquery_library_bounds_test test_616 = {
    .iters = results_616,
    .iters_count = 35,
    .in_start = (app_pc)0x000055d8f2343c63,
    .want_return = 4,
    .want_start = (app_pc)0x000055d8f209d000,
    .want_end = (app_pc)0x000055d8f26af000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_crash",
};

fake_memquery_result results_617[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce3a7000,
                         .vm_end = (app_pc)0x00005558ce73d000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce93c000,
                         .vm_end = (app_pc)0x00005558ce964000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce964000,
                         .vm_end = (app_pc)0x00005558ce983000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce983000,
                         .vm_end = (app_pc)0x00005558ce9b9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558cf7ce000,
                         .vm_end = (app_pc)0x00005558cf7ef000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45a000,
                         .vm_end = (app_pc)0x00007efebd45b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45b000,
                         .vm_end = (app_pc)0x00007efebdc5b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5b000,
                         .vm_end = (app_pc)0x00007efebdc5c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5c000,
                         .vm_end = (app_pc)0x00007efebe45c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45c000,
                         .vm_end = (app_pc)0x00007efebe45d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45d000,
                         .vm_end = (app_pc)0x00007efebec5d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5d000,
                         .vm_end = (app_pc)0x00007efebec5e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5e000,
                         .vm_end = (app_pc)0x00007efebf45e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf45e000,
                         .vm_end = (app_pc)0x00007efebf5f3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf5f3000,
                         .vm_end = (app_pc)0x00007efebf7f3000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f3000,
                         .vm_end = (app_pc)0x00007efebf7f7000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f7000,
                         .vm_end = (app_pc)0x00007efebf7f9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f9000,
                         .vm_end = (app_pc)0x00007efebf7fd000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7fd000,
                         .vm_end = (app_pc)0x00007efebf800000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf800000,
                         .vm_end = (app_pc)0x00007efebf9ff000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf9ff000,
                         .vm_end = (app_pc)0x00007efebfa00000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa00000,
                         .vm_end = (app_pc)0x00007efebfa01000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa01000,
                         .vm_end = (app_pc)0x00007efebfa19000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa19000,
                         .vm_end = (app_pc)0x00007efebfc18000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc18000,
                         .vm_end = (app_pc)0x00007efebfc19000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc19000,
                         .vm_end = (app_pc)0x00007efebfc1a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1a000,
                         .vm_end = (app_pc)0x00007efebfc1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1e000,
                         .vm_end = (app_pc)0x00007efebfd21000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfd21000,
                         .vm_end = (app_pc)0x00007efebff20000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff20000,
                         .vm_end = (app_pc)0x00007efebff21000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff21000,
                         .vm_end = (app_pc)0x00007efebff22000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff22000,
                         .vm_end = (app_pc)0x00007efebff45000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0111000,
                         .vm_end = (app_pc)0x00007efec0113000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec013d000,
                         .vm_end = (app_pc)0x00007efec0140000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0140000,
                         .vm_end = (app_pc)0x00007efec0143000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0143000,
                         .vm_end = (app_pc)0x00007efec0145000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0145000,
                         .vm_end = (app_pc)0x00007efec0146000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0146000,
                         .vm_end = (app_pc)0x00007efec0147000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0147000,
                         .vm_end = (app_pc)0x00007efec0148000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffed8b1b000,
                         .vm_end = (app_pc)0x00007ffed8b3d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },

};

memquery_library_bounds_test test_617 = {
    .iters = results_617,
    .iters_count = 40,
    .in_start = (app_pc)0x0000000000000000,
    .in_name = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/"
               "api.static_sideline_FLAKY",
    .want_return = 4,
    .want_start = (app_pc)0x00005558ce3a7000,
    .want_end = (app_pc)0x00005558ce9b9000,
};

fake_memquery_result results_618[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3a6000,
                         .vm_end = (app_pc)0x00005558ce3a7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce3a7000,
                         .vm_end = (app_pc)0x00005558ce73d000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce93c000,
                         .vm_end = (app_pc)0x00005558ce964000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce964000,
                         .vm_end = (app_pc)0x00005558ce983000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce983000,
                         .vm_end = (app_pc)0x00005558ce9b9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558cf7ce000,
                         .vm_end = (app_pc)0x00005558cf7ef000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45a000,
                         .vm_end = (app_pc)0x00007efebd45b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45b000,
                         .vm_end = (app_pc)0x00007efebdc5b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5b000,
                         .vm_end = (app_pc)0x00007efebdc5c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5c000,
                         .vm_end = (app_pc)0x00007efebe45c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45c000,
                         .vm_end = (app_pc)0x00007efebe45d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45d000,
                         .vm_end = (app_pc)0x00007efebec5d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5d000,
                         .vm_end = (app_pc)0x00007efebec5e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5e000,
                         .vm_end = (app_pc)0x00007efebf45e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf45e000,
                         .vm_end = (app_pc)0x00007efebf5f3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf5f3000,
                         .vm_end = (app_pc)0x00007efebf7f3000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f3000,
                         .vm_end = (app_pc)0x00007efebf7f7000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f7000,
                         .vm_end = (app_pc)0x00007efebf7f9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f9000,
                         .vm_end = (app_pc)0x00007efebf7fd000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7fd000,
                         .vm_end = (app_pc)0x00007efebf800000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf800000,
                         .vm_end = (app_pc)0x00007efebf9ff000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf9ff000,
                         .vm_end = (app_pc)0x00007efebfa00000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa00000,
                         .vm_end = (app_pc)0x00007efebfa01000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa01000,
                         .vm_end = (app_pc)0x00007efebfa19000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa19000,
                         .vm_end = (app_pc)0x00007efebfc18000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc18000,
                         .vm_end = (app_pc)0x00007efebfc19000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc19000,
                         .vm_end = (app_pc)0x00007efebfc1a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1a000,
                         .vm_end = (app_pc)0x00007efebfc1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1e000,
                         .vm_end = (app_pc)0x00007efebfd21000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfd21000,
                         .vm_end = (app_pc)0x00007efebff20000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff20000,
                         .vm_end = (app_pc)0x00007efebff21000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff21000,
                         .vm_end = (app_pc)0x00007efebff22000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff22000,
                         .vm_end = (app_pc)0x00007efebff45000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0111000,
                         .vm_end = (app_pc)0x00007efec0113000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec013d000,
                         .vm_end = (app_pc)0x00007efec0140000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0140000,
                         .vm_end = (app_pc)0x00007efec0143000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0143000,
                         .vm_end = (app_pc)0x00007efec0145000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0145000,
                         .vm_end = (app_pc)0x00007efec0146000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0146000,
                         .vm_end = (app_pc)0x00007efec0147000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0147000,
                         .vm_end = (app_pc)0x00007efec0148000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffed8b1b000,
                         .vm_end = (app_pc)0x00007ffed8b3d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },

};

memquery_library_bounds_test test_618 = {
    .iters = results_618,
    .iters_count = 41,
    .in_start = (app_pc)0x00005558ce965010,
    .want_return = 2,
    .want_start = (app_pc)0x00005558ce3a7000,
    .want_end = (app_pc)0x00005558ce9b9000,
};

fake_memquery_result results_619[100] = {
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3a6000,
                         .vm_end = (app_pc)0x00005558be3a7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3a7000,
                         .vm_end = (app_pc)0x00005558be3ad000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3ad000,
                         .vm_end = (app_pc)0x00005558be3af000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3af000,
                         .vm_end = (app_pc)0x00005558be3b9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3b9000,
                         .vm_end = (app_pc)0x00005558be3bf000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3bf000,
                         .vm_end = (app_pc)0x00005558be3c0000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3c0000,
                         .vm_end = (app_pc)0x00005558be3c7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3c7000,
                         .vm_end = (app_pc)0x00005558be3c8000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558be3c8000,
                         .vm_end = (app_pc)0x00005558ce3a7000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce3a7000,
                         .vm_end = (app_pc)0x00005558ce73d000,
                         .prot = 5,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000612000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce93c000,
                         .vm_end = (app_pc)0x00005558ce964000,
                         .prot = 1,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce964000,
                         .vm_end = (app_pc)0x00005558ce983000,
                         .prot = 3,
                         .comment = "/usr/local/google/home/chowski/dynamorio/build/"
                                    "suite/tests/bin/api.static_sideline_FLAKY" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558ce983000,
                         .vm_end = (app_pc)0x00005558ce9b9000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00005558cf7ce000,
                         .vm_end = (app_pc)0x00005558cf7ef000,
                         .prot = 3,
                         .comment = "[heap]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45a000,
                         .vm_end = (app_pc)0x00007efebd45b000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebd45b000,
                         .vm_end = (app_pc)0x00007efebdc5b000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5b000,
                         .vm_end = (app_pc)0x00007efebdc5c000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebdc5c000,
                         .vm_end = (app_pc)0x00007efebe45c000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45c000,
                         .vm_end = (app_pc)0x00007efebe45d000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebe45d000,
                         .vm_end = (app_pc)0x00007efebec5d000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5d000,
                         .vm_end = (app_pc)0x00007efebec5e000,
                         .prot = 0,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebec5e000,
                         .vm_end = (app_pc)0x00007efebf45e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf45e000,
                         .vm_end = (app_pc)0x00007efebf5f3000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000039f000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf5f3000,
                         .vm_end = (app_pc)0x00007efebf7f3000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f3000,
                         .vm_end = (app_pc)0x00007efebf7f7000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f7000,
                         .vm_end = (app_pc)0x00007efebf7f9000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libc-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7f9000,
                         .vm_end = (app_pc)0x00007efebf7fd000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf7fd000,
                         .vm_end = (app_pc)0x00007efebf800000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000204000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf800000,
                         .vm_end = (app_pc)0x00007efebf9ff000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebf9ff000,
                         .vm_end = (app_pc)0x00007efebfa00000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa00000,
                         .vm_end = (app_pc)0x00007efebfa01000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libdl-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa01000,
                         .vm_end = (app_pc)0x00007efebfa19000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x000000000021d000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfa19000,
                         .vm_end = (app_pc)0x00007efebfc18000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc18000,
                         .vm_end = (app_pc)0x00007efebfc19000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc19000,
                         .vm_end = (app_pc)0x00007efebfc1a000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libpthread-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1a000,
                         .vm_end = (app_pc)0x00007efebfc1e000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfc1e000,
                         .vm_end = (app_pc)0x00007efebfd21000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000304000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebfd21000,
                         .vm_end = (app_pc)0x00007efebff20000,
                         .prot = 0,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff20000,
                         .vm_end = (app_pc)0x00007efebff21000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff21000,
                         .vm_end = (app_pc)0x00007efebff22000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/libm-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efebff22000,
                         .vm_end = (app_pc)0x00007efebff45000,
                         .prot = 5,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000226000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0111000,
                         .vm_end = (app_pc)0x00007efec0113000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec013d000,
                         .vm_end = (app_pc)0x00007efec0140000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0140000,
                         .vm_end = (app_pc)0x00007efec0143000,
                         .prot = 1,
                         .comment = "[vvar]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0143000,
                         .vm_end = (app_pc)0x00007efec0145000,
                         .prot = 5,
                         .comment = "[vdso]" },
        .is_header = true,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000001000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0145000,
                         .vm_end = (app_pc)0x00007efec0146000,
                         .prot = 1,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0146000,
                         .vm_end = (app_pc)0x00007efec0147000,
                         .prot = 3,
                         .comment = "/lib/x86_64-linux-gnu/ld-2.24.so" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007efec0147000,
                         .vm_end = (app_pc)0x00007efec0148000,
                         .prot = 3,
                         .comment = "" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },
    {
        .iter_result = { .vm_start = (app_pc)0x00007ffed8b1b000,
                         .vm_end = (app_pc)0x00007ffed8b3d000,
                         .prot = 3,
                         .comment = "[stack]" },
        .is_header = false,
        .mod_base = (app_pc)0x0000000000000000,
        .mod_end = (app_pc)0x0000000000000000,
    },

};

memquery_library_bounds_test test_619 = {
    .iters = results_619,
    .iters_count = 49,
    .in_start = (app_pc)0x00005558ce64e074,
    .want_return = 4,
    .want_start = (app_pc)0x00005558ce3a7000,
    .want_end = (app_pc)0x00005558ce9b9000,
    .want_fulldir = "/usr/local/google/home/chowski/dynamorio/build/suite/tests/bin/",
    .want_filename = "api.static_sideline_FLAKY",
};

#define NUM_MEMQUERY_TESTS 63
memquery_library_bounds_test *all_memquery_tests[NUM_MEMQUERY_TESTS] = {
    &test_0,   &test_1,   &test_4,   &test_5,   &test_6,   &test_7,   &test_189,
    &test_489, &test_490, &test_491, &test_492, &test_504, &test_505, &test_506,
    &test_507, &test_508, &test_509, &test_510, &test_562, &test_563, &test_564,
    &test_565, &test_566, &test_577, &test_578, &test_579, &test_580, &test_583,
    &test_584, &test_585, &test_586, &test_587, &test_588, &test_589, &test_590,
    &test_592, &test_593, &test_594, &test_595, &test_596, &test_597, &test_598,
    &test_599, &test_600, &test_601, &test_602, &test_603, &test_604, &test_605,
    &test_606, &test_607, &test_608, &test_609, &test_610, &test_611, &test_612,
    &test_613, &test_614, &test_615, &test_616, &test_617, &test_618, &test_619,
};

const char *all_memquery_test_names[NUM_MEMQUERY_TESTS] = {
    "test_0",   "test_1",   "test_4",   "test_5",   "test_6",   "test_7",   "test_189",
    "test_489", "test_490", "test_491", "test_492", "test_504", "test_505", "test_506",
    "test_507", "test_508", "test_509", "test_510", "test_562", "test_563", "test_564",
    "test_565", "test_566", "test_577", "test_578", "test_579", "test_580", "test_583",
    "test_584", "test_585", "test_586", "test_587", "test_588", "test_589", "test_590",
    "test_592", "test_593", "test_594", "test_595", "test_596", "test_597", "test_598",
    "test_599", "test_600", "test_601", "test_602", "test_603", "test_604", "test_605",
    "test_606", "test_607", "test_608", "test_609", "test_610", "test_611", "test_612",
    "test_613", "test_614", "test_615", "test_616", "test_617", "test_618", "test_619",
};
