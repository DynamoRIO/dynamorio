/* **********************************************************
 * Copyright (c) 2013, Branden Clark All rights reserved.
 * **********************************************************/

/* Dr. GUI
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the conditions outlined in
 * the BSD 2-Clause license are met.
 
 * This software is provided by the copyright holders and contributors "AS IS"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. See the BSD 2-Clause license for more details.
 */

/* drgui_options_interface.h
 * 
 * Defines the interface for options pages for tools
 */

#ifndef DRGUI_OPTIONS_INTERFACE_H
#define DRGUI_OPTIONS_INTERFACE_H

#include <QWidget>

/**
 * Interface for tools' options pages that are to be accessed by drgui's 
 * preferences dialog. The page will be accessed by drgui's preferences dialog 
 * through drgui_tool_interface_t's create_options_page() method.
 */
class drgui_options_interface_t : public QWidget
{
    Q_OBJECT
public:
    /**
     * Returns a list of the names of the tools that are supported by the
     * options page.
     * The names are used for accessing the different options pages
     * via the list widget in drgui's preferences dialog, and must be unique.
     */
    virtual
    QStringList tool_names(void) const = 0;

    /**
     * Writes the settings for the options page.
     * Called by drgui_options_window_t for each tool when the save button is pressed.
     * The tool will decide how it wants to handle its settings' persistence.
     */
    virtual
    void write_settings(void) = 0;

    /**
     * Reads the settings for the options page.
     * Called by drgui_options_window_t for each tool on load, and when the cancel
     * button is pressed.
     * The tool will decide how it wants to handle its settings' persistence.
     */
    virtual
    void read_settings(void) = 0;

};

#define DrGUI_OptionsInterface_iid "DynamoRIO.DrGUI.OptionsInterface"

Q_DECLARE_INTERFACE(drgui_options_interface_t, DrGUI_OptionsInterface_iid)

#endif
