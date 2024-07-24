#-------------------------------------------------
#
# Project created by QtCreator 2015-05-13T11:28:27
#
#-------------------------------------------------

QT       += core gui dbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

# Do not print qDebug() messages in release builds
CONFIG(release): DEFINES += QT_NO_DEBUG_OUTPUT

TARGET = ThermalMonitor
TEMPLATE = app

# Depending on distro and QT version
# name can be different, so choose correct one for your distro
LIBS += -lQCustomPlot
#LIBS += -lqcustomplot
#LIBS += -lqcustomplot-qt6

SOURCES += main.cpp\
        mainwindow.cpp \
    thermaldinterface.cpp \
    pollingdialog.cpp \
    sensorsdialog.cpp \
    logdialog.cpp \
    tripsdialog.cpp

HEADERS  += mainwindow.h \
    thermaldinterface.h \
    pollingdialog.h \
    sensorsdialog.h \
    logdialog.h \
    tripsdialog.h

FORMS    += \
    pollingdialog.ui \
    logdialog.ui \
    tripsdialog.ui
