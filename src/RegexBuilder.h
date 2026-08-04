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
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef REGEXBUILDER_H
#define REGEXBUILDER_H

#include <QVector>
#include <QString>
#include <QStringList>

class RegexBuilder final
{
public:
    struct Group
    {
        int number = 0;
        QString name;
        QString value;
        int start = -1;
        int length = 0;

        bool matched() const { return start >= 0; }
    };

    struct Match
    {
        int number = 0;
        QString value;
        int start = -1;
        QVector<Group> groups;
    };

    struct Result
    {
        bool valid = false;
        QString error;
        int errorOffset = -1;
        int captureCount = 0;
        QStringList captureNames;
        QVector<Match> matches;
        bool truncated = false;
    };

    static Result analyze(const QString &pattern,
                          const QString &sample,
                          bool caseInsensitive,
                          bool dotMatchesNewline,
                          int maximumMatches = 500);
};

#endif // REGEXBUILDER_H
