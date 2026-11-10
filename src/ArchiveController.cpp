#include "ArchiveController.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace {

constexpr qint64 kMaxListingBytes = 8 * 1024 * 1024; // beyond this we stop reading the listing
constexpr int kMaxStderrBytes = 64 * 1024;

const QStringList &extractableSuffixes() {
    static const QStringList s = {QStringLiteral(".zip"),     QStringLiteral(".cbz"),     QStringLiteral(".tar"),
                                  QStringLiteral(".tar.gz"),  QStringLiteral(".tgz"),     QStringLiteral(".tar.xz"),
                                  QStringLiteral(".txz"),     QStringLiteral(".tar.bz2"), QStringLiteral(".tbz2"),
                                  QStringLiteral(".tbz"),     QStringLiteral(".tar.zst"), QStringLiteral(".tzst"),
                                  QStringLiteral(".7z"),      QStringLiteral(".rar"),     QStringLiteral(".cbr")};
    return s;
}

} // namespace

ArchiveController::ArchiveController(QObject *parent)
    : QObject(parent), m_bsdtar(QStandardPaths::findExecutable(QStringLiteral("bsdtar"))) {
    m_ioTimer.setInterval(150);
    connect(&m_ioTimer, &QTimer::timeout, this, &ArchiveController::pollIo);
}

void ArchiveController::setProgress(qreal p) {
    // Report in 0.5% steps (and always the -1 / 1 ends): plenty for a bar, cheap for QML.
    const qreal q = p < 0 ? -1 : qRound(qBound(0.0, p, 1.0) * 200) / 200.0;
    if (q == m_progress)
        return;
    m_progress = q;
    emit progressChanged();
}

/// Bytes the bsdtar process has read so far (Linux: /proc/<pid>/io "rchar").
static qint64 readBytes(qint64 pid) {
    QFile f(QStringLiteral("/proc/%1/io").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray &line : f.readAll().split('\n'))
        if (line.startsWith("rchar:"))
            return line.mid(6).trimmed().toLongLong();
    return -1;
}

void ArchiveController::pollIo() {
    if (!m_proc || m_totalBytes <= 0)
        return;
    const qint64 rd = readBytes(m_proc->processId());
    if (rd >= 0)
        setProgress(qMin(0.99, double(rd) / double(m_totalBytes))); // 100% only when it has finished
}

void ArchiveController::startPolling(qint64 total) {
    m_totalBytes = total;
    setProgress(total > 0 ? 0 : -1);
    if (total > 0)
        m_ioTimer.start();
}

void ArchiveController::stopPolling() {
    m_ioTimer.stop();
    ++m_ioGen;
    m_totalBytes = 0;
    setProgress(-1);
}

bool ArchiveController::canExtract(const QString &path) const {
    if (!available())
        return false;
    const QFileInfo fi(path);
    if (!fi.isFile())
        return false;
    const QString n = fi.fileName().toLower();
    for (const QString &s : extractableSuffixes())
        if (n.endsWith(s) && n.size() > s.size())
            return true;
    return false;
}

QString ArchiveController::archiveBaseName(const QString &fileName) {
    const QString lower = fileName.toLower();
    // Longest suffix first so "a.tar.gz" becomes "a", not "a.tar".
    QStringList sfx = extractableSuffixes();
    std::sort(sfx.begin(), sfx.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &s : std::as_const(sfx))
        if (lower.endsWith(s) && fileName.size() > s.size())
            return fileName.left(fileName.size() - s.size());
    const QString base = QFileInfo(fileName).completeBaseName();
    return base.isEmpty() ? fileName : base;
}

QString ArchiveController::uniqueName(const QString &dir, const QString &base, const QString &ext) {
    auto name = [&](int n) {
        const QString b = n == 0 ? base : QStringLiteral("%1 (%2)").arg(base).arg(n + 1);
        return ext.isEmpty() ? b : b + QLatin1Char('.') + ext;
    };
    int n = 0;
    while (QFile::exists(QDir(dir).filePath(name(n))))
        ++n;
    return name(n);
}

void ArchiveController::begin(Stage stage, const QString &text) {
    m_stage = stage;
    m_taskText = text;
    m_detail.clear();
    m_stderr.clear();
    m_cancelled = false;
    emit busyChanged();
    emit detailChanged();
}

