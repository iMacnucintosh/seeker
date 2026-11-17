#include "ArchiveController.h"

#include <QCoreApplication>
#include "BrowserController.h"
#include "FileManager1.h"
#include "PickController.h"
#include "SoundPlayer.h"
#include "OpenWithController.h"
#include "PropertiesController.h"
#include "QuickLookController.h"
#include "Theme.h"
#include "ThemeIconProvider.h"
#include "ThumbnailProvider.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include "JsonTranslator.h"

#include <QGuiApplication>
#include <QLocale>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDBusConnection>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <cstring>

namespace {

class ShellKeyFilter : public QObject {
public:
    explicit ShellKeyFilter(BrowserController *browser, PickController *pick, QObject *parent = nullptr)
        : QObject(parent)
        , m_browser(browser)
        , m_pick(pick) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        Q_UNUSED(watched);
        if (!m_browser)
            return false;
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)
            return false;

        auto *ke = static_cast<QKeyEvent *>(event);
        const auto mods = ke->modifiers()
            & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);

        const bool isUndo = (ke->key() == Qt::Key_Z && mods == Qt::ControlModifier)
            || ke->matches(QKeySequence::Undo);
        const bool isDelete = (mods == Qt::NoModifier || mods == Qt::ShiftModifier)
            && (ke->key() == Qt::Key_Delete || ke->matches(QKeySequence::Delete));

        if (!isUndo && !isDelete)
            return false;

        if (m_pick && m_pick->active())
            return false;

        if (QObject *fo = QGuiApplication::focusObject()) {
            const char *cn = fo->metaObject()->className();
            if (cn && (std::strstr(cn, "TextInput") || std::strstr(cn, "TextEdit")
                       || std::strstr(cn, "QQuickTextField") || std::strstr(cn, "QQuickTextArea")))
                return false;
        }

        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }

        if (isUndo) {
            m_browser->undo();
            return true;
        }

        if (m_browser->selectionCount() > 0)
            m_browser->trashSelection();
        return true;
    }

private:
    BrowserController *m_browser = nullptr;
    PickController *m_pick = nullptr;
};

void loadLuneleUiFont() {
    const QStringList candidates = {
        QDir::home().filePath(QStringLiteral(
            ".config/quickshell/lunele/assets/google-sans-flex/"
            "GoogleSansFlex-VariableFont_GRAD,ROND,opsz,slnt,wdth,wght.ttf")),
        QDir::home().filePath(QStringLiteral(
            ".local/share/seeker/fonts/GoogleSansFlex.ttf")),
        QStringLiteral(":/fonts/GoogleSansFlex.ttf"),
    };
    for (const QString &path : candidates) {
        if (!QFile::exists(path))
            continue;
        if (QFontDatabase::addApplicationFont(path) >= 0)
            return;
    }
}

} // namespace

/// The code is in English; a language with a catalogue in i18n/<lang>.json (SEEKER_LANG overrides the system one)
/// is shown translated.
static void installTranslation(QCoreApplication &app) {
    const QString lang = qEnvironmentVariable("SEEKER_LANG", QLocale::system().name()).section(QLatin1Char('_'), 0, 0);
    auto *t = new JsonTranslator(&app);
    if (t->loadFile(QStringLiteral(":/i18n/%1.json").arg(lang)))
        app.installTranslator(t);
    else
        delete t;
}

