/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SftpClient.h"

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <QDir>
#include <QFileInfo>
#include <QTcpSocket>

#include <mutex>

namespace
{
constexpr int SocketTimeoutMs = 15000;
constexpr qsizetype TransferBufferSize = 16 * 1024;
constexpr qsizetype MaximumDocumentSize = 256 * 1024 * 1024;

int initializeLibssh2()
{
    static std::once_flag initFlag;
    static int initResult = -1;
    std::call_once(initFlag, []() {
        initResult = libssh2_init(0);
    });
    return initResult;
}

QString sessionError(LIBSSH2_SESSION *session, int code)
{
    char *message = nullptr;
    int messageLength = 0;
    libssh2_session_last_error(session, &message, &messageLength, 0);

    const QString detail = message && messageLength > 0
        ? QString::fromUtf8(message, messageLength).trimmed()
        : QStringLiteral("libssh2 error %1").arg(code);
    return detail;
}

QString sftpError(LIBSSH2_SFTP *sftp)
{
    return QStringLiteral("SFTP error %1").arg(libssh2_sftp_last_error(sftp));
}

int knownHostKeyType(int hostKeyType)
{
    switch (hostKeyType) {
    case LIBSSH2_HOSTKEY_TYPE_RSA:
        return LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    case LIBSSH2_HOSTKEY_TYPE_DSS:
        return LIBSSH2_KNOWNHOST_KEY_SSHDSS;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
        return LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
        return LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
        return LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
    case LIBSSH2_HOSTKEY_TYPE_ED25519:
        return LIBSSH2_KNOWNHOST_KEY_ED25519;
    default:
        return LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
    }
}

struct SessionContext
{
    QTcpSocket socket;
    LIBSSH2_SESSION *session = nullptr;
    LIBSSH2_SFTP *sftp = nullptr;

