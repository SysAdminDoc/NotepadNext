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

#include "SnippetManagerDock.h"

#include "ApplicationSettings.h"
#include "MainWindow.h"
#include "ScintillaNext.h"
#include "SnippetEngine.h"

#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

namespace
{
constexpr int SnippetRole = Qt::UserRole;

bool isDuplicate(const QVector<SnippetDefinition> &snippets,
                 const QString &name,
                 const QString &trigger,
                 int ignoredIndex)
{
    for (int index = 0; index < snippets.size(); ++index) {
        if (index == ignoredIndex) {
            continue;
        }

        const SnippetDefinition &snippet = snippets.at(index);
        if (snippet.name.compare(name, Qt::CaseInsensitive) == 0
            || snippet.trigger.compare(trigger, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
}

SnippetManagerDock::SnippetManagerDock(MainWindow *window, QWidget *parent)
    : QDockWidget(tr("Snippet Manager"), parent)
    , window(window)
{
    setObjectName(QStringLiteral("snippetManagerDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumHeight(360);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    auto *filterLabel = new QLabel(tr("Filter:"), container);
    filterEdit = new QLineEdit(container);
    filterEdit->setObjectName(QStringLiteral("snippetFilter"));
    filterEdit->setAccessibleName(tr("Filter snippets"));
    filterEdit->setPlaceholderText(tr("Filter by name or trigger"));
    filterLabel->setBuddy(filterEdit);

    auto *filterLayout = new QHBoxLayout;
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(filterEdit, 1);
    layout->addLayout(filterLayout);

    snippetList = new QListWidget(container);
    snippetList->setObjectName(QStringLiteral("snippetList"));
    snippetList->setAccessibleName(tr("Available snippets"));
    snippetList->setAccessibleDescription(tr("Select a snippet with the arrow keys and press Enter to insert it."));
    snippetList->setMinimumWidth(220);

    auto *editor = new QWidget(container);
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(5);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    nameEdit = new QLineEdit(editor);
    nameEdit->setObjectName(QStringLiteral("snippetName"));
    nameEdit->setAccessibleName(tr("Snippet name"));
    form->addRow(tr("Name:"), nameEdit);

    triggerEdit = new QLineEdit(editor);
    triggerEdit->setObjectName(QStringLiteral("snippetTrigger"));
    triggerEdit->setAccessibleName(tr("Snippet trigger"));
    triggerEdit->setPlaceholderText(tr("Type this word, then press Tab"));
    form->addRow(tr("Trigger:"), triggerEdit);
    editorLayout->addLayout(form);

    auto *bodyLabel = new QLabel(tr("Body (use ${1:default}, ${2}, and ${0}):"), editor);
    bodyLabel->setObjectName(QStringLiteral("snippetBodyLabel"));
    editorLayout->addWidget(bodyLabel);

    bodyEdit = new QPlainTextEdit(editor);
    bodyEdit->setObjectName(QStringLiteral("snippetBody"));
    bodyEdit->setAccessibleName(tr("Snippet body"));
    bodyEdit->setAccessibleDescription(tr("Snippet template. Use Ctrl+Tab to leave this editor; a Tab without Ctrl inserts indentation."));
    bodyEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    bodyEdit->setMinimumHeight(170);
    bodyEdit->setTabChangesFocus(false);
    editorLayout->addWidget(bodyEdit, 1);

    auto *buttonLayout = new QHBoxLayout;
    auto *newButton = new QPushButton(tr("New"), editor);
    newButton->setObjectName(QStringLiteral("snippetNew"));
    auto *saveButton = new QPushButton(tr("Save"), editor);
    saveButton->setObjectName(QStringLiteral("snippetSave"));
    saveButton->setDefault(true);
    auto *deleteButton = new QPushButton(tr("Delete"), editor);
    deleteButton->setObjectName(QStringLiteral("snippetDelete"));
    auto *insertButton = new QPushButton(tr("Insert"), editor);
    insertButton->setObjectName(QStringLiteral("snippetInsert"));
    buttonLayout->addWidget(newButton);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(insertButton);
    editorLayout->addLayout(buttonLayout);

    auto *splitter = new QSplitter(Qt::Horizontal, container);
    splitter->setObjectName(QStringLiteral("snippetSplitter"));
    splitter->addWidget(snippetList);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    statusLabel = new QLabel(container);
    statusLabel->setObjectName(QStringLiteral("snippetStatus"));
    statusLabel->setAccessibleName(tr("Snippet manager status"));
    statusLabel->setAccessibleDescription(tr("Validation and insertion messages."));
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    QWidget::setTabOrder(filterEdit, snippetList);
    QWidget::setTabOrder(snippetList, nameEdit);
    QWidget::setTabOrder(nameEdit, triggerEdit);
    QWidget::setTabOrder(triggerEdit, bodyEdit);
    QWidget::setTabOrder(bodyEdit, newButton);
    QWidget::setTabOrder(newButton, saveButton);
    QWidget::setTabOrder(saveButton, deleteButton);
    QWidget::setTabOrder(deleteButton, insertButton);

    setWidget(container);

    bodyEdit->installEventFilter(this);

    loadSettings();
    refreshList();

    connect(filterEdit, &QLineEdit::textChanged, this, &SnippetManagerDock::refreshList);
    connect(snippetList, &QListWidget::currentRowChanged, this, &SnippetManagerDock::selectSnippet);
    connect(snippetList, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        insertSelectedSnippet();
    });
    connect(newButton, &QPushButton::clicked, this, &SnippetManagerDock::newSnippet);
    connect(saveButton, &QPushButton::clicked, this, &SnippetManagerDock::saveSnippet);
    connect(deleteButton, &QPushButton::clicked, this, &SnippetManagerDock::deleteSnippet);
    connect(insertButton, &QPushButton::clicked, this, &SnippetManagerDock::insertSelectedSnippet);
}

QVector<SnippetDefinition> SnippetManagerDock::defaultSnippets()
{
    return {
        {QObject::tr("Conditional block"), QStringLiteral("if"), QStringLiteral("if (${1:condition}) {\n\t${2}\n}\n${0}")},
        {QObject::tr("C-style loop"), QStringLiteral("for"), QStringLiteral("for (int ${1:i} = 0; ${1:i} < ${2:count}; ++${1:i}) {\n\t${3}\n}\n${0}")},
        {QObject::tr("JavaScript function"), QStringLiteral("function"), QStringLiteral("function ${1:name}(${2:args}) {\n\t${3}\n}\n${0}")},
        {QObject::tr("Try/catch block"), QStringLiteral("try"), QStringLiteral("try {\n\t${1}\n} catch (${2:Error} &${3:error}) {\n\t${4}\n}\n${0}")}
    };
}

void SnippetManagerDock::loadSettings()
{
    ApplicationSettings settings;
    const bool hasSavedSnippets = settings.contains(QStringLiteral("Snippets/size"));
    const int size = settings.beginReadArray(QStringLiteral("Snippets"));
    for (int index = 0; index < size; ++index) {
        settings.setArrayIndex(index);
        const QString name = settings.value(QStringLiteral("Name")).toString().trimmed();
        const QString trigger = settings.value(QStringLiteral("Trigger")).toString().trimmed();
        const QString body = settings.value(QStringLiteral("Body")).toString();
        if (!name.isEmpty() && !trigger.isEmpty() && !isDuplicate(snippets, name, trigger, -1)) {
            snippets.append({name, trigger, body});
        }
    }
    settings.endArray();

    if (!hasSavedSnippets) {
        snippets = defaultSnippets();
        saveSettings();
    }
}

void SnippetManagerDock::saveSettings() const
{
    ApplicationSettings settings;
    settings.remove(QStringLiteral("Snippets"));
    settings.beginWriteArray(QStringLiteral("Snippets"));
    for (int index = 0; index < snippets.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("Name"), snippets.at(index).name);
        settings.setValue(QStringLiteral("Trigger"), snippets.at(index).trigger);
        settings.setValue(QStringLiteral("Body"), snippets.at(index).body);
    }
    settings.endArray();
    settings.sync();
}

void SnippetManagerDock::refreshList()
{
    const int previousIndex = editingIndex;
    const QString filter = filterEdit ? filterEdit->text().trimmed() : QString();
    QSignalBlocker blocker(snippetList);
    snippetList->clear();

    int rowForPrevious = -1;
    for (int index = 0; index < snippets.size(); ++index) {
        const SnippetDefinition &snippet = snippets.at(index);
        if (!filter.isEmpty()
            && !snippet.name.contains(filter, Qt::CaseInsensitive)
            && !snippet.trigger.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        auto *item = new QListWidgetItem(tr("%1 — %2").arg(snippet.trigger, snippet.name), snippetList);
        item->setData(SnippetRole, index);
        item->setToolTip(snippet.body);
        if (index == previousIndex) {
            rowForPrevious = snippetList->count() - 1;
        }
    }

    int row = rowForPrevious;
    if (row < 0 && snippetList->count() > 0) {
        row = 0;
    }
    snippetList->setCurrentRow(row);
    blocker.unblock();
    selectSnippet(row);
}

void SnippetManagerDock::selectSnippet(int row)
{
    if (row < 0 || !snippetList->item(row)) {
        editingIndex = -1;
        clearEditorFields();
        return;
    }

    const int index = snippetList->item(row)->data(SnippetRole).toInt();
    if (index < 0 || index >= snippets.size()) {
        editingIndex = -1;
        clearEditorFields();
        return;
    }

    editingIndex = index;
    updateEditorFields(snippets.at(index));
}

void SnippetManagerDock::updateEditorFields(const SnippetDefinition &snippet)
{
    nameEdit->setText(snippet.name);
    triggerEdit->setText(snippet.trigger);
    bodyEdit->setPlainText(snippet.body);
}

void SnippetManagerDock::clearEditorFields()
{
    nameEdit->clear();
    triggerEdit->clear();
    bodyEdit->clear();
}

void SnippetManagerDock::showStatus(const QString &message, bool error)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString());
}

void SnippetManagerDock::newSnippet()
{
    editingIndex = -1;
    snippetList->clearSelection();
    clearEditorFields();
    showStatus(tr("Enter a name, a trigger, and a body."));
    nameEdit->setFocus();
}

void SnippetManagerDock::saveSnippet()
{
    const QString name = nameEdit->text().trimmed();
    const QString trigger = triggerEdit->text().trimmed();
    const QString body = bodyEdit->toPlainText();

    if (name.isEmpty() || trigger.isEmpty()) {
        showStatus(tr("Name and trigger are required."), true);
        return;
    }
    if (trigger.contains(QRegularExpression(QStringLiteral("\\s")))) {
        showStatus(tr("The trigger must be a single word without spaces."), true);
        return;
    }
    if (isDuplicate(snippets, name, trigger, editingIndex)) {
        showStatus(tr("Snippet names and triggers must be unique."), true);
        return;
    }

    const SnippetDefinition snippet{name, trigger, body};
    if (editingIndex >= 0 && editingIndex < snippets.size()) {
        snippets[editingIndex] = snippet;
    }
    else {
        snippets.append(snippet);
        editingIndex = snippets.size() - 1;
    }

    saveSettings();
    refreshList();
    showStatus(tr("Saved snippet “%1”.").arg(name));
}

void SnippetManagerDock::deleteSnippet()
{
    if (editingIndex < 0 || editingIndex >= snippets.size()) {
        showStatus(tr("Select a snippet to delete."), true);
        return;
    }

    const QString name = snippets.at(editingIndex).name;
    if (QMessageBox::question(this, tr("Delete Snippet"),
                              tr("Delete the “%1” snippet?").arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    snippets.removeAt(editingIndex);
    editingIndex = -1;
    saveSettings();
    refreshList();
    showStatus(tr("Deleted snippet “%1”.").arg(name));
}

void SnippetManagerDock::insertSelectedSnippet()
{
    if (editingIndex < 0 || editingIndex >= snippets.size()) {
        showStatus(tr("Select a snippet to insert."), true);
        return;
    }

    ScintillaNext *editor = window ? window->currentEditor() : nullptr;
    if (!editor) {
        showStatus(tr("There is no active document."), true);
        return;
    }

    const int selection = editor->mainSelection();
    const int start = editor->selectionNStart(selection);
    const int end = editor->selectionNEnd(selection);
    if (!insertSnippet(editor, snippets.at(editingIndex), start, end)) {
        return;
    }

    editor->grabFocus();
    showStatus(tr("Inserted “%1”. Press Tab to move through placeholders.").arg(snippets.at(editingIndex).name));
}

void SnippetManagerDock::focusFilter()
{
    filterEdit->setFocus();
    filterEdit->selectAll();
}

void SnippetManagerDock::attachEditor(ScintillaNext *editor)
{
    if (!editor || attachedEditors.contains(editor)) {
        return;
    }

    attachedEditors.insert(editor);
    editor->installEventFilter(this);
    connect(editor, &ScintillaNext::closed, this, [this, editor]() {
        activeSessions.remove(editor);
        attachedEditors.remove(editor);
    });
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        activeSessions.remove(editor);
        attachedEditors.remove(editor);
    });
}

bool SnippetManagerDock::insertSnippet(ScintillaNext *editor,
                                       const SnippetDefinition &snippet,
                                       int start,
                                       int end)
{
    if (!editor || editor->readOnly() || start < 0 || end < start) {
        showStatus(tr("The active document cannot be edited."), true);
        return false;
    }

    const SnippetEngine::Expansion expansion = SnippetEngine::expand(snippet.body);
    const QByteArray expandedBytes = expansion.text.toUtf8();
    editor->beginUndoAction();
    editor->setTargetRange(start, end);
    editor->replaceTarget(expandedBytes.size(), expandedBytes.constData());
    editor->endUndoAction();

    ActiveSession session;
    session.navigationSlots.reserve(expansion.placeholders.size());
    for (const SnippetEngine::Placeholder &placeholder : expansion.placeholders) {
        session.navigationSlots.append({placeholder.number, start + placeholder.start, placeholder.length});
    }

    if (session.navigationSlots.isEmpty()) {
        activeSessions.remove(editor);
        editor->setEmptySelection(start + expandedBytes.size());
    }
    else {
        activeSessions.insert(editor, session);
        selectSlot(editor, session.navigationSlots.constFirst());
    }
    return true;
}

bool SnippetManagerDock::tryExpandTrigger(ScintillaNext *editor)
{
    if (!editor || editor->readOnly() || editor->selections() != 1 || !editor->selectionEmpty()) {
        return false;
    }

    const int caret = editor->currentPos();
    const int start = editor->wordStartPosition(caret, true);
    if (start >= caret) {
        return false;
    }

    const QString trigger = QString::fromUtf8(editor->get_text_range(start, caret));
    for (const SnippetDefinition &snippet : snippets) {
        if (snippet.trigger == trigger) {
            return insertSnippet(editor, snippet, start, caret);
        }
    }
    return false;
}

void SnippetManagerDock::selectSlot(ScintillaNext *editor, const ActiveSlot &slot)
{
    editor->setSelection(slot.start + slot.length, slot.start);
}

bool SnippetManagerDock::updateCurrentSlot(ScintillaNext *editor, ActiveSession *session)
{
    if (!session || session->currentIndex < 0 || session->currentIndex >= session->navigationSlots.size()
        || editor->selections() != 1) {
        return false;
    }

    ActiveSlot &slot = session->navigationSlots[session->currentIndex];
    const int selectionStart = editor->selectionNStart(0);
    const int selectionEnd = editor->selectionNEnd(0);
    const int oldEnd = slot.start + slot.length;
    int newLength = slot.length;

    if (selectionStart == slot.start && selectionEnd == oldEnd) {
        newLength = slot.length;
    }
    else if (selectionStart == selectionEnd
             && editor->currentPos() >= slot.start
             && editor->currentPos() <= editor->textLength()) {
        newLength = editor->currentPos() - slot.start;
    }
    else if (selectionStart >= slot.start && selectionEnd <= oldEnd) {
        newLength = slot.length;
    }
    else {
        return false;
    }

    const int delta = newLength - slot.length;
    slot.length = newLength;
    for (int index = 0; index < session->navigationSlots.size(); ++index) {
        if (index != session->currentIndex
            && session->navigationSlots[index].start >= oldEnd) {
            session->navigationSlots[index].start += delta;
        }
    }
    return true;
}

bool SnippetManagerDock::handleTab(ScintillaNext *editor, bool backwards)
{
    auto sessionIterator = activeSessions.find(editor);
    if (sessionIterator == activeSessions.end()) {
        return tryExpandTrigger(editor);
    }

    ActiveSession &session = sessionIterator.value();
    if (!updateCurrentSlot(editor, &session)) {
        activeSessions.erase(sessionIterator);
        return false;
    }

    const int nextIndex = session.currentIndex + (backwards ? -1 : 1);
    if (nextIndex < 0 || nextIndex >= session.navigationSlots.size()) {
        const ActiveSlot currentSlot = session.navigationSlots.at(session.currentIndex);
        activeSessions.erase(sessionIterator);
        editor->setEmptySelection(backwards ? currentSlot.start : currentSlot.start + currentSlot.length);
        return true;
    }

    session.currentIndex = nextIndex;
    selectSlot(editor, session.navigationSlots.at(nextIndex));
    return true;
}

bool SnippetManagerDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == bodyEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            focusNextPrevChild(!keyEvent->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
    }

    auto *editor = qobject_cast<ScintillaNext *>(watched);
    if (!editor || event->type() != QEvent::KeyPress) {
        return QDockWidget::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape && activeSessions.contains(editor)) {
        activeSessions.remove(editor);
        return false;
    }

    if (keyEvent->key() != Qt::Key_Tab && keyEvent->key() != Qt::Key_Backtab) {
        return QDockWidget::eventFilter(watched, event);
    }

    if (keyEvent->modifiers().testFlag(Qt::ControlModifier)
        || keyEvent->modifiers().testFlag(Qt::AltModifier)
        || keyEvent->modifiers().testFlag(Qt::MetaModifier)) {
        return QDockWidget::eventFilter(watched, event);
    }

    const bool backwards = keyEvent->key() == Qt::Key_Backtab
        || keyEvent->modifiers().testFlag(Qt::ShiftModifier);
    return handleTab(editor, backwards);
}
