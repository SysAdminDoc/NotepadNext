/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LspManager.h"

#include "EditorManager.h"
#include "ScintillaNext.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include "Scintilla.h"

namespace
{
constexpr int LspErrorIndicatorColor = 0x0000FF;
constexpr int LspWarningIndicatorColor = 0x0080FF;
constexpr int LspInfoIndicatorColor = 0xFF8000;

void configureIndicator(ScintillaNext *editor, int indicator, int color)
{
    if (indicator < 0) {
        return;
    }
    editor->indicSetStyle(indicator, INDIC_SQUIGGLE);
    editor->indicSetFore(indicator, color);
    editor->indicSetUnder(indicator, true);
    editor->indicSetAlpha(indicator, 180);
}

}

LspManager::LspManager(EditorManager *editorManager, QObject *parent)
    : QObject(parent), editorManager(editorManager)
{
    Q_ASSERT(editorManager != nullptr);

    connect(editorManager, &EditorManager::editorCreated, this, &LspManager::attachEditor);
    connect(editorManager, &EditorManager::editorClosed, this, &LspManager::detachEditor);
}

LspManager::~LspManager()
{
    const QList<ScintillaNext *> editors = states.keys();
    for (ScintillaNext *editor : editors) {
        detachEditor(editor);
    }
}

void LspManager::attachEditor(ScintillaNext *editor)
{
    if (!editor || states.contains(editor)) {
        return;
    }

    EditorState state;
    state.errorIndicator = editor->allocateIndicator(QStringLiteral("lsp-error"));
    state.warningIndicator = editor->allocateIndicator(QStringLiteral("lsp-warning"));
    state.infoIndicator = editor->allocateIndicator(QStringLiteral("lsp-info"));
    configureIndicator(editor, state.errorIndicator, LspErrorIndicatorColor);
    configureIndicator(editor, state.warningIndicator, LspWarningIndicatorColor);
    configureIndicator(editor, state.infoIndicator, LspInfoIndicatorColor);
    states.insert(editor, state);

    editor->setModEventMask(editor->modEventMask() | SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
    editor->setMouseDwellTime(700);

    connect(editor, &ScintillaNext::notify, this, [this, editor](Scintilla::NotificationData *notification) {
        if (notification && notification->nmhdr.code == Scintilla::Notification::Modified &&
            (Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::InsertText) ||
             Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::DeleteText))) {
            editorModified(editor);
        }
    });
    connect(editor, &ScintillaNext::lexerChanged, this, [this, editor]() {
        configureEditor(editor);
    });
    connect(editor, &ScintillaNext::renamed, this, [this, editor]() {
        configureEditor(editor);
    });
    connect(editor, &ScintillaNext::saved, this, [this, editor]() {
        editorSaved(editor);
    });
    connect(editor, &ScintillaNext::reloaded, this, [this, editor]() {
        editorModified(editor);
    });
    connect(editor, &ScintillaNext::dwellStart, this, [this, editor](int x, int y) {
        editorDwelled(editor, x, y);
    });

    // File editors have normally not had their language selected when the editorCreated signal
    // is delivered. The lexerChanged connection above handles the normal path; this queued pass
    // also covers callers that create and configure an editor in the same event turn.
    QMetaObject::invokeMethod(this, [this, editor]() {
        if (states.contains(editor)) {
            configureEditor(editor);
        }
    }, Qt::QueuedConnection);
}

void LspManager::detachEditor(ScintillaNext *editor)
{
    if (!editor) {
        return;
    }

    auto it = states.find(editor);
    if (it == states.end()) {
        return;
    }

    if (it->client) {
        LspClient *client = it->client;
        client->stop();
        delete client;
    }
    states.erase(it);
}

