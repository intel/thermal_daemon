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

#ifndef POLLINGDIALOG_H
#define POLLINGDIALOG_H

#include <QDialog>

namespace Ui {
class PollingDialog;
}

class PollingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PollingDialog(QWidget *parent, uint current_interval);
    ~PollingDialog();

signals:
    void setPollInterval(uint val);

private slots:
    void on_lineEdit_returnPressed();
    void on_setButton_clicked();

private:
    Ui::PollingDialog *ui;
    uint default_interval;
};

#endif // POLLINGDIALOG_H
