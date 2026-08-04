/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "HexDocument.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextCodec>

#include <memory>

namespace
{
constexpr int ProbeBytes = 8192;

bool startsWithTextBom(const QByteArray &data)
{
    return data.startsWith(QByteArray::fromHex("EFBBBF"))
        || data.startsWith(QByteArray::fromHex("FFFE"))
        || data.startsWith(QByteArray::fromHex("FEFF"))
        || data.startsWith(QByteArray::fromHex("FFFE0000"))
        || data.startsWith(QByteArray::fromHex("0000FEFF"));
}

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}
}

HexDocument::ProbeResult HexDocument::probeFile(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return ProbeResult::Unreadable;
    }

    const QByteArray sample = file.read(ProbeBytes);
    if (file.error() != QFileDevice::NoError) {
        setError(error, file.errorString());
        return ProbeResult::Unreadable;
    }
    if (sample.isEmpty() || startsWithTextBom(sample)) {
        return ProbeResult::Text;
    }

    if (sample.contains('\0')) {
        return ProbeResult::Binary;
    }

    int controlCharacters = 0;
    for (const unsigned char byte : sample) {
        if ((byte < 0x09) || (byte > 0x0D && byte < 0x20)) {
            ++controlCharacters;
        }
    }
    if (controlCharacters > qMax(1, sample.size() / 20)) {
        return ProbeResult::Binary;
    }

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (codec) {
        const std::unique_ptr<QTextDecoder> decoder(codec->makeDecoder());
        decoder->toUnicode(sample);
        if (decoder->hasFailure()) {
            return ProbeResult::Binary;
        }
    }

    return ProbeResult::Text;
}

bool HexDocument::load(const QString &filePath, QString *error)
{
    QFileInfo info(filePath);
    if (!info.isFile()) {
        setError(error, QStringLiteral("The file does not exist."));
        return false;
    }
    if (info.size() > MaximumFileSize) {
        setError(error, QStringLiteral("The file is larger than the 64 MiB hex-editor limit."));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }
    const QByteArray loaded = file.readAll();
    if (file.error() != QFileDevice::NoError || loaded.size() != info.size()) {
        setError(error, file.errorString().isEmpty()
                      ? QStringLiteral("The file could not be read completely.")
                      : file.errorString());
        return false;
    }

    bytes = loaded;
    path = info.absoluteFilePath();
    dirty = false;
    readOnly = !info.isWritable();
    return true;
}

bool HexDocument::save(QString *error)
{
    if (path.isEmpty()) {
        setError(error, QStringLiteral("No file is open."));
        return false;
    }
    if (readOnly) {
        setError(error, QStringLiteral("The file is read-only."));
        return false;
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        setError(error, file.errorString());
        return false;
    }

    dirty = false;
    QFileInfo info(path);
    readOnly = !info.isWritable();
    return true;
}

bool HexDocument::setByte(qsizetype index, uchar value)
{
    if (readOnly || index < 0 || index >= bytes.size()) {
        return false;
    }

    if (static_cast<uchar>(bytes.at(index)) == value) {
        return true;
    }

    bytes[index] = static_cast<char>(value);
    dirty = true;
    return true;
}

void HexDocument::discardChanges()
{
    if (!dirty || path.isEmpty()) {
        return;
    }

    QString ignored;
    load(path, &ignored);
}
