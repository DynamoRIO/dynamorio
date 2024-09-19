/* ***************************************************************************
 * Copyright (c) 2020-2023 Google, Inc.  All rights reserved.
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

/* drgui_main_window.h
 *
 * Defines a main structure for users to interface with tools.
 */

#ifndef DRGUI_MAIN_WINDOW_H
#define DRGUI_MAIN_WINDOW_H

#include <QDir>
#include <QMainWindow>
#include <QStringList>

class OptionBase;
class tool_base_t;
class QAction;
class QMenu;
class QMdiArea;
class QMdiSubWindow;
class QActionGroup;
class QPluginLoader;

namespace dynamorio {
namespace drgui {

class drgui_options_window_t;
class drgui_tool_interface_t;

class drgui_main_window_t : public QMainWindow {
    Q_OBJECT

public:
    drgui_main_window_t(QString tool_name, QStringList tool_args);
    ~drgui_main_window_t(void);

protected:
    void
    closeEvent(QCloseEvent *event) override;

private slots:
    void
    about(void);

    void
    update_menus(void);

    void
    update_window_menu(void);

    void
    switch_layout_direction(void);

    void
    maybe_close_me(void);

    void
    maybe_close(int index);

    void
    hide_tab(int index);

    void
    close_all_tabs(void);

    void
    activate_next_tab(void);

    void
    activate_previous_tab(void);

    void
    show_preferences_dialog(void);

    void
    add_tab(void);

    void
    add_tab(drgui_tool_interface_t *factory, const QStringList &args);

    void
    add_tool(void);

    void
    new_tool_instance(QWidget *tool, QString tool_name);

private:
    void
    create_actions(void);

    void
    create_menus(void);

    void
    create_status_bar(void);

    void
    read_settings(void);

    void
    write_settings(void);

    QWidget *
    active_tool(void);

    void
    load_tools(void);

    void
    add_tool_to_menu(QObject *plugin, const QStringList &texts, const char *member,
                     QActionGroup *action_group);

    /* GUI */
    QDir plugins_dir;
    QStringList plugin_names;
    QTabWidget *tab_area;
    QActionGroup *tool_action_group;
    QAction *separator_act;

    QMenu *file_menu;
    /* Switch layout act */
    QAction *exit_act;

    QMenu *edit_menu;
    QAction *preferences_act;
    drgui_options_window_t *opt_win;

    QMenu *window_menu;
    QAction *close_act;
    QAction *close_all_act;
    QAction *next_act;
    QAction *previous_act;

    QMenu *help_menu;
    QAction *about_act;
    QAction *about_qt_act;

    QMenu *tool_menu;
    QAction *load_tools_act;

    /* Data */
    QVector<drgui_tool_interface_t *> plugins;
    QString tool_to_auto_load;
    QStringList tool_to_auto_load_args;

    /* Options */
    QString custom_command_format;
    QList<QString> tool_files;
};

} // namespace drgui
} // namespace dynamorio

#endif
