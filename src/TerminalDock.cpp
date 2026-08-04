/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TerminalDock.h"

#include "TerminalProcess.h"

#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include <utility>

TerminalDock::TerminalDock(QWidget *parent)
    : QDockWidget(parent),
      process(new TerminalProcess(this))
{
    setObjectName(QStringLiteral("terminalDock"));
    setWindowTitle(tr("Terminal"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumHeight(180);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    output = new QPlainTextEdit(container);
    output->setObjectName(QStringLiteral("terminalOutput"));
    output->setAccessibleName(tr("Terminal output"));
    output->setAccessibleDescription(tr("Read-only output from the shell."));
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setMaximumBlockCount(10000);
    output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    output->setPlaceholderText(tr("Terminal output will appear here."));
    layout->addWidget(output, 1);

    auto *toolbar = new QWidget(container);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    workingDirectoryLabel = new QLabel(toolbar);
    workingDirectoryLabel->setObjectName(QStringLiteral("terminalWorkingDirectory"));
    workingDirectoryLabel->setAccessibleName(tr("Terminal working directory"));
    workingDirectoryLabel->setAccessibleDescription(tr("Directory used when the shell starts."));
    workingDirectoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    toolbarLayout->addWidget(workingDirectoryLabel, 1);

    auto *clearButton = new QPushButton(tr("Clear"), toolbar);
    clearButton->setObjectName(QStringLiteral("terminalClearButton"));
    clearButton->setToolTip(tr("Clear terminal output"));
    clearButton->setAccessibleDescription(tr("Clear all text from terminal output."));
    toolbarLayout->addWidget(clearButton);

    auto *restartButton = new QPushButton(tr("Restart"), toolbar);
    restartButton->setObjectName(QStringLiteral("terminalRestartButton"));
    restartButton->setToolTip(tr("Restart the shell"));
    restartButton->setAccessibleDescription(tr("Stop and restart the shell process."));
    toolbarLayout->addWidget(restartButton);

    auto *stopButton = new QPushButton(tr("Stop"), toolbar);
    stopButton->setObjectName(QStringLiteral("terminalStopButton"));
    stopButton->setToolTip(tr("Stop the shell"));
    stopButton->setAccessibleDescription(tr("Stop the running shell process."));
    toolbarLayout->addWidget(stopButton);

    layout->addWidget(toolbar);

    input = new QLineEdit(container);
    input->setObjectName(QStringLiteral("terminalInput"));
    input->setAccessibleName(tr("Terminal command"));
    input->setAccessibleDescription(tr("Enter a shell command and press Enter to run it."));
    input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    input->setPlaceholderText(tr("Enter a command and press Enter"));
    input->setClearButtonEnabled(true);
    layout->addWidget(input);

    QWidget::setTabOrder(output, clearButton);
    QWidget::setTabOrder(clearButton, restartButton);
    QWidget::setTabOrder(restartButton, stopButton);
    QWidget::setTabOrder(stopButton, input);

    setWidget(container);

    requestedWorkingDirectory = QDir::homePath();
    updateWorkingDirectoryLabel();

    connect(input, &QLineEdit::returnPressed, this, [this]() {
        executeCommand(input->text());
    });
    connect(clearButton, &QPushButton::clicked, this, &TerminalDock::clearOutput);
    connect(restartButton, &QPushButton::clicked, this, &TerminalDock::restartProcess);
    connect(stopButton, &QPushButton::clicked, this, &TerminalDock::stopProcess);
    connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible && !isProcessRunning()) {
            startProcess();
        }
    });

    connect(process, &TerminalProcess::started, this, &TerminalDock::handleStarted);
    connect(process, &TerminalProcess::outputReady, this, &TerminalDock::appendOutput);
    connect(process, &TerminalProcess::finished, this, &TerminalDock::handleFinished);
    connect(process, &TerminalProcess::errorOccurred, this, &TerminalDock::handleError);
}

TerminalDock::~TerminalDock()
{
    process->stop();
}

QString TerminalDock::workingDirectory() const
{
    return requestedWorkingDirectory;
}

bool TerminalDock::isProcessRunning() const
{
    return process->isRunning();
}

void TerminalDock::setWorkingDirectory(const QString &directory)
{
    const QString requested = directory.trimmed();
    const QString resolved = requested.isEmpty() ? QDir::homePath() : QDir(requested).absolutePath();
    if (!QDir(resolved).exists() || requestedWorkingDirectory == resolved) {
        return;
    }

    requestedWorkingDirectory = resolved;
    updateWorkingDirectoryLabel();
    if (isProcessRunning()) {
        appendOutput(tr("[Notepad Next] The active document directory changed. Restart the terminal to use %1.\n")
                         .arg(resolved));
    }
}

void TerminalDock::startProcess()
{
    if (isProcessRunning()) {
        return;
    }

    if (!process->start(requestedWorkingDirectory)) {
        handleError(process->errorString());
    }
}

void TerminalDock::restartProcess()
{
    pendingCommand.clear();
    process->stop();
    startProcess();
}

void TerminalDock::stopProcess()
{
    pendingCommand.clear();
    process->stop();
}

void TerminalDock::clearOutput()
{
    output->clear();
}

void TerminalDock::executeCommand(const QString &command)
{
    if (command.isEmpty()) {
        return;
    }

    appendOutput(QStringLiteral("> %1\n").arg(command));
    input->clear();

    if (!isProcessRunning()) {
        pendingCommand = command;
        startProcess();
        return;
    }

    if (!process->sendCommand(command)) {
        handleError(process->errorString().isEmpty()
                        ? tr("Unable to send the command to the terminal.")
                        : process->errorString());
    }
}

void TerminalDock::appendOutput(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    QTextCursor cursor = output->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    output->setTextCursor(cursor);
    output->ensureCursorVisible();
}

void TerminalDock::updateWorkingDirectoryLabel()
{
    workingDirectoryLabel->setText(tr("Working directory: %1").arg(requestedWorkingDirectory));
    workingDirectoryLabel->setToolTip(requestedWorkingDirectory);
}

void TerminalDock::handleStarted()
{
    appendOutput(tr("[Notepad Next] Started shell in %1\n").arg(process->workingDirectory()));
    if (!pendingCommand.isEmpty()) {
        const QString command = std::exchange(pendingCommand, QString());
        if (!process->sendCommand(command)) {
            handleError(process->errorString().isEmpty()
                            ? tr("Unable to send the command to the terminal.")
                            : process->errorString());
        }
    }
}

void TerminalDock::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    pendingCommand.clear();
    const QString status = exitStatus == QProcess::NormalExit
        ? tr("[Notepad Next] Shell exited with code %1.\n").arg(exitCode)
        : tr("[Notepad Next] Shell terminated unexpectedly.\n");
    appendOutput(status);
}

void TerminalDock::handleError(const QString &error)
{
    if (!error.isEmpty()) {
        appendOutput(tr("[Notepad Next] %1\n").arg(error));
    }
}