void ArchiveController::fail(const QString &message) {
    const bool cleanup = m_createdDir && !m_resultPath.isEmpty();
    if (m_stage == Stage::Compressing && !m_partPath.isEmpty())
        QFile::remove(m_partPath);
    if (cleanup) { // extraction created something that is now incomplete
        const QFileInfo fi(m_resultPath);
        if (fi.isDir() && !fi.isSymLink())
            QDir(m_resultPath).removeRecursively();
        else
            QFile::remove(m_resultPath);
    }
    stopPolling();
    m_stage = Stage::Idle;
    m_createdDir = false;
    m_partPath.clear();
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    emit busyChanged();
    emit finished(false, message, {});
}

void ArchiveController::done(const QString &message, const QString &resultPath) {
    stopPolling();
    m_stage = Stage::Idle;
    m_createdDir = false;
    m_partPath.clear();
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    emit busyChanged();
    emit finished(true, message, resultPath);
}

QString ArchiveController::lastErrorLine() const {
    // bsdtar mixes progress ("a name"/"x name"), warnings and errors on stderr; a failing entry is
    // reported as "x <name>: <reason>". Pick the most informative line.
    const QStringList lines = m_stderr.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &raw : lines)
        if (raw.contains(QLatin1String("Path contains '..'")))
            return tr("el archivo contiene rutas inseguras («..») y no se ha extraído");
    for (const QString &raw : lines) {
        QString l = raw.trimmed();
        const bool echo = l.startsWith(QLatin1String("a ")) || l.startsWith(QLatin1String("x "));
        if (echo) {
            const int at = l.indexOf(QLatin1String(": "));
            if (at < 0)
                continue; // plain progress line
            l = l.mid(at + 2);
        }
        if (l.isEmpty() || l.contains(QLatin1String("Removing leading"))
            || l.contains(QLatin1String("Error exit delayed")))
            continue; // warnings / bsdtar's generic closing line
        return l.startsWith(QLatin1String("bsdtar: ")) ? l.mid(8) : l;
    }
    return {};
}

void ArchiveController::compress(const QStringList &paths) {
    if (!available()) {
        emit finished(false, tr("Falta bsdtar (paquete libarchive) para comprimir"), {});
        return;
    }
    if (busy()) {
        emit finished(false, tr("Ya hay una operación en curso"), {});
        return;
    }
    if (paths.isEmpty())
        return;

    const QFileInfo first(paths.first());
    const QString parent = first.absolutePath();
    QStringList names;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (!fi.exists() && !fi.isSymLink()) {
            emit finished(false, tr("No existe: %1").arg(fi.fileName()), {});
            return;
        }
        if (fi.absolutePath() != parent) {
            emit finished(false, tr("Selecciona elementos de una sola carpeta"), {});
            return;
        }
        names << fi.fileName();
    }

    // Like macOS: one item → "<name>.zip", several → "Archivo.zip".
    QString base;
    if (names.size() == 1) {
        base = first.isDir() || first.suffix().isEmpty() ? first.fileName() : first.completeBaseName();
        if (base.isEmpty())
            base = first.fileName();
    } else {
        base = tr("Archivo");
    }

    // A taken name gets the date and time (so repeated runs are distinguishable and never
    // overwrite each other); the " (2)" suffix is only the last resort within the same second.
    QString zipName = base + QStringLiteral(".zip");
    if (QFile::exists(QDir(parent).filePath(zipName))) {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"));
        zipName = uniqueName(parent, base + QLatin1Char(' ') + stamp, QStringLiteral("zip"));
    }
    m_archive = QDir(parent).filePath(zipName);
    m_partPath = m_archive + QStringLiteral(".part");
    QFile::remove(m_partPath); // stale leftover of an interrupted run
    m_resultPath = m_archive;
    m_createdDir = false;

    m_kind = QStringLiteral("compress");
    begin(Stage::Compressing, names.size() == 1 ? tr("Comprimiendo «%1»…").arg(names.first())
                                                : tr("Comprimiendo %1 elementos…").arg(names.size()));
    m_proc = new QProcess(this);
    m_proc->setStandardInputFile(QProcess::nullDevice());
    connect(m_proc, &QProcess::readyReadStandardError, this, &ArchiveController::readProgress);
    connect(m_proc, &QProcess::finished, this, &ArchiveController::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(tr("No se pudo iniciar bsdtar"));
    });
    QStringList args = {QStringLiteral("--format"), QStringLiteral("zip"), QStringLiteral("-cvf"), m_partPath,
                        QStringLiteral("-C"), parent, QStringLiteral("--")};
    args += names;
    m_proc->start(m_bsdtar, args);

    // Total size of what is being packed (off-thread: a big tree can take a moment to walk).
    // Until it is known the bar stays indeterminate.
    m_kind = QStringLiteral("compress");
    setProgress(-1);
    const int gen = ++m_ioGen;
    const QStringList inputs = [&] { QStringList l; for (const QString &n : names) l << QDir(parent).filePath(n); return l; }();
    auto *watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcher<qint64>::finished, this, [this, watcher, gen] {
        const qint64 total = watcher->result();
        watcher->deleteLater();
        if (gen == m_ioGen && m_stage == Stage::Compressing)
            startPolling(total);
    });
    watcher->setFuture(QtConcurrent::run([inputs]() -> qint64 {
        qint64 total = 0;
        for (const QString &p : inputs) {
            const QFileInfo fi(p);
            if (fi.isSymLink())
                continue; // stored as a link, nothing is read
            if (fi.isFile()) {
                total += fi.size();
            } else if (fi.isDir()) {
                QDirIterator it(p, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    total += it.fileInfo().size();
                }
            }
        }
        return total;
    }));
}

