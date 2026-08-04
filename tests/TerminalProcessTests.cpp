/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "TerminalProcess.h"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QString markerCommand()
{
#ifdef Q_OS_WIN
    return QStringLiteral("if exist terminal-marker.txt echo NOTEPADNEXT_TERMINAL_TEST");
#else
    return QStringLiteral("[ -f terminal-marker.txt ] && printf NOTEPADNEXT_TERMINAL_TEST");
#endif
}
}

class TerminalProcessTests final : public QObject
{
    Q_OBJECT

private slots:
    void selectsAnInteractiveShell();
    void runsCommandsInTheRequestedDirectory();
};

void TerminalProcessTests::selectsAnInteractiveShell()
{
    const TerminalProcess::ShellCommand shell = TerminalProcess::defaultShell();
    QVERIFY(!shell.program.isEmpty());
#ifdef Q_OS_WIN
    QVERIFY(shell.arguments.contains(QStringLiteral("/K")));
#else
    QVERIFY(shell.arguments.contains(QStringLiteral("-i")));
#endif
}

void TerminalProcessTests::runsCommandsInTheRequestedDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QFile marker(directory.filePath(QStringLiteral("terminal-marker.txt")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    TerminalProcess process;
    QSignalSpy outputSpy(&process, &TerminalProcess::outputReady);
    QSignalSpy finishedSpy(&process, &TerminalProcess::finished);

    QVERIFY(process.start(directory.path()));
    QTRY_VERIFY_WITH_TIMEOUT(process.isRunning(), 5000);
    QVERIFY(process.sendCommand(markerCommand()));

    QTRY_VERIFY_WITH_TIMEOUT([&outputSpy]() {
        for (const QList<QVariant> &signal : outputSpy) {
            if (signal.value(0).toString().contains(QStringLiteral("NOTEPADNEXT_TERMINAL_TEST"))) {
                return true;
            }
        }
        return false;
    }(), 5000);

    QVERIFY(process.sendCommand(QStringLiteral("exit")));
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QVERIFY(!process.isRunning());
}

QTEST_MAIN(TerminalProcessTests)
#include "TerminalProcessTests.moc"
