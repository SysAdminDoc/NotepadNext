/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "CapabilityTrust.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using CapabilityTrust::Capability;

class CapabilityTrustTests final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsDenyAndAuditWithoutSecrets();
    void sessionAndPersistentGrantsHaveDifferentLifetimes();
    void revokeRemovesAllGrantForms();
    void headlessAuthorizationFailsClosed();
};

void CapabilityTrustTests::defaultsDenyAndAuditWithoutSecrets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspace = directory.filePath(QStringLiteral("workspace"));
    QVERIFY(QDir().mkpath(QDir(workspace).filePath(QStringLiteral(".git"))));

    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    CapabilityTrust::Manager manager(&settings);
    QVERIFY(!manager.isGranted(workspace, Capability::Terminal));

    manager.record(workspace, Capability::Terminal, QStringLiteral("terminal.start"), QStringLiteral("timeout"));
    manager.record(workspace, Capability::LuaConsole, QStringLiteral("lua.execute"), QStringLiteral("crash"));
    const QVector<CapabilityTrust::AuditEntry> entries = manager.auditEntries();
    QCOMPARE(entries.size(), 2);
    QVERIFY(!entries.at(0).workspaceId.contains(workspace));
    QCOMPARE(entries.at(0).result, QStringLiteral("timeout"));
    QCOMPARE(entries.at(1).result, QStringLiteral("crash"));
    QVERIFY(!entries.at(0).operation.contains(workspace));
}

void CapabilityTrustTests::sessionAndPersistentGrantsHaveDifferentLifetimes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspace = directory.filePath(QStringLiteral("workspace"));
    QVERIFY(QDir().mkpath(workspace));
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

    CapabilityTrust::Manager first(&settings);
    first.grant(workspace, Capability::ScriptDocumentEdit, false);
    QVERIFY(first.isGranted(workspace, Capability::ScriptDocumentEdit));

    CapabilityTrust::Manager second(&settings);
    QVERIFY(!second.isGranted(workspace, Capability::ScriptDocumentEdit));

    first.grant(workspace, Capability::ScriptDocumentSave, true);
    CapabilityTrust::Manager reloaded(&settings);
    QVERIFY(reloaded.isGranted(workspace, Capability::ScriptDocumentSave));
}

void CapabilityTrustTests::revokeRemovesAllGrantForms()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspace = directory.filePath(QStringLiteral("workspace"));
    QVERIFY(QDir().mkpath(workspace));
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

    CapabilityTrust::Manager manager(&settings);
    manager.grant(workspace, Capability::JavaScriptExecution, true);
    manager.grant(workspace, Capability::ScriptFileAccess, false);
    manager.revoke(workspace, Capability::JavaScriptExecution);
    manager.revoke(workspace, Capability::ScriptFileAccess);
    QVERIFY(!manager.isGranted(workspace, Capability::JavaScriptExecution));
    QVERIFY(!manager.isGranted(workspace, Capability::ScriptFileAccess));

    manager.grant(workspace, Capability::Terminal, true);
    manager.grant(workspace, Capability::LuaConsole, false);
    manager.revokeAll(workspace);
    QVERIFY(!manager.isGranted(workspace, Capability::Terminal));
    QVERIFY(!manager.isGranted(workspace, Capability::LuaConsole));
}

void CapabilityTrustTests::headlessAuthorizationFailsClosed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspace = directory.filePath(QStringLiteral("workspace"));
    QVERIFY(QDir().mkpath(workspace));
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    CapabilityTrust::Manager manager(&settings);

    QVERIFY(!manager.authorize(nullptr, workspace, Capability::TrustedStartup,
                               QStringLiteral("startup.execute")));
    QVERIFY(!manager.auditEntries().isEmpty());
    QCOMPARE(manager.auditEntries().constLast().result, QStringLiteral("denied-no-parent"));
}

QTEST_MAIN(CapabilityTrustTests)
#include "CapabilityTrustTests.moc"