int main(int argc, char *argv[]) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    installTranslation(app);
    app.setOrganizationName(QStringLiteral("Lunele"));
    app.setApplicationName(QStringLiteral("Seeker"));
    app.setApplicationDisplayName(QCoreApplication::translate("Seeker", "Files"));
    app.setDesktopFileName(QStringLiteral("seeker"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager"),
                                       QIcon(QStringLiteral(":/data/seeker.svg"))));
    loadLuneleUiFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Seeker — Material You file manager"));
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("new-window")}, QStringLiteral("Open a new window")});
    parser.addOption({{QStringLiteral("gapplication-service")}, QStringLiteral("D-Bus service mode")});
    parser.addOption({QStringLiteral("pick"), QStringLiteral("Portal / CLI file-chooser mode")});
    parser.addOption({QStringList{QStringLiteral("pick-mode")},
                      QStringLiteral("Pick mode: file, folder, or save"),
                      QStringLiteral("mode"), QStringLiteral("file")});
    parser.addOption({QStringList{QStringLiteral("pick-multiple")},
                      QStringLiteral("Allow multiple selection")});
    parser.addOption({QStringList{QStringLiteral("pick-title")},
                      QStringLiteral("Window title"), QStringLiteral("title")});
    parser.addOption({QStringList{QStringLiteral("pick-accept")},
                      QStringLiteral("Accept button label"), QStringLiteral("label")});
    parser.addOption({QStringList{QStringLiteral("pick-current")},
                      QStringLiteral("Initial folder"), QStringLiteral("path")});
    parser.addOption({QStringList{QStringLiteral("pick-name")},
                      QStringLiteral("Suggested save name"), QStringLiteral("name")});
    parser.addOption({QStringList{QStringLiteral("pick-result")},
                      QStringLiteral("JSON result path"), QStringLiteral("path")});
    parser.addOption({QStringList{QStringLiteral("pick-extensions")},
                      QStringLiteral("Comma-separated allowed extensions (file mode)"),
                      QStringLiteral("list")});
    parser.addPositionalArgument(QStringLiteral("paths"),
                                 QStringLiteral("Folders or files to open"),
                                 QStringLiteral("[paths...]"));
    parser.process(app);

    const bool pickMode = parser.isSet(QStringLiteral("pick"));

    // Allow translucent window buffers (Lunele transparency → Hyprland blur).
    QQuickWindow::setDefaultAlphaBuffer(true);

    Theme theme;
    BrowserController browser;
    ArchiveController archive;
    SoundPlayer sounds;
    browser.setTheme(&theme);
    PropertiesController properties;
    QuickLookController quickLook;
    OpenWithController openWith;
    browser.setOpenWith(&openWith);

    QStringList pickExts = parser.value(QStringLiteral("pick-extensions"))
                               .split(QLatin1Char(','), Qt::SkipEmptyParts);

    PickController pick;
    pick.configure(pickMode,
                   parser.value(QStringLiteral("pick-mode")),
                   parser.isSet(QStringLiteral("pick-multiple")),
                   parser.value(QStringLiteral("pick-title")),
                   parser.value(QStringLiteral("pick-accept")),
                   parser.value(QStringLiteral("pick-name")),
                   parser.value(QStringLiteral("pick-result")),
                   pickExts);
    pick.setBrowser(&browser);

    FileManager1 *fm1 = nullptr;
    if (!pickMode) {
        fm1 = new FileManager1(&browser, &properties);
        fm1->setParent(&app);
        QDBusConnection session = QDBusConnection::sessionBus();
        session.registerObject(QStringLiteral("/org/freedesktop/FileManager1"), fm1,
                               QDBusConnection::ExportAllSlots);
        session.registerService(QStringLiteral("org.freedesktop.FileManager1"));
    }

    const QString pickCurrent = parser.value(QStringLiteral("pick-current"));
    const QStringList args = parser.positionalArguments();
    if (!pickCurrent.isEmpty() && QFileInfo(pickCurrent).isDir())
        browser.navigate(pickCurrent);
    else if (!args.isEmpty())
        browser.navigate(args.first());

    QQmlApplicationEngine engine;
    engine.addImportPath(QDir::home().filePath(QStringLiteral(".local/lib/qt6/qml")));
    engine.addImageProvider(QStringLiteral("theme"), new ThemeIconProvider(&theme));
    engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("Browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("Archive"), &archive);
    engine.rootContext()->setContextProperty(QStringLiteral("Pick"), &pick);
    engine.rootContext()->setContextProperty(QStringLiteral("Properties"), &properties);
    engine.rootContext()->setContextProperty(QStringLiteral("OpenWith"), &openWith);
    engine.rootContext()->setContextProperty(QStringLiteral("QuickLook"), &quickLook);
    // Development: with SEEKER_SHOT, SEEKER_SHOT_OPEN names a dialog to open before the screenshot (Main.qml)
    engine.rootContext()->setContextProperty(QStringLiteral("DevShotOpen"), qEnvironmentVariable("SEEKER_SHOT_OPEN"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Development: SEEKER_SHOT=<file.png> saves the window after SEEKER_SHOT_DELAY ms (2500) and quits, for checking
    // the UI headless (QT_QPA_PLATFORM=offscreen, with its own XDG_CONFIG_HOME so no setting is touched)
    if (const QString shot = qEnvironmentVariable("SEEKER_SHOT"); !shot.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const int delay = qEnvironmentVariableIsSet("SEEKER_SHOT_DELAY") ? qEnvironmentVariableIntValue("SEEKER_SHOT_DELAY") : 2500;
        QTimer::singleShot(delay, &app, [window, shot] {
            if (window)
                window->grabWindow().save(shot);
            QGuiApplication::quit();
        });
    }

    QObject::connect(&pick, &PickController::finished, &app, &QGuiApplication::quit);
    // System sounds (on by default, switchable from the ⋮ menu).
    sounds.setEnabled(browser.soundsEnabled());
    QObject::connect(&browser, &BrowserController::soundsEnabledChanged, &sounds,
                     [&browser, &sounds] { sounds.setEnabled(browser.soundsEnabled()); });
    QObject::connect(&archive, &ArchiveController::finished, &sounds,
                     [&sounds, &archive](bool ok, const QString &message, const QString &) {
                         if (ok) // a different cue for each: packing "closes" (falling), unpacking "opens" (rising)
                             sounds.play(archive.kind() == QLatin1String("extract") ? QStringLiteral("device-added")
                                                                                    : QStringLiteral("device-removed"));
                         else if (message != QCoreApplication::translate("Seeker", "Cancelled"))
                             sounds.play(QStringLiteral("dialog-error"));
                     });
    QObject::connect(&browser, &BrowserController::trashEmptied, &sounds,
                     [&sounds] { sounds.play(QStringLiteral("trash-empty")); });
    QObject::connect(&browser, &BrowserController::errorOccurred, &sounds,
                     [&sounds](const QString &) { sounds.play(QStringLiteral("dialog-error")); });
    QObject::connect(&archive, &ArchiveController::finished, &browser,
                     [&browser](bool ok, const QString &, const QString &path) {
                         if (ok && !path.isEmpty()) {
                             browser.refresh();
                             browser.setSelectedPaths({path});
                         }
                     });

    auto *shellKeys = new ShellKeyFilter(&browser, &pick, &app);
    app.installEventFilter(shellKeys);
    for (QObject *root : engine.rootObjects()) {
        root->installEventFilter(shellKeys);
        for (QObject *child : root->children())
            child->installEventFilter(shellKeys);
    }

    return app.exec();
}
