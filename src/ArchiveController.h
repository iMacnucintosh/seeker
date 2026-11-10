#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

/// Compress to ZIP / extract archives, in the background, with libarchive's `bsdtar`.
///
/// - Compress: the selection (siblings in one folder) goes into `<name>.zip` next to it. It is
///   written as `<name>.zip.part` and renamed only on success, so a cancelled or failed run never
///   leaves a half-written archive behind.
/// - Extract: if the archive has a single top-level entry it is extracted next to the archive;
///   otherwise into a new folder named after the archive. Existing files are never overwritten
///   (names get a " (2)" suffix). bsdtar itself refuses absolute paths and `..` (no zip-slip).
///
/// Paths always reach bsdtar as separate arguments (never through a shell).
class ArchiveController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString taskText READ taskText NOTIFY busyChanged)
    /// Entry bsdtar is working on right now (for the progress toast).
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    /// 0..1 while the size of the job is known (bytes bsdtar has read / total); -1 = unknown yet.
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    /// "compress" | "extract" — of the current or the last job (picks the sound).
    Q_PROPERTY(QString kind READ kind NOTIFY busyChanged)

public:
    explicit ArchiveController(QObject *parent = nullptr);

    bool busy() const { return m_stage != Stage::Idle; }
    QString taskText() const { return m_taskText; }
    QString detail() const { return m_detail; }
    bool available() const { return !m_bsdtar.isEmpty(); }
    qreal progress() const { return m_progress; }
    QString kind() const { return m_kind; }

    /// By file name only: zip, tar(.gz/.xz/.bz2/.zst), 7z, rar, cbz…
    Q_INVOKABLE bool canExtract(const QString &path) const;
    Q_INVOKABLE void compress(const QStringList &paths);
    Q_INVOKABLE void extractHere(const QString &archivePath);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void detailChanged();
    void progressChanged();
    /// ok=false: `message` says what went wrong. ok=true: `resultPath` is the new archive/file/folder.
    void finished(bool ok, const QString &message, const QString &resultPath);

private:
    enum class Stage { Idle, Compressing, Listing, Extracting };

    static QString archiveBaseName(const QString &fileName);
    static QString uniqueName(const QString &dir, const QString &base, const QString &ext = {});
    void begin(Stage stage, const QString &text);
    void fail(const QString &message);
    void done(const QString &message, const QString &resultPath);
    void startExtraction(const QString &destDir, const QString &resultPath, bool createdDir, bool stripTop = false);
    void readProgress();
    void pollIo();
    void setProgress(qreal p);
    void startPolling(qint64 total);
    void stopPolling();
    void onFinished(int code, QProcess::ExitStatus status);
    QString lastErrorLine() const;

    QString m_bsdtar;
    Stage m_stage = Stage::Idle;
    QString m_taskText;
    QString m_detail;
    QProcess *m_proc = nullptr;
    bool m_cancelled = false;

    // current job
    QString m_archive;      // compress: final .zip; extract: the archive being read
    QString m_partPath;     // compress: temporary name
    QString m_destDir;      // extract: where bsdtar runs
    QString m_resultPath;   // what the user gets selected afterwards
    bool m_createdDir = false;
    QString m_kind;
    qreal m_progress = -1;
    qint64 m_totalBytes = 0;   // compress: sum of the inputs; extract: size of the archive
    int m_ioGen = 0;           // invalidates an input-size scan that finishes after the job ended
    QTimer m_ioTimer;
    bool m_stripTop = false; // extract a lone top-level folder's contents straight into the new folder
    QString m_stderr;
    QByteArray m_listing;
};
