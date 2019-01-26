#-------------------------------------------------
#
# Project created by QtCreator 2018-07-11T15:20:07
#
#-------------------------------------------------

QT       += core gui network charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = WiFiGui
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
        myudp.cpp \
    chartswidgt.cpp \
    showwidget.cpp \
    mytcpclient.cpp \
    chartview.cpp \
    callout.cpp \
    login.cpp \
    md5.cpp

HEADERS  += mainwindow.h \
    myudp.h \
    chartswidgt.h \
    showwidget.h \
    mytcpclient.h \
    chartview.h \
    myeventfilter.h \
    callout.h \
    login.h \
    md5.h

FORMS    += mainwindow.ui \
    login.ui

RESOURCES += \
    src.qrc

DISTFILES += \
    readme.md
