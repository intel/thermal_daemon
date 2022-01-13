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
#include <QMessageBox>
#include "mainwindow.h"
#include <unistd.h>

#define ROOT_ID 0

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Warn the user if not running as root
    uid_t id = getuid();
    if (id == ROOT_ID){
        QMessageBox::critical(0, "Running as root is not supported",
                      "Running X11 applications as root is unsafe.\n"
                      "Try invoking again without root privileges.");
        return(1);
    }

    ThermaldInterface thermaldInterface;
    if (!thermaldInterface.initialize()) {
        QMessageBox::critical(0, "Can't establish link with thermal daemon.",
                              "Make sure that thermal daemon started with --dbus-enable option\n"
                              "and that you're in the power management group ('power', 'powerdev' etc).");
        return 1;
    }

    QCoreApplication::setOrganizationDomain("intel.com");
    QCoreApplication::setOrganizationName("Intel");
    QCoreApplication::setApplicationName("ThermalMonitor");

    MainWindow w(&thermaldInterface);
    w.show();

    return a.exec();
}
