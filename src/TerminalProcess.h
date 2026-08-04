/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TERMINALPROCESS_H
#define TERMINALPROCESS_H

#include <QProcess>

class TerminalProcess final : public QObject
{
    Q_OBJECT

public:
    struct ShellCommand
    {
        QString program;
        QStringList arguments;
    };

    explicit TerminalProcess(QObject *parent = nullptr);
    ~TerminalProcess() override;

    static ShellCommand defaultShell();

    bool start(const QString &workingDirectory);
    void stop();
    bool sendCommand(const QString &command);

    bool isRunning() const;
    QString workingDirectory() const;
    QString errorString() const;

signals:
    void started();
    void outputReady(const QString &output);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void errorOccurred(const QString &error);

private:
    void readOutput();
    void handleError(QProcess::ProcessError error);
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    bool writeCommand(const QString &command);

    QProcess process;
    QString currentWorkingDirectory;
    QString currentError;
    QString pendingCommand;
    bool hasPendingCommand = false;
};

#endif // TERMINALPROCESS_H
