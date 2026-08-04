/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CommandPaletteDialog.h"

#include "CommandPaletteFilter.h"

#include <QAction>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QShowEvent>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
struct ActionMatch
{
    QAction *action;
    int score;
};
}

CommandPaletteDialog::CommandPaletteDialog(QWidget *parent, const QList<QAction *> &actions) :
    QDialog(parent),
    queryEdit(new QLineEdit(this)),
    results(new QListWidget(this)),
    actions(actions)
{
    setWindowTitle(tr("Command Palette"));
    setModal(true);
    resize(640, 480);

    auto *layout = new QVBoxLayout(this);
    auto *prompt = new QLabel(tr("Search commands"), this);
    layout->addWidget(prompt);
    layout->addWidget(queryEdit);
    layout->addWidget(results);

    queryEdit->setObjectName(QStringLiteral("commandPaletteQuery"));
    queryEdit->setAccessibleName(tr("Command search"));
    queryEdit->setAccessibleDescription(tr("Type a command name or shortcut. Use Up and Down to choose a result, then press Enter."));
    queryEdit->setPlaceholderText(tr("Type a command name or shortcut"));
    prompt->setBuddy(queryEdit);

    results->setObjectName(QStringLiteral("commandPaletteResults"));
    results->setAccessibleName(tr("Command results"));
    results->setAccessibleDescription(tr("Matching commands. Press Enter to run the selected command."));
    results->setSelectionMode(QAbstractItemView::SingleSelection);
    results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results->setAlternatingRowColors(true);
    queryEdit->installEventFilter(this);

    connect(queryEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::updateResults);
    connect(queryEdit, &QLineEdit::returnPressed, this, &CommandPaletteDialog::triggerCurrent);
    connect(results, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        triggerCurrent();
    });

    updateResults();
}

bool CommandPaletteDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == queryEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool moveDown = keyEvent->key() == Qt::Key_Down || keyEvent->key() == Qt::Key_PageDown;
        const bool moveUp = keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_PageUp;
        if ((moveDown || moveUp) && results->count() > 0
            && !keyEvent->modifiers().testFlag(Qt::ControlModifier)
            && !keyEvent->modifiers().testFlag(Qt::AltModifier)
            && !keyEvent->modifiers().testFlag(Qt::MetaModifier)) {
            const int currentRow = qMax(0, results->currentRow());
            const int step = keyEvent->key() == Qt::Key_PageDown || keyEvent->key() == Qt::Key_PageUp ? 5 : 1;
            const int nextRow = moveDown
                ? qMin(results->count() - 1, currentRow + step)
                : qMax(0, currentRow - step);
            results->setCurrentRow(nextRow);
            results->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }

    return QDialog::eventFilter(watched, event);
}

void CommandPaletteDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    queryEdit->setFocus();
    queryEdit->selectAll();
}

QString CommandPaletteDialog::actionLabel(const QAction *action)
{
    QString label = action->text();
    label.remove('&');

    const QKeySequence shortcut = action->shortcut();
    if (!shortcut.isEmpty()) {
        label += QStringLiteral("    [")
                + shortcut.toString(QKeySequence::NativeText)
                + QLatin1Char(']');
    }

    return label;
}

void CommandPaletteDialog::updateResults()
{
    actionForItem.clear();
    results->clear();

    QVector<ActionMatch> matches;
    const QString query = queryEdit->text();
    QSet<QAction *> seen;

    for (QAction *action : actions) {
        if (action == nullptr || seen.contains(action) || action->isSeparator()
                || !action->isEnabled() || action->text().trimmed().isEmpty()
                || action->objectName() == QStringLiteral("actionCommandPalette")) {
            continue;
        }

        seen.insert(action);
        const int matchScore = CommandPaletteFilter::score(query, action->text());
        if (matchScore >= 0) {
            matches.append({action, matchScore});
        }
    }

    std::stable_sort(matches.begin(), matches.end(), [](const ActionMatch &left, const ActionMatch &right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return QString::compare(left.action->text(), right.action->text(), Qt::CaseInsensitive) < 0;
    });

    for (const ActionMatch &match : matches) {
        auto *item = new QListWidgetItem(actionLabel(match.action), results);
        actionForItem.insert(item, match.action);
    }

    if (results->count() > 0) {
        results->setCurrentRow(0);
    }
}

void CommandPaletteDialog::triggerCurrent()
{
    QAction *action = actionForItem.value(results->currentItem(), nullptr);
    if (action == nullptr) {
        return;
    }

    accept();
    action->trigger();
}
