/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "RipgrepSearch.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(contents) == contents.size());
}
}

class RipgrepSearchTests final : public QObject
{
    Q_OBJECT

private slots:
    void findsMatchesInTemporaryTree();
    void reportsInvalidRegularExpressions();
};

void RipgrepSearchTests::findsMatchesInTemporaryTree()
{
    if (QStandardPaths::findExecutable(QStringLiteral("rg")).isEmpty()) {
        QSKIP("ripgrep is not available on PATH");
    }

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString firstFile = root.filePath(QStringLiteral("first.txt"));
    const QString secondFile = root.filePath(QStringLiteral("second.txt"));
    writeFile(firstFile, QByteArrayLiteral("needle here\nno match\n"));
    writeFile(secondFile, QByteArrayLiteral("NEEDLE again\n"));

    RipgrepSearch search;
    QSignalSpy matchSpy(&search, &RipgrepSearch::matchFound);
    QSignalSpy finishedSpy(&search, &RipgrepSearch::searchFinished);
    QSignalSpy errorSpy(&search, &RipgrepSearch::searchError);

    RipgrepSearch::Options options;
    options.pattern = QStringLiteral("needle");
    options.rootPath = root.path();
    options.caseSensitive = false;
    QVERIFY(search.start(options));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(matchSpy.count(), 2);

    const RipgrepSearch::Match firstMatch = qvariant_cast<RipgrepSearch::Match>(matchSpy.at(0).at(0));
    QCOMPARE(firstMatch.lineNumber, 1);
    QCOMPARE(firstMatch.column, 1);
    QCOMPARE(firstMatch.hitCount, 1);
    QVERIFY(firstMatch.filePath == QFileInfo(firstFile).absoluteFilePath() ||
            firstMatch.filePath == QFileInfo(secondFile).absoluteFilePath());
    QCOMPARE(finishedSpy.at(0).at(0).toInt(), 2);
    QCOMPARE(finishedSpy.at(0).at(1).toInt(), 2);
    QCOMPARE(finishedSpy.at(0).at(2).toBool(), false);
}

void RipgrepSearchTests::reportsInvalidRegularExpressions()
{
    if (QStandardPaths::findExecutable(QStringLiteral("rg")).isEmpty()) {
        QSKIP("ripgrep is not available on PATH");
    }

    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeFile(root.filePath(QStringLiteral("input.txt")), QByteArrayLiteral("text\n"));

    RipgrepSearch search;
    QSignalSpy errorSpy(&search, &RipgrepSearch::searchError);
    QSignalSpy finishedSpy(&search, &RipgrepSearch::searchFinished);

    RipgrepSearch::Options options;
    options.pattern = QStringLiteral("[");
    options.rootPath = root.path();
    options.regularExpression = true;
    QVERIFY(search.start(options));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains(QStringLiteral("regex"), Qt::CaseInsensitive));
    QCOMPARE(finishedSpy.at(0).at(2).toBool(), false);
}

QTEST_MAIN(RipgrepSearchTests)
#include "RipgrepSearchTests.moc"
