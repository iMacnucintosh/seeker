#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantMap>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QVariantMap colours READ colours NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainerHigh READ surfaceContainerHigh NOTIFY changed)
    /// Floating panels (dialogs, menus, toasts, suggestion lists) must not let the content behind show
    /// through: these are surfaceContainerHigh/Highest without the transparency alpha.
    /// The page surface when it sits on top of other translucent layers (the file area): layer 1.
    Q_PROPERTY(QColor panel READ panel NOTIFY changed)
    Q_PROPERTY(QColor popup READ popup NOTIFY changed)
    Q_PROPERTY(QColor popupHighest READ popupHighest NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainerHighest READ surfaceContainerHighest NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY changed)
    Q_PROPERTY(QColor surfaceContainerLowest READ surfaceContainerLowest NOTIFY changed)
    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY changed)
    Q_PROPERTY(QColor onSurfaceVariant READ onSurfaceVariant NOTIFY changed)
    Q_PROPERTY(QColor outline READ outline NOTIFY changed)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY changed)
    Q_PROPERTY(QColor primary READ primary NOTIFY changed)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY changed)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY changed)
    Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY changed)
    Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY changed)
    Q_PROPERTY(QColor onSecondaryContainer READ onSecondaryContainer NOTIFY changed)
    Q_PROPERTY(QColor error READ error NOTIFY changed)
    Q_PROPERTY(QColor onError READ onError NOTIFY changed)
    // Match Sylphrena appearance.font defaults (with local fallbacks).
    Q_PROPERTY(QString uiFamily READ uiFamily NOTIFY changed)
    Q_PROPERTY(QString iconFamily READ iconFamily NOTIFY changed)
    /// Mirrors GlobalConfig.appearance.transparency (written into theme.json).
    Q_PROPERTY(bool transparencyEnabled READ transparencyEnabled NOTIFY changed)
    Q_PROPERTY(qreal transparencyBase READ transparencyBase NOTIFY changed)

public:
    explicit Theme(QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    QVariantMap colours() const { return m_colours; }
    QString uiFamily() const { return m_uiFamily; }
    QString iconFamily() const { return m_iconFamily; }
    bool transparencyEnabled() const { return m_transparencyEnabled; }
    qreal transparencyBase() const { return m_transparencyBase; }

    QColor background() const { return withAlpha(color("background", "surface")); }
    QColor surface() const { return withAlpha(color("surface")); } // layer 0
    QColor panel() const { return layer(color("surface"), 1); }
    QColor surfaceContainer() const { return layer(color("surfaceContainer", "surface"), 1); }
    QColor popup() const { QColor c = color("surfaceContainerHigh", "surfaceContainer"); c.setAlpha(255); return c; }
    QColor popupHighest() const { QColor c = color("surfaceContainerHighest", "surfaceContainerHigh"); c.setAlpha(255); return c; }
    QColor surfaceContainerHigh() const { return layer(color("surfaceContainerHigh", "surfaceContainer"), 1); }
    QColor surfaceContainerHighest() const { return layer(color("surfaceContainerHighest", "surfaceContainerHigh"), 1); }
    QColor surfaceContainerLow() const { return layer(color("surfaceContainerLow", "surface"), 1); }
    QColor surfaceContainerLowest() const { return layer(color("surfaceContainerLowest", "surface"), 1); }
    QColor onSurface() const { return color("onSurface"); }
    QColor onSurfaceVariant() const { return color("onSurfaceVariant", "onSurface"); }
    QColor outline() const { return color("outline"); }
    QColor outlineVariant() const { return color("outlineVariant", "outline"); }
    QColor primary() const { return color("primary"); }
    QColor onPrimary() const { return color("onPrimary"); }
    QColor primaryContainer() const { return color("primaryContainer", "primary"); }
    QColor onPrimaryContainer() const { return color("onPrimaryContainer", "onPrimary"); }
    QColor secondaryContainer() const { return color("secondaryContainer", "surfaceContainerHigh"); }
    QColor onSecondaryContainer() const { return color("onSecondaryContainer", "onSurface"); }
    QColor error() const { return color("error"); }
    QColor onError() const { return color("onError", "onPrimary"); }

    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    QColor color(const QString &key, const QString &fallback = {}) const;
    QColor withAlpha(const QColor &c) const;
    /// Same rule as the shell's Colours.layer(): layer 0 = the surface with alpha `base`; layer >= 1 =
    /// a luminance-shifted tone with alpha `layers`, so stacked layers do not add up to opaque.
    QColor layer(const QColor &c, int layer) const;
    void loadFrom(const QString &path);
    void applyDefaults();

    QString m_mode = QStringLiteral("dark");
    QVariantMap m_colours;
    QString m_uiFamily;
    QString m_iconFamily;
    bool m_transparencyEnabled = false;
    qreal m_transparencyBase = 0.85;
    qreal m_transparencyLayers = 0.4; // alpha of layer >= 1 (shell: appearance.transparency.layers)
    qreal m_wallLuminance = 0.5;      // luminance of the wallpaper, drives the layer tone shift
    QFileSystemWatcher m_watcher;

    static QString pickFamily(const QStringList &candidates);
};
