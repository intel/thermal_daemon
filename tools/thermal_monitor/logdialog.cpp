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

#include "logdialog.h"
#include "ui_logdialog.h"
#include <QDebug>
#include <QMessageBox>

LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);
}

LogDialog::~LogDialog()
{
    delete ui;
}

void LogDialog::setLoggingState(bool logging, bool visible_only, QString filename)
{
    ui->checkBoxLogging->setChecked(logging);
    ui->checkBoxLogVisibleOnly->setChecked(visible_only);
    ui->lineEditLogFilename->setText(filename);

    // disable inputs if logging is not enabled
    on_checkBoxLogging_toggled(ui->checkBoxLogging->checkState());
}

void LogDialog::on_checkBoxLogging_toggled(bool checked)
{
    if (checked) {
        ui->checkBoxLogVisibleOnly->setEnabled(true);
        ui->lineEditLogFilename->setEnabled(true);
    } else {
        ui->checkBoxLogVisibleOnly->setEnabled(false);
        ui->lineEditLogFilename->setEnabled(false);
    }
}

void LogDialog::on_buttonBox_accepted()
{
    // send signal to mainwindow with logging status
    emit setLogVariable(ui->checkBoxLogging->checkState(),
                        ui->checkBoxLogVisibleOnly->checkState(),
                        ui->lineEditLogFilename->text());
}
