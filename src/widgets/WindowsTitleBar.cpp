/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "WindowsTitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QToolButton>

WindowsTitleBar::WindowsTitleBar(QMainWindow *window, QWidget *parent)
    : QWidget(parent)
    , mainWindow(window)
{
    setObjectName(QStringLiteral("windowsTitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(32);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(6);

    iconLabel = new QLabel(this);
    iconLabel->setObjectName(QStringLiteral("windowsTitleBarIcon"));
    iconLabel->setFixedSize(18, 18);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(iconLabel);

    titleLabel = new QLabel(window->windowTitle(), this);
    titleLabel->setObjectName(QStringLiteral("windowsTitleBarTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(titleLabel);

    auto makeButton = [this](const QString &objectName, const QString &text, const QString &toolTip) {
        auto *button = new QToolButton(this);
        button->setObjectName(objectName);
        button->setText(text);
        button->setToolTip(toolTip);
        button->setAccessibleName(toolTip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(46, 32);
        return button;
    };

    auto *minimizeButton = makeButton(QStringLiteral("windowsTitleBarMinimizeButton"), QStringLiteral("\u2014"), tr("Minimize"));
    layout->addWidget(minimizeButton);
    connect(minimizeButton, &QToolButton::clicked, mainWindow, &QWidget::showMinimized);

    maximizeButton = makeButton(QStringLiteral("windowsTitleBarMaximizeButton"), QStringLiteral("\u25A1"), tr("Maximize"));
    layout->addWidget(maximizeButton);
    connect(maximizeButton, &QToolButton::clicked, this, [this]() {
        if (mainWindow->isMaximized()) {
            mainWindow->showNormal();
        }
        else {
            mainWindow->showMaximized();
        }
        updateMaximizeButton();
    });

    auto *closeButton = makeButton(QStringLiteral("windowsTitleBarCloseButton"), QStringLiteral("\u00D7"), tr("Close"));
    layout->addWidget(closeButton);
    connect(closeButton, &QToolButton::clicked, mainWindow, &QWidget::close);

    connect(mainWindow, &QWidget::windowTitleChanged, titleLabel, &QLabel::setText);
    connect(mainWindow, &QWidget::windowIconChanged, this, &WindowsTitleBar::updateWindowIcon);
    updateWindowIcon();
    updateMaximizeButton();
}

bool WindowsTitleBar::isInteractivePoint(const QPoint &point) const
{
    for (const QToolButton *button : findChildren<QToolButton *>()) {
        if (button->geometry().contains(point)) {
            return true;
        }
    }
    return false;
}

void WindowsTitleBar::refreshWindowState()
{
    updateMaximizeButton();
}

void WindowsTitleBar::updateMaximizeButton()
{
    if (!maximizeButton) {
        return;
    }

    maximizeButton->setText(mainWindow->isMaximized() ? QStringLiteral("\u2750") : QStringLiteral("\u25A1"));
    maximizeButton->setToolTip(mainWindow->isMaximized() ? tr("Restore") : tr("Maximize"));
    maximizeButton->setAccessibleName(maximizeButton->toolTip());
}

void WindowsTitleBar::updateWindowIcon()
{
    if (iconLabel) {
        iconLabel->setPixmap(mainWindow->windowIcon().pixmap(18, 18));
    }
}
