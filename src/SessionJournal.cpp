/*
 * This file is part of Notepad Next.
 * Copyright 2026 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SessionJournal.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace SessionJournal
{
QString manifestFileName()
{
    return QStringLiteral("session-manifest.json");
}

QByteArray serialize(const Manifest &manifest)
{
    QJsonObject object;
    object[QStringLiteral("Version")] = Version;
    object[QStringLiteral("Generation")] = manifest.generation;
    object[QStringLiteral("CurrentEditorIndex")] = manifest.currentEditorIndex;
    object[QStringLiteral("OpenedFiles")] = manifest.openedFiles;

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<Manifest> parse(const QByteArray &data, QString *errorString)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorString != nullptr) {
            *errorString = parseError.errorString();
        }
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("Version")).toInt() != Version) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("unsupported session journal version");
        }
        return std::nullopt;
    }

    if (!object.value(QStringLiteral("Generation")).isString() ||
        !object.value(QStringLiteral("OpenedFiles")).isArray()) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("missing session journal fields");
        }
        return std::nullopt;
    }

    Manifest manifest;
    manifest.generation = object.value(QStringLiteral("Generation")).toString();
    manifest.currentEditorIndex = object.value(QStringLiteral("CurrentEditorIndex")).toInt(-1);
    manifest.openedFiles = object.value(QStringLiteral("OpenedFiles")).toArray();
    return manifest;
}

bool isSafeBufferName(const QDir &generationDirectory, const QString &fileName)
{
    if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName) {
        return false;
    }

    const QFileInfo candidate(generationDirectory.filePath(fileName));
    return candidate.absolutePath() == generationDirectory.absolutePath();
}
}
