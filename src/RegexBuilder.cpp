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

#include "RegexBuilder.h"

#include <QRegularExpression>

RegexBuilder::Result RegexBuilder::analyze(const QString &pattern,
                                           const QString &sample,
                                           bool caseInsensitive,
                                           bool dotMatchesNewline,
                                           int maximumMatches)
{
    Result result;
    const QRegularExpression::PatternOptions options =
        QRegularExpression::MultilineOption
        | QRegularExpression::UseUnicodePropertiesOption
        | (caseInsensitive ? QRegularExpression::CaseInsensitiveOption
                           : QRegularExpression::PatternOptions())
        | (dotMatchesNewline ? QRegularExpression::DotMatchesEverythingOption
                             : QRegularExpression::PatternOptions());
    const QRegularExpression expression(pattern, options);

    if (!expression.isValid()) {
        result.error = expression.errorString();
        result.errorOffset = expression.patternErrorOffset();
        return result;
    }

    result.valid = true;
    result.captureCount = expression.captureCount();
    result.captureNames = expression.namedCaptureGroups();

    if (pattern.isEmpty() || maximumMatches <= 0) {
        return result;
    }

    QRegularExpressionMatchIterator iterator = expression.globalMatch(sample);
    while (iterator.hasNext()) {
        if (result.matches.size() >= maximumMatches) {
            result.truncated = true;
            break;
        }

        const QRegularExpressionMatch match = iterator.next();
        if (!match.hasMatch()) {
            continue;
        }

        Match matchResult;
        matchResult.number = result.matches.size() + 1;
        matchResult.value = match.captured(0);
        matchResult.start = match.capturedStart(0);
        for (int groupNumber = 0; groupNumber <= result.captureCount; ++groupNumber) {
            Group group;
            group.number = groupNumber;
            if (groupNumber < result.captureNames.size()) {
                group.name = result.captureNames.at(groupNumber);
            }
            group.value = match.captured(groupNumber);
            group.start = match.capturedStart(groupNumber);
            group.length = match.capturedLength(groupNumber);
            matchResult.groups.append(group);
        }
        result.matches.append(matchResult);
    }

    return result;
}
