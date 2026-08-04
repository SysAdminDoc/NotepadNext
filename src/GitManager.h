/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GITMANAGER_H
#define GITMANAGER_H

#include "GitRepository.h"

#include <QHash>
#include <QSet>
#include <QTimer>


class EditorManager;
class ScintillaNext;


class GitManager final : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(EditorManager *editorManager, QObject *parent = nullptr);
    ~GitManager() override;

    GitRepository::FileState stateForEditor(const ScintillaNext *editor) const;
    bool stageFile(ScintillaNext *editor, QString *error = nullptr);
    bool unstageFile(ScintillaNext *editor, QString *error = nullptr);

    bool blameGutterVisible() const { return showBlameGutter; }
    void setBlameGutterVisible(bool visible);
    void refreshNow(ScintillaNext *editor);

signals:
    void gitStateChanged(ScintillaNext *editor);

private:
    struct EditorState
    {
        GitRepository::FileState fileState;
    };

    void attachEditor(ScintillaNext *editor);
    void detachEditor(ScintillaNext *editor);
    void scheduleRefresh(ScintillaNext *editor);
    void refreshPendingEditors();
    void configureEditor(ScintillaNext *editor);
    void clearDecorations(ScintillaNext *editor);
    void updateBlameMargin(ScintillaNext *editor, const QVector<GitRepository::BlameLine> &lines);

    EditorManager *editorManager = nullptr;
    QHash<ScintillaNext *, EditorState> states;
    QSet<ScintillaNext *> pendingEditors;
    QTimer refreshTimer;
    bool showBlameGutter = false;
    bool gitInitialized = false;
};

#endif // GITMANAGER_H
