#-------------------------------------------------
#
# Project created by QtCreator 2021-09-08T08:56:52
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CetSimulateEcu
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    main.cpp

HEADERS += \
    connection.h \
    includes.h \
    version.h

INCLUDEPATH += \
        $$PWD/ecuconfig \
        $$PWD/canconfig \
        $$PWD/canthread \
        $$PWD/canoperate \
        $$PWD/filterconfig \
        $$PWD/funcconfig \
        $$PWD/mainwindow

include ($$PWD/ecuconfig/ecuconfig.pri)
include ($$PWD/canconfig/canconfig.pri)
include ($$PWD/canthread/canthread.pri)
include ($$PWD/canoperate/canoperate.pri)
include ($$PWD/filterconfig/filterconfig.pri)
include ($$PWD/funcconfig/funcconfig.pri)
include ($$PWD/mainwindow/mainwindow.pri)

win32: LIBS += -L$$PWD/./ -lControlCAN

INCLUDEPATH += $$PWD/.
DEPENDPATH += $$PWD/.

RC_FILE += cetapp.rc

DISTFILES +=

win32: LIBS += -LD:\CetToolLibs\lib \
               -LD:\CetToolLibs\plugins
