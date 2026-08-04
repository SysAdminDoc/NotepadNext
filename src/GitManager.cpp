/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "GitManager.h"

#include "EditorManager.h"
#include "ScintillaNext.h"

#include <Scintilla.h>

#include <QMetaObject>

#include <utility>


namespace
{
constexpr int GitAddedMarker = 25;
constexpr int GitModifiedMarker = 26;
constexpr int GitDeletedMarker = 27;
constexpr int GitChangeMargin = 1;
constexpr int GitBlameMargin = 3;
constexpr int GitRefreshDelayMs = 250;

QString blameLabel(const GitRepository::BlameLine &line)
{
    QString author = line.author.trimmed();
    if (author.isEmpty()) {
        author = QObject::tr("Working tree");
    }
    if (line.uncommitted) {
        return author + QStringLiteral(" *");
    }
    if (!line.commit.isEmpty()) {
        return author + QLatin1Char(' ') + line.commit;
    }
    return author;
}
}

GitManager::GitManager(EditorManager *editorManager, QObject *parent)
    : QObject(parent), editorManager(editorManager)
{
    Q_ASSERT(editorManager != nullptr);

    gitInitialized = GitRepository::initialize();
    if (!gitInitialized) {
        qWarning("Unable to initialize libgit2; Git integration is disabled");
        return;
    }

    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(GitRefreshDelayMs);
    connect(&refreshTimer, &QTimer::timeout, this, &GitManager::refreshPendingEditors);
    connect(editorManager, &EditorManager::editorCreated, this, &GitManager::attachEditor);
    connect(editorManager, &EditorManager::editorClosed, this, &GitManager::detachEditor);
}

GitManager::~GitManager()
{
    pendingEditors.clear();
    states.clear();
    if (gitInitialized) {
        GitRepository::shutdown();
    }
}

void GitManager::attachEditor(ScintillaNext *editor)
{
    if (!gitInitialized || !editor || states.contains(editor)) {
        return;
    }

    states.insert(editor, {});
    configureEditor(editor);
    editor->setModEventMask(editor->modEventMask() | SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);

    connect(editor, &ScintillaNext::notify, this, [this, editor](Scintilla::NotificationData *notification) {
        if (notification && notification->nmhdr.code == Scintilla::Notification::Modified &&
            (Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::InsertText) ||
             Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::DeleteText))) {
            scheduleRefresh(editor);
        }
    });
    connect(editor, &ScintillaNext::renamed, this, [this, editor]() { scheduleRefresh(editor); });
    connect(editor, &ScintillaNext::saved, this, [this, editor]() { scheduleRefresh(editor); });
    connect(editor, &ScintillaNext::reloaded, this, [this, editor]() { scheduleRefresh(editor); });

    QMetaObject::invokeMethod(this, [this, editor]() {
        if (states.contains(editor)) {
            refreshNow(editor);
        }
    }, Qt::QueuedConnection);
}

void GitManager::detachEditor(ScintillaNext *editor)
{
    if (!editor) {
        return;
    }
    pendingEditors.remove(editor);
    states.remove(editor);
}

void GitManager::scheduleRefresh(ScintillaNext *editor)
{
    if (!editor || !states.contains(editor)) {
        return;
    }
    pendingEditors.insert(editor);
    refreshTimer.start();
}

void GitManager::refreshPendingEditors()
{
    const QSet<ScintillaNext *> editors = std::exchange(pendingEditors, {});
    for (ScintillaNext *editor : editors) {
        if (states.contains(editor)) {
            refreshNow(editor);
        }
    }
}

void GitManager::configureEditor(ScintillaNext *editor)
{
    editor->markerDefine(GitAddedMarker, SC_MARK_VLINE);
    editor->markerSetFore(GitAddedMarker, 0x2E8B57);
    editor->markerSetBack(GitAddedMarker, 0x2E8B57);
    editor->markerSetAlpha(GitAddedMarker, 220);

    editor->markerDefine(GitModifiedMarker, SC_MARK_VLINE);
    editor->markerSetFore(GitModifiedMarker, 0x2F80ED);
    editor->markerSetBack(GitModifiedMarker, 0x2F80ED);
    editor->markerSetAlpha(GitModifiedMarker, 220);

    editor->markerDefine(GitDeletedMarker, SC_MARK_VLINE);
    editor->markerSetFore(GitDeletedMarker, 0xD64545);
    editor->markerSetBack(GitDeletedMarker, 0xD64545);
    editor->markerSetAlpha(GitDeletedMarker, 220);

    const int mask = editor->marginMaskN(GitChangeMargin) |
        (1 << GitAddedMarker) | (1 << GitModifiedMarker) | (1 << GitDeletedMarker);
    editor->setMarginMaskN(GitChangeMargin, mask);
    editor->setMarginWidthN(GitChangeMargin, 6);

    editor->setMarginTypeN(GitBlameMargin, SC_MARGIN_TEXT);
    editor->setMarginMaskN(GitBlameMargin, 0);
    editor->setMarginWidthN(GitBlameMargin, 0);
}

