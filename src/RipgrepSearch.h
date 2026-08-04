/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef RIPGREPSEARCH_H
#define RIPGREPSEARCH_H

#include <QProcess>
#include <QJsonObject>
#include <QSet>

class RipgrepSearch final : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString pattern;
        QString rootPath;
        bool regularExpression = false;
        bool caseSensitive = true;
        bool includeHidden = false;
        QStringList globs;
    };

    struct Match
    {
        QString filePath;
        QString lineText;
        int lineNumber = 0;
        int column = 0;
        int startByte = 0;
        int endByte = 0;
        int hitCount = 1;
    };

    explicit RipgrepSearch(QObject *parent = nullptr);
    ~RipgrepSearch() override;

    bool start(const Options &options);
    void cancel();
    bool isRunning() const;

signals:
    void matchFound(const RipgrepSearch::Match &match);
    void searchFinished(int matchCount, int fileCount, bool cancelled);
    void searchError(const QString &message);

private:
    void readStandardOutput();
    void readStandardError();
    void handleError(QProcess::ProcessError error);
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void consumeOutput();
    void consumeLine(const QByteArray &line);
    QString pathFromJson(const QJsonObject &pathObject) const;

    QProcess process;
    Options options;
    QByteArray outputBuffer;
    QByteArray errorBuffer;
    QSet<QString> matchedFiles;
    int matchCount = 0;
    bool cancelRequested = false;
};

Q_DECLARE_METATYPE(RipgrepSearch::Match)

#endif // RIPGREPSEARCH_H
