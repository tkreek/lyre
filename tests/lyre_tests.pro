QT += core gui qml quick quickcontrols2 testlib dbus network multimedia
CONFIG += c++17 testcase
TARGET = lyre_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/artcache.h \
    ../src/item.h \
    ../src/itemmodel.h \
    ../src/jellyfin.h \
    ../src/library.h \
    ../src/mpris.h \
    ../src/player.h \
    ../src/queue.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    lyre_tests.cpp \
    ../src/artcache.cpp \
    ../src/item.cpp \
    ../src/itemmodel.cpp \
    ../src/jellyfin.cpp \
    ../src/library.cpp \
    ../src/mpris.cpp \
    ../src/player.cpp \
    ../src/queue.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
