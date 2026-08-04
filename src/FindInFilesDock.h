/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FINDINFILESDOCK_H
#define FINDINFILESDOCK_H

#include <QDockWidget>

#include "RipgrepSearch.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class FindInFilesDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit FindInFilesDock(QWidget *parent = nullptr);
    ~FindInFilesDock() override;

    QString pattern() const;
    QString searchPath() const;

public slots:
    void setPattern(const QString &pattern);
    void setSearchPath(const QString &path);
    void focusPattern();
    void startSearch();
    void cancelSearch();
    void clearResults();

signals:
    void resultActivated(const QString &filePath, int lineNumber, int startByte, int endByte);

private slots:
    void addMatch(const RipgrepSearch::Match &match);
    void finishSearch(int matchCount, int fileCount, bool cancelled);
    void showSearchError(const QString &message);
    void activateResult(QTreeWidgetItem *item, int column);
    void browseForPath();

private:
    void setSearchControlsEnabled(bool enabled);
    void updateStatus(const QString &message, bool error = false);

    RipgrepSearch *search = nullptr;
    QLineEdit *patternEdit = nullptr;
    QLineEdit *pathEdit = nullptr;
    QCheckBox *regexCheck = nullptr;
    QCheckBox *caseSensitiveCheck = nullptr;
    QCheckBox *hiddenCheck = nullptr;
    QPushButton *searchButton = nullptr;
    QPushButton *stopButton = nullptr;
    QLabel *statusLabel = nullptr;
    QTreeWidget *results = nullptr;
    QString activeRootPath;
    QString lastError;
};

#endif // FINDINFILESDOCK_H
