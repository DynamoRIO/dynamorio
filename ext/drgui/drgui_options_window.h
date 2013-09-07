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

class drgui_options_window_t : public QDialog
{
    Q_OBJECT

public:
    drgui_options_window_t(QActionGroup *tool_group_);

    void display(void);

public slots:
    void change_page(QListWidgetItem *current, QListWidgetItem *previous);

private slots:
    void save(void);

    void cancel(void);

private:
    void create_tool_list(void);

    void read_settings(void);

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

#endif
