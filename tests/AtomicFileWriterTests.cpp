/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "AtomicFileWriter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

namespace
{
QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
}

class AtomicFileWriterTests final : public QObject
{
    Q_OBJECT

private slots:
    void writesByteArrayAtomically();
    void checksWriterFailuresAndPreservesDestination();
    void reportsOpenFailures();
};

void AtomicFileWriterTests::writesByteArrayAtomically()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("export.txt"));
    QVERIFY(QFile(path).open(QIODevice::WriteOnly));

    const AtomicFileWriter::Result result = AtomicFileWriter::write(path, QByteArrayLiteral("replacement"));
    QVERIFY2(result.succeeded(), qPrintable(result.errorString));
    QCOMPARE(readFile(path), QByteArrayLiteral("replacement"));
    QCOMPARE(QDir(root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList{QStringLiteral("export.txt")});
}

void AtomicFileWriterTests::checksWriterFailuresAndPreservesDestination()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("export.txt"));
    QFile original(path);
    QVERIFY(original.open(QIODevice::WriteOnly));
    QVERIFY(original.write("original") == 8);
    original.close();

    const AtomicFileWriter::Result result = AtomicFileWriter::write(
        path,
        [](QIODevice *device, QString *errorString) {
            device->write("partial");
            if (errorString) {
                *errorString = QStringLiteral("simulated stream failure");
            }
            return false;
        });

    QVERIFY(!result.succeeded());
    QCOMPARE(result.error, QFileDevice::WriteError);
    QCOMPARE(result.errorString, QStringLiteral("simulated stream failure"));
    QCOMPARE(readFile(path), QByteArrayLiteral("original"));
    QCOMPARE(QDir(root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList{QStringLiteral("export.txt")});
}

void AtomicFileWriterTests::reportsOpenFailures()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("missing/export.txt"));

    const AtomicFileWriter::Result result = AtomicFileWriter::write(path, QByteArrayLiteral("data"));
    QVERIFY(!result.succeeded());
    QVERIFY(result.error != QFileDevice::NoError);
    QVERIFY(!result.errorString.isEmpty());
}

QTEST_MAIN(AtomicFileWriterTests)
#include "AtomicFileWriterTests.moc"
