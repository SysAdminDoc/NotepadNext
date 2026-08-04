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

#ifndef HEXEDITORDOCK_H
#define HEXEDITORDOCK_H

#include <QDockWidget>

#include "HexDocument.h"

class HexTableModel;
class QLabel;
class MainWindow;
class QTableView;

class HexEditorDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit HexEditorDock(MainWindow *window, QWidget *parent = nullptr);

    bool openFile(const QString &filePath);
    bool hasFile() const;
    bool isDirty() const;
    bool canSave() const;
    bool confirmClose();

public slots:
    void openFileDialog();
    void openCurrentFile();
    void reloadFile();
    void saveFile();
    void focusTable();

signals:
    void documentStateChanged();

private:
    bool confirmDiscardChanges(const QString &action);
    void updateStatus(const QString &message = QString(), bool error = false);

    MainWindow *window;
    HexDocument document;
    HexTableModel *model;
    QTableView *table;
    QLabel *statusLabel;
    QString lastError;
};

#endif // HEXEDITORDOCK_H
