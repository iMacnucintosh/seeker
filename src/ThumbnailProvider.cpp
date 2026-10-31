#include "ThumbnailProvider.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QRunnable>
#include <QStandardPaths>
#include <QThread>
#include <QtGlobal>

#include <algorithm>

namespace {

QMutex g_pruneMutex;
int g_writesSincePrune = 0;

QString thumbsRoot() {
    // CacheLocation is already ~/.cache/seeker (applicationName=Seeker).
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("thumbs"));
}

QString cacheFileFor(const QString &absPath, qint64 mtime, qint64 size, int edge) {
    const QByteArray material = (absPath + QLatin1Char('|') + QString::number(mtime) + QLatin1Char('|')
                                 + QString::number(size) + QLatin1Char('|') + QString::number(edge))
                                    .toUtf8();
    const QByteArray hash = QCryptographicHash::hash(material, QCryptographicHash::Sha1).toHex();
    return QDir(thumbsRoot()).filePath(QString::fromLatin1(hash) + QStringLiteral(".jpg"));
}

void touch(const QString &path) {
    // Rewrite first byte to bump mtime (LRU).
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite))
        return;
    const QByteArray head = f.read(1);
    if (!head.isEmpty()) {
        f.seek(0);
        f.write(head);
    }
}

qint64 dirByteSize(const QString &dir) {
    qint64 total = 0;
    const QFileInfoList list = QDir(dir).entryInfoList({QStringLiteral("*.jpg")}, QDir::Files);
    for (const QFileInfo &fi : list)
        total += fi.size();
    return total;
}

void pruneCache(qint64 maxBytes) {
    QMutexLocker lock(&g_pruneMutex);
    const QString root = thumbsRoot();
    QFileInfoList list = QDir(root).entryInfoList({QStringLiteral("*.jpg")}, QDir::Files, QDir::Time);
    std::reverse(list.begin(), list.end()); // oldest first
    qint64 total = 0;
    for (const QFileInfo &fi : list)
        total += fi.size();
    for (const QFileInfo &fi : list) {
        if (total <= maxBytes)
            break;
        total -= fi.size();
        QFile::remove(fi.absoluteFilePath());
    }
}

QImage makeThumb(const QString &absPath, int edge) {
    QImageReader reader(absPath);
    reader.setAutoTransform(true);
    if (!reader.canRead())
        return {};

    const QSize orig = reader.size();
    if (orig.isValid() && !orig.isEmpty()) {
        QSize target = orig;
        target.scale(edge, edge, Qt::KeepAspectRatio);
        if (target.width() < orig.width() || target.height() < orig.height())
            reader.setScaledSize(target);
    }

    QImage img = reader.read();
    if (img.isNull())
        return {};
    if (img.width() > edge || img.height() > edge)
        img = img.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}

/// Qt-recommended: response is also the runnable.
class ThumbResponse : public QQuickImageResponse, public QRunnable {
public:
    ThumbResponse(const QString &absPath, int edge)
        : m_absPath(absPath)
        , m_edge(qBound(32, 512, edge)) {
        setAutoDelete(false);
    }

    void run() override {
        QImage image;
        const QFileInfo fi(m_absPath);
        if (fi.isFile()) {
            const QString cachePath = cacheFileFor(m_absPath, fi.lastModified().toSecsSinceEpoch(),
                                                   fi.size(), m_edge);
            QDir().mkpath(thumbsRoot());

            if (QFile::exists(cachePath)) {
                QImage cached(cachePath);
                if (!cached.isNull()) {
                    touch(cachePath);
                    m_image = cached;
                    emit finished();
                    return;
                }
                QFile::remove(cachePath);
            }

            image = makeThumb(m_absPath, m_edge);
            if (!image.isNull()) {
                if (image.save(cachePath, "JPG", 82)) {
                    bool doPrune = false;
                    {
                        QMutexLocker lock(&g_pruneMutex);
                        if (++g_writesSincePrune >= 24) {
                            g_writesSincePrune = 0;
                            doPrune = true;
                        }
                    }
                    if (doPrune || dirByteSize(thumbsRoot()) > ThumbnailProvider::maxCacheBytes())
                        pruneCache(ThumbnailProvider::maxCacheBytes());
                }
            }
        }
        m_image = image;
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override {
        return m_image.isNull() ? QStringLiteral("thumbnail failed") : QString();
    }

private:
    QString m_absPath;
    int m_edge = 160;
    QImage m_image;
};

} // namespace

ThumbnailProvider::ThumbnailProvider() {
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() / 2));
}

QString ThumbnailProvider::cacheDir() {
    return thumbsRoot();
}

qint64 ThumbnailProvider::maxCacheBytes() {
    return 256ll * 1024ll * 1024ll;
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize) {
    const int slash = id.indexOf(QLatin1Char('/'));
    int edge = 160;
    QString hex = id;
    if (slash > 0) {
        bool ok = false;
        const int parsed = id.left(slash).toInt(&ok);
        if (ok && parsed > 0)
            edge = parsed;
        hex = id.mid(slash + 1);
    }
    if (requestedSize.isValid()) {
        const int want = qMax(requestedSize.width(), requestedSize.height());
        if (want > 0)
            edge = want;
    }

    const QString absPath = QString::fromUtf8(QByteArray::fromHex(hex.toLatin1()));
    auto *response = new ThumbResponse(absPath, edge);
    m_pool.start(response);
    return response;
}
