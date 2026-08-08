/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SFTPCLIENT_H
#define SFTPCLIENT_H

#include <QByteArray>
#include <QString>

#include <functional>

class SftpClient final
{
public:
    struct RemoteFileIdentity
    {
        bool known = false;
        bool exists = false;
        bool hasSize = false;
        bool hasModifiedTime = false;
        quint64 size = 0;
        quint64 modifiedTime = 0;
    };

    struct Connection
    {
        QString host;
        quint16 port = 22;
        QString username;
        QString password;
        QString publicKeyPath;
        QString privateKeyPath;
        QString privateKeyPassphrase;
        QString knownHostsPath;
        QString remotePath;

        // This is populated only for the lifetime of an open editor. It is
        // never written to application settings or the session journal.
        QString expectedHostFingerprint;

        // This is the remote version observed when the editor was opened or
        // last saved. It is used to reject stale saves.
        bool expectedRemoteKnown = false;
        RemoteFileIdentity expectedRemote;
    };

    struct Result
    {
        QByteArray data;
        QString error;
        QString fingerprint;
        RemoteFileIdentity remoteIdentity;
        bool conflict = false;

        bool success() const { return error.isEmpty(); }
    };

    using HostKeyConfirmationCallback = std::function<bool(const QString &fingerprint)>;

    static Result download(const Connection &connection,
                           const HostKeyConfirmationCallback &confirmUnknownHost = {});
    static Result upload(const Connection &connection,
                         const QByteArray &data,
                         const HostKeyConfirmationCallback &confirmUnknownHost = {});

    static QString normalizedRemotePath(const QString &path);
    static QString displayName(const Connection &connection);
    static QString defaultKnownHostsPath();
    static bool validateConnection(const Connection &connection, QString *error = nullptr);
    static bool remoteIdentityMatches(const RemoteFileIdentity &expected,
                                      const RemoteFileIdentity &actual);
};

#endif // SFTPCLIENT_H
