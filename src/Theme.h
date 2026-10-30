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

public:
    explicit Theme(QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    QVariantMap colours() const { return m_colours; }

    QColor background() const { return color("background", "surface"); }
    QColor surface() const { return color("surface"); }
    QColor surfaceContainer() const { return color("surfaceContainer", "surface"); }
    QColor surfaceContainerHigh() const { return color("surfaceContainerHigh", "surfaceContainer"); }
    QColor surfaceContainerHighest() const { return color("surfaceContainerHighest", "surfaceContainerHigh"); }
    QColor surfaceContainerLow() const { return color("surfaceContainerLow", "surface"); }
    QColor surfaceContainerLowest() const { return color("surfaceContainerLowest", "surface"); }
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
    void loadFrom(const QString &path);
    void applyDefaults();

    QString m_mode = QStringLiteral("dark");
    QVariantMap m_colours;
    QFileSystemWatcher m_watcher;
};
