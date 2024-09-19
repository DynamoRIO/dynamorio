/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/* C++ sample client:
 * stl_test.cpp
 *
 * Some simple tests with C++ STL containers to make sure they
 * work with a DR client.
 */

#include "dr_api.h"
#include <vector>
#include <list>
#include <map>
#include <iostream>

#ifdef UNIX
/* included as a test of i#34 */
#    include <signal.h>
#endif

namespace dynamorio {
namespace samples {
namespace {

#if defined(UNIX) && defined(SHOW_RESULTS)
__thread int tls_var;
#endif

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
#    ifdef UNIX
    std::cout << "value of tls_var on exit: " << tls_var << "\n";
#    endif
    std::cout << "Exit..."
              << "\n";
#endif
}

} // namespace
} // namespace samples
} // namespace dynamorio

DR_EXPORT void
dr_init(client_id_t client_id)
{
    int i;
#ifdef SHOW_RESULTS
    bool success = true;
#endif

    dr_set_client_name("DynamoRIO Sample Client 'stl_test'",
                       "http://dynamorio.org/issues");

#ifdef SHOW_RESULTS
    std::cout << "Start..."
              << "\n";
#endif
    dr_register_exit_event(dynamorio::samples::event_exit);
#if defined(UNIX) && defined(SHOW_RESULTS)
    std::cout << "input a tls value"
              << "\n";
    std::cin >> dynamorio::samples::tls_var;
    std::cout << "Set tls var to " << dynamorio::samples::tls_var << "\n";
#endif

    //
    // Put values in a vector and read them out
    //
#ifdef SHOW_RESULTS
    std::cout << "testing vector...";
#endif

    std::vector<int> *v = new std::vector<int>();
    for (i = 0; i < 5; i++) {
        v->push_back(i);
    }

    for (i = 0; i < 5; i++) {
#ifdef SHOW_RESULTS
        std::cout << (*v)[i];
        if ((*v)[i] != i) {
            success = false;
        }
#endif
    }
    delete v;

    //
    // Put values in a list and read them out
    //
#ifdef SHOW_RESULTS
    std::cout << "\ntesting list...";
#endif

    std::list<int> l;
    for (i = 0; i < 5; i++) {
        l.push_back(i);
    }

    i = 0;
    for (std::list<int>::iterator l_iter = l.begin(); l_iter != l.end(); l_iter++) {
#ifdef SHOW_RESULTS
        std::cout << *l_iter;
        if (*l_iter != i) {
            success = false;
        }
#endif
        i++;
    }

    //
    // Put values in a map and read them out
    //
#ifdef SHOW_RESULTS
    std::cout << "\ntesting map...";
#endif

    std::map<int, int> m;
    for (i = 0; i < 5; i++) {
        m[i] = i;
    }

    for (i = 0; i < 5; i++) {
#ifdef SHOW_RESULTS
        std::cout << m[i];
        if (m[i] != i) {
            success = false;
        }
#endif
    }

    //
    // Done; print summary
    //
#ifdef SHOW_RESULTS
    if (success) {
#    ifdef WINDOWS
        dr_messagebox("SUCCESS");
#    else
        std::cout << "\nSUCCESS\n";
#    endif
    } else {
#    ifdef WINDOWS
        dr_messagebox("FAILURE");
#    else
        std::cout << "\nFAILURE\n";
#    endif
    }
#endif
}
