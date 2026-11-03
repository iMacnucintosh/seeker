#include "PickController.h"
#include "BrowserController.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

namespace {

/// Portal / callers often pass English GTK labels ("_Select"). Seeker UI is ES.
QString localizeAcceptLabel(QString label) {
    label = label.trimmed();
    while (label.startsWith(QLatin1Char('_')))
        label.remove(0, 1);
    const QString lower = label.toLower();
    if (label.isEmpty() || lower == QLatin1String("select") || lower == QLatin1String("open")
        || lower == QLatin1String("choose") || lower == QLatin1String("ok"))
        return QStringLiteral("Seleccionar");
    if (lower == QLatin1String("save") || lower == QLatin1String("save as"))
        return QStringLiteral("Guardar");
    if (lower == QLatin1String("cancel"))
        return QStringLiteral("Cancelar");
    return label;
}

QString localizePickTitle(QString title) {
    title = title.trimmed();
    while (title.startsWith(QLatin1Char('_')))
        title.remove(0, 1);
    const QString lower = title.toLower();
    if (title.isEmpty())
        return QStringLiteral("Seleccionar");
    if (lower == QLatin1String("select a file") || lower == QLatin1String("open file")
        || lower == QLatin1String("open files"))
        return QStringLiteral("Seleccionar archivo");
    if (lower == QLatin1String("select a folder") || lower == QLatin1String("select folder")
        || lower == QLatin1String("open folder"))
        return QStringLiteral("Seleccionar carpeta");
    if (lower == QLatin1String("select an image") || lower == QLatin1String("select image"))
        return QStringLiteral("Seleccionar imagen");
    if (lower == QLatin1String("select a wallpaper") || lower == QLatin1String("choose wallpaper"))
        return QStringLiteral("Seleccionar fondo");
    if (lower == QLatin1String("choose icon") || lower == QLatin1String("select icon"))
        return QStringLiteral("Elegir icono");
    if (lower == QLatin1String("select a profile picture"))
        return QStringLiteral("Seleccionar foto de perfil");
    if (lower == QLatin1String("save file") || lower == QLatin1String("save as"))
        return QStringLiteral("Guardar archivo");
    return title;
}

} // namespace

PickController::PickController(QObject *parent)
    : QObject(parent) {}

void PickController::configure(bool active, const QString &mode, bool multiple, const QString &title,
                               const QString &acceptLabel, const QString &suggestedName,
                               const QString &resultPath, const QStringList &extensions) {
    m_active = active;
    m_mode = mode.isEmpty() ? QStringLiteral("file") : mode;
    m_multiple = multiple;
    m_title = localizePickTitle(title);
    m_acceptLabel = localizeAcceptLabel(acceptLabel);
    m_suggestedName = suggestedName;
    m_resultPath = resultPath;
    m_extensions.clear();
    for (QString e : extensions) {
        e = e.trimmed().toLower();
        if (e.startsWith(QLatin1Char('.')))
            e.remove(0, 1);
        if (e == QLatin1String("*") || e.isEmpty())
            continue;
        m_extensions.append(e);
    }
}

void PickController::setBrowser(BrowserController *browser) {
    if (m_browser == browser)
        return;
    if (m_browser)
        disconnect(m_browser, nullptr, this, nullptr);
    m_browser = browser;
    if (m_browser) {
        connect(m_browser, &BrowserController::selectionChanged, this, &PickController::canConfirmChanged);
        connect(m_browser, &BrowserController::pathChanged, this, &PickController::canConfirmChanged);
    }
    emit canConfirmChanged();
}

void PickController::setSuggestedName(const QString &name) {
    if (m_suggestedName == name)
        return;
    m_suggestedName = name;
    emit suggestedNameChanged();
    emit canConfirmChanged();
}

bool PickController::pathMatchesFilter(const QString &path) const {
    if (m_extensions.isEmpty())
        return true;
    const QString suffix = QFileInfo(path).suffix().toLower();
    return m_extensions.contains(suffix);
}

bool PickController::canConfirm() const {
    if (!m_active || !m_browser || m_done)
        return false;
    if (m_mode == QLatin1String("save")) {
        const QString name = m_suggestedName.trimmed();
        if (name.isEmpty() || m_browser->currentPath().isEmpty())
            return false;
        return pathMatchesFilter(m_browser->currentPath() + QLatin1Char('/') + name)
            || m_extensions.isEmpty();
    }
    if (m_mode == QLatin1String("folder"))
        return true; // current folder always valid; selection optional
    // file
    const QStringList sel = m_browser->selectedPaths();
    if (sel.isEmpty())
        return false;
    for (const QString &p : sel) {
        if (QFileInfo(p).isDir())
            return false;
        if (!pathMatchesFilter(p))
            return false;
    }
    return true;
}

QString PickController::toFileUri(const QString &path) {
    return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

QStringList PickController::resolveUris() const {
    QStringList uris;
    if (!m_browser)
        return uris;

    if (m_mode == QLatin1String("save")) {
        const QString name = m_suggestedName.trimmed();
        if (name.isEmpty())
            return uris;
        const QString dest = m_browser->currentPath() + QLatin1Char('/') + name;
        uris << toFileUri(dest);
        return uris;
    }

    if (m_mode == QLatin1String("folder")) {
        const QStringList sel = m_browser->selectedPaths();
        if (!sel.isEmpty()) {
            for (const QString &p : sel) {
                if (!QFileInfo(p).isDir())
                    continue;
                uris << toFileUri(p);
                if (!m_multiple)
                    break;
            }
        }
        if (uris.isEmpty())
            uris << toFileUri(m_browser->currentPath());
        return uris;
    }

    // file
    for (const QString &p : m_browser->selectedPaths()) {
        if (QFileInfo(p).isDir())
            continue;
        if (!pathMatchesFilter(p))
            continue;
        uris << toFileUri(p);
        if (!m_multiple)
            break;
    }
    return uris;
}

void PickController::writeResult(uint response, const QStringList &uris) {
    if (m_done)
        return;
    m_done = true;

    QJsonObject obj;
    obj.insert(QStringLiteral("response"), static_cast<int>(response));
    QJsonArray arr;
    for (const QString &u : uris)
        arr.append(u);
    obj.insert(QStringLiteral("uris"), arr);

    if (!m_resultPath.isEmpty()) {
        QSaveFile file(m_resultPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            file.write("\n");
            file.commit();
        }
    }

    emit finished();
}

void PickController::confirm() {
    if (!canConfirm())
        return;
    const QStringList uris = resolveUris();
    if (uris.isEmpty()) {
        writeResult(2, {});
        return;
    }
    writeResult(0, uris);
}

void PickController::confirmPaths(const QStringList &paths) {
    if (!m_active || m_done)
        return;
    QStringList uris;
    for (const QString &p : paths) {
        if (p.isEmpty())
            continue;
        if (m_mode == QLatin1String("folder") && !QFileInfo(p).isDir())
            continue;
        if (m_mode == QLatin1String("file") && QFileInfo(p).isDir())
            continue;
        if (m_mode == QLatin1String("file") && !pathMatchesFilter(p))
            continue;
        uris << toFileUri(p);
        if (!m_multiple)
            break;
    }
    if (uris.isEmpty())
        return;
    writeResult(0, uris);
}

void PickController::cancel() {
    writeResult(1, {});
}
