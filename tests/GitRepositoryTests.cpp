/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "GitRepository.h"

#include <git2.h>

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>


namespace
{
bool writeText(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

bool createInitialCommit(const QString &root, const char *path, QString *error)
{
    git_repository *repository = nullptr;
    git_index *index = nullptr;
    git_tree *tree = nullptr;
    git_signature *signature = nullptr;
    bool success = false;

    const QByteArray rootBytes = root.toUtf8();
    int result = git_repository_open(&repository, rootBytes.constData());
    if (result == 0) {
        result = git_repository_index(&index, repository);
    }
    if (result == 0) {
        result = git_index_add_bypath(index, path);
    }
    if (result == 0) {
        result = git_index_write(index);
    }

    git_oid treeId;
    if (result == 0) {
        result = git_index_write_tree(&treeId, index);
    }
    if (result == 0) {
        result = git_tree_lookup(&tree, repository, &treeId);
    }
    if (result == 0) {
        result = git_signature_now(&signature, "SysAdminDoc", "test@example.com");
    }
    if (result == 0) {
        git_oid commitId;
        result = git_commit_create_v(&commitId, repository, "HEAD", signature, signature,
                                     nullptr, "Initial commit", tree, 0, nullptr);
    }
    if (result == 0) {
        success = true;
    }

    if (!success && error) {
        const git_error *gitError = git_error_last();
        *error = gitError && gitError->message ? QString::fromUtf8(gitError->message)
                                               : QStringLiteral("Unable to create the initial commit");
    }
    if (signature) {
        git_signature_free(signature);
    }
    if (tree) {
        git_tree_free(tree);
    }
    if (index) {
        git_index_free(index);
    }
    if (repository) {
        git_repository_free(repository);
    }
    return success;
}
}

class GitRepositoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void tracksStatusAndLineChanges();
    void reportsBlameForSavedAndWorkingTreeLines();
};

void GitRepositoryTests::initTestCase()
{
    QVERIFY(GitRepository::initialize());
}

void GitRepositoryTests::cleanupTestCase()
{
    GitRepository::shutdown();
}

void GitRepositoryTests::tracksStatusAndLineChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeText(filePath, QByteArrayLiteral("alpha\nbeta\n")));

    git_repository *repository = nullptr;
    const QByteArray root = directory.path().toUtf8();
    QVERIFY2(git_repository_init(&repository, root.constData(), 0) == 0, qPrintable(QString::fromUtf8(git_error_last()->message)));
    git_repository_free(repository);

    QString error;
    QVERIFY2(createInitialCommit(directory.path(), "notes.txt", &error), qPrintable(error));

    const GitRepository::FileState clean = GitRepository::state(filePath);
    QVERIFY(clean.inRepository);
    QVERIFY(!clean.staged);
    QVERIFY(!clean.unstaged);
    QCOMPARE(QDir::cleanPath(clean.repositoryRoot), QDir::cleanPath(directory.path()));

    QVERIFY(writeText(filePath, QByteArrayLiteral("alpha\nbravo\ncharlie\n")));
    const GitRepository::FileState modified = GitRepository::state(filePath);
    QVERIFY(modified.unstaged);
    QVERIFY(!modified.staged);

    const QVector<GitRepository::LineChange> changes = GitRepository::changes(filePath, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(std::any_of(changes.cbegin(), changes.cend(), [](const GitRepository::LineChange &change) {
        return change.line == 1 && change.kind == GitRepository::LineChangeKind::Modified;
    }));
    QVERIFY(std::any_of(changes.cbegin(), changes.cend(), [](const GitRepository::LineChange &change) {
        return change.line == 2 && change.kind == GitRepository::LineChangeKind::Added;
    }));

    QVERIFY2(GitRepository::stage(filePath, &error), qPrintable(error));
    const GitRepository::FileState staged = GitRepository::state(filePath);
    QVERIFY(staged.staged);
    QVERIFY(!staged.unstaged);

    QVERIFY(writeText(filePath, QByteArrayLiteral("alpha\nbravo\ncharlie!\n")));
    const GitRepository::FileState stagedAndModified = GitRepository::state(filePath);
    QVERIFY(stagedAndModified.staged);
    QVERIFY(stagedAndModified.unstaged);

    QVERIFY2(GitRepository::unstage(filePath, &error), qPrintable(error));
    const GitRepository::FileState unstaged = GitRepository::state(filePath);
    QVERIFY(!unstaged.staged);
    QVERIFY(unstaged.unstaged);

    const QString newFilePath = directory.filePath(QStringLiteral("new.txt"));
    QVERIFY(writeText(newFilePath, QByteArrayLiteral("new file\n")));
    const GitRepository::FileState untracked = GitRepository::state(newFilePath);
    QVERIFY(untracked.inRepository);
    QVERIFY(untracked.untracked);
    QVERIFY(untracked.unstaged);
    const QVector<GitRepository::LineChange> untrackedChanges = GitRepository::changes(newFilePath, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(std::any_of(untrackedChanges.cbegin(), untrackedChanges.cend(), [](const GitRepository::LineChange &change) {
        return change.line == 0 && change.kind == GitRepository::LineChangeKind::Added;
    }));
    QVERIFY2(GitRepository::stage(newFilePath, &error), qPrintable(error));
    QVERIFY(GitRepository::state(newFilePath).staged);
    QVERIFY2(GitRepository::unstage(newFilePath, &error), qPrintable(error));
    QVERIFY(GitRepository::state(newFilePath).untracked);
}

void GitRepositoryTests::reportsBlameForSavedAndWorkingTreeLines()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("blame.txt"));
    QVERIFY(writeText(filePath, QByteArrayLiteral("first\nsecond\n")));

    git_repository *repository = nullptr;
    const QByteArray root = directory.path().toUtf8();
    QVERIFY(git_repository_init(&repository, root.constData(), 0) == 0);
    git_repository_free(repository);

    QString error;
    QVERIFY2(createInitialCommit(directory.path(), "blame.txt", &error), qPrintable(error));

    const QVector<GitRepository::BlameLine> saved = GitRepository::blame(filePath, QByteArrayLiteral("first\nsecond\n"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(saved.size(), 3);
    QVERIFY(!saved.at(0).uncommitted);
    QCOMPARE(saved.at(0).author, QStringLiteral("SysAdminDoc"));
    QVERIFY(!saved.at(0).commit.isEmpty());

    const QVector<GitRepository::BlameLine> working = GitRepository::blame(filePath, QByteArrayLiteral("first\nchanged\n"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(working.size(), 3);
    QVERIFY(!working.at(0).uncommitted);
    QVERIFY(working.at(1).uncommitted);
}

QTEST_GUILESS_MAIN(GitRepositoryTests)
#include "GitRepositoryTests.moc"
