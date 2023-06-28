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

/* drgui_options_window.cpp
 *
 * Provides a main interface for users to adjust options for tools
 */

#ifdef __CLASS__
#    undef __CLASS__
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

#include "drgui_tool_interface.h"
#include "drgui_options_interface.h"
#include "drgui_options_window.h"

namespace dynamorio {
namespace drgui {

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
    connect(save_button, SIGNAL(clicked()), this, SLOT(save()));

    reset_button = new QPushButton(tr("Reset"), this);
    connect(reset_button, SIGNAL(clicked()), this, SLOT(cancel()));

    cancel_button = new QPushButton(tr("Cancel"), this);
    connect(cancel_button, SIGNAL(clicked()), this, SLOT(cancel()));

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

    drgui_tool_interface_t *i_tool;
    foreach (QAction *action, tool_action_group->actions()) {
        i_tool = qobject_cast<drgui_tool_interface_t *>(action->parent());
        /* Skip if already added */
        if (!tool_page_list->findItems(i_tool->tool_names().first(), Qt::MatchExactly)
                 .isEmpty())
            continue;
        tool_page_stack->addWidget(i_tool->create_options_page());
        QListWidgetItem *config_button = new QListWidgetItem(tool_page_list);
        config_button->setText(i_tool->tool_names().first());
        config_button->setFont(list_font);
        config_button->setTextAlignment(Qt::AlignHCenter);
        config_button->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }

    connect(tool_page_list,
            SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this,
            SLOT(change_page(QListWidgetItem *, QListWidgetItem *)));
}

/* Private
 * Changes the viewed page in tool_page_stack
 */
void
drgui_options_window_t::change_page(QListWidgetItem *current, QListWidgetItem *previous)
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
        drgui_options_interface_t *i_opt;
        i_opt = qobject_cast<drgui_options_interface_t *>(tool_page_stack->widget(i));
        i_opt->write_settings();
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
        drgui_options_interface_t *i_opt;
        i_opt = qobject_cast<drgui_options_interface_t *>(tool_page_stack->widget(i));
        i_opt->read_settings();
    }
    if (sender() == cancel_button)
        close();
}

} // namespace drgui
} // namespace dynamorio
