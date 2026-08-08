/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#ifndef DIRECTORYDROPSCANNER_H
#define DIRECTORYDROPSCANNER_H

#include <QStringList>
#include <QVector>

#include <functional>

class DirectoryDropScanner final
{
public:
    struct Limits
    {
        qsizetype maxFiles = 512;
        qint64 maxBytes = 256LL * 1024 * 1024;
        int maxDepth = 32;
        int maxMilliseconds = 2000;
    };

    struct SkippedItem
    {
        QString path;
        QString reason;
    };

    struct Result
    {
        QStringList files;
        QVector<SkippedItem> skipped;
        bool cancelled = false;
        bool budgetExceeded = false;
    };

    using CancellationCallback = std::function<bool()>;

    static Result scan(const QStringList &paths,
                       const Limits &limits = Limits(),
                       const CancellationCallback &isCancelled = {});
};

#endif // DIRECTORYDROPSCANNER_H
