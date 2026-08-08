/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "NppPluginTrust.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class PluginTrustTests final : public QObject
{
    Q_OBJECT

private slots:
    void discoversCanonicalDllsOnce();
    void hashesIdentityAndPersistsTrust();
    void changedPluginHashRequiresNewTrustDecision();
    void rejectsMissingPluginFiles();
};

void PluginTrustTests::discoversCanonicalDllsOnce()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("app/nested"))));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("user"))));

    const QString appPlugin = root.filePath(QStringLiteral("app/nested/Example.dll"));
    const QString userPlugin = root.filePath(QStringLiteral("user/Profile.dll"));
    QVERIFY(QFile(appPlugin).open(QIODevice::WriteOnly));
    QVERIFY(QFile(userPlugin).open(QIODevice::WriteOnly));

    const QVector<NppPluginTrust::Candidate> candidates = NppPluginTrust::discover(
        root.filePath(QStringLiteral("app")),
        root.filePath(QStringLiteral("user")));

    QCOMPARE(candidates.size(), 2);
    QCOMPARE(candidates.at(0).displayName, QStringLiteral("Example"));
    QCOMPARE(candidates.at(1).displayName, QStringLiteral("Profile"));
    QCOMPARE(candidates.at(0).location, NppPluginTrust::Location::Application);
    QCOMPARE(candidates.at(1).location, NppPluginTrust::Location::User);
}

void PluginTrustTests::hashesIdentityAndPersistsTrust()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("Example.dll"));
    QFile pluginFile(path);
    QVERIFY(pluginFile.open(QIODevice::WriteOnly));
    QVERIFY(pluginFile.write("plugin-binary-fixture") > 0);
    pluginFile.close();

    const NppPluginTrust::Candidate candidate{QFileInfo(path).canonicalFilePath(), QStringLiteral("Example"), NppPluginTrust::Location::Application};
    NppPluginTrust::Identity identity;
    QString error;
    QVERIFY(NppPluginTrust::identify(candidate, &identity, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(identity.pluginId, QStringLiteral("Example"));
    QCOMPARE(identity.abi, QString::fromLatin1(NppPluginTrust::CompatibilityAbi));
    QCOMPARE(identity.sha256.size(), 32);

    QSettings settings(root.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    NppPluginTrust::TrustStore store(&settings);
    QVERIFY(!store.isTrusted(identity));
    store.trust(identity);
    QVERIFY(store.isTrusted(identity));
    QCOMPARE(store.entries().size(), 1);
    store.revoke(identity);
    QVERIFY(!store.isTrusted(identity));
}

void PluginTrustTests::changedPluginHashRequiresNewTrustDecision()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("Example.dll"));
    QFile pluginFile(path);
    QVERIFY(pluginFile.open(QIODevice::WriteOnly));
    QVERIFY(pluginFile.write("first-version") > 0);
    pluginFile.close();

    const NppPluginTrust::Candidate candidate{QFileInfo(path).canonicalFilePath(), QStringLiteral("Example"), NppPluginTrust::Location::Application};
    NppPluginTrust::Identity firstIdentity;
    QVERIFY(NppPluginTrust::identify(candidate, &firstIdentity));

    QSettings settings(root.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    NppPluginTrust::TrustStore store(&settings);
    store.trust(firstIdentity);
    QVERIFY(store.userPluginsEnabled() == false);
    store.setUserPluginsEnabled(true);
    QVERIFY(store.userPluginsEnabled());

    QVERIFY(pluginFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(pluginFile.write("replacement-version") > 0);
    pluginFile.close();

    NppPluginTrust::Identity replacementIdentity;
    QVERIFY(NppPluginTrust::identify(candidate, &replacementIdentity));
    QVERIFY(firstIdentity.sha256 != replacementIdentity.sha256);
    QVERIFY(!store.isTrusted(replacementIdentity));
    QVERIFY(store.isTrusted(firstIdentity));
}

void PluginTrustTests::rejectsMissingPluginFiles()
{
    const NppPluginTrust::Candidate candidate{
        QStringLiteral("C:/does-not-exist/NotepadNextPlugin.dll"),
        QStringLiteral("Missing"),
        NppPluginTrust::Location::Application,
    };
    NppPluginTrust::Identity identity;
    QString error;
    QVERIFY(!NppPluginTrust::identify(candidate, &identity, &error));
    QVERIFY(error.contains(QStringLiteral("does not exist"), Qt::CaseInsensitive));
}

QTEST_MAIN(PluginTrustTests)
#include "PluginTrustTests.moc"
