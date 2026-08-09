/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CAPABILITYTRUST_H
#define CAPABILITYTRUST_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class QSettings;
class QWidget;

namespace CapabilityTrust
{
enum class Capability {
    JavaScriptExecution,
    ScriptFileAccess,
    ScriptDocumentEdit,
    ScriptDocumentSave,
    ScriptDocumentOpen,
    Terminal,
    LuaConsole,
    TrustedStartup,
};

struct AuditEntry
{
    QDateTime timestamp;
    QString workspaceId;
    QString capability;
    QString operation;
    QString result;
};

QString capabilityName(Capability capability);

class Manager final : public QObject
{
    Q_OBJECT

public:
    explicit Manager(QSettings *settings, QObject *parent = nullptr);

    static QString workspaceRootForPath(const QString &path);
    static QString workspaceId(const QString &workspaceRoot);

    bool isGranted(const QString &workspaceRoot, Capability capability) const;
    void grant(const QString &workspaceRoot, Capability capability, bool persist);
    void revoke(const QString &workspaceRoot, Capability capability);
    void revokeAll(const QString &workspaceRoot);
    void clearSession();

    // Prompts only when a capability has not been explicitly granted. A null
    // parent fails closed, which also keeps headless callers non-interactive.
    bool authorize(QWidget *parent, const QString &workspaceRoot, Capability capability,
                   const QString &operation);

    void record(const QString &workspaceRoot, Capability capability,
                const QString &operation, const QString &result);
    QVector<AuditEntry> auditEntries() const;

signals:
    void capabilityChanged(const QString &workspaceRoot, Capability capability);
    void auditRecorded(const QString &workspaceId, const QString &operation, const QString &result);

private:
    QString normalizedWorkspace(const QString &workspaceRoot) const;
    QString grantKey(const QString &workspaceRoot, Capability capability) const;

    QSettings *settings = nullptr;
    QHash<QString, QSet<int>> sessionGrants;
};
}

#endif // CAPABILITYTRUST_H
