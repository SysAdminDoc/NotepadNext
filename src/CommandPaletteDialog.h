/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMANDPALETTEDIALOG_H
#define COMMANDPALETTEDIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>

class QAction;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QEvent;

class CommandPaletteDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPaletteDialog(QWidget *parent, const QList<QAction *> &actions);

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateResults();
    void triggerCurrent();

private:
    static QString actionLabel(const QAction *action);

    QLineEdit *queryEdit;
    QListWidget *results;
    QList<QAction *> actions;
    QHash<QListWidgetItem *, QAction *> actionForItem;
};

#endif // COMMANDPALETTEDIALOG_H
