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

#ifndef REGEXBUILDERDOCK_H
#define REGEXBUILDERDOCK_H

#include <QDockWidget>

#include "RegexBuilder.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;
class MainWindow;

class RegexBuilderDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit RegexBuilderDock(MainWindow *window, QWidget *parent = nullptr);

    QString pattern() const;
    QString sampleText() const;
    int matchCount() const;
    const RegexBuilder::Result &lastResult() const;

public slots:
    void setPattern(const QString &pattern);
    void setSampleText(const QString &sample);
    void focusPattern();
    void analyzePattern();
    void loadCurrentDocument();
    void loadCurrentSelection();

private:
    void loadEditorText(bool selectionOnly);
    void updateStatus(const QString &message, bool error = false);
    void updateMatchTree();
    void updateHighlights();

    MainWindow *window = nullptr;
    QLineEdit *patternEdit = nullptr;
    QPlainTextEdit *sampleEdit = nullptr;
    QCheckBox *caseInsensitiveCheck = nullptr;
    QCheckBox *dotMatchesCheck = nullptr;
    QLabel *statusLabel = nullptr;
    QTreeWidget *matches = nullptr;
    RegexBuilder::Result result;
};

#endif // REGEXBUILDERDOCK_H
