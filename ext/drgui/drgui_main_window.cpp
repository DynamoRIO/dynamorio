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

/* drgui_main_window.cpp
 *
 * Provides a main structure for users to interface with tools.
 */

#ifdef __CLASS__
#    undef __CLASS__
#endif
#define __CLASS__ "drgui_main_window_t::"

#include <QMenu>
#include <QAction>
#include <QString>
#include <QTabWidget>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QActionGroup>
#include <QSettings>
#include <QPluginLoader>
#include <QStatusBar>
#include <QMenuBar>
#include <QCloseEvent>
#include <QDebug>
#include <QApplication>

#include <cassert>

#include "drgui_tool_interface.h"
#include "drgui_options_window.h"
#include "drgui_main_window.h"

namespace dynamorio {
namespace drgui {

/* Public
 * Constructor, everything begins here
 */
drgui_main_window_t::drgui_main_window_t(QString tool_name, QStringList tool_args)
    : tool_to_auto_load(tool_name)
    , tool_to_auto_load_args(tool_args)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    tab_area = new QTabWidget(this);
    tab_area->setTabsClosable(true);
    tab_area->setMovable(true);
    connect(tab_area, SIGNAL(tabCloseRequested(int)), this, SLOT(maybe_close(int)));
    setCentralWidget(tab_area);

    connect(tab_area, SIGNAL(currentChanged(int)), this, SLOT(update_menus()));

    create_actions();
    create_menus();
    create_status_bar();
    update_menus();

    opt_win = new drgui_options_window_t(tool_action_group);
    read_settings();
    load_tools();

    setWindowTitle(tr("DrGUI"));
    setUnifiedTitleAndToolBarOnMac(true);
}

/* Public
 * Destructor
 */
drgui_main_window_t::~drgui_main_window_t(void)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;

    while (plugins.count() > 0) {
        drgui_tool_interface_t *plugin = plugins.back();
        plugins.pop_back();
        delete plugin;
    }
    delete opt_win;
}

/* Protected
 * Handles closing of all tabs
 */
void
drgui_main_window_t::closeEvent(QCloseEvent *event)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    close_all_tabs();
    if (tab_area->currentWidget() != NULL) {
        event->ignore();
    } else {
        write_settings();
        event->accept();
        /* Ensure child windows close */
        qApp->quit();
    }
}

/* Private Slot
 * Shows About page for this program
 */
void
drgui_main_window_t::about(void)
{
    QMessageBox::about(this, tr("About Dr. GUI"),
                       tr("<center><b>Dr. GUI</b></center><br>"
                          "Interface for DynamoRIO and various extensions"));
}

/* Slot
 * Updates the menus to reflect current tab's abilities
 */
void
drgui_main_window_t::update_menus(void)
{
    bool has_tool_base = (active_tool() != 0);
    close_act->setEnabled(has_tool_base);
    close_all_act->setEnabled(has_tool_base);
    next_act->setEnabled(has_tool_base);
    previous_act->setEnabled(has_tool_base);
    separator_act->setVisible(has_tool_base);
}

/* Private Slot
 * Updates the Window menu to reflect current tab's abilities
 */
void
drgui_main_window_t::update_window_menu(void)
{
    window_menu->clear();
    window_menu->addAction(close_act);
    window_menu->addAction(close_all_act);
    window_menu->addSeparator();
    window_menu->addAction(next_act);
    window_menu->addAction(previous_act);
    window_menu->addAction(separator_act);

    separator_act->setVisible(tab_area->currentWidget() != NULL);

    static const int MAX_SHORTCUT_KEY = 9;
    for (int i = 0; i < tab_area->count(); ++i) {
        QWidget *tool = qobject_cast<QWidget *>(tab_area->widget(i));

        QString text;
        if (i < MAX_SHORTCUT_KEY) {
            /* '&' adds 'alt' shortcut to letter that follows */
            text = tr("&%1 %2").arg(i + 1).arg(tab_area->tabText(i));
        } else {
            text = tr("%1 %2").arg(i + 1).arg(tab_area->tabText(i));
        }
        QAction *action = window_menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(tool == active_tool());
        connect(action, &QAction::triggered,
                [=]() { emit tab_area->setCurrentIndex(i); });
    }
}

/* Private Slot
 * Creates and connects the actions for the mainwindow
 */
