#include "BrowserController.h"
#include "FileManager1.h"
#include "Theme.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QDBusConnection>
#include <QCommandLineParser>
#include <QQuickStyle>

class ThemeIconProvider : public QQuickImageProvider {
public:
    ThemeIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override {
        const QIcon icon = QIcon::fromTheme(id);
        const int w = requestedSize.width() > 0 ? requestedSize.width() : 48;
        const int h = requestedSize.height() > 0 ? requestedSize.height() : 48;
        QPixmap pix = icon.pixmap(w, h);
        if (size)
            *size = pix.size();
        return pix;
    }
};

int main(int argc, char *argv[]) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Sylphrena"));
    app.setApplicationName(QStringLiteral("Seeker"));
    app.setApplicationDisplayName(QStringLiteral("Archivos"));
    app.setDesktopFileName(QStringLiteral("seeker"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager"),
                                       QIcon(QStringLiteral(":/data/seeker.svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Seeker — Material You file manager"));
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("new-window")}, QStringLiteral("Open a new window")});
    parser.addOption({{QStringLiteral("gapplication-service")}, QStringLiteral("D-Bus service mode")});
    parser.addPositionalArgument(QStringLiteral("paths"), QStringLiteral("Folders or files to open"), QStringLiteral("[paths...]"));
    parser.process(app);

    Theme theme;
    BrowserController browser;

    FileManager1 fm1(&browser);
    QDBusConnection session = QDBusConnection::sessionBus();
    session.registerObject(QStringLiteral("/org/freedesktop/FileManager1"), &fm1, QDBusConnection::ExportAllSlots);
    session.registerService(QStringLiteral("org.freedesktop.FileManager1"));

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        browser.navigate(args.first());

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("theme"), new ThemeIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("Browser"), &browser);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
