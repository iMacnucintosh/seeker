#include "Trash.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

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

bool Trash::moveToTrash(const QStringList &paths) {
    const QString root = homeTrashRoot();
    const QString filesDir = QDir(root).filePath(QStringLiteral("files"));
    const QString infoDir = QDir(root).filePath(QStringLiteral("info"));
    QDir().mkpath(filesDir);
    QDir().mkpath(infoDir);

    bool ok = true;
    for (const QString &path : paths) {
        const QFileInfo fi(path);
        if (!fi.exists()) {
            ok = false;
            continue;
        }

        QString destName = fi.fileName();
        QString dest = QDir(filesDir).filePath(destName);
        int n = 1;
        while (QFile::exists(dest)) {
            destName = QStringLiteral("%1.%2").arg(fi.fileName()).arg(n++);
            dest = QDir(filesDir).filePath(destName);
        }

        if (!QFile::rename(path, dest)) {
            // Cross-device: copy then remove.
            if (fi.isDir()) {
                ok = false;
                continue;
            }
            if (!QFile::copy(path, dest) || !QFile::remove(path)) {
                ok = false;
                continue;
            }
        }

        const QString infoPath = QDir(infoDir).filePath(destName + QStringLiteral(".trashinfo"));
        QFile info(infoPath);
        if (info.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QString uri = QUrl::fromLocalFile(fi.absoluteFilePath()).toString(QUrl::FullyEncoded);
            const QString deletion = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"));
            info.write("[Trash Info]\n");
            info.write("Path=");
            info.write(uri.toUtf8());
            info.write("\nDeletionDate=");
            info.write(deletion.toUtf8());
            info.write("\n");
        }
    }
    return ok;
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
