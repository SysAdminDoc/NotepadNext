/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "DirectoryDropScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace
{
struct PendingDirectory
{
    QString path;
    int depth = 0;
};

QString pathKey(const QString &path)
{
#ifdef Q_OS_WIN
    return QDir::fromNativeSeparators(path).toCaseFolded();
#else
    return QDir::cleanPath(path);
#endif
}

QString canonicalPath(const QFileInfo &info)
{
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

void skip(DirectoryDropScanner::Result &result, const QString &path, const QString &reason)
{
    result.skipped.append({path, reason});
}
}

DirectoryDropScanner::Result DirectoryDropScanner::scan(const QStringList &paths,
                                                         const Limits &limits,
                                                         const CancellationCallback &isCancelled)
{
    Result result;
    QElapsedTimer timer;
    timer.start();

    const qsizetype maxFiles = qMax<qsizetype>(0, limits.maxFiles);
    const qint64 maxBytes = qMax<qint64>(0, limits.maxBytes);
    const int maxDepth = qMax(0, limits.maxDepth);
    const int maxMilliseconds = qMax(0, limits.maxMilliseconds);

    QSet<QString> visitedDirectories;
    QSet<QString> visitedFiles;
    QVector<PendingDirectory> pendingDirectories;
    qint64 totalBytes = 0;
    bool hardBudgetReached = false;

    const auto stopIfRequested = [&]() {
        if (hardBudgetReached) {
            return true;
        }
        if (isCancelled && isCancelled()) {
            result.cancelled = true;
            return true;
        }
        if (timer.elapsed() >= maxMilliseconds) {
            result.budgetExceeded = true;
            return true;
        }
        return false;
    };

    const auto addFile = [&](const QFileInfo &info) {
        const QString path = info.absoluteFilePath();
        if (info.isSymLink()) {
            skip(result, path, QStringLiteral("symbolic links are not followed"));
            return;
        }
        if (!info.isFile()) {
            skip(result, path, QStringLiteral("unsupported or special file"));
            return;
        }
        if (!info.isReadable()) {
            skip(result, path, QStringLiteral("file is not readable"));
            return;
        }

        const QString canonical = canonicalPath(info);
        const QString key = pathKey(canonical);
        if (visitedFiles.contains(key)) {
            skip(result, path, QStringLiteral("duplicate file"));
            return;
        }

        const qint64 size = info.size();
        if (size < 0) {
            skip(result, path, QStringLiteral("file size is unavailable"));
            return;
        }
        if (result.files.size() >= maxFiles) {
            result.budgetExceeded = true;
            hardBudgetReached = true;
            skip(result, path, QStringLiteral("file-count limit reached"));
            return;
        }
        if (size > maxBytes - totalBytes) {
            result.budgetExceeded = true;
            hardBudgetReached = true;
            skip(result, path, QStringLiteral("byte limit reached"));
            return;
        }

        QFile readableFile(path);
        if (!readableFile.open(QIODevice::ReadOnly)) {
            skip(result, path, QStringLiteral("file could not be opened"));
            return;
        }
        readableFile.close();

        visitedFiles.insert(key);
        result.files.append(canonical);
        totalBytes += size;
    };

    const auto enqueueDirectory = [&](const QFileInfo &info, int depth) {
        const QString path = info.absoluteFilePath();
        if (info.isSymLink()) {
            skip(result, path, QStringLiteral("symbolic links are not followed"));
            return;
        }
        if (!info.isDir()) {
            addFile(info);
            return;
        }
        if (depth > maxDepth) {
            result.budgetExceeded = true;
            skip(result, path, QStringLiteral("directory depth limit reached"));
            return;
        }

        const QString canonical = canonicalPath(info);
        const QString key = pathKey(canonical);
        if (visitedDirectories.contains(key)) {
            skip(result, path, QStringLiteral("duplicate directory"));
            return;
        }

        visitedDirectories.insert(key);
        pendingDirectories.append({canonical, depth});
    };

    for (const QString &path : paths) {
        if (stopIfRequested()) {
            return result;
        }

        const QFileInfo info(path);
        if (!info.exists()) {
            skip(result, path, QStringLiteral("path does not exist"));
            continue;
        }
        enqueueDirectory(info, 0);
    }

    while (!pendingDirectories.isEmpty()) {
        if (stopIfRequested()) {
            return result;
        }

        const PendingDirectory current = pendingDirectories.takeLast();
        QDir directory(current.path);
        if (!directory.exists() || !directory.isReadable()) {
            skip(result, current.path, QStringLiteral("directory could not be read"));
            continue;
        }

        QDirIterator iterator(directory.absolutePath(),
                              QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                              QDirIterator::NoIteratorFlags);
        while (iterator.hasNext()) {
            if (stopIfRequested()) {
                return result;
            }

            const QString entryPath = iterator.next();
            const QFileInfo info = iterator.fileInfo();
            if (info.isDir() && !info.isSymLink()) {
                if (current.depth >= maxDepth) {
                    result.budgetExceeded = true;
                    skip(result, entryPath, QStringLiteral("directory depth limit reached"));
                }
                else {
                    enqueueDirectory(info, current.depth + 1);
                }
            }
            else {
                enqueueDirectory(info, current.depth);
            }
        }
    }

    return result;
}
