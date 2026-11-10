#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

/// Short system sounds for file operations (archive done, error, trash emptied), like Finder's.
/// Uses the standard freedesktop sound theme (`/usr/share/sounds/freedesktop/stereo/*.oga`) and
/// whichever player is installed (pw-play, then paplay, then canberra-gtk-play). Fire-and-forget:
/// a missing theme or player just means silence.
class SoundPlayer : public QObject {
    Q_OBJECT
public:
    explicit SoundPlayer(QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }

    /// event: freedesktop sound name, e.g. "complete", "dialog-error", "trash-empty".
    Q_INVOKABLE void play(const QString &event);

private:
    QString findSound(const QString &event) const;

    bool m_enabled = true;
    QString m_player;
    QString m_lastEvent;
    QElapsedTimer m_lastAt;
};