void ArchiveController::extractHere(const QString &archivePath) {
    if (busy()) {
        emit finished(false, tr("Ya hay una operación en curso"), {});
        return;
    }
    if (!canExtract(archivePath)) {
        emit finished(false, tr("Este tipo de archivo no se puede extraer"), {});
        return;
    }
    const QFileInfo fi(archivePath);
    m_archive = fi.absoluteFilePath();
    m_resultPath.clear();
    m_createdDir = false;
    m_listing.clear();

    m_kind = QStringLiteral("extract");
    begin(Stage::Listing, tr("Leyendo «%1»…").arg(fi.fileName()));
    m_proc = new QProcess(this);
    m_proc->setStandardInputFile(QProcess::nullDevice());
    connect(m_proc, &QProcess::readyReadStandardError, this, &ArchiveController::readProgress);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_listing.size() < kMaxListingBytes)
            m_listing += m_proc->readAllStandardOutput();
        else
            m_proc->readAllStandardOutput(); // keep draining; the listing is already "too big to inspect"
    });
    connect(m_proc, &QProcess::finished, this, &ArchiveController::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(tr("No se pudo iniciar bsdtar"));
    });
    m_proc->start(m_bsdtar, {QStringLiteral("-tf"), m_archive});
}

void ArchiveController::startExtraction(const QString &destDir, const QString &resultPath, bool createdDir,
                                        bool stripTop) {
    m_destDir = destDir;
    m_resultPath = resultPath;
    m_createdDir = createdDir;
    m_stripTop = stripTop;
    m_stage = Stage::Extracting;
    m_taskText = tr("Extrayendo «%1»…").arg(QFileInfo(m_archive).fileName());
    m_detail.clear();
    m_stderr.clear();
    emit busyChanged();
    emit detailChanged();

    m_proc->deleteLater();
    m_proc = new QProcess(this);
    m_proc->setStandardInputFile(QProcess::nullDevice());
    connect(m_proc, &QProcess::readyReadStandardError, this, &ArchiveController::readProgress);
    connect(m_proc, &QProcess::finished, this, &ArchiveController::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(tr("No se pudo iniciar bsdtar"));
    });
    startPolling(QFileInfo(m_archive).size()); // bytes of the archive read so far / archive size
    // bsdtar already drops absolute paths and "..": nothing can land outside destDir.
    QStringList args = {QStringLiteral("-xvf"), m_archive, QStringLiteral("-C"), destDir};
    if (stripTop)
        args << QStringLiteral("--strip-components=1");
    m_proc->start(m_bsdtar, args);
}

