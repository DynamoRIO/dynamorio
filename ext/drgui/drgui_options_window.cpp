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

/* drgui_options_window.cpp
 * 
 * Provides a main interface for users to adjust options for tools
 */

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "drgui_options_window_t::"

#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QFont>
#include <QAction>
#include <QActionGroup>
#include <QDebug>

#include "drgui_options_window.h"
#include "drgui_options_interface.h"

/* Public
 * Constructor
 */
drgui_options_window_t::drgui_options_window_t(QActionGroup *tool_group_)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    tool_action_group = tool_group_;

    /* Will list the tool option pages available */
    tool_page_list = new QListWidget(this);
    tool_page_list->setViewMode(QListView::IconMode);
    tool_page_list->setIconSize(QSize(96, 84));
    tool_page_list->setMovement(QListView::Static);
    tool_page_list->setMaximumWidth(140);
    tool_page_list->setSpacing(12);

    tool_page_stack = new QStackedWidget(this);

    save_button = new QPushButton(tr("Save"));
    connect(save_button, SIGNAL(clicked()),
            this, SLOT(save()));
    
    reset_button = new QPushButton(tr("Reset"), this);
    connect(reset_button, SIGNAL(clicked()), 
            this, SLOT(cancel()));

    cancel_button = new QPushButton(tr("Cancel"), this);
    connect(cancel_button, SIGNAL(clicked()),
            this, SLOT(cancel()));

    horizontal_layout = new QHBoxLayout;
    horizontal_layout->addWidget(tool_page_list);
    horizontal_layout->addWidget(tool_page_stack, 1);

    buttons_layout = new QHBoxLayout;
    buttons_layout->addStretch(1);
    buttons_layout->addWidget(save_button);
    buttons_layout->addWidget(reset_button);
    buttons_layout->addWidget(cancel_button);

    main_layout = new QVBoxLayout;
    main_layout->addLayout(horizontal_layout);
    main_layout->addSpacing(12);
    main_layout->addLayout(buttons_layout);
    setLayout(main_layout);

    setWindowTitle(tr("Preferences"));
}

/* Private
 * Adds the tools to the list
 */
void
drgui_options_window_t::create_tool_list(void)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    QFont list_font;
    list_font.setPointSize(12);
    list_font.setBold(true);

    /* XXX i#1251: Get tools from plugin system */

    connect(tool_page_list,
            SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            this, SLOT(change_page(QListWidgetItem*, QListWidgetItem*)));
}

/* Private
 * Changes the viewed page in tool_page_stack
 */
void
drgui_options_window_t::change_page(QListWidgetItem *current,
                                    QListWidgetItem *previous)
{
    if (!current) 
        current = previous;
    tool_page_stack->setCurrentIndex(tool_page_list->row(current));
}

/* Private
 * Saves all data for all tools
 * XXX i#1251: use "changed" variable
 */
void
drgui_options_window_t::save(void)
{
    for (int i = 0; i < tool_page_stack->count(); i++) {
        /* XXX i#1251: write settings for each tool */
    }
}

/* Public
 * Refreshes and displays the options window
 */
void
drgui_options_window_t::display(void)
{
    create_tool_list();
    tool_page_list->setCurrentRow(0);
    exec();
}

/* Public Slot
 * Reverts un-saved changes
 */
void
drgui_options_window_t::cancel(void)
{
    for (int i = 0; i < tool_page_stack->count(); i++) {
        /* XXX i#1251: read settings for each tool */
    }
    if (sender() == cancel_button)
        close();
}