void
drgui_main_window_t::create_actions(void)
{
    separator_act = new QAction(this);
    separator_act->setSeparator(true);

    /* File */
    exit_act = new QAction(tr("E&xit"), this);
    exit_act->setShortcuts(QKeySequence::Quit);
    exit_act->setStatusTip(tr("Exit the application"));
    connect(exit_act, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

    /* Edit */
    preferences_act = new QAction(tr("&Preferences"), this);
    preferences_act->setStatusTip(tr("Edit Preferences"));
    connect(preferences_act, SIGNAL(triggered()), this, SLOT(show_preferences_dialog()));

    /* Window */
    close_act = new QAction(tr("Cl&ose"), this);
    close_act->setStatusTip(tr("Close the active tab"));
    connect(close_act, SIGNAL(triggered()), this, SLOT(maybe_close_me()));

    close_all_act = new QAction(tr("Close &All"), this);
    close_all_act->setStatusTip(tr("Close all the tabs"));
    connect(close_all_act, SIGNAL(triggered()), this, SLOT(close_all_tabs()));

    next_act = new QAction(tr("Ne&xt"), this);
    next_act->setShortcuts(QKeySequence::NextChild);
    next_act->setStatusTip(tr("Move the focus to the next tab"));
    connect(next_act, SIGNAL(triggered()), this, SLOT(activate_next_tab()));

    previous_act = new QAction(tr("Pre&vious"), this);
    previous_act->setShortcuts(QKeySequence::PreviousChild);
    previous_act->setStatusTip(tr("Move the focus to the previous tab"));
    connect(previous_act, SIGNAL(triggered()), this, SLOT(activate_previous_tab()));

    /* Tools */
    tool_action_group = new QActionGroup(this);
    load_tools_act = new QAction(tr("Load tools"), this);
    load_tools_act->setStatusTip(tr("Choose another directory to search for tools"));
    connect(load_tools_act, SIGNAL(triggered()), this, SLOT(add_tool()));

    /* Help */
    about_act = new QAction(tr("&About"), this);
    about_act->setStatusTip(tr("Show the application's About box"));
    connect(about_act, SIGNAL(triggered()), this, SLOT(about()));

    about_qt_act = new QAction(tr("About &Qt"), this);
    about_qt_act->setStatusTip(tr("Show the Qt library's About box"));
    connect(about_qt_act, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

/* Private
 * Creates the menus in the menu bar of the mainwindow
 */
void
drgui_main_window_t::create_menus(void)
{
    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addSeparator();
    QAction *action = file_menu->addAction(tr("Switch layout direction"));
    connect(action, SIGNAL(triggered()), this, SLOT(switch_layout_direction()));
    file_menu->addAction(exit_act);

    edit_menu = menuBar()->addMenu(tr("&Edit"));
    edit_menu->addAction(preferences_act);

    window_menu = menuBar()->addMenu(tr("&Window"));
    update_window_menu();
    connect(window_menu, SIGNAL(aboutToShow()), this, SLOT(update_window_menu()));

    menuBar()->addSeparator();

    tool_menu = menuBar()->addMenu(tr("&Tools"));
    /* add_tool_to_menu() depends on these two actions being added for adding
     * future tools to this menu. If a change is made here, it should be
     * reflected in add_tool_to_menu()
     */
    tool_menu->addSeparator();
    tool_menu->addAction(load_tools_act);

    help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->addAction(about_act);
    help_menu->addAction(about_qt_act);
}

/* Private
 * Creates status bar for displaying status tips
 */
void
drgui_main_window_t::create_status_bar(void)
{
    statusBar()->showMessage(tr("Ready"));
}

/* Private
 * Loads settings for mainwindow
 */
void
drgui_main_window_t::read_settings(void)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;

    /* We avoid the usual Dr. GUI naming convention here to avoid file system
     * issues as DrGUI will become a registry key on Windows, and
     * a filename on *nix.
     */
    QSettings settings("DynamoRIO", "DrGUI");
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(800, 600)).toSize();
    settings.beginGroup("Tools_to_load");
    int count = settings.value("Number_of_tools", 0).toInt();
    for (int i = 0; i < count; i++) {
        tool_files << settings.value(QString::number(i), QString()).toString();
    }
    settings.endGroup();
    move(pos);
    resize(size);
    /* Check if tool_to_auto load is a library or a name and add it to the
     * list of tools to be loaded if it is a file.
     */
    QFile tool_file(tool_to_auto_load);
    if (tool_file.exists())
        tool_files << tool_file.fileName();
    ;
}

/* Private
 * Saves settings for mainwindow
 */
void
drgui_main_window_t::write_settings(void)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;

    QSettings settings("DynamoRIO", "DrGUI");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.beginGroup("Tools_to_load");
    settings.setValue("Number_of_tools", tool_files.count());
    for (int i = 0; i < tool_files.count(); i++) {
        settings.setValue(QString::number(i), tool_files.at(i));
    }
    settings.endGroup();
}

/* Private Slot
 * Switches direction of layout for mainwindow
 */
void
drgui_main_window_t::switch_layout_direction(void)
{
    if (layoutDirection() == Qt::LeftToRight)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);
}

/* Private
 * Finds and returns the active tool tab, if there is one
 */
QWidget *
drgui_main_window_t::active_tool(void)
{
    int active_tab = tab_area->currentIndex();
    if (active_tab != -1)
        return qobject_cast<QWidget *>(tab_area->currentWidget());
    return 0;
}

/* Private Slot
 * Closes every tab in mainwindow
 */
void
drgui_main_window_t::close_all_tabs(void)
{
    while (tab_area->count() > 0) {
        tab_area->removeTab(0);
    }
}

/* Private Slot
 * Helper for closing current tab
 */
void
drgui_main_window_t::maybe_close_me(void)
{
    maybe_close(tab_area->currentIndex());
}

/* Private Slot
 * Confirms closing of a tab
 */
