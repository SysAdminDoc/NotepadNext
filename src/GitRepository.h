/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GITREPOSITORY_H
#define GITREPOSITORY_H

#include <QByteArray>
#include <QString>
#include <QVector>


class GitRepository final
{
public:
    struct FileState
    {
        bool inRepository = false;
        bool staged = false;
        bool unstaged = false;
        bool untracked = false;
        bool conflicted = false;
        QString repositoryRoot;
        QString error;
    };

    enum class LineChangeKind
    {
        Added,
        Modified,
        Deleted
    };

    struct LineChange
    {
        int line = 0;
        LineChangeKind kind = LineChangeKind::Modified;
    };

    struct BlameLine
    {
        QString author;
        QString commit;
        QString summary;
        bool uncommitted = false;
    };

    static bool initialize();
    static void shutdown();

    static FileState state(const QString &filePath);
    static QVector<LineChange> changes(const QString &filePath, QString *error = nullptr);
    static bool stage(const QString &filePath, QString *error = nullptr);
    static bool unstage(const QString &filePath, QString *error = nullptr);
    static QVector<BlameLine> blame(const QString &filePath, const QByteArray &contents,
                                    QString *error = nullptr);
};

#endif // GITREPOSITORY_H
