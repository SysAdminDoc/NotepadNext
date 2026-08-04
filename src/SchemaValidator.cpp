/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SchemaValidator.h"

#include <yaml-cpp/yaml.h>
#include <toml++/toml.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace
{
struct ParsedValue
{
    QJsonValue value;
    bool valid = false;
    QString error;
    int line = 0;
    int column = 0;
};

struct SourceLocation
{
    int line = 0;
    int column = 0;
};

QString jsonPointerEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("~"), QStringLiteral("~0"));
    escaped.replace(QStringLiteral("/"), QStringLiteral("~1"));
    return escaped;
}

QString jsonPointerUnescape(QString value)
{
    value.replace(QStringLiteral("~1"), QStringLiteral("/"));
    value.replace(QStringLiteral("~0"), QStringLiteral("~"));
    return value;
}

SourceLocation locationAt(const QString &source, int position)
{
    position = qBound(0, position, source.size());
    const int line = source.left(position).count(QChar::fromLatin1('\n'));
    const int lineStart = source.lastIndexOf(QChar::fromLatin1('\n'), position - 1);
    return {line, position - (lineStart < 0 ? 0 : lineStart + 1)};
}

SourceLocation rootLocation(const QString &source)
{
    int position = 0;
    while (position < source.size() && source.at(position).isSpace()) {
        ++position;
    }
    return locationAt(source, position);
}

QString pathLeaf(const QString &instancePath)
{
    if (instancePath.isEmpty()) {
        return QString();
    }

    const QStringList parts = instancePath.mid(1).split(QChar::fromLatin1('/'), Qt::KeepEmptyParts);
    for (auto it = parts.crbegin(); it != parts.crend(); ++it) {
        const QString part = jsonPointerUnescape(*it);
        bool isIndex = false;
        part.toInt(&isIndex);
        if (!isIndex && !part.isEmpty()) {
            return part;
        }
    }
    return QString();
}

SourceLocation locationForPath(const QString &source, SchemaValidator::Format format, const QString &instancePath)
{
    const QString leaf = pathLeaf(instancePath);
    if (leaf.isEmpty()) {
        return rootLocation(source);
    }

    QString pattern;
    switch (format) {
    case SchemaValidator::Format::Json:
        pattern = QStringLiteral("\\\"%1\\\"\\s*:").arg(QRegularExpression::escape(leaf));
        break;
    case SchemaValidator::Format::Yaml:
        pattern = QStringLiteral("(?m)^\\s*(?:\\\"%1\\\"|'%1'|%1)\\s*:").arg(QRegularExpression::escape(leaf));
        break;
    case SchemaValidator::Format::Toml:
        pattern = QStringLiteral("(?m)^\\s*%1\\s*=").arg(QRegularExpression::escape(leaf));
        break;
    case SchemaValidator::Format::Unknown:
        break;
    }

    if (!pattern.isEmpty()) {
        const QRegularExpressionMatch match = QRegularExpression(pattern).match(source);
        if (match.hasMatch()) {
            return locationAt(source, match.capturedStart());
        }
    }
    return rootLocation(source);
}

SchemaValidator::Diagnostic diagnostic(const QString &message, const QString &path,
                                       const QString &source, SchemaValidator::Format format,
                                       int line = -1, int column = -1, int length = 1)
{
    SchemaValidator::Diagnostic result;
    result.message = message;
    result.instancePath = path;
    result.length = qMax(1, length);
    if (line >= 0 && column >= 0) {
        result.line = line;
        result.column = column;
    }
    else {
        const SourceLocation location = locationForPath(source, format, path);
        result.line = location.line;
        result.column = location.column;
    }
    return result;
}

