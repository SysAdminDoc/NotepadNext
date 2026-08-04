/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "GitRepository.h"

#include <git2.h>

#include <QDir>
#include <QFileInfo>
#include <QMap>


namespace
{
enum class OpenRepositoryResult
{
    Opened,
    NotRepository,
    Error
};

struct RepositoryContext
{
    ~RepositoryContext()
    {
        if (repository) {
            git_repository_free(repository);
        }
    }

    git_repository *repository = nullptr;
    QByteArray relativePath;
    QString root;
};

QString gitError(const QString &operation)
{
    const git_error *error = git_error_last();
    if (!error || !error->message) {
        return operation;
    }
    return operation + QStringLiteral(": ") + QString::fromUtf8(error->message);
}

OpenRepositoryResult openRepository(const QString &filePath, RepositoryContext &context, QString *error)
{
    if (filePath.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("The file path is empty");
        }
        return OpenRepositoryResult::Error;
    }

    const QFileInfo fileInfo(filePath);
    const QString absolutePath = fileInfo.absoluteFilePath();
    const QByteArray startDirectory = fileInfo.absolutePath().toUtf8();

    git_buf discovered = GIT_BUF_INIT;
    const int discoverResult = git_repository_discover(&discovered, startDirectory.constData(), 0, nullptr);
    if (discoverResult == GIT_ENOTFOUND) {
        git_buf_dispose(&discovered);
        return OpenRepositoryResult::NotRepository;
    }
    if (discoverResult != 0 || !discovered.ptr) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to discover the Git repository"));
        }
        git_buf_dispose(&discovered);
        return OpenRepositoryResult::Error;
    }

    const int openResult = git_repository_open(&context.repository, discovered.ptr);
    git_buf_dispose(&discovered);
    if (openResult != 0 || !context.repository) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to open the Git repository"));
        }
        return OpenRepositoryResult::Error;
    }

    const char *workDirectory = git_repository_workdir(context.repository);
    if (!workDirectory || !*workDirectory) {
        if (error) {
            *error = QStringLiteral("The Git repository has no working tree");
        }
        return OpenRepositoryResult::Error;
    }

    const QDir root(QString::fromUtf8(workDirectory));
    const QString relativePath = QDir::cleanPath(root.relativeFilePath(absolutePath));
    if (relativePath == QStringLiteral(".") || relativePath == QStringLiteral("..") ||
        relativePath.startsWith(QStringLiteral("../")) || relativePath.startsWith(QStringLiteral("..\\"))) {
        if (error) {
            *error = QStringLiteral("The file is outside the Git working tree");
        }
        return OpenRepositoryResult::Error;
    }

    context.root = root.absolutePath();
    context.relativePath = QDir::fromNativeSeparators(relativePath).toUtf8();
    return OpenRepositoryResult::Opened;
}

bool isStaged(unsigned int status)
{
    constexpr unsigned int stagedFlags = GIT_STATUS_INDEX_NEW |
        GIT_STATUS_INDEX_MODIFIED |
        GIT_STATUS_INDEX_DELETED |
        GIT_STATUS_INDEX_RENAMED |
        GIT_STATUS_INDEX_TYPECHANGE;
    return (status & stagedFlags) != 0;
}

bool isUnstaged(unsigned int status)
{
    constexpr unsigned int worktreeFlags = GIT_STATUS_WT_NEW |
        GIT_STATUS_WT_MODIFIED |
        GIT_STATUS_WT_DELETED |
        GIT_STATUS_WT_RENAMED |
        GIT_STATUS_WT_TYPECHANGE;
    return (status & worktreeFlags) != 0;
}

git_object *headCommitObject(git_repository *repository)
{
    git_reference *head = nullptr;
    if (git_repository_head(&head, repository) != 0) {
        return nullptr;
    }

    git_object *commit = nullptr;
    if (git_reference_peel(&commit, head, GIT_OBJECT_COMMIT) != 0) {
        commit = nullptr;
    }
    git_reference_free(head);
    return commit;
}

git_tree *headTree(git_repository *repository)
{
    git_object *commitObject = headCommitObject(repository);
    if (!commitObject) {
        return nullptr;
    }

    git_tree *tree = nullptr;
    if (git_commit_tree(&tree, reinterpret_cast<git_commit *>(commitObject)) != 0) {
        tree = nullptr;
    }
    git_object_free(commitObject);
    return tree;
}

struct DiffAccumulator
{
    QMap<int, GitRepository::LineChangeKind> lines;
    int currentNewLine = 0;
};

void recordChange(DiffAccumulator &accumulator, int line, GitRepository::LineChangeKind kind)
{
    if (line < 0) {
        return;
    }

    const auto existing = accumulator.lines.constFind(line);
    if (existing == accumulator.lines.constEnd()) {
        accumulator.lines.insert(line, kind);
        return;
    }

    if (existing.value() != kind) {
        accumulator.lines[line] = GitRepository::LineChangeKind::Modified;
    }
}

