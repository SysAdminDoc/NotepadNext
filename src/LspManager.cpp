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

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include "Scintilla.h"

namespace
{
constexpr int LspErrorIndicatorColor = 0x0000FF;
constexpr int LspWarningIndicatorColor = 0x0080FF;
constexpr int LspInfoIndicatorColor = 0xFF8000;

const QStringList WorkspaceMarkers = {
    QStringLiteral(".git"),
    QStringLiteral("compile_commands.json"),
    QStringLiteral("CMakeLists.txt"),
    QStringLiteral("Cargo.toml"),
    QStringLiteral("go.mod"),
    QStringLiteral("package.json"),
    QStringLiteral("pyproject.toml"),
    QStringLiteral("setup.cfg"),
};

QString workspaceRootForPath(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir directory(fileInfo.absolutePath());
    while (directory.exists()) {
        for (const QString &marker : WorkspaceMarkers) {
            if (QFileInfo::exists(directory.filePath(marker))) {
                return QDir(directory.absolutePath()).canonicalPath().isEmpty()
                    ? directory.absolutePath()
                    : QDir(directory.absolutePath()).canonicalPath();
            }
        }

        const QDir parent = directory;
        if (!directory.cdUp() || directory.absolutePath() == parent.absolutePath()) {
            break;
        }
    }
    return fileInfo.absolutePath();
}

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
    connect(editor, &ScintillaNext::largeFileModeChanged, this, [this, editor]() {
        configureEditor(editor);
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

    const QString workspaceKey = it->workspaceKey;
    LspClient *client = it->client;
    if (client && !it->uri.isEmpty()) {
        client->closeDocument(it->uri);
    }
    states.erase(it);
    releaseClient(workspaceKey, client);
}

void LspManager::configureEditor(ScintillaNext *editor)
{
    auto it = states.find(editor);
    if (it == states.end()) {
        return;
    }

    EditorState &state = it.value();
    clearDiagnostics(editor);

    const QString previousWorkspaceKey = state.workspaceKey;
    LspClient *previousClient = state.client;
    if (previousClient && !state.uri.isEmpty()) {
        previousClient->closeDocument(state.uri);
    }
    state.client = nullptr;
    state.workspaceKey.clear();
    state.uri.clear();
    state.version = 1;
    releaseClient(previousWorkspaceKey, previousClient);

    if (editor->isLargeFileMode()) {
        qInfo("LSP is disabled for %s in large-file safety mode", qUtf8Printable(editor->getName()));
        emit statusChanged(editor, tr("LSP disabled in large-file safety mode"));
        return;
    }

    QString languageId;
    QString serverCommand;
    QStringList serverArguments;
    if (!LspClient::configurationForLanguage(editor->languageName,
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

    const QString rootPath = workspaceRootForEditor(editor);
    state.uri = editorUri(editor);
    if (!editor->isFile()) {
        emit statusChanged(editor, tr("LSP is using an untitled document URI until this buffer is saved."));
    }
    const QString workspaceKey = rootPath + QChar('\n') + executable + QChar('\n') + serverArguments.join(QChar('\n'));
    LspClient *client = workspaceClients.value(workspaceKey);
    if (!client) {
        client = new LspClient(executable, serverArguments, this);
        workspaceClients.insert(workspaceKey, client);
        connectClient(client, workspaceKey);
    }

    state.client = client;
    state.workspaceKey = workspaceKey;
    if (!client->start(rootPath)) {
        state.client = nullptr;
        state.workspaceKey.clear();
        state.uri.clear();
        releaseClient(workspaceKey, client);
        emit statusChanged(editor, tr("Unable to start the LSP server"));
        return;
    }
    client->openDocument(state.uri, languageId, editorText(editor), state.version, rootPath);
}

void LspManager::connectClient(LspClient *client, const QString &workspaceKey)
{
    connect(client, &LspClient::diagnosticsReady, this,
            [this, client](const QString &uri, int documentVersion,
                           const QVector<LspDiagnostic> &diagnostics) {
        showDiagnostics(client, uri, documentVersion, diagnostics);
    });
    connect(client, &LspClient::hoverReady, this,
            [this, client](const QString &uri, int documentVersion,
                           const LspPosition &position, const QString &text) {
        showHover(client, uri, documentVersion, position, text);
    });
    connect(client, &LspClient::definitionReady, this,
            [this, client](const QString &requestUri, int documentVersion,
                           const QString &targetUri, const LspRange &range) {
        goToDefinition(client, requestUri, documentVersion, targetUri, range);
    });
    connect(client, &LspClient::serverError, this, [this, client](const QString &message) {
        reportClientStatus(client, message);
    });
    connect(client, &LspClient::statusChanged, this, [this, client](const QString &message) {
        reportClientStatus(client, message);
    });
    connect(client, &LspClient::stopped, this, [this, client, workspaceKey](int) {
        reportClientStatus(client, tr("LSP server stopped"));
        if (client->documentCount() == 0) {
            releaseClient(workspaceKey, client);
        }
    });
}

void LspManager::releaseClient(const QString &workspaceKey, LspClient *client)
{
    if (!client || client->documentCount() > 0) {
        return;
    }

    if (workspaceClients.value(workspaceKey) != client) {
        return;
    }

    workspaceClients.remove(workspaceKey);
    client->stop();
    client->deleteLater();
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

    it->client->requestHover(it->uri, toLspPosition(editor, position), it->version);
}

void LspManager::requestDefinition(ScintillaNext *editor)
{
    const auto it = states.constFind(editor);
    if (!editor || it == states.constEnd() || !it->client || it->uri.isEmpty()) {
        return;
    }

    it->client->requestDefinition(it->uri, toLspPosition(editor, static_cast<int>(editor->currentPos())), it->version);
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

void LspManager::showDiagnostics(LspClient *client, const QString &uri, int documentVersion,
                                 const QVector<LspDiagnostic> &diagnostics)
{
    ScintillaNext *editor = editorForDocument(client, uri);
    if (!editor) {
        return;
    }

    const auto it = states.constFind(editor);
    if (it == states.constEnd() || (documentVersion >= 0 && documentVersion != it->version) ||
        (documentVersion < 0 && it->version > 1)) {
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

void LspManager::showHover(LspClient *client, const QString &uri, int documentVersion,
                           const LspPosition &position, const QString &text)
{
    ScintillaNext *editor = editorForDocument(client, uri);
    if (!editor) {
        return;
    }

    const auto it = states.constFind(editor);
    if (it == states.constEnd() || (documentVersion >= 0 && documentVersion != it->version) ||
        text.trimmed().isEmpty()) {
        return;
    }

    const int editorPosition = toScintillaPosition(editor, position);
    const QByteArray textBytes = text.left(8192).toUtf8();
    editor->callTipShow(editorPosition, textBytes.constData());
}

void LspManager::goToDefinition(LspClient *client, const QString &requestUri, int documentVersion,
                                const QString &uri, const LspRange &range)
{
    ScintillaNext *sourceEditor = editorForDocument(client, requestUri);
    if (!sourceEditor) {
        return;
    }

    const auto sourceIt = states.constFind(sourceEditor);
    if (sourceIt == states.constEnd() ||
        (documentVersion >= 0 && documentVersion != sourceIt->version) || uri.isEmpty()) {
        return;
    }

    ScintillaNext *targetEditor = nullptr;
    if (uri == sourceIt->uri) {
        targetEditor = sourceEditor;
    }
    else {
        const QUrl fileUrl(uri);
        if (!fileUrl.isLocalFile()) {
            emit statusChanged(sourceEditor, tr("LSP returned a non-local definition that cannot be opened."));
            return;
        }
        const QString path = fileUrl.toLocalFile();
        targetEditor = editorManager->getEditorByFilePath(path);
        if (!targetEditor) {
            QString error;
            targetEditor = editorManager->createEditorFromFile(path, false, &error);
            if (!targetEditor && !error.isEmpty()) {
                emit statusChanged(sourceEditor, tr("Unable to open LSP definition: %1").arg(error));
            }
        }
    }

    if (!targetEditor) {
        return;
    }

    const int start = toScintillaPosition(targetEditor, range.start);
    const int end = toScintillaPosition(targetEditor, range.end);
    targetEditor->goToRange({start, qMax(start, end)});
}

void LspManager::reportClientStatus(LspClient *client, const QString &message)
{
    if (!client || message.trimmed().isEmpty()) {
        return;
    }

    const QList<ScintillaNext *> editors = states.keys();
    for (ScintillaNext *editor : editors) {
        const auto it = states.constFind(editor);
        if (it != states.constEnd() && it->client == client) {
            emit statusChanged(editor, message);
        }
    }
}

ScintillaNext *LspManager::editorForDocument(LspClient *client, const QString &uri) const
{
    if (!client || uri.isEmpty()) {
        return nullptr;
    }

    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        if (it->client == client && it->uri == uri) {
            return it.key();
        }
    }
    return nullptr;
}

QString LspManager::workspaceRootForEditor(ScintillaNext *editor) const
{
    if (!editor) {
        return QDir::currentPath();
    }

    if (editor->isFile()) {
        return workspaceRootForPath(editor->getFilePath());
    }

    return QDir::currentPath();
}

QByteArray LspManager::editorText(ScintillaNext *editor)
{
    return editor ? editor->getText(editor->textLength()) : QByteArray();
}

QString LspManager::editorUri(ScintillaNext *editor)
{
    if (!editor) {
        return QString();
    }
    if (editor->isFile()) {
        return QUrl::fromLocalFile(QFileInfo(editor->getFilePath()).absoluteFilePath()).toString(QUrl::FullyEncoded);
    }

    const QString name = editor->getName().isEmpty() ? QStringLiteral("buffer") : editor->getName();
    return QStringLiteral("untitled:%1").arg(QString::fromLatin1(QUrl::toPercentEncoding(name)));
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
