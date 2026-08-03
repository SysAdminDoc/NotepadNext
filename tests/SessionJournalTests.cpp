/*
 * This file is part of Notepad Next.
 * Copyright 2026 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SessionJournal.h"

#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class SessionJournalTests : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsManifest();
    void rejectsInvalidManifest();
    void rejectsUnsafeBufferNames();
};

void SessionJournalTests::roundTripsManifest()
{
    SessionJournal::Manifest expected;
    expected.generation = QStringLiteral("generation-test");
    expected.currentEditorIndex = 2;

    QJsonObject entry;
    entry[QStringLiteral("Type")] = QStringLiteral("Temp");
    entry[QStringLiteral("SessionFileName")] = QStringLiteral("buffer");
    expected.openedFiles.append(entry);

    const QByteArray data = SessionJournal::serialize(expected);
    QString error;
    const auto actual = SessionJournal::parse(data, &error);

    QVERIFY2(actual.has_value(), qPrintable(error));
    QCOMPARE(actual->generation, expected.generation);
    QCOMPARE(actual->currentEditorIndex, expected.currentEditorIndex);
    QCOMPARE(actual->openedFiles, expected.openedFiles);
}

void SessionJournalTests::rejectsInvalidManifest()
{
    QString error;
    QVERIFY(!SessionJournal::parse(QByteArrayLiteral("{}"), &error).has_value());
    QVERIFY(!error.isEmpty());
    QVERIFY(!SessionJournal::parse(QByteArrayLiteral("not json"), &error).has_value());
}

void SessionJournalTests::rejectsUnsafeBufferNames()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QDir generationDirectory(temporaryDirectory.path());
    QVERIFY(generationDirectory.mkpath(QStringLiteral("generation-test")));
    QVERIFY(generationDirectory.cd(QStringLiteral("generation-test")));

    QVERIFY(SessionJournal::isSafeBufferName(generationDirectory, QStringLiteral("buffer")));
    QVERIFY(!SessionJournal::isSafeBufferName(generationDirectory, QStringLiteral("..\\buffer")));
    QVERIFY(!SessionJournal::isSafeBufferName(generationDirectory, QStringLiteral("../buffer")));
    QVERIFY(!SessionJournal::isSafeBufferName(generationDirectory, generationDirectory.filePath(QStringLiteral("buffer"))));
}

QTEST_MAIN(SessionJournalTests)
#include "SessionJournalTests.moc"
