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

#include "RegexBuilderDock.h"

#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QCheckBox>
#include <QColor>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
QColor groupColor(int groupNumber)
{
    return QColor::fromHsv((groupNumber * 67) % 360, 115, 255, 100);
}

QString groupLabel(const RegexBuilder::Group &group)
{
    if (group.number == 0) {
        return QObject::tr("Whole match");
    }

    const QString name = group.name.isEmpty()
        ? QString()
        : QObject::tr(" (%1)").arg(group.name);
    return QObject::tr("Group %1%2").arg(group.number).arg(name);
}
}

RegexBuilderDock::RegexBuilderDock(MainWindow *window, QWidget *parent)
    : QDockWidget(tr("Regex Builder"), parent)
    , window(window)
{
    setObjectName(QStringLiteral("regexBuilderDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumHeight(320);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    auto *controls = new QGridLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setColumnStretch(1, 1);
    auto *patternLabel = new QLabel(tr("Pattern:"), container);

    patternEdit = new QLineEdit(container);
    patternEdit->setObjectName(QStringLiteral("regexBuilderPattern"));
    patternEdit->setAccessibleName(tr("Regular expression pattern"));
    patternEdit->setAccessibleDescription(tr("Enter a Qt regular expression to inspect."));
    patternEdit->setPlaceholderText(tr("Enter a regular expression"));
    patternLabel->setBuddy(patternEdit);
    controls->addWidget(patternLabel, 0, 0);
    controls->addWidget(patternEdit, 0, 1, 1, 3);

    auto *analyzeButton = new QPushButton(tr("Analyze"), container);
    analyzeButton->setObjectName(QStringLiteral("regexBuilderAnalyze"));
    analyzeButton->setAccessibleDescription(tr("Analyze the pattern and update its matches."));
    controls->addWidget(analyzeButton, 0, 4);

    auto *options = new QHBoxLayout;
    caseInsensitiveCheck = new QCheckBox(tr("Case insensitive"), container);
    caseInsensitiveCheck->setObjectName(QStringLiteral("regexBuilderCaseInsensitive"));
    caseInsensitiveCheck->setAccessibleDescription(tr("Ignore letter case while matching."));
    options->addWidget(caseInsensitiveCheck);
    dotMatchesCheck = new QCheckBox(tr("Dot matches newline"), container);
    dotMatchesCheck->setObjectName(QStringLiteral("regexBuilderDotMatches"));
    dotMatchesCheck->setAccessibleDescription(tr("Allow a dot in the pattern to match line breaks."));
    options->addWidget(dotMatchesCheck);
    options->addStretch();

    auto *useDocumentButton = new QPushButton(tr("Use Current Document"), container);
    useDocumentButton->setObjectName(QStringLiteral("regexBuilderUseDocument"));
    useDocumentButton->setAccessibleDescription(tr("Load the active document as sample text."));
    options->addWidget(useDocumentButton);
    auto *useSelectionButton = new QPushButton(tr("Use Selection"), container);
    useSelectionButton->setObjectName(QStringLiteral("regexBuilderUseSelection"));
    useSelectionButton->setAccessibleDescription(tr("Load the active selection as sample text."));
    options->addWidget(useSelectionButton);

    layout->addLayout(controls);
    layout->addLayout(options);

    statusLabel = new QLabel(container);
    statusLabel->setObjectName(QStringLiteral("regexBuilderStatus"));
    statusLabel->setAccessibleName(tr("Regex Builder status"));
    statusLabel->setAccessibleDescription(tr("Pattern validation and match summary."));
    layout->addWidget(statusLabel);

    auto *sampleLabel = new QLabel(tr("Sample text — matched groups are highlighted below:"), container);
    sampleLabel->setObjectName(QStringLiteral("regexBuilderSampleLabel"));
    layout->addWidget(sampleLabel);
    sampleEdit = new QPlainTextEdit(container);
    sampleEdit->setObjectName(QStringLiteral("regexBuilderSample"));
    sampleEdit->setAccessibleName(tr("Regular expression sample text"));
    sampleEdit->setAccessibleDescription(tr("Text used to evaluate the regular expression."));
    sampleLabel->setBuddy(sampleEdit);
    sampleEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    sampleEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    sampleEdit->setPlaceholderText(tr("Paste sample text or load the current document."));
    sampleEdit->setMinimumHeight(100);

    matches = new QTreeWidget(container);
    matches->setObjectName(QStringLiteral("regexBuilderMatches"));
    matches->setAccessibleName(tr("Regular expression matches"));
    matches->setAccessibleDescription(tr("Matches and capture groups with their values and offsets."));
    matches->setColumnCount(4);
    matches->setHeaderLabels({tr("Capture"), tr("Value"), tr("Start"), tr("Length")});
    matches->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    matches->setAlternatingRowColors(true);
    matches->setUniformRowHeights(true);
    matches->setRootIsDecorated(true);
    matches->header()->setStretchLastSection(false);
    matches->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    matches->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    matches->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    matches->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    auto *splitter = new QSplitter(Qt::Vertical, container);
    splitter->setObjectName(QStringLiteral("regexBuilderSplitter"));
    splitter->addWidget(sampleEdit);
    splitter->addWidget(matches);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    QWidget::setTabOrder(patternEdit, analyzeButton);
    QWidget::setTabOrder(analyzeButton, caseInsensitiveCheck);
    QWidget::setTabOrder(caseInsensitiveCheck, dotMatchesCheck);
    QWidget::setTabOrder(dotMatchesCheck, useDocumentButton);
    QWidget::setTabOrder(useDocumentButton, useSelectionButton);
    QWidget::setTabOrder(useSelectionButton, sampleEdit);
    QWidget::setTabOrder(sampleEdit, matches);

    setWidget(container);
    updateStatus(tr("Enter a pattern and sample text."));

    connect(patternEdit, &QLineEdit::textChanged, this, &RegexBuilderDock::analyzePattern);
    connect(patternEdit, &QLineEdit::returnPressed, this, &RegexBuilderDock::analyzePattern);
    connect(sampleEdit, &QPlainTextEdit::textChanged, this, &RegexBuilderDock::analyzePattern);
    connect(caseInsensitiveCheck, &QCheckBox::toggled, this, &RegexBuilderDock::analyzePattern);
    connect(dotMatchesCheck, &QCheckBox::toggled, this, &RegexBuilderDock::analyzePattern);
    connect(analyzeButton, &QPushButton::clicked, this, &RegexBuilderDock::analyzePattern);
    connect(useDocumentButton, &QPushButton::clicked, this, &RegexBuilderDock::loadCurrentDocument);
    connect(useSelectionButton, &QPushButton::clicked, this, &RegexBuilderDock::loadCurrentSelection);
}

QString RegexBuilderDock::pattern() const
{
    return patternEdit->text();
}

QString RegexBuilderDock::sampleText() const
{
    return sampleEdit->toPlainText();
}

int RegexBuilderDock::matchCount() const
{
    return result.matches.size();
}

const RegexBuilder::Result &RegexBuilderDock::lastResult() const
{
    return result;
}

void RegexBuilderDock::setPattern(const QString &value)
{
    patternEdit->setText(value);
}

void RegexBuilderDock::setSampleText(const QString &value)
{
    sampleEdit->setPlainText(value);
}

void RegexBuilderDock::focusPattern()
{
    patternEdit->setFocus();
    patternEdit->selectAll();
}

void RegexBuilderDock::analyzePattern()
{
    result = RegexBuilder::analyze(pattern(), sampleText(),
                                   caseInsensitiveCheck->isChecked(),
                                   dotMatchesCheck->isChecked());
    updateMatchTree();
    updateHighlights();

    if (!result.valid) {
        const QString offset = result.errorOffset >= 0
            ? tr(" at offset %1").arg(result.errorOffset)
            : QString();
        updateStatus(tr("Invalid pattern%1: %2").arg(offset, result.error), true);
    } else if (pattern().isEmpty()) {
        updateStatus(tr("Enter a pattern to inspect its capture groups."));
    } else if (result.matches.isEmpty()) {
        updateStatus(tr("No matches. %L1 capture groups defined.").arg(result.captureCount));
    } else {
        const QString suffix = result.truncated ? tr(" (showing first 500)") : QString();
        updateStatus(tr("%L1 matches, %L2 capture groups%3.")
                         .arg(result.matches.size())
                         .arg(result.captureCount)
                         .arg(suffix));
    }
}

void RegexBuilderDock::loadCurrentDocument()
{
    loadEditorText(false);
}

void RegexBuilderDock::loadCurrentSelection()
{
    loadEditorText(true);
}

void RegexBuilderDock::loadEditorText(bool selectionOnly)
{
    ScintillaNext *editor = window ? window->currentEditor() : nullptr;
    if (!editor) {
        updateStatus(tr("There is no active document."), true);
        return;
    }

    if (selectionOnly && editor->selectionEmpty()) {
        updateStatus(tr("Select text in the active document first."), true);
        return;
    }

    const QString text = selectionOnly
        ? QString::fromUtf8(editor->getSelText())
        : QString::fromUtf8(editor->getText(editor->textLength()));
    sampleEdit->setPlainText(text);
    analyzePattern();
}

void RegexBuilderDock::updateStatus(const QString &message, bool error)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString());
}

