/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CommandPaletteFilter.h"

namespace CommandPaletteFilter
{
int score(const QString &query, const QString &candidate)
{
    const QString needle = query.trimmed().toCaseFolded();
    const QString haystack = candidate.toCaseFolded();

    if (needle.isEmpty()) {
        return 0;
    }

    const int exactPosition = haystack.indexOf(needle);
    if (exactPosition >= 0) {
        return 10000 - exactPosition * 10 - (haystack.size() - needle.size());
    }

    int queryPosition = 0;
    int previousMatch = -2;
    int firstMatch = -1;
    int matchScore = 0;

    for (int candidatePosition = 0;
         candidatePosition < haystack.size() && queryPosition < needle.size();
         ++candidatePosition) {
        if (haystack.at(candidatePosition) != needle.at(queryPosition)) {
            continue;
        }

        if (firstMatch < 0) {
            firstMatch = candidatePosition;
        }

        const bool contiguous = previousMatch + 1 == candidatePosition;
        const bool wordBoundary = candidatePosition == 0
                || !haystack.at(candidatePosition - 1).isLetterOrNumber();

        matchScore += contiguous ? 35 : -5;
        if (wordBoundary) {
            matchScore += 25;
        }

        previousMatch = candidatePosition;
        ++queryPosition;
    }

    if (queryPosition != needle.size()) {
        return -1;
    }

    return 1000 + matchScore - firstMatch * 2 - (haystack.size() - needle.size());
}
}
