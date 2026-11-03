#pragma once

#include <QHash>
#include <QImage>
#include <QMimeType>
#include <QString>
#include <QStringList>
#include <QVector>

/// FreeDesktop thumbnailer registry (same idea as Dolphin/Nautilus):
/// loads `*.thumbnailer` from XDG data dirs and matches by MIME type.
class Thumbnailers {
public:
    struct Entry {
        QString tryExec;
        QString exec;
        QStringList mimeTypes;
    };

    static Thumbnailers &instance();

    void reload();
    bool supports(const QMimeType &mime) const;
    bool supports(const QString &mimeName) const;
    /// True if any MIME for this path (extension first, then content) has a thumbnailer.
    bool supportsFile(const QString &absPath) const;
    /// First matching entry for this MIME (including parents/aliases), or null.
    const Entry *entryFor(const QMimeType &mime) const;
    /// Prefer extension MIME (e.g. *.blend → x-blender; content often sniffs as zstd).
    const Entry *entryForFile(const QString &absPath) const;

    /// Shared ~/.cache/thumbnails lookup (MD5 of file:// URI).
    static QString sharedCachePath(const QString &absPath, int edge);
    static QImage loadShared(const QString &absPath, int edge);

    /// Run a thumbnailer Exec line; returns PNG/JPG path in seeker temp or empty.
    static QImage run(const Entry &entry, const QString &absPath, int edge);

private:
    Thumbnailers();
    void loadDir(const QString &dir);

    QVector<Entry> m_entries;
    QHash<QString, int> m_byMime; // mime → index in m_entries
};
