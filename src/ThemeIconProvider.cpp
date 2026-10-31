#include "ThemeIconProvider.h"
#include "Theme.h"

#include <QIcon>
#include <QImage>
#include <QUrlQuery>

ThemeIconProvider::ThemeIconProvider(Theme *theme)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_theme(theme) {}

bool ThemeIconProvider::isFolderIcon(const QString &name) {
    return name == QLatin1String("folder")
        || name == QLatin1String("inode-directory")
        || name.startsWith(QLatin1String("folder-"));
}

QPixmap ThemeIconProvider::tintFolderAccent(const QPixmap &src, const QColor &accent) {
    if (src.isNull() || !accent.isValid())
        return src;

    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal targetHue = accent.hslHueF();
    if (targetHue < 0)
        return src;

    const qreal targetSat = qBound(0.15, 0.85, accent.hslSaturationF());

    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QColor c = QColor::fromRgba(line[x]);
            if (c.alpha() < 8)
                continue;
            // Papirus folder blues sit ~200–220° with noticeable saturation.
            const qreal hue = c.hslHueF();
            const qreal sat = c.hslSaturationF();
            const qreal lit = c.lightnessF();
            if (sat < 0.18 || hue < 0.0)
                continue;
            // Blue–cyan band (Papirus #5294e2 ≈ 0.58).
            if (hue < 0.48 || hue > 0.72)
                continue;
            c.setHslF(targetHue, qBound(0.12, 0.9, sat * 0.55 + targetSat * 0.45), lit, c.alphaF());
            line[x] = c.rgba();
        }
    }
    return QPixmap::fromImage(img);
}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    // id may be "folder" or "folder?c=e4c09b" (cache-bust + explicit accent).
    QString name = id;
    QColor accent;
    const int qpos = id.indexOf(QLatin1Char('?'));
    if (qpos >= 0) {
        name = id.left(qpos);
        const QUrlQuery query(id.mid(qpos + 1));
        const QString hex = query.queryItemValue(QStringLiteral("c"));
        if (!hex.isEmpty())
            accent = QColor(QLatin1Char('#') + hex);
    }
    if (!accent.isValid() && m_theme)
        accent = m_theme->primaryContainer();

    const QIcon icon = QIcon::fromTheme(name);
    const int w = requestedSize.width() > 0 ? requestedSize.width() : 48;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : 48;
    QPixmap pix = icon.pixmap(w, h);
    if (isFolderIcon(name) && accent.isValid())
        pix = tintFolderAccent(pix, accent);

    if (size)
        *size = pix.size();
    return pix;
}
