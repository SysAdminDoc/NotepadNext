/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Minimap.h"

#include "ScintillaNext.h"

#include <QEvent>
#include <QPainter>

#include "Scintilla.h"

namespace
{
constexpr int MinimapWidth = 110;
constexpr int MinimapRightPadding = 5;
constexpr int MinimapLineHeight = 2;
constexpr int MinimapTextScale = 3;
constexpr int MinimapDiffWidth = 3;
}

Minimap::Minimap(ScintillaNext *editor)
    : QWidget(editor->viewport()), editor(editor)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("minimap"));
    setFixedWidth(MinimapWidth);
    setAutoFillBackground(false);

    editor->viewport()->installEventFilter(this);
    connect(editor, &ScintillaNext::updateUi, this, &Minimap::editorUpdated);
    connect(editor, &ScintillaNext::notify, this, &Minimap::editorModified);
    connect(editor, &ScintillaNext::savePointChanged, this, &Minimap::savePointChanged);
    connect(editor, &ScintillaNext::reloaded, this, &Minimap::editorReloaded);
    connect(editor, &ScintillaNext::resized, this, &Minimap::updateGeometryForViewport);

    updateGeometryForViewport();
    show();
}

bool Minimap::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == editor->viewport() && event->type() == QEvent::Resize) {
        updateGeometryForViewport();
    }
    return QWidget::eventFilter(watched, event);
}

void Minimap::editorUpdated(Scintilla::Update updated)
{
    if (Scintilla::FlagSet(updated, Scintilla::Update::Content) ||
        Scintilla::FlagSet(updated, Scintilla::Update::Selection) ||
        Scintilla::FlagSet(updated, Scintilla::Update::VScroll)) {
        update();
    }
}

void Minimap::editorModified(const Scintilla::NotificationData *notification)
{
    if (!notification || notification->nmhdr.code != Scintilla::Notification::Modified) {
        return;
    }
    if (Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::InsertText) ||
        Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::DeleteText)) {
        markModifiedLines(notification);
        update();
    }
}

void Minimap::savePointChanged(bool dirty)
{
    if (!dirty) {
        modifiedLines.clear();
        update();
    }
}

void Minimap::editorReloaded()
{
    modifiedLines.clear();
    update();
}

void Minimap::updateGeometryForViewport()
{
    if (!editor || !editor->viewport()) {
        return;
    }

    setGeometry(qMax(0, editor->viewport()->width() - width()), 0, width(), editor->viewport()->height());
    raise();
    update();
}

void Minimap::markModifiedLines(const Scintilla::NotificationData *notification)
{
    const int lineCount = static_cast<int>(editor->lineCount());
    if (lineCount <= 0) {
        return;
    }

    const int changeLine = qBound(0, static_cast<int>(notification->line), lineCount - 1);
    const int linesChanged = qMax(1, static_cast<int>(notification->linesAdded) + 1);
    const int lastLine = qMin(lineCount - 1, changeLine + linesChanged);
    for (int line = changeLine; line <= lastLine; ++line) {
        modifiedLines.insert(line);
    }
}

int Minimap::yForLine(int line) const
{
    const int lineCount = qMax(1, static_cast<int>(editor->lineCount()));
    return qBound(0, (line * height()) / lineCount, qMax(0, height() - 1));
}

void Minimap::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    QColor background = palette().color(QPalette::Base);
    background.setAlpha(235);
    painter.fillRect(rect(), background);

    const int lineCount = qMax(1, static_cast<int>(editor->lineCount()));
    const int visibleStart = qBound(0, static_cast<int>(editor->docLineFromVisible(editor->firstVisibleLine())), lineCount - 1);
    const int visibleEnd = qBound(visibleStart, static_cast<int>(editor->docLineFromVisible(editor->firstVisibleLine() + editor->linesOnScreen())), lineCount - 1);

    QColor viewportColor = palette().color(QPalette::Highlight);
    viewportColor.setAlpha(55);
    painter.fillRect(0, yForLine(visibleStart), width(), qMax(3, yForLine(visibleEnd) - yForLine(visibleStart)), viewportColor);

    const QColor textColor = palette().color(QPalette::Text);
    const QColor diffColor = QColor(0xD0, 0x60, 0x30);
    const int drawableWidth = width() - MinimapRightPadding - MinimapDiffWidth - 2;

    for (int y = 0; y < height(); y += MinimapLineHeight) {
        const int firstLine = (y * lineCount) / qMax(1, height());
        const int nextLine = qMin(lineCount, ((y + MinimapLineHeight) * lineCount) / qMax(1, height()) + 1);
        int longestLine = 0;
        bool hasDiff = false;
        for (int line = firstLine; line < nextLine; ++line) {
            longestLine = qMax(longestLine, static_cast<int>(editor->getLine(line).trimmed().size()));
            hasDiff = hasDiff || modifiedLines.contains(line);
        }

        if (longestLine > 0) {
            const int lineWidth = qBound(3, qMin(drawableWidth, 3 + longestLine / MinimapTextScale), drawableWidth);
            painter.setPen(textColor);
            painter.drawLine(width() - MinimapRightPadding - lineWidth, y,
                             width() - MinimapRightPadding, y);
        }
        if (hasDiff) {
            painter.fillRect(0, y, MinimapDiffWidth, MinimapLineHeight, diffColor);
        }
    }
}
