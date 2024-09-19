/* ***************************************************************************
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

/* drgui_options_window.h
 *
 * Defines a main interface for users to adjust options for tools
 */

#ifndef DRGUI_OPTIONS_WINDOW_H
#define DRGUI_OPTIONS_WINDOW_H

#include <QDialog>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QHBoxLayout;
class QVBoxLayout;
class QActionGroup;

namespace dynamorio {
namespace drgui {

class drgui_options_window_t : public QDialog {
    Q_OBJECT

public:
    drgui_options_window_t(QActionGroup *tool_group_);

    void
    display(void);

public slots:
    void
    change_page(QListWidgetItem *current, QListWidgetItem *previous);

private slots:
    void
    save(void);

    void
    cancel(void);

private:
    void
    create_tool_list(void);

    void
    read_settings(void);

    /* GUI */
    QVBoxLayout *main_layout;
    QHBoxLayout *horizontal_layout;
    QListWidget *tool_page_list;
    QStackedWidget *tool_page_stack;

    QHBoxLayout *buttons_layout;
    QPushButton *save_button;
    QPushButton *reset_button;
    QPushButton *cancel_button;

    QActionGroup *tool_action_group;
};

} // namespace drgui
} // namespace dynamorio

#endif
