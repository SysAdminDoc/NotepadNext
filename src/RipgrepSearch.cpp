/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "RipgrepSearch.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

RipgrepSearch::RipgrepSearch(QObject *parent)
    : QObject(parent),
      process(this)
{
    process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&process, &QProcess::readyReadStandardOutput, this, &RipgrepSearch::readStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &RipgrepSearch::readStandardError);
    connect(&process, &QProcess::errorOccurred, this, &RipgrepSearch::handleError);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &RipgrepSearch::handleFinished);
}

RipgrepSearch::~RipgrepSearch()
{
    cancel();
}

bool RipgrepSearch::start(const Options &newOptions)
{
    if (isRunning()) {
        emit searchError(QStringLiteral("A file search is already running."));
        return false;
    }

    const QString pattern = newOptions.pattern;
    const QString rootPath = newOptions.rootPath.trimmed();
    if (pattern.isEmpty()) {
        emit searchError(QStringLiteral("Enter text or a regular expression to search for."));
        return false;
    }
    if (rootPath.isEmpty() || !QDir(rootPath).exists()) {
        emit searchError(QStringLiteral("Search folder does not exist: %1").arg(rootPath));
        return false;
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("rg"));
    if (executable.isEmpty()) {
        emit searchError(QStringLiteral("ripgrep (rg) was not found on PATH."));
        return false;
    }

    options = newOptions;
    options.rootPath = QDir(rootPath).absolutePath();
    outputBuffer.clear();
    errorBuffer.clear();
    matchedFiles.clear();
    matchCount = 0;
    cancelRequested = false;

    QStringList arguments{
        QStringLiteral("--json"),
        QStringLiteral("--line-number"),
        QStringLiteral("--column"),
        QStringLiteral("--color"),
        QStringLiteral("never"),
        QStringLiteral("--no-messages")
    };
    if (!options.regularExpression) {
        arguments.append(QStringLiteral("--fixed-strings"));
    }
    if (!options.caseSensitive) {
        arguments.append(QStringLiteral("--ignore-case"));
    }
    if (options.includeHidden) {
        arguments.append(QStringLiteral("--hidden"));
    }
    for (const QString &glob : options.globs) {
        if (!glob.trimmed().isEmpty()) {
            arguments.append(QStringLiteral("--glob"));
            arguments.append(glob);
        }
    }
    arguments.append(QStringLiteral("--"));
    arguments.append(options.pattern);
    arguments.append(options.rootPath);

    process.start(executable, arguments);
    return true;
}

void RipgrepSearch::cancel()
{
    if (!isRunning()) {
        return;
    }

    cancelRequested = true;
    process.terminate();
    if (!process.waitForFinished(500)) {
        process.kill();
        process.waitForFinished(500);
    }
}

bool RipgrepSearch::isRunning() const
{
    return process.state() != QProcess::NotRunning;
}

void RipgrepSearch::readStandardOutput()
{
    outputBuffer.append(process.readAllStandardOutput());
    consumeOutput();
}

void RipgrepSearch::readStandardError()
{
    errorBuffer.append(process.readAllStandardError());
}

void RipgrepSearch::handleError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        emit searchError(process.errorString());
    }
}

void RipgrepSearch::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readStandardOutput();
    readStandardError();
    consumeOutput();

    const QString error = QString::fromLocal8Bit(errorBuffer).trimmed();
    if (!cancelRequested && (exitStatus != QProcess::NormalExit || (exitCode != 0 && exitCode != 1))) {
        emit searchError(error.isEmpty()
                             ? QStringLiteral("ripgrep exited with code %1.").arg(exitCode)
                             : error);
    }

    emit searchFinished(matchCount, matchedFiles.size(), cancelRequested);
    cancelRequested = false;
}

void RipgrepSearch::consumeOutput()
{
    while (true) {
        const qsizetype newline = outputBuffer.indexOf('\n');
        if (newline < 0) {
            return;
        }

        QByteArray line = outputBuffer.left(newline);
        outputBuffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.trimmed().isEmpty()) {
            consumeLine(line);
        }
    }
}

void RipgrepSearch::consumeLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit searchError(QStringLiteral("ripgrep returned invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("type")).toString() != QStringLiteral("match")) {
        return;
    }

    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    const QString path = pathFromJson(data.value(QStringLiteral("path")).toObject());
    const QJsonObject lines = data.value(QStringLiteral("lines")).toObject();
    if (path.isEmpty() || lines.isEmpty()) {
        return;
    }

    Match match;
    match.filePath = path;
    match.lineText = lines.value(QStringLiteral("text")).toString();
    while (match.lineText.endsWith(QLatin1Char('\n')) || match.lineText.endsWith(QLatin1Char('\r'))) {
        match.lineText.chop(1);
    }
    match.lineNumber = data.value(QStringLiteral("line_number")).toInt();
    match.column = data.value(QStringLiteral("column")).toInt();

    const QJsonArray submatches = data.value(QStringLiteral("submatches")).toArray();
    match.hitCount = qMax(1, submatches.size());
    if (!submatches.isEmpty()) {
        const QJsonObject submatch = submatches.at(0).toObject();
        match.startByte = submatch.value(QStringLiteral("start")).toInt();
        match.endByte = submatch.value(QStringLiteral("end")).toInt();
    }
    if (match.column <= 0) {
        match.column = match.startByte + 1;
    }

    matchedFiles.insert(match.filePath);
    matchCount += match.hitCount;
    emit matchFound(match);
}

QString RipgrepSearch::pathFromJson(const QJsonObject &pathObject) const
{
    QString path = pathObject.value(QStringLiteral("text")).toString();
    if (path.isEmpty()) {
        path = QString::fromUtf8(pathObject.value(QStringLiteral("bytes")).toString().toUtf8());
    }
    if (path.isEmpty()) {
        return QString();
    }

    const QFileInfo fileInfo(path);
    if (fileInfo.isAbsolute()) {
        return QDir::cleanPath(fileInfo.absoluteFilePath());
    }
    return QDir::cleanPath(QDir(options.rootPath).absoluteFilePath(path));
}
