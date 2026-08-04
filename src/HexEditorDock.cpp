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

#include "HexEditorDock.h"

#include "HexTableModel.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

namespace
{
class HexByteDelegate final : public QStyledItemDelegate
{
public:
    explicit HexByteDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *editor = new QLineEdit(parent);
        editor->setMaxLength(2);
        editor->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9A-Fa-f]{0,2}")), editor));
        editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit) {
            lineEdit->setText(index.data(Qt::EditRole).toString());
            lineEdit->selectAll();
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        auto *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit) {
            model->setData(index, lineEdit->text().toUpper(), Qt::EditRole);
        }
    }
};
}

HexEditorDock::HexEditorDock(MainWindow *window, QWidget *parent)
    : QDockWidget(tr("Hex Editor"), parent)
    , window(window)
    , model(new HexTableModel(&document, this))
    , table(new QTableView(this))
    , statusLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("hexEditorDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumHeight(320);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    auto *controls = new QHBoxLayout;
    auto *openButton = new QPushButton(tr("Open..."), container);
    openButton->setObjectName(QStringLiteral("hexEditorOpen"));
    auto *currentButton = new QPushButton(tr("Current File"), container);
    currentButton->setObjectName(QStringLiteral("hexEditorCurrent"));
    auto *reloadButton = new QPushButton(tr("Reload"), container);
    reloadButton->setObjectName(QStringLiteral("hexEditorReload"));
    auto *saveButton = new QPushButton(tr("Save"), container);
    saveButton->setObjectName(QStringLiteral("hexEditorSave"));
    controls->addWidget(openButton);
    controls->addWidget(currentButton);
    controls->addWidget(reloadButton);
    controls->addWidget(saveButton);
    controls->addStretch();
    layout->addLayout(controls);

    statusLabel->setObjectName(QStringLiteral("hexEditorStatus"));
    layout->addWidget(statusLabel);

    table->setObjectName(QStringLiteral("hexEditorTable"));
    table->setModel(model);
    table->setItemDelegate(new HexByteDelegate(table));
    table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(true);
    table->verticalHeader()->setDefaultSectionSize(table->fontMetrics().height() + 6);
    table->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int column = 1; column <= 16; ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    table->horizontalHeader()->setSectionResizeMode(17, QHeaderView::Stretch);
    layout->addWidget(table, 1);

    setWidget(container);
    updateStatus();

    connect(openButton, &QPushButton::clicked, this, &HexEditorDock::openFileDialog);
    connect(currentButton, &QPushButton::clicked, this, &HexEditorDock::openCurrentFile);
    connect(reloadButton, &QPushButton::clicked, this, &HexEditorDock::reloadFile);
    connect(saveButton, &QPushButton::clicked, this, &HexEditorDock::saveFile);
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() {
        updateStatus();
        emit documentStateChanged();
    });
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
        updateStatus();
        emit documentStateChanged();
    });
}

bool HexEditorDock::openFile(const QString &filePath)
{
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    if (absolutePath == document.filePath() && !document.filePath().isEmpty()) {
        return true;
    }
    if (!confirmDiscardChanges(tr("open another file"))) {
        return false;
    }

    lastError.clear();
    if (!model->loadFile(absolutePath, &lastError)) {
        updateStatus(lastError, true);
        return false;
    }

    setWindowTitle(tr("Hex Editor — %1").arg(QFileInfo(absolutePath).fileName()));
    updateStatus();
    emit documentStateChanged();
    return true;
}

bool HexEditorDock::hasFile() const
{
    return !document.filePath().isEmpty();
}

bool HexEditorDock::isDirty() const
{
    return document.isDirty();
}

bool HexEditorDock::canSave() const
{
    return document.isDirty() && !document.isReadOnly();
}

bool HexEditorDock::confirmClose()
{
    if (!document.isDirty()) {
        return true;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Unsaved Hex Changes"),
        tr("Save changes to %1 before closing?").arg(QFileInfo(document.filePath()).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        saveFile();
        return !document.isDirty();
    }
    if (answer == QMessageBox::Discard) {
        document.discardChanges();
        updateStatus();
        emit documentStateChanged();
        return true;
    }
    return false;
}

void HexEditorDock::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File in Hex Editor"));
    if (!path.isEmpty() && openFile(path)) {
        show();
        raise();
        focusTable();
    }
}

void HexEditorDock::openCurrentFile()
{
    ScintillaNext *editor = window ? window->currentEditor() : nullptr;
    if (!editor || !editor->isFile()) {
        updateStatus(tr("The active document is not a file."), true);
        return;
    }

    if (openFile(editor->getFilePath())) {
        show();
        raise();
        focusTable();
    }
}

void HexEditorDock::reloadFile()
{
    if (!hasFile()) {
        updateStatus(tr("No file is open."), true);
        return;
    }
    if (!confirmDiscardChanges(tr("reload the file"))) {
        return;
    }

    lastError.clear();
    if (!model->reload(&lastError)) {
        updateStatus(lastError, true);
        return;
    }
    updateStatus();
    emit documentStateChanged();
}

void HexEditorDock::saveFile()
{
    if (!hasFile()) {
        updateStatus(tr("No file is open."), true);
        return;
    }
    lastError.clear();
    if (!model->save(&lastError)) {
        updateStatus(lastError, true);
        return;
    }
    updateStatus();
    emit documentStateChanged();
}

void HexEditorDock::focusTable()
{
    table->setFocus();
}

bool HexEditorDock::confirmDiscardChanges(const QString &action)
{
    if (!document.isDirty()) {
        return true;
    }

    const auto answer = QMessageBox::warning(
        this,
        tr("Unsaved Hex Changes"),
        tr("Save changes before you %1?").arg(action),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        saveFile();
        return !document.isDirty();
    }
    if (answer == QMessageBox::Discard) {
        document.discardChanges();
        updateStatus();
        emit documentStateChanged();
        return !document.isDirty();
    }
    return false;
}

void HexEditorDock::updateStatus(const QString &message, bool error)
{
    if (!message.isEmpty()) {
        statusLabel->setText(message);
        statusLabel->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString());
        return;
    }

    if (!hasFile()) {
        statusLabel->setText(tr("Open a binary file to inspect and edit its bytes."));
        statusLabel->setStyleSheet(QString());
        return;
    }

    const QString dirtyMarker = document.isDirty() ? tr(" • Modified") : QString();
    const QString readOnlyMarker = document.isReadOnly() ? tr(" • Read-only") : QString();
    statusLabel->setText(tr("%1 bytes • %2%3%4")
                             .arg(document.size())
                             .arg(QFileInfo(document.filePath()).fileName())
                             .arg(dirtyMarker)
                             .arg(readOnlyMarker));
    statusLabel->setStyleSheet(QString());
}
