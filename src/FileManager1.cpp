#include "FileManager1.h"
#include "BrowserController.h"

#include <QUrl>

FileManager1::FileManager1(BrowserController *browser, QObject *parent)
    : QObject(parent)
    , m_browser(browser) {}

void FileManager1::ShowFolders(const QStringList &uris, const QString &startupId) {
    Q_UNUSED(startupId);
    if (uris.isEmpty() || !m_browser)
        return;
    m_browser->navigate(uris.first());
}

void FileManager1::ShowItems(const QStringList &uris, const QString &startupId) {
    Q_UNUSED(startupId);
    if (uris.isEmpty() || !m_browser)
        return;
    const QString local = QUrl(uris.first()).isLocalFile() ? QUrl(uris.first()).toLocalFile() : uris.first();
    m_browser->navigate(local);
    // Select the items that live in the navigated folder.
    QStringList selected;
    for (const QString &uri : uris) {
        const QString p = QUrl(uri).isLocalFile() ? QUrl(uri).toLocalFile() : uri;
        selected << p;
    }
    m_browser->setSelectedPaths(selected);
}

void FileManager1::ShowItemProperties(const QStringList &uris, const QString &startupId) {
    // v0: same as ShowItems — dedicated properties dialog comes later.
    ShowItems(uris, startupId);
}
