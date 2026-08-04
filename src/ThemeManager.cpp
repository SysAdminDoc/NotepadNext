/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ThemeManager.h"

#include <QColor>

namespace
{
void setAllGroups(QPalette &palette, QPalette::ColorRole role, const QColor &color)
{
    palette.setColor(QPalette::Active, role, color);
    palette.setColor(QPalette::Inactive, role, color);
    palette.setColor(QPalette::Disabled, role, color);
}

void setDisabled(QPalette &palette, QPalette::ColorRole role, const QColor &color)
{
    palette.setColor(QPalette::Disabled, role, color);
}
}

ThemeManager::Variant ThemeManager::variantFromValue(int value)
{
    switch (value) {
    case static_cast<int>(Variant::Material):
        return Variant::Material;
    case static_cast<int>(Variant::Fluent):
        return Variant::Fluent;
    default:
        return Variant::Fusion;
    }
}

ThemeManager::Variant ThemeManager::variantFromName(const QString &name)
{
    if (name.compare(QStringLiteral("Material"), Qt::CaseInsensitive) == 0) {
        return Variant::Material;
    }
    if (name.compare(QStringLiteral("Fluent"), Qt::CaseInsensitive) == 0) {
        return Variant::Fluent;
    }
    return Variant::Fusion;
}

int ThemeManager::value(Variant variant)
{
    return static_cast<int>(variant);
}

QString ThemeManager::name(Variant variant)
{
    switch (variant) {
    case Variant::Material:
        return QStringLiteral("Material");
    case Variant::Fluent:
        return QStringLiteral("Fluent");
    default:
        return QStringLiteral("Fusion");
    }
}

QPalette ThemeManager::palette(Variant variant, const QPalette &base)
{
    if (variant == Variant::Fusion) {
        return base;
    }

    QPalette result(base);
    if (variant == Variant::Material) {
        setAllGroups(result, QPalette::Window, QColor(QStringLiteral("#fffbfe")));
        setAllGroups(result, QPalette::WindowText, QColor(QStringLiteral("#1d1b20")));
        setAllGroups(result, QPalette::Base, QColor(QStringLiteral("#fffbfe")));
        setAllGroups(result, QPalette::AlternateBase, QColor(QStringLiteral("#f3edf7")));
        setAllGroups(result, QPalette::Text, QColor(QStringLiteral("#1d1b20")));
        setAllGroups(result, QPalette::Button, QColor(QStringLiteral("#f3edf7")));
        setAllGroups(result, QPalette::ButtonText, QColor(QStringLiteral("#1d1b20")));
        setAllGroups(result, QPalette::Highlight, QColor(QStringLiteral("#6750a4")));
        setAllGroups(result, QPalette::HighlightedText, QColor(Qt::white));
        setAllGroups(result, QPalette::Link, QColor(QStringLiteral("#6750a4")));
        setAllGroups(result, QPalette::Mid, QColor(QStringLiteral("#79747e")));
        setAllGroups(result, QPalette::Dark, QColor(QStringLiteral("#49454f")));
        setAllGroups(result, QPalette::ToolTipBase, QColor(QStringLiteral("#fffbfe")));
        setAllGroups(result, QPalette::ToolTipText, QColor(QStringLiteral("#1d1b20")));

        setDisabled(result, QPalette::WindowText, QColor(QStringLiteral("#938f99")));
        setDisabled(result, QPalette::Text, QColor(QStringLiteral("#938f99")));
        setDisabled(result, QPalette::ButtonText, QColor(QStringLiteral("#938f99")));
        return result;
    }

    setAllGroups(result, QPalette::Window, QColor(QStringLiteral("#202020")));
    setAllGroups(result, QPalette::WindowText, QColor(QStringLiteral("#f3f3f3")));
    setAllGroups(result, QPalette::Base, QColor(QStringLiteral("#1f1f1f")));
    setAllGroups(result, QPalette::AlternateBase, QColor(QStringLiteral("#292929")));
    setAllGroups(result, QPalette::Text, QColor(QStringLiteral("#f3f3f3")));
    setAllGroups(result, QPalette::Button, QColor(QStringLiteral("#2d2d2d")));
    setAllGroups(result, QPalette::ButtonText, QColor(QStringLiteral("#f3f3f3")));
    setAllGroups(result, QPalette::Highlight, QColor(QStringLiteral("#60cdff")));
    setAllGroups(result, QPalette::HighlightedText, QColor(QStringLiteral("#111111")));
    setAllGroups(result, QPalette::Link, QColor(QStringLiteral("#60cdff")));
    setAllGroups(result, QPalette::Mid, QColor(QStringLiteral("#666666")));
    setAllGroups(result, QPalette::Dark, QColor(QStringLiteral("#0f0f0f")));
    setAllGroups(result, QPalette::ToolTipBase, QColor(QStringLiteral("#2d2d2d")));
    setAllGroups(result, QPalette::ToolTipText, QColor(QStringLiteral("#f3f3f3")));

    setDisabled(result, QPalette::WindowText, QColor(QStringLiteral("#777777")));
    setDisabled(result, QPalette::Text, QColor(QStringLiteral("#777777")));
    setDisabled(result, QPalette::ButtonText, QColor(QStringLiteral("#777777")));
    return result;
}

