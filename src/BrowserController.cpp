#include "BrowserController.h"
#include "OpenWithController.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVariant>

namespace {

bool copyRecursively(const QString &src, const QString &dest) {
    const QFileInfo fi(src);
    if (!fi.exists())
        return false;

    if (fi.isSymLink()) {
        const QString target = fi.symLinkTarget();
        QFile::remove(dest);
        return QFile::link(target, dest);
    }

    if (fi.isDir()) {
        if (!QDir().mkpath(dest))
            return false;
        const QDir dir(src);
        const auto entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot
                                               | QDir::Hidden | QDir::System);
        for (const QString &name : entries) {
            if (!copyRecursively(dir.filePath(name), QDir(dest).filePath(name)))
                return false;
        }
        return true;
    }

    QFile::remove(dest);
    return QFile::copy(src, dest);
}

bool removeRecursively(const QString &path) {
    QFileInfo fi(path);
    if (!fi.exists() && !fi.isSymLink())
        return true;
    if (fi.isDir() && !fi.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

} // namespace

BrowserController::BrowserController(QObject *parent)
    : QObject(parent)
    , m_model(new DirModel(this)) {
    loadSettings();
    seedDefaultPinsIfNeeded();
    loadFolderIcons();
    navigate(QDir::homePath());
}

QString BrowserController::currentPath() const {
    return m_model->path();
}

QString BrowserController::displayPath() const {
    const QString home = QDir::homePath();
    QString p = currentPath();
    if (p == home)
        return QStringLiteral("Inicio");
    if (p.startsWith(home + QLatin1Char('/')))
        return p.mid(home.size() + 1); // skip "~/" noise — show Documentos/… under home
    return p;
}

QVariantMap BrowserController::builtinPlace(const QString &id, const QString &label, const QString &icon,
                                            const QString &symbol, const QString &path) const {
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("symbol"), symbol},
        {QStringLiteral("path"), path},
        {QStringLiteral("pinned"), false},
        {QStringLiteral("builtin"), true},
    };
}

QStringList BrowserController::builtinPlacePaths() const {
    // Only fixed entries: Inicio + Papelera. XDG home dirs are removable pins.
    QStringList out;
    out << QDir::cleanPath(QDir::homePath());
    out << QDir::cleanPath(m_trash.trashPath());
    out.removeAll(QString());
    return out;
}

QVector<BrowserController::DefaultPin> BrowserController::defaultHomePins() const {
    // Fixed short list of XDG user dirs — never enumerate $HOME (could be huge).
    QVector<DefaultPin> out;
    auto add = [&](QStandardPaths::StandardLocation loc, const QString &label, const QString &icon,
                   const QString &symbol) {
        const QString path = QDir::cleanPath(QStandardPaths::writableLocation(loc));
        if (path.isEmpty())
            return;
        out.push_back({label, icon, symbol, path});
    };
    add(QStandardPaths::DesktopLocation, QStringLiteral("Escritorio"), QStringLiteral("user-desktop"),
        QStringLiteral("desktop_windows"));
    add(QStandardPaths::DocumentsLocation, QStringLiteral("Documentos"), QStringLiteral("folder-documents"),
        QStringLiteral("description"));
    add(QStandardPaths::DownloadLocation, QStringLiteral("Descargas"), QStringLiteral("folder-download"),
        QStringLiteral("download"));
    add(QStandardPaths::PicturesLocation, QStringLiteral("Imágenes"), QStringLiteral("folder-pictures"),
        QStringLiteral("image"));
    add(QStandardPaths::MusicLocation, QStringLiteral("Música"), QStringLiteral("folder-music"),
        QStringLiteral("music_note"));
    add(QStandardPaths::MoviesLocation, QStringLiteral("Vídeos"), QStringLiteral("folder-videos"),
        QStringLiteral("video_library"));
    return out;
}

QString BrowserController::labelForPinnedPath(const QString &path) const {
    const QString key = QDir::cleanPath(path);
    for (const DefaultPin &pin : defaultHomePins()) {
        if (pin.path == key)
            return pin.label;
    }
    const QFileInfo fi(key);
    return fi.fileName().isEmpty() ? key : fi.fileName();
}

QString BrowserController::symbolForPinnedPath(const QString &path) const {
    const QString key = QDir::cleanPath(path);
    if (m_folderIcons.contains(key))
        return m_folderIcons.value(key);
    for (const DefaultPin &pin : defaultHomePins()) {
        if (pin.path == key)
            return pin.symbol;
    }
    if (key == QDir::cleanPath(QDir::homePath()))
        return QStringLiteral("home");
    if (key == QDir::cleanPath(m_trash.trashPath()))
        return QStringLiteral("delete");
    return {}; // unknown dirs: no watermark / sidebar uses "folder" at display time
}

void BrowserController::seedDefaultPinsIfNeeded() {
    if (m_pinnedPlacesSeeded)
        return;
    const QString home = QDir::cleanPath(QDir::homePath());
    const QString trash = QDir::cleanPath(m_trash.trashPath());
    for (const DefaultPin &pin : defaultHomePins()) {
        if (pin.path.isEmpty() || pin.path == home || pin.path == trash)
            continue;
        if (!QFileInfo(pin.path).isDir())
            continue;
        if (m_pinnedPlaces.contains(pin.path))
            continue;
        m_pinnedPlaces.append(pin.path);
    }
    m_pinnedPlacesSeeded = true;
    saveSettings();
    emit placesChanged();
}

