/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SFTPCONNECTIONDIALOG_H
#define SFTPCONNECTIONDIALOG_H

#include "SftpClient.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;

class SftpConnectionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SftpConnectionDialog(QWidget *parent = nullptr);

    SftpClient::Connection connection() const { return connectionData; }

private slots:
    void browsePrivateKey();
    void browsePublicKey();
    void browseKnownHosts();

    void accept() override;

private:
    QLineEdit *hostEdit;
    QSpinBox *portSpinBox;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *privateKeyEdit;
    QLineEdit *publicKeyEdit;
    QLineEdit *passphraseEdit;
    QLineEdit *knownHostsEdit;
    QLineEdit *remotePathEdit;

    SftpClient::Connection connectionData;
};

#endif // SFTPCONNECTIONDIALOG_H
