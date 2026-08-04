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

#include "CommandPaletteDialog.h"
#include "FindInFilesDock.h"
#include "TerminalDock.h"

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTest>
#include <QTreeWidget>

class AccessibilityTests final : public QObject
{
    Q_OBJECT

private slots:
    void commandPaletteExposesNamesAndKeyboardNavigation();
    void searchDockExposesNamedControls();
    void terminalDockExposesNamedControls();
};

void AccessibilityTests::commandPaletteExposesNamesAndKeyboardNavigation()
{
    bool triggered = false;
    QAction action(QStringLiteral("Open Document"));
    QObject::connect(&action, &QAction::triggered, [&triggered]() { triggered = true; });

    CommandPaletteDialog dialog(nullptr, {&action});
    auto *query = dialog.findChild<QLineEdit *>(QStringLiteral("commandPaletteQuery"));
    auto *results = dialog.findChild<QListWidget *>(QStringLiteral("commandPaletteResults"));
    QVERIFY(query);
    QVERIFY(results);
    QVERIFY(!query->accessibleName().isEmpty());
    QVERIFY(!query->accessibleDescription().isEmpty());
    QVERIFY(!results->accessibleName().isEmpty());
    QVERIFY(!results->accessibleDescription().isEmpty());

    dialog.show();
    query->setFocus();
    QApplication::processEvents();
    QVERIFY(query->hasFocus());
    QTest::keyClick(query, Qt::Key_Down);
    QVERIFY(results->hasFocus());
    QTest::keyClick(results, Qt::Key_Return);
    QVERIFY(triggered);
}

void AccessibilityTests::searchDockExposesNamedControls()
{
    FindInFilesDock dock;
    auto *pattern = dock.findChild<QLineEdit *>(QStringLiteral("findInFilesPattern"));
    auto *path = dock.findChild<QLineEdit *>(QStringLiteral("findInFilesPath"));
    auto *results = dock.findChild<QTreeWidget *>(QStringLiteral("findInFilesResults"));
    auto *search = dock.findChild<QPushButton *>(QStringLiteral("findInFilesSearch"));
    QVERIFY(pattern);
    QVERIFY(path);
    QVERIFY(results);
    QVERIFY(search);
    QVERIFY(!pattern->accessibleName().isEmpty());
    QVERIFY(!pattern->accessibleDescription().isEmpty());
    QVERIFY(!path->accessibleName().isEmpty());
    QVERIFY(!results->accessibleName().isEmpty());
    QVERIFY(!results->accessibleDescription().isEmpty());
    QVERIFY(!search->accessibleDescription().isEmpty());
}

void AccessibilityTests::terminalDockExposesNamedControls()
{
    TerminalDock dock;
    auto *output = dock.findChild<QPlainTextEdit *>(QStringLiteral("terminalOutput"));
    auto *input = dock.findChild<QLineEdit *>(QStringLiteral("terminalInput"));
    auto *clear = dock.findChild<QPushButton *>(QStringLiteral("terminalClearButton"));
    QVERIFY(output);
    QVERIFY(input);
    QVERIFY(clear);
    QVERIFY(!output->accessibleName().isEmpty());
    QVERIFY(!output->accessibleDescription().isEmpty());
    QVERIFY(!input->accessibleName().isEmpty());
    QVERIFY(!input->accessibleDescription().isEmpty());
    QVERIFY(!clear->accessibleDescription().isEmpty());
}

QTEST_MAIN(AccessibilityTests)
#include "AccessibilityTests.moc"