QVariantList BrowserController::places() const {
    // Fixed: Inicio → user pins (incl. seeded XDG) → Papelera.
    QVariantList list;
    list << builtinPlace(QStringLiteral("home"), QStringLiteral("Inicio"), QStringLiteral("user-home"),
                         QStringLiteral("home"), QDir::homePath());

    const QStringList builtins = builtinPlacePaths();
    for (const QString &raw : m_pinnedPlaces) {
        const QString path = QDir::cleanPath(raw);
        if (path.isEmpty() || builtins.contains(path))
            continue;
        QString icon = QStringLiteral("folder");
        for (const DefaultPin &pin : defaultHomePins()) {
            if (pin.path == path) {
                icon = pin.icon;
                break;
            }
        }
        QString symbol = symbolForPinnedPath(path);
        if (symbol.isEmpty())
            symbol = QStringLiteral("folder");
        list << QVariantMap{
            {QStringLiteral("id"), QStringLiteral("pin:") + path},
            {QStringLiteral("label"), labelForPinnedPath(path)},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("symbol"), symbol},
            {QStringLiteral("path"), path},
            {QStringLiteral("pinned"), true},
            {QStringLiteral("builtin"), false},
        };
    }

    list << builtinPlace(QStringLiteral("trash"), QStringLiteral("Papelera"), QStringLiteral("user-trash"),
                         QStringLiteral("delete"), m_trash.trashPath());
    return list;
}

bool BrowserController::isBuiltinPlace(const QString &path) const {
    if (path.isEmpty())
        return false;
    return builtinPlacePaths().contains(QDir::cleanPath(path));
}

bool BrowserController::isPinned(const QString &path) const {
    if (path.isEmpty())
        return false;
    return m_pinnedPlaces.contains(QDir::cleanPath(path));
}

void BrowserController::pinPlace(const QString &path) {
    const QString key = QDir::cleanPath(ensureLocal(path));
    if (key.isEmpty())
        return;
    const QFileInfo fi(key);
    if (!fi.exists() || !fi.isDir())
        return;
    if (isBuiltinPlace(key) || m_pinnedPlaces.contains(key))
        return;
    m_pinnedPlaces.append(key);
    savePinnedPlaces();
    emit placesChanged();
}

void BrowserController::unpinPlace(const QString &path) {
    const QString key = QDir::cleanPath(path);
    if (key.isEmpty())
        return;
    if (!m_pinnedPlaces.removeAll(key))
        return;
    savePinnedPlaces();
    emit placesChanged();
}

void BrowserController::reorderPinnedPlace(const QString &dragPath, const QString &targetPath, bool after) {
    const QString fromKey = QDir::cleanPath(dragPath);
    const QString toKey = QDir::cleanPath(targetPath);
    if (fromKey.isEmpty() || toKey.isEmpty() || fromKey == toKey)
        return;
    const int from = m_pinnedPlaces.indexOf(fromKey);
    int to = m_pinnedPlaces.indexOf(toKey);
    if (from < 0 || to < 0)
        return;
    m_pinnedPlaces.removeAt(from);
    if (from < to)
        --to;
    int insertAt = after ? to + 1 : to;
    insertAt = qBound(0, m_pinnedPlaces.size(), insertAt);
    m_pinnedPlaces.insert(insertAt, fromKey);
    savePinnedPlaces();
    emit placesChanged();
}

int BrowserController::pinPlacesFromUrls(const QVariantList &urls) {
    int added = 0;
    const QStringList builtins = builtinPlacePaths();
    for (const QVariant &v : urls) {
        QString raw;
        if (v.canConvert<QUrl>())
            raw = v.toUrl().isLocalFile() ? v.toUrl().toLocalFile() : v.toString();
        else
            raw = v.toString();
        const QString key = QDir::cleanPath(ensureLocal(raw));
        if (key.isEmpty())
            continue;
        const QFileInfo fi(key);
        if (!fi.exists() || !fi.isDir())
            continue;
        if (builtins.contains(key) || m_pinnedPlaces.contains(key))
            continue;
        m_pinnedPlaces.append(key);
        ++added;
    }
    if (added > 0) {
        savePinnedPlaces();
        emit placesChanged();
    }
    return added;
}

void BrowserController::savePinnedPlaces() const {
    // Persisted inside settings.json (same mkpath / merge as saveSettings).
    saveSettings();
}

void BrowserController::setSelectedPaths(const QStringList &paths) {
    if (paths == m_selected)
        return;
    m_selected = paths;
    emit selectionChanged();
}

bool BrowserController::selectionIsDir() const {
    if (m_selected.size() != 1)
        return false;
    return QFileInfo(m_selected.first()).isDir();
}

void BrowserController::setViewMode(const QString &mode) {
    QString next = mode.trimmed().toLower();
    if (next != QLatin1String("grid") && next != QLatin1String("list") && next != QLatin1String("details"))
        next = QStringLiteral("grid");
    if (next == m_viewMode)
        return;
    m_viewMode = next;
    emit viewModeChanged();
    saveSettings();
}

void BrowserController::setShowImagePreviews(bool show) {
    if (m_showImagePreviews == show)
        return;
    m_showImagePreviews = show;
    emit showImagePreviewsChanged();
    saveSettings();
}

void BrowserController::setShowFolderPreviews(bool show) {
    if (m_showFolderPreviews == show)
        return;
    m_showFolderPreviews = show;
    if (!show)
        clearFolderPreviewCache();
    else {
        ++m_folderPreviewsRevision;
        emit folderPreviewsChanged();
    }
    emit showFolderPreviewsChanged();
    saveSettings();
}

void BrowserController::setShowUpButton(bool show) {
    if (m_showUpButton == show)
        return;
    m_showUpButton = show;
    emit showUpButtonChanged();
    saveSettings();
}

void BrowserController::setShowRefreshButton(bool show) {
    if (m_showRefreshButton == show)
        return;
    m_showRefreshButton = show;
    emit showRefreshButtonChanged();
    saveSettings();
}

void BrowserController::setShowNewFolderButton(bool show) {
    if (m_showNewFolderButton == show)
        return;
    m_showNewFolderButton = show;
    emit showNewFolderButtonChanged();
    saveSettings();
}