ParsedValue parseJsonValue(const QString &source)
{
    const QByteArray data = source.toUtf8();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error == QJsonParseError::NoError && !document.isNull()) {
        if (document.isObject()) {
            return {QJsonValue(document.object()), true, {}, 0, 0};
        }
        if (document.isArray()) {
            return {QJsonValue(document.array()), true, {}, 0, 0};
        }
    }

    // QJsonDocument only exposes object and array roots. Wrapping the source
    // lets schema and document parsing accept JSON primitive roots as well.
    const QByteArray wrapped = QByteArrayLiteral("[") + data + QByteArrayLiteral("]");
    QJsonParseError wrappedError;
    const QJsonDocument wrappedDocument = QJsonDocument::fromJson(wrapped, &wrappedError);
    if (wrappedError.error == QJsonParseError::NoError && wrappedDocument.isArray() && wrappedDocument.array().size() == 1) {
        return {wrappedDocument.array().at(0), true, {}, 0, 0};
    }

    qint64 errorOffset = wrappedError.error == QJsonParseError::NoError ? error.offset : wrappedError.offset - 1;
    errorOffset = qMax<qint64>(0, errorOffset);
    const int utf8PrefixLength = static_cast<int>(qBound<qint64>(0, errorOffset, data.size()));
    const SourceLocation location = locationAt(source, QString::fromUtf8(data.left(utf8PrefixLength)).size());
    const QString message = wrappedError.error == QJsonParseError::NoError
        ? QStringLiteral("JSON document must contain exactly one value")
        : wrappedError.errorString();
    return {{}, false, message, location.line, location.column};
}

QJsonValue yamlValue(const YAML::Node &node, QString *error)
{
    if (!node || node.IsNull()) {
        return QJsonValue(QJsonValue::Null);
    }

    if (node.IsSequence()) {
        QJsonArray array;
        for (const YAML::Node &child : node) {
            array.append(yamlValue(child, error));
            if (error && !error->isEmpty()) {
                return {};
            }
        }
        return array;
    }

    if (node.IsMap()) {
        QJsonObject object;
        for (const auto &entry : node) {
            if (!entry.first.IsScalar()) {
                if (error) {
                    *error = QStringLiteral("YAML mapping keys must be scalar values");
                }
                return {};
            }
            const QString key = QString::fromStdString(entry.first.Scalar());
            if (object.contains(key)) {
                if (error) {
                    *error = QStringLiteral("YAML mapping contains duplicate key '%1'").arg(key);
                }
                return {};
            }
            object.insert(key, yamlValue(entry.second, error));
            if (error && !error->isEmpty()) {
                return {};
            }
        }
        return object;
    }

    if (!node.IsScalar()) {
        if (error) {
            *error = QStringLiteral("Unsupported YAML node type");
        }
        return {};
    }

    const QString scalar = QString::fromStdString(node.Scalar());
    const QString tag = QString::fromStdString(node.Tag());
    if (tag.endsWith(QStringLiteral(":str"))) {
        return scalar;
    }
    if (tag.endsWith(QStringLiteral(":null")) || scalar == QStringLiteral("~") || scalar.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0) {
        return QJsonValue(QJsonValue::Null);
    }
    if (tag.endsWith(QStringLiteral(":bool")) || scalar.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || scalar.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return scalar.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    }

    bool integerOk = false;
    const qlonglong integer = scalar.toLongLong(&integerOk);
    if (tag.endsWith(QStringLiteral(":int")) || integerOk) {
        return static_cast<double>(integer);
    }

    bool doubleOk = false;
    const double real = scalar.toDouble(&doubleOk);
    if (tag.endsWith(QStringLiteral(":float")) || doubleOk) {
        return real;
    }
    return scalar;
}

ParsedValue parseYamlValue(const QString &source)
{
    try {
        const YAML::Node node = YAML::Load(source.toStdString());
        QString conversionError;
        const QJsonValue value = yamlValue(node, &conversionError);
        if (!conversionError.isEmpty()) {
            return {{}, false, conversionError, 0, 0};
        }
        return {value, true, {}, 0, 0};
    }
    catch (const YAML::Exception &exception) {
        return {{}, false, QString::fromUtf8(exception.what()), exception.mark.line, exception.mark.column};
    }
    catch (const std::exception &exception) {
        return {{}, false, QString::fromUtf8(exception.what()), 0, 0};
    }
}

