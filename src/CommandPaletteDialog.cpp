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

    queryEdit->setPlaceholderText(tr("Type a command name or shortcut"));
    results->setSelectionMode(QAbstractItemView::SingleSelection);
    results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results->setAlternatingRowColors(true);

    connect(queryEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::updateResults);
    connect(queryEdit, &QLineEdit::returnPressed, this, &CommandPaletteDialog::triggerCurrent);
    connect(results, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        triggerCurrent();
    });

    updateResults();
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
