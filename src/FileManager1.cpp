#include "FileManager1.h"
#include "BrowserController.h"
#include "PropertiesController.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

FileManager1::FileManager1(BrowserController *browser, PropertiesController *properties, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_properties(properties) {}

QStringList FileManager1::urisToLocal(const QStringList &uris) {
    QStringList out;
    out.reserve(uris.size());
    for (const QString &uri : uris) {
        const QUrl url(uri);
        const QString local = url.isLocalFile() ? url.toLocalFile() : uri;
        if (!local.isEmpty())
            out << QDir::cleanPath(local);
    }
    return out;
}

void FileManager1::ShowFolders(const QStringList &uris, const QString &startupId) {
    Q_UNUSED(startupId);
    if (uris.isEmpty() || !m_browser)
        return;
    m_browser->navigate(urisToLocal(uris).value(0));
}

void FileManager1::ShowItems(const QStringList &uris, const QString &startupId) {
    Q_UNUSED(startupId);
    if (uris.isEmpty() || !m_browser)
        return;
    const QStringList paths = urisToLocal(uris);
    if (paths.isEmpty())
        return;
    m_browser->navigate(paths.first());
    m_browser->setSelectedPaths(paths);
}

void FileManager1::ShowItemProperties(const QStringList &uris, const QString &startupId) {
    Q_UNUSED(startupId);
    if (uris.isEmpty() || !m_browser)
        return;
    const QStringList paths = urisToLocal(uris);
    if (paths.isEmpty())
        return;
    // Always open the parent so the selection is visible in the view.
    m_browser->navigate(QFileInfo(paths.first()).absolutePath());
    m_browser->setSelectedPaths(paths);
    if (m_properties)
        m_properties->openPaths(paths);
}
