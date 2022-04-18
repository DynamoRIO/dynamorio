/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "configure.h"
#if defined(UNIX) || defined(_MSC_VER)
#    include "tools.h"
#else /* cygwin/mingw */
#    include <windows.h>
#    include <stdio.h>
#    include <assert.h>
#    define print(...) fprintf(stderr, __VA_ARGS__)
/* we want PE exports so dr_get_proc_addr finds them */
#    define EXPORT __declspec(dllexport)
#    define NOINLINE __declspec(noinline)
#endif

#include <new> /* for std::nothrow_t */

#ifdef UNIX
#    include "dlfcn.h"
#endif

static int (*dll_export)(int);

/* Put the class in an anonymous namespace to get the same effect as C static.
 * We want to make sure we can find the debug info even if it's not in
 * .debug_pubnames.  __attribute__((visibility("hidden"))) isn't the same as C
 * static, it just means it's not exported when linked into a DSO.
 */
namespace {
class Foo {
public:
    int NOINLINE
    Bar(int a);
};
class HasFields {
public:
    int x;
    bool y;
    short z;
    char a[4];
};
}

int NOINLINE
Foo::Bar(int a)
{
    return dll_export(a + 1);
}

extern "C" EXPORT int
exe_export(int a)
{
    Foo f;
    return f.Bar(a + 1);
}

NOINLINE int
exe_public(int a)
{
    return exe_export(a + 1);
}

static NOINLINE int
exe_static(int a)
{
    return exe_public(a + 1);
}

int
overloaded(char *a)
{
    return 1;
}
int
overloaded(wchar_t *a)
{
    return 2;
}
int
overloaded(int *a)
{
    return 4;
}
int
overloaded(void *a)
{
    return 8;
}
int
overloaded(Foo *a)
{
    return 16;
}
int
overloaded(HasFields *a)
{
    return 32;
}
/* test an empty struct */
int
overloaded(std::nothrow_t *a)
{
    return 64;
}
/* no arg so not really an overload, but we need to test no-arg func */
int
overloaded(void)
{
    return 128;
}

template <typename T>
T *
templated_func(T *t)
{
    return t;
}

/* test some nesting */
namespace name_outer {
namespace name_middle {
    namespace name_inner {
        template <typename X> class sample_class {
        public:
            template <typename Y> class nested_class {
            public:
                template <typename T>
                T *
                templated_func(T *t)
                {
                    return t;
                }
                union {
                    int zz;
                };
            };
        };
    }
}
}

void
test_templates(void)
{
    int x = 4;
    int *y = templated_func<int>(&x);
    print("got back %d\n", *y);
    char c = 'x';
    name_outer::name_middle::name_inner::sample_class<char>::nested_class<int> sc;
    y = sc.templated_func<int>(&x);
    print("got back %d\n", *y);
}

int
main(int argc, char **argv)
{
    int num_calls;
#ifdef WINDOWS
    HMODULE lib;
#else
    void *lib;
#endif
    /* Get appdll path. */
    if (argc < 2) {
        print("need to pass in appdll path.\n");
        return 1;
    }
#ifdef WINDOWS
    lib = LoadLibrary(argv[1]);
    if (lib == NULL) {
        print("error loading library %s\n", argv[1]);
    } else {
        dll_export = (int (*)(int))GetProcAddress(lib, "dll_export");
    }
#else
    lib = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
    dll_export = (int (*)(int))dlsym(lib, "dll_export");
#endif
    assert(dll_export != NULL);

    /* Call a function which calls other functions so we can test looking up the
     * PCs from the stack trace.
     */
    num_calls = exe_static(0);

    print("overloaded: %d\n", overloaded((char *)NULL));
    print("overloaded: %d\n", overloaded((wchar_t *)NULL));
    print("overloaded: %d\n", overloaded((int *)NULL));

#ifdef WINDOWS
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif

    print("app num_calls: %d\n", num_calls);

    test_templates();

    return 0;
}