ParsedValue parseTomlValue(const QString &source)
{
    try {
        const std::string input = source.toStdString();
        const toml::table table = toml::parse(input);
        std::ostringstream stream;
        stream << toml::json_formatter{table};
        return parseJsonValue(QString::fromStdString(stream.str()));
    }
    catch (const std::exception &exception) {
        return {{}, false, QString::fromUtf8(exception.what()), 0, 0};
    }
}

ParsedValue parseDocument(const QString &source, SchemaValidator::Format format)
{
    switch (format) {
    case SchemaValidator::Format::Json:
        return parseJsonValue(source);
    case SchemaValidator::Format::Yaml:
        return parseYamlValue(source);
    case SchemaValidator::Format::Toml:
        return parseTomlValue(source);
    case SchemaValidator::Format::Unknown:
        return {{}, false, QStringLiteral("Unsupported document language"), 0, 0};
    }
    return {{}, false, QStringLiteral("Unsupported document language"), 0, 0};
}

bool typeMatches(const QJsonValue &value, const QString &type)
{
    if (type == QStringLiteral("object")) return value.isObject();
    if (type == QStringLiteral("array")) return value.isArray();
    if (type == QStringLiteral("string")) return value.isString();
    if (type == QStringLiteral("number")) return value.isDouble();
    if (type == QStringLiteral("integer")) return value.isDouble() && std::floor(value.toDouble()) == value.toDouble();
    if (type == QStringLiteral("boolean")) return value.isBool();
    if (type == QStringLiteral("null")) return value.isNull();
    return false;
}

QJsonValue resolveLocalReference(const QJsonValue &root, const QString &reference)
{
    if (reference == QStringLiteral("#")) {
        return root;
    }
    if (!reference.startsWith(QStringLiteral("#/"))) {
        return QJsonValue(QJsonValue::Undefined);
    }

    QJsonValue current = root;
    const QStringList parts = reference.mid(2).split(QChar::fromLatin1('/'), Qt::KeepEmptyParts);
    for (const QString &part : parts) {
        const QString key = jsonPointerUnescape(part);
        if (current.isObject()) {
            current = current.toObject().value(key);
        }
        else if (current.isArray()) {
            bool ok = false;
            const int index = key.toInt(&ok);
            if (!ok || index < 0 || index >= current.toArray().size()) {
                return QJsonValue(QJsonValue::Undefined);
            }
            current = current.toArray().at(index);
        }
        else {
            return QJsonValue(QJsonValue::Undefined);
        }
        if (current.isUndefined()) {
            return current;
        }
    }
    return current;
}

void addDiagnostic(QVector<SchemaValidator::Diagnostic> &diagnostics, const QString &message,
                   const QString &path, const QString &source, SchemaValidator::Format format)
{
    diagnostics.append(diagnostic(message, path, source, format));
}

