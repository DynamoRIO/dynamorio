/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* Represent an object for loading dynamic library and getting export function by symbol
 * name.
 */

#ifndef _DYNAMIC_LIB_H_
#define _DYNAMIC_LIB_H_ 1

#ifdef UNIX
#    include <dlfcn.h>
#elif WINDOWS
#    include <windows.h>
#endif
#include <stdexcept>

namespace dynamorio {
namespace drmemtrace {

class dynamic_lib_t {
public:
    dynamic_lib_t(const std::string &filename);
    dynamic_lib_t(const dynamic_lib_t &) = delete;
    dynamic_lib_t(dynamic_lib_t &&);
    dynamic_lib_t &
    operator=(const dynamic_lib_t &) = delete;
    dynamic_lib_t &
    operator=(dynamic_lib_t &&);
    virtual ~dynamic_lib_t();
    virtual std::string
    error();

protected:
    template <typename T>
    T
    get_export(const std::string &symbol) const
    {
#ifdef UNIX
        static_cast<void>(dlerror());
        return reinterpret_cast<T>(dlsym(handle_, symbol.c_str()));
#elif WINDOWS
        return reinterpret_cast<T>(
            GetProcAddress(reinterpret_cast<HMODULE>(handle_), symbol.c_str()));
#endif
    }

protected:
    void *handle_;
    std::string error_string_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif // _DYNAMIC_LIB_H_