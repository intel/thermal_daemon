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

class SensorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SensorsDialog(QWidget *parent = 0);
    ~SensorsDialog();

    void addSensor(QString name, bool checked);

    QVector<bool> getVisibilities();

private slots:
    void uncheckAll();
    void checkAll();


private:
    QVector<QCheckBox*> m_checkboxes;
    QLayout *m_checkboxLayout;
};

#endif // SENSORSDIALOG_H
