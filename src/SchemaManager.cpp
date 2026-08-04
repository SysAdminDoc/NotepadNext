/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SchemaManager.h"

#include "EditorManager.h"
#include "ScintillaNext.h"

#include <Scintilla.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QRegularExpression>
#include <QUrl>

#include <utility>

namespace
{
constexpr int SchemaErrorIndicatorColor = 0x0000FF;
constexpr int SchemaValidationDelayMs = 350;

void configureIndicator(ScintillaNext *editor, int indicator)
{
    if (indicator < 0) {
        return;
    }
    editor->indicSetStyle(indicator, INDIC_SQUIGGLE);
    editor->indicSetFore(indicator, SchemaErrorIndicatorColor);
    editor->indicSetUnder(indicator, true);
    editor->indicSetAlpha(indicator, 180);
}

QStringList schemaCandidates(const QFileInfo &fileInfo)
{
    return {
        fileInfo.absoluteFilePath() + QStringLiteral(".schema.json"),
        fileInfo.absolutePath() + QDir::separator() + fileInfo.completeBaseName() + QStringLiteral(".schema.json"),
        fileInfo.absolutePath() + QDir::separator() + QStringLiteral(".notepadnext-schema.json")
    };
}
}

SchemaManager::SchemaManager(EditorManager *editorManager, QObject *parent)
    : QObject(parent), editorManager(editorManager)
{
    Q_ASSERT(editorManager != nullptr);

    validationTimer.setSingleShot(true);
    validationTimer.setInterval(SchemaValidationDelayMs);
    connect(&validationTimer, &QTimer::timeout, this, &SchemaManager::validatePendingEditors);
    connect(editorManager, &EditorManager::editorCreated, this, &SchemaManager::attachEditor);
    connect(editorManager, &EditorManager::editorClosed, this, &SchemaManager::detachEditor);
}

SchemaManager::~SchemaManager()
{
    const QList<ScintillaNext *> editors = states.keys();
    for (ScintillaNext *editor : editors) {
        detachEditor(editor);
    }
}

void SchemaManager::attachEditor(ScintillaNext *editor)
{
    if (!editor || states.contains(editor)) {
        return;
    }

    EditorState state;
    state.errorIndicator = editor->allocateIndicator(QStringLiteral("schema-error"));
    configureIndicator(editor, state.errorIndicator);
    states.insert(editor, state);

    editor->setModEventMask(editor->modEventMask() | SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);

    connect(editor, &ScintillaNext::notify, this, [this, editor](Scintilla::NotificationData *notification) {
        if (notification && notification->nmhdr.code == Scintilla::Notification::Modified &&
            (Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::InsertText) ||
             Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::DeleteText))) {
            editorModified(editor);
        }
    });
    connect(editor, &ScintillaNext::lexerChanged, this, [this, editor]() {
        clearDiagnostics(editor);
        scheduleValidation(editor);
    });
    connect(editor, &ScintillaNext::renamed, this, [this, editor]() {
        clearDiagnostics(editor);
        scheduleValidation(editor);
    });
    connect(editor, &ScintillaNext::saved, this, [this, editor]() {
        scheduleValidation(editor);
    });
    connect(editor, &ScintillaNext::reloaded, this, [this, editor]() {
        editorModified(editor);
    });

    QMetaObject::invokeMethod(this, [this, editor]() {
        if (states.contains(editor)) {
            scheduleValidation(editor);
        }
    }, Qt::QueuedConnection);
}

void SchemaManager::detachEditor(ScintillaNext *editor)
{
    if (!editor) {
        return;
    }

    pendingEditors.remove(editor);
    states.remove(editor);
}

void SchemaManager::editorModified(ScintillaNext *editor)
{
    if (!states.contains(editor)) {
        return;
    }
    clearDiagnostics(editor);
    scheduleValidation(editor);
}

void SchemaManager::scheduleValidation(ScintillaNext *editor)
{
    if (!editor || !states.contains(editor)) {
        return;
    }
    pendingEditors.insert(editor);
    validationTimer.start();
}

void SchemaManager::validatePendingEditors()
{
    const QSet<ScintillaNext *> editors = std::exchange(pendingEditors, {});
    for (ScintillaNext *editor : editors) {
        if (states.contains(editor)) {
            validateNow(editor);
        }
    }
}

void SchemaManager::clearDiagnostics(ScintillaNext *editor)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd() || it->errorIndicator < 0) {
        return;
    }

    editor->setIndicatorCurrent(it->errorIndicator);
    editor->indicatorClearRange(0, static_cast<int>(editor->length()));
}

void SchemaManager::showDiagnostics(ScintillaNext *editor, const SchemaValidator::Result &result)
{
    const auto it = states.constFind(editor);
    if (it == states.constEnd() || it->errorIndicator < 0) {
        return;
    }

    clearDiagnostics(editor);
    const int documentLength = static_cast<int>(editor->length());
    if (documentLength == 0) {
        return;
    }

    editor->setIndicatorCurrent(it->errorIndicator);
    const int lineCount = static_cast<int>(editor->lineCount());
    for (const SchemaValidator::Diagnostic &diagnostic : result.diagnostics) {
        const int line = qBound(0, diagnostic.line, qMax(0, lineCount - 1));
        const int lineStart = static_cast<int>(editor->positionFromLine(line));
        const int lineEnd = static_cast<int>(editor->lineEndPosition(line));
        const QString lineText = QString::fromUtf8(editor->textRange(lineStart, lineEnd));
        const int column = qBound(0, diagnostic.column, lineText.size());
        const int start = qMin(lineStart + static_cast<int>(lineText.left(column).toUtf8().size()), lineEnd);
        int end = qMin(lineEnd, start + qMax(1, diagnostic.length));
        if (end == start && start < documentLength) {
            end = start + 1;
        }
        if (end > start) {
            editor->indicatorFillRange(start, end - start);
        }
    }
}

