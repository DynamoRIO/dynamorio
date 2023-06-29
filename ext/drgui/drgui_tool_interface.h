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

/* drgui_tool_interface.h
 *
 * Defines the structures through which tools will interface with the
 * main window.
 * Anything needed by most or all tools should be defined here.
 */

#include <QWidget>
#include <QtPlugin>
#include <QFile>

#ifndef DRGUI_TOOL_INTERFACE_H
#    define DRGUI_TOOL_INTERFACE_H

#    include "drgui_options_interface.h"

namespace dynamorio {
namespace drgui {

/**
 * Interface for drgui to interact with its loaded tools.
 * drgui discovers tools through the QPluginLoader system, which attempts
 * to load each of the tools it has been pointed at by the 'Load Tools' action.
 */
class drgui_tool_interface_t : public QWidget {
    Q_OBJECT
public:
    /**
     * Returns a list of the names of the tools that are to be provided.
     * The names are used for titles in the 'Tools' menu and in the tab widget.
     * They are also used to keep track of which plugins are already loaded,
     * and must be unique.
     */
    virtual QStringList
    tool_names() const = 0;

    /**
     * Returns a new instance of a tool to be displayed by drgui's
     * tab interface. This is called by drgui when a user requests a new
     * tab by clicking on the tool's action in the 'Tools' menu.
     * Arguments can be optionally supplied to the tool.
     */
    virtual QWidget *
    create_instance(const QStringList &args = QStringList()) = 0;

    /**
     * Returns an instance of the tools options page to be displayed
     * by drgui's preferences dialog. This is called by drgui's preferences
     * dialog the first time it disovers the tool on each run of drgui.
     */
    virtual drgui_options_interface_t *
    create_options_page(void) = 0;

    /**
     * Used by drgui to tell a tool to open a file at a line number
     * XXX i#1251: Currently only used as a bridge from tools to the
     * code editor plugin.
     */
    virtual void
    open_file(const QString &path, int line_num) = 0;

signals:
    /**
     * Allows a tool to tell drgui to open the given widget in a new tab
     * with the given name as a label.
     * Signals do not have implementations, therefore it does not make sense
     * to declare a signal as virtual.
     */
    void
    new_instance_requested(QWidget *tool, QString tool_name);
};

#    define DrGUI_ToolInterface_iid "DynamoRIO.DrGUI.ToolInterface"

} // namespace drgui
} // namespace dynamorio

Q_DECLARE_INTERFACE(dynamorio::drgui::drgui_tool_interface_t, DrGUI_ToolInterface_iid)

#endif
