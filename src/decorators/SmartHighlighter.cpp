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


#include "SmartHighlighter.h"

using namespace Scintilla;


SmartHighlighter::SmartHighlighter(ScintillaNext *editor) :
    EditorDecorator(editor)
{
    setObjectName("SmartHighlighter");

    indicator = editor->allocateIndicator("smart_highlighter");

    editor->indicSetFore(indicator, 0x00FF00);
    editor->indicSetStyle(indicator, INDIC_ROUNDBOX);
    editor->indicSetOutlineAlpha(indicator, 150);
    editor->indicSetAlpha(indicator, 100);
    editor->indicSetUnder(indicator, true);
}

void SmartHighlighter::notify(const NotificationData *pscn)
{
    if (pscn->nmhdr.code == Notification::UpdateUI
        && (FlagSet(pscn->updated, Update::Content)
            || FlagSet(pscn->updated, Update::Selection)
            || (editor->isLargeFileMode() && FlagSet(pscn->updated, Update::VScroll)))) {
        highlightCurrentView();
    }
}

void SmartHighlighter::highlightCurrentView()
{
    editor->setIndicatorCurrent(indicator);

    const bool largeFileMode = editor->isLargeFileMode();
    if (largeFileMode) {
        if (highlightedStart >= 0 && highlightedEnd > highlightedStart) {
            editor->indicatorClearRange(highlightedStart, highlightedEnd - highlightedStart);
        }
    }
    else {
        editor->indicatorClearRange(0, editor->length());
        highlightedStart = -1;
        highlightedEnd = -1;
    }

    if (editor->selectionEmpty()) {
        highlightedStart = -1;
        highlightedEnd = -1;
        return;
    }

    const int mainSelection = editor->mainSelection();
    const int selectionStart = editor->selectionNStart(mainSelection);
    const int selectionEnd = editor->selectionNEnd(mainSelection);

    // Make sure the current selection is valid
    if (selectionStart == selectionEnd) {
        highlightedStart = -1;
        highlightedEnd = -1;
        return;
    }

    const int curPos = editor->currentPos();
    const int wordStart = editor->wordStartPosition(curPos, true);
    const int wordEnd = editor->wordEndPosition(wordStart, true);

    // Make sure the selection is on word boundaries
    if (wordStart == wordEnd || wordStart != selectionStart || wordEnd != selectionEnd) {
        highlightedStart = -1;
        highlightedEnd = -1;
        return;
    }

    const QByteArray selText = editor->get_text_range(selectionStart, selectionEnd);
    const int flags = SCFIND_MATCHCASE | SCFIND_WHOLEWORD;

    if (!largeFileMode) {
        Sci_TextToFind ttf {{0, (Sci_PositionCR)editor->length()}, selText.constData(), {-1, -1}};
        while (editor->send(SCI_FINDTEXT, flags, (sptr_t)&ttf) != -1) {
            editor->indicatorFillRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax - ttf.chrgText.cpMin);
            ttf.chrg.cpMin = ttf.chrgText.cpMax;
        }
        return;
    }

    const int lineCount = static_cast<int>(editor->lineCount());
    const int firstVisibleLine = qMax(0, editor->firstVisibleLine());
    const int firstLine = qBound(0, static_cast<int>(editor->docLineFromVisible(firstVisibleLine)), qMax(0, lineCount - 1));
    const int lastVisibleLine = firstVisibleLine + qMax(1, editor->linesOnScreen()) + 1;
    const int endLine = qMin(lineCount, static_cast<int>(editor->docLineFromVisible(lastVisibleLine)) + 1);
    highlightedStart = editor->positionFromLine(firstLine);
    highlightedEnd = endLine >= lineCount ? editor->length() : editor->positionFromLine(endLine);

    for (int line = firstLine; line < endLine;) {
        if (!editor->lineVisible(line)) {
            line = qMax(line + 1, editor->lastChild(line, -1) + 1);
            continue;
        }

        const int lineStart = editor->positionFromLine(line);
        const int lineEnd = editor->lineEndPosition(line);
        Sci_TextToFind ttf {{lineStart, lineEnd}, selText.constData(), {-1, -1}};
        while (editor->send(SCI_FINDTEXT, flags, (sptr_t)&ttf) != -1) {
            editor->indicatorFillRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax - ttf.chrgText.cpMin);
            ttf.chrg.cpMin = ttf.chrgText.cpMax;
        }
        ++line;
    }
}
