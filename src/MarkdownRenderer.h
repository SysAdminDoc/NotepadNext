/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include <QColor>
#include <QString>

namespace MarkdownRenderer
{
bool isMarkdownLanguage(const QString &languageName);
QString toHtml(const QString &markdown, const QColor &foreground, const QColor &background);
}

#endif // MARKDOWNRENDERER_H