void LspManager::configureEditor(ScintillaNext *editor)
{
    auto it = states.find(editor);
    if (it == states.end()) {
        return;
    }

    EditorState &state = it.value();
    clearDiagnostics(editor);

    if (state.client) {
        LspClient *client = state.client;
        client->stop();
        delete client;
        state.client = nullptr;
    }
    state.uri.clear();
    state.version = 1;

    QString languageId;
    QString serverCommand;
    QStringList serverArguments;
    if (!editor->isFile() || !LspClient::configurationForLanguage(editor->languageName,
                                                                  &languageId,
                                                                  &serverCommand,
                                                                  &serverArguments)) {
        return;
    }

    const QString executable = QStandardPaths::findExecutable(serverCommand);
    if (executable.isEmpty()) {
        qInfo("LSP server %s is not installed; language services are disabled for %s",
              qUtf8Printable(serverCommand), qUtf8Printable(editor->languageName));
        return;
    }

    const QFileInfo fileInfo(editor->getFilePath());
    if (fileInfo.absoluteFilePath().isEmpty()) {
        return;
    }

    state.uri = editorUri(editor);
    LspClient *client = new LspClient(executable, serverArguments, this);
    state.client = client;

    connect(client, &LspClient::diagnosticsReady, this,
            [this, editor](const QString &uri, const QVector<LspDiagnostic> &diagnostics) {
        showDiagnostics(editor, uri, diagnostics);
    });
    connect(client, &LspClient::hoverReady, this,
            [this, editor](const QString &uri, const LspPosition &position, const QString &text) {
        showHover(editor, uri, position, text);
    });
    connect(client, &LspClient::definitionReady, this,
            [this, editor](const QString &uri, const LspRange &range) {
        goToDefinition(editor, uri, range);
    });
    connect(client, &LspClient::serverError, this, [editor](const QString &message) {
        qWarning("LSP server for %s: %s", qUtf8Printable(editor->languageName), qUtf8Printable(message));
    });

    const QString rootPath = fileInfo.absolutePath();
    if (!client->start(rootPath)) {
        delete client;
        state.client = nullptr;
        state.uri.clear();
        return;
    }
    client->openDocument(state.uri, languageId, editorText(editor), state.version, rootPath);
}

void LspManager::editorModified(ScintillaNext *editor)
{
    auto it = states.find(editor);
    if (it == states.end()) {
        return;
    }

    EditorState &state = it.value();
    clearDiagnostics(editor);
    if (state.client && !state.uri.isEmpty()) {
        state.client->changeDocument(state.uri, editorText(editor), ++state.version);
    }
}

void LspManager::editorSaved(ScintillaNext *editor)
{
    const auto it = states.constFind(editor);
    if (it != states.constEnd() && it->client && !it->uri.isEmpty()) {
        it->client->saveDocument(it->uri);
    }
}

void LspManager::editorDwelled(ScintillaNext *editor, int x, int y)
{
    const int position = static_cast<int>(editor->positionFromPoint(x, y));
    if (position >= 0) {
        requestHover(editor, position);
    }
}

void LspManager::requestHover(ScintillaNext *editor, int position)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd() || !it->client || it->uri.isEmpty()) {
        return;
    }

    it->client->requestHover(it->uri, toLspPosition(editor, position));
}

void LspManager::requestDefinition(ScintillaNext *editor)
{
    const auto it = states.constFind(editor);
    if (!editor || it == states.constEnd() || !it->client || it->uri.isEmpty()) {
        return;
    }

    it->client->requestDefinition(it->uri, toLspPosition(editor, static_cast<int>(editor->currentPos())));
}

void LspManager::clearDiagnostics(ScintillaNext *editor)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd()) {
        return;
    }

    const int length = static_cast<int>(editor->length());
    for (const int indicator : {it->errorIndicator, it->warningIndicator, it->infoIndicator}) {
        if (indicator >= 0) {
            editor->setIndicatorCurrent(indicator);
            editor->indicatorClearRange(0, length);
        }
    }
}

