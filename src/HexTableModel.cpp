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

#include "HexTableModel.h"

#include <QStringView>

namespace
{
QString formatByte(uchar value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString formatAscii(uchar value)
{
    return value >= 0x20 && value <= 0x7E ? QString(QChar(value)) : QStringLiteral(".");
}
}

HexTableModel::HexTableModel(HexDocument *document, QObject *parent)
    : QAbstractTableModel(parent)
    , document(document)
{
}

int HexTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>((document->size() + 15) / 16);
}

int HexTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 18;
}

QVariant HexTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int byte = byteIndex(index);
    if (index.column() == 0) {
        if (role == Qt::DisplayRole) {
            return QStringLiteral("%1").arg(index.row() * 16, 8, 16, QLatin1Char('0')).toUpper();
        }
        return QVariant();
    }

    if (index.column() == 17) {
        if (role != Qt::DisplayRole && role != Qt::EditRole) {
            return QVariant();
        }

        QString ascii;
        const int rowStart = index.row() * 16;
        const int rowEnd = qMin(rowStart + 16, static_cast<int>(document->size()));
        ascii.reserve(rowEnd - rowStart);
        for (int rowByte = rowStart; rowByte < rowEnd; ++rowByte) {
            ascii += formatAscii(static_cast<uchar>(document->data().at(rowByte)));
        }
        return ascii;
    }

    if (byte < 0 || byte >= document->size()) {
        return QVariant();
    }

    const uchar value = static_cast<uchar>(document->data().at(byte));
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return formatByte(value);
    }
    if (role == Qt::ToolTipRole) {
        return QStringLiteral("Byte %1: 0x%2 (%3)")
            .arg(byte)
            .arg(formatByte(value))
            .arg(value);
    }
    return QVariant();
}

bool HexTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || index.column() < 1 || index.column() > 16) {
        return false;
    }

    const int byte = byteIndex(index);
    if (byte < 0 || byte >= document->size()) {
        return false;
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty() || text.size() > 2) {
        return false;
    }
    bool ok = false;
    const uint parsed = text.toUInt(&ok, 16);
    if (!ok || parsed > 0xFF || !document->setByte(byte, static_cast<uchar>(parsed))) {
        return false;
    }

    const int row = index.row();
    emit dataChanged(this->index(row, index.column()), this->index(row, 17), {Qt::DisplayRole, Qt::EditRole});
    return true;
}

Qt::ItemFlags HexTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() >= 1 && index.column() <= 16 && byteIndex(index) < document->size()
        && !document->isReadOnly()) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

QVariant HexTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    if (orientation == Qt::Vertical) {
        return section + 1;
    }
    if (section == 0) {
        return QStringLiteral("Offset");
    }
    if (section == 17) {
        return QStringLiteral("ASCII");
    }
    return QStringLiteral("%1").arg(section - 1, 1, 16).toUpper();
}

bool HexTableModel::loadFile(const QString &filePath, QString *error)
{
    if (!document->load(filePath, error)) {
        return false;
    }
    beginResetModel();
    endResetModel();
    return true;
}

bool HexTableModel::reload(QString *error)
{
    return loadFile(document->filePath(), error);
}

bool HexTableModel::save(QString *error)
{
    return document->save(error);
}

int HexTableModel::byteIndex(const QModelIndex &index) const
{
    if (index.column() < 1 || index.column() > 16) {
        return -1;
    }
    return index.row() * 16 + index.column() - 1;
}
