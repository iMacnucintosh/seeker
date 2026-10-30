#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class Trash : public QObject {
    Q_OBJECT
public:
    explicit Trash(QObject *parent = nullptr);

    Q_INVOKABLE QString trashPath() const;
    Q_INVOKABLE bool moveToTrash(const QStringList &paths);
    Q_INVOKABLE bool emptyTrash();

private:
    QString homeTrashRoot() const;
};
