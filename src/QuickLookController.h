#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>

/// Quick Look: a clean preview of the selected file on Space, like Finder's. Images, video and audio are shown
/// as they are; PDF one page at a time (rendered by poppler's pdftoppm, cached), text as its first lines.
/// Anything else is not previewed here (the window opens its properties instead).
class QuickLookController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY openChanged)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    /// "image", "video", "audio", "pdf", "text" or "other" (only while moving with the arrows)
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY pageChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageChanged)
    /// File of the rendered page (empty while it is being rendered)
    Q_PROPERTY(QString pageImage READ pageImage NOTIFY pageChanged)

public:
    explicit QuickLookController(QObject *parent = nullptr);
    ~QuickLookController() override;

    bool open() const { return m_open; }
    QString path() const { return m_path; }
    QString name() const { return m_name; }
    QString kind() const { return m_kind; }
    QString iconName() const { return m_iconName; }
    QString text() const { return m_text; }
    int page() const { return m_page; }
    int pageCount() const { return m_pageCount; }
    QString pageImage() const { return m_pageImage; }

    /// What a file would show: one of the kinds above, "other" when there is no preview for it
    Q_INVOKABLE QString kindOf(const QString &path) const;
    /// Opens (or moves) the preview to path. A file without a preview shows as "other" (icon and name).
    Q_INVOKABLE void show(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE void setPage(int page);

signals:
    void openChanged();
    void changed();
    void pageChanged();

private:
    void countPages();
    void renderPage();
    QString pageCachePath(int page) const;

    bool m_open = false;
    QString m_path;
    QString m_name;
    QString m_kind;
    QString m_iconName;
    QString m_text;
    int m_page = 0;
    int m_pageCount = 0;
    QString m_pageImage;
    // Stale answers (the user already moved on) are dropped
    quint64 m_generation = 0;
    QPointer<QProcess> m_infoProc;
    QPointer<QProcess> m_renderProc;
};
