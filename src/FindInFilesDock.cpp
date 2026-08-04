/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "FindInFilesDock.h"

#include "RipgrepSearch.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int FilePathRole = Qt::UserRole;
constexpr int LineNumberRole = Qt::UserRole + 1;
constexpr int StartByteRole = Qt::UserRole + 2;
constexpr int EndByteRole = Qt::UserRole + 3;
}

FindInFilesDock::FindInFilesDock(QWidget *parent)
    : QDockWidget(parent),
      search(new RipgrepSearch(this))
{
    setObjectName(QStringLiteral("findInFilesDock"));
    setWindowTitle(tr("Find in Files"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumHeight(260);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    auto *controls = new QGridLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setColumnStretch(1, 1);

    controls->addWidget(new QLabel(tr("Find:"), container), 0, 0);
    patternEdit = new QLineEdit(container);
    patternEdit->setObjectName(QStringLiteral("findInFilesPattern"));
    patternEdit->setPlaceholderText(tr("Text or regular expression"));
    controls->addWidget(patternEdit, 0, 1, 1, 3);

    controls->addWidget(new QLabel(tr("Folder:"), container), 1, 0);
    pathEdit = new QLineEdit(container);
    pathEdit->setObjectName(QStringLiteral("findInFilesPath"));
    pathEdit->setPlaceholderText(tr("Folder to search"));
    controls->addWidget(pathEdit, 1, 1);

    auto *browseButton = new QPushButton(tr("Browse..."), container);
    browseButton->setObjectName(QStringLiteral("findInFilesBrowse"));
    controls->addWidget(browseButton, 1, 2);

    searchButton = new QPushButton(tr("Search"), container);
    searchButton->setObjectName(QStringLiteral("findInFilesSearch"));
    controls->addWidget(searchButton, 1, 3);

    auto *options = new QHBoxLayout();
    regexCheck = new QCheckBox(tr("Regular expression"), container);
    regexCheck->setObjectName(QStringLiteral("findInFilesRegex"));
    options->addWidget(regexCheck);
    caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), container);
    caseSensitiveCheck->setObjectName(QStringLiteral("findInFilesCaseSensitive"));
    caseSensitiveCheck->setChecked(true);
    options->addWidget(caseSensitiveCheck);
    hiddenCheck = new QCheckBox(tr("Include hidden files"), container);
    hiddenCheck->setObjectName(QStringLiteral("findInFilesHidden"));
    options->addWidget(hiddenCheck);
    options->addStretch();
    stopButton = new QPushButton(tr("Stop"), container);
    stopButton->setObjectName(QStringLiteral("findInFilesStop"));
    stopButton->setEnabled(false);
    options->addWidget(stopButton);

    layout->addLayout(controls);
    layout->addLayout(options);

    statusLabel = new QLabel(container);
    statusLabel->setObjectName(QStringLiteral("findInFilesStatus"));
    layout->addWidget(statusLabel);

    results = new QTreeWidget(container);
    results->setObjectName(QStringLiteral("findInFilesResults"));
    results->setColumnCount(4);
    results->setHeaderLabels({tr("File"), tr("Line"), tr("Column"), tr("Match")});
    results->setRootIsDecorated(false);
    results->setUniformRowHeights(true);
    results->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    results->setSelectionMode(QAbstractItemView::SingleSelection);
    results->setAlternatingRowColors(true);
    results->header()->setStretchLastSection(true);
    results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    results->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(results, 1);

    setWidget(container);
    setSearchPath(QDir::homePath());
    updateStatus(tr("Enter a search pattern and folder."));

    connect(patternEdit, &QLineEdit::returnPressed, this, &FindInFilesDock::startSearch);
    connect(pathEdit, &QLineEdit::returnPressed, this, &FindInFilesDock::startSearch);
    connect(searchButton, &QPushButton::clicked, this, &FindInFilesDock::startSearch);
    connect(stopButton, &QPushButton::clicked, this, &FindInFilesDock::cancelSearch);
    connect(browseButton, &QPushButton::clicked, this, &FindInFilesDock::browseForPath);
    connect(results, &QTreeWidget::itemActivated, this, &FindInFilesDock::activateResult);
    connect(search, &RipgrepSearch::matchFound, this, &FindInFilesDock::addMatch);
    connect(search, &RipgrepSearch::searchFinished, this, &FindInFilesDock::finishSearch);
    connect(search, &RipgrepSearch::searchError, this, &FindInFilesDock::showSearchError);
    new QShortcut(QKeySequence::Cancel, this, this, &FindInFilesDock::cancelSearch, Qt::WidgetWithChildrenShortcut);
}

