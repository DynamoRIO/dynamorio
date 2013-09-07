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
#define DRGUI_TOOL_INTERFACE_H

#include "drgui_options_interface.h"

/**
 * Interface for drgui to interact with its loaded tools.
 * drgui discovers tools through the QPluginLoader system, which attempts
 * to load each of the tools it has been pointed at by the 'Load Tools' action.
 */
class drgui_tool_interface_t : public QWidget
{
    Q_OBJECT
public:
    /**
     * Returns a list of the names of the tools that are to be provided.
     * The names are used for titles in the 'Tools' menu and in the tab widget.
     * They are also used to keep track of which plugins are already loaded,
     * and must be unique.
     */
    virtual QStringList tool_names() const = 0;

    /**
     * Returns a new instance of a tool to be displayed by drgui's
     * tab interface. This is called by drgui when a user requests a new
     * tab by clicking on the tool's action in the 'Tools' menu.
     */
    virtual QWidget *create_instance(void) = 0;

    /**
     * Returns an instance of the tools options page to be displayed
     * by drgui's preferences dialog. This is called by drgui's preferences 
     * dialog the first time it disovers the tool on each run of drgui.
     */
    virtual drgui_options_interface_t *create_options_page(void) = 0;

    /**
     * Used by drgui to tell a tool to open a file at a line number.
     * XXX i#1251: Currently only used as a bridge from tools to the
     * code editor plugin. 
     */
    virtual void open_file(const QString &path, int line_num) = 0;
};

#define DrGUI_ToolInterface_iid "DynamoRIO.DrGUI.ToolInterface"

Q_DECLARE_INTERFACE(drgui_tool_interface_t, DrGUI_ToolInterface_iid)

#endif