void BrowserController::clearFolderPreviewCache() const {
    if (m_folderPreviewCache.isEmpty() && m_folderPreviewsRevision == 0)
        return;
    m_folderPreviewCache.clear();
    ++m_folderPreviewsRevision;
    emit const_cast<BrowserController *>(this)->folderPreviewsChanged();
}

QStringList BrowserController::folderPreviewPaths(const QString &dirPath, int maxCount) const {
    if (!m_showFolderPreviews || dirPath.isEmpty() || maxCount <= 0)
        return {};

    const QString clean = QDir::cleanPath(dirPath);
    const QFileInfo di(clean);
    if (!di.isDir())
        return {};

    const qint64 mtime = di.lastModified().toSecsSinceEpoch();
    const auto cached = m_folderPreviewCache.constFind(clean);
    if (cached != m_folderPreviewCache.cend() && cached->dirMtime == mtime)
        return cached->paths;

    QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
    if (m_model && m_model->showHidden())
        filters |= QDir::Hidden;

    const QDir dir(clean);
    const QStringList names = dir.entryList(filters, QDir::Name);
    QMimeDatabase mimeDb;
    QStringList paths;
    paths.reserve(qMin(maxCount, names.size()));
    for (const QString &name : names) {
        const QString fp = dir.filePath(name);
        const auto mime = mimeDb.mimeTypeForFile(QFileInfo(fp), QMimeDatabase::MatchExtension);
        if (!mime.name().startsWith(QLatin1String("image/")))
            continue;
        paths.append(fp);
        if (paths.size() >= maxCount)
            break;
    }

    FolderPreviewCacheEntry entry;
    entry.dirMtime = mtime;
    entry.paths = paths;
    m_folderPreviewCache.insert(clean, entry);
    return paths;
}

QString BrowserController::expandTypedPath(const QString &typed) const {
    QString raw = typed.trimmed();
    const QString home = QDir::homePath();
    if (raw.isEmpty())
        return {};
    if (raw == QLatin1String("Inicio") || raw == QLatin1String("~"))
        return home;
    if (raw.startsWith(QLatin1String("~/")))
        return home + raw.mid(1);
    if (raw.startsWith(QLatin1Char('/')))
        return raw;
    // Relative → under home (matches displayPath-style typing).
    return home + QLatin1Char('/') + raw;
}

QVariantList BrowserController::pathSuggestions(const QString &typed, int maxCount) const {
    const int limit = maxCount > 0 ? maxCount : 14;
    const QString home = QDir::cleanPath(QDir::homePath());
    const QString raw = typed.trimmed();

    QVariantList out;
    QSet<QString> seen;

    auto push = [&](const QString &path) {
        if (out.size() >= limit)
            return;
        const QString clean = QDir::cleanPath(path);
        if (clean.isEmpty() || seen.contains(clean))
            return;
        const QFileInfo fi(clean);
        if (!fi.exists() || !fi.isDir())
            return;
        seen.insert(clean);

        QString label;
        if (clean == home)
            label = QStringLiteral("Inicio");
        else if (clean == QLatin1String("/"))
            label = QStringLiteral("/");
        else
            label = fi.fileName().isEmpty() ? clean : fi.fileName();

        QString symbol = folderSymbol(clean);
        if (symbol.isEmpty())
            symbol = QStringLiteral("folder");

        out.append(QVariantMap{
            {QStringLiteral("path"), clean},
            {QStringLiteral("label"), label},
            {QStringLiteral("symbol"), symbol},
        });
    };

    // Focus = typed path if it is a dir; otherwise its parent (partial name).
    QString focus;
    if (raw.isEmpty() || raw == QLatin1String("~") || raw == QLatin1String("Inicio")) {
        focus = home;
    } else if (raw.endsWith(QLatin1Char('/'))) {
        focus = QDir::cleanPath(expandTypedPath(raw));
    } else {
        const QString expanded = QDir::cleanPath(expandTypedPath(raw));
        const QFileInfo fi(expanded);
        focus = (fi.exists() && fi.isDir()) ? fi.absoluteFilePath() : fi.absolutePath();
    }

    // Only upward hierarchy: here → … → /.
    QString cur = focus;
    int guard = 0;
    while (!cur.isEmpty() && guard++ < 64) {
        push(cur);
        if (cur == QLatin1String("/"))
            break;
        const QString parent = QFileInfo(cur).absolutePath();
        if (parent.isEmpty() || parent == cur)
            break;
        cur = parent;
    }

    return out;
}

void BrowserController::toggleViewMode() {
    cycleViewMode(1);
}

void BrowserController::cycleViewMode(int delta) {
    static const QStringList kModes = {
        QStringLiteral("grid"),
        QStringLiteral("list"),
        QStringLiteral("details"),
    };
    int i = kModes.indexOf(m_viewMode);
    if (i < 0)
        i = 0;
    const int n = kModes.size();
    int step = delta % n;
    if (step < 0)
        step += n;
    setViewMode(kModes.at((i + step) % n));
}

QString BrowserController::viewModeLabel(const QString &mode) const {
    const QString m = mode.trimmed().isEmpty() ? m_viewMode : mode.trimmed().toLower();
    if (m == QLatin1String("list"))
        return QStringLiteral("Lista");
    if (m == QLatin1String("details"))
        return QStringLiteral("Detalles");
    return QStringLiteral("Cuadrícula");
}

QString BrowserController::viewModeIcon(const QString &mode) const {
    const QString m = mode.trimmed().isEmpty() ? m_viewMode : mode.trimmed().toLower();
    if (m == QLatin1String("list"))
        return QStringLiteral("view_list");
    if (m == QLatin1String("details"))
        return QStringLiteral("view_column");
    return QStringLiteral("grid_view");
}

QString BrowserController::formatSize(qint64 bytes) const {
    if (bytes < 0)
        return {};
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    double v = double(bytes);
    int u = 0;
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    const int decimals = (v >= 10.0 || u == 0) ? 0 : 1;
    return QString::number(v, 'f', decimals) + QLatin1Char(' ') + QLatin1String(units[u]);
}

