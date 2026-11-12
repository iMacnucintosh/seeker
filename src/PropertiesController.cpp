#include "PropertiesController.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QThreadPool>
#include <QUrl>

#include <unistd.h>

namespace {

QString rwxTriplet(bool r, bool w, bool x) {
    return QString(r ? QLatin1Char('r') : QLatin1Char('-'))
        + (w ? QLatin1Char('w') : QLatin1Char('-'))
        + (x ? QLatin1Char('x') : QLatin1Char('-'));
}

} // namespace

PropertiesController::PropertiesController(QObject *parent)
    : QObject(parent) {}

QString PropertiesController::ensureLocal(const QString &pathOrUri) {
    const QUrl url(pathOrUri);
    if (url.isLocalFile())
        return url.toLocalFile();
    if (pathOrUri.startsWith(QLatin1String("file:")))
        return QUrl(pathOrUri).toLocalFile();
    return pathOrUri;
}

QString PropertiesController::displayLocation(const QString &path) {
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

QString PropertiesController::formatSize(qint64 bytes) {
    return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeIecFormat);
}

QString PropertiesController::formatContents(int dirs, int files) {
    QStringList parts;
    if (dirs > 0)
        parts << (dirs == 1 ? QObject::tr("1 folder")
                            : QObject::tr("%1 folders").arg(dirs));
    if (files > 0)
        parts << (files == 1 ? QObject::tr("1 file")
                             : QObject::tr("%1 files").arg(files));
    if (parts.isEmpty())
        return QObject::tr("Empty");
    return parts.join(QStringLiteral(", "));
}

QString PropertiesController::formatPermissions(QFileDevice::Permissions perms) {
    using P = QFileDevice::Permission;
    return rwxTriplet(perms.testFlag(P::ReadOwner), perms.testFlag(P::WriteOwner),
                      perms.testFlag(P::ExeOwner))
        + rwxTriplet(perms.testFlag(P::ReadGroup), perms.testFlag(P::WriteGroup),
                     perms.testFlag(P::ExeGroup))
        + rwxTriplet(perms.testFlag(P::ReadOther), perms.testFlag(P::WriteOther),
                     perms.testFlag(P::ExeOther));
}

void PropertiesController::resetUi() {
    m_isMulti = false;
    m_showModified = false;
    m_showPermissions = false;
    m_showContents = false;
    m_sizeReady = false;
    m_isImage = false;
    m_canToggleExec = false;
    m_executable = false;
    m_title.clear();
    m_kindLabel.clear();
    m_location.clear();
    m_modified.clear();
    m_permissions.clear();
    m_sizeLabel = tr("Working out…");
    m_contentsLabel.clear();
    m_iconName = QStringLiteral("text-x-generic");
    m_previewPath.clear();
}

void PropertiesController::openPaths(const QStringList &rawPaths) {
    QStringList paths;
    paths.reserve(rawPaths.size());
    for (const QString &raw : rawPaths) {
        const QString local = QDir::cleanPath(ensureLocal(raw));
        if (local.isEmpty() || !QFileInfo::exists(local))
            continue;
        if (!paths.contains(local))
            paths << local;
    }
    if (paths.isEmpty())
        return;

    const bool wasOpen = m_open;
    applySnapshot(paths);
    m_open = true;
    if (!wasOpen)
        emit openChanged();
    emit infoChanged();
    emit sizeChanged();
    startSizeWalk(paths);
}

void PropertiesController::close() {
    m_gen.fetchAndAddOrdered(1);
    if (!m_open)
        return;
    m_open = false;
    emit openChanged();
}

bool PropertiesController::setExecutable(bool on) {
    if (!m_canToggleExec || m_previewPath.isEmpty())
        return false;
    using P = QFileDevice::Permission;
    QFile file(m_previewPath);
    QFileDevice::Permissions perms = file.permissions();
    // Mirror chmod +x: grant exec only to classes that can already read.
    const QPair<P, P> classes[] = {{P::ReadOwner, P::ExeOwner},
                                   {P::ReadGroup, P::ExeGroup},
                                   {P::ReadOther, P::ExeOther}};
    for (const auto &[read, exe] : classes)
        perms.setFlag(exe, on && perms.testFlag(read));
    if (!file.setPermissions(perms)) {
        emit infoChanged();
        return false;
    }
    const QFileInfo fi(m_previewPath);
    m_executable = fi.permissions().testFlag(P::ExeOwner);
    m_permissions = formatPermissions(fi.permissions());
    emit infoChanged();
    return true;
}

