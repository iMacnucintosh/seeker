QT += quick qml quickcontrols2 dbus svg widgets concurrent
CONFIG += c++20
TARGET = seeker
TEMPLATE = app

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SOURCES += \
    src/main.cpp \
    src/JsonTranslator.cpp \
    src/ArchiveController.cpp \
    src/SoundPlayer.cpp \
    src/BrowserController.cpp \
    src/DirModel.cpp \
    src/Theme.cpp \
    src/ThemeIconProvider.cpp \
    src/Trash.cpp \
    src/FileManager1.cpp \
    src/PickController.cpp \
    src/ThumbnailProvider.cpp \
    src/Thumbnailers.cpp \
    src/PropertiesController.cpp \
    src/OpenWithController.cpp

HEADERS += \
    src/JsonTranslator.h \
    src/ArchiveController.h \
    src/SoundPlayer.h \
    src/BrowserController.h \
    src/DirModel.h \
    src/Theme.h \
    src/ThemeIconProvider.h \
    src/Trash.h \
    src/FileManager1.h \
    src/PickController.h \
    src/ThumbnailProvider.h \
    src/Thumbnailers.h \
    src/PropertiesController.h \
    src/OpenWithController.h

RESOURCES += qml.qrc

target.path = $$PREFIX/bin
desktop.path = $$PREFIX/share/applications
desktop.files = data/seeker.desktop
dbus.path = $$PREFIX/share/dbus-1/services
dbus.files = data/dbus/org.freedesktop.FileManager1.service
icons.path = $$PREFIX/share/icons/hicolor/scalable/apps
icons.files = data/seeker.svg

portal_libexec.path = $$PREFIX/libexec
portal_libexec.files = portal/xdg-desktop-portal-seeker
portal_portal.path = $$PREFIX/share/xdg-desktop-portal/portals
portal_portal.files = portal/seeker.portal

isEmpty(PREFIX) {
    PREFIX = $$HOME/.local
}

INSTALLS += target desktop dbus icons portal_libexec portal_portal

# D-Bus service + portals.conf: run portal/install-portal.sh after make install.