bool validateValue(const QJsonValue &value, const QJsonValue &schema, const QJsonValue &schemaRoot,
                   const QString &path, const QString &source, SchemaValidator::Format format,
                   QVector<SchemaValidator::Diagnostic> &diagnostics, const QSet<QString> &activeReferences = {})
{
    if (schema.isBool()) {
        if (!schema.toBool()) {
            addDiagnostic(diagnostics, QStringLiteral("Value does not satisfy the schema"), path, source, format);
            return false;
        }
        return true;
    }
    if (!schema.isObject()) {
        addDiagnostic(diagnostics, QStringLiteral("Schema must be an object or boolean"), path, source, format);
        return false;
    }

    const QJsonObject schemaObject = schema.toObject();
    bool valid = true;

    const QJsonValue reference = schemaObject.value(QStringLiteral("$ref"));
    if (!reference.isUndefined()) {
        if (!reference.isString() || !reference.toString().startsWith(QStringLiteral("#"))) {
            addDiagnostic(diagnostics, QStringLiteral("Only local JSON Schema references are supported"), path, source, format);
            valid = false;
        }
        else if (!activeReferences.contains(reference.toString())) {
            const QJsonValue resolved = resolveLocalReference(schemaRoot, reference.toString());
            if (resolved.isUndefined()) {
                addDiagnostic(diagnostics, QStringLiteral("JSON Schema reference '%1' could not be resolved").arg(reference.toString()), path, source, format);
                valid = false;
            }
            else {
                QSet<QString> nestedReferences = activeReferences;
                nestedReferences.insert(reference.toString());
                valid = validateValue(value, resolved, schemaRoot, path, source, format, diagnostics, nestedReferences) && valid;
            }
        }
    }

    const QJsonValue type = schemaObject.value(QStringLiteral("type"));
    bool typeValid = true;
    if (type.isString()) {
        typeValid = typeMatches(value, type.toString());
    }
    else if (type.isArray()) {
        typeValid = false;
        for (const QJsonValue &candidate : type.toArray()) {
            if (candidate.isString() && typeMatches(value, candidate.toString())) {
                typeValid = true;
                break;
            }
        }
    }
    else if (!type.isUndefined()) {
        addDiagnostic(diagnostics, QStringLiteral("Schema 'type' must be a string or array"), path, source, format);
        typeValid = false;
    }
    if (!typeValid) {
        QString expected;
        if (type.isString()) {
            expected = type.toString();
        }
        else if (type.isArray()) {
            QStringList types;
            for (const QJsonValue &candidate : type.toArray()) {
                types.append(candidate.toString());
            }
            expected = types.join(QStringLiteral(", "));
        }
        addDiagnostic(diagnostics, QStringLiteral("Expected value of type %1").arg(expected), path, source, format);
        return false;
    }

    const QJsonValue enumValues = schemaObject.value(QStringLiteral("enum"));
    if (enumValues.isArray()) {
        bool found = false;
        for (const QJsonValue &candidate : enumValues.toArray()) {
            if (value == candidate) {
                found = true;
                break;
            }
        }
        if (!found) {
            addDiagnostic(diagnostics, QStringLiteral("Value is not one of the allowed enum values"), path, source, format);
            valid = false;
        }
    }

    const QJsonValue constant = schemaObject.value(QStringLiteral("const"));
    if (!constant.isUndefined() && !(value == constant)) {
        addDiagnostic(diagnostics, QStringLiteral("Value does not match the schema constant"), path, source, format);
        valid = false;
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QJsonValue required = schemaObject.value(QStringLiteral("required"));
        if (required.isArray()) {
            for (const QJsonValue &requiredValue : required.toArray()) {
                if (requiredValue.isString() && !object.contains(requiredValue.toString())) {
                    const QString childPath = path + QStringLiteral("/") + jsonPointerEscape(requiredValue.toString());
                    addDiagnostic(diagnostics, QStringLiteral("Required property '%1' is missing").arg(requiredValue.toString()), childPath, source, format);
                    valid = false;
                }
            }
        }

        const QJsonObject properties = schemaObject.value(QStringLiteral("properties")).toObject();
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            if (object.contains(it.key())) {
                const QString childPath = path + QStringLiteral("/") + jsonPointerEscape(it.key());
                valid = validateValue(object.value(it.key()), it.value(), schemaRoot, childPath, source, format, diagnostics, activeReferences) && valid;
            }
        }

        const QJsonValue additionalProperties = schemaObject.value(QStringLiteral("additionalProperties"));
        if (!additionalProperties.isUndefined()) {
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (properties.contains(it.key())) {
                    continue;
                }
                const QString childPath = path + QStringLiteral("/") + jsonPointerEscape(it.key());
                if (additionalProperties.isBool() && !additionalProperties.toBool()) {
                    addDiagnostic(diagnostics, QStringLiteral("Additional property '%1' is not allowed").arg(it.key()), childPath, source, format);
                    valid = false;
                }
                else if (additionalProperties.isObject() || additionalProperties.isBool()) {
                    valid = validateValue(it.value(), additionalProperties, schemaRoot, childPath, source, format, diagnostics, activeReferences) && valid;
                }
            }
        }

        const QJsonValue minProperties = schemaObject.value(QStringLiteral("minProperties"));
        if (minProperties.isDouble() && object.size() < minProperties.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("Object must contain at least %1 properties").arg(minProperties.toInt()), path, source, format);
            valid = false;
        }
        const QJsonValue maxProperties = schemaObject.value(QStringLiteral("maxProperties"));
        if (maxProperties.isDouble() && object.size() > maxProperties.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("Object must contain no more than %1 properties").arg(maxProperties.toInt()), path, source, format);
            valid = false;
        }
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        const QJsonValue items = schemaObject.value(QStringLiteral("items"));
        if (items.isObject() || items.isBool()) {
            for (int index = 0; index < array.size(); ++index) {
                const QString childPath = path + QStringLiteral("/") + QString::number(index);
                valid = validateValue(array.at(index), items, schemaRoot, childPath, source, format, diagnostics, activeReferences) && valid;
            }
        }

        const QJsonValue minItems = schemaObject.value(QStringLiteral("minItems"));
        if (minItems.isDouble() && array.size() < minItems.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("Array must contain at least %1 items").arg(minItems.toInt()), path, source, format);
            valid = false;
        }
        const QJsonValue maxItems = schemaObject.value(QStringLiteral("maxItems"));
        if (maxItems.isDouble() && array.size() > maxItems.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("Array must contain no more than %1 items").arg(maxItems.toInt()), path, source, format);
            valid = false;
        }
        if (schemaObject.value(QStringLiteral("uniqueItems")).toBool(false)) {
            for (int i = 0; i < array.size(); ++i) {
                for (int j = i + 1; j < array.size(); ++j) {
                    if (array.at(i) == array.at(j)) {
                        addDiagnostic(diagnostics, QStringLiteral("Array items must be unique"), path, source, format);
                        valid = false;
                        i = array.size();
                        break;
                    }
                }
            }
        }
    }

    if (value.isString()) {
        const QString string = value.toString();
        const QJsonValue minLength = schemaObject.value(QStringLiteral("minLength"));
        if (minLength.isDouble() && string.size() < minLength.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("String must contain at least %1 characters").arg(minLength.toInt()), path, source, format);
            valid = false;
        }
        const QJsonValue maxLength = schemaObject.value(QStringLiteral("maxLength"));
        if (maxLength.isDouble() && string.size() > maxLength.toInt()) {
            addDiagnostic(diagnostics, QStringLiteral("String must contain no more than %1 characters").arg(maxLength.toInt()), path, source, format);
            valid = false;
        }
        const QJsonValue pattern = schemaObject.value(QStringLiteral("pattern"));
        if (pattern.isString()) {
            const QRegularExpression expression(pattern.toString());
            if (!expression.isValid()) {
                addDiagnostic(diagnostics, QStringLiteral("Schema contains an invalid regular expression"), path, source, format);
                valid = false;
            }
            else if (!expression.match(string).hasMatch()) {
                addDiagnostic(diagnostics, QStringLiteral("String does not match the required pattern"), path, source, format);
                valid = false;
            }
        }
    }

    if (value.isDouble()) {
        const double number = value.toDouble();
        const QJsonValue minimum = schemaObject.value(QStringLiteral("minimum"));
        if (minimum.isDouble() && number < minimum.toDouble()) {
            addDiagnostic(diagnostics, QStringLiteral("Number is less than the minimum of %1").arg(minimum.toDouble()), path, source, format);
            valid = false;
        }
        const QJsonValue maximum = schemaObject.value(QStringLiteral("maximum"));
        if (maximum.isDouble() && number > maximum.toDouble()) {
            addDiagnostic(diagnostics, QStringLiteral("Number is greater than the maximum of %1").arg(maximum.toDouble()), path, source, format);
            valid = false;
        }
        const QJsonValue exclusiveMinimum = schemaObject.value(QStringLiteral("exclusiveMinimum"));
        if (exclusiveMinimum.isDouble() && number <= exclusiveMinimum.toDouble()) {
            addDiagnostic(diagnostics, QStringLiteral("Number must be greater than %1").arg(exclusiveMinimum.toDouble()), path, source, format);
            valid = false;
        }
        const QJsonValue exclusiveMaximum = schemaObject.value(QStringLiteral("exclusiveMaximum"));
        if (exclusiveMaximum.isDouble() && number >= exclusiveMaximum.toDouble()) {
            addDiagnostic(diagnostics, QStringLiteral("Number must be less than %1").arg(exclusiveMaximum.toDouble()), path, source, format);
            valid = false;
        }
    }

    const QJsonValue allOf = schemaObject.value(QStringLiteral("allOf"));
    if (allOf.isArray()) {
        for (const QJsonValue &subschema : allOf.toArray()) {
            valid = validateValue(value, subschema, schemaRoot, path, source, format, diagnostics, activeReferences) && valid;
        }
    }

    const QJsonValue anyOf = schemaObject.value(QStringLiteral("anyOf"));
    if (anyOf.isArray()) {
        bool matched = false;
        for (const QJsonValue &subschema : anyOf.toArray()) {
            QVector<SchemaValidator::Diagnostic> branchDiagnostics;
            if (validateValue(value, subschema, schemaRoot, path, source, format, branchDiagnostics, activeReferences)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            addDiagnostic(diagnostics, QStringLiteral("Value must satisfy at least one schema in anyOf"), path, source, format);
            valid = false;
        }
    }

    const QJsonValue oneOf = schemaObject.value(QStringLiteral("oneOf"));
    if (oneOf.isArray()) {
        int matches = 0;
        for (const QJsonValue &subschema : oneOf.toArray()) {
            QVector<SchemaValidator::Diagnostic> branchDiagnostics;
            if (validateValue(value, subschema, schemaRoot, path, source, format, branchDiagnostics, activeReferences)) {
                ++matches;
            }
        }
        if (matches != 1) {
            addDiagnostic(diagnostics, QStringLiteral("Value must satisfy exactly one schema in oneOf"), path, source, format);
            valid = false;
        }
    }

    const QJsonValue notSchema = schemaObject.value(QStringLiteral("not"));
    if (!notSchema.isUndefined()) {
        QVector<SchemaValidator::Diagnostic> branchDiagnostics;
        if (validateValue(value, notSchema, schemaRoot, path, source, format, branchDiagnostics, activeReferences)) {
            addDiagnostic(diagnostics, QStringLiteral("Value must not satisfy the schema in not"), path, source, format);
            valid = false;
        }
    }

    return valid;
}
}

