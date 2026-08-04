/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SftpManager.h"

#include "EditorManager.h"
#include "ScintillaNext.h"
#include "SftpConnectionDialog.h"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QUuid>

SftpManager::SftpManager(EditorManager *editorManager, QObject *parent)
    : QObject(parent),
      editorManager(editorManager)
{
    connect(editorManager, &EditorManager::editorClosed, this, &SftpManager::editorClosed);
}

void SftpManager::openRemote(QWidget *parent)
{
    SftpConnectionDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SftpClient::Connection connection = dialog.connection();
    const SftpClient::Result result = SftpClient::download(
        connection,
        [parent, connection](const QString &fingerprint) {
            const QString message = QObject::tr(
                "The host key for %1 is not in the known-hosts file.\n\n"
                "Fingerprint:\n%2\n\n"
                "Only continue if this fingerprint has been verified through a trusted channel.")
                .arg(SftpClient::displayName(connection), fingerprint);
            return QMessageBox::question(parent,
                                          QObject::tr("Verify SFTP Host Key"),
                                          message,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) == QMessageBox::Yes;
        });

    if (!result.success()) {
        QMessageBox::warning(parent, tr("SFTP Open Failed"), result.error);
        return;
    }
    if (!temporaryDirectory.isValid()) {
        QMessageBox::warning(parent,
                             tr("SFTP Open Failed"),
                             tr("Unable to create a temporary mirror for the remote document."));
        return;
    }

    const QString temporaryPath = temporaryFilePath(connection.remotePath);
    QFile temporaryFile(temporaryPath);
    const bool mirrorCreated = temporaryFile.open(QIODevice::WriteOnly)
        && temporaryFile.write(result.data) == result.data.size();
    temporaryFile.close();
    if (!mirrorCreated) {
        QFile::remove(temporaryPath);
        QMessageBox::warning(parent,
                             tr("SFTP Open Failed"),
                             tr("Unable to create a temporary mirror for the remote document."));
        return;
    }

    ScintillaNext *editor = editorManager->createEditorFromFile(temporaryPath);
    if (!editor) {
        QFile::remove(temporaryPath);
        QMessageBox::warning(parent,
                             tr("SFTP Open Failed"),
                             tr("The remote document could not be opened in an editor."));
        return;
    }

    connection.expectedHostFingerprint = result.fingerprint;
    const QString remoteDisplayName = SftpClient::displayName(connection);
    remoteDocuments.insert(editor, RemoteDocument{connection, remoteDisplayName, temporaryPath});
    editor->setName(remoteDisplayName);
    editor->omitModifications();
}

bool SftpManager::isRemote(const ScintillaNext *editor) const
{
    return editor && remoteDocuments.contains(const_cast<ScintillaNext *>(editor));
}

bool SftpManager::saveRemote(ScintillaNext *editor, QString *error)
{
    auto it = remoteDocuments.constFind(editor);
    if (it == remoteDocuments.constEnd()) {
        if (error) {
            *error = tr("The editor is not associated with an SFTP document.");
        }
        return false;
    }

    const QByteArray data = editor->getText(editor->textLength());
    const SftpClient::Result result = SftpClient::upload(it->connection, data);
    if (!result.success()) {
        if (error) {
            *error = result.error;
        }
        return false;
    }

    QFile mirror(it->temporaryPath);
    if (mirror.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        mirror.write(data);
        mirror.close();
    }
    editor->omitModifications();
    if (error) {
        error->clear();
    }
    return true;
}

void SftpManager::forgetRemote(ScintillaNext *editor)
{
    auto it = remoteDocuments.find(editor);
    if (it == remoteDocuments.end()) {
        return;
    }

    const QString temporaryPath = it->temporaryPath;
    remoteDocuments.erase(it);
    QFile::remove(temporaryPath);
}

QString SftpManager::displayName(const ScintillaNext *editor) const
{
    const auto it = remoteDocuments.constFind(const_cast<ScintillaNext *>(editor));
    return it == remoteDocuments.constEnd() ? QString() : it->displayName;
}

QString SftpManager::remotePath(const ScintillaNext *editor) const
{
    const auto it = remoteDocuments.constFind(const_cast<ScintillaNext *>(editor));
    return it == remoteDocuments.constEnd() ? QString() : it->connection.remotePath;
}

void SftpManager::editorClosed(ScintillaNext *editor)
{
    forgetRemote(editor);
}

QString SftpManager::temporaryFilePath(const QString &remotePath) const
{
    QString fileName = QFileInfo(SftpClient::normalizedRemotePath(remotePath)).fileName();
    if (fileName.isEmpty() || fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
        fileName = QStringLiteral("remote.txt");
    }
    fileName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));

    return temporaryDirectory.filePath(
        QStringLiteral("%1_%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), fileName));
}
