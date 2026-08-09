/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "CapabilityTrust.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVariantMap>

namespace
{
constexpr auto GrantsPrefix = "Security/WorkspaceGrants/";
constexpr auto AuditKey = "Security/AuditTrail";
constexpr int MaximumAuditEntries = 128;

const QStringList WorkspaceMarkers = {
    QStringLiteral(".git"),
    QStringLiteral("CMakeLists.txt"),
    QStringLiteral("Cargo.toml"),
    QStringLiteral("go.mod"),
    QStringLiteral("package.json"),
    QStringLiteral("pyproject.toml"),
    QStringLiteral("setup.cfg"),
};

QString canonicalOrAbsolute(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical;
}

QString cleanAuditText(QString value)
{
    value.replace(QChar('\r'), QChar(' '));
    value.replace(QChar('\n'), QChar(' '));
    return value.left(160);
}
}

namespace CapabilityTrust
{
QString capabilityName(Capability capability)
{
    switch (capability) {
    case Capability::JavaScriptExecution: return QStringLiteral("javascript.execution");
    case Capability::ScriptFileAccess: return QStringLiteral("javascript.file-access");
    case Capability::ScriptDocumentEdit: return QStringLiteral("javascript.document-edit");
    case Capability::ScriptDocumentSave: return QStringLiteral("javascript.document-save");
    case Capability::ScriptDocumentOpen: return QStringLiteral("javascript.document-open");
    case Capability::Terminal: return QStringLiteral("terminal.shell");
    case Capability::LuaConsole: return QStringLiteral("lua.console");
    case Capability::TrustedStartup: return QStringLiteral("lua.trusted-startup");
    }
    return QStringLiteral("unknown");
}

Manager::Manager(QSettings *settings, QObject *parent)
    : QObject(parent), settings(settings)
{
}

QString Manager::workspaceRootForPath(const QString &path)
{
    QFileInfo info(path.trimmed());
    QString directoryPath;
    if (info.exists() && info.isFile()) {
        directoryPath = info.absolutePath();
    }
    else if (!path.trimmed().isEmpty() && QDir(path).exists()) {
        directoryPath = QDir(path).absolutePath();
    }
    else {
        directoryPath = info.absolutePath();
    }

    QDir directory(directoryPath.isEmpty() ? QDir::currentPath() : directoryPath);
    while (directory.exists()) {
        for (const QString &marker : WorkspaceMarkers) {
            if (QFileInfo::exists(directory.filePath(marker))) {
                return canonicalOrAbsolute(directory.absolutePath());
            }
        }

        const QString before = directory.absolutePath();
        if (!directory.cdUp() || directory.absolutePath() == before) {
            break;
        }
    }
    return canonicalOrAbsolute(directoryPath.isEmpty() ? QDir::currentPath() : directoryPath);
}

QString Manager::workspaceId(const QString &workspaceRoot)
{
    const QByteArray normalized = canonicalOrAbsolute(workspaceRoot).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(normalized, QCryptographicHash::Sha256).toHex());
}

QString Manager::normalizedWorkspace(const QString &workspaceRoot) const
{
    return workspaceRootForPath(workspaceRoot);
}

QString Manager::grantKey(const QString &workspaceRoot, Capability capability) const
{
    return QString::fromLatin1(GrantsPrefix) + workspaceId(normalizedWorkspace(workspaceRoot))
        + QStringLiteral("/") + capabilityName(capability);
}

bool Manager::isGranted(const QString &workspaceRoot, Capability capability) const
{
    const QString normalized = normalizedWorkspace(workspaceRoot);
    const QString id = workspaceId(normalized);
    const auto sessionIt = sessionGrants.constFind(id);
    if (sessionIt != sessionGrants.constEnd() && sessionIt->contains(static_cast<int>(capability))) {
        return true;
    }
    return settings && settings->value(grantKey(normalized, capability), false).toBool();
}

void Manager::grant(const QString &workspaceRoot, Capability capability, bool persist)
{
    const QString normalized = normalizedWorkspace(workspaceRoot);
    const QString id = workspaceId(normalized);
    if (persist) {
        if (settings) {
            settings->setValue(grantKey(normalized, capability), true);
            settings->sync();
        }
    }
    else {
        sessionGrants[id].insert(static_cast<int>(capability));
    }
    emit capabilityChanged(normalized, capability);
}