QString BrowserController::ensureLocal(const QString &pathOrUri) const {
    const QUrl url(pathOrUri);
    if (url.isLocalFile())
        return url.toLocalFile();
    if (pathOrUri.startsWith(QLatin1String("file:")))
        return QUrl(pathOrUri).toLocalFile();
    return pathOrUri;
}

void BrowserController::pushHistory(const QString &path) {
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size() && m_history.at(m_historyIndex) == path)
        return;
    if (m_historyIndex + 1 < m_history.size())
        m_history = m_history.mid(0, m_historyIndex + 1);
    m_history.append(path);
    m_historyIndex = m_history.size() - 1;
    emit historyChanged();
}

void BrowserController::navigate(const QString &path) {
    QString raw = path.trimmed();
    const QString home = QDir::homePath();
    if (raw == QLatin1String("Inicio") || raw == QLatin1String("~"))
        raw = home;
    else if (raw.startsWith(QLatin1String("~/")))
        raw = home + raw.mid(1);

    const QString local = ensureLocal(raw);
    QFileInfo fi(local);
    QString target = QDir::cleanPath(local);
    if (fi.exists() && fi.isFile())
        target = fi.absolutePath();
    if (!QDir(target).exists()) {
        emit errorOccurred(tr("No se puede abrir: %1").arg(local));
        return;
    }
    m_model->setPath(target);
    pushHistory(target);
    clearSelection();
    emit pathChanged();
}

void BrowserController::openPath(const QString &path) {
    const QString local = ensureLocal(path);
    QFileInfo fi(local);
    if (!fi.exists()) {
        emit errorOccurred(tr("No existe: %1").arg(local));
        return;
    }
    if (fi.isDir()) {
        navigate(local);
        return;
    }

    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(fi).name();
    const QString desktop = OpenWithController::defaultDesktopForMime(mime);
    if (desktop.isEmpty()) {
        // No default handler — Seeker dialog instead of the GTK xdg-open chooser.
        if (m_openWith) {
            m_openWith->openPath(local);
            return;
        }
        emit errorOccurred(tr("Ninguna aplicación asociada a %1").arg(mime));
        return;
    }

    if (OpenWithController::launchDesktop(desktop, local))
        return;

    // Default .desktop missing/broken — same Seeker sheet as «Abrir con…».
    if (m_openWith) {
        m_openWith->openPath(local);
        return;
    }
    emit errorOccurred(tr("No se pudo abrir: %1").arg(local));
}

void BrowserController::openTerminalHere(const QString &dir) {
    QString path = dir.trimmed().isEmpty() ? currentPath() : ensureLocal(dir);
    QFileInfo fi(path);
    if (fi.exists() && !fi.isDir())
        path = fi.absolutePath();
    if (!QFileInfo(path).isDir()) {
        emit errorOccurred(tr("No es una carpeta: %1").arg(path));
        return;
    }

    auto tryDetach = [](const QString &bin, const QStringList &args) -> bool {
        if (QStandardPaths::findExecutable(bin).isEmpty())
            return false;
        return QProcess::startDetached(bin, args);
    };

    // Desktop default (Arch / portals), then kitty (Sylphrena default), then $TERMINAL.
    if (tryDetach(QStringLiteral("xdg-terminal-exec"),
                  {QStringLiteral("--working-directory=%1").arg(path)}))
        return;
    if (tryDetach(QStringLiteral("kitty"), {QStringLiteral("--directory"), path}))
        return;

    const QByteArray termEnv = qgetenv("TERMINAL");
    if (!termEnv.isEmpty()) {
        const QString term = QString::fromLocal8Bit(termEnv);
        QStringList args;
        if (term.contains(QLatin1String("kitty")))
            args = {QStringLiteral("--directory"), path};
        else if (term.contains(QLatin1String("foot"))
                 || term.contains(QLatin1String("alacritty"))
                 || term.contains(QLatin1String("wezterm")))
            args = {QStringLiteral("--working-directory"), path};

        if (!args.isEmpty() && QProcess::startDetached(term, args))
            return;

        QProcess proc;
        proc.setProgram(term);
        proc.setWorkingDirectory(path);
        if (proc.startDetached())
            return;
    }

    emit errorOccurred(tr("No se pudo abrir un terminal"));
}

void BrowserController::goUp() {
    QDir dir(currentPath());
    if (dir.cdUp())
        navigate(dir.absolutePath());
}

void BrowserController::goBack() {
    if (!canGoBack())
        return;
    --m_historyIndex;
    m_model->setPath(m_history.at(m_historyIndex));
    clearSelection();
    emit historyChanged();
    emit pathChanged();
}

void BrowserController::goForward() {
    if (!canGoForward())
        return;
    ++m_historyIndex;
    m_model->setPath(m_history.at(m_historyIndex));
    clearSelection();
    emit historyChanged();
    emit pathChanged();
}

void BrowserController::refresh() {
    clearFolderPreviewCache();
    m_model->refresh();
}

void BrowserController::copySelection() {
    if (m_selected.isEmpty())
        return;
    m_clipboard = m_selected;
    m_clipboardCut = false;
    emit clipboardChanged();
}

void BrowserController::cutSelection() {
    if (m_selected.isEmpty())
        return;
    m_clipboard = m_selected;
    m_clipboardCut = true;
    emit clipboardChanged();
}

QString BrowserController::uniqueDestPath(const QString &destDir, const QFileInfo &fi) const {
    QString dest = QDir(destDir).filePath(fi.fileName());
    if (!QFile::exists(dest))
        return dest;

    const QString base = fi.completeBaseName().isEmpty() ? fi.fileName() : fi.completeBaseName();
    const QString suffix = fi.isDir() || fi.suffix().isEmpty()
        ? QString()
        : (QStringLiteral(".") + fi.suffix());
    // Directories / extensionless: "name (N)". Files: "name (N).ext".
    const QString stem = fi.isDir() ? fi.fileName() : base;
    int n = 1;
    do {
        dest = QDir(destDir).filePath(QStringLiteral("%1 (%2)%3").arg(stem).arg(n++).arg(suffix));
    } while (QFile::exists(dest));
    return dest;
}

