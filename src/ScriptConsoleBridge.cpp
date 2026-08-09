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

#include "ScriptConsoleBridge.h"

#include "CapabilityTrust.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

ScriptConsoleBridge::ScriptConsoleBridge(MainWindow *window, CapabilityTrust::Manager *trustManager, QObject *parent)
    : QObject(parent)
    , window(window)
    , trustManager(trustManager)
{
}

QString ScriptConsoleBridge::workspaceRoot() const
{
    ScintillaNext *editor = currentEditor();
    return CapabilityTrust::Manager::workspaceRootForPath(
        editor && editor->isFile() ? editor->getFilePath() : QString());
}

bool ScriptConsoleBridge::authorize(CapabilityTrust::Capability capability, const QString &operation)
{
    return !trustManager || trustManager->authorize(window, workspaceRoot(), capability, operation);
}

ScintillaNext *ScriptConsoleBridge::currentEditor() const
{
    return window ? window->currentEditor() : nullptr;
}

QString ScriptConsoleBridge::text() const
{
    ScintillaNext *editor = currentEditor();
    return editor ? QString::fromUtf8(editor->getText(editor->textLength())) : QString();
}

void ScriptConsoleBridge::setText(const QString &value)
{
    if (!authorize(CapabilityTrust::Capability::ScriptDocumentEdit, QStringLiteral("javascript.document-edit"))) {
        emit outputMessage(tr("Script document editing is blocked by workspace trust."));
        return;
    }
    if (ScintillaNext *editor = currentEditor()) {
        const QByteArray utf8 = value.toUtf8();
        editor->setText(utf8.constData());
    }
}

QString ScriptConsoleBridge::selectedText() const
{
    ScintillaNext *editor = currentEditor();
    return editor ? QString::fromUtf8(editor->getSelText()) : QString();
}

void ScriptConsoleBridge::replaceSelection(const QString &value)
{
    if (!authorize(CapabilityTrust::Capability::ScriptDocumentEdit, QStringLiteral("javascript.document-edit"))) {
        emit outputMessage(tr("Script document editing is blocked by workspace trust."));
        return;
    }
    if (ScintillaNext *editor = currentEditor()) {
        const QByteArray utf8 = value.toUtf8();
        editor->replaceSel(utf8.constData());
    }
}

void ScriptConsoleBridge::insertText(const QString &value)
{
    if (!authorize(CapabilityTrust::Capability::ScriptDocumentEdit, QStringLiteral("javascript.document-edit"))) {
        emit outputMessage(tr("Script document editing is blocked by workspace trust."));
        return;
    }
    if (ScintillaNext *editor = currentEditor()) {
        const QByteArray utf8 = value.toUtf8();
        editor->insertText(editor->currentPos(), utf8.constData());
    }
}

QString ScriptConsoleBridge::filePath() const
{
    ScintillaNext *editor = currentEditor();
    return editor && editor->isFile() ? editor->getFilePath() : QString();
}

bool ScriptConsoleBridge::save()
{
    ScintillaNext *editor = currentEditor();
    if (!editor || !authorize(CapabilityTrust::Capability::ScriptDocumentSave,
                              QStringLiteral("javascript.document-save"))) {
        if (editor) {
            emit outputMessage(tr("Script document saving is blocked by workspace trust."));
        }
        return false;
    }

    const bool saved = window && window->saveFile(editor);
    if (trustManager) {
        trustManager->record(workspaceRoot(), CapabilityTrust::Capability::ScriptDocumentSave,
                             QStringLiteral("javascript.document-save"), saved ? QStringLiteral("saved") : QStringLiteral("failed"));
    }
    return saved;
}

bool ScriptConsoleBridge::openFile(const QString &path)
{
    if (!window || path.isEmpty()) {
        return false;
    }

    const QString targetRoot = CapabilityTrust::Manager::workspaceRootForPath(path);
    if (trustManager && !trustManager->authorize(window, targetRoot,
                                                 CapabilityTrust::Capability::ScriptDocumentOpen,
                                                 QStringLiteral("javascript.document-open"))) {
        emit outputMessage(tr("Opening a document from JavaScript is blocked by workspace trust."));
        return false;
    }

    window->openFile(path);
    if (trustManager) {
        trustManager->record(targetRoot, CapabilityTrust::Capability::ScriptDocumentOpen,
                             QStringLiteral("javascript.document-open"), QStringLiteral("opened"));
    }
    return true;
}

void ScriptConsoleBridge::log(const QString &value)
{
    emit outputMessage(value);
}
