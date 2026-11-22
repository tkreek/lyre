QT += core gui qml quick quickcontrols2 dbus network multimedia

CONFIG += c++17 release
TARGET = lyre
TEMPLATE = app

HEADERS += \
    src/artcache.h \
    src/item.h \
    src/itemmodel.h \
    src/jellyfin.h \
    src/library.h \
    src/mpris.h \
    src/player.h \
    src/queue.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/artcache.cpp \
    src/item.cpp \
    src/itemmodel.cpp \
    src/jellyfin.cpp \
    src/library.cpp \
    src/mpris.cpp \
    src/player.cpp \
    src/queue.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