QString BrowserController::uniqueNameIn(const QString &dir, const QString &baseName) const {
    QString name = baseName;
    if (name.isEmpty())
        name = tr("Carpeta nueva");
    QString dest = QDir(dir).filePath(name);
    if (!QFile::exists(dest))
        return name;
    int n = 1;
    do {
        dest = QDir(dir).filePath(QStringLiteral("%1 (%2)").arg(name).arg(n++));
    } while (QFile::exists(dest));
    return QFileInfo(dest).fileName();
}

bool BrowserController::copyPath(const QString &src, const QString &dest) const {
    return copyRecursively(src, dest);
}

bool BrowserController::removePath(const QString &path) const {
    return removeRecursively(path);
}

void BrowserController::pushUndo(UndoOp op) {
    m_undo.append(std::move(op));
    // Cap history — file ops are heavy.
    while (m_undo.size() > 32)
        m_undo.removeFirst();
    emit undoChanged();
}

void BrowserController::paste() {
    if (m_clipboard.isEmpty())
        return;
    const QString destDir = currentPath();
    const bool wasCut = m_clipboardCut;
    QStringList sources;
    QStringList pasted;
    for (const QString &src : m_clipboard) {
        const QFileInfo fi(src);
        if (!fi.exists())
            continue;
        const QString dest = uniqueDestPath(destDir, fi);
        bool ok = false;
        if (wasCut) {
            ok = QFile::rename(src, dest);
            if (!ok)
                ok = copyPath(src, dest) && removePath(src);
        } else {
            ok = copyPath(src, dest);
        }
        if (!ok)
            emit errorOccurred(tr("No se pudo pegar: %1").arg(fi.fileName()));
        else {
            sources << src;
            pasted << dest;
        }
    }
    if (wasCut) {
        m_clipboard.clear();
        m_clipboardCut = false;
        emit clipboardChanged();
    }
    refresh();
    if (!pasted.isEmpty()) {
        UndoOp op;
        op.kind = wasCut ? UndoKind::PasteMove : UndoKind::PasteCopy;
        op.from = sources;
        op.to = pasted;
        pushUndo(std::move(op));
        setSelectedPaths(pasted);
    }
}

QString BrowserController::uriList(const QStringList &paths) const {
    QStringList uris;
    uris.reserve(paths.size());
    for (const QString &p : paths) {
        const QString local = ensureLocal(p);
        if (!local.isEmpty())
            uris << QUrl::fromLocalFile(local).toString();
    }
    return uris.join(QLatin1Char('\n'));
}

void BrowserController::dropUrls(const QVariantList &urls, const QString &destDirRaw, bool copy) {
    const QString destDir = QDir::cleanPath(ensureLocal(destDirRaw));
    if (destDir.isEmpty() || !QFileInfo(destDir).isDir()) {
        emit errorOccurred(tr("Destino de soltado no válido"));
        return;
    }

    QStringList sources;
    sources.reserve(urls.size());
    for (const QVariant &v : urls) {
        QString raw;
        if (v.canConvert<QUrl>())
            raw = v.toUrl().isLocalFile() ? v.toUrl().toLocalFile() : v.toUrl().toString();
        else
            raw = v.toString();
        const QString local = QDir::cleanPath(ensureLocal(raw));
        if (local.isEmpty() || !QFileInfo::exists(local))
            continue;
        if (!sources.contains(local))
            sources << local;
    }
    if (sources.isEmpty())
        return;

    // Drop onto trash → trash entries (not a plain move into ~/.local/share/Trash/files).
    const QString trashDir = QDir::cleanPath(m_trash.trashPath());
    if (!trashDir.isEmpty() && destDir == trashDir) {
        setSelectedPaths(sources);
        trashSelection();
        return;
    }

    QStringList from;
    QStringList to;
    for (const QString &src : sources) {
        const QFileInfo fi(src);
        // No-op: already in destination with same name / same path.
        if (fi.absolutePath() == destDir && QDir(destDir).filePath(fi.fileName()) == src)
            continue;
        // Refuse nesting a folder into itself or a descendant.
        if (fi.isDir()) {
            const QString srcSlash = src.endsWith(QLatin1Char('/')) ? src : src + QLatin1Char('/');
            if (destDir == src || destDir.startsWith(srcSlash)) {
                emit errorOccurred(tr("No se puede soltar una carpeta dentro de sí misma"));
                continue;
            }
        }

        const QString dest = uniqueDestPath(destDir, fi);
        bool ok = false;
        if (copy) {
            ok = copyPath(src, dest);
        } else {
            ok = QFile::rename(src, dest);
            if (!ok)
                ok = copyPath(src, dest) && removePath(src);
        }
        if (!ok)
            emit errorOccurred(tr("No se pudo soltar: %1").arg(fi.fileName()));
        else {
            from << src;
            to << dest;
        }
    }

    refresh();
    if (!to.isEmpty()) {
        UndoOp op;
        op.kind = copy ? UndoKind::PasteCopy : UndoKind::PasteMove;
        op.from = from;
        op.to = to;
        pushUndo(std::move(op));
        setSelectedPaths(to);
    }
}

bool BrowserController::inTrash() const {
    const QString trash = QDir::cleanPath(m_trash.trashPath());
    if (trash.isEmpty())
        return false;
    return QDir::cleanPath(currentPath()) == trash;
}

bool BrowserController::trashIsEmpty() const {
    const QDir dir(m_trash.trashPath());
    if (!dir.exists())
        return true;
    return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).isEmpty();
}

