#include "OpenWithController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <algorithm>

namespace {

QString desktopIdOf(const QString &desktopFile) {
    return QFileInfo(desktopFile).fileName();
}

bool truthy(const QString &s) {
    const QString t = s.trimmed().toLower();
    return t == QLatin1String("true") || t == QLatin1String("1");
}

/// Minimal .desktop reader — QSettings treats ';' as comments and breaks MimeType=.
struct DesktopEntry {
    QString type = QStringLiteral("Application");
    QString name;
    QString nameEs;
    QString icon;
    QString exec;
    QString mimeType;
    QString startupWmClass;
    bool noDisplay = false;
    bool hidden = false;
};

/// Raise the launched app on Hyprland 0.57+ (Lua dsp; retries for slow/existing apps).
void scheduleHyprlandFocus(const QString &wmClass) {
    if (wmClass.isEmpty())
        return;
    if (QStandardPaths::findExecutable(QStringLiteral("hyprctl")).isEmpty())
        return;
    // Strip anything that could break the Lua string.
    QString safe = wmClass;
    safe.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")));
    if (safe.isEmpty())
        return;

    // Hyprland 0.57: workspace switch needs hl.dispatch(hl.dsp.focus({workspace=id}));
    // then focus the window. Plain hl.dsp.focus({window=…}) alone stays on the current desktop.
    const QString script = QStringLiteral(
        "for delay in 0.12 0.28 0.5 0.9 1.4 2.2; do "
        "  sleep \"$delay\"; "
        "  if hyprctl eval '"
        "local want=\"%1\"; "
        "local w=hl.get_window(want); "
        "if not w then "
        "  for _,c in ipairs(hl.get_windows()) do "
        "    if c.class==want then w=c; break end "
        "  end "
        "end; "
        "if not w then error(\"missing\") end; "
        "hl.dispatch(hl.dsp.focus({workspace=w.workspace.id})); "
        "hl.dsp.focus({window=w}); "
        "hl.dsp.window.bring_to_top();"
        "' >/dev/null 2>&1; then "
        "    exit 0; "
        "  fi; "
        "done")
                               .arg(safe);
    QProcess::startDetached(QStringLiteral("bash"), {QStringLiteral("-c"), script});
}

DesktopEntry parseDesktopFile(const QString &path) {
    DesktopEntry e;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return e;

    QTextStream in(&f);
    bool inEntry = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (!inEntry)
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq);
        const QString val = line.mid(eq + 1);

        if (key == QLatin1String("Type"))
            e.type = val;
        else if (key == QLatin1String("Name"))
            e.name = val;
        else if (key == QLatin1String("Name[es]") || key == QLatin1String("Name[es_ES]"))
            e.nameEs = val;
        else if (key == QLatin1String("Icon"))
            e.icon = val;
        else if (key == QLatin1String("Exec"))
            e.exec = val;
        else if (key == QLatin1String("MimeType"))
            e.mimeType = val;
        else if (key == QLatin1String("StartupWMClass"))
            e.startupWmClass = val;
        else if (key == QLatin1String("NoDisplay"))
            e.noDisplay = truthy(val);
        else if (key == QLatin1String("Hidden"))
            e.hidden = truthy(val);
    }
    return e;
}

QStringList splitDesktopList(const QString &raw) {
    return raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

} // namespace

OpenWithController::OpenWithController(QObject *parent)
    : QObject(parent) {}

bool OpenWithController::canLaunch() const {
    return m_selectedIndex >= 0 && m_selectedIndex < m_apps.size();
}

void OpenWithController::setSelectedIndex(int index) {
    if (index == m_selectedIndex)
        return;
    if (index < -1 || index >= m_apps.size())
        index = -1;
    m_selectedIndex = index;
    emit selectedIndexChanged();
}

void OpenWithController::setRemember(bool remember) {
    if (m_remember == remember)
        return;
    m_remember = remember;
    emit rememberChanged();
}

void OpenWithController::select(int index) {
    setSelectedIndex(index);
}

void OpenWithController::close() {
    if (!m_open)
        return;
    m_open = false;
    emit openChanged();
}

