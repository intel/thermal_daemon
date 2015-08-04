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

#include "pollingdialog.h"
#include "ui_pollingdialog.h"
#include "mainwindow.h"

PollingDialog::PollingDialog(QWidget *parent, uint current_interval) :
    QDialog(parent),
    ui(new Ui::PollingDialog),
    default_interval(current_interval)
{
    ui->setupUi(this);
    ui->lineEdit->setValidator(new QIntValidator);
    ui->lineEdit->setText(QString("%1").arg(current_interval));
    ui->label->setText("Set temperature polling interval (ms)");
    ui->label_2->setText("ms");
}

PollingDialog::~PollingDialog()
{
    delete ui;
}

void PollingDialog::on_lineEdit_returnPressed()
{
    emit setPollInterval(ui->lineEdit->text().toInt());
}

void PollingDialog::on_setButton_clicked()
{
    emit setPollInterval(ui->lineEdit->text().toInt());
}
