#include "DirModel.h"
#include "Thumbnailers.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>

DirModel::DirModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(300);
    connect(&m_syncTimer, &QTimer::timeout, this, &DirModel::sync);
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(1000);
    connect(&m_settleTimer, &QTimer::timeout, this, &DirModel::sync);
    // Not restarted on every event: a folder that keeps changing still refreshes every 300 ms, not never
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        if (!m_syncTimer.isActive())
            m_syncTimer.start();
    });
}

void DirModel::settleLater() {
    const QDateTime recent = QDateTime::currentDateTime().addSecs(-3);
    for (const DirEntry &e : std::as_const(m_entries)) {
        if (!e.isDir && e.modified > recent) {
            m_settleTimer.start();
            return;
        }
    }
}

void DirModel::watch() {
    const QStringList watched = m_watcher.directories();
    if (watched.size() == 1 && watched.first() == m_path)
        return;
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    if (!m_path.isEmpty() && QFileInfo(m_path).isDir())
        m_watcher.addPath(m_path);
}

void DirModel::setPath(const QString &path) {
    QString normalized = QDir::cleanPath(path);
    if (normalized.isEmpty())
        normalized = QDir::homePath();
    if (normalized == m_path && !m_entries.isEmpty())
        return;
    m_path = normalized;
    reload();
    emit pathChanged();
}

void DirModel::refresh() {
    reload();
    emit pathChanged();
}

void DirModel::setShowHidden(bool show) {
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    emit showHiddenChanged();
    reload();
    emit pathChanged();
}

void DirModel::reload() {
    m_syncTimer.stop();
    m_settleTimer.stop();
    beginResetModel();
    m_entries = scan({});
    endResetModel();
    watch();
    emit countChanged();
}

void DirModel::sync() {
    // The folder itself went away (deleted, unmounted): show it empty, as a reload would
    if (!QFileInfo(m_path).isDir()) {
        reload();
        return;
    }
    const QVector<DirEntry> fresh = scan(m_entries);
    QHash<QString, int> inFresh;
    inFresh.reserve(fresh.size());
    for (int i = 0; i < fresh.size(); ++i)
        inFresh.insert(fresh.at(i).path, i);

    // Rows that are gone, from the end so the indexes hold
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (inFresh.contains(m_entries.at(i).path))
            continue;
        beginRemoveRows({}, i, i);
        m_entries.removeAt(i);
        endRemoveRows();
    }
    // Both lists share the order, so what is left is a subsequence of the fresh one: insert the new rows
    // where they belong, and refresh the ones whose size or date changed
    for (int i = 0; i < fresh.size(); ++i) {
        const DirEntry &e = fresh.at(i);
        if (i >= m_entries.size() || m_entries.at(i).path != e.path) {
            beginInsertRows({}, i, i);
            m_entries.insert(i, e);
            endInsertRows();
            continue;
        }
        DirEntry &cur = m_entries[i];
        if (cur.size != e.size || cur.modified != e.modified || cur.mimeIcon != e.mimeIcon || cur.isHidden != e.isHidden) {
            cur = e;
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx);
        }
    }
    if (m_entries.size() != fresh.size()) {
        // Should not happen (the order is the same); fall back to a full read rather than show a wrong list
        reload();
        return;
    }
    emit countChanged();
    settleLater();
}

QVector<DirEntry> DirModel::scan(const QVector<DirEntry> &known) const {
    QVector<DirEntry> out;
    QDir dir(m_path);
    if (!dir.exists())
        return out;

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (m_showHidden)
        filters |= QDir::Hidden;

    // What a refresh already knows: a file with the same name, size and date keeps its type, without asking
    // the MIME database again
    QHash<QString, const DirEntry *> byPath;
    byPath.reserve(known.size());
    for (const DirEntry &k : known)
        byPath.insert(k.path, &k);

    const auto infos = dir.entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QMimeDatabase mimeDb;
    out.reserve(infos.size());
    for (const QFileInfo &fi : infos) {
        DirEntry e;
        e.name = fi.fileName();
        e.path = fi.absoluteFilePath();
        e.isDir = fi.isDir();
        e.isHidden = fi.isHidden();
        e.size = fi.isDir() ? 0 : fi.size();
        e.modified = fi.lastModified();
        const DirEntry *old = byPath.value(e.path, nullptr);
        if (old && old->isDir == e.isDir && old->size == e.size && old->modified == e.modified) {
            e.isImage = old->isImage;
            e.canPreview = old->canPreview;
            e.mimeIcon = old->mimeIcon;
        } else if (e.isDir) {
            e.mimeIcon = QStringLiteral("folder");
        } else {
            // Extension first; read the file's content only when the name says nothing. Sniffing
            // every file (twice, with the thumbnailer check) made big folders slow to open.
            const auto mimeExt = mimeDb.mimeTypeForFile(fi, QMimeDatabase::MatchExtension);
            const bool extKnown = mimeExt.isValid() && !mimeExt.isDefault();
            const auto mime = extKnown ? mimeExt : mimeDb.mimeTypeForFile(fi, QMimeDatabase::MatchContent);
            e.isImage = mime.name().startsWith(QLatin1String("image/"));
            e.canPreview = e.isImage || Thumbnailers::instance().supports(mime);
            // Prefer extension icon for types content-sniff gets wrong (.blend → zstd).
            QString icon = mimeExt.iconName();
            if (icon.isEmpty() || !extKnown)
                icon = mime.iconName();
            e.mimeIcon = icon.isEmpty() ? QStringLiteral("text-x-generic") : icon;
        }
        out.push_back(std::move(e));
    }
    return out;
}

int DirModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant DirModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const DirEntry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case PathRole:
        return e.path;
    case IsDirRole:
        return e.isDir;
    case IsHiddenRole:
        return e.isHidden;
    case SizeRole:
        return e.size;
    case ModifiedRole:
        return e.modified;
    case ModifiedTextRole:
        return e.modified.isValid()
            ? QLocale::system().toString(e.modified, QLocale::ShortFormat)
            : QString();
    case IconNameRole:
        return e.mimeIcon;
    case IsImageRole:
        return e.isImage;
    case CanPreviewRole:
        return e.canPreview;
    case VersionRole:
        // Changes when the file does: thumbnails asked for while it was still being written get asked again
        return QStringLiteral("%1-%2").arg(e.modified.toMSecsSinceEpoch()).arg(e.size);
    default:
        return {};
    }
}

QHash<int, QByteArray> DirModel::roleNames() const {
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IsDirRole, "isDir"},
        {IsHiddenRole, "isHidden"},
        {SizeRole, "size"},
        {ModifiedRole, "modified"},
        {ModifiedTextRole, "modifiedText"},
        {IconNameRole, "iconName"},
        {IsImageRole, "isImage"},
        {CanPreviewRole, "canPreview"},
        {VersionRole, "version"},
    };
}
