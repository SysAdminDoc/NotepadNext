/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCHEMAVALIDATOR_H
#define SCHEMAVALIDATOR_H

#include <QVector>
#include <QString>

class SchemaValidator final
{
public:
    enum class Format
    {
        Json,
        Yaml,
        Toml,
        Unknown,
    };

    struct Diagnostic
    {
        QString message;
        QString instancePath;
        int line = 0;
        int column = 0;
        int length = 1;
    };

    struct Result
    {
        bool valid = false;
        bool documentParsed = false;
        bool schemaParsed = false;
        QVector<Diagnostic> diagnostics;
    };

    static Format formatForLanguage(const QString &languageName);
    static bool isSupportedLanguage(const QString &languageName);
    static Result validate(const QString &document, const QString &schema, Format format);
};

#endif // SCHEMAVALIDATOR_H
