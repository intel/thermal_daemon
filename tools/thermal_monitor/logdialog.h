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

#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>

namespace Ui {
class LogDialog;
}

class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget *parent = 0);
    ~LogDialog();

    void setLoggingState(bool logging, bool visible_only, QString filename);

signals:
    void setLogVariable(bool logging_enabled, bool log_visible_only,
                        QString log_file_name);

private slots:
    void on_checkBoxLogging_toggled(bool checked);
    void on_buttonBox_accepted();

private:
    Ui::LogDialog *ui;
};

#endif // LOGDIALOG_H
