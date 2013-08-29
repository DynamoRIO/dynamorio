/* **********************************************************
 * Copyright (c) 2013, Branden Clark All rights reserved.
 * **********************************************************/

/* Dr. Heapstat Visualizer
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the conditions outlined in
 * the BSD 2-Clause license are met.
 
 * This software is provided by the copyright holders and contributors "AS IS"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. See the BSD 2-Clause license for more details.
 */

/* main.cpp
 * 
 * Starts the app
 */

#include <QApplication>

#include "drgui_main_window.h"

int 
main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    drgui_main_window_t main_win;
    main_win.show();
    return app.exec();
}
