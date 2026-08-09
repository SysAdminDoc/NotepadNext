/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LSPMANAGER_H
#define LSPMANAGER_H

#include "LspClient.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class EditorManager;
class ScintillaNext;

class LspManager final : public QObject
{
    Q_OBJECT

public:
    explicit LspManager(EditorManager *editorManager, QObject *parent = nullptr);
    ~LspManager() override;

    void requestDefinition(ScintillaNext *editor);
    void requestHover(ScintillaNext *editor, int position);

private:
    struct EditorState
    {
        QPointer<LspClient> client;
        QString workspaceKey;
        QString uri;
        int version = 1;
        int errorIndicator = -1;
        int warningIndicator = -1;
        int infoIndicator = -1;
    };

    void attachEditor(ScintillaNext *editor);
    void detachEditor(ScintillaNext *editor);
    void configureEditor(ScintillaNext *editor);
    void editorModified(ScintillaNext *editor);
    void editorSaved(ScintillaNext *editor);
    void editorDwelled(ScintillaNext *editor, int x, int y);
    void clearDiagnostics(ScintillaNext *editor);
    void connectClient(LspClient *client, const QString &workspaceKey);
    void releaseClient(const QString &workspaceKey, LspClient *client);
    void showDiagnostics(LspClient *client, const QString &uri, int documentVersion,
                         const QVector<LspDiagnostic> &diagnostics);
    void showHover(LspClient *client, const QString &uri, int documentVersion,
                   const LspPosition &position, const QString &text);
    void goToDefinition(LspClient *client, const QString &requestUri, int documentVersion,
                        const QString &targetUri, const LspRange &range);
    void reportClientStatus(LspClient *client, const QString &message);
    ScintillaNext *editorForDocument(LspClient *client, const QString &uri) const;
    QString workspaceRootForEditor(ScintillaNext *editor) const;

    static QByteArray editorText(ScintillaNext *editor);
    static QString editorUri(ScintillaNext *editor);
    static LspPosition toLspPosition(ScintillaNext *editor, int position);
    static int toScintillaPosition(ScintillaNext *editor, const LspPosition &position);

    EditorManager *editorManager = nullptr;
    QHash<ScintillaNext *, EditorState> states;
    QHash<QString, QPointer<LspClient>> workspaceClients;

signals:
    void statusChanged(ScintillaNext *editor, const QString &message);
};

#endif // LSPMANAGER_H
