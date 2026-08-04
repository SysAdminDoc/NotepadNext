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

#pragma once

#include <QDockWidget>
#include <QElapsedTimer>

class QPlainTextEdit;
class ScriptConsoleBridge;
class NotepadNextApplication;
struct JSContext;
struct JSRuntime;

class ScriptConsoleDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ScriptConsoleDock(NotepadNextApplication *app, QWidget *parent = nullptr);
    ~ScriptConsoleDock() override;

    static QString languageDescription();

public slots:
    void runScript();
    void runScriptFile();
    void clearOutput();
    void focusInput();

private:
    static int interruptHandler(JSRuntime *runtime, void *opaque);
    void executeScript(const QString &script, const QString &sourceName = QStringLiteral("Console"));
    void appendOutput(const QString &text);

    NotepadNextApplication *app;
    ScriptConsoleBridge *bridge;
    QPlainTextEdit *output;
    QPlainTextEdit *input;
    JSRuntime *javascriptRuntime = nullptr;
    JSContext *javascriptContext = nullptr;
    QElapsedTimer executionTimer;
};