    ~SessionContext()
    {
        if (sftp) {
            libssh2_sftp_shutdown(sftp);
        }
        if (session) {
            libssh2_session_disconnect(session, "Notepad Next closed the SFTP session");
            libssh2_session_free(session);
        }
    }
};

bool authenticate(SessionContext &context,
                  const SftpClient::Connection &connection,
                  QString *error)
{
    const QByteArray username = connection.username.toUtf8();
    char *methodList = libssh2_userauth_list(context.session,
                                             username.constData(),
                                             static_cast<unsigned int>(username.size()));
    if (!methodList) {
        *error = QStringLiteral("Unable to query SFTP authentication methods: %1")
                     .arg(sessionError(context.session, libssh2_session_last_errno(context.session)));
        return false;
    }

    const QByteArray methods(methodList);
    const auto supports = [&methods](const char *method) {
        return methods.split(',').contains(QByteArray(method));
    };

    int lastError = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
    if (!connection.password.isEmpty() && supports("password")) {
        const QByteArray password = connection.password.toUtf8();
        lastError = libssh2_userauth_password_ex(context.session,
                                                  username.constData(),
                                                  static_cast<unsigned int>(username.size()),
                                                  password.constData(),
                                                  static_cast<unsigned int>(password.size()),
                                                  nullptr);
        if (lastError == 0) {
            return true;
        }
    }

    if (!connection.privateKeyPath.isEmpty() && supports("publickey")) {
        const QByteArray publicKey = connection.publicKeyPath.toUtf8();
        const QByteArray privateKey = connection.privateKeyPath.toUtf8();
        const QByteArray passphrase = connection.privateKeyPassphrase.toUtf8();

        lastError = libssh2_userauth_publickey_fromfile_ex(
            context.session,
            username.constData(),
            static_cast<unsigned int>(username.size()),
            publicKey.isEmpty() ? nullptr : publicKey.constData(),
            privateKey.constData(),
            passphrase.isEmpty() ? nullptr : passphrase.constData());
        if (lastError == 0) {
            return true;
        }
    }

    *error = QStringLiteral("SFTP authentication failed: %1")
                 .arg(sessionError(context.session, lastError));
    return false;
}

bool verifyHostKey(SessionContext &context,
                   const SftpClient::Connection &connection,
                   const SftpClient::HostKeyConfirmationCallback &confirmUnknownHost,
                   QString *fingerprint,
                   QString *error)
{
    size_t hostKeyLength = 0;
    int hostKeyType = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    const char *hostKey = libssh2_session_hostkey(context.session, &hostKeyLength, &hostKeyType);
    const char *hash = libssh2_hostkey_hash(context.session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!hostKey || hostKeyLength == 0 || !hash) {
        *error = QStringLiteral("The SSH server did not provide a verifiable host key.");
        return false;
    }

    *fingerprint = QStringLiteral("SHA256:%1")
                       .arg(QByteArray(hash, 32).toBase64(QByteArray::Base64Encoding));

    if (!connection.expectedHostFingerprint.isEmpty()) {
        if (QString::compare(connection.expectedHostFingerprint, *fingerprint, Qt::CaseSensitive) != 0) {
            *error = QStringLiteral("The SFTP host key changed. Expected %1, received %2.")
                         .arg(connection.expectedHostFingerprint, *fingerprint);
            return false;
        }
        return true;
    }

    if (!connection.knownHostsPath.isEmpty() && QFileInfo(connection.knownHostsPath).isFile()) {
        LIBSSH2_KNOWNHOSTS *knownHosts = libssh2_knownhost_init(context.session);
        if (!knownHosts) {
            *error = QStringLiteral("Unable to initialize the SFTP known-hosts database.");
            return false;
        }

        const int readResult = libssh2_knownhost_readfile(
            knownHosts, connection.knownHostsPath.toUtf8().constData(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
        if (readResult < 0) {
            libssh2_knownhost_free(knownHosts);
            *error = QStringLiteral("Unable to read the SFTP known-hosts file.");
            return false;
        }

        struct libssh2_knownhost *knownHost = nullptr;
        const int checkResult = libssh2_knownhost_checkp(
            knownHosts,
            connection.host.toUtf8().constData(),
            connection.port,
            hostKey,
            hostKeyLength,
            LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | knownHostKeyType(hostKeyType),
            &knownHost);
        libssh2_knownhost_free(knownHosts);

        if (checkResult == LIBSSH2_KNOWNHOST_CHECK_MATCH) {
            return true;
        }
        if (checkResult == LIBSSH2_KNOWNHOST_CHECK_MISMATCH) {
            *error = QStringLiteral("The SFTP host key does not match the known-hosts file (%1).")
                         .arg(*fingerprint);
            return false;
        }
        if (checkResult == LIBSSH2_KNOWNHOST_CHECK_FAILURE) {
            *error = QStringLiteral("The SFTP host key could not be checked against the known-hosts file.");
            return false;
        }
        if (checkResult != LIBSSH2_KNOWNHOST_CHECK_NOTFOUND) {
            *error = QStringLiteral("The SFTP host key could not be checked against the known-hosts file.");
            return false;
        }
    }

    if (!confirmUnknownHost || !confirmUnknownHost(*fingerprint)) {
        *error = QStringLiteral("The SFTP host key is unknown. Verify %1 before connecting.")
                     .arg(*fingerprint);
        return false;
    }

    return true;
}

bool openSession(const SftpClient::Connection &connection,
                 const SftpClient::HostKeyConfirmationCallback &confirmUnknownHost,
                 SessionContext &context,
                 QString *fingerprint,
                 QString *error)
{
    if (initializeLibssh2() != 0) {
        *error = QStringLiteral("Unable to initialize the libssh2 SFTP library.");
        return false;
    }

    context.socket.connectToHost(connection.host, connection.port);
    if (!context.socket.waitForConnected(SocketTimeoutMs)) {
        *error = QStringLiteral("Unable to connect to %1:%2: %3")
                     .arg(connection.host)
                     .arg(connection.port)
                     .arg(context.socket.errorString());
        return false;
    }

    context.session = libssh2_session_init();
    if (!context.session) {
        *error = QStringLiteral("Unable to allocate the libssh2 SFTP session.");
        return false;
    }

    libssh2_session_set_blocking(context.session, 1);
    libssh2_session_set_timeout(context.session, SocketTimeoutMs);

    const int handshakeResult = libssh2_session_handshake(
        context.session,
        static_cast<libssh2_socket_t>(context.socket.socketDescriptor()));
    if (handshakeResult != 0) {
        *error = QStringLiteral("SFTP handshake failed: %1")
                     .arg(sessionError(context.session, handshakeResult));
        return false;
    }

    if (!verifyHostKey(context, connection, confirmUnknownHost, fingerprint, error)) {
        return false;
    }

    if (!authenticate(context, connection, error)) {
        return false;
    }

    context.sftp = libssh2_sftp_init(context.session);
    if (!context.sftp) {
        *error = QStringLiteral("Unable to start the SFTP subsystem: %1")
                     .arg(sessionError(context.session, libssh2_session_last_errno(context.session)));
        return false;
    }

    return true;
}
}

QString SftpClient::normalizedRemotePath(const QString &path)
{
    QString normalized = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty()) {
        return QStringLiteral("/");
    }

    normalized = QDir::cleanPath(normalized);
    if (!normalized.startsWith(QLatin1Char('/'))) {
        normalized.prepend(QLatin1Char('/'));
    }
    return normalized;
}

QString SftpClient::displayName(const Connection &connection)
{
    QString host = connection.host;
    if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('['))) {
        host = QStringLiteral("[%1]").arg(host);
    }

    return QStringLiteral("sftp://%1@%2:%3%4")
        .arg(connection.username, host)
        .arg(connection.port)
        .arg(normalizedRemotePath(connection.remotePath));
}

