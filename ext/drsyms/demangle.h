// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Satoru Takabayashi
//
// Retreived from http://code.google.com/p/google-glog/ at r91.
//
// An async-signal-safe and thread-safe demangler for Itanium C++ ABI
// (aka G++ V3 ABI).

// The demangler is implemented to be used in async signal handlers to
// symbolize stack traces.  We cannot use libstdc++'s
// abi::__cxa_demangle() in such signal handlers since it's not async
// signal safe (it uses malloc() internally).
//
// Note that this demangler doesn't support full demangling.  More
// specifically, it doesn't print types of function parameters and
// types of template arguments.  It just skips them.  However, it's
// still very useful to extract basic information such as class,
// function, constructor, destructor, and operator names.
//
// See the implementation note in demangle.cc if you are interested.
//
// Example:
//
// | Mangled Name  | The Demangler | abi::__cxa_demangle()
// |---------------|---------------|-----------------------
// | _Z1fv         | f()           | f()
// | _Z1fi         | f()           | f(int)
// | _Z3foo3bar    | foo()         | foo(bar)
// | _Z1fIiEvi     | f<>()         | void f<int>(int)
// | _ZN1N1fE      | N::f          | N::f
// | _ZN3Foo3BarEv | Foo::Bar()    | Foo::Bar()
// | _Zrm1XS_"     | operator%()   | operator%(X, X)
// | _ZN3FooC1Ev   | Foo::Foo()    | Foo::Foo()
// | _Z1fSs        | f()           | f(std::basic_string<char,
// |               |               |   std::char_traits<char>,
// |               |               |   std::allocator<char> >)
//
// See the unit test for more examples.
//
// Note: we might want to write demanglers for ABIs other than Itanium
// C++ ABI in the future.
//

#ifndef BASE_DEMANGLE_H_
#define BASE_DEMANGLE_H_

/* Modifications for use in drsyms:
 * - Deleted #include "config.h".
 * - Defined away _START_GOOGLE_NAMESPACE_ and _END_GOOGLE_NAMESPACE_ macros.
 * - Added extern "C" to call from C.
 * - demangle.h uses C++ // comments, but does not need to compile as C89 under
 *   MSVC, so they were left as-is.
 * - Do not add "()" when removing parameters.
 */
#define _START_GOOGLE_NAMESPACE_
#define _END_GOOGLE_NAMESPACE_

_START_GOOGLE_NAMESPACE_

#ifdef __cplusplus
extern "C" {
#endif

// Options accepted by Demangle.
enum {
    DEMANGLE_DEFAULT = 0x00,        //< Replace templates with <> and remove overloads.
    DEMANGLE_KEEP_TEMPLATES = 0x02, //< Do not strip template arguments.
    DEMANGLE_KEEP_OVERLOADS = 0x04, //< Do not strip function parameter types.
};

// Demangle "mangled".  On success, return the length required to store the
// fully demangled name.  If the return value is greater than out_size, the
// output is truncated and nul-terminated.  If the demangling fails, return
// zero.  "out" is modified even if demangling is unsuccessful.
int
Demangle(const char *mangled, char *out, int out_size, unsigned short options);

#ifdef __cplusplus
} /* extern "C" */
#endif

_END_GOOGLE_NAMESPACE_

#endif // BASE_DEMANGLE_H_