void PropertiesController::applySnapshot(const QStringList &paths) {
    resetUi();
    m_isMulti = paths.size() > 1;

    if (!m_isMulti) {
        const QFileInfo fi(paths.first());
        m_title = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
        m_location = displayLocation(fi.absolutePath());
        m_modified = QLocale::system().toString(fi.lastModified(), QLocale::ShortFormat);
        m_permissions = formatPermissions(fi.permissions());
        m_showModified = true;
        m_showPermissions = true;
        m_previewPath = fi.absoluteFilePath();

        if (fi.isDir()) {
            m_kindLabel = tr("Folder");
            m_iconName = QStringLiteral("folder");
            m_showContents = true;
            m_contentsLabel = tr("Working out…");
        } else {
            QMimeDatabase mimeDb;
            const QMimeType mime = mimeDb.mimeTypeForFile(fi);
            m_kindLabel = mime.comment().isEmpty() ? mime.name() : mime.comment();
            const QString icon = mime.iconName();
            m_iconName = icon.isEmpty() ? QStringLiteral("text-x-generic") : icon;
            m_isImage = mime.name().startsWith(QLatin1String("image/"));
            m_canToggleExec = fi.isFile() && fi.ownerId() == ::getuid();
            m_executable = fi.permissions().testFlag(QFileDevice::ExeOwner);
            // Plain file size is immediate.
            m_sizeLabel = formatSize(fi.size());
            m_sizeReady = true;
        }
        return;
    }

    m_title = tr("%1 items").arg(paths.size());
    m_kindLabel = tr("Several items");
    m_iconName = QStringLiteral("folder-documents");
    m_showContents = true;
    m_contentsLabel = tr("Working out…");

    QString commonParent;
    bool sameParent = true;
    int topDirs = 0;
    int topFiles = 0;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (fi.isDir())
            ++topDirs;
        else
            ++topFiles;
        const QString parent = fi.absolutePath();
        if (commonParent.isEmpty())
            commonParent = parent;
        else if (commonParent != parent)
            sameParent = false;
    }
    m_location = sameParent ? displayLocation(commonParent) : tr("Several locations");
    // Immediate top-level mix while the recursive walk runs.
    m_contentsLabel = formatContents(topDirs, topFiles);
}

PropertiesController::WalkResult PropertiesController::walkPaths(const QStringList &paths,
                                                                 const QAtomicInt *gen,
                                                                 int myGen,
                                                                 PropertiesController *self) {
    WalkResult out;
    qint64 lastEmitMs = 0;

    auto maybeEmit = [&](bool force) {
        if (!self)
            return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!force && now - lastEmitMs < 100)
            return;
        lastEmitMs = now;
        const qint64 bytes = out.bytes;
        const int files = out.files;
        const int dirs = out.dirs;
        QMetaObject::invokeMethod(
            self,
            [self, myGen, bytes, files, dirs]() { self->applyWalkProgress(myGen, bytes, files, dirs, false); },
            Qt::QueuedConnection);
    };

    auto stillValid = [&]() { return gen->loadAcquire() == myGen; };

    for (const QString &root : paths) {
        if (!stillValid())
            return out;
        const QFileInfo fi(root);
        if (!fi.exists())
            continue;
        if (fi.isDir()) {
            ++out.dirs;
            QDirIterator it(root,
                            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden
                                | QDir::System,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                if (!stillValid())
                    return out;
                it.next();
                const QFileInfo entry = it.fileInfo();
                if (entry.isDir()) {
                    ++out.dirs;
                } else {
                    ++out.files;
                    out.bytes += entry.size();
                }
                maybeEmit(false);
            }
        } else {
            ++out.files;
            out.bytes += fi.size();
            maybeEmit(false);
        }
    }

    maybeEmit(true);
    return out;
}

void PropertiesController::startSizeWalk(const QStringList &paths) {
    const int myGen = m_gen.fetchAndAddOrdered(1) + 1;
    // Single plain file already has size; nothing async.
    if (!m_isMulti && m_sizeReady && !m_showContents)
        return;

    m_sizeReady = false;
    m_sizeLabel = tr("Working out…");
    emit sizeChanged();

    const QStringList copy = paths;
    QThreadPool::globalInstance()->start([this, copy, myGen]() {
        const WalkResult result = walkPaths(copy, &m_gen, myGen, this);
        if (m_gen.loadAcquire() != myGen)
            return;
        QMetaObject::invokeMethod(
            this,
            [this, myGen, result]() {
                applyWalkProgress(myGen, result.bytes, result.files, result.dirs, true);
            },
            Qt::QueuedConnection);
    });
}

void PropertiesController::applyWalkProgress(int gen, qint64 bytes, int files, int dirs, bool done) {
    if (m_gen.loadAcquire() != gen || !m_open)
        return;

    m_sizeLabel = formatSize(bytes);
    // Multi keeps the top-level mix from the snapshot; only a single folder
    // updates contents from the recursive walk (children, excluding the root).
    if (m_showContents && !m_isMulti) {
        const int shownDirs = qMax(0, dirs - 1);
        m_contentsLabel = formatContents(shownDirs, files);
    }
    m_sizeReady = done;
    emit sizeChanged();
}
