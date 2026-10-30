#pragma once

#include "DirModel.h"
#include "Trash.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

class BrowserController : public QObject {
    Q_OBJECT
    Q_PROPERTY(DirModel *model READ model CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY pathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY pathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(QVariantList places READ places CONSTANT)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths WRITE setSelectedPaths NOTIFY selectionChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardIsCut READ clipboardIsCut NOTIFY clipboardChanged)

public:
    explicit BrowserController(QObject *parent = nullptr);

    DirModel *model() const { return m_model; }
    QString currentPath() const;
    QString displayPath() const;
    bool canGoBack() const { return m_historyIndex > 0; }
    bool canGoForward() const { return m_historyIndex + 1 < m_history.size(); }
    QVariantList places() const;
    QStringList selectedPaths() const { return m_selected; }
    void setSelectedPaths(const QStringList &paths);
    bool hasClipboard() const { return !m_clipboard.isEmpty(); }
    bool clipboardIsCut() const { return m_clipboardCut; }

    Q_INVOKABLE void navigate(const QString &path);
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void cutSelection();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void trashSelection();
    Q_INVOKABLE void renameSelected(const QString &newName);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QString iconUrl(const QString &iconName) const;

signals:
    void pathChanged();
    void historyChanged();
    void selectionChanged();
    void clipboardChanged();
    void errorOccurred(const QString &message);

private:
    void pushHistory(const QString &path);
    QString ensureLocal(const QString &pathOrUri) const;

    DirModel *m_model = nullptr;
    Trash m_trash;
    QStringList m_history;
    int m_historyIndex = -1;
    QStringList m_selected;
    QStringList m_clipboard;
    bool m_clipboardCut = false;
};
