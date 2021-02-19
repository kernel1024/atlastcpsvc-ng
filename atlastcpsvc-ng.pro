#-------------------------------------------------
#
# Project created by QtCreator 2016-06-02T16:44:12
#
#-------------------------------------------------

QT       += core gui widgets network

TARGET = atlastcpsvc-ng
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    atlas.cpp \
    service.cpp \
    server.cpp \
    atlassocket.cpp

HEADERS  += mainwindow.h \
    atlas.h \
    qsl.h \
    service.h \
    server.h \
    atlassocket.h

CONFIG += warn_on \
    exceptions \
    rtti \
    stl \
    c++17

win32-msvc* {
    LIBS += -ladvapi32
    message("Using MSVC specific options")
}

FORMS    += mainwindow.ui \
    logindlg.ui

include(qtservice/src/qtservice.pri)

RC_FILE = atlastcpsvc-ng.rc

DISTFILES += \
    atlastcpsvc-ng.rc

RESOURCES += \
    atlastcpsvc-ng.qrc
