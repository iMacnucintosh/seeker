#include "SoundPlayer.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent) {
    for (const char *p : {"pw-play", "paplay", "canberra-gtk-play"}) {
        m_player = QStandardPaths::findExecutable(QString::fromLatin1(p));
        if (!m_player.isEmpty())
            break;
    }
}

QString SoundPlayer::findSound(const QString &event) const {
    // Event names come from our own code, but never build a path out of anything odd.
    static const QRegularExpression ok(QStringLiteral("^[a-z0-9-]+$"));
    if (!ok.match(event).hasMatch())
        return {};
    const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &root : roots)
        for (const char *ext : {".oga", ".ogg", ".wav"}) {
            const QString path = root + QStringLiteral("/sounds/freedesktop/stereo/") + event + QLatin1String(ext);
            if (QFile::exists(path))
                return path;
        }
    return {};
}

void SoundPlayer::play(const QString &event) {
    if (!m_enabled || m_player.isEmpty())
        return;
    // Two errors in a row must not machine-gun the speakers.
    if (m_lastAt.isValid() && m_lastAt.elapsed() < 250 && m_lastEvent == event)
        return;
    m_lastEvent = event;
    m_lastAt.start();

    if (m_player.endsWith(QLatin1String("canberra-gtk-play"))) {
        QProcess::startDetached(m_player, {QStringLiteral("-i"), event});
        return;
    }
    const QString file = findSound(event);
    if (!file.isEmpty())
        QProcess::startDetached(m_player, {file});
}
