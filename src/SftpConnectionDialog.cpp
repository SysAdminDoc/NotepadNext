/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SftpConnectionDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDir>

namespace
{
QWidget *pathEditor(QWidget *parent, QLineEdit **edit, const QString &buttonText, const QObject *receiver, const char *member)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    *edit = new QLineEdit(container);
    layout->addWidget(*edit, 1);

    auto *button = new QPushButton(buttonText, container);
    button->setAutoDefault(false);
    layout->addWidget(button);
    QObject::connect(button, SIGNAL(clicked()), receiver, member);
    return container;
}

QString defaultPrivateKeyPath()
{
    const QDir sshDirectory(QDir::home().filePath(QStringLiteral(".ssh")));
    const QStringList candidates = {
        QStringLiteral("id_ed25519"),
        QStringLiteral("id_ecdsa"),
        QStringLiteral("id_rsa")
    };
    for (const QString &candidate : candidates) {
        const QString path = sshDirectory.filePath(candidate);
        if (QFileInfo(path).isFile()) {
            return path;
        }
    }
    return {};
}
}

SftpConnectionDialog::SftpConnectionDialog(QWidget *parent)
    : QDialog(parent),
      hostEdit(new QLineEdit(this)),
      portSpinBox(new QSpinBox(this)),
      usernameEdit(new QLineEdit(this)),
      passwordEdit(new QLineEdit(this)),
      privateKeyEdit(nullptr),
      publicKeyEdit(nullptr),
      passphraseEdit(new QLineEdit(this)),
      knownHostsEdit(nullptr),
      remotePathEdit(new QLineEdit(this))
{
    setWindowTitle(tr("Open Remote SFTP File"));
    setModal(true);
    setMinimumWidth(600);

    auto *layout = new QVBoxLayout(this);
    auto *connectionGroup = new QGroupBox(tr("Connection"), this);
    auto *form = new QFormLayout(connectionGroup);

    hostEdit->setPlaceholderText(tr("server.example.com"));
    form->addRow(tr("Host:"), hostEdit);

    portSpinBox->setRange(1, 65535);
    portSpinBox->setValue(22);
    form->addRow(tr("Port:"), portSpinBox);

    const QString environmentUser = qEnvironmentVariable("USERNAME").isEmpty()
        ? qEnvironmentVariable("USER")
        : qEnvironmentVariable("USERNAME");
    usernameEdit->setText(environmentUser);
    form->addRow(tr("User:"), usernameEdit);

    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(tr("Optional when using a private key"));
    form->addRow(tr("Password:"), passwordEdit);

    layout->addWidget(connectionGroup);

    auto *keyGroup = new QGroupBox(tr("Key authentication (optional)"), this);
    auto *keyForm = new QFormLayout(keyGroup);
    keyForm->addRow(tr("Private key:"), pathEditor(this, &privateKeyEdit, tr("Browse..."), this, SLOT(browsePrivateKey())));
    privateKeyEdit->setText(defaultPrivateKeyPath());
    keyForm->addRow(tr("Public key:"), pathEditor(this, &publicKeyEdit, tr("Browse..."), this, SLOT(browsePublicKey())));
    passphraseEdit->setEchoMode(QLineEdit::Password);
    keyForm->addRow(tr("Passphrase:"), passphraseEdit);
    layout->addWidget(keyGroup);

    auto *remoteGroup = new QGroupBox(tr("Remote file"), this);
    auto *remoteForm = new QFormLayout(remoteGroup);
    remotePathEdit->setPlaceholderText(tr("/home/user/project/file.txt"));
    remoteForm->addRow(tr("Path:"), remotePathEdit);
    remoteForm->addRow(tr("Known hosts:"), pathEditor(this, &knownHostsEdit, tr("Browse..."), this, SLOT(browseKnownHosts())));
    knownHostsEdit->setText(SftpClient::defaultKnownHostsPath());
    layout->addWidget(remoteGroup);

    auto *help = new QLabel(
        tr("Credentials remain in memory only. Known host keys are checked against the selected OpenSSH known-hosts file; an unknown key requires explicit fingerprint confirmation."),
        this);
    help->setWordWrap(true);
    layout->addWidget(help);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SftpConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SftpConnectionDialog::reject);
    layout->addWidget(buttons);

    hostEdit->setFocus();
}

void SftpConnectionDialog::browsePrivateKey()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select private key"), QDir::homePath());
    if (!path.isEmpty()) {
        privateKeyEdit->setText(path);
    }
}

void SftpConnectionDialog::browsePublicKey()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select public key"), QDir::homePath());
    if (!path.isEmpty()) {
        publicKeyEdit->setText(path);
    }
}

void SftpConnectionDialog::browseKnownHosts()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select known-hosts file"), QDir::homePath());
    if (!path.isEmpty()) {
        knownHostsEdit->setText(path);
    }
}

void SftpConnectionDialog::accept()
{
    SftpClient::Connection candidate;
    candidate.host = hostEdit->text().trimmed();
    candidate.port = static_cast<quint16>(portSpinBox->value());
    candidate.username = usernameEdit->text().trimmed();
    candidate.password = passwordEdit->text();
    candidate.privateKeyPath = privateKeyEdit->text().trimmed();
    candidate.publicKeyPath = publicKeyEdit->text().trimmed();
    candidate.privateKeyPassphrase = passphraseEdit->text();
    candidate.knownHostsPath = knownHostsEdit->text().trimmed();
    candidate.remotePath = SftpClient::normalizedRemotePath(remotePathEdit->text());

    QString error;
    if (!SftpClient::validateConnection(candidate, &error)) {
        QMessageBox::warning(this, tr("Invalid SFTP Connection"), error);
        return;
    }

    connectionData = candidate;
    QDialog::accept();
}
