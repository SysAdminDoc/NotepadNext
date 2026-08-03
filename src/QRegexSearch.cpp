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

    if (doc == Q_NULLPTR || length == Q_NULLPTR || s == Q_NULLPTR) {
        return -1;
    }

    const bool forward = minPos <= maxPos;
    Sci::Position rangeStart = forward ? minPos : maxPos;
    Sci::Position rangeEnd = forward ? maxPos : minPos;

    rangeStart = qBound<Sci::Position>(0, rangeStart, doc->Length());
    rangeEnd = qBound<Sci::Position>(0, rangeEnd, doc->Length());
    rangeStart = doc->MovePositionOutsideChar(rangeStart, 1, false);
    rangeEnd = doc->MovePositionOutsideChar(rangeEnd, -1, false);

    if (rangeStart > rangeEnd) {
        return -1;
    }

    auto options = QRegularExpression::MultilineOption
            | QRegularExpression::UseUnicodePropertiesOption;

    if (!FlagSet(flags, FindOption::MatchCase))
        options |= QRegularExpression::CaseInsensitiveOption;

    if (FlagSet(flags, FindOption::Cxx11RegEx))
        options |= QRegularExpression::DotMatchesEverythingOption;

    // TODO: does (*ANYCRLF) need prepended to the search string?
    QRegularExpression re(s, options);
    if (!re.isValid())
        return -1; // Invalid regular expression

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
        return -1;
    }

    match = selectedMatch;
    *length = selectedEnd - selectedStart;
    return selectedStart;
}

const char *QRegexSearch::SubstituteByPosition(Document *doc, const char *text, Sci::Position *length)
{
    Q_UNUSED(doc);

    qInfo(Q_FUNC_INFO);

    Q_ASSERT(match.isValid());
    Q_ASSERT(match.hasMatch());

    // Get the captured text and replace the match
    QString newString = match.captured();
    newString.replace(match.regularExpression(), QByteArray(text, *length));

    // TODO: figure out why this has to be new'd and can't be an instantiated class member
    if (substituted) {
        delete substituted;
    }

    substituted = new QByteArray(newString.toUtf8());
    *length = substituted->length();
    return substituted->data();
}
