/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "DirectoryDropScanner.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool containsPath(const QStringList &paths, const QString &path)
{
    return paths.contains(QFileInfo(path).canonicalFilePath());
}
}

class DirectoryDropScannerTests final : public QObject
{
    Q_OBJECT

private slots:
    void deduplicatesDirectoriesAndFiles();
    void enforcesDepthFileAndByteLimits();
    void cancellationAndMissingPathsAreReported();
    void doesNotFollowSymbolicLinks();
    void inaccessibleDirectoriesAreSkipped();
};

void DirectoryDropScannerTests::deduplicatesDirectoriesAndFiles()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("nested"))));

    const QString first = root.filePath(QStringLiteral("first.txt"));
    const QString second = root.filePath(QStringLiteral("nested/second.txt"));
    QVERIFY(writeFile(first, QByteArrayLiteral("first")));
    QVERIFY(writeFile(second, QByteArrayLiteral("second")));

    const DirectoryDropScanner::Result result = DirectoryDropScanner::scan(
        {root.path(), root.path(), first});

    QCOMPARE(result.files.size(), 2);
    QVERIFY(containsPath(result.files, first));
    QVERIFY(containsPath(result.files, second));
    QVERIFY(!result.cancelled);
}

void DirectoryDropScannerTests::enforcesDepthFileAndByteLimits()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("level1/level2"))));

    const QString rootFile = root.filePath(QStringLiteral("root.txt"));
    const QString levelOneFile = root.filePath(QStringLiteral("level1/one.txt"));
    const QString levelTwoFile = root.filePath(QStringLiteral("level1/level2/two.txt"));
    QVERIFY(writeFile(rootFile, QByteArrayLiteral("root")));
    QVERIFY(writeFile(levelOneFile, QByteArrayLiteral("one")));
    QVERIFY(writeFile(levelTwoFile, QByteArrayLiteral("two")));

    DirectoryDropScanner::Limits depthLimits;
    depthLimits.maxDepth = 1;
    depthLimits.maxFiles = 10;
    depthLimits.maxBytes = 1024;
    depthLimits.maxMilliseconds = 1000;
    const DirectoryDropScanner::Result depthResult = DirectoryDropScanner::scan({root.path()}, depthLimits);
    QCOMPARE(depthResult.files.size(), 2);
    QVERIFY(containsPath(depthResult.files, rootFile));
    QVERIFY(containsPath(depthResult.files, levelOneFile));
    QVERIFY(!containsPath(depthResult.files, levelTwoFile));
    QVERIFY(depthResult.budgetExceeded);

    DirectoryDropScanner::Limits fileLimits = depthLimits;
    fileLimits.maxDepth = 5;
    fileLimits.maxFiles = 1;
    const DirectoryDropScanner::Result fileResult = DirectoryDropScanner::scan({root.path()}, fileLimits);
    QCOMPARE(fileResult.files.size(), 1);
    QVERIFY(fileResult.budgetExceeded);

    DirectoryDropScanner::Limits byteLimits = depthLimits;
    byteLimits.maxDepth = 5;
    byteLimits.maxFiles = 10;
    byteLimits.maxBytes = 2;
    const DirectoryDropScanner::Result byteResult = DirectoryDropScanner::scan({rootFile}, byteLimits);
    QCOMPARE(byteResult.files.size(), 0);
    QVERIFY(byteResult.budgetExceeded);
}

void DirectoryDropScannerTests::cancellationAndMissingPathsAreReported()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString existing = root.filePath(QStringLiteral("existing.txt"));
    QVERIFY(writeFile(existing, QByteArrayLiteral("content")));

    const DirectoryDropScanner::Result cancelled = DirectoryDropScanner::scan(
        {root.path()},
        {},
        []() { return true; });
    QVERIFY(cancelled.cancelled);
    QCOMPARE(cancelled.files.size(), 0);

    const QString missing = root.filePath(QStringLiteral("missing.txt"));
    const DirectoryDropScanner::Result missingResult = DirectoryDropScanner::scan({missing});
    QCOMPARE(missingResult.files.size(), 0);
    QCOMPARE(missingResult.skipped.size(), 1);
    QVERIFY(missingResult.skipped.constFirst().reason.contains(QStringLiteral("does not exist")));
}

void DirectoryDropScannerTests::doesNotFollowSymbolicLinks()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString target = root.filePath(QStringLiteral("target.txt"));
    const QString link = root.filePath(QStringLiteral("link.txt"));
    QVERIFY(writeFile(target, QByteArrayLiteral("target")));

    if (!QFile::link(target, link) || !QFileInfo(link).isSymLink()) {
        QSKIP("symbolic links are unavailable in this test environment");
    }

    const DirectoryDropScanner::Result result = DirectoryDropScanner::scan({link});
    QCOMPARE(result.files.size(), 0);
    QCOMPARE(result.skipped.size(), 1);
    QVERIFY(result.skipped.constFirst().reason.contains(QStringLiteral("symbolic links")));
}

void DirectoryDropScannerTests::inaccessibleDirectoriesAreSkipped()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString inaccessible = root.filePath(QStringLiteral("inaccessible"));
    QVERIFY(QDir().mkpath(inaccessible));
    QVERIFY(writeFile(QDir(inaccessible).filePath(QStringLiteral("hidden.txt")), QByteArrayLiteral("hidden")));

    const QFileDevice::Permissions originalPermissions = QFile::permissions(inaccessible);
    if (!QFile::setPermissions(inaccessible, QFileDevice::Permissions()))
        QSKIP("permissions cannot be changed in this test environment");

    const bool becameUnreadable = !QDir(inaccessible).isReadable();
    const DirectoryDropScanner::Result result = DirectoryDropScanner::scan({inaccessible});
    QFile::setPermissions(inaccessible, originalPermissions);

    if (!becameUnreadable)
        QSKIP("permissions are not enforced in this test environment");

    QVERIFY(result.files.isEmpty());
    QVERIFY(!result.skipped.isEmpty());
}

QTEST_MAIN(DirectoryDropScannerTests)
#include "DirectoryDropScannerTests.moc"
