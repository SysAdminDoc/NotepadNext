/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
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


#include "QRegexSearch.h"

#include <QtGlobal>
#include <QRegularExpression>

using namespace Scintilla;

namespace
{
QString expandReplacement(const QRegularExpressionMatch &match, const QString &replacement)
{
    QString expanded;
    expanded.reserve(replacement.size());

    const int captureCount = match.regularExpression().captureCount();
    const auto appendCapture = [&expanded, &match](int index) {
        if (index >= 0) {
            expanded += match.captured(index);
        }
    };

    for (qsizetype index = 0; index < replacement.size();) {
        const QChar character = replacement.at(index);
        if (character == QLatin1Char('$')) {
            if (index + 1 >= replacement.size()) {
                expanded += character;
                ++index;
                continue;
            }

            const QChar next = replacement.at(index + 1);
            if (next == QLatin1Char('$')) {
                expanded += QLatin1Char('$');
                index += 2;
                continue;
            }
            if (next == QLatin1Char('&')) {
                appendCapture(0);
                index += 2;
                continue;
            }
            if (replacement.mid(index, 6) == QStringLiteral("$MATCH")) {
                appendCapture(0);
                index += 6;
                continue;
            }

            if (next == QLatin1Char('{')) {
                const qsizetype close = replacement.indexOf(QLatin1Char('}'), index + 2);
                if (close != -1) {
                    const QString reference = replacement.mid(index + 2, close - index - 2);
                    bool numeric = false;
                    const int capture = reference.toInt(&numeric);
                    if (numeric) {
                        appendCapture(capture);
                    } else {
                        expanded += match.captured(reference);
                    }
                    index = close + 1;
                    continue;
                }
            }

            if (next.isDigit()) {
                qsizetype end = index + 1;
                while (end < replacement.size() && replacement.at(end).isDigit()) {
                    ++end;
                }

                const QString digits = replacement.mid(index + 1, end - index - 1);
                int selectedCapture = -1;
                qsizetype selectedDigits = 0;
                for (qsizetype count = digits.size(); count > 0; --count) {
                    bool numeric = false;
                    const int candidate = digits.left(count).toInt(&numeric);
                    if (numeric && (candidate == 0 || candidate <= captureCount)) {
                        selectedCapture = candidate;
                        selectedDigits = count;
                        break;
                    }
                }

                if (selectedCapture >= 0) {
                    appendCapture(selectedCapture);
                    index += 1 + selectedDigits;
                    continue;
                }
            }

            expanded += character;
            ++index;
            continue;
        }

        if (character == QLatin1Char('\\') && index + 1 < replacement.size()) {
            const QChar next = replacement.at(index + 1);
            if (next.isDigit()) {
                qsizetype end = index + 1;
                while (end < replacement.size() && replacement.at(end).isDigit()) {
                    ++end;
                }

                bool numeric = false;
                const int capture = replacement.mid(index + 1, end - index - 1).toInt(&numeric);
                if (numeric) {
                    appendCapture(capture);
                    index = end;
                    continue;
                }
            }

            const QChar escaped = replacement.at(index + 1);
            if (escaped == QLatin1Char('a')) {
                expanded += QChar(0x0007);
            } else if (escaped == QLatin1Char('b')) {
                expanded += QChar(0x0008);
            } else if (escaped == QLatin1Char('f')) {
                expanded += QChar(0x000C);
            } else if (escaped == QLatin1Char('n')) {
                expanded += QLatin1Char('\n');
            } else if (escaped == QLatin1Char('r')) {
                expanded += QLatin1Char('\r');
            } else if (escaped == QLatin1Char('t')) {
                expanded += QLatin1Char('\t');
            } else if (escaped == QLatin1Char('v')) {
                expanded += QChar(0x000B);
            } else if (escaped == QLatin1Char('\\')) {
                expanded += QLatin1Char('\\');
            } else {
                expanded += character;
                expanded += escaped;
            }
            index += 2;
            continue;
        }

        expanded += character;
        ++index;
    }

    return expanded;
}
}

#ifdef SCI_OWNREGEX
RegexSearchBase *Scintilla::Internal::CreateRegexSearch(CharClassify *charClassTable)
{
    Q_UNUSED(charClassTable);

    qInfo(Q_FUNC_INFO);

    return new QRegexSearch();
}
#endif

QRegexSearch::QRegexSearch()
{

}

QRegexSearch::~QRegexSearch()
{
    delete substituted;
}