void ArchiveController::readProgress() {
    if (!m_proc)
        return;
    const QString chunk = QString::fromUtf8(m_proc->readAllStandardError());
    if (m_stderr.size() < kMaxStderrBytes)
        m_stderr += chunk;
    // Last "a name" / "x name" line = what it is doing now.
    const QStringList lines = chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString l = lines.at(i);
        if (l.startsWith(QLatin1String("a ")) || l.startsWith(QLatin1String("x "))) {
            QString name = l.mid(2).trimmed();
            while (name.endsWith(QLatin1Char('/')))
                name.chop(1);
            const QString shown = name.section(QLatin1Char('/'), -1);
            if (shown != m_detail) {
                m_detail = shown;
                emit detailChanged();
            }
            break;
        }
    }
}

void ArchiveController::cancel() {
    if (!busy() || !m_proc)
        return;
    m_cancelled = true;
    m_proc->kill();
}

void ArchiveController::onFinished(int code, QProcess::ExitStatus status) {
    if (!m_proc)
        return;
    readProgress();
    const bool ok = status == QProcess::NormalExit && code == 0;

    if (m_cancelled) {
        fail(tr("Cancelado"));
        return;
    }

    switch (m_stage) {
    case Stage::Compressing: {
        if (!ok) {
            const QString why = lastErrorLine();
            fail(why.isEmpty() ? tr("No se pudo comprimir") : tr("No se pudo comprimir: %1").arg(why));
            return;
        }
        if (!QFile::rename(m_partPath, m_archive)) {
            fail(tr("No se pudo guardar el archivo comprimido"));
            return;
        }
        done(tr("Creado «%1»").arg(QFileInfo(m_archive).fileName()), m_archive);
        return;
    }
    case Stage::Listing: {
        if (!ok) {
            const QString why = lastErrorLine();
            fail(why.isEmpty() ? tr("No se pudo leer el archivo") : tr("No se pudo leer el archivo: %1").arg(why));
            return;
        }
        // One top-level entry → extract beside the archive; anything else → into its own folder.
        QSet<QString> tops;
        bool unsafe = m_listing.size() >= kMaxListingBytes;
        bool topIsDir = false; // some entry lives under "<top>/"
        int entries = 0;
        for (const QString &raw : QString::fromUtf8(m_listing).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            QString e = raw.trimmed();
            while (e.startsWith(QLatin1String("./")))
                e.remove(0, 2);
            if (e.isEmpty() || e == QLatin1String("."))
                continue;
            ++entries;
            if (e.startsWith(QLatin1Char('/')) || e == QLatin1String("..") || e.startsWith(QLatin1String("../"))
                || e.contains(QLatin1String("/../")))
                unsafe = true;
            tops.insert(e.section(QLatin1Char('/'), 0, 0));
            if (e.contains(QLatin1Char('/')))
                topIsDir = true;
        }
        if (entries == 0) {
            fail(tr("El archivo está vacío"));
            return;
        }
        const QString parent = QFileInfo(m_archive).absolutePath();
        const bool lone = !unsafe && tops.size() == 1;
        if (lone) {
            const QString top = *tops.constBegin();
            const QString target = QDir(parent).filePath(top);
            if (!QFile::exists(target) && !QFileInfo(target).isSymLink()) {
                startExtraction(parent, target, true); // lands as "<top>" next to the archive
                return;
            }
            if (topIsDir) {
                // "<top>" is taken: give the folder a free name and drop the redundant wrapper level
                // (no "carpeta (2)/carpeta/…").
                const QString dest = QDir(parent).filePath(uniqueName(parent, top));
                if (!QDir().mkpath(dest)) {
                    fail(tr("No se pudo crear la carpeta «%1»").arg(QFileInfo(dest).fileName()));
                    return;
                }
                startExtraction(dest, dest, true, true);
                return;
            }
        }
        const QString folder = uniqueName(parent, archiveBaseName(QFileInfo(m_archive).fileName()));
        const QString dest = QDir(parent).filePath(folder);
        if (!QDir().mkpath(dest)) {
            fail(tr("No se pudo crear la carpeta «%1»").arg(folder));
            return;
        }
        startExtraction(dest, dest, true);
        return;
    }
    case Stage::Extracting: {
        if (!ok) {
            const QString why = lastErrorLine();
            fail(why.isEmpty() ? tr("No se pudo extraer") : tr("No se pudo extraer: %1").arg(why));
            return;
        }
        done(tr("Extraído «%1»").arg(QFileInfo(m_resultPath).fileName()), m_resultPath);
        return;
    }
    case Stage::Idle:
        return;
    }
}
