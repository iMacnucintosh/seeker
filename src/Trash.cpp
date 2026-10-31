#include "Trash.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace {

bool copyRecursively(const QString &src, const QString &dest) {
    const QFileInfo fi(src);
    if (!fi.exists() && !fi.isSymLink())
        return false;

    if (fi.isSymLink()) {
        QFile::remove(dest);
        return QFile::link(fi.symLinkTarget(), dest);
    }

    if (fi.isDir()) {
        if (!QDir().mkpath(dest))
            return false;
        const QDir dir(src);
        const auto entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot
                                               | QDir::Hidden | QDir::System);
        for (const QString &name : entries) {
            if (!copyRecursively(dir.filePath(name), QDir(dest).filePath(name)))
                return false;
        }
        return true;
    }

    QFile::remove(dest);
    return QFile::copy(src, dest);
}

bool movePath(const QString &src, const QString &dest) {
    const QFileInfo fi(src);
    if (fi.isDir() && !fi.isSymLink()) {
        if (QDir().rename(src, dest))
            return true;
        return copyRecursively(src, dest) && QDir(src).removeRecursively();
    }
    if (QFile::rename(src, dest))
        return true;
    return QFile::copy(src, dest) && QFile::remove(src);
}

bool gioTrash(const QString &path) {
    static const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (gio.isEmpty())
        return false;
    QProcess p;
    p.start(gio, {QStringLiteral("trash"), QStringLiteral("--"), path});
    return p.waitForFinished(15000) && p.exitCode() == 0;
}

QString decodeTrashPath(const QString &value) {
    QString v = value.trimmed();
    if (v.startsWith(QLatin1String("file:")))
        return QUrl(v).toLocalFile();
    // gio writes Path=/home/…/Carpeta%20nueva (percent-encoded, no scheme)
    return QString::fromUtf8(QByteArray::fromPercentEncoding(v.toUtf8()));
}

} // namespace

Trash::Trash(QObject *parent)
    : QObject(parent) {}

QString Trash::homeTrashRoot() const {
    const QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
    if (!xdg.isEmpty())
        return QDir(xdg).filePath(QStringLiteral("Trash"));
    return QDir::home().filePath(QStringLiteral(".local/share/Trash"));
}

QString Trash::trashPath() const {
    return QDir(homeTrashRoot()).filePath(QStringLiteral("files"));
}

QString Trash::findTrashFileForOriginal(const QString &originalAbs) const {
    const QString infoDirPath = QDir(homeTrashRoot()).filePath(QStringLiteral("info"));
    const QString filesDir = trashPath();
    const QString want = QDir::cleanPath(originalAbs);

    QDir infoDir(infoDirPath);
    // Include hidden (e.g. ".folder.trashinfo")
    const auto infos = infoDir.entryList({QStringLiteral("*.trashinfo"), QStringLiteral(".*.trashinfo")},
                                         QDir::Files | QDir::Hidden, QDir::Time);
    for (const QString &name : infos) {
        QFile f(infoDir.filePath(name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (!line.startsWith(QLatin1String("Path=")))
                continue;
            const QString restored = QDir::cleanPath(decodeTrashPath(line.mid(5)));
            if (restored == want) {
                QString base = name;
                if (base.endsWith(QLatin1String(".trashinfo")))
                    base.chop(10);
                const QString trashFile = QDir(filesDir).filePath(base);
                if (QFileInfo::exists(trashFile))
                    return trashFile;
            }
            break;
        }
    }
    return {};
}

bool Trash::moveToTrash(const QStringList &paths) {
    if (paths.isEmpty())
        return true;
    return !moveToTrashEntries(paths).isEmpty();
}

QVector<TrashEntry> Trash::moveToTrashEntries(const QStringList &paths) {
    const QString root = homeTrashRoot();
    const QString filesDir = QDir(root).filePath(QStringLiteral("files"));
    const QString infoDir = QDir(root).filePath(QStringLiteral("info"));
    QDir().mkpath(filesDir);
    QDir().mkpath(infoDir);

    QVector<TrashEntry> done;
    for (const QString &path : paths) {
        const QFileInfo fi(path);
        if (!fi.exists() && !fi.isSymLink())
            continue;
        const QString abs = QDir::cleanPath(fi.absoluteFilePath());

        // Manual first — we know the trash path exactly (needed for Ctrl+Z).
        QString destName = fi.fileName();
        QString dest = QDir(filesDir).filePath(destName);
        int n = 1;
        while (QFileInfo::exists(dest)) {
            destName = QStringLiteral("%1.%2").arg(fi.fileName()).arg(n++);
            dest = QDir(filesDir).filePath(destName);
        }

        if (movePath(abs, dest)) {
            const QString infoPath = QDir(infoDir).filePath(destName + QStringLiteral(".trashinfo"));
            QFile info(infoPath);
            if (info.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                // Store decoded absolute path (no %20) so restore matching is trivial.
                const QByteArray pathBytes = abs.toUtf8();
                const QString deletion = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"));
                info.write("[Trash Info]\nPath=");
                info.write(pathBytes);
                info.write("\nDeletionDate=");
                info.write(deletion.toUtf8());
                info.write("\n");
            }
            done.push_back({abs, dest});
            continue;
        }

        // Cross-device fallback via gio, then resolve trash location.
        if (gioTrash(abs)) {
            const QString trashFile = findTrashFileForOriginal(abs);
            done.push_back({abs, trashFile});
            continue;
        }
    }
    return done;
}

bool Trash::restore(const QString &trashFilePath, const QString &originalPath) const {
    if (originalPath.isEmpty())
        return false;

    QString trashFile = trashFilePath;
    if (trashFile.isEmpty() || !QFileInfo::exists(trashFile))
        trashFile = findTrashFileForOriginal(originalPath);
    if (trashFile.isEmpty() || !QFileInfo::exists(trashFile))
        return false;

    const QString original = QDir::cleanPath(originalPath);
    const QFileInfo origFi(original);
    QDir().mkpath(origFi.absolutePath());

    QString dest = original;
    if (QFileInfo::exists(dest))
        dest = QDir(origFi.absolutePath()).filePath(QFileInfo(trashFile).fileName() + QStringLiteral(" (restaurado)"));

    if (!movePath(trashFile, dest))
        return false;

    const QString infoPath = QDir(homeTrashRoot()).filePath(
        QStringLiteral("info/") + QFileInfo(trashFile).fileName() + QStringLiteral(".trashinfo"));
    QFile::remove(infoPath);
    return true;
}

bool Trash::emptyTrash() {
    const QString filesDir = trashPath();
    const QString infoDir = QDir(homeTrashRoot()).filePath(QStringLiteral("info"));
    bool ok = true;
    for (const QString &dirPath : {filesDir, infoDir}) {
        QDir dir(dirPath);
        const auto entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QString &e : entries) {
            QFileInfo fi(dir.filePath(e));
            if (fi.isDir())
                ok = QDir(fi.absoluteFilePath()).removeRecursively() && ok;
            else
                ok = QFile::remove(fi.absoluteFilePath()) && ok;
        }
    }
    return ok;
}