Sci::Position QRegexSearch::FindText(Document *doc, Sci::Position minPos, Sci::Position maxPos, const char *s, bool caseSensitive, bool word, bool wordStart, Scintilla::FindOption flags, Sci::Position *length)
{
    // -----------------------------------------------------------------------------------------------------------------------
    // NOTE: This section of code has to be very careful about what units of measure is being used. Scintilla wants to operate
    // in units of bytes (e.g. position 3 is 3 bytes into the text). Qt wants to operate in units of UTF16 chars. The trouble is
    // when you start using characters that are >1 byte a piece. Meaning position 3 (3 bytes into a file) could be 1 character.
    // -----------------------------------------------------------------------------------------------------------------------

    if (length == Q_NULLPTR) {
        return -1;
    }
    if (doc == Q_NULLPTR || s == Q_NULLPTR || *length <= 0) {
        *length = 0;
        return -1;
    }

    const Sci::Position patternLength = *length;

    const bool forward = minPos <= maxPos;
    Sci::Position rangeStart = forward ? minPos : maxPos;
    Sci::Position rangeEnd = forward ? maxPos : minPos;

    rangeStart = qBound<Sci::Position>(0, rangeStart, doc->Length());
    rangeEnd = qBound<Sci::Position>(0, rangeEnd, doc->Length());
    rangeStart = doc->MovePositionOutsideChar(rangeStart, 1, false);
    rangeEnd = doc->MovePositionOutsideChar(rangeEnd, -1, false);

    if (rangeStart > rangeEnd) {
        *length = 0;
        return -1;
    }

    auto options = QRegularExpression::MultilineOption
            | QRegularExpression::UseUnicodePropertiesOption;

    // Document::FindText passes the decoded search options as explicit
    // arguments. Keep the adapter aligned with that contract rather than
    // deriving case sensitivity a second time from the flag bitset.
    if (!caseSensitive)
        options |= QRegularExpression::CaseInsensitiveOption;

    if (FlagSet(flags, FindOption::Cxx11RegEx))
        options |= QRegularExpression::DotMatchesEverythingOption;

    // TODO: does (*ANYCRLF) need prepended to the search string?
    const QByteArray pattern(s, static_cast<qsizetype>(patternLength));
    QRegularExpression re(QString::fromUtf8(pattern), options);
    if (!re.isValid()) {
        *length = 0;
        return -1; // Invalid regular expression
    }

    // Get the bytes from the document. Scintilla positions are UTF-8 bytes;
    // QRegularExpression offsets are UTF-16 code units.
    const Sci::Position rangeLength = rangeEnd - rangeStart;
    const char *rangePointer = doc->RangePointer(rangeStart, rangeLength);
    const QString utf8 = QString::fromUtf8(rangePointer, static_cast<qsizetype>(rangeLength));

    QRegularExpressionMatch selectedMatch;
    Sci::Position selectedStart = -1;
    Sci::Position selectedEnd = -1;

    QRegularExpressionMatchIterator iterator = re.globalMatch(utf8);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch candidate = iterator.next();
        if (!candidate.hasMatch()) {
            continue;
        }

        const Sci::Position candidateStart = doc->GetRelativePositionUTF16(
                rangeStart, candidate.capturedStart(0));
        const Sci::Position candidateEnd = doc->GetRelativePositionUTF16(
                candidateStart, candidate.capturedLength(0));

        if (candidateStart < rangeStart || candidateEnd > rangeEnd
                || candidateStart < 0 || candidateEnd < candidateStart
                || !doc->MatchesWordOptions(word, wordStart, candidateStart,
                                             candidateEnd - candidateStart)) {
            continue;
        }

        if (forward) {
            selectedMatch = candidate;
            selectedStart = candidateStart;
            selectedEnd = candidateEnd;
            break;
        }

        if (candidateStart >= selectedStart) {
            selectedMatch = candidate;
            selectedStart = candidateStart;
            selectedEnd = candidateEnd;
        }
    }

    if (!selectedMatch.hasMatch()) {
        *length = 0;
        return -1;
    }

    match = selectedMatch;
    *length = selectedEnd - selectedStart;
    return selectedStart;
}

const char *QRegexSearch::SubstituteByPosition(Document *doc, const char *text, Sci::Position *length)
{
    Q_UNUSED(doc);

    if (length == Q_NULLPTR || text == Q_NULLPTR || !match.isValid() || !match.hasMatch()) {
        if (length) {
            *length = 0;
        }
        return Q_NULLPTR;
    }

    const QString replacement = QString::fromUtf8(text, static_cast<qsizetype>(*length));
    const QString newString = expandReplacement(match, replacement);

    if (substituted) {
        delete substituted;
    }

    substituted = new QByteArray(newString.toUtf8());
    *length = substituted->length();
    return substituted->data();
}
