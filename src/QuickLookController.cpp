#include "QuickLookController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QFontDatabase>
#include <QFontInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QtConcurrent/QtConcurrentRun>

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
    : QObject(parent) {
    // Rendered PDF pages nobody looked at for two weeks go, off the UI thread
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("quicklook"));
    (void)QtConcurrent::run([dir] {
        const QDateTime limit = QDateTime::currentDateTime().addDays(-14);
        const auto files = QDir(dir).entryInfoList({QStringLiteral("*.png")}, QDir::Files);
        for (const QFileInfo &fi : files) {
            if (fi.lastModified() < limit)
                QFile::remove(fi.absoluteFilePath());
        }
    });
}

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
    // Markdown is shown formatted (headings, lists, tables, code, images next to it)
    if (name == QLatin1String("text/markdown") || mime.inherits(QStringLiteral("text/markdown")))
        return QStringLiteral("markdown");
    if (isTextMime(mime))
        return QStringLiteral("text");
    return QStringLiteral("other");
}

QString QuickLookController::markdownHtml(const QString &markdown, const QColor &text, const QColor &muted,
                                         const QColor &accent, const QColor &codeBackground, const QColor &rule) const {
    QTextDocument doc;
    doc.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    // A real monospace family: the platform's "fixed font" can come back as a proportional one (Noto Sans here), so
    // ask fontconfig for a monospace face instead
    QFont monoQuery(QStringLiteral("monospace"));
    monoQuery.setStyleHint(QFont::Monospace);
    monoQuery.setFixedPitch(true);
    const QStringList mono{QFontInfo(monoQuery).family(), QStringLiteral("monospace")};

    struct Span {
        int from, length;
        QTextCharFormat format;
    };
    QList<Span> spans;
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat bf = block.blockFormat();
        const int heading = bf.headingLevel();
        const bool code = bf.hasProperty(QTextFormat::BlockCodeFence) || bf.nonBreakableLines();
        const bool quote = bf.hasProperty(QTextFormat::BlockQuoteLevel) && bf.intProperty(QTextFormat::BlockQuoteLevel) > 0;
        if (heading > 0) {
            bf.setTopMargin(heading <= 2 ? 20 : 14);
            bf.setBottomMargin(heading == 1 ? 10 : 6);
        } else if (code) {
            // One block per line: only the outer ones get room, so the code reads as one box
            const bool prevCode = block.previous().isValid() && block.previous().blockFormat().nonBreakableLines();
            const bool nextCode = block.next().isValid() && block.next().blockFormat().nonBreakableLines();
            bf.setTopMargin(prevCode ? 0 : 6);
            bf.setBottomMargin(nextCode ? 0 : 10);
            bf.setBackground(codeBackground);
            bf.setLeftMargin(0);
            bf.setTextIndent(10);
        } else if (block.textList()) {
            // List items close together; the list as a whole gets room after it
            const bool nextInList = block.next().isValid() && block.next().textList();
            bf.setTopMargin(0);
            bf.setBottomMargin(nextInList ? 2 : 8);
        } else {
            bf.setBottomMargin(8);
        }
        if (quote)
            bf.setLeftMargin(18 * bf.intProperty(QTextFormat::BlockQuoteLevel));
        // Code lines touch, so the box reads as one; the rest breathes a little
        bf.setLineHeight(code ? 100 : 120, QTextBlockFormat::ProportionalHeight);
        QTextCursor(block).setBlockFormat(bf);

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            QTextCharFormat cf = fragment.charFormat();
            if (cf.isImageFormat())
                continue;
            cf.setForeground(quote ? muted : text);
            if (cf.isAnchor()) {
                cf.setForeground(accent);
                cf.setFontUnderline(false);
            }
            if (code || cf.fontFixedPitch()) {
                cf.setFontFamilies(mono);
                cf.setFontFixedPitch(true);
                if (!code)
                    cf.setBackground(codeBackground);
            }
            if (heading > 0)
                cf.setFontWeight(QFont::Bold);
            spans.append({fragment.position(), fragment.length(), cf});
        }
    }
    for (const Span &span : spans) {
        QTextCursor cursor(&doc);
        cursor.setPosition(span.from);
        cursor.setPosition(span.from + span.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(span.format);
    }

    // Tables: soft borders and room inside the cells
    for (QTextFrame *frame : doc.rootFrame()->childFrames()) {
        if (auto *table = qobject_cast<QTextTable *>(frame)) {
            QTextTableFormat tf = table->format();
            tf.setBorderBrush(rule);
            tf.setBorder(1);
            tf.setBorderCollapse(true);
            tf.setCellPadding(6);
            tf.setBottomMargin(10);
            table->setFormat(tf);
        }
    }
    return doc.toHtml();
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

    if (m_kind == QLatin1String("text") || m_kind == QLatin1String("markdown")) {
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
        QFile(png).setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
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
