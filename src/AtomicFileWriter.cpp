/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "AtomicFileWriter.h"

#include <QSaveFile>

namespace
{
AtomicFileWriter::Result failure(QFileDevice::FileError error, const QString &message)
{
    return {error == QFileDevice::NoError ? QFileDevice::WriteError : error, message};
}
}

AtomicFileWriter::Result AtomicFileWriter::write(const QString &path, const QByteArray &data)
{
    return write(path, [&data](QIODevice *device, QString *errorString) {
        if (device->write(data) == data.size()) {
            return true;
        }

        if (errorString) {
            *errorString = QStringLiteral("The complete file contents could not be written.");
        }
        return false;
    });
}

AtomicFileWriter::Result AtomicFileWriter::write(const QString &path, const Writer &writer)
{
    if (!writer) {
        return failure(QFileDevice::WriteError, QStringLiteral("No file writer was provided."));
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return failure(file.error(), file.errorString());
    }

    QString writerError;
    if (!writer(&file, &writerError)) {
        const QString message = writerError.isEmpty() ? file.errorString() : writerError;
        return failure(file.error(), message);
    }

    if (file.error() != QFileDevice::NoError) {
        return failure(file.error(), file.errorString());
    }

    if (!file.commit()) {
        return failure(file.error(), file.errorString());
    }

    return {};
}
