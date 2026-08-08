/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#ifndef ATOMICFILEWRITER_H
#define ATOMICFILEWRITER_H

#include <QFileDevice>
#include <QString>

#include <functional>

class QIODevice;

class AtomicFileWriter final
{
public:
    struct Result
    {
        QFileDevice::FileError error = QFileDevice::NoError;
        QString errorString;

        bool succeeded() const { return error == QFileDevice::NoError; }
    };

    using Writer = std::function<bool(QIODevice *device, QString *errorString)>;

    static Result write(const QString &path, const QByteArray &data);
    static Result write(const QString &path, const Writer &writer);
};

#endif // ATOMICFILEWRITER_H
