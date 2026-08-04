/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SFTPMANAGER_H
#define SFTPMANAGER_H

#include "SftpClient.h"

#include <QHash>
#include <QObject>
#include <QTemporaryDir>

class EditorManager;
class ScintillaNext;
class QWidget;

class SftpManager final : public QObject
{
    Q_OBJECT

public:
    explicit SftpManager(EditorManager *editorManager, QObject *parent = nullptr);

    void openRemote(QWidget *parent);

    bool isRemote(const ScintillaNext *editor) const;
    bool saveRemote(ScintillaNext *editor, QString *error = nullptr);
    void forgetRemote(ScintillaNext *editor);

    QString displayName(const ScintillaNext *editor) const;
    QString remotePath(const ScintillaNext *editor) const;

private slots:
    void editorClosed(ScintillaNext *editor);

private:
    struct RemoteDocument
    {
        SftpClient::Connection connection;
        QString displayName;
        QString temporaryPath;
    };

    QString temporaryFilePath(const QString &remotePath) const;

    EditorManager *editorManager;
    QTemporaryDir temporaryDirectory;
    QHash<ScintillaNext *, RemoteDocument> remoteDocuments;
};

#endif // SFTPMANAGER_H
