/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "ScintillaNext.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

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

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
}

class ExternalFileChangeTests final : public QObject
{
    Q_OBJECT

private slots:
    void reloadFailurePreservesInMemoryDocument();
    void fileStateTransitionsIncludeConflictAndRestore();
    void saveRefusesExternalChangeUntilExplicitOverride();
};

void ExternalFileChangeTests::reloadFailurePreservesInMemoryDocument()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("on disk")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("on disk"));

    editor->setText("in memory");
    QVERIFY(QFile::remove(path));

    QString error;
    QVERIFY(!editor->reload(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("in memory"));
}

void ExternalFileChangeTests::fileStateTransitionsIncludeConflictAndRestore()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("original")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::NoChange);

    QVERIFY(writeFile(path, QByteArrayLiteral("external")));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Modified);

    editor->omitModifications();
    QVERIFY(editor->hasExternalChangePending());
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Conflict);

    QString error;
    QVERIFY(editor->reload(&error));
    QVERIFY(!editor->hasExternalChangePending());
    QVERIFY(QFile::remove(path));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Deleted);

    QVERIFY(writeFile(path, QByteArrayLiteral("restored")));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Restored);
    QVERIFY(editor->hasExternalChangePending());
}

void ExternalFileChangeTests::saveRefusesExternalChangeUntilExplicitOverride()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("original")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    editor->setText("mine");
    QVERIFY(writeFile(path, QByteArrayLiteral("theirs")));

    QCOMPARE(editor->save(), QFileDevice::ResourceError);
    QVERIFY(editor->lastSaveWasConflict());
    QCOMPARE(readFile(path), QByteArrayLiteral("theirs"));

    QCOMPARE(editor->save(true), QFileDevice::NoError);
    QVERIFY(!editor->lastSaveWasConflict());
    QCOMPARE(readFile(path), QByteArrayLiteral("mine"));
}

QTEST_MAIN(ExternalFileChangeTests)
#include "ExternalFileChangeTests.moc"
