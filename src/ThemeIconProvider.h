#pragma once

#include <QColor>
#include <QQuickImageProvider>

class Theme;

/// Freedesktop theme icons; folder glyphs are hue-shifted to Theme.primary.
class ThemeIconProvider : public QQuickImageProvider {
public:
    explicit ThemeIconProvider(Theme *theme);

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    static bool isFolderIcon(const QString &name);
    static QPixmap tintFolderAccent(const QPixmap &src, const QColor &accent);

    Theme *m_theme = nullptr;
};
