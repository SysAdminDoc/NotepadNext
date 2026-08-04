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

#include "MainWindow.h"
#include "ScintillaNext.h"

ScriptConsoleBridge::ScriptConsoleBridge(MainWindow *window, QObject *parent)
    : QObject(parent)
    , window(window)
{
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
    if (ScintillaNext *editor = currentEditor()) {
        const QByteArray utf8 = value.toUtf8();
        editor->replaceSel(utf8.constData());
    }
}

void ScriptConsoleBridge::insertText(const QString &value)
{
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
    return window && editor && window->saveFile(editor);
}

void ScriptConsoleBridge::openFile(const QString &path)
{
    if (window && !path.isEmpty()) {
        window->openFile(path);
    }
}

void ScriptConsoleBridge::log(const QString &value)
{
    emit outputMessage(value);
}
