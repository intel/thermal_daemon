/*
 * Thermal Monitor displays current temperature readings on a graph
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 3 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
*/

#include <QApplication>
#include "mainwindow.h"
#include <unistd.h>

#define ROOT_ID 0

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Warn the user if not running as root
    uid_t id = getuid();
    if (id != ROOT_ID){
        QMessageBox msgBox;
        QString str;

        str = QString("%1 requires root privilages to access the thermal daemon.  "
                      "Try invoking again with root privileges.\n")
                .arg(QCoreApplication::applicationName());
        msgBox.setText(str);
        msgBox.setStandardButtons(QMessageBox::Abort);
        int ret = msgBox.exec();

        switch (ret) {
        case QMessageBox::Abort:
            // Abort was clicked
            return(1);
            break;
        default:
            // should never be reached
            qFatal("main: unexpected button result");
            break;
        }
    }

    QCoreApplication::setOrganizationDomain("intel.com");
    QCoreApplication::setOrganizationName("Intel");
    QCoreApplication::setApplicationName("ThermalMonitor");

    MainWindow w;
    w.show();

    return a.exec();
}
