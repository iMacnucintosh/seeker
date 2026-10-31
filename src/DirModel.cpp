#include "DirModel.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>

DirModel::DirModel(QObject *parent)
    : QAbstractListModel(parent) {}

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
    beginResetModel();
    m_entries.clear();

    QDir dir(m_path);
    if (!dir.exists()) {
        endResetModel();
        return;
    }

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (m_showHidden)
        filters |= QDir::Hidden;

    const auto infos = dir.entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QMimeDatabase mimeDb;
    m_entries.reserve(infos.size());
    for (const QFileInfo &fi : infos) {
        DirEntry e;
        e.name = fi.fileName();
        e.path = fi.absoluteFilePath();
        e.isDir = fi.isDir();
        e.isHidden = fi.isHidden();
        e.size = fi.isDir() ? 0 : fi.size();
        e.modified = fi.lastModified();
        if (e.isDir)
            e.mimeIcon = QStringLiteral("folder");
        else {
            const auto mime = mimeDb.mimeTypeForFile(fi);
            e.isImage = mime.name().startsWith(QLatin1String("image/"));
            const QString icon = mime.iconName();
            e.mimeIcon = icon.isEmpty() ? QStringLiteral("text-x-generic") : icon;
        }
        m_entries.push_back(std::move(e));
    }
    endResetModel();
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
    };
}
