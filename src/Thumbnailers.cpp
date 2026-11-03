#include "Thumbnailers.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeDatabase>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

#include <algorithm>

namespace {

QStringList xdgDataDirs() {
    QStringList out;
    const QString home = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!home.isEmpty())
        out << home;
    // Extra system paths (Flatpak exports, distro)
    const QByteArray env = qgetenv("XDG_DATA_DIRS");
    if (!env.isEmpty()) {
        for (const QByteArray &p : env.split(':')) {
            const QString s = QString::fromLocal8Bit(p).trimmed();
            if (!s.isEmpty() && !out.contains(s))
                out << s;
        }
    } else {
        out << QStringLiteral("/usr/local/share") << QStringLiteral("/usr/share");
    }
    return out;
}

QString shellQuote(const QString &s) {
    // QProcess::splitCommand understands double quotes; paths with spaces
    // (e.g. "Dash T7.blend") must not be re-tokenized.
    QString out = s;
    out.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    out.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

QString expandExec(QString exec, const QString &absPath, const QString &outPath, int edge) {
    const QString uri = QString::fromUtf8(QUrl::fromLocalFile(absPath).toEncoded());
    const QString qPath = shellQuote(absPath);
    const QString qOut = shellQuote(outPath);
    const QString qUri = shellQuote(uri);
    exec.replace(QLatin1String("%s"), QString::number(std::max(32, edge)));
    exec.replace(QLatin1String("%u"), qUri);
    exec.replace(QLatin1String("%U"), qUri);
    exec.replace(QLatin1String("%i"), qPath);
    exec.replace(QLatin1String("%o"), qOut);
    exec.replace(QLatin1String("%f"), qPath);
    exec.replace(QLatin1String("%F"), qPath);
    return exec;
}

bool tryExecOk(const QString &tryExec) {
    if (tryExec.isEmpty())
        return true;
    if (QFileInfo::exists(tryExec) && QFileInfo(tryExec).isExecutable())
        return true;
    return !QStandardPaths::findExecutable(tryExec).isEmpty();
}

/// blender-thumbnailer often emits an opaque black PNG when the .blend has no
/// embedded file preview (or extraction fails on zstd blends). Prefer the MIME
/// icon over a black tile in that case.
bool isMostlyBlack(const QImage &img) {
    if (img.isNull() || img.width() < 2 || img.height() < 2)
        return true;
    const QImage src = img.convertToFormat(QImage::Format_ARGB32);
    qint64 sum = 0;
    int opaque = 0;
    const int stepX = qMax(1, src.width() / 24);
    const int stepY = qMax(1, src.height() / 24);
    for (int y = 0; y < src.height(); y += stepY) {
        const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < src.width(); x += stepX) {
            const QRgb px = line[x];
            if (qAlpha(px) < 16)
                continue;
            sum += qRed(px) + qGreen(px) + qBlue(px);
            ++opaque;
        }
    }
    if (opaque < 8)
        return true;
    return (sum / (opaque * 3.0)) < 6.0; // mean channel < ~6/255
}

QImage rejectBlack(QImage img) {
    if (isMostlyBlack(img))
        return {};
    return img;
}

} // namespace

Thumbnailers &Thumbnailers::instance() {
    static Thumbnailers reg;
    return reg;
}

Thumbnailers::Thumbnailers() {
    reload();
}

void Thumbnailers::reload() {
    m_entries.clear();
    m_byMime.clear();
    for (const QString &root : xdgDataDirs())
        loadDir(QDir(root).filePath(QStringLiteral("thumbnailers")));
}

