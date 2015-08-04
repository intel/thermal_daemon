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

#ifndef SENSORSDIALOG_H
#define SENSORSDIALOG_H

#include <QCheckBox>
#include <QDialog>

#define MAX_NUMBER_SENSOR_VISIBILITY_CHECKBOXES 20

namespace Ui {
class SensorsDialog;
}

class SensorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SensorsDialog(QWidget *parent = 0);
    ~SensorsDialog();

    void disableCheckbox(int index);
    void setupCheckbox(int index, QString name, bool checked);

signals:
    void setGraphVisibility(uint, bool);

private slots:
    void on_checkBox_toggled(bool checked);
    void on_checkBox_2_toggled(bool checked);
    void on_checkBox_3_toggled(bool checked);
    void on_checkBox_4_toggled(bool checked);
    void on_checkBox_5_toggled(bool checked);
    void on_checkBox_6_toggled(bool checked);
    void on_checkBox_7_toggled(bool checked);
    void on_checkBox_8_toggled(bool checked);
    void on_checkBox_9_toggled(bool checked);
    void on_checkBox_10_toggled(bool checked);
    void on_checkBox_11_toggled(bool checked);
    void on_checkBox_12_toggled(bool checked);
    void on_checkBox_13_toggled(bool checked);
    void on_checkBox_14_toggled(bool checked);
    void on_checkBox_15_toggled(bool checked);
    void on_checkBox_16_toggled(bool checked);

    void on_checkBox_17_toggled(bool checked);
    void on_checkBox_18_toggled(bool checked);
    void on_checkBox_19_toggled(bool checked);
    void on_checkBox_20_toggled(bool checked);

    void on_setAllButton_clicked();
    void on_clearAllButton_clicked();


private:
    Ui::SensorsDialog *ui;
    //QCheckBox *checkbox;
    QVector<QCheckBox*> checkbox;

    QCheckBox* getCheckboxPtr(int index);
};

#endif // SENSORSDIALOG_H
