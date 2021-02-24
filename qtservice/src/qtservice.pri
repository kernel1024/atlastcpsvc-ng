INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
LIBS += -luser32

HEADERS       += $$PWD/qtservice.h \
                 $$PWD/qtservice_p.h
SOURCES       += $$PWD/qtservice.cpp
SOURCES       += $$PWD/qtservice_win.cpp
