#include "BrowserController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

BrowserController::BrowserController(QObject *parent)
    : QObject(parent)
    , m_model(new DirModel(this)) {
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
        return QStringLiteral("~") + p.mid(home.size());
    return p;
}

QVariantList BrowserController::places() const {
    auto place = [](const QString &id, const QString &label, const QString &icon, const QString &path) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("path"), path},
        };
    };

    QVariantList list;
    list << place(QStringLiteral("home"), QStringLiteral("Inicio"), QStringLiteral("home"), QDir::homePath());
    list << place(QStringLiteral("desktop"), QStringLiteral("Escritorio"), QStringLiteral("desktop_windows"),
                  QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    list << place(QStringLiteral("documents"), QStringLiteral("Documentos"), QStringLiteral("description"),
                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    list << place(QStringLiteral("downloads"), QStringLiteral("Descargas"), QStringLiteral("download"),
                  QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    list << place(QStringLiteral("pictures"), QStringLiteral("Imágenes"), QStringLiteral("image"),
                  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    list << place(QStringLiteral("music"), QStringLiteral("Música"), QStringLiteral("music_note"),
                  QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    list << place(QStringLiteral("videos"), QStringLiteral("Vídeos"), QStringLiteral("movie"),
                  QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    list << place(QStringLiteral("trash"), QStringLiteral("Papelera"), QStringLiteral("delete"), m_trash.trashPath());
    return list;
}

void BrowserController::setSelectedPaths(const QStringList &paths) {
    if (paths == m_selected)
        return;
    m_selected = paths;
    emit selectionChanged();
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
    const QString local = ensureLocal(path);
    QFileInfo fi(local);
    QString target = local;
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
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(local)))
        QProcess::startDetached(QStringLiteral("xdg-open"), {local});
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
    m_model->refresh();
}

void BrowserController::copySelection() {
    m_clipboard = m_selected;
    m_clipboardCut = false;
    emit clipboardChanged();
}

void BrowserController::cutSelection() {
    m_clipboard = m_selected;
    m_clipboardCut = true;
    emit clipboardChanged();
}

void BrowserController::paste() {
    if (m_clipboard.isEmpty())
        return;
    const QString destDir = currentPath();
    for (const QString &src : m_clipboard) {
        const QFileInfo fi(src);
        QString dest = QDir(destDir).filePath(fi.fileName());
        int n = 1;
        while (QFile::exists(dest))
            dest = QDir(destDir).filePath(QStringLiteral("%1 (%2)").arg(fi.completeBaseName()).arg(n++)
                                          + (fi.suffix().isEmpty() ? QString() : QStringLiteral(".") + fi.suffix()));

        if (fi.isDir()) {
            // Lightweight: skip deep copy of trees in v0 cut/paste for dirs — navigate only supports files for cut.
            if (m_clipboardCut)
                QDir().rename(src, dest);
            continue;
        }
        if (m_clipboardCut) {
            if (!QFile::rename(src, dest)) {
                if (QFile::copy(src, dest))
                    QFile::remove(src);
            }
        } else {
            QFile::copy(src, dest);
        }
    }
    if (m_clipboardCut) {
        m_clipboard.clear();
        m_clipboardCut = false;
        emit clipboardChanged();
    }
    refresh();
}

void BrowserController::trashSelection() {
    if (m_selected.isEmpty())
        return;
    if (!m_trash.moveToTrash(m_selected))
        emit errorOccurred(tr("No se pudo mover a la papelera"));
    clearSelection();
    refresh();
}

void BrowserController::renameSelected(const QString &newName) {
    if (m_selected.size() != 1 || newName.trimmed().isEmpty())
        return;
    const QString src = m_selected.first();
    const QString dest = QDir(QFileInfo(src).absolutePath()).filePath(newName.trimmed());
    if (!QFile::rename(src, dest))
        emit errorOccurred(tr("No se pudo renombrar"));
    clearSelection();
    refresh();
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
    const QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull())
        return {};
    // QML Image cannot load QIcon directly; use theme name via image:// or file.
    // Prefer freedesktop theme path through Qt's icon engine via temporary — use "image://?" 
    // Simplest: return theme name as `image://icon/` custom provider in main.
    return QStringLiteral("image://theme/%1").arg(iconName);
}