void LspManager::showDiagnostics(ScintillaNext *editor, const QString &uri,
                                 const QVector<LspDiagnostic> &diagnostics)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd() || uri != it->uri) {
        return;
    }

    clearDiagnostics(editor);
    const int documentLength = static_cast<int>(editor->length());
    for (const LspDiagnostic &diagnostic : diagnostics) {
        const int start = qBound(0, toScintillaPosition(editor, diagnostic.range.start), documentLength);
        int end = qBound(start, toScintillaPosition(editor, diagnostic.range.end), documentLength);
        if (end == start && start < documentLength) {
            end = start + 1;
        }
        if (end <= start) {
            continue;
        }

        const int indicator = diagnostic.severity <= 1
            ? it->errorIndicator
            : (diagnostic.severity == 2 ? it->warningIndicator : it->infoIndicator);
        if (indicator >= 0) {
            editor->setIndicatorCurrent(indicator);
            editor->indicatorFillRange(start, end - start);
        }
    }
}

void LspManager::showHover(ScintillaNext *editor, const QString &uri,
                           const LspPosition &position, const QString &text)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd() || uri != it->uri || text.trimmed().isEmpty()) {
        return;
    }

    const int editorPosition = toScintillaPosition(editor, position);
    const QByteArray textBytes = text.left(8192).toUtf8();
    editor->callTipShow(editorPosition, textBytes.constData());
}

void LspManager::goToDefinition(ScintillaNext *sourceEditor, const QString &uri, const LspRange &range)
{
    const auto sourceIt = states.constFind(sourceEditor);
    if (sourceIt == states.constEnd() || uri.isEmpty()) {
        return;
    }

    ScintillaNext *targetEditor = nullptr;
    if (uri == sourceIt->uri) {
        targetEditor = sourceEditor;
    }
    else {
        const QUrl fileUrl(uri);
        if (!fileUrl.isLocalFile()) {
            return;
        }
        const QString path = fileUrl.toLocalFile();
        targetEditor = editorManager->getEditorByFilePath(path);
        if (!targetEditor) {
            targetEditor = editorManager->createEditorFromFile(path);
        }
    }

    if (!targetEditor) {
        return;
    }

    const int start = toScintillaPosition(targetEditor, range.start);
    const int end = toScintillaPosition(targetEditor, range.end);
    targetEditor->goToRange({start, qMax(start, end)});
}

QByteArray LspManager::editorText(ScintillaNext *editor)
{
    return editor ? editor->getText(editor->textLength()) : QByteArray();
}

QString LspManager::editorUri(ScintillaNext *editor)
{
    if (!editor || !editor->isFile()) {
        return QString();
    }
    return QUrl::fromLocalFile(QFileInfo(editor->getFilePath()).absoluteFilePath()).toString(QUrl::FullyEncoded);
}

LspPosition LspManager::toLspPosition(ScintillaNext *editor, int position)
{
    if (!editor) {
        return {};
    }

    const int documentLength = static_cast<int>(editor->length());
    position = qBound(0, position, documentLength);
    const int line = static_cast<int>(editor->lineFromPosition(position));
    const int lineStart = static_cast<int>(editor->positionFromLine(line));
    const int lineEnd = static_cast<int>(editor->lineEndPosition(line));
    const QByteArray prefix = editor->textRange(lineStart, qMin(position, lineEnd));

    return {line, static_cast<int>(QString::fromUtf8(prefix).size())};
}

int LspManager::toScintillaPosition(ScintillaNext *editor, const LspPosition &position)
{
    if (!editor) {
        return 0;
    }

    const int lineCount = static_cast<int>(editor->lineCount());
    const int line = qBound(0, position.line, qMax(0, lineCount - 1));
    const int lineStart = static_cast<int>(editor->positionFromLine(line));
    const int lineEnd = static_cast<int>(editor->lineEndPosition(line));
    const QString lineText = QString::fromUtf8(editor->textRange(lineStart, lineEnd));
    const int character = qBound(0, position.character, lineText.size());
    const int byteOffset = static_cast<int>(lineText.left(character).toUtf8().size());
    return qMin(lineStart + byteOffset, lineEnd);
}
