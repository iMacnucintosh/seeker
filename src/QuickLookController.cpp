#include "QuickLookController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringDecoder>

namespace {

// Enough to read, little enough to show at once
constexpr qint64 kTextBytes = 64 * 1024;
constexpr int kTextLines = 400;
// Width of a rendered PDF page: sharp on a 1440p screen without being heavy
constexpr int kPageWidth = 1600;

bool isTextMime(const QMimeType &mime) {
    if (mime.name().startsWith(QLatin1String("text/")))
        return true;
    return mime.inherits(QStringLiteral("text/plain"));
}

void stopProcess(QPointer<QProcess> &proc) {
    if (!proc)
        return;
    proc->disconnect();
    proc->kill();
    proc->deleteLater();
    proc.clear();
}

} // namespace

QuickLookController::QuickLookController(QObject *parent)
    : QObject(parent) {}

QuickLookController::~QuickLookController() {
    stopProcess(m_infoProc);
    stopProcess(m_renderProc);
}

QString QuickLookController::kindOf(const QString &path) const {
    const QFileInfo fi(path);
    if (!fi.isFile())
        return QStringLiteral("other");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(fi);
    const QString name = mime.name();
    if (name.startsWith(QLatin1String("image/")))
        return QStringLiteral("image");
    if (name.startsWith(QLatin1String("video/")))
        return QStringLiteral("video");
    if (name.startsWith(QLatin1String("audio/")))
        return QStringLiteral("audio");
    if (name == QLatin1String("application/pdf"))
        return QStandardPaths::findExecutable(QStringLiteral("pdftoppm")).isEmpty() ? QStringLiteral("other")
                                                                                       : QStringLiteral("pdf");
    if (isTextMime(mime))
        return QStringLiteral("text");
    return QStringLiteral("other");
}

void QuickLookController::show(const QString &path) {
    stopProcess(m_infoProc);
    stopProcess(m_renderProc);
    ++m_generation;

    const QFileInfo fi(path);
    m_path = fi.absoluteFilePath();
    m_name = fi.fileName();
    m_kind = kindOf(m_path);
    m_text.clear();
    m_page = 0;
    m_pageCount = 0;
    m_pageImage.clear();

    QMimeDatabase db;
    const QMimeType mime = fi.isDir() ? db.mimeTypeForName(QStringLiteral("inode/directory")) : db.mimeTypeForFile(fi);
    m_iconName = fi.isDir() ? QStringLiteral("folder") : mime.iconName();

    if (m_kind == QLatin1String("text")) {
        QFile f(m_path);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = f.read(kTextBytes);
            QStringDecoder decoder(QStringDecoder::Utf8);
            QString decoded = decoder.decode(bytes);
            // Not text after all (NULs, not UTF-8): no preview rather than garbage. A cut multi-byte character
            // at the very end is fine.
            if (bytes.contains('\0') || (decoder.hasError() && bytes.size() < kTextBytes)) {
                m_kind = QStringLiteral("other");
            } else {
                QStringList lines = decoded.split(QLatin1Char('\n'));
                if (lines.size() > kTextLines)
                    lines = lines.mid(0, kTextLines);
                m_text = lines.join(QLatin1Char('\n'));
            }
        } else {
            m_kind = QStringLiteral("other");
        }
    }

    const bool wasOpen = m_open;
    m_open = true;
    emit changed();
    emit pageChanged();
    if (!wasOpen)
        emit openChanged();

    if (m_kind == QLatin1String("pdf")) {
        m_page = 1;
        renderPage();
        countPages();
    }
}

void QuickLookController::close() {
    stopProcess(m_infoProc);
    stopProcess(m_renderProc);
    ++m_generation;
    if (!m_open)
        return;
    m_open = false;
    emit openChanged();
}

void QuickLookController::setPage(int page) {
    if (m_kind != QLatin1String("pdf"))
        return;
    const int last = m_pageCount > 0 ? m_pageCount : m_page;
    page = qBound(1, page, qMax(1, last));
    if (page == m_page)
        return;
    m_page = page;
    renderPage();
}

void QuickLookController::countPages() {
    const quint64 gen = m_generation;
    auto *proc = new QProcess(this);
    m_infoProc = proc;
    proc->setStandardInputFile(QProcess::nullDevice());
    connect(proc, &QProcess::finished, this, [this, proc, gen](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (gen != m_generation || code != 0)
            return;
        static const QRegularExpression re(QStringLiteral("^Pages:\\s+(\\d+)"), QRegularExpression::MultilineOption);
        const auto match = re.match(QString::fromUtf8(proc->readAllStandardOutput()));
        if (!match.hasMatch())
            return;
        m_pageCount = match.captured(1).toInt();
        emit pageChanged();
    });
    proc->start(QStringLiteral("pdfinfo"), {m_path});
}

QString QuickLookController::pageCachePath(int page) const {
    const QFileInfo fi(m_path);
    const QByteArray key = (m_path + QLatin1Char('|') + QString::number(fi.lastModified().toMSecsSinceEpoch())
                            + QLatin1Char('|') + QString::number(fi.size()))
                               .toUtf8();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex());
    // CacheLocation is ~/.cache/seeker
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("quicklook/%1-%2").arg(hash).arg(page));
}

void QuickLookController::renderPage() {
    stopProcess(m_renderProc);
    const QString prefix = pageCachePath(m_page);
    const QString png = prefix + QStringLiteral(".png");
    if (QFileInfo::exists(png)) {
        m_pageImage = png;
        emit pageChanged();
        return;
    }
    m_pageImage.clear();
    emit pageChanged();

    QDir().mkpath(QFileInfo(prefix).absolutePath());
    const quint64 gen = m_generation;
    const int page = m_page;
    auto *proc = new QProcess(this);
    m_renderProc = proc;
    proc->setStandardInputFile(QProcess::nullDevice());
    connect(proc, &QProcess::finished, this, [this, proc, gen, page, png](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (gen != m_generation || page != m_page || code != 0 || !QFileInfo::exists(png))
            return;
        m_pageImage = png;
        emit pageChanged();
    });
    // Paths go as arguments, never through a shell
    proc->start(QStringLiteral("pdftoppm"),
                {QStringLiteral("-f"), QString::number(page), QStringLiteral("-l"), QString::number(page),
                 QStringLiteral("-png"), QStringLiteral("-singlefile"), QStringLiteral("-scale-to-x"),
                 QString::number(kPageWidth), QStringLiteral("-scale-to-y"), QStringLiteral("-1"), m_path, prefix});
}
