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

#include "SnippetEngine.h"

#include <algorithm>

namespace
{
bool isDigit(const QChar character)
{
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

void appendText(SnippetEngine::Expansion *expansion, int *byteLength, const QString &text)
{
    expansion->text += text;
    *byteLength += text.toUtf8().size();
}

void appendPlaceholder(SnippetEngine::Expansion *expansion,
                       int *byteLength,
                       int number,
                       const QString &defaultText)
{
    const int start = *byteLength;
    appendText(expansion, byteLength, defaultText);
    expansion->placeholders.append({number, start, *byteLength - start, defaultText});
}
}

SnippetEngine::Expansion SnippetEngine::expand(const QString &body)
{
    Expansion expansion;
    int byteLength = 0;

    for (int index = 0; index < body.size();) {
        if (body.at(index) != QLatin1Char('$')) {
            int nextIndex = index + 1;
            if (body.at(index).isHighSurrogate()
                && nextIndex < body.size()
                && body.at(nextIndex).isLowSurrogate()) {
                ++nextIndex;
            }
            appendText(&expansion, &byteLength, body.mid(index, nextIndex - index));
            index = nextIndex;
            continue;
        }

        if (index + 1 >= body.size()) {
            appendText(&expansion, &byteLength, QStringLiteral("$"));
            ++index;
            continue;
        }

        const QChar next = body.at(index + 1);
        if (next == QLatin1Char('$')) {
            appendText(&expansion, &byteLength, QStringLiteral("$"));
            index += 2;
            continue;
        }

        if (isDigit(next)) {
            int end = index + 1;
            while (end < body.size() && isDigit(body.at(end))) {
                ++end;
            }

            bool ok = false;
            const int number = body.mid(index + 1, end - index - 1).toInt(&ok);
            if (ok) {
                appendPlaceholder(&expansion, &byteLength, number, QString());
                index = end;
                continue;
            }
        }

        if (next == QLatin1Char('{')) {
            int numberEnd = index + 2;
            while (numberEnd < body.size() && isDigit(body.at(numberEnd))) {
                ++numberEnd;
            }

            bool ok = false;
            const int number = body.mid(index + 2, numberEnd - index - 2).toInt(&ok);
            if (ok && numberEnd < body.size()) {
                if (body.at(numberEnd) == QLatin1Char('}')) {
                    appendPlaceholder(&expansion, &byteLength, number, QString());
                    index = numberEnd + 1;
                    continue;
                }

                if (body.at(numberEnd) == QLatin1Char(':')) {
                    const int close = body.indexOf(QLatin1Char('}'), numberEnd + 1);
                    if (close >= 0) {
                        const QString defaultText = body.mid(numberEnd + 1, close - numberEnd - 1);
                        appendPlaceholder(&expansion, &byteLength, number, defaultText);
                        index = close + 1;
                        continue;
                    }
                }
            }
        }

        appendText(&expansion, &byteLength, QStringLiteral("$"));
        ++index;
    }

    std::stable_sort(expansion.placeholders.begin(), expansion.placeholders.end(),
                     [](const Placeholder &left, const Placeholder &right) {
        if ((left.number == 0) != (right.number == 0)) {
            return left.number != 0;
        }
        if (left.number != right.number) {
            return left.number < right.number;
        }
        return left.start < right.start;
    });

    return expansion;
}
