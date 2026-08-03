/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMANDPALETTEFILTER_H
#define COMMANDPALETTEFILTER_H

#include <QString>

namespace CommandPaletteFilter
{
    // Returns -1 when query is not a subsequence of candidate. Higher scores
    // are better and are stable for the same candidate/query pair.
    int score(const QString &query, const QString &candidate);
}

#endif // COMMANDPALETTEFILTER_H