FindInFilesDock::~FindInFilesDock()
{
    search->cancel();
}

QString FindInFilesDock::pattern() const
{
    return patternEdit->text();
}

QString FindInFilesDock::searchPath() const
{
    return pathEdit->text();
}

void FindInFilesDock::setPattern(const QString &pattern)
{
    patternEdit->setText(pattern);
}

void FindInFilesDock::setSearchPath(const QString &path)
{
    const QString resolved = path.trimmed().isEmpty() ? QDir::homePath() : QDir(path).absolutePath();
    if (QDir(resolved).exists()) {
        pathEdit->setText(resolved);
    }
}

void FindInFilesDock::focusPattern()
{
    patternEdit->setFocus();
    patternEdit->selectAll();
}

void FindInFilesDock::startSearch()
{
    if (search->isRunning()) {
        return;
    }

    const QString rootPath = pathEdit->text().trimmed();
    activeRootPath = QDir(rootPath).absolutePath();
    clearResults();

    RipgrepSearch::Options options;
    options.pattern = patternEdit->text();
    options.rootPath = activeRootPath;
    options.regularExpression = regexCheck->isChecked();
    options.caseSensitive = caseSensitiveCheck->isChecked();
    options.includeHidden = hiddenCheck->isChecked();

    lastError.clear();
    updateStatus(tr("Searching..."));
    setSearchControlsEnabled(false);
    if (!search->start(options)) {
        setSearchControlsEnabled(true);
    }
}

void FindInFilesDock::cancelSearch()
{
    if (!search->isRunning()) {
        return;
    }
    search->cancel();
}

void FindInFilesDock::clearResults()
{
    results->clear();
}

void FindInFilesDock::addMatch(const RipgrepSearch::Match &match)
{
    auto *item = new QTreeWidgetItem(results);
    const QString relativePath = QDir(activeRootPath).relativeFilePath(match.filePath);
    item->setText(0, relativePath.isEmpty() ? match.filePath : relativePath);
    item->setText(1, QString::number(match.lineNumber));
    item->setText(2, QString::number(match.column));
    item->setText(3, match.lineText);
    item->setToolTip(0, match.filePath);
    item->setData(0, FilePathRole, match.filePath);
    item->setData(0, LineNumberRole, match.lineNumber);
    item->setData(0, StartByteRole, match.startByte);
    item->setData(0, EndByteRole, match.endByte);
}

void FindInFilesDock::finishSearch(int matchCount, int fileCount, bool cancelled)
{
    setSearchControlsEnabled(true);
    if (!lastError.isEmpty()) {
        updateStatus(lastError, true);
        return;
    }
    const QString prefix = cancelled ? tr("Search cancelled. ") : QString();
    updateStatus(prefix + tr("Found %L1 matches in %L2 files.").arg(matchCount).arg(fileCount));
    results->resizeColumnToContents(0);
}

void FindInFilesDock::showSearchError(const QString &message)
{
    lastError = message;
    updateStatus(message, true);
}

void FindInFilesDock::activateResult(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) {
        return;
    }

    emit resultActivated(item->data(0, FilePathRole).toString(),
                         item->data(0, LineNumberRole).toInt(),
                         item->data(0, StartByteRole).toInt(),
                         item->data(0, EndByteRole).toInt());
}

void FindInFilesDock::browseForPath()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select Search Folder"), searchPath());
    if (!path.isEmpty()) {
        setSearchPath(path);
    }
}

void FindInFilesDock::setSearchControlsEnabled(bool enabled)
{
    patternEdit->setEnabled(enabled);
    pathEdit->setEnabled(enabled);
    regexCheck->setEnabled(enabled);
    caseSensitiveCheck->setEnabled(enabled);
    hiddenCheck->setEnabled(enabled);
    searchButton->setEnabled(enabled);
    stopButton->setEnabled(!enabled);
}

void FindInFilesDock::updateStatus(const QString &message, bool error)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString());
}