QString SftpClient::defaultKnownHostsPath()
{
    return QDir(QDir::homePath()).filePath(QStringLiteral(".ssh/known_hosts"));
}

bool SftpClient::validateConnection(const Connection &connection, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (connection.host.trimmed().isEmpty()) {
        return fail(QStringLiteral("Enter an SSH host name."));
    }
    if (connection.port == 0) {
        return fail(QStringLiteral("Enter a valid SSH port."));
    }
    if (connection.username.trimmed().isEmpty()) {
        return fail(QStringLiteral("Enter an SSH user name."));
    }
    if (normalizedRemotePath(connection.remotePath) == QStringLiteral("/")) {
        return fail(QStringLiteral("Enter a remote file path."));
    }
    if (connection.password.isEmpty() && connection.privateKeyPath.trimmed().isEmpty()) {
        return fail(QStringLiteral("Enter a password or a private-key path."));
    }
    if (!connection.privateKeyPath.trimmed().isEmpty() && !QFileInfo(connection.privateKeyPath).isFile()) {
        return fail(QStringLiteral("The private key does not exist."));
    }
    if (!connection.publicKeyPath.trimmed().isEmpty() && !QFileInfo(connection.publicKeyPath).isFile()) {
        return fail(QStringLiteral("The public key does not exist."));
    }

    if (error) {
        error->clear();
    }
    return true;
}

SftpClient::Result SftpClient::download(const Connection &connection,
                                        const HostKeyConfirmationCallback &confirmUnknownHost)
{
    Result result;
    if (!validateConnection(connection, &result.error)) {
        return result;
    }

    SessionContext context;
    QString fingerprint;
    if (!openSession(connection, confirmUnknownHost, context, &fingerprint, &result.error)) {
        result.fingerprint = fingerprint;
        return result;
    }
    result.fingerprint = fingerprint;

    const QByteArray remotePath = normalizedRemotePath(connection.remotePath).toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(context.sftp, remotePath.constData(), LIBSSH2_FXF_READ, 0);
    if (!handle) {
        result.error = QStringLiteral("Unable to open %1: %2")
                           .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
        return result;
    }

    char buffer[TransferBufferSize];
    while (true) {
        const ssize_t read = libssh2_sftp_read(handle, buffer, sizeof(buffer));
        if (read == 0) {
            break;
        }
        if (read < 0) {
            result.error = QStringLiteral("Unable to read %1: %2")
                               .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
            libssh2_sftp_close(handle);
            return result;
        }
        if (result.data.size() > MaximumDocumentSize - read) {
            result.error = QStringLiteral("The remote document is larger than the 256 MiB safety limit.");
            libssh2_sftp_close(handle);
            result.data.clear();
            return result;
        }
        result.data.append(buffer, read);
    }

    if (libssh2_sftp_close(handle) != 0) {
        result.error = QStringLiteral("Unable to close %1: %2")
                           .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
    }
    return result;
}

SftpClient::Result SftpClient::upload(const Connection &connection,
                                      const QByteArray &data,
                                      const HostKeyConfirmationCallback &confirmUnknownHost)
{
    Result result;
    if (!validateConnection(connection, &result.error)) {
        return result;
    }
    if (data.size() > MaximumDocumentSize) {
        result.error = QStringLiteral("The document is larger than the 256 MiB safety limit.");
        return result;
    }

    SessionContext context;
    QString fingerprint;
    if (!openSession(connection, confirmUnknownHost, context, &fingerprint, &result.error)) {
        result.fingerprint = fingerprint;
        return result;
    }
    result.fingerprint = fingerprint;

    const QByteArray remotePath = normalizedRemotePath(connection.remotePath).toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(
        context.sftp,
        remotePath.constData(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        0644);
    if (!handle) {
        result.error = QStringLiteral("Unable to open %1 for writing: %2")
                           .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
        return result;
    }

    qsizetype offset = 0;
    while (offset < data.size()) {
        const ssize_t written = libssh2_sftp_write(
            handle, data.constData() + offset, static_cast<size_t>(data.size() - offset));
        if (written <= 0) {
            result.error = QStringLiteral("Unable to write %1: %2")
                               .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
            libssh2_sftp_close(handle);
            return result;
        }
        offset += written;
    }

    // Closing the handle commits the buffered write. The optional SFTP fsync
    // extension is not implemented by every server, so do not make it a
    // compatibility requirement for a successful save.
    if (libssh2_sftp_close(handle) != 0) {
        result.error = QStringLiteral("Unable to close %1: %2")
                           .arg(normalizedRemotePath(connection.remotePath), sftpError(context.sftp));
    }
    return result;
}
