#include "DirModel.h"

#include <QDir>
#include <QFileInfo>
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

void DirModel::reload() {
    beginResetModel();
    m_entries.clear();

    QDir dir(m_path);
    if (!dir.exists()) {
        endResetModel();
        return;
    }

    const auto infos = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                         QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QMimeDatabase mimeDb;
    m_entries.reserve(infos.size());
    for (const QFileInfo &fi : infos) {
        DirEntry e;
        e.name = fi.fileName();
        e.path = fi.absoluteFilePath();
        e.isDir = fi.isDir();
        e.isHidden = fi.isHidden();
        e.size = fi.isDir() ? 0 : fi.size();
        if (e.isDir)
            e.mimeIcon = QStringLiteral("folder");
        else {
            const QString icon = mimeDb.mimeTypeForFile(fi).iconName();
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
    case IconNameRole:
        return e.mimeIcon;
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
        {IconNameRole, "iconName"},
    };
}
