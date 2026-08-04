/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TerminalProcess.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

TerminalProcess::TerminalProcess(QObject *parent)
    : QObject(parent),
      process(this)
{
    process.setProcessChannelMode(QProcess::MergedChannels);

    connect(&process, &QProcess::started, this, [this]() {
        currentError.clear();
        emit started();
        if (hasPendingCommand) {
            const QString command = std::exchange(pendingCommand, QString());
            hasPendingCommand = false;
            writeCommand(command);
        }
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, &TerminalProcess::readOutput);
    connect(&process, &QProcess::errorOccurred, this, &TerminalProcess::handleError);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &TerminalProcess::handleFinished);
}

TerminalProcess::~TerminalProcess()
{
    stop();
}

TerminalProcess::ShellCommand TerminalProcess::defaultShell()
{
#ifdef Q_OS_WIN
    QString program = qEnvironmentVariable("COMSPEC");
    if (program.isEmpty()) {
        program = QStringLiteral("cmd.exe");
    }
    return {program, {QStringLiteral("/D"), QStringLiteral("/Q"), QStringLiteral("/K")}};
#else
    QString program = qEnvironmentVariable("SHELL");
    if (program.isEmpty() || !QFileInfo::exists(program)) {
        program = QStringLiteral("/bin/sh");
    }
    return {program, {QStringLiteral("-i")}};
#endif
}

bool TerminalProcess::start(const QString &workingDirectory)
{
    if (isRunning()) {
        return true;
    }

    const QString requestedDirectory = workingDirectory.trimmed();
    const QString resolvedDirectory = requestedDirectory.isEmpty()
        ? QDir::homePath()
        : QDir(requestedDirectory).absolutePath();
    if (!QDir(resolvedDirectory).exists()) {
        currentError = QStringLiteral("Working directory does not exist: %1").arg(resolvedDirectory);
        emit errorOccurred(currentError);
        return false;
    }

    currentWorkingDirectory = resolvedDirectory;
    currentError.clear();
    const ShellCommand shell = defaultShell();
    process.setWorkingDirectory(currentWorkingDirectory);
    process.start(shell.program, shell.arguments);
    return true;
}

void TerminalProcess::stop()
{
    hasPendingCommand = false;
    pendingCommand.clear();

    if (!isRunning()) {
        return;
    }

    process.terminate();
    if (!process.waitForFinished(500)) {
        process.kill();
        process.waitForFinished(500);
    }
}

bool TerminalProcess::sendCommand(const QString &command)
{
    if (process.state() == QProcess::Starting) {
        pendingCommand = command;
        hasPendingCommand = true;
        return true;
    }

    if (!isRunning()) {
        pendingCommand = command;
        hasPendingCommand = true;
        if (!start(currentWorkingDirectory)) {
            hasPendingCommand = false;
            pendingCommand.clear();
            return false;
        }
        return true;
    }

    return writeCommand(command);
}

bool TerminalProcess::isRunning() const
{
    return process.state() != QProcess::NotRunning;
}

QString TerminalProcess::workingDirectory() const
{
    return currentWorkingDirectory;
}

QString TerminalProcess::errorString() const
{
    return currentError;
}

void TerminalProcess::readOutput()
{
    const QByteArray output = process.readAllStandardOutput();
    if (!output.isEmpty()) {
        emit outputReady(QString::fromLocal8Bit(output));
    }
}

void TerminalProcess::handleError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    currentError = process.errorString();
    emit errorOccurred(currentError);
}

void TerminalProcess::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readOutput();
    hasPendingCommand = false;
    pendingCommand.clear();
    emit finished(exitCode, exitStatus);
}

bool TerminalProcess::writeCommand(const QString &command)
{
    if (!isRunning()) {
        return false;
    }

    QByteArray payload = command.toLocal8Bit();
#ifdef Q_OS_WIN
    payload.append("\r\n");
#else
    payload.append('\n');
#endif
    return process.write(payload) == payload.size();
}