void BrowserController::trashSelection() {
    if (m_selected.isEmpty())
        return;
    const QStringList targets = m_selected;

    // Already in the trash → permanent delete (and drop matching .trashinfo).
    if (inTrash()) {
        // trashPath is .../Trash/files → parent is Trash root
        const QString infoRoot = QDir(QFileInfo(m_trash.trashPath()).absolutePath())
                                     .filePath(QStringLiteral("info"));
        for (const QString &p : targets) {
            const QFileInfo fi(p);
            if (fi.isDir() && !fi.isSymLink())
                QDir(p).removeRecursively();
            else
                QFile::remove(p);
            QFile::remove(QDir(infoRoot).filePath(fi.fileName() + QStringLiteral(".trashinfo")));
        }
        clearSelection();
        refresh();
        emit pathChanged();
        return;
    }

    const QVector<TrashEntry> moved = m_trash.moveToTrashEntries(targets);
    if (moved.isEmpty()) {
        emit errorOccurred(tr("No se pudo mover a la papelera"));
        refresh();
        return;
    }

    UndoOp op;
    op.kind = UndoKind::Trash;
    for (const TrashEntry &e : moved) {
        op.from << e.originalPath;
        op.to << e.trashFilePath;
    }
    pushUndo(std::move(op));

    QStringList still;
    for (const QString &p : targets) {
        if (QFileInfo::exists(p))
            still << p;
    }
    setSelectedPaths(still);
    refresh();
}

void BrowserController::emptyTrash() {
    if (!m_trash.emptyTrash())
        emit errorOccurred(tr("No se pudo vaciar la papelera"));
    clearSelection();
    // Drop undo entries that pointed at trash contents — they're gone.
    m_undo.clear();
    emit undoChanged();
    refresh();
    emit pathChanged(); // refresh trashIsEmpty bindings
}

void BrowserController::renameSelected(const QString &newName) {
    if (m_selected.size() != 1 || newName.trimmed().isEmpty())
        return;
    const QString src = m_selected.first();
    const QString dest = QDir(QFileInfo(src).absolutePath()).filePath(newName.trimmed());
    if (dest == src)
        return;
    if (QFile::exists(dest)) {
        emit errorOccurred(tr("Ya existe: %1").arg(newName.trimmed()));
        return;
    }
    if (!QFile::rename(src, dest)) {
        emit errorOccurred(tr("No se pudo renombrar"));
        return;
    }
    pushUndo(UndoOp{UndoKind::Rename, {src}, {dest}});
    refresh();
    setSelectedPaths({dest});
}

QString BrowserController::createFolder(const QString &preferredName) {
    const QString raw = preferredName.trimmed();
    if (raw.isEmpty()) {
        emit errorOccurred(tr("El nombre no puede estar vacío"));
        return {};
    }
    if (raw.contains(QLatin1Char('/')) || raw == QLatin1String(".") || raw == QLatin1String("..")) {
        emit errorOccurred(tr("Nombre no válido"));
        return {};
    }
    const QString dir = currentPath();
    const QString name = uniqueNameIn(dir, raw);
    const QString path = QDir(dir).filePath(name);
    if (!QDir().mkdir(path)) {
        emit errorOccurred(tr("No se pudo crear la carpeta"));
        return {};
    }
    pushUndo(UndoOp{UndoKind::CreateFolder, {}, {path}});
    refresh();
    setSelectedPaths({path});
    emit folderCreated(path);
    return path;
}

void BrowserController::undo() {
    if (m_undo.isEmpty()) {
        emit errorOccurred(tr("Nada que deshacer"));
        return;
    }
    const UndoOp op = m_undo.takeLast();
    emit undoChanged();

    QStringList restored;
    bool ok = true;

    switch (op.kind) {
    case UndoKind::Trash:
        for (int i = 0; i < op.from.size(); ++i) {
            const QString trashPath = i < op.to.size() ? op.to.at(i) : QString();
            if (m_trash.restore(trashPath, op.from.at(i))) {
                // Prefer the path that actually exists after restore (may be " (restaurado)").
                if (QFileInfo::exists(op.from.at(i)))
                    restored << op.from.at(i);
                else
                    restored << op.from.at(i);
            } else {
                ok = false;
            }
        }
        break;
    case UndoKind::Rename:
        if (op.from.size() == 1 && op.to.size() == 1) {
            if (QFile::rename(op.to.first(), op.from.first()))
                restored << op.from.first();
            else
                ok = false;
        }
        break;
    case UndoKind::CreateFolder:
        for (const QString &path : op.to) {
            if (QFileInfo::exists(path) && !removePath(path))
                ok = false;
        }
        break;
    case UndoKind::PasteCopy:
    case UndoKind::Duplicate:
        for (const QString &path : op.to) {
            if (QFileInfo::exists(path) && !removePath(path))
                ok = false;
        }
        break;
    case UndoKind::PasteMove:
        for (int i = 0; i < op.to.size() && i < op.from.size(); ++i) {
            const QString &src = op.to.at(i);
            const QString &dest = op.from.at(i);
            QDir().mkpath(QFileInfo(dest).absolutePath());
            bool moved = QFile::rename(src, dest);
            if (!moved)
                moved = copyPath(src, dest) && removePath(src);
            if (moved)
                restored << dest;
            else
                ok = false;
        }
        break;
    }

    if (!ok)
        emit errorOccurred(tr("No se pudo deshacer por completo"));
    refresh();
    if (!restored.isEmpty())
        setSelectedPaths(restored);
    else
        clearSelection();
}

int BrowserController::currentIndex() const {
    if (m_selected.isEmpty() || !m_model)
        return -1;
    const QString target = m_selected.first();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->data(m_model->index(i, 0), DirModel::PathRole).toString() == target)
            return i;
    }
    return -1;
}

QString BrowserController::pathAt(int index) const {
    if (!m_model || index < 0 || index >= m_model->rowCount())
        return {};
    return m_model->data(m_model->index(index, 0), DirModel::PathRole).toString();
}

