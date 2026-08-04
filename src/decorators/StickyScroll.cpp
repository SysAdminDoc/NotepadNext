/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "StickyScroll.h"

#include "ScintillaNext.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMap>
#include <QPainter>

#include "Scintilla.h"

namespace
{
constexpr int MaximumStickyRows = 3;
constexpr int StickyRowPadding = 3;
}

StickyScroll::StickyScroll(ScintillaNext *editor)
    : QWidget(editor->viewport()), editor(editor)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("stickyScroll"));
    setAutoFillBackground(false);

    editor->viewport()->installEventFilter(this);
    connect(editor, &ScintillaNext::updateUi, this, &StickyScroll::editorUpdated);
    connect(editor, &ScintillaNext::lexerChanged, this, &StickyScroll::refresh);
    connect(editor, &ScintillaNext::reloaded, this, &StickyScroll::refresh);
    connect(editor, &ScintillaNext::resized, this, &StickyScroll::updateGeometryForViewport);

    updateGeometryForViewport();
    show();
    refresh();
}

bool StickyScroll::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == editor->viewport() && event->type() == QEvent::Resize) {
        updateGeometryForViewport();
    }
    return QWidget::eventFilter(watched, event);
}

void StickyScroll::editorUpdated(Scintilla::Update updated)
{
    if (Scintilla::FlagSet(updated, Scintilla::Update::Content) ||
        Scintilla::FlagSet(updated, Scintilla::Update::VScroll)) {
        refresh();
    }
}

void StickyScroll::refresh()
{
    const QVector<QString> newLabels = currentLabels();
    if (newLabels == labels) {
        return;
    }

    labels = newLabels;
    const int rowHeight = QFontMetrics(font()).height() + StickyRowPadding * 2;
    setFixedHeight(qMax(1, rowHeight * labels.size()));
    update();
}

void StickyScroll::updateGeometryForViewport()
{
    if (!editor || !editor->viewport()) {
        return;
    }

    setGeometry(0, 0, editor->viewport()->width(), height());
    raise();
}

QVector<QString> StickyScroll::currentLabels() const
{
    QVector<QString> result;
    const int lineCount = static_cast<int>(editor->lineCount());
    if (lineCount <= 0) {
        return result;
    }

    const int topLine = qBound(0, static_cast<int>(editor->docLineFromVisible(editor->firstVisibleLine())), lineCount - 1);
    const int topLevel = static_cast<int>(editor->foldLevel(topLine)) & SC_FOLDLEVELNUMBERMASK;
    QMap<int, int> latestHeaders;

    for (int line = 0; line < topLine; ++line) {
        const int foldLevel = static_cast<int>(editor->foldLevel(line));
        if ((foldLevel & SC_FOLDLEVELHEADERFLAG) == 0) {
            continue;
        }

        const int level = foldLevel & SC_FOLDLEVELNUMBERMASK;
        for (auto it = latestHeaders.begin(); it != latestHeaders.end();) {
            if (it.key() >= level) {
                it = latestHeaders.erase(it);
            }
            else {
                ++it;
            }
        }
        latestHeaders.insert(level, line);
    }

    for (auto it = latestHeaders.cbegin(); it != latestHeaders.cend(); ++it) {
        if (it.key() >= topLevel) {
            continue;
        }

        QString label = QString::fromUtf8(editor->getLine(it.value())).trimmed();
        if (!label.isEmpty()) {
            result.append(label);
        }
    }

    if (result.size() > MaximumStickyRows) {
        result = result.mid(result.size() - MaximumStickyRows);
    }
    return result;
}

void StickyScroll::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (labels.isEmpty()) {
        return;
    }

    QPainter painter(this);
    const int rowHeight = QFontMetrics(font()).height() + StickyRowPadding * 2;
    QColor background = palette().color(QPalette::Base);
    background.setAlpha(248);
    painter.fillRect(rect(), background);

    const QColor textColor = palette().color(QPalette::Text);
    painter.setPen(textColor);
    const QFontMetrics metrics(font());
    for (int row = 0; row < labels.size(); ++row) {
        const QRect rowRect(8, row * rowHeight, width() - 16, rowHeight);
        const QString label = metrics.elidedText(labels.at(row), Qt::ElideRight, qMax(0, rowRect.width()));
        painter.drawText(rowRect, Qt::AlignVCenter | Qt::AlignLeft, label);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawLine(rowRect.left(), rowRect.bottom(), rowRect.right(), rowRect.bottom());
        painter.setPen(textColor);
    }
}
