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

/* drgui_options_interface.h
 *
 * Defines the interface for options pages for tools
 */

#ifndef DRGUI_OPTIONS_INTERFACE_H
#define DRGUI_OPTIONS_INTERFACE_H

#include <QWidget>

namespace dynamorio {
namespace drgui {

/**
 * Interface for tools' options pages that are to be accessed by drgui's
 * preferences dialog. The page will be accessed by drgui's preferences dialog
 * through drgui_tool_interface_t's create_options_page() method.
 */
class drgui_options_interface_t : public QWidget {
    Q_OBJECT
public:
    /**
     * Returns a list of the names of the tools that are supported by the
     * options page.
     * The names are used for accessing the different options pages
     * via the list widget in drgui's preferences dialog, and must be unique.
     */
    virtual QStringList
    tool_names(void) const = 0;

    /**
     * Writes the settings for the options page.
     * Called by drgui_options_window_t for each tool when the save button is pressed.
     * The tool will decide how it wants to handle its settings' persistence.
     */
    virtual void
    write_settings(void) = 0;

    /**
     * Reads the settings for the options page.
     * Called by drgui_options_window_t for each tool on load, and when the cancel
     * button is pressed.
     * The tool will decide how it wants to handle its settings' persistence.
     */
    virtual void
    read_settings(void) = 0;
};

#define DrGUI_OptionsInterface_iid "DynamoRIO.DrGUI.OptionsInterface"

} // namespace drgui
} // namespace dynamorio

Q_DECLARE_INTERFACE(dynamorio::drgui::drgui_options_interface_t,
                    DrGUI_OptionsInterface_iid)

#endif
