/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "SchemaValidator.h"

#include <QtTest>

#include <algorithm>

class SchemaValidatorTests final : public QObject
{
    Q_OBJECT

private slots:
    void detectsSupportedLanguages();
    void validatesJsonSchemaKeywords();
    void validatesYamlDocuments();
    void validatesTomlDocuments();
    void reportsParseErrorsWithLocations();
    void supportsCompositionAndLocalReferences();
};

void SchemaValidatorTests::detectsSupportedLanguages()
{
    QCOMPARE(SchemaValidator::formatForLanguage(QStringLiteral("JSON")), SchemaValidator::Format::Json);
    QCOMPARE(SchemaValidator::formatForLanguage(QStringLiteral("yaml")), SchemaValidator::Format::Yaml);
    QCOMPARE(SchemaValidator::formatForLanguage(QStringLiteral("TOML")), SchemaValidator::Format::Toml);
    QCOMPARE(SchemaValidator::formatForLanguage(QStringLiteral("INI")), SchemaValidator::Format::Unknown);
    QVERIFY(SchemaValidator::isSupportedLanguage(QStringLiteral("JSON")));
    QVERIFY(!SchemaValidator::isSupportedLanguage(QStringLiteral("Text")));
}

void SchemaValidatorTests::validatesJsonSchemaKeywords()
{
    const QString document = QStringLiteral(R"json({"name":"Notepad Next","version":14,"tags":["editor"]})json");
    const QString schema = QStringLiteral(R"json({
        "$schema":"https://json-schema.org/draft/2020-12/schema",
        "type":"object",
        "required":["name","version"],
        "additionalProperties":false,
        "properties":{
            "name":{"type":"string","minLength":3},
            "version":{"type":"integer","minimum":1},
            "tags":{"type":"array","items":{"type":"string"},"minItems":1}
        }
    })json");

    const auto valid = SchemaValidator::validate(document, schema, SchemaValidator::Format::Json);
    QVERIFY(valid.valid);
    QVERIFY(valid.documentParsed);
    QVERIFY(valid.schemaParsed);
    QVERIFY(valid.diagnostics.isEmpty());

    const auto invalid = SchemaValidator::validate(QStringLiteral(R"json({"name":"x","version":0,"unknown":true})json"), schema, SchemaValidator::Format::Json);
    QVERIFY(!invalid.valid);
    QCOMPARE(invalid.diagnostics.size(), 3);
    QCOMPARE(invalid.diagnostics.at(0).instancePath, QStringLiteral("/name"));
    QCOMPARE(invalid.diagnostics.at(0).line, 0);
    QVERIFY(invalid.diagnostics.at(0).column > 0);
}

void SchemaValidatorTests::validatesYamlDocuments()
{
    const QString document = QStringLiteral("name: Notepad Next\nversion: 14\nenabled: true\n");
    const QString schema = QStringLiteral(R"json({
        "type":"object",
        "required":["name","version"],
        "properties":{
            "name":{"type":"string"},
            "version":{"type":"integer"},
            "enabled":{"type":"boolean"}
        }
    })json");

    QVERIFY(SchemaValidator::validate(document, schema, SchemaValidator::Format::Yaml).valid);
    const auto invalid = SchemaValidator::validate(QStringLiteral("name: Notepad Next\nversion: wrong\n"), schema, SchemaValidator::Format::Yaml);
    QVERIFY(!invalid.valid);
    QVERIFY(!invalid.diagnostics.isEmpty());
    QCOMPARE(invalid.diagnostics.constFirst().instancePath, QStringLiteral("/version"));
    QCOMPARE(invalid.diagnostics.constFirst().line, 1);
}

void SchemaValidatorTests::validatesTomlDocuments()
{
    const QString document = QStringLiteral("title = \"Notepad Next\"\nversion = 14\n\n[editor]\n\tline_numbers = true\n");
    const QString schema = QStringLiteral(R"json({
        "type":"object",
        "required":["title","version","editor"],
        "properties":{
            "title":{"type":"string"},
            "version":{"type":"integer"},
            "editor":{"type":"object","required":["line_numbers"],"properties":{"line_numbers":{"type":"boolean"}}}
        }
    })json");

    const auto valid = SchemaValidator::validate(document, schema, SchemaValidator::Format::Toml);
    QVERIFY2(valid.valid, qPrintable(valid.diagnostics.isEmpty() ? QString() : valid.diagnostics.constFirst().message));
    const auto invalid = SchemaValidator::validate(QStringLiteral("title = 14\nversion = 14\n\n[editor]\nline_numbers = true\n"), schema, SchemaValidator::Format::Toml);
    QVERIFY(!invalid.valid);
    QCOMPARE(invalid.diagnostics.constFirst().instancePath, QStringLiteral("/title"));
}

void SchemaValidatorTests::reportsParseErrorsWithLocations()
{
    const auto json = SchemaValidator::validate(QStringLiteral("{\n  \"name\":\n}"), QStringLiteral("{}"), SchemaValidator::Format::Json);
    QVERIFY(!json.valid);
    QCOMPARE(json.diagnostics.constFirst().line, 2);

    const auto yaml = SchemaValidator::validate(QStringLiteral("name: [one\n"), QStringLiteral("{}"), SchemaValidator::Format::Yaml);
    QVERIFY(!yaml.valid);
    QVERIFY(yaml.diagnostics.constFirst().message.contains(QStringLiteral("parse"), Qt::CaseInsensitive));
}

void SchemaValidatorTests::supportsCompositionAndLocalReferences()
{
    const QString schema = QStringLiteral(R"json({
        "$defs":{"port":{"type":"integer","minimum":1,"maximum":65535}},
        "type":"object",
        "properties":{"port":{"$ref":"#/$defs/port"},"mode":{"enum":["read","write"]}},
        "required":["port"],
        "anyOf":[{"required":["mode"]},{"required":["port"]}]
    })json");

    QVERIFY(SchemaValidator::validate(QStringLiteral(R"json({"port":8080})json"), schema, SchemaValidator::Format::Json).valid);
    const auto invalid = SchemaValidator::validate(QStringLiteral(R"json({"port":0,"mode":"other"})json"), schema, SchemaValidator::Format::Json);
    QVERIFY(!invalid.valid);
    QVERIFY(std::any_of(invalid.diagnostics.cbegin(), invalid.diagnostics.cend(), [](const auto &entry) {
        return entry.instancePath == QStringLiteral("/port");
    }));
}

QTEST_MAIN(SchemaValidatorTests)
#include "SchemaValidatorTests.moc"
