#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct TrashEntry {
    QString originalPath;
    QString trashFilePath; // ~/.local/share/Trash/files/...
};

class Trash : public QObject {
    Q_OBJECT
public:
    explicit Trash(QObject *parent = nullptr);

    Q_INVOKABLE QString trashPath() const;
    Q_INVOKABLE bool moveToTrash(const QStringList &paths);
    /// Same as moveToTrash but returns per-item locations for undo/restore.
    QVector<TrashEntry> moveToTrashEntries(const QStringList &paths);
    bool restore(const QString &trashFilePath, const QString &originalPath) const;
    Q_INVOKABLE bool emptyTrash();

private:
    QString homeTrashRoot() const;
    QString findTrashFileForOriginal(const QString &originalAbs) const;
};
