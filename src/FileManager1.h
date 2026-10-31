#pragma once

#include <QObject>
#include <QStringList>

class BrowserController;
class PropertiesController;

class FileManager1 : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
    explicit FileManager1(BrowserController *browser, PropertiesController *properties,
                          QObject *parent = nullptr);

public slots:
    void ShowFolders(const QStringList &uris, const QString &startupId);
    void ShowItems(const QStringList &uris, const QString &startupId);
    void ShowItemProperties(const QStringList &uris, const QString &startupId);

private:
    static QStringList urisToLocal(const QStringList &uris);

    BrowserController *m_browser = nullptr;
    PropertiesController *m_properties = nullptr;
};