void GitManager::clearDecorations(ScintillaNext *editor)
{
    editor->markerDeleteAll(GitAddedMarker);
    editor->markerDeleteAll(GitModifiedMarker);
    editor->markerDeleteAll(GitDeletedMarker);
    editor->marginTextClearAll();
}

void GitManager::updateBlameMargin(ScintillaNext *editor, const QVector<GitRepository::BlameLine> &lines)
{
    if (!showBlameGutter) {
        editor->setMarginWidthN(GitBlameMargin, 0);
        return;
    }

    int width = static_cast<int>(editor->textWidth(STYLE_LINENUMBER, "Working tree 0000000")) + 8;
    for (int line = 0; line < lines.size(); ++line) {
        const QString label = blameLabel(lines.at(line));
        const QByteArray labelBytes = label.toUtf8();
        editor->marginSetText(line, labelBytes.constData());
        editor->marginSetStyle(line, STYLE_LINENUMBER);
        width = qMax(width, static_cast<int>(editor->textWidth(STYLE_LINENUMBER, labelBytes.constData())) + 8);
    }
    editor->setMarginWidthN(GitBlameMargin, qMin(width, 280));
}

void GitManager::refreshNow(ScintillaNext *editor)
{
    const auto it = states.find(editor);
    if (it == states.end() || !editor) {
        return;
    }

    clearDecorations(editor);
    it->fileState = {};
    if (editor->isFile()) {
        const QString filePath = editor->getFilePath();
        if (!filePath.isEmpty()) {
            it->fileState = GitRepository::state(filePath);
            if (it->fileState.inRepository && it->fileState.error.isEmpty()) {
                QString changesError;
                const QVector<GitRepository::LineChange> changes = GitRepository::changes(filePath, &changesError);
                if (!changesError.isEmpty() && it->fileState.error.isEmpty()) {
                    it->fileState.error = changesError;
                }

                for (const GitRepository::LineChange &change : changes) {
                    const int lineCount = static_cast<int>(editor->lineCount());
                    if (change.line < 0 || change.line >= lineCount) {
                        continue;
                    }
                    int marker = GitModifiedMarker;
                    if (change.kind == GitRepository::LineChangeKind::Added) {
                        marker = GitAddedMarker;
                    }
                    else if (change.kind == GitRepository::LineChangeKind::Deleted) {
                        marker = GitDeletedMarker;
                    }
                    editor->markerAdd(change.line, marker);
                }

                if (showBlameGutter) {
                    const QVector<GitRepository::BlameLine> lines = GitRepository::blame(
                        filePath, editor->getText(editor->textLength()));
                    updateBlameMargin(editor, lines);
                }
            }
        }
    }
    if (!showBlameGutter || !it->fileState.inRepository) {
        editor->setMarginWidthN(GitBlameMargin, 0);
    }
    emit gitStateChanged(editor);
}

GitRepository::FileState GitManager::stateForEditor(const ScintillaNext *editor) const
{
    const auto it = states.constFind(const_cast<ScintillaNext *>(editor));
    return it == states.constEnd() ? GitRepository::FileState() : it->fileState;
}

bool GitManager::stageFile(ScintillaNext *editor, QString *error)
{
    if (!gitInitialized) {
        if (error) {
            *error = QStringLiteral("Git integration is unavailable");
        }
        return false;
    }
    if (!editor || !editor->isFile()) {
        if (error) {
            *error = QStringLiteral("The current editor is not a file");
        }
        return false;
    }
    const bool result = GitRepository::stage(editor->getFilePath(), error);
    if (result) {
        refreshNow(editor);
    }
    return result;
}

bool GitManager::unstageFile(ScintillaNext *editor, QString *error)
{
    if (!gitInitialized) {
        if (error) {
            *error = QStringLiteral("Git integration is unavailable");
        }
        return false;
    }
    if (!editor || !editor->isFile()) {
        if (error) {
            *error = QStringLiteral("The current editor is not a file");
        }
        return false;
    }
    const bool result = GitRepository::unstage(editor->getFilePath(), error);
    if (result) {
        refreshNow(editor);
    }
    return result;
}

void GitManager::setBlameGutterVisible(bool visible)
{
    if (showBlameGutter == visible) {
        return;
    }
    showBlameGutter = visible;
    const QList<ScintillaNext *> editors = states.keys();
    for (ScintillaNext *editor : editors) {
        refreshNow(editor);
    }
}