bool BrowserController::pathIsDir(const QString &path) const {
    const QString local = ensureLocal(path);
    return !local.isEmpty() && QFileInfo(local).isDir();
}

void BrowserController::selectIndex(int index) {
    const QString path = pathAt(index);
    if (path.isEmpty())
        return;
    setSelectedPaths({path});
}

void BrowserController::moveSelection(int delta) {
    if (!m_model || m_model->rowCount() <= 0 || delta == 0)
        return;
    int idx = currentIndex();
    if (idx < 0)
        idx = delta > 0 ? 0 : m_model->rowCount() - 1;
    else
        idx = qBound(0, m_model->rowCount() - 1, idx + delta);
    selectIndex(idx);
}

void BrowserController::openSelection() {
    if (m_selected.isEmpty())
        return;
    openPath(m_selected.first());
}

QString BrowserController::duplicateSelection() {
    if (m_selected.size() != 1)
        return {};
    const QString src = m_selected.first();
    const QFileInfo fi(src);
    const QString dest = uniqueDestPath(fi.absolutePath(), fi);
    if (!copyPath(src, dest)) {
        emit errorOccurred(tr("No se pudo duplicar"));
        return {};
    }
    pushUndo(UndoOp{UndoKind::Duplicate, {src}, {dest}});
    refresh();
    setSelectedPaths({dest});
    return dest;
}

void BrowserController::selectAll() {
    QStringList all;
    for (int i = 0; i < m_model->rowCount(); ++i)
        all << m_model->data(m_model->index(i, 0), DirModel::PathRole).toString();
    setSelectedPaths(all);
}

void BrowserController::clearSelection() {
    setSelectedPaths({});
}

QString BrowserController::iconUrl(const QString &iconName) const {
    if (iconName.isEmpty())
        return {};
    const QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull())
        return {};
    QString url = QStringLiteral("image://theme/%1").arg(iconName);
    // Bust image cache + drive folder accent when the Material You primary changes.
    const bool folderIcon = iconName == QLatin1String("folder")
        || iconName == QLatin1String("inode-directory")
        || iconName.startsWith(QLatin1String("folder-"));
    if (folderIcon && m_theme) {
        // Match chrome accents (sidebar pill, buttons) — primary is too bright alone.
        const QString hex = m_theme->primaryContainer().name(QColor::HexRgb).mid(1); // rrggbb
        url += QStringLiteral("?c=%1").arg(hex);
    }
    return url;
}

QString BrowserController::thumbUrl(const QString &path, int edge) const {
    if (path.isEmpty())
        return {};
    const int e = qBound(32, 512, edge > 0 ? edge : 160);
    return QStringLiteral("image://thumb/%1/%2")
        .arg(e)
        .arg(QString::fromLatin1(path.toUtf8().toHex()));
}

QString BrowserController::placeSymbol(const QString &path) const {
    if (path.isEmpty())
        return {};
    const QString want = QDir::cleanPath(path);
    if (want == QDir::cleanPath(QDir::homePath()))
        return QStringLiteral("home");
    if (want == QDir::cleanPath(m_trash.trashPath()))
        return QStringLiteral("delete");
    for (const DefaultPin &pin : defaultHomePins()) {
        if (pin.path == want)
            return pin.symbol;
    }
    return {};
}

QString BrowserController::iconsStatePath() const {
    return QDir::home().filePath(QStringLiteral(".local/state/seeker/folder-icons.json"));
}

QString BrowserController::settingsPath() const {
    return QDir::home().filePath(QStringLiteral(".config/seeker/settings.json"));
}

void BrowserController::loadSettings() {
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    const QString mode = obj.value(QStringLiteral("viewMode")).toString();
    if (mode == QLatin1String("list") || mode == QLatin1String("grid") || mode == QLatin1String("details"))
        m_viewMode = mode;
    if (obj.contains(QStringLiteral("showImagePreviews")))
        m_showImagePreviews = obj.value(QStringLiteral("showImagePreviews")).toBool();
    if (obj.contains(QStringLiteral("showFolderPreviews")))
        m_showFolderPreviews = obj.value(QStringLiteral("showFolderPreviews")).toBool();
    if (obj.contains(QStringLiteral("showUpButton")))
        m_showUpButton = obj.value(QStringLiteral("showUpButton")).toBool();
    if (obj.contains(QStringLiteral("showRefreshButton")))
        m_showRefreshButton = obj.value(QStringLiteral("showRefreshButton")).toBool();
    if (obj.contains(QStringLiteral("showNewFolderButton")))
        m_showNewFolderButton = obj.value(QStringLiteral("showNewFolderButton")).toBool();
    m_pinnedPlaces.clear();
    const QJsonArray pinned = obj.value(QStringLiteral("pinnedPlaces")).toArray();
    for (const QJsonValue &v : pinned) {
        const QString p = QDir::cleanPath(v.toString().trimmed());
        if (p.isEmpty() || m_pinnedPlaces.contains(p))
            continue;
        m_pinnedPlaces.append(p);
    }
    // Missing key → false → seed once (also migrates older installs off fixed XDG builtins).
    m_pinnedPlacesSeeded = obj.value(QStringLiteral("pinnedPlacesSeeded")).toBool(false);
}

void BrowserController::saveSettings() const {
    const QString path = settingsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject obj;
    // Preserve unknown keys if the file already exists.
    {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(existing.readAll());
            if (doc.isObject())
                obj = doc.object();
        }
    }
    obj.insert(QStringLiteral("viewMode"), m_viewMode);
    obj.insert(QStringLiteral("showImagePreviews"), m_showImagePreviews);
    obj.insert(QStringLiteral("showFolderPreviews"), m_showFolderPreviews);
    obj.insert(QStringLiteral("showUpButton"), m_showUpButton);
    obj.insert(QStringLiteral("showRefreshButton"), m_showRefreshButton);
    obj.insert(QStringLiteral("showNewFolderButton"), m_showNewFolderButton);
    QJsonArray pinned;
    for (const QString &p : m_pinnedPlaces)
        pinned.append(p);
    obj.insert(QStringLiteral("pinnedPlaces"), pinned);
    obj.insert(QStringLiteral("pinnedPlacesSeeded"), m_pinnedPlacesSeeded);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void BrowserController::loadFolderIcons() {
    m_folderIcons.clear();
    QFile f(iconsStatePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString sym = it.value().toString().trimmed();
        if (sym.isEmpty())
            continue;
        m_folderIcons.insert(QDir::cleanPath(it.key()), sym);
    }
}