void RegexBuilderDock::updateMatchTree()
{
    matches->clear();
    for (const RegexBuilder::Match &match : result.matches) {
        auto *matchItem = new QTreeWidgetItem(matches);
        matchItem->setText(0, tr("Match %1").arg(match.number));
        matchItem->setText(1, match.value);
        matchItem->setText(2, QString::number(match.start));
        matchItem->setText(3, QString::number(match.value.size()));
        matchItem->setToolTip(1, match.value);
        matchItem->setExpanded(true);

        for (const RegexBuilder::Group &group : match.groups) {
            auto *groupItem = new QTreeWidgetItem(matchItem);
            groupItem->setText(0, groupLabel(group));
            groupItem->setText(1, group.matched() ? group.value : tr("<not matched>"));
            groupItem->setText(2, group.matched() ? QString::number(group.start) : QStringLiteral("-"));
            groupItem->setText(3, group.matched() ? QString::number(group.length) : QStringLiteral("-"));
            groupItem->setToolTip(1, group.value);
            groupItem->setBackground(0, groupColor(group.number));
        }
    }
}

void RegexBuilderDock::updateHighlights()
{
    QList<QTextEdit::ExtraSelection> selections;
    for (const RegexBuilder::Match &match : result.matches) {
        for (const RegexBuilder::Group &group : match.groups) {
            if (!group.matched() || group.length <= 0) {
                continue;
            }

            QTextCursor cursor(sampleEdit->document());
            cursor.setPosition(group.start);
            cursor.setPosition(group.start + group.length, QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(groupColor(group.number));
            selections.append(selection);
        }
    }
    sampleEdit->setExtraSelections(selections);
}
