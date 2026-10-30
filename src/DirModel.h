#pragma once

#include <QAbstractListModel>
#include <QFileIconProvider>
#include <QVector>

struct DirEntry {
    QString name;
    QString path;
    bool isDir = false;
    bool isHidden = false;
    qint64 size = 0;
    QString mimeIcon;
};

class DirModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY pathChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsDirRole,
        IsHiddenRole,
        SizeRole,
        IconNameRole,
    };

    explicit DirModel(QObject *parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

signals:
    void pathChanged();

private:
    void reload();

    QString m_path;
    QVector<DirEntry> m_entries;
    QFileIconProvider m_icons;
};