void
drgui_main_window_t::maybe_close(int index)
{
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(
        this, tr("Confirm"),
        tr("Are you sure you want to close '%1'?").arg(tab_area->tabText(index)),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (ret == QMessageBox::Yes)
        hide_tab(index);
}

/* Private Slot
 * Closes the tab at index
 */
void
drgui_main_window_t::hide_tab(int index)
{
    tab_area->removeTab(index);
}

/* Private Slot
 * Moves view to next tab in order, circular
 */
void
drgui_main_window_t::activate_next_tab(void)
{
    int index = tab_area->currentIndex() + 1;
    if (index == tab_area->count())
        index = 0;
    tab_area->setCurrentIndex(index);
}

/* Private Slot
 * Moves view to previous tab in order, wraps around
 */
void
drgui_main_window_t::activate_previous_tab(void)
{
    int index = tab_area->currentIndex() - 1;
    if (index == -1)
        index = tab_area->count() - 1;
    tab_area->setCurrentIndex(index);
}

/* Private Slot
 * Displays preferences dialog
 */
void
drgui_main_window_t::show_preferences_dialog(void)
{
    if (opt_win != NULL) {
        opt_win->display();
    }
}

/* Private Slot
 * Creates a new instance of a tool and displays it in the tab interface
 */
void
drgui_main_window_t::add_tab(void)
{
    QAction *action = qobject_cast<QAction *>(sender());
    QWidget *tool =
        qobject_cast<drgui_tool_interface_t *>(action->parent())->create_instance();
    const QString tool_name = action->text();
    new_tool_instance(tool, tool_name);
}

/* Private Slot
 * Creates a new instance of a tool with the specified arguments and displays
 * it in the tab interface.
 */
void
drgui_main_window_t::add_tab(drgui_tool_interface_t *factory, const QStringList &args)
{
    QWidget *tool = factory->create_instance(args);
    const QString tool_name = factory->tool_names().front();
    new_tool_instance(tool, tool_name);
}

/* Private Slot
 * Lets user choose more tools to load
 */
void
drgui_main_window_t::add_tool(void)
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    QStringList test_locs;
    test_locs = QFileDialog::getOpenFileNames(this, tr("Load tool"), "",
                                              tr("Tools (*.so *.dll)"));
    foreach (QString test_loc, test_locs) {
        QFile test_file(test_loc);
        if (test_file.exists() && !tool_files.contains(test_loc)) {
            tool_files << test_loc;
            load_tools();
        }
    }
}

/* Private
 * Loads available tools
 */
void
drgui_main_window_t::load_tools(void)
{
    foreach (QString tool_loc, tool_files) {
        QFile tool_file(tool_loc);
        QPluginLoader loader(tool_file.fileName(), this);
        QObject *plugin = loader.instance();
        if (plugin != NULL) {
            drgui_tool_interface_t *i_tool;
            i_tool = qobject_cast<drgui_tool_interface_t *>(plugin);
            /* Check if already loaded */
            bool stop = false;
            foreach (QString name, i_tool->tool_names()) {
                if (plugin_names.contains(name)) {
                    stop = true;
                    break;
                }
            }
            if (stop)
                break;
            /* Create and load */
            connect(i_tool, SIGNAL(new_instance_requested(QWidget *, QString)), this,
                    SLOT(new_tool_instance(QWidget *, QString)));
            if (i_tool != NULL) {
                add_tool_to_menu(plugin, i_tool->tool_names(), SLOT(add_tab()),
                                 tool_action_group);
                plugins.append(i_tool);
                qDebug() << "INFO: Loaded Plugins" << i_tool->tool_names();
                plugin_names << i_tool->tool_names();
                /* Auto-open the requested tool*/
                if (i_tool->tool_names().contains(tool_to_auto_load) ||
                    tool_to_auto_load == tool_loc) {
                    add_tab(i_tool, tool_to_auto_load_args);
                }
            }
        } else {
            qDebug() << "INFO: Denied Plugins" << loader.errorString();
        }
    }
}

/* Private
 * Adds a tool to tools_menu
 */
void
drgui_main_window_t::add_tool_to_menu(QObject *plugin, const QStringList &texts,
                                      const char *member, QActionGroup *action_group)
{
    foreach (QString text, texts) {
        QAction *action = new QAction(text, plugin);
        connect(action, SIGNAL(triggered()), this, member);

        /* The -2 is to insert the tool above the separator
         * and 'Load tools' actions. If this is changed then create_menus()
         * should be updated.
         */
        const QList<QAction *> &action_list = tool_menu->actions();
        static const unsigned int num_actions_to_skip = 2;
        const unsigned int pos = action_list.count() - num_actions_to_skip;
        assert(pos >= 0);
        tool_menu->insertAction(action_list.at(pos), action);

        if (action_group != NULL) {
            action->setCheckable(true);
            action_group->addAction(action);
        }
    }
}

/* Private Slot
 * Displays the new tool instance in the tab interface
 */
void
drgui_main_window_t::new_tool_instance(QWidget *tool, QString tool_name)
{
    tab_area->setCurrentIndex(tab_area->addTab(tool, tool_name));
}

} // namespace drgui
} // namespace dynamorio
