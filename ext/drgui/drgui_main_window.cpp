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

/* drgui_main_window.cpp
 * 
 * Provides a main structure for users to interface with tools.
 */

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "drgui_main_window_t::"

#include <QMenu>
#include <QAction>
#include <QSignalMapper>
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
 
#include "drgui_main_window.h"

/* Public
 * Constructor, everything begins here
 */
drgui_main_window_t::drgui_main_window_t(void) 
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
    window_mapper = new QSignalMapper(this);
    tab_area = new QTabWidget(this);
    tab_area->setTabsClosable(true);
    tab_area->setMovable(true);
    connect(tab_area, SIGNAL(tabCloseRequested(int)),
            this, SLOT(maybe_close(int)));
    setCentralWidget(tab_area);
    
    connect(tab_area, SIGNAL(currentChanged(int)),
            this, SLOT(update_menus()));

    create_actions();
    create_menus();
    create_status_bar();
    update_menus();

    read_settings();

    setWindowTitle(tr("DrGUI"));
    setUnifiedTitleAndToolBarOnMac(true);
}

/* Public
 * Destructor
 */
drgui_main_window_t::~drgui_main_window_t(void) 
{
    qDebug().nospace() << "INFO: Entering " << __CLASS__ << __FUNCTION__;
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
            text = tr("&%1 %2").arg(i + 1)
                               .arg(tab_area->tabText(i));
        } else {
            text = tr("%1 %2").arg(i + 1)
                              .arg(tab_area->tabText(i));
        }
        QAction *action  = window_menu->addAction(text);
        action->setCheckable(true);
        action ->setChecked(tool == active_tool());
        connect(action, SIGNAL(triggered()), 
                window_mapper, SLOT(map()));
        window_mapper->setMapping(action, i);
        connect(window_mapper, SIGNAL(mapped(int)), 
                tab_area, SLOT(setCurrentIndex(int)));
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
    connect(exit_act, SIGNAL(triggered()), 
            qApp, SLOT(closeAllWindows()));

    /* Edit */
    preferences_act = new QAction(tr("&Preferences"), this);
    preferences_act->setStatusTip(tr("Edit Preferences"));

    /* Window */
    close_act = new QAction(tr("Cl&ose"), this);
    close_act->setStatusTip(tr("Close the active tab"));
    connect(close_act, SIGNAL(triggered()),
            this, SLOT(maybe_close_me()));

    close_all_act = new QAction(tr("Close &All"), this);
    close_all_act->setStatusTip(tr("Close all the tabs"));
    connect(close_all_act, SIGNAL(triggered()),
            this, SLOT(close_all_tabs()));

    next_act = new QAction(tr("Ne&xt"), this);
    next_act->setShortcuts(QKeySequence::NextChild);
    next_act->setStatusTip(tr("Move the focus to the next tab"));
    connect(next_act, SIGNAL(triggered()),
            this, SLOT(activate_next_tab()));

    previous_act = new QAction(tr("Pre&vious"), this);
    previous_act->setShortcuts(QKeySequence::PreviousChild);
    previous_act->setStatusTip(tr("Move the focus to the previous tab"));
    connect(previous_act, SIGNAL(triggered()),
            this, SLOT(activate_previous_tab()));

    /* Help */
    about_act = new QAction(tr("&About"), this);
    about_act->setStatusTip(tr("Show the application's About box"));
    connect(about_act, SIGNAL(triggered()), 
            this, SLOT(about()));

    about_qt_act = new QAction(tr("About &Qt"), this);
    about_qt_act->setStatusTip(tr("Show the Qt library's About box"));
    connect(about_qt_act, SIGNAL(triggered()), 
            qApp, SLOT(aboutQt()));

    /* Tools */
    tool_action_group = new QActionGroup(this);
    load_tools_act = new QAction(tr("Load tools"), this);
    load_tools_act->setStatusTip(tr("Choose another directory to search "
                                    "for tools"));
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
    connect(action, SIGNAL(triggered()), 
            this, SLOT(switch_layout_direction()));
    file_menu->addAction(exit_act);

    edit_menu = menuBar()->addMenu(tr("&Edit"));
    edit_menu->addAction(preferences_act);

    window_menu = menuBar()->addMenu(tr("&Window"));
    update_window_menu();
    connect(window_menu, SIGNAL(aboutToShow()), 
            this, SLOT(update_window_menu()));

    menuBar()->addSeparator();

    tool_menu = menuBar()->addMenu(tr("&Tools"));
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
        tool_files << settings.value(QString::number(i), QString())
                              .toString();
    }
    settings.endGroup();
    move(pos);
    resize(size);
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
    ret = QMessageBox::warning(this, tr("Confirm"),
                               tr("Are you sure you want to close '%1'?")
                               .arg(tab_area->tabText(index)),
                               QMessageBox::Yes | QMessageBox::No |
                               QMessageBox::Cancel);
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
 * Movies view to previous tab in order, wraps around
 */
void 
drgui_main_window_t::activate_previous_tab(void) 
{
    int index = tab_area->currentIndex() - 1;
    if (index == -1)
        index = tab_area->count() - 1;
    tab_area->setCurrentIndex(index);
}
