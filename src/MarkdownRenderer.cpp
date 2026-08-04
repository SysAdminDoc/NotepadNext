/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MarkdownRenderer.h"

#include <QTextDocument>

namespace
{
QString colorName(const QColor &color, const QString &fallback)
{
    return color.isValid() ? color.name(QColor::HexRgb) : fallback;
}
}

bool MarkdownRenderer::isMarkdownLanguage(const QString &languageName)
{
    return languageName.compare(QStringLiteral("Markdown"), Qt::CaseInsensitive) == 0 ||
           languageName.compare(QStringLiteral("MDX"), Qt::CaseInsensitive) == 0;
}

QString MarkdownRenderer::toHtml(const QString &markdown, const QColor &foreground, const QColor &background)
{
    QTextDocument document;
    document.setDefaultStyleSheet(QStringLiteral(
        "body { color: %1; background-color: %2; font-family: sans-serif; margin: 1.5em; }"
        "h1, h2, h3, h4, h5, h6 { margin-top: 1.2em; margin-bottom: 0.5em; }"
        "a { color: %3; }"
        "blockquote { border-left: 0.25em solid %3; margin-left: 0; padding-left: 1em; }"
        "code { background-color: %4; padding: 0.1em 0.25em; }"
        "pre { background-color: %4; padding: 0.8em; }"
        "table { border-collapse: collapse; }"
        "th, td { border: 1px solid %3; padding: 0.35em 0.6em; }"
    ).arg(colorName(foreground, QStringLiteral("#202124")),
          colorName(background, QStringLiteral("#ffffff")),
          colorName(foreground.lighter(125), QStringLiteral("#345a9c")),
          colorName(background.darker(110), QStringLiteral("#f0f0f0"))));
    document.setMarkdown(markdown);
    return document.toHtml();
}
