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

#include "sensorsdialog.h"
#include "ui_sensorsdialog.h"
#include <QDebug>
#include <QVector>

SensorsDialog::SensorsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SensorsDialog)
{
    ui->setupUi(this);
}

SensorsDialog::~SensorsDialog()
{
    delete ui;
}

void SensorsDialog::disableCheckbox(int index)
{
    QCheckBox *cb = getCheckboxPtr(index);

    if (cb != NULL) {
        cb->setText("");
        cb->setEnabled(false);
        cb->setVisible(false);
    }
}

void SensorsDialog::setupCheckbox(int index, QString name, bool checked)
{
    QCheckBox *cb = getCheckboxPtr(index);

    if (cb != NULL) {
        cb->setText(name);
        cb->setChecked(checked);
    }
}

QCheckBox* SensorsDialog::getCheckboxPtr(int index)
{
    if (index >= 0 && index < MAX_NUMBER_SENSOR_VISIBILITY_CHECKBOXES) {
        switch(index){
        case 0: return ui->checkBox;
            break;
        case 1: return ui->checkBox_2;
            break;
        case 2: return ui->checkBox_3;
            break;
        case 3: return ui->checkBox_4;
            break;
        case 4: return ui->checkBox_5;
            break;
        case 5: return ui->checkBox_6;
            break;
        case 6: return ui->checkBox_7;
            break;
        case 7: return ui->checkBox_8;
            break;
        case 8: return ui->checkBox_9;
            break;
        case 9: return ui->checkBox_10;
            break;
        case 10: return ui->checkBox_11;
            break;
        case 11: return ui->checkBox_12;
            break;
        case 12: return ui->checkBox_13;
            break;
        case 13: return ui->checkBox_14;
            break;
        case 14: return ui->checkBox_15;
            break;
        case 15: return ui->checkBox_16;
            break;
        case 16: return ui->checkBox_17;
            break;
        case 17: return ui->checkBox_18;
            break;
        case 18: return ui->checkBox_19;
            break;
        case 19: return ui->checkBox_20;
            break;
        }
    } else {
        qDebug() << "index" << index << "out of range, SensorsDialog::getCheckboxPtr";
    }
    return NULL;
}


void SensorsDialog::on_checkBox_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(0, true);
    } else {
        emit setGraphVisibility(0, false);
    }
}

void SensorsDialog::on_checkBox_2_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(1, true);
    } else {
        emit setGraphVisibility(1, false);
    }

}

void SensorsDialog::on_checkBox_3_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(2, true);
    } else {
        emit setGraphVisibility(2, false);
    }
}

void SensorsDialog::on_checkBox_4_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(3, true);
    } else {
        emit setGraphVisibility(3, false);
    }
}

void SensorsDialog::on_checkBox_5_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(4, true);
    } else {
        emit setGraphVisibility(4, false);
    }
}

void SensorsDialog::on_checkBox_6_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(5, true);
    } else {
        emit setGraphVisibility(5, false);
    }
}

void SensorsDialog::on_checkBox_7_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(6, true);
    } else {
        emit setGraphVisibility(6, false);    }
}

void SensorsDialog::on_checkBox_8_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(7, true);
    } else {
        emit setGraphVisibility(7, false);
    }
}

void SensorsDialog::on_checkBox_9_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(8, true);
    } else {
        emit setGraphVisibility(8, false);
    }
}

void SensorsDialog::on_checkBox_10_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(9, true);
    } else {
        emit setGraphVisibility(9, false);
    }
}

void SensorsDialog::on_checkBox_11_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(10, true);
    } else {
        emit setGraphVisibility(10, false);
    }
}

void SensorsDialog::on_checkBox_12_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(11, true);
    } else {
        emit setGraphVisibility(11, false);
    }
}

void SensorsDialog::on_checkBox_13_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(12, true);
    } else {
        emit setGraphVisibility(12, false);
    }
}

void SensorsDialog::on_checkBox_14_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(13, true);
    } else {
        emit setGraphVisibility(13, false);
    }
}

void SensorsDialog::on_checkBox_15_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(14, true);
    } else {
        emit setGraphVisibility(14, false);
    }
}

void SensorsDialog::on_checkBox_16_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(15, true);
    } else {
        emit setGraphVisibility(15, false);
    }
}

void SensorsDialog::on_checkBox_17_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(16, true);
    } else {
        emit setGraphVisibility(16, false);
    }
}

void SensorsDialog::on_checkBox_18_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(17, true);
    } else {
        emit setGraphVisibility(17, false);
    }
}

void SensorsDialog::on_checkBox_19_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(18, true);
    } else {
        emit setGraphVisibility(18, false);
    }
}

void SensorsDialog::on_checkBox_20_toggled(bool checked)
{
    if (checked) {
        emit setGraphVisibility(19, true);
    } else {
        emit setGraphVisibility(19, false);
    }
}

void SensorsDialog::on_setAllButton_clicked()
{
    ui->checkBox->setChecked(true);
    ui->checkBox_2->setChecked(true);
    ui->checkBox_3->setChecked(true);
    ui->checkBox_4->setChecked(true);
    ui->checkBox_5->setChecked(true);
    ui->checkBox_6->setChecked(true);
    ui->checkBox_7->setChecked(true);
    ui->checkBox_8->setChecked(true);
    ui->checkBox_9->setChecked(true);
    ui->checkBox_10->setChecked(true);
    ui->checkBox_11->setChecked(true);
    ui->checkBox_12->setChecked(true);
    ui->checkBox_13->setChecked(true);
    ui->checkBox_14->setChecked(true);
    ui->checkBox_15->setChecked(true);
    ui->checkBox_16->setChecked(true);
    ui->checkBox_17->setChecked(true);
    ui->checkBox_18->setChecked(true);
    ui->checkBox_19->setChecked(true);
    ui->checkBox_20->setChecked(true);
}

void SensorsDialog::on_clearAllButton_clicked()
{
    ui->checkBox->setChecked(false);
    ui->checkBox_2->setChecked(false);
    ui->checkBox_3->setChecked(false);
    ui->checkBox_4->setChecked(false);
    ui->checkBox_5->setChecked(false);
    ui->checkBox_6->setChecked(false);
    ui->checkBox_7->setChecked(false);
    ui->checkBox_8->setChecked(false);
    ui->checkBox_9->setChecked(false);
    ui->checkBox_10->setChecked(false);
    ui->checkBox_11->setChecked(false);
    ui->checkBox_12->setChecked(false);
    ui->checkBox_13->setChecked(false);
    ui->checkBox_14->setChecked(false);
    ui->checkBox_15->setChecked(false);
    ui->checkBox_16->setChecked(false);
    ui->checkBox_17->setChecked(false);
    ui->checkBox_18->setChecked(false);
    ui->checkBox_19->setChecked(false);
    ui->checkBox_20->setChecked(false);
}
