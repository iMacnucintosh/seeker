#pragma once

#include "DirModel.h"
#include "Trash.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class OpenWithController;
class Theme;

class BrowserController : public QObject {
    Q_OBJECT
    Q_PROPERTY(DirModel *model READ model CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY pathChanged)
    Q_PROPERTY(QString displayPath READ displayPath NOTIFY pathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths WRITE setSelectedPaths NOTIFY selectionChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardIsCut READ clipboardIsCut NOTIFY clipboardChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(bool showImagePreviews READ showImagePreviews WRITE setShowImagePreviews NOTIFY showImagePreviewsChanged)
    /// Dolphin-style 2×2 image mosaic on folder icons (grid view).
    Q_PROPERTY(bool showFolderPreviews READ showFolderPreviews WRITE setShowFolderPreviews NOTIFY showFolderPreviewsChanged)
    Q_PROPERTY(int folderPreviewsRevision READ folderPreviewsRevision NOTIFY folderPreviewsChanged)
    Q_PROPERTY(bool showUpButton READ showUpButton WRITE setShowUpButton NOTIFY showUpButtonChanged)
    Q_PROPERTY(bool showRefreshButton READ showRefreshButton WRITE setShowRefreshButton NOTIFY showRefreshButtonChanged)
    Q_PROPERTY(bool showNewFolderButton READ showNewFolderButton WRITE setShowNewFolderButton NOTIFY showNewFolderButtonChanged)
    /// Grid/list icon size step: 0…4 → 48/56/64/72/80 px (default 2 = 64).
    Q_PROPERTY(int iconSizeStep READ iconSizeStep WRITE setIconSizeStep NOTIFY iconSizeStepChanged)
    Q_PROPERTY(int iconSize READ iconSize NOTIFY iconSizeStepChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionIsDir READ selectionIsDir NOTIFY selectionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(bool inTrash READ inTrash NOTIFY pathChanged)
    Q_PROPERTY(bool trashIsEmpty READ trashIsEmpty NOTIFY pathChanged)
    /// Bumps when custom folder icon map changes (bind in QML to refresh badges).
    Q_PROPERTY(int folderIconsRevision READ folderIconsRevision NOTIFY folderIconsChanged)

public:
    explicit BrowserController(QObject *parent = nullptr);

    void setTheme(Theme *theme) { m_theme = theme; }
    void setOpenWith(OpenWithController *openWith) { m_openWith = openWith; }

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
    QString viewMode() const { return m_viewMode; }
    void setViewMode(const QString &mode);
    bool showImagePreviews() const { return m_showImagePreviews; }
    void setShowImagePreviews(bool show);
    bool showFolderPreviews() const { return m_showFolderPreviews; }
    void setShowFolderPreviews(bool show);
    int folderPreviewsRevision() const { return m_folderPreviewsRevision; }
    bool showUpButton() const { return m_showUpButton; }
    void setShowUpButton(bool show);
    bool showRefreshButton() const { return m_showRefreshButton; }
    void setShowRefreshButton(bool show);
    bool showNewFolderButton() const { return m_showNewFolderButton; }
    void setShowNewFolderButton(bool show);
    int iconSizeStep() const { return m_iconSizeStep; }
    void setIconSizeStep(int step);
    int iconSize() const;
    int selectionCount() const { return m_selected.size(); }
    bool selectionIsDir() const;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool inTrash() const;
    bool trashIsEmpty() const;
    int folderIconsRevision() const { return m_folderIconsRevision; }

    Q_INVOKABLE void navigate(const QString &path);
    Q_INVOKABLE void openPath(const QString &path);
    /// Open the default terminal with cwd = dir (empty → currentPath).
    Q_INVOKABLE void openTerminalHere(const QString &dir = {});
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void cutSelection();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void trashSelection();
    /// Permanently erase everything in the trash (no undo).
    Q_INVOKABLE void emptyTrash();
    /// Drop / DnD: copy or move `urls` (file:// or paths) into destDir.
    /// If destDir is the trash, moves to trash instead. Undoable.
    Q_INVOKABLE void dropUrls(const QVariantList &urls, const QString &destDir, bool copy = false);
    /// Newline-joined file:// URIs for Drag.mimeData text/uri-list.
    Q_INVOKABLE QString uriList(const QStringList &paths) const;
    Q_INVOKABLE void renameSelected(const QString &newName);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QString createFolder(const QString &preferredName = {});
    Q_INVOKABLE QString duplicateSelection();
    Q_INVOKABLE void toggleViewMode();
    /// delta +1 / -1 cycles grid → list → details.
    Q_INVOKABLE void cycleViewMode(int delta = 1);
    Q_INVOKABLE QString viewModeLabel(const QString &mode = {}) const;
    Q_INVOKABLE QString viewModeIcon(const QString &mode = {}) const;
    Q_INVOKABLE QString formatSize(qint64 bytes) const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE QString iconUrl(const QString &iconName) const;
    /// Disk-cached preview: image://thumb/<edge>/<hex-path>
    Q_INVOKABLE QString thumbUrl(const QString &path, int edge = 160) const;
    /// First image paths inside a folder for the mosaic (cached by dir mtime).
    Q_INVOKABLE QStringList folderPreviewPaths(const QString &dirPath, int maxCount = 4) const;
    /// Address-bar autocomplete: places + child dirs (no visit history).
    /// Each entry: { path, label, symbol }.
    Q_INVOKABLE QVariantList pathSuggestions(const QString &typed, int maxCount = 14) const;
    /// Material Symbols name for a known place path (home, docs, …), else empty.
    Q_INVOKABLE QString placeSymbol(const QString &path) const;
    /// Custom folder badge, falling back to placeSymbol.
    Q_INVOKABLE QString folderSymbol(const QString &path) const;
    Q_INVOKABLE bool hasCustomFolderIcon(const QString &path) const;
    Q_INVOKABLE void setFolderIcon(const QString &path, const QString &symbol);
    Q_INVOKABLE void clearFolderIcon(const QString &path);
    /// User-pinned sidebar folders (after the built-in places).
    Q_INVOKABLE bool isBuiltinPlace(const QString &path) const;
    Q_INVOKABLE bool isPinned(const QString &path) const;
    Q_INVOKABLE void pinPlace(const QString &path);
    Q_INVOKABLE void unpinPlace(const QString &path);
    /// Pin every local directory among `urls` (file:// or paths). Returns how many were added.
    Q_INVOKABLE int pinPlacesFromUrls(const QVariantList &urls);
    /// Reorder a pinned place relative to another pin (`after` = insert below target).
    Q_INVOKABLE void reorderPinnedPlace(const QString &dragPath, const QString &targetPath, bool after);
    /// Filter Material Symbols names (empty query → destacados + sample).
    Q_INVOKABLE QStringList filterMaterialSymbols(const QString &query, int limit = 360) const;
    Q_INVOKABLE QString uniqueNameIn(const QString &dir, const QString &baseName) const;
    Q_INVOKABLE int currentIndex() const;
    Q_INVOKABLE QString pathAt(int index) const;
    Q_INVOKABLE bool pathIsDir(const QString &path) const;
    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void moveSelection(int delta);
    Q_INVOKABLE void openSelection();

signals:
    void pathChanged();
    void historyChanged();
    void selectionChanged();
    void clipboardChanged();
    void viewModeChanged();
    void showImagePreviewsChanged();
    void showFolderPreviewsChanged();
    void folderPreviewsChanged();
    void showUpButtonChanged();
    void showRefreshButtonChanged();
    void showNewFolderButtonChanged();
    void iconSizeStepChanged();
    void undoChanged();
    void errorOccurred(const QString &message);
    void folderCreated(const QString &path);
    void folderIconsChanged();
    void placesChanged();

private:
    void loadFolderIcons();
    void saveFolderIcons() const;
    void loadSettings();
    void saveSettings() const;
    void savePinnedPlaces() const;
    void seedDefaultPinsIfNeeded();
    void clearFolderPreviewCache() const;
    QString expandTypedPath(const QString &typed) const;
    QVariantMap builtinPlace(const QString &id, const QString &label, const QString &icon,
                             const QString &symbol, const QString &path) const;
    QStringList builtinPlacePaths() const;
    /// Standard home dirs only (Desktop, Documents, …) — never a full home scan.
    struct DefaultPin {
        QString label;
        QString icon;
        QString symbol;
        QString path;
    };
    QVector<DefaultPin> defaultHomePins() const;
    QString labelForPinnedPath(const QString &path) const;
    QString symbolForPinnedPath(const QString &path) const;
    void ensureMaterialSymbolsLoaded() const;
    QString iconsStatePath() const;
    QString settingsPath() const;
    enum class UndoKind {
        Trash,
        Rename,
        CreateFolder,
        PasteCopy,
        PasteMove,
        Duplicate,
    };

    struct UndoOp {
        UndoKind kind = UndoKind::Trash;
        QStringList from; // originals / old paths
        QStringList to;   // trash paths / new paths
    };

    void pushHistory(const QString &path);
    void pushUndo(UndoOp op);
    QString ensureLocal(const QString &pathOrUri) const;
    QString uniqueDestPath(const QString &destDir, const QFileInfo &fi) const;
    bool copyPath(const QString &src, const QString &dest) const;
    bool removePath(const QString &path) const;

    DirModel *m_model = nullptr;
    Theme *m_theme = nullptr;
    OpenWithController *m_openWith = nullptr;
    Trash m_trash;
    QStringList m_history;
    int m_historyIndex = -1;
    QStringList m_selected;
    QStringList m_clipboard;
    bool m_clipboardCut = false;
    QString m_viewMode = QStringLiteral("grid");
    bool m_showImagePreviews = true;
    bool m_showFolderPreviews = false;
    bool m_showUpButton = false;
    bool m_showRefreshButton = false;
    bool m_showNewFolderButton = true;
    int m_iconSizeStep = 2; // 64 px
    mutable int m_folderPreviewsRevision = 0;
    struct FolderPreviewCacheEntry {
        qint64 dirMtime = -1;
        QStringList paths;
    };
    mutable QHash<QString, FolderPreviewCacheEntry> m_folderPreviewCache;
    QVector<UndoOp> m_undo;
    QHash<QString, QString> m_folderIcons; // cleanPath → Material symbol
    int m_folderIconsRevision = 0;
    QStringList m_pinnedPlaces; // cleanPath, ordered
    bool m_pinnedPlacesSeeded = false;
    mutable QStringList m_materialSymbols;
    mutable bool m_materialSymbolsLoaded = false;
};
