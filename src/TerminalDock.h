/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TERMINALDOCK_H
#define TERMINALDOCK_H

#include <QDockWidget>
#include <QProcess>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class TerminalProcess;
namespace CapabilityTrust { class Manager; }

class TerminalDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit TerminalDock(QWidget *parent = nullptr);
    TerminalDock(CapabilityTrust::Manager *trustManager, QWidget *parent);
    ~TerminalDock() override;

    QString workingDirectory() const;
    bool isProcessRunning() const;

public slots:
    void setWorkingDirectory(const QString &directory);
    void startProcess();
    void restartProcess();
    void stopProcess();
    void clearOutput();
    void executeCommand(const QString &command);

private:
    void appendOutput(const QString &text);
    void updateWorkingDirectoryLabel();
    void handleStarted();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(const QString &error);

    TerminalProcess *process = nullptr;
    CapabilityTrust::Manager *trustManager = nullptr;
    QPlainTextEdit *output = nullptr;
    QLineEdit *input = nullptr;
    QLabel *workingDirectoryLabel = nullptr;
    QString requestedWorkingDirectory;
    QString pendingCommand;
};

#endif // TERMINALDOCK_H
