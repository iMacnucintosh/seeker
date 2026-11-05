#pragma once

#include <QAtomicInt>
#include <QFileDevice>
#include <QObject>
#include <QString>
#include <QStringList>

/// Properties sheet for one or more selected paths (exec bit is the only editable field).
class PropertiesController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY openChanged)
    Q_PROPERTY(bool isMulti READ isMulti NOTIFY infoChanged)
    Q_PROPERTY(QString title READ title NOTIFY infoChanged)
    Q_PROPERTY(QString kindLabel READ kindLabel NOTIFY infoChanged)
    Q_PROPERTY(QString location READ location NOTIFY infoChanged)
    Q_PROPERTY(QString modified READ modified NOTIFY infoChanged)
    Q_PROPERTY(QString permissions READ permissions NOTIFY infoChanged)
    Q_PROPERTY(bool showModified READ showModified NOTIFY infoChanged)
    Q_PROPERTY(bool showPermissions READ showPermissions NOTIFY infoChanged)
    Q_PROPERTY(bool showContents READ showContents NOTIFY infoChanged)
    Q_PROPERTY(QString sizeLabel READ sizeLabel NOTIFY sizeChanged)
    Q_PROPERTY(QString contentsLabel READ contentsLabel NOTIFY sizeChanged)
    Q_PROPERTY(bool sizeReady READ sizeReady NOTIFY sizeChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY infoChanged)
    Q_PROPERTY(bool isImage READ isImage NOTIFY infoChanged)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY infoChanged)
    Q_PROPERTY(bool canToggleExec READ canToggleExec NOTIFY infoChanged)
    Q_PROPERTY(bool executable READ executable NOTIFY infoChanged)

public:
    explicit PropertiesController(QObject *parent = nullptr);

    bool open() const { return m_open; }
    bool isMulti() const { return m_isMulti; }
    QString title() const { return m_title; }
    QString kindLabel() const { return m_kindLabel; }
    QString location() const { return m_location; }
    QString modified() const { return m_modified; }
    QString permissions() const { return m_permissions; }
    bool showModified() const { return m_showModified; }
    bool showPermissions() const { return m_showPermissions; }
    bool showContents() const { return m_showContents; }
    QString sizeLabel() const { return m_sizeLabel; }
    QString contentsLabel() const { return m_contentsLabel; }
    bool sizeReady() const { return m_sizeReady; }
    QString iconName() const { return m_iconName; }
    bool isImage() const { return m_isImage; }
    QString previewPath() const { return m_previewPath; }
    bool canToggleExec() const { return m_canToggleExec; }
    bool executable() const { return m_executable; }

    Q_INVOKABLE void openPaths(const QStringList &paths);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool setExecutable(bool on);

signals:
    void openChanged();
    void infoChanged();
    void sizeChanged();

private:
    struct WalkResult {
        qint64 bytes = 0;
        int files = 0;
        int dirs = 0;
    };

    void resetUi();
    void applySnapshot(const QStringList &paths);
    void startSizeWalk(const QStringList &paths);
    void applyWalkProgress(int gen, qint64 bytes, int files, int dirs, bool done);
    static QString formatSize(qint64 bytes);
    static QString formatContents(int dirs, int files);
    static QString formatPermissions(QFileDevice::Permissions perms);
    static QString displayLocation(const QString &path);
    static QString ensureLocal(const QString &pathOrUri);
    static WalkResult walkPaths(const QStringList &paths, const QAtomicInt *gen, int myGen,
                                PropertiesController *self);

    bool m_open = false;
    bool m_isMulti = false;
    bool m_showModified = false;
    bool m_showPermissions = false;
    bool m_showContents = false;
    bool m_sizeReady = false;
    bool m_isImage = false;
    bool m_canToggleExec = false;
    bool m_executable = false;
    QString m_title;
    QString m_kindLabel;
    QString m_location;
    QString m_modified;
    QString m_permissions;
    QString m_sizeLabel;
    QString m_contentsLabel;
    QString m_iconName;
    QString m_previewPath;
    QAtomicInt m_gen{0};
};