void BrowserController::saveFolderIcons() const {
    const QString path = iconsStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject obj;
    for (auto it = m_folderIcons.constBegin(); it != m_folderIcons.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QString BrowserController::folderSymbol(const QString &path) const {
    if (path.isEmpty())
        return {};
    return symbolForPinnedPath(path);
}

bool BrowserController::hasCustomFolderIcon(const QString &path) const {
    if (path.isEmpty())
        return false;
    return m_folderIcons.contains(QDir::cleanPath(path));
}

void BrowserController::setFolderIcon(const QString &path, const QString &symbol) {
    const QString key = QDir::cleanPath(path);
    const QString sym = symbol.trimmed();
    if (key.isEmpty() || sym.isEmpty())
        return;
    if (m_folderIcons.value(key) == sym)
        return;
    m_folderIcons.insert(key, sym);
    ++m_folderIconsRevision;
    saveFolderIcons();
    emit folderIconsChanged();
    emit placesChanged(); // pinned sidebar rows bind symbol from folder icons
}

void BrowserController::clearFolderIcon(const QString &path) {
    const QString key = QDir::cleanPath(path);
    if (!m_folderIcons.remove(key))
        return;
    ++m_folderIconsRevision;
    saveFolderIcons();
    emit folderIconsChanged();
    emit placesChanged();
}

void BrowserController::ensureMaterialSymbolsLoaded() const {
    if (m_materialSymbolsLoaded)
        return;
    m_materialSymbolsLoaded = true;
    QFile f(QStringLiteral(":/data/material-symbols.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            m_materialSymbols.append(line);
    }
}

QStringList BrowserController::filterMaterialSymbols(const QString &query, int limit) const {
    ensureMaterialSymbolsLoaded();
    const int cap = qBound(24, 800, limit);
    QStringList out;
    out.reserve(cap);

    const QString q = query.trimmed().toLower().replace(QLatin1Char(' '), QLatin1Char('_'));

    static const QStringList featured = {
        QStringLiteral("home"),        QStringLiteral("folder"),
        QStringLiteral("description"), QStringLiteral("download"),
        QStringLiteral("image"),       QStringLiteral("music_note"),
        QStringLiteral("video_library"), QStringLiteral("delete"),
        QStringLiteral("star"),        QStringLiteral("favorite"),
        QStringLiteral("work"),        QStringLiteral("school"),
        QStringLiteral("sports_esports"), QStringLiteral("code"),
        QStringLiteral("terminal"),    QStringLiteral("cloud"),
        QStringLiteral("lock"),        QStringLiteral("pets"),
        QStringLiteral("coffee"),      QStringLiteral("palette"),
        QStringLiteral("photo_camera"), QStringLiteral("movie"),
        QStringLiteral("headphones"),  QStringLiteral("games"),
        QStringLiteral("science"),     QStringLiteral("biotech"),
        QStringLiteral("bolt"),        QStringLiteral("favorite"),
        QStringLiteral("bookmark"),    QStringLiteral("inventory_2"),
        QStringLiteral("archive"),     QStringLiteral("folder_shared"),
        QStringLiteral("account_circle"), QStringLiteral("settings"),
        QStringLiteral("build"),       QStringLiteral("handyman"),
        QStringLiteral("brush"),       QStringLiteral("design_services"),
        QStringLiteral("travel_explore"), QStringLiteral("public"),
        QStringLiteral("language"),    QStringLiteral("mail"),
        QStringLiteral("chat"),        QStringLiteral("call"),
        QStringLiteral("calendar_month"), QStringLiteral("schedule"),
        QStringLiteral("shopping_bag"), QStringLiteral("payments"),
        QStringLiteral("database"),    QStringLiteral("dns"),
        QStringLiteral("memory"),      QStringLiteral("computer"),
        QStringLiteral("smartphone"),  QStringLiteral("headphones"),
        QStringLiteral("restaurant"),  QStringLiteral("local_cafe"),
        QStringLiteral("fitness_center"), QStringLiteral("spa"),
        QStringLiteral("flight"),      QStringLiteral("directions_car"),
        QStringLiteral("map"),         QStringLiteral("place"),
        QStringLiteral("celebration"), QStringLiteral("cake"),
        QStringLiteral("emoji_emotions"), QStringLiteral("interests"),
        QStringLiteral("lightbulb"),   QStringLiteral("psychology"),
        QStringLiteral("security"),    QStringLiteral("vpn_key"),
        QStringLiteral("verified"),    QStringLiteral("workspace_premium"),
    };

    auto pushUnique = [&](const QString &name) {
        if (out.size() >= cap)
            return false;
        if (out.contains(name))
            return true;
        out.append(name);
        return out.size() < cap;
    };

    if (q.isEmpty()) {
        for (const QString &name : featured) {
            if (!pushUnique(name))
                return out;
        }
        for (const QString &name : m_materialSymbols) {
            if (!pushUnique(name))
                return out;
        }
        return out;
    }

    // Prefer prefix matches, then substring.
    QStringList rest;
    for (const QString &name : m_materialSymbols) {
        if (name.startsWith(q)) {
            if (!pushUnique(name))
                return out;
        } else if (name.contains(q)) {
            rest.append(name);
        }
    }
    for (const QString &name : rest) {
        if (!pushUnique(name))
            break;
    }
    return out;
}
