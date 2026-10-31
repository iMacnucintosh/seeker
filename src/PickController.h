#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class BrowserController;

/// Portal / CLI file-chooser session. Active only with --pick.
class PickController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active CONSTANT)
    Q_PROPERTY(QString mode READ mode CONSTANT) // file | folder | save
    Q_PROPERTY(bool multiple READ multiple CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
    Q_PROPERTY(QString suggestedName READ suggestedName WRITE setSuggestedName NOTIFY suggestedNameChanged)
    Q_PROPERTY(bool canConfirm READ canConfirm NOTIFY canConfirmChanged)
    Q_PROPERTY(QStringList extensions READ extensions CONSTANT)

public:
    explicit PickController(QObject *parent = nullptr);

    void configure(bool active, const QString &mode, bool multiple, const QString &title,
                   const QString &acceptLabel, const QString &suggestedName,
                   const QString &resultPath, const QStringList &extensions = {});
    void setBrowser(BrowserController *browser);

    bool active() const { return m_active; }
    QString mode() const { return m_mode; }
    bool multiple() const { return m_multiple; }
    QString title() const { return m_title; }
    QString acceptLabel() const { return m_acceptLabel; }
    QString suggestedName() const { return m_suggestedName; }
    void setSuggestedName(const QString &name);
    QStringList extensions() const { return m_extensions; }
    bool canConfirm() const;
    Q_INVOKABLE bool pathMatchesFilter(const QString &path) const;

    Q_INVOKABLE void confirm();
    Q_INVOKABLE void cancel();
    /// Double-click a file in file mode → accept that path.
    Q_INVOKABLE void confirmPaths(const QStringList &paths);

signals:
    void suggestedNameChanged();
    void canConfirmChanged();
    void finished(); // app should quit

private:
    void writeResult(uint response, const QStringList &uris);
    QStringList resolveUris() const;
    static QString toFileUri(const QString &path);

    BrowserController *m_browser = nullptr;
    bool m_active = false;
    QString m_mode = QStringLiteral("file");
    bool m_multiple = false;
    QString m_title;
    QString m_acceptLabel = QStringLiteral("Seleccionar");
    QString m_suggestedName;
    QString m_resultPath;
    QStringList m_extensions; // lowercase, no dots; empty = any
    bool m_done = false;
};