SchemaValidator::Result SchemaManager::setSchemaFile(ScintillaNext *editor, const QString &schemaPath)
{
    if (!editor || !states.contains(editor)) {
        return errorResult(QStringLiteral("The document is no longer open"));
    }

    states[editor].explicitSchemaPath = QFileInfo(schemaPath).absoluteFilePath();
    return validateNow(editor);
}

SchemaValidator::Result SchemaManager::clearSchemaFile(ScintillaNext *editor)
{
    if (!editor || !states.contains(editor)) {
        return errorResult(QStringLiteral("The document is no longer open"));
    }

    states[editor].explicitSchemaPath.clear();
    return validateNow(editor);
}

SchemaValidator::Result SchemaManager::validateNow(ScintillaNext *editor)
{
    if (!editor || !states.contains(editor)) {
        return errorResult(QStringLiteral("The document is no longer open"));
    }

    const SchemaValidator::Format format = SchemaValidator::formatForLanguage(editor->languageName);
    if (format == SchemaValidator::Format::Unknown) {
        clearDiagnostics(editor);
        return errorResult(QStringLiteral("Schema validation supports JSON, YAML, and TOML documents only"));
    }

    const QString schemaPath = schemaFileForEditor(editor);
    if (schemaPath.isEmpty()) {
        clearDiagnostics(editor);
        SchemaValidator::Result result;
        result.valid = true;
        return result;
    }

    QFile schemaFile(schemaPath);
    if (!schemaFile.open(QIODevice::ReadOnly)) {
        const SchemaValidator::Result result = errorResult(QStringLiteral("Unable to open schema '%1': %2").arg(schemaPath, schemaFile.errorString()));
        showDiagnostics(editor, result);
        return result;
    }

    const SchemaValidator::Result result = SchemaValidator::validate(QString::fromUtf8(editorText(editor)),
                                                                       QString::fromUtf8(schemaFile.readAll()), format);
    showDiagnostics(editor, result);
    return result;
}

QString SchemaManager::schemaFileForEditor(const ScintillaNext *editor) const
{
    if (!editor) {
        return QString();
    }
    const auto it = states.constFind(const_cast<ScintillaNext *>(editor));
    if (it == states.constEnd()) {
        return QString();
    }
    return discoverSchemaFile(editor, it.value());
}

QString SchemaManager::discoverSchemaFile(const ScintillaNext *editor, const EditorState &state) const
{
    if (!editor || !editor->isFile()) {
        return QString();
    }
    if (!state.explicitSchemaPath.isEmpty()) {
        return state.explicitSchemaPath;
    }

    const QFileInfo fileInfo(editor->getFilePath());
    for (const QString &candidate : schemaCandidates(fileInfo)) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    const QString source = QString::fromUtf8(editorText(const_cast<ScintillaNext *>(editor)));
    const QRegularExpression directive(QStringLiteral(R"((?im)^\s*(?:#|//|/\*)\s*(?:yaml-language-server:\s*\$schema=|schema:\s*)([^\s*]+))"));
    const QRegularExpressionMatch directiveMatch = directive.match(source);
    if (directiveMatch.hasMatch()) {
        const QString resolved = resolveSchemaReference(editor, directiveMatch.captured(1));
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }

    // A local $schema value is useful for small checked-in projects. Remote
    // references are intentionally ignored because validation is offline.
    const QRegularExpression jsonSchema(QStringLiteral(R"SCHEMA("\$schema"\s*:\s*"([^"]+)")SCHEMA"));
    const QRegularExpressionMatch jsonSchemaMatch = jsonSchema.match(source);
    if (jsonSchemaMatch.hasMatch()) {
        const QString resolved = resolveSchemaReference(editor, jsonSchemaMatch.captured(1));
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }

    return QString();
}

QString SchemaManager::resolveSchemaReference(const ScintillaNext *editor, const QString &reference) const
{
    QString value = reference.trimmed();
    if ((value.startsWith(QChar::fromLatin1('\"')) && value.endsWith(QChar::fromLatin1('\"'))) ||
        (value.startsWith(QChar::fromLatin1('\'')) && value.endsWith(QChar::fromLatin1('\'')))) {
        value = value.mid(1, value.size() - 2);
    }

    const QUrl url(value);
    if (url.isValid() && url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).absoluteFilePath();
    }
    if (url.isValid() && !url.scheme().isEmpty()) {
        return QString();
    }

    if (!editor || !editor->isFile() || value.isEmpty()) {
        return QString();
    }
    return QFileInfo(QFileInfo(editor->getFilePath()).absolutePath(), value).absoluteFilePath();
}

QByteArray SchemaManager::editorText(ScintillaNext *editor)
{
    return editor ? editor->getText(editor->textLength()) : QByteArray();
}

SchemaValidator::Result SchemaManager::errorResult(const QString &message)
{
    SchemaValidator::Result result;
    result.diagnostics.append({message, {}, 0, 0, 1});
    return result;
}