int diffHunkCallback(const git_diff_delta *, const git_diff_hunk *hunk, void *payload)
{
    auto *accumulator = static_cast<DiffAccumulator *>(payload);
    accumulator->currentNewLine = hunk && hunk->new_start > 0
        ? static_cast<int>(hunk->new_start - 1)
        : 0;
    return 0;
}

int diffLineCallback(const git_diff_delta *, const git_diff_hunk *, const git_diff_line *line, void *payload)
{
    if (!line) {
        return 0;
    }

    auto *accumulator = static_cast<DiffAccumulator *>(payload);
    if (line->origin == GIT_DIFF_LINE_CONTEXT || line->origin == GIT_DIFF_LINE_CONTEXT_EOFNL) {
        if (line->new_lineno > 0) {
            accumulator->currentNewLine = line->new_lineno;
        }
    }
    else if (line->origin == GIT_DIFF_LINE_DELETION || line->origin == GIT_DIFF_LINE_DEL_EOFNL) {
        recordChange(*accumulator, accumulator->currentNewLine, GitRepository::LineChangeKind::Deleted);
    }
    else if (line->origin == GIT_DIFF_LINE_ADDITION || line->origin == GIT_DIFF_LINE_ADD_EOFNL) {
        const int newLine = line->new_lineno > 0 ? line->new_lineno - 1 : accumulator->currentNewLine;
        recordChange(*accumulator, newLine, GitRepository::LineChangeKind::Added);
        if (line->new_lineno > 0) {
            accumulator->currentNewLine = line->new_lineno;
        }
    }
    return 0;
}

QString signatureName(const git_signature *signature)
{
    return signature && signature->name ? QString::fromUtf8(signature->name) : QString();
}

QString signatureCommit(const git_oid &oid, bool *zero)
{
    if (git_oid_iszero(&oid)) {
        if (zero) {
            *zero = true;
        }
        return QString();
    }

    char shortOid[9] = {};
    git_oid_tostr(shortOid, sizeof(shortOid), &oid);
    if (zero) {
        *zero = false;
    }
    return QString::fromLatin1(shortOid);
}

QVector<GitRepository::BlameLine> workingTreeBlame(int lineCount)
{
    QVector<GitRepository::BlameLine> lines;
    lines.reserve(lineCount);
    for (int line = 0; line < lineCount; ++line) {
        lines.append({QStringLiteral("Working tree"), {}, {}, true});
    }
    return lines;
}
}

bool GitRepository::initialize()
{
    return git_libgit2_init() > 0;
}

void GitRepository::shutdown()
{
    git_libgit2_shutdown();
}

GitRepository::FileState GitRepository::state(const QString &filePath)
{
    FileState result;
    RepositoryContext context;
    QString error;
    const OpenRepositoryResult openResult = openRepository(filePath, context, &error);
    if (openResult == OpenRepositoryResult::NotRepository) {
        return result;
    }
    if (openResult == OpenRepositoryResult::Error) {
        result.error = error;
        return result;
    }

    result.inRepository = true;
    result.repositoryRoot = context.root;

    unsigned int status = 0;
    const int statusResult = git_status_file(&status, context.repository, context.relativePath.constData());
    if (statusResult == GIT_ENOTFOUND) {
        return result;
    }
    if (statusResult != 0) {
        result.error = gitError(QStringLiteral("Unable to read Git status"));
        return result;
    }

    result.staged = isStaged(status);
    result.unstaged = isUnstaged(status);
    result.untracked = (status & GIT_STATUS_WT_NEW) != 0;
    result.conflicted = (status & GIT_STATUS_CONFLICTED) != 0;
    return result;
}

QVector<GitRepository::LineChange> GitRepository::changes(const QString &filePath, QString *error)
{
    if (error) {
        error->clear();
    }

    QVector<LineChange> result;
    RepositoryContext context;
    QString openError;
    const OpenRepositoryResult openResult = openRepository(filePath, context, &openError);
    if (openResult == OpenRepositoryResult::NotRepository) {
        return result;
    }
    if (openResult == OpenRepositoryResult::Error) {
        if (error) {
            *error = openError;
        }
        return result;
    }

    git_tree *tree = headTree(context.repository);
    git_diff *diff = nullptr;
    git_diff_options options = GIT_DIFF_OPTIONS_INIT;
    options.flags = static_cast<git_diff_option_t>(options.flags |
        GIT_DIFF_INCLUDE_UNTRACKED |
        GIT_DIFF_RECURSE_UNTRACKED_DIRS |
        GIT_DIFF_SHOW_UNTRACKED_CONTENT |
        GIT_DIFF_IGNORE_SUBMODULES |
        GIT_DIFF_DISABLE_PATHSPEC_MATCH);

    char *path = context.relativePath.data();
    options.pathspec.count = 1;
    options.pathspec.strings = &path;

    const int diffResult = git_diff_tree_to_workdir_with_index(&diff, context.repository, tree, &options);
    if (tree) {
        git_tree_free(tree);
    }
    if (diffResult != 0 || !diff) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to calculate Git changes"));
        }
        return result;
    }

    DiffAccumulator accumulator;
    const int foreachResult = git_diff_foreach(diff, nullptr, nullptr,
                                               diffHunkCallback, diffLineCallback, &accumulator);
    git_diff_free(diff);
    if (foreachResult != 0) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to read Git changes"));
        }
        return result;
    }

    result.reserve(accumulator.lines.size());
    for (auto it = accumulator.lines.cbegin(); it != accumulator.lines.cend(); ++it) {
        result.append({it.key(), it.value()});
    }
    return result;
}

