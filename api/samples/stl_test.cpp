/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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

using namespace std;

#ifdef UNIX
__thread int tls_var;
#endif

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
#    ifdef UNIX
    cout << "value of tls_var on exit: " << tls_var << endl;
#    endif
    cout << "Exit..." << endl;
#endif
}

DR_EXPORT void
dr_init(client_id_t client_id)
{
    int i;
    bool success = true;

    dr_set_client_name("DynamoRIO Sample Client 'stl_test'",
                       "http://dynamorio.org/issues");

#ifdef SHOW_RESULTS
    cout << "Start..." << endl;
#endif
    dr_register_exit_event(event_exit);
#if defined(UNIX) && defined(SHOW_RESULTS)
    cout << "input a tls value" << endl;
    cin >> tls_var;
    cout << "Set tls var to " << tls_var << endl;
#endif

    //
    // Put values in a vector and read them out
    //
#ifdef SHOW_RESULTS
    cout << "testing vector...";
#endif

    vector<int> *v = new vector<int>();
    for (i = 0; i < 5; i++) {
        v->push_back(i);
    }

    for (i = 0; i < 5; i++) {
#ifdef SHOW_RESULTS
        cout << (*v)[i];
#endif
        if ((*v)[i] != i) {
            success = false;
        }
    }
    delete v;

    //
    // Put values in a list and read them out
    //
#ifdef SHOW_RESULTS
    cout << "\ntesting list...";
#endif

    list<int> l;
    for (i = 0; i < 5; i++) {
        l.push_back(i);
    }

    i = 0;
    for (list<int>::iterator l_iter = l.begin(); l_iter != l.end(); l_iter++) {
#ifdef SHOW_RESULTS
        cout << *l_iter;
#endif
        if (*l_iter != i) {
            success = false;
        }
        i++;
    }

    //
    // Put values in a map and read them out
    //
#ifdef SHOW_RESULTS
    cout << "\ntesting map...";
#endif

    map<int, int> m;
    for (i = 0; i < 5; i++) {
        m[i] = i;
    }

    for (i = 0; i < 5; i++) {
#ifdef SHOW_RESULTS
        cout << m[i];
#endif
        if (m[i] != i) {
            success = false;
        }
    }

    //
    // Done; print summary
    //
#ifdef SHOW_RESULTS
    if (success) {
#    ifdef WINDOWS
        dr_messagebox("SUCCESS");
#    else
        cout << "\nSUCCESS\n";
#    endif
    } else {
#    ifdef WINDOWS
        dr_messagebox("FAILURE");
#    else
        cout << "\nFAILURE\n";
#    endif
    }
#endif
}
