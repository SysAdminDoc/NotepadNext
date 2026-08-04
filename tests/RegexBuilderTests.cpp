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
 */

#include "RegexBuilder.h"

#include <QtTest>

class RegexBuilderTests final : public QObject
{
    Q_OBJECT

private slots:
    void reportsNamedAndNumberedGroups();
    void reportsUnmatchedOptionalGroups();
    void reportsInvalidPatterns();
    void appliesPatternOptions();
    void capsLargeMatchSets();
};

void RegexBuilderTests::reportsNamedAndNumberedGroups()
{
    const RegexBuilder::Result result = RegexBuilder::analyze(
        QStringLiteral("(?<key>[A-Za-z]+)=(\\d+)"),
        QStringLiteral("one=1\ntwo=22"), false, false);

    QVERIFY(result.valid);
    QCOMPARE(result.captureCount, 2);
    QCOMPARE(result.matches.size(), 2);
    QCOMPARE(result.matches.at(0).groups.size(), 3);
    QCOMPARE(result.matches.at(0).groups.at(0).value, QStringLiteral("one=1"));
    QCOMPARE(result.matches.at(0).groups.at(1).name, QStringLiteral("key"));
    QCOMPARE(result.matches.at(0).groups.at(1).value, QStringLiteral("one"));
    QCOMPARE(result.matches.at(0).groups.at(2).value, QStringLiteral("1"));
    QCOMPARE(result.matches.at(1).start, 6);
}

void RegexBuilderTests::reportsUnmatchedOptionalGroups()
{
    const RegexBuilder::Result result = RegexBuilder::analyze(
        QStringLiteral("(a)(b)?"), QStringLiteral("a"), false, false);

    QVERIFY(result.valid);
    QCOMPARE(result.matches.size(), 1);
    QVERIFY(result.matches.at(0).groups.at(1).matched());
    QVERIFY(!result.matches.at(0).groups.at(2).matched());
    QCOMPARE(result.matches.at(0).groups.at(2).start, -1);
}

void RegexBuilderTests::reportsInvalidPatterns()
{
    const RegexBuilder::Result result = RegexBuilder::analyze(
        QStringLiteral("("), QStringLiteral("sample"), false, false);

    QVERIFY(!result.valid);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.errorOffset >= 0);
    QCOMPARE(result.matches.size(), 0);
}

void RegexBuilderTests::appliesPatternOptions()
{
    const RegexBuilder::Result caseSensitive = RegexBuilder::analyze(
        QStringLiteral("foo"), QStringLiteral("FOO"), false, false);
    QCOMPARE(caseSensitive.matches.size(), 0);

    const RegexBuilder::Result caseInsensitive = RegexBuilder::analyze(
        QStringLiteral("foo"), QStringLiteral("FOO"), true, false);
    QCOMPARE(caseInsensitive.matches.size(), 1);

    const RegexBuilder::Result withoutDotOption = RegexBuilder::analyze(
        QStringLiteral("a.b"), QStringLiteral("a\nb"), false, false);
    QCOMPARE(withoutDotOption.matches.size(), 0);

    const RegexBuilder::Result withDotOption = RegexBuilder::analyze(
        QStringLiteral("a.b"), QStringLiteral("a\nb"), false, true);
    QCOMPARE(withDotOption.matches.size(), 1);
}

void RegexBuilderTests::capsLargeMatchSets()
{
    const RegexBuilder::Result result = RegexBuilder::analyze(
        QStringLiteral("."), QStringLiteral("abcdef"), false, false, 3);

    QVERIFY(result.valid);
    QCOMPARE(result.matches.size(), 3);
    QVERIFY(result.truncated);
}

QTEST_APPLESS_MAIN(RegexBuilderTests)
#include "RegexBuilderTests.moc"
