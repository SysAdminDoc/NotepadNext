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

#include <QColor>
#include <QIcon>
#include <QString>

class IconThemeManager final
{
public:
    enum class Pack {
        Default = 0,
        Nord,
        Catppuccin,
        GitHubDark,
    };

    struct Colors {
        QColor foreground;
        QColor accent;
        QColor positive;
        QColor warning;
        QColor destructive;
        QColor disabled;
    };

    static Pack packFromValue(int value);
    static Pack packFromName(const QString &name);
    static int value(Pack pack);
    static QString name(Pack pack);
    static Colors colors(Pack pack);
    static QColor colorForKey(Pack pack, const QString &key);

    static QIcon recolor(const QIcon &source, Pack pack, const QString &key = QString());
};
