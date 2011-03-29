/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

#include "tools.h"
#ifdef LINUX
# include "dlfcn.h"
#endif

int
main(int argc, char *argv[])
{
#ifdef WINDOWS
    HANDLE lib = LoadLibrary("client.drwrap-test.appdll.dll");
    if (lib == NULL) {
        print("error loading library\n");
    } else {
        print("loaded library\n");
        FreeLibrary(lib);
    }
#else
    /* We don't have "." on LD_LIBRARY_PATH path so we take in abs path */
    void *lib;
    if (argc < 2) {
        print("need to pass in lib path\n");
        return 1;
    }
    lib = dlopen(argv[1], RTLD_LAZY|RTLD_LOCAL);
    if (lib == NULL) {
        print("error loading library %s: %s\n", argv[1], dlerror());
    } else {
        print("loaded library\n");
	dlclose(lib);
    }
#endif
    print("thank you for testing the client interface\n");
    return 0;
}