bool GitRepository::stage(const QString &filePath, QString *error)
{
    if (error) {
        error->clear();
    }

    RepositoryContext context;
    QString openError;
    const OpenRepositoryResult openResult = openRepository(filePath, context, &openError);
    if (openResult != OpenRepositoryResult::Opened) {
        if (error) {
            *error = openResult == OpenRepositoryResult::NotRepository
                ? QStringLiteral("The file is not inside a Git repository")
                : openError;
        }
        return false;
    }

    git_index *index = nullptr;
    int result = git_repository_index(&index, context.repository);
    if (result == 0) {
        result = git_index_add_bypath(index, context.relativePath.constData());
    }
    if (result == 0) {
        result = git_index_write(index);
    }
    if (index) {
        git_index_free(index);
    }
    if (result != 0) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to stage the file"));
        }
        return false;
    }
    return true;
}

bool GitRepository::unstage(const QString &filePath, QString *error)
{
    if (error) {
        error->clear();
    }

    RepositoryContext context;
    QString openError;
    const OpenRepositoryResult openResult = openRepository(filePath, context, &openError);
    if (openResult != OpenRepositoryResult::Opened) {
        if (error) {
            *error = openResult == OpenRepositoryResult::NotRepository
                ? QStringLiteral("The file is not inside a Git repository")
                : openError;
        }
        return false;
    }

    git_object *target = headCommitObject(context.repository);
    char *path = context.relativePath.data();
    git_strarray pathspec {&path, 1};
    const int result = git_reset_default(context.repository, target, &pathspec);
    if (target) {
        git_object_free(target);
    }
    if (result != 0) {
        if (error) {
            *error = gitError(QStringLiteral("Unable to unstage the file"));
        }
        return false;
    }
    return true;
}

QVector<GitRepository::BlameLine> GitRepository::blame(const QString &filePath, const QByteArray &contents,
                                                        QString *error)
{
    if (error) {
        error->clear();
    }

    const int lineCount = qMax(1, contents.count('\n') + 1);
    RepositoryContext context;
    QString openError;
    const OpenRepositoryResult openResult = openRepository(filePath, context, &openError);
    if (openResult == OpenRepositoryResult::NotRepository) {
        return {};
    }
    if (openResult == OpenRepositoryResult::Error) {
        if (error) {
            *error = openError;
        }
        return {};
    }

    git_blame_options options = GIT_BLAME_OPTIONS_INIT;
    git_blame *fileBlame = nullptr;
    const int fileResult = git_blame_file(&fileBlame, context.repository,
                                          context.relativePath.constData(), &options);
    if (fileResult != 0 || !fileBlame) {
        return workingTreeBlame(lineCount);
    }

    git_blame *blameResult = nullptr;
    const int bufferResult = git_blame_buffer(&blameResult, fileBlame, contents.constData(), contents.size());
    git_blame_free(fileBlame);
    if (bufferResult != 0 || !blameResult) {
        if (blameResult) {
            git_blame_free(blameResult);
        }
        return workingTreeBlame(lineCount);
    }

    QVector<BlameLine> lines;
    lines.reserve(lineCount);
    for (int line = 1; line <= lineCount; ++line) {
        const git_blame_hunk *hunk = git_blame_hunk_byline(blameResult, static_cast<size_t>(line));
        if (!hunk) {
            lines.append({QStringLiteral("Working tree"), {}, {}, true});
            continue;
        }

        bool uncommitted = false;
        const QString commit = signatureCommit(hunk->final_commit_id, &uncommitted);
        const git_signature *signature = hunk->final_signature ? hunk->final_signature : hunk->orig_signature;
        lines.append({signatureName(signature), commit,
                      hunk->summary ? QString::fromUtf8(hunk->summary) : QString(), uncommitted});
    }
    git_blame_free(blameResult);
    return lines;
}
