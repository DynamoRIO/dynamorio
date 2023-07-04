/* ***************************************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2013 Branden Clark  All rights reserved.
 * ***************************************************************************/

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

/* main.cpp
 *
 * Starts the app
 */

#include <QApplication>
#include <iostream>

#include "drgui_main_window.h"

namespace dynamorio {
namespace drgui {

struct tool_data_t {
    QString name;
    QStringList arguments;
};

static tool_data_t *
process_arguments(int argc, char *argv[]);
static void
print_usage(char *arg_0);

tool_data_t *
process_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        /* Help */
        if (QString(argv[i]) == QString("-h")) {
            print_usage(argv[0]);
            exit(0);
        }
        /* Auto load a tool */
        if (QString(argv[i]) == QString("-t")) {
            if (++i >= argc) {
                print_usage(argv[0]);
                exit(1);
            }
            tool_data_t *tool = new tool_data_t;
            tool->name = argv[i];
            /* Get arguments to pass to the tool */
            while (++i < argc) {
                tool->arguments << argv[i];
            }
            return tool;
        }
    }
    return NULL;
}

void
print_usage(char *arg_0)
{
    std::cout << "usage " << arg_0 << " [options]" << std::endl;
    std::cout << "options:" << std::endl;
    printf("  %-40s%s", "-h", "Display option list");
    std::cout << std::endl;
    printf("  %-40s%s", "-t <tool_name> [options]",
           "Automatically load a tool with optional arguments");
    std::cout << std::endl;
}

} // namespace drgui
} // namespace dynamorio

int
main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    dynamorio::drgui::tool_data_t *tool = dynamorio::drgui::process_arguments(argc, argv);
    QString tool_name = "";
    QStringList tool_args;
    if (tool != NULL) {
        tool_name = tool->name;
        tool_args = tool->arguments;
        delete tool;
    }
    dynamorio::drgui::drgui_main_window_t main_win(tool_name, tool_args);
    main_win.show();
    return app.exec();
}