SchemaValidator::Format SchemaValidator::formatForLanguage(const QString &languageName)
{
    if (languageName.compare(QStringLiteral("JSON"), Qt::CaseInsensitive) == 0) {
        return Format::Json;
    }
    if (languageName.compare(QStringLiteral("YAML"), Qt::CaseInsensitive) == 0) {
        return Format::Yaml;
    }
    if (languageName.compare(QStringLiteral("TOML"), Qt::CaseInsensitive) == 0) {
        return Format::Toml;
    }
    return Format::Unknown;
}

bool SchemaValidator::isSupportedLanguage(const QString &languageName)
{
    return formatForLanguage(languageName) != Format::Unknown;
}

SchemaValidator::Result SchemaValidator::validate(const QString &document, const QString &schema, Format format)
{
    Result result;
    const ParsedValue parsedDocument = parseDocument(document, format);
    if (!parsedDocument.valid) {
        result.diagnostics.append(diagnostic(QStringLiteral("Unable to parse document: %1").arg(parsedDocument.error), {}, document, format,
                                             parsedDocument.line, parsedDocument.column));
        return result;
    }
    result.documentParsed = true;

    const ParsedValue parsedSchema = parseJsonValue(schema);
    if (!parsedSchema.valid) {
        result.diagnostics.append(diagnostic(QStringLiteral("Unable to parse JSON Schema: %1").arg(parsedSchema.error), {}, document, format));
        return result;
    }
    result.schemaParsed = true;

    if (!parsedSchema.value.isObject() && !parsedSchema.value.isBool()) {
        result.diagnostics.append(diagnostic(QStringLiteral("JSON Schema root must be an object or boolean"), {}, document, format));
        return result;
    }

    validateValue(parsedDocument.value, parsedSchema.value, parsedSchema.value, {}, document, format, result.diagnostics);
    result.valid = result.diagnostics.isEmpty();
    return result;
}
