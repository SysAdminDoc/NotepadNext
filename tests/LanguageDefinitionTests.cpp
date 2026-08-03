/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LuaState.h"

#include "ILexer.h"
#include "Lexilla.h"

#include <QtTest>

class LanguageDefinitionTests final : public QObject
{
    Q_OBJECT

private slots:
    void modernDefinitionsLoadAndExposeLexers();
};

void LanguageDefinitionTests::modernDefinitionsLoadAndExposeLexers()
{
    LuaState state;
    state.executeFile(QStringLiteral(":/scripts/init.lua"));

    const QStringList definitions = state.executeAndReturn<QStringList>(R"(
        local definitions = {}
        local names = {"Nim", "Zig", "Svelte", "MDX", "HCL", "Terraform", "TOML"}
        for _, name in ipairs(names) do
            local language = assert(languages[name])
            assert(language.lexer)
            assert(language.extensions)
            table.insert(definitions, name .. "|" .. language.lexer .. "|" .. table.concat(language.extensions, ","))
        end
        return definitions
    )");

    QCOMPARE(definitions, QStringList({
        QStringLiteral("Nim|nim|nim"),
        QStringLiteral("Zig|zig|zig"),
        QStringLiteral("Svelte|hypertext|svelte"),
        QStringLiteral("MDX|markdown|mdx"),
        QStringLiteral("HCL|cpp|hcl"),
        QStringLiteral("Terraform|cpp|tf,tfvars"),
        QStringLiteral("TOML|toml|toml"),
    }));

    const QList<QPair<const char *, const char *>> lexers = {
        {"nim", "Nim"},
        {"zig", "Zig"},
        {"hypertext", "Svelte"},
        {"markdown", "MDX"},
        {"cpp", "HCL/Terraform"},
        {"toml", "TOML"},
    };

    for (const auto &[lexerName, languageName] : lexers) {
        ILexer5 *lexer = CreateLexer(lexerName);
        QVERIFY2(lexer != nullptr, languageName);
        if (lexer) {
            lexer->Release();
        }
    }
}

QTEST_APPLESS_MAIN(LanguageDefinitionTests)
#include "LanguageDefinitionTests.moc"
