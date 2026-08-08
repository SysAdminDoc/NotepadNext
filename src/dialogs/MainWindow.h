/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QActionGroup>
#include <QHash>
#include <QIcon>
#include <QToolButton>

#include "DockedEditor.h"

#include "MacroManager.h"
#include "ScintillaNext.h"
#include "NppImporter.h"
#include "SearchResultsCollector.h"

namespace Ui {
class MainWindow;
}

class NotepadNextApplication;
class Macro;
class Settings;
class QuickFindWidget;
class ZoomEventWatcher;
class Converter;
class DefaultDirectoryManager;
class TabsQuickActionsBar;
class MarkdownPreviewDock;
class TerminalDock;
class FindInFilesDock;
class HexEditorDock;
class RegexBuilderDock;
class ScriptConsoleDock;
class SnippetManagerDock;
#ifdef Q_OS_WIN
class WindowsTitleBar;
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(NotepadNextApplication *app);
    ~MainWindow() override;

    bool isAnyUnsaved() const;

    void setupLanguageMenu();
    ScintillaNext *currentEditor() const;
    int editorCount() const;
    QVector<ScintillaNext *> editors() const;
    DockedEditor *getDockedEditor() const { return dockedEditor; }

public slots:
    void newFile();

    void openFileDialog();
    void openFile(const QString &filePath);

    void openFolderAsWorkspaceDialog();
    void setFolderAsWorkspacePath(const QString &dir);

    void reloadFile();

    void closeCurrentFile();
    void closeFile(ScintillaNext *editor);
    void closeAllFiles();
    void closeAllExceptActive();
    void closeAllToLeft();
    void closeAllToRight();

    bool saveCurrentFile();
    bool saveFile(ScintillaNext *editor);

    bool saveCurrentFileAsDialog();
    bool saveCurrentFileAs(const QString &fileName);
    bool saveFileAs(ScintillaNext *editor, const QString &fileName);

    bool saveCopyAsDialog();
    bool saveCopyAs(const QString &fileName);
    bool saveAll();
    bool saveAllEditors(const QVector<ScintillaNext *> &editors);

    void exportAsFormat(Converter *converter, const QString &filter);
    void copyAsFormat(Converter *converter, const QString &mimeType);

    void renameFile();

    void moveCurrentFileToTrash();
    void moveFileToTrash(ScintillaNext *editor);

    void print();

    void convertEOLs(int eolMode);

    void showFindReplaceDialog(int index);

    void updateFileStatusBasedUi(ScintillaNext *editor);
    void updateEOLBasedUi(ScintillaNext *editor);
    void updateDocumentBasedUi(Scintilla::Update updated);
    void updateSelectionBasedUi(ScintillaNext *editor);
    void updateContentBasedUi(ScintillaNext *editor);
    void updateSaveStatusBasedUi(ScintillaNext *editor);
    void updateEditorPositionBasedUi();
    void updateLanguageBasedUi(ScintillaNext *editor);
    void updateGui(ScintillaNext *editor);

    void detectLanguage(ScintillaNext *editor);

    void setLanguage(ScintillaNext *editor, const QString &languageName);

    void bringWindowToForeground();
    void focusIn();

    void addEditor(ScintillaNext *editor);

    void checkForUpdates(bool silent = false);

    void restoreWindowState();

    void switchToEditor(const ScintillaNext *editor);

signals:
    void editorActivated(ScintillaNext *editor);
    void aboutToClose();
    void fileDialogAccepted(const QString &filePath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#ifdef Q_OS_WIN
    void showEvent(QShowEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void tabBarRightClicked(ScintillaNext *editor);
    void languageMenuTriggered();
    void checkForUpdatesFinished(QString url);
    void activateEditor(ScintillaNext *editor);

private:
    Ui::MainWindow *ui = Q_NULLPTR;
    NotepadNextApplication *app = Q_NULLPTR;
    DockedEditor *dockedEditor = Q_NULLPTR;
    MarkdownPreviewDock *markdownPreview = Q_NULLPTR;
    TerminalDock *terminalDock = Q_NULLPTR;
    FindInFilesDock *findInFilesDock = Q_NULLPTR;
    HexEditorDock *hexEditorDock = Q_NULLPTR;
    RegexBuilderDock *regexBuilderDock = Q_NULLPTR;
    ScriptConsoleDock *scriptConsoleDock = Q_NULLPTR;
    SnippetManagerDock *snippetManagerDock = Q_NULLPTR;
    QAction *stageCurrentFileAction = Q_NULLPTR;
    QAction *unstageCurrentFileAction = Q_NULLPTR;
    QAction *blameGutterAction = Q_NULLPTR;

    QScopedPointer<SearchResultsCollector> searchResults;

    template <typename Method>
    void connectEditorAction(QAction* action, Method method) {
        connect(action, &QAction::triggered, this, [this, method]() {
            if (auto* editor = currentEditor()) {
                (editor->*method)();
            }
        });
    }
    template <typename Method, typename Arg>
    void connectEditorAction(QAction* action, Method method, Arg value) {
        connect(action, &QAction::triggered, this, [this, method, value]() {
            if (auto* editor = currentEditor()) {
                (editor->*method)(value);
            }
        });
    }
    void applyStyleSheet();
    void applyCustomShortcuts();
    void initUpdateCheck();
    ScintillaNext *getInitialEditor();
    void openFileList(const QStringList &fileNames);
    bool checkEditorsBeforeClose(const QVector<ScintillaNext *> &editors);
    bool checkFileForModification(ScintillaNext *editor);
    bool resolveExternalSaveConflict(ScintillaNext *editor);
    void openExternalVersion(ScintillaNext *editor);
    void showSaveErrorMessage(ScintillaNext *editor, QFileDevice::FileError error);
    void showEditorZoomLevelIndicator();
    void setupThemeMenu();
    void setupIconThemeMenu();
    void applyIconTheme();
#ifdef Q_OS_WIN
    void setupWindowsTitleBar();
    void applyWindowsFrameEffects();
#endif

    enum class UserSaveAction { SaveAll, DiscardAll, Cancel };
    UserSaveAction promptForSave(const QVector<ScintillaNext *> &editors);

    void saveSettings() const;
    void restoreSettings();

    ISearchResultsHandler *determineSearchResultsHandler();

    QActionGroup *languageActionGroup;
    QActionGroup *themeActionGroup = Q_NULLPTR;
    QActionGroup *iconThemeActionGroup = Q_NULLPTR;
    QHash<QAction *, QIcon> originalActionIcons;
    QHash<QToolButton *, QIcon> originalToolButtonIcons;
#ifdef Q_OS_WIN
    WindowsTitleBar *windowsTitleBar = Q_NULLPTR;
#endif

    TabsQuickActionsBar *tabsQuickActionsBar = Q_NULLPTR;

    //NppImporter *npp;

    MacroManager macroManager;
    DefaultDirectoryManager *defaultDirectoryManager;

    ZoomEventWatcher *zoomEventWatcher;
    int zoomLevel = 0;
    int contextMenuPos = 0;
    QMenu *buildMenu(QStringList actionNames);
};

#endif // MAINWINDOW_H
