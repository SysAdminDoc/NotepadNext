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

#include "SnippetEngine.h"

#include <QtTest>

class SnippetEngineTests final : public QObject
{
    Q_OBJECT

private slots:
    void expandsDefaultsAndSortsPlaceholders();
    void supportsShortAndFinalTabstops();
    void preservesLiteralDollarsAndUnicodeOffsets();
};

void SnippetEngineTests::expandsDefaultsAndSortsPlaceholders()
{
    const SnippetEngine::Expansion expansion = SnippetEngine::expand(
        QStringLiteral("if (${2:condition}) { ${1:body} } ${0}"));

    QCOMPARE(expansion.text, QStringLiteral("if (condition) { body } "));
    QCOMPARE(expansion.placeholders.size(), 3);
    QCOMPARE(expansion.placeholders.at(0).number, 1);
    QCOMPARE(expansion.placeholders.at(0).start, 17);
    QCOMPARE(expansion.placeholders.at(0).length, 4);
    QCOMPARE(expansion.placeholders.at(0).defaultText, QStringLiteral("body"));
    QCOMPARE(expansion.placeholders.at(1).number, 2);
    QCOMPARE(expansion.placeholders.at(1).start, 4);
    QCOMPARE(expansion.placeholders.at(1).length, 9);
    QCOMPARE(expansion.placeholders.at(2).number, 0);
    QCOMPARE(expansion.placeholders.at(2).start, expansion.text.toUtf8().size());
    QCOMPARE(expansion.placeholders.at(2).length, 0);
}

void SnippetEngineTests::supportsShortAndFinalTabstops()
{
    const SnippetEngine::Expansion expansion = SnippetEngine::expand(
        QStringLiteral("$2/$1/$1/${0}"));

    QCOMPARE(expansion.text, QStringLiteral("///"));
    QCOMPARE(expansion.placeholders.size(), 4);
    QCOMPARE(expansion.placeholders.at(0).number, 1);
    QCOMPARE(expansion.placeholders.at(0).start, 1);
    QCOMPARE(expansion.placeholders.at(1).number, 1);
    QCOMPARE(expansion.placeholders.at(1).start, 2);
    QCOMPARE(expansion.placeholders.at(2).number, 2);
    QCOMPARE(expansion.placeholders.at(2).start, 0);
    QCOMPARE(expansion.placeholders.at(3).number, 0);
    QCOMPARE(expansion.placeholders.at(3).start, 3);
}

void SnippetEngineTests::preservesLiteralDollarsAndUnicodeOffsets()
{
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    const SnippetEngine::Expansion expansion = SnippetEngine::expand(
        QStringLiteral("cost $$${1:") + emoji + QStringLiteral("} ${3:} ${bad} $") );

    QCOMPARE(expansion.text, QStringLiteral("cost $") + emoji + QStringLiteral("  ${bad} $"));
    QCOMPARE(expansion.placeholders.size(), 2);
    QCOMPARE(expansion.placeholders.at(0).number, 1);
    QCOMPARE(expansion.placeholders.at(0).start, QStringLiteral("cost $").toUtf8().size());
    QCOMPARE(expansion.placeholders.at(0).length, emoji.toUtf8().size());
    QCOMPARE(expansion.placeholders.at(1).number, 3);
    QCOMPARE(expansion.placeholders.at(1).start, (QStringLiteral("cost $") + emoji + QStringLiteral(" ")).toUtf8().size());
    QCOMPARE(expansion.placeholders.at(1).length, 0);
}

QTEST_APPLESS_MAIN(SnippetEngineTests)
#include "SnippetEngineTests.moc"