void Thumbnailers::loadDir(const QString &dirPath) {
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.thumbnailer")}, QDir::Files);
    for (const QFileInfo &fi : files) {
        QSettings ini(fi.absoluteFilePath(), QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Thumbnailer Entry"));
        Entry e;
        e.tryExec = ini.value(QStringLiteral("TryExec")).toString().trimmed();
        e.exec = ini.value(QStringLiteral("Exec")).toString().trimmed();
        const QString mimes = ini.value(QStringLiteral("MimeType")).toString();
        for (const QString &raw : mimes.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QString m = raw.trimmed();
            if (!m.isEmpty())
                e.mimeTypes << m;
        }
        ini.endGroup();
        if (e.exec.isEmpty() || e.mimeTypes.isEmpty())
            continue;
        if (!tryExecOk(e.tryExec))
            continue;
        const int idx = m_entries.size();
        m_entries.push_back(e);
        for (const QString &mime : e.mimeTypes) {
            if (!m_byMime.contains(mime))
                m_byMime.insert(mime, idx);
        }
    }
}

bool Thumbnailers::supports(const QString &mimeName) const {
    if (mimeName.isEmpty())
        return false;
    if (m_byMime.contains(mimeName))
        return true;
    QMimeDatabase db;
    return supports(db.mimeTypeForName(mimeName));
}

bool Thumbnailers::supportsFile(const QString &absPath) const {
    return entryForFile(absPath) != nullptr;
}

bool Thumbnailers::supports(const QMimeType &mime) const {
    if (!mime.isValid())
        return false;
    if (m_byMime.contains(mime.name()))
        return true;
    for (const QString &a : mime.aliases()) {
        if (m_byMime.contains(a))
            return true;
    }
    for (const QString &p : mime.parentMimeTypes()) {
        if (m_byMime.contains(p))
            return true;
        // One level of parents is enough for most; walk a bit further.
        QMimeDatabase db;
        const QMimeType parent = db.mimeTypeForName(p);
        for (const QString &pp : parent.parentMimeTypes()) {
            if (m_byMime.contains(pp))
                return true;
        }
    }
    return false;
}

const Thumbnailers::Entry *Thumbnailers::entryFor(const QMimeType &mime) const {
    auto lookup = [this](const QString &name) -> const Entry * {
        const auto it = m_byMime.constFind(name);
        if (it == m_byMime.cend())
            return nullptr;
        return &m_entries.at(it.value());
    };
    if (!mime.isValid())
        return nullptr;
    if (const Entry *e = lookup(mime.name()))
        return e;
    for (const QString &a : mime.aliases()) {
        if (const Entry *e = lookup(a))
            return e;
    }
    for (const QString &p : mime.parentMimeTypes()) {
        if (const Entry *e = lookup(p))
            return e;
        QMimeDatabase db;
        for (const QString &pp : db.mimeTypeForName(p).parentMimeTypes()) {
            if (const Entry *e = lookup(pp))
                return e;
        }
    }
    return nullptr;
}

const Thumbnailers::Entry *Thumbnailers::entryForFile(const QString &absPath) const {
    QMimeDatabase db;
    // Extension first: compressed .blend files sniff as application/zstd.
    if (const Entry *e = entryFor(db.mimeTypeForFile(absPath, QMimeDatabase::MatchExtension)))
        return e;
    return entryFor(db.mimeTypeForFile(absPath, QMimeDatabase::MatchContent));
}

QString Thumbnailers::sharedCachePath(const QString &absPath, int edge) {
    const QByteArray uri = QUrl::fromLocalFile(absPath).toEncoded();
    const QByteArray md5 = QCryptographicHash::hash(uri, QCryptographicHash::Md5).toHex();
    const QString name = QString::fromLatin1(md5) + QStringLiteral(".png");
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                         + QStringLiteral("/thumbnails");
    // Prefer a bucket that fits the requested edge (FreeDesktop sizes).
    QStringList buckets;
    if (edge > 256)
        buckets << QStringLiteral("x-large") << QStringLiteral("large") << QStringLiteral("normal");
    else if (edge > 128)
        buckets << QStringLiteral("large") << QStringLiteral("x-large") << QStringLiteral("normal");
    else
        buckets << QStringLiteral("normal") << QStringLiteral("large") << QStringLiteral("x-large");
    buckets << QStringLiteral("xx-large");

    const QFileInfo src(absPath);
    const qint64 mtime = src.lastModified().toSecsSinceEpoch();
    for (const QString &bucket : buckets) {
        const QString path = QDir(base).filePath(bucket + QLatin1Char('/') + name);
        if (!QFile::exists(path))
            continue;
        // Optional MTime check via PNG tEXt is complex; use file mtime vs source.
        const QFileInfo thumb(path);
        if (thumb.lastModified().toSecsSinceEpoch() + 2 < mtime)
            continue; // stale-ish; skip (generators usually rewrite)
        return path;
    }
    return {};
}

QImage Thumbnailers::loadShared(const QString &absPath, int edge) {
    const QString path = sharedCachePath(absPath, edge);
    if (path.isEmpty())
        return {};
    QImage img(path);
    if (img.isNull())
        return {};
    if (img.width() > edge || img.height() > edge)
        img = img.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return rejectBlack(img);
}

QImage Thumbnailers::run(const Entry &entry, const QString &absPath, int edge) {
    QTemporaryFile tmp(QDir::temp().filePath(QStringLiteral("seeker-thumb-XXXXXX.png")));
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return {};
    const QString outPath = tmp.fileName();
    tmp.close();

    const QString prepared = expandExec(entry.exec, absPath, outPath, edge);
    const QStringList parts = QProcess::splitCommand(prepared);
    if (parts.isEmpty()) {
        QFile::remove(outPath);
        return {};
    }
    QString program = parts.first();
    QStringList args = parts.mid(1);
    if (!QFileInfo(program).isAbsolute()) {
        const QString resolved = QStandardPaths::findExecutable(program);
        if (resolved.isEmpty()) {
            QFile::remove(outPath);
            return {};
        }
        program = resolved;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    proc.start(program, args);
    if (!proc.waitForStarted(3000)) {
        QFile::remove(outPath);
        return {};
    }
    // Blender thumbs can be slow; keep a hard cap so the grid stays responsive.
    if (!proc.waitForFinished(20000)) {
        proc.kill();
        proc.waitForFinished(2000);
        QFile::remove(outPath);
        return {};
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QFile::remove(outPath);
        return {};
    }

    QImage img(outPath);
    QFile::remove(outPath);
    if (img.isNull())
        return {};
    if (img.width() > edge || img.height() > edge)
        img = img.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = rejectBlack(img);
    if (img.isNull())
        return {};

    // Also publish into the shared cache so Dolphin/Nautilus/Seeker share work.
    const QByteArray uri = QUrl::fromLocalFile(absPath).toEncoded();
    const QByteArray md5 = QCryptographicHash::hash(uri, QCryptographicHash::Md5).toHex();
    const QString bucket = edge > 256 ? QStringLiteral("x-large")
                         : edge > 128 ? QStringLiteral("large")
                                      : QStringLiteral("normal");
    const QString sharedDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                              + QStringLiteral("/thumbnails/") + bucket;
    QDir().mkpath(sharedDir);
    const QString sharedPath = QDir(sharedDir).filePath(QString::fromLatin1(md5) + QStringLiteral(".png"));
    img.save(sharedPath, "PNG");

    return img;
}