void OpenWithController::openPath(const QString &path) {
    QString local = path;
    const QUrl url(path);
    if (url.isLocalFile())
        local = url.toLocalFile();
    local = QFileInfo(local).absoluteFilePath();
    if (!QFileInfo::exists(local) || QFileInfo(local).isDir()) {
        emit errorOccurred(tr("No se puede abrir con una app: %1").arg(path));
        return;
    }

    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(local);
    m_path = local;
    m_fileName = QFileInfo(local).fileName();
    m_mimeName = mime.name();
    m_mimeLabel = mime.comment().isEmpty() ? mime.name() : mime.comment();
    m_remember = false;
    m_selectedIndex = -1;

    QStringList parents = mime.parentMimeTypes();
    QStringList extra;
    for (const QString &p : parents) {
        const QMimeType pm = db.mimeTypeForName(p);
        for (const QString &g : pm.parentMimeTypes()) {
            if (!parents.contains(g) && !extra.contains(g))
                extra << g;
        }
    }
    parents += extra;

    loadApps(m_mimeName, parents);

    const QString defId = defaultDesktopForMime(m_mimeName);
    if (!defId.isEmpty()) {
        for (int i = 0; i < m_apps.size(); ++i) {
            if (m_apps.at(i).toMap().value(QStringLiteral("id")).toString() == defId) {
                m_selectedIndex = i;
                break;
            }
        }
    }
    if (m_selectedIndex < 0 && !m_apps.isEmpty())
        m_selectedIndex = 0;

    const bool wasOpen = m_open;
    m_open = true;
    emit infoChanged();
    emit appsChanged();
    emit selectedIndexChanged();
    emit rememberChanged();
    if (!wasOpen)
        emit openChanged();
}

QStringList OpenWithController::applicationDirs() {
    QStringList dirs;
    const QStringList data = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &d : data) {
        if (!dirs.contains(d))
            dirs << d;
    }
    const QStringList extras = {
        QDir::home().filePath(QStringLiteral(".local/share/applications")),
        QStringLiteral("/usr/local/share/applications"),
        QStringLiteral("/usr/share/applications"),
    };
    for (const QString &d : extras) {
        if (!dirs.contains(d))
            dirs << d;
    }
    return dirs;
}

bool OpenWithController::mimeMatches(const QStringList &desktopMimes, const QString &mime,
                                     const QStringList &parents) {
    if (desktopMimes.contains(mime))
        return true;
    for (const QString &p : parents) {
        if (desktopMimes.contains(p))
            return true;
    }
    const int slash = mime.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        const QString wild = mime.left(slash + 1) + QLatin1Char('*');
        if (desktopMimes.contains(wild))
            return true;
    }
    return false;
}

void OpenWithController::loadApps(const QString &mimeName, const QStringList &mimeParents) {
    m_apps.clear();
    QHash<QString, AppEntry> byId;

    for (const QString &dirPath : applicationDirs()) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QFileInfo &fi : files) {
            const DesktopEntry desk = parseDesktopFile(fi.absoluteFilePath());
            if (desk.noDisplay || desk.hidden)
                continue;
            if (desk.type != QLatin1String("Application"))
                continue;
            if (desk.mimeType.isEmpty())
                continue;
            const QStringList deskMimes = splitDesktopList(desk.mimeType);
            if (!mimeMatches(deskMimes, mimeName, mimeParents))
                continue;

            AppEntry e;
            e.id = desktopIdOf(fi.absoluteFilePath());
            e.name = desk.nameEs.isEmpty() ? desk.name : desk.nameEs;
            if (e.name.isEmpty())
                e.name = e.id;
            e.icon = desk.icon.isEmpty() ? QStringLiteral("application-x-executable") : desk.icon;
            e.desktopFile = fi.absoluteFilePath();
            byId.insert(e.id, e);
        }
    }

    QList<AppEntry> list = byId.values();
    const QString defId = defaultDesktopForMime(mimeName);
    std::sort(list.begin(), list.end(), [&](const AppEntry &a, const AppEntry &b) {
        if (a.id == defId)
            return true;
        if (b.id == defId)
            return false;
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    m_apps.reserve(list.size());
    for (const AppEntry &e : list) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), e.id);
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("icon"), e.icon);
        m.insert(QStringLiteral("desktopFile"), e.desktopFile);
        m_apps << m;
    }
}

