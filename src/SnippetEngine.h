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

#ifndef SNIPPETENGINE_H
#define SNIPPETENGINE_H

#include <QString>
#include <QVector>

class SnippetEngine final
{
public:
    struct Placeholder
    {
        int number = 0;
        int start = 0;
        int length = 0;
        QString defaultText;
    };

    struct Expansion
    {
        QString text;
        QVector<Placeholder> placeholders;
    };

    // Placeholder offsets are UTF-8 byte offsets, matching Scintilla's
    // document positions. Supported forms are ${1:default}, ${1}, $1, and ${0}.
    static Expansion expand(const QString &body);
};

#endif // SNIPPETENGINE_H
