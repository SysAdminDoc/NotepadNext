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

#include <QPalette>
#include <QString>

class ThemeManager final
{
public:
    enum class Variant {
        Fusion = 0,
        Material,
        Fluent,
    };

    static Variant variantFromValue(int value);
    static Variant variantFromName(const QString &name);
    static int value(Variant variant);
    static QString name(Variant variant);

    static QPalette palette(Variant variant, const QPalette &base);
    static QString styleSheet(Variant variant);
};