QString OpenWithController::defaultDesktopForMime(const QString &mime) {
    if (mime.isEmpty())
        return {};

    // Only an explicit Default Applications entry counts as “specified”.
    // `xdg-mime query default` falls back to any claiming .desktop and would
    // skip our Abrir-con dialog on double-click for unset types.
    const QStringList lists = {
        QDir::home().filePath(QStringLiteral(".config/mimeapps.list")),
        QDir::home().filePath(QStringLiteral(".local/share/applications/mimeapps.list")),
        QStringLiteral("/etc/xdg/mimeapps.list"),
    };
    for (const QString &listPath : lists) {
        if (!QFileInfo::exists(listPath))
            continue;
        QFile f(listPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream in(&f);
        bool inDefaults = false;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            if (line.startsWith(QLatin1Char('['))) {
                inDefaults = (line == QLatin1String("[Default Applications]"));
                continue;
            }
            if (!inDefaults)
                continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0)
                continue;
            if (line.left(eq) != mime)
                continue;
            const QString raw = line.mid(eq + 1).trimmed();
            const QString first = splitDesktopList(raw).value(0).trimmed();
            if (!first.isEmpty())
                return first;
        }
    }
    return {};
}

bool OpenWithController::setDefaultDesktop(const QString &desktopId, const QString &mime) {
    if (desktopId.isEmpty() || mime.isEmpty())
        return false;
    QProcess proc;
    proc.start(QStringLiteral("xdg-mime"),
               {QStringLiteral("default"), desktopId, mime});
    return proc.waitForFinished(3000) && proc.exitCode() == 0;
}

bool OpenWithController::launchDesktop(const QString &desktopFileOrId, const QString &localPath) {
    if (desktopFileOrId.isEmpty() || localPath.isEmpty())
        return false;

    QString desktopPath = desktopFileOrId;
    if (!QFileInfo::exists(desktopPath)) {
        const QString id = desktopFileOrId.endsWith(QLatin1String(".desktop"))
            ? desktopFileOrId
            : desktopFileOrId + QStringLiteral(".desktop");
        for (const QString &dirPath : applicationDirs()) {
            const QString candidate = QDir(dirPath).filePath(id);
            if (QFileInfo::exists(candidate)) {
                desktopPath = candidate;
                break;
            }
        }
    }
    if (!QFileInfo::exists(desktopPath))
        return false;

    const DesktopEntry desk = parseDesktopFile(desktopPath);
    const QString wmClass = desk.startupWmClass;

    if (!QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty()) {
        if (QProcess::startDetached(QStringLiteral("gio"),
                                    {QStringLiteral("launch"), desktopPath, localPath})) {
            scheduleHyprlandFocus(wmClass);
            return true;
        }
    }

    QString exec = desk.exec;
    if (exec.isEmpty())
        return false;

    const bool hadFieldCode = exec.contains(QLatin1Char('%'));
    const QString uri = QUrl::fromLocalFile(localPath).toString();
    exec.replace(QStringLiteral("%F"), localPath);
    exec.replace(QStringLiteral("%f"), localPath);
    exec.replace(QStringLiteral("%U"), uri);
    exec.replace(QStringLiteral("%u"), uri);
    exec.replace(QRegularExpression(QStringLiteral("%[iIcCk]")), QString());

    QStringList args;
    QString cur;
    bool inQuote = false;
    QChar q;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec.at(i);
        if (inQuote) {
            if (c == q)
                inQuote = false;
            else
                cur += c;
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            inQuote = true;
            q = c;
            continue;
        }
        if (c.isSpace()) {
            if (!cur.isEmpty()) {
                args << cur;
                cur.clear();
            }
            continue;
        }
        cur += c;
    }
    if (!cur.isEmpty())
        args << cur;
    if (args.isEmpty())
        return false;

    if (!hadFieldCode)
        args << localPath;

    const QString bin = args.takeFirst();
    if (!QProcess::startDetached(bin, args))
        return false;
    scheduleHyprlandFocus(wmClass);
    return true;
}

void OpenWithController::launchSelected() {
    if (!canLaunch())
        return;
    const QVariantMap app = m_apps.at(m_selectedIndex).toMap();
    const QString desktopFile = app.value(QStringLiteral("desktopFile")).toString();
    const QString id = app.value(QStringLiteral("id")).toString();

    if (m_remember)
        setDefaultDesktop(id, m_mimeName);

    if (!launchDesktop(desktopFile, m_path)) {
        emit errorOccurred(tr("No se pudo abrir con %1").arg(app.value(QStringLiteral("name")).toString()));
        return;
    }
    close();
}
