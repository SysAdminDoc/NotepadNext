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

#include <QObject>
#include <QString>

class MainWindow;
class ScintillaNext;

class ScriptConsoleBridge : public QObject
{
    Q_OBJECT

public:
    explicit ScriptConsoleBridge(MainWindow *window, QObject *parent = nullptr);

    ScintillaNext *currentEditor() const;

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void setText(const QString &value);
    Q_INVOKABLE QString selectedText() const;
    Q_INVOKABLE void replaceSelection(const QString &value);
    Q_INVOKABLE void insertText(const QString &value);
    Q_INVOKABLE QString filePath() const;
    Q_INVOKABLE bool save();
    Q_INVOKABLE void openFile(const QString &path);

public slots:
    void log(const QString &value);

signals:
    void outputMessage(const QString &value);

private:
    MainWindow *window;
};
