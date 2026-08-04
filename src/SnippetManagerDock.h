/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
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

#ifndef SNIPPETMANAGERDOCK_H
#define SNIPPETMANAGERDOCK_H

#include <QDockWidget>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

class MainWindow;
class ScintillaNext;
class QListWidget;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QEvent;

struct SnippetDefinition
{
    QString name;
    QString trigger;
    QString body;
};

class SnippetManagerDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit SnippetManagerDock(MainWindow *window, QWidget *parent = nullptr);

    void attachEditor(ScintillaNext *editor);

public slots:
    void insertSelectedSnippet();
    void focusFilter();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void refreshList();
    void selectSnippet(int row);
    void newSnippet();
    void saveSnippet();
    void deleteSnippet();

private:
    struct ActiveSlot
    {
        int number = 0;
        int start = 0;
        int length = 0;
    };

    struct ActiveSession
    {
        QVector<ActiveSlot> navigationSlots;
        int currentIndex = 0;
    };

    static QVector<SnippetDefinition> defaultSnippets();

    void loadSettings();
    void saveSettings() const;
    void updateEditorFields(const SnippetDefinition &snippet);
    void clearEditorFields();
    void showStatus(const QString &message, bool error = false);
    bool insertSnippet(ScintillaNext *editor, const SnippetDefinition &snippet,
                       int start, int end);
    bool tryExpandTrigger(ScintillaNext *editor);
    bool handleTab(ScintillaNext *editor, bool backwards);
    bool updateCurrentSlot(ScintillaNext *editor, ActiveSession *session);
    void selectSlot(ScintillaNext *editor, const ActiveSlot &slot);

    MainWindow *window = nullptr;
    QListWidget *snippetList = nullptr;
    QLineEdit *filterEdit = nullptr;
    QLineEdit *nameEdit = nullptr;
    QLineEdit *triggerEdit = nullptr;
    QPlainTextEdit *bodyEdit = nullptr;
    QLabel *statusLabel = nullptr;
    QVector<SnippetDefinition> snippets;
    int editingIndex = -1;
    QHash<ScintillaNext *, ActiveSession> activeSessions;
    QSet<ScintillaNext *> attachedEditors;
};

#endif // SNIPPETMANAGERDOCK_H