QString ThemeManager::styleSheet(Variant variant)
{
    switch (variant) {
    case Variant::Material:
        return QStringLiteral(R"(
QWidget {
    selection-background-color: #6750a4;
    selection-color: #ffffff;
}

QMenuBar, QMenu {
    background-color: #fffbfe;
    color: #1d1b20;
    border: 1px solid #e7e0ec;
}

QMenu::item {
    padding: 5px 24px 5px 8px;
    border-radius: 4px;
    margin: 2px 4px;
}

QMenu::item:selected {
    background-color: #6750a4;
    color: #ffffff;
}

QToolBar {
    background-color: #fffbfe;
    border: none;
    spacing: 4px;
}

QToolButton, QPushButton {
    background-color: #f3edf7;
    border: 1px solid #79747e;
    border-radius: 6px;
    padding: 4px 10px;
}

QToolButton:hover, QPushButton:hover {
    background-color: #e8def8;
}

QToolButton:pressed, QPushButton:pressed {
    background-color: #d0bcff;
}

QLineEdit, QComboBox, QSpinBox {
    background-color: #fffbfe;
    border: 1px solid #79747e;
    border-radius: 5px;
    padding: 3px 6px;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
    border: 2px solid #6750a4;
    padding: 2px 5px;
}

QTabBar::tab {
    background-color: #f3edf7;
    color: #49454f;
    border: 1px solid #e7e0ec;
    border-radius: 5px;
    padding: 5px 10px;
    margin: 2px;
}

QTabBar::tab:selected {
    background-color: #6750a4;
    color: #ffffff;
}

QStatusBar {
    border-top: 1px solid #e7e0ec;
}

ads--CDockWidgetTab {
    background: #f3edf7;
    color: #49454f;
    border: 1px solid #e7e0ec;
    border-top: 3px solid #d0bcff;
    border-radius: 5px 5px 0 0;
}

ads--CDockWidgetTab[activeTab="true"], ads--CDockWidgetTab[focused="true"] {
    background: #6750a4;
    color: #ffffff;
    border-top-color: #6750a4;
}

ads--CDockWidgetTab[activeTab="false"] QLabel {
    color: #49454f;
}

ads--CDockAreaTitleBar {
    background: #fffbfe;
    border-bottom: 1px solid #e7e0ec;
}

#QuickFindWidget {
    border-left: 1px solid #e7e0ec;
    border-right: 1px solid #e7e0ec;
    border-bottom: 3px solid #6750a4;
    background: #fffbfe;
}
)");
    case Variant::Fluent:
        return QStringLiteral(R"(
QWidget {
    selection-background-color: #60cdff;
    selection-color: #111111;
}

QMenuBar, QMenu {
    background-color: #202020;
    color: #f3f3f3;
    border: 1px solid #454545;
}

QMenu::item {
    padding: 5px 24px 5px 8px;
    border-radius: 4px;
    margin: 2px 4px;
}

QMenu::item:selected {
    background-color: #3b3b3b;
    color: #60cdff;
}

QToolBar {
    background-color: #202020;
    border: none;
    spacing: 4px;
}

QToolButton, QPushButton {
    background-color: #2d2d2d;
    border: 1px solid #454545;
    border-radius: 4px;
    padding: 4px 10px;
}

QToolButton:hover, QPushButton:hover {
    background-color: #3b3b3b;
    border-color: #60cdff;
}

QToolButton:pressed, QPushButton:pressed {
    background-color: #454545;
}

QLineEdit, QComboBox, QSpinBox {
    background-color: #1f1f1f;
    border: 1px solid #666666;
    border-radius: 4px;
    padding: 3px 6px;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
    border: 2px solid #60cdff;
    padding: 2px 5px;
}

QTabBar::tab {
    background-color: #2d2d2d;
    color: #bdbdbd;
    border: 1px solid #454545;
    border-radius: 4px;
    padding: 5px 10px;
    margin: 2px;
}

QTabBar::tab:selected {
    background-color: #3b3b3b;
    color: #60cdff;
    border-bottom: 2px solid #60cdff;
}

QStatusBar {
    border-top: 1px solid #454545;
}

ads--CDockWidgetTab {
    background: #2d2d2d;
    color: #bdbdbd;
    border: 1px solid #454545;
    border-top: 3px solid #454545;
    border-radius: 4px 4px 0 0;
}

ads--CDockWidgetTab[activeTab="true"], ads--CDockWidgetTab[focused="true"] {
    background: #3b3b3b;
    color: #f3f3f3;
    border-top-color: #60cdff;
}

ads--CDockWidgetTab[activeTab="false"] QLabel {
    color: #bdbdbd;
}

ads--CDockAreaTitleBar {
    background: #202020;
    border-bottom: 1px solid #454545;
}

#QuickFindWidget {
    border-left: 1px solid #454545;
    border-right: 1px solid #454545;
    border-bottom: 3px solid #60cdff;
    background: #202020;
}
)");
    default:
        return QString();
    }
}
