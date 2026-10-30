QT += quick qml quickcontrols2 dbus svg widgets concurrent
CONFIG += c++20
TARGET = seeker
TEMPLATE = app

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SOURCES += \
    src/main.cpp \
    src/BrowserController.cpp \
    src/DirModel.cpp \
    src/Theme.cpp \
    src/Trash.cpp \
    src/FileManager1.cpp

HEADERS += \
    src/BrowserController.h \
    src/DirModel.h \
    src/Theme.h \
    src/Trash.h \
    src/FileManager1.h

RESOURCES += qml.qrc

target.path = $$PREFIX/bin
desktop.path = $$PREFIX/share/applications
desktop.files = data/seeker.desktop
dbus.path = $$PREFIX/share/dbus-1/services
dbus.files = data/dbus/org.freedesktop.FileManager1.service
icons.path = $$PREFIX/share/icons/hicolor/scalable/apps
icons.files = data/seeker.svg

isEmpty(PREFIX) {
    PREFIX = $$HOME/.local
}

INSTALLS += target desktop dbus icons
