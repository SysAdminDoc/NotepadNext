/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QWidget>

class QMainWindow;
class QLabel;
class QToolButton;

class WindowsTitleBar final : public QWidget
{
    Q_OBJECT

public:
    explicit WindowsTitleBar(QMainWindow *window, QWidget *parent = nullptr);

    bool isInteractivePoint(const QPoint &point) const;
    void refreshWindowState();

private slots:
    void updateMaximizeButton();
    void updateWindowIcon();

private:
    QMainWindow *mainWindow = nullptr;
    QLabel *iconLabel = nullptr;
    QLabel *titleLabel = nullptr;
    QToolButton *maximizeButton = nullptr;
};