void Manager::revoke(const QString &workspaceRoot, Capability capability)
{
    const QString normalized = normalizedWorkspace(workspaceRoot);
    const QString id = workspaceId(normalized);
    sessionGrants[id].remove(static_cast<int>(capability));
    if (settings) {
        settings->remove(grantKey(normalized, capability));
        settings->sync();
    }
    emit capabilityChanged(normalized, capability);
}

void Manager::revokeAll(const QString &workspaceRoot)
{
    const QString normalized = normalizedWorkspace(workspaceRoot);
    const QString id = workspaceId(normalized);
    sessionGrants.remove(id);
    if (settings) {
        const QString prefix = QString::fromLatin1(GrantsPrefix) + id + QStringLiteral("/");
        settings->beginGroup(prefix);
        settings->remove(QString());
        settings->endGroup();
        settings->sync();
    }
    emit capabilityChanged(normalized, Capability::JavaScriptExecution);
}

void Manager::clearSession()
{
    sessionGrants.clear();
}

bool Manager::authorize(QWidget *parent, const QString &workspaceRoot, Capability capability,
                        const QString &operation)
{
    const QString normalized = normalizedWorkspace(workspaceRoot);
    if (isGranted(normalized, capability)) {
        record(normalized, capability, operation, QStringLiteral("allowed"));
        return true;
    }

    if (!parent) {
        record(normalized, capability, operation, QStringLiteral("denied-no-parent"));
        return false;
    }

    QMessageBox prompt(parent);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(tr("Workspace Capability Request"));
    prompt.setText(tr("Allow %1 in this workspace?").arg(capabilityName(capability)));
    prompt.setInformativeText(tr("Workspace: %1\nOperation: %2\n\nThe grant can be revoked from Settings > Workspace Trust.")
                                  .arg(normalized, operation));
    QPushButton *allowOnce = prompt.addButton(tr("Allow Once"), QMessageBox::AcceptRole);
    QPushButton *allowAlways = prompt.addButton(tr("Always Allow"), QMessageBox::YesRole);
    prompt.addButton(tr("Deny"), QMessageBox::RejectRole);
    prompt.exec();

    if (prompt.clickedButton() == allowOnce) {
        grant(normalized, capability, false);
        record(normalized, capability, operation, QStringLiteral("allowed-session"));
        return true;
    }
    if (prompt.clickedButton() == allowAlways) {
        grant(normalized, capability, true);
        record(normalized, capability, operation, QStringLiteral("allowed-persistent"));
        return true;
    }

    record(normalized, capability, operation, QStringLiteral("denied"));
    return false;
}

void Manager::record(const QString &workspaceRoot, Capability capability,
                     const QString &operation, const QString &result)
{
    const QString id = workspaceId(normalizedWorkspace(workspaceRoot));
    const QString safeOperation = cleanAuditText(operation);
    const QString safeResult = cleanAuditText(result);
    if (settings) {
        QVariantList entries = settings->value(QLatin1String(AuditKey)).toList();
        entries.append(QVariantMap{
            {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc()},
            {QStringLiteral("workspaceId"), id},
            {QStringLiteral("capability"), capabilityName(capability)},
            {QStringLiteral("operation"), safeOperation},
            {QStringLiteral("result"), safeResult},
        });
        while (entries.size() > MaximumAuditEntries) {
            entries.removeFirst();
        }
        settings->setValue(QLatin1String(AuditKey), entries);
        settings->sync();
    }
    qInfo("Capability audit workspace=%s capability=%s operation=%s result=%s",
          qUtf8Printable(id), qUtf8Printable(capabilityName(capability)),
          qUtf8Printable(safeOperation), qUtf8Printable(safeResult));
    emit auditRecorded(id, safeOperation, safeResult);
}

QVector<AuditEntry> Manager::auditEntries() const
{
    QVector<AuditEntry> result;
    if (!settings) {
        return result;
    }

    const QVariantList entries = settings->value(QLatin1String(AuditKey)).toList();
    result.reserve(entries.size());
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        AuditEntry value;
        value.timestamp = map.value(QStringLiteral("timestamp")).toDateTime();
        value.workspaceId = map.value(QStringLiteral("workspaceId")).toString();
        value.capability = map.value(QStringLiteral("capability")).toString();
        value.operation = map.value(QStringLiteral("operation")).toString();
        value.result = map.value(QStringLiteral("result")).toString();
        if (!value.workspaceId.isEmpty() && !value.operation.isEmpty()) {
            result.append(value);
        }
    }
    return result;
}
}
