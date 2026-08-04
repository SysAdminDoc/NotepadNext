/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "IconThemeManager.h"

#include <QImage>
#include <QList>
#include <QPainter>

namespace
{
QColor tintPixel(const QColor &source, const QColor &tint)
{
    QColor result(tint);
    result.setAlpha(source.alpha());
    return result;
}

QPixmap tintPixmap(const QPixmap &source, const QColor &tint)
{
    if (source.isNull()) {
        return QPixmap();
    }

    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 0) {
                image.setPixelColor(x, y, tintPixel(pixel, tint));
            }
        }
    }

    return QPixmap::fromImage(image);
}

QList<QSize> iconSizes(const QIcon &source, QIcon::Mode mode, QIcon::State state)
{
    QList<QSize> sizes = source.availableSizes(mode, state);
    if (sizes.isEmpty()) {
        sizes = {QSize(16, 16), QSize(24, 24), QSize(32, 32)};
    }
    return sizes;
}
}

IconThemeManager::Pack IconThemeManager::packFromValue(int value)
{
    switch (value) {
    case static_cast<int>(Pack::Nord):
        return Pack::Nord;
    case static_cast<int>(Pack::Catppuccin):
        return Pack::Catppuccin;
    case static_cast<int>(Pack::GitHubDark):
        return Pack::GitHubDark;
    default:
        return Pack::Default;
    }
}

IconThemeManager::Pack IconThemeManager::packFromName(const QString &name)
{
    if (name.compare(QStringLiteral("Nord"), Qt::CaseInsensitive) == 0) {
        return Pack::Nord;
    }
    if (name.compare(QStringLiteral("Catppuccin"), Qt::CaseInsensitive) == 0) {
        return Pack::Catppuccin;
    }
    if (name.compare(QStringLiteral("GitHub Dark"), Qt::CaseInsensitive) == 0) {
        return Pack::GitHubDark;
    }
    return Pack::Default;
}

int IconThemeManager::value(Pack pack)
{
    return static_cast<int>(pack);
}

QString IconThemeManager::name(Pack pack)
{
    switch (pack) {
    case Pack::Nord:
        return QStringLiteral("Nord");
    case Pack::Catppuccin:
        return QStringLiteral("Catppuccin");
    case Pack::GitHubDark:
        return QStringLiteral("GitHub Dark");
    default:
        return QStringLiteral("Default");
    }
}

IconThemeManager::Colors IconThemeManager::colors(Pack pack)
{
    switch (pack) {
    case Pack::Nord:
        return {
            QColor(QStringLiteral("#d8dee9")),
            QColor(QStringLiteral("#88c0d0")),
            QColor(QStringLiteral("#a3be8c")),
            QColor(QStringLiteral("#d08770")),
            QColor(QStringLiteral("#bf616a")),
            QColor(QStringLiteral("#4c566a")),
        };
    case Pack::Catppuccin:
        return {
            QColor(QStringLiteral("#cdd6f4")),
            QColor(QStringLiteral("#89b4fa")),
            QColor(QStringLiteral("#a6e3a1")),
            QColor(QStringLiteral("#fab387")),
            QColor(QStringLiteral("#f38ba8")),
            QColor(QStringLiteral("#6c7086")),
        };
    case Pack::GitHubDark:
        return {
            QColor(QStringLiteral("#c9d1d9")),
            QColor(QStringLiteral("#58a6ff")),
            QColor(QStringLiteral("#3fb950")),
            QColor(QStringLiteral("#d29922")),
            QColor(QStringLiteral("#f85149")),
            QColor(QStringLiteral("#484f58")),
        };
    default:
        return {};
    }
}

QColor IconThemeManager::colorForKey(Pack pack, const QString &key)
{
    const Colors palette = colors(pack);
    const QString normalized = key.toLower();

    if (normalized.contains(QStringLiteral("unsaved")) || normalized.contains(QStringLiteral("dirty"))) {
        return palette.warning;
    }
    if (normalized.contains(QStringLiteral("close")) ||
        normalized.contains(QStringLiteral("delete")) ||
        normalized.contains(QStringLiteral("trash"))) {
        return palette.destructive;
    }
    if (normalized.contains(QStringLiteral("save")) || normalized.contains(QStringLiteral("record"))) {
        return palette.positive;
    }
    if (normalized.contains(QStringLiteral("readonly"))) {
        return palette.disabled;
    }
    if (normalized.contains(QStringLiteral("find")) || normalized.contains(QStringLiteral("zoom"))) {
        return palette.accent;
    }
    return palette.foreground;
}

QIcon IconThemeManager::recolor(const QIcon &source, Pack pack, const QString &key)
{
    if (source.isNull() || pack == Pack::Default) {
        return source;
    }

    QIcon result;
    const Colors palette = colors(pack);
    const QColor normalColor = colorForKey(pack, key);
    const QList<QIcon::Mode> modes = {
        QIcon::Normal,
        QIcon::Disabled,
        QIcon::Active,
        QIcon::Selected,
    };
    const QList<QIcon::State> states = {QIcon::Off, QIcon::On};

    for (const QIcon::Mode mode : modes) {
        for (const QIcon::State state : states) {
            const QColor tint = mode == QIcon::Disabled ? palette.disabled : normalColor;
            for (const QSize &size : iconSizes(source, mode, state)) {
                const QPixmap pixmap = tintPixmap(source.pixmap(size, mode, state), tint);
                if (!pixmap.isNull()) {
                    result.addPixmap(pixmap, mode, state);
                }
            }
        }
    }

    return result.isNull() ? source : result;
}
