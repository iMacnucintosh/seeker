#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileIconProvider>
#include <QVector>

struct DirEntry {
    QString name;
    QString path;
    bool isDir = false;
    bool isHidden = false;
    bool isImage = false;
    /// Image or MIME with a FreeDesktop thumbnailer (Dolphin-style previews).
    bool canPreview = false;
    qint64 size = 0;
    QDateTime modified;
    QString mimeIcon;
};

class DirModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY pathChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsDirRole,
        IsHiddenRole,
        SizeRole,
        ModifiedRole,
        ModifiedTextRole,
        IconNameRole,
        IsImageRole,
        CanPreviewRole,
    };

    explicit DirModel(QObject *parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

signals:
    void pathChanged();
    void showHiddenChanged();

private:
    void reload();

    QString m_path;
    bool m_showHidden = false;
    QVector<DirEntry> m_entries;
    QFileIconProvider m_icons;
};
