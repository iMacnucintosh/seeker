#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

/// Disk-backed image thumbnails: image://thumb/<edge>/<hex-utf8-path>
/// Cache: ~/.cache/seeker/thumbs/<sha1>.jpg  (keyed by path+mtime+size+edge)
class ThumbnailProvider : public QQuickAsyncImageProvider {
public:
    ThumbnailProvider();

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    static QString cacheDir();
    static qint64 maxCacheBytes();

private:
    QThreadPool m_pool;
};
