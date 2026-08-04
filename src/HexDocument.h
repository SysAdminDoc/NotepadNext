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

#ifndef HEXDOCUMENT_H
#define HEXDOCUMENT_H

#include <QByteArray>
#include <QString>

class HexDocument final
{
public:
    enum class ProbeResult
    {
        Text,
        Binary,
        Unreadable
    };

    static constexpr qint64 MaximumFileSize = 64 * 1024 * 1024;

    static ProbeResult probeFile(const QString &filePath, QString *error = nullptr);

    bool load(const QString &filePath, QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool setByte(qsizetype index, uchar value);
    void discardChanges();

    const QByteArray &data() const { return bytes; }
    qsizetype size() const { return bytes.size(); }
    QString filePath() const { return path; }
    bool isDirty() const { return dirty; }
    bool isReadOnly() const { return readOnly; }

private:
    QByteArray bytes;
    QString path;
    bool dirty = false;
    bool readOnly = false;
};

#endif // HEXDOCUMENT_H
