#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

/// Material "Abrir con…" sheet — apps for a file's MIME type.
class OpenWithController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY openChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY infoChanged)
    Q_PROPERTY(QString mimeLabel READ mimeLabel NOTIFY infoChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(bool canLaunch READ canLaunch NOTIFY selectedIndexChanged)
    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY rememberChanged)

public:
    explicit OpenWithController(QObject *parent = nullptr);

    bool open() const { return m_open; }
    QString fileName() const { return m_fileName; }
    QString mimeLabel() const { return m_mimeLabel; }
    QVariantList apps() const { return m_apps; }
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);
    bool canLaunch() const;
    bool remember() const { return m_remember; }
    void setRemember(bool remember);

    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void launchSelected();

    /// Desktop id (foo.desktop) for the MIME default, or empty.
    static QString defaultDesktopForMime(const QString &mime);
    /// Launch desktop file against a local path. Returns false on failure.
    static bool launchDesktop(const QString &desktopFileOrId, const QString &localPath);
    /// Set MIME default via xdg-mime.
    static bool setDefaultDesktop(const QString &desktopId, const QString &mime);

signals:
    void openChanged();
    void infoChanged();
    void appsChanged();
    void selectedIndexChanged();
    void rememberChanged();
    void errorOccurred(const QString &message);

private:
    struct AppEntry {
        QString id;
        QString name;
        QString icon;
        QString desktopFile;
    };

    void loadApps(const QString &mimeName, const QStringList &mimeParents);
    static QStringList applicationDirs();
    static bool mimeMatches(const QStringList &desktopMimes, const QString &mime,
                            const QStringList &parents);

    bool m_open = false;
    bool m_remember = false;
    int m_selectedIndex = -1;
    QString m_path;
    QString m_fileName;
    QString m_mimeName;
    QString m_mimeLabel;
    QVariantList m_apps;
};
