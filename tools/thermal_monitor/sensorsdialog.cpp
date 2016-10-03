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
#include <QDebug>
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>

SensorsDialog::SensorsDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle("Select visible sensors");

    QVBoxLayout *dialogLayout = new QVBoxLayout;
    setLayout(dialogLayout);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    dialogLayout->addLayout(mainLayout);

    m_checkboxLayout = new QVBoxLayout;
    mainLayout->addLayout(m_checkboxLayout);

    QPushButton *checkAllButton = new QPushButton("Show all");
    QPushButton *uncheckAllButton = new QPushButton("Hide all");
    connect(checkAllButton, SIGNAL(clicked(bool)), this, SLOT(checkAll()));
    connect(uncheckAllButton, SIGNAL(clicked(bool)), this, SLOT(uncheckAll()));

    QVBoxLayout *actionsLayout = new QVBoxLayout;
    mainLayout->addLayout(actionsLayout);
    actionsLayout->addWidget(checkAllButton);
    actionsLayout->addWidget(uncheckAllButton);
    actionsLayout->addStretch();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

}

SensorsDialog::~SensorsDialog()
{
}

void SensorsDialog::addSensor(QString name, bool checked)
{
    QCheckBox *checkbox = new QCheckBox(name);
    checkbox->setChecked(checked);
    m_checkboxes.append(checkbox);
    m_checkboxLayout->addWidget(checkbox);

}

QVector<bool> SensorsDialog::getVisibilities()
{
    QVector<bool> visibilities;
    foreach(const QCheckBox *checkbox, m_checkboxes) {
        visibilities.append(checkbox->isChecked());
    }

    return visibilities;
}

void SensorsDialog::uncheckAll()
{
    foreach(QCheckBox *checkbox, m_checkboxes) {
        checkbox->setChecked(false);
    }
}

void SensorsDialog::checkAll()
{
    foreach(QCheckBox *checkbox, m_checkboxes) {
        checkbox->setChecked(true);
    }
}
