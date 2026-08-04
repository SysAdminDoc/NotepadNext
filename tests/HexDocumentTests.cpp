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

#include "HexDocument.h"
#include "HexTableModel.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class HexDocumentTests final : public QObject
{
    Q_OBJECT

private slots:
    void probesTextAndBinaryFiles();
    void loadsEditsAndSavesExactBytes();
    void exposesHexRowsAndAsciiColumn();
};

void HexDocumentTests::probesTextAndBinaryFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString textPath = directory.filePath(QStringLiteral("notes.txt"));
    QFile textFile(textPath);
    QVERIFY(textFile.open(QIODevice::WriteOnly));
    QCOMPARE(textFile.write("hello\nworld"), qint64(11));
    textFile.close();

    const QString binaryPath = directory.filePath(QStringLiteral("data.bin"));
    QFile binaryFile(binaryPath);
    QVERIFY(binaryFile.open(QIODevice::WriteOnly));
    QCOMPARE(binaryFile.write(QByteArray::fromHex("00414200FF10")), qint64(6));
    binaryFile.close();

    QCOMPARE(HexDocument::probeFile(textPath), HexDocument::ProbeResult::Text);
    QCOMPARE(HexDocument::probeFile(binaryPath), HexDocument::ProbeResult::Binary);
}

void HexDocumentTests::loadsEditsAndSavesExactBytes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("data.bin"));
    const QByteArray original = QByteArray::fromHex("0001027F80FF");

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(original), qint64(original.size()));
    file.close();

    HexDocument document;
    QVERIFY(document.load(path));
    QCOMPARE(document.data(), original);
    QVERIFY(!document.isDirty());
    QVERIFY(document.setByte(4, 0x42));
    QVERIFY(document.isDirty());
    QVERIFY(document.save());
    QVERIFY(!document.isDirty());

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray::fromHex("0001027F42FF"));
}

void HexDocumentTests::exposesHexRowsAndAsciiColumn()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("data.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(QByteArray::fromHex("414200FF10")), qint64(5));
    file.close();

    HexDocument document;
    HexTableModel model(&document);
    QVERIFY(model.loadFile(path));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), 18);
    QCOMPARE(model.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Offset"));
    QCOMPARE(model.data(model.index(0, 0)).toString(), QStringLiteral("00000000"));
    QCOMPARE(model.data(model.index(0, 1)).toString(), QStringLiteral("41"));
    QCOMPARE(model.data(model.index(0, 17)).toString(), QStringLiteral("AB..."));
    QVERIFY(model.setData(model.index(0, 4), QStringLiteral("7E")));
    QCOMPARE(model.data(model.index(0, 4)).toString(), QStringLiteral("7E"));
    QCOMPARE(model.data(model.index(0, 17)).toString(), QStringLiteral("AB.~."));
}

QTEST_APPLESS_MAIN(HexDocumentTests)
#include "HexDocumentTests.moc"
