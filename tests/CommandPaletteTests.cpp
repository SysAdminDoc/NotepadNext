/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CommandPaletteFilter.h"

#include <QtTest>

class CommandPaletteTests final : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueryMatchesEverything();
    void exactMatchRanksAboveSubsequence();
    void wordBoundariesImproveSubsequenceScore();
    void unrelatedCommandIsRejected();
};

void CommandPaletteTests::emptyQueryMatchesEverything()
{
    QCOMPARE(CommandPaletteFilter::score(QString(), QStringLiteral("Save All")), 0);
    QCOMPARE(CommandPaletteFilter::score(QStringLiteral("   "), QStringLiteral("Save All")), 0);
}

void CommandPaletteTests::exactMatchRanksAboveSubsequence()
{
    const int exact = CommandPaletteFilter::score(QStringLiteral("save"), QStringLiteral("Save All"));
    const int subsequence = CommandPaletteFilter::score(QStringLiteral("sv"), QStringLiteral("Save All"));

    QVERIFY(exact > subsequence);
}

void CommandPaletteTests::wordBoundariesImproveSubsequenceScore()
{
    const int boundaryMatch = CommandPaletteFilter::score(QStringLiteral("fl"), QStringLiteral("Find All"));
    const int interiorMatch = CommandPaletteFilter::score(QStringLiteral("fl"), QStringLiteral("Refill"));

    QVERIFY(boundaryMatch > interiorMatch);
}

void CommandPaletteTests::unrelatedCommandIsRejected()
{
    QCOMPARE(CommandPaletteFilter::score(QStringLiteral("xyz"), QStringLiteral("Save All")), -1);
}

QTEST_MAIN(CommandPaletteTests)
#include "CommandPaletteTests.moc"
