/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "SftpClient.h"

#include <QtTest>

class SftpClientTests final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesRemotePaths();
    void formatsDisplayNames();
    void rejectsIncompleteConnections();
    void acceptsPasswordConnection();
    void comparesRemoteVersions();
};

void SftpClientTests::normalizesRemotePaths()
{
    QCOMPARE(SftpClient::normalizedRemotePath(QStringLiteral("etc/../var/log/app.txt")),
             QStringLiteral("/var/log/app.txt"));
    QCOMPARE(SftpClient::normalizedRemotePath(QStringLiteral("\\srv\\file.txt")),
             QStringLiteral("/srv/file.txt"));
    QCOMPARE(SftpClient::normalizedRemotePath(QString()), QStringLiteral("/"));
}

void SftpClientTests::formatsDisplayNames()
{
    SftpClient::Connection connection;
    connection.host = QStringLiteral("server.example");
    connection.port = 2222;
    connection.username = QStringLiteral("editor");
    connection.remotePath = QStringLiteral("docs/readme.md");

    QCOMPARE(SftpClient::displayName(connection),
             QStringLiteral("sftp://editor@server.example:2222/docs/readme.md"));
}

void SftpClientTests::rejectsIncompleteConnections()
{
    SftpClient::Connection connection;
    QString error;

    QVERIFY(!SftpClient::validateConnection(connection, &error));
    QVERIFY(error.contains(QStringLiteral("host"), Qt::CaseInsensitive));
}

void SftpClientTests::acceptsPasswordConnection()
{
    SftpClient::Connection connection;
    connection.host = QStringLiteral("server.example");
    connection.username = QStringLiteral("editor");
    connection.password = QStringLiteral("transient-password");
    connection.remotePath = QStringLiteral("/docs/readme.md");

    QString error;
    QVERIFY(SftpClient::validateConnection(connection, &error));
    QVERIFY(error.isEmpty());
}

void SftpClientTests::comparesRemoteVersions()
{
    SftpClient::RemoteFileIdentity expected;
    expected.known = true;
    expected.exists = true;
    expected.hasSize = true;
    expected.hasModifiedTime = true;
    expected.size = 42;
    expected.modifiedTime = 100;

    QVERIFY(SftpClient::remoteIdentityMatches(expected, expected));

    auto changed = expected;
    changed.modifiedTime++;
    QVERIFY(!SftpClient::remoteIdentityMatches(expected, changed));

    auto incomplete = expected;
    incomplete.hasModifiedTime = false;
    QVERIFY(!SftpClient::remoteIdentityMatches(expected, incomplete));

    SftpClient::RemoteFileIdentity missing = expected;
    missing.exists = false;
    QVERIFY(!SftpClient::remoteIdentityMatches(expected, missing));
}

QTEST_MAIN(SftpClientTests)
#include "SftpClientTests.moc"
