/*
 * This file is part of Notepad Next.
 * Copyright 2026 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SESSIONJOURNAL_H
#define SESSIONJOURNAL_H

#include <QDir>
#include <QJsonArray>
#include <QString>

#include <optional>

namespace SessionJournal
{
constexpr int Version = 1;

struct Manifest
{
    QString generation;
    int currentEditorIndex = -1;
    QJsonArray openedFiles;
};

QString manifestFileName();

QByteArray serialize(const Manifest &manifest);
std::optional<Manifest> parse(const QByteArray &data, QString *errorString = nullptr);

bool isSafeBufferName(const QDir &generationDirectory, const QString &fileName);
}

#endif // SESSIONJOURNAL_H
