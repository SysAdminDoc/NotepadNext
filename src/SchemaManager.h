/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SCHEMAMANAGER_H
#define SCHEMAMANAGER_H

#include "SchemaValidator.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class EditorManager;
class ScintillaNext;

class SchemaManager final : public QObject
{
    Q_OBJECT

public:
    explicit SchemaManager(EditorManager *editorManager, QObject *parent = nullptr);
    ~SchemaManager() override;

    SchemaValidator::Result setSchemaFile(ScintillaNext *editor, const QString &schemaPath);
    SchemaValidator::Result clearSchemaFile(ScintillaNext *editor);
    SchemaValidator::Result validateNow(ScintillaNext *editor);

    QString schemaFileForEditor(const ScintillaNext *editor) const;

private:
    struct EditorState
    {
        QString explicitSchemaPath;
        int errorIndicator = -1;
    };

    void attachEditor(ScintillaNext *editor);
    void detachEditor(ScintillaNext *editor);
    void editorModified(ScintillaNext *editor);
    void scheduleValidation(ScintillaNext *editor);
    void validatePendingEditors();
    void clearDiagnostics(ScintillaNext *editor);
    void showDiagnostics(ScintillaNext *editor, const SchemaValidator::Result &result);

    QString discoverSchemaFile(const ScintillaNext *editor, const EditorState &state) const;
    QString resolveSchemaReference(const ScintillaNext *editor, const QString &reference) const;
    static QByteArray editorText(ScintillaNext *editor);
    static SchemaValidator::Result errorResult(const QString &message);

    EditorManager *editorManager = nullptr;
    QHash<ScintillaNext *, EditorState> states;
    QSet<ScintillaNext *> pendingEditors;
    QTimer validationTimer;
};

#endif // SCHEMAMANAGER_H
