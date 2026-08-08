/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef NPPPLUGINTRUST_H
#define NPPPLUGINTRUST_H

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QVector>

namespace NppPluginTrust
{
inline constexpr char CompatibilityAbi[] = "notepadnext-npp-compatible-v1";

enum class Location {
    Application,
    User,
};

struct Candidate
{
    QString canonicalPath;
    QString displayName;
    Location location = Location::Application;
};

struct Identity
{
    QString pluginId;
    QString canonicalPath;
    QByteArray sha256;
    QString abi = QString::fromLatin1(CompatibilityAbi);
    Location location = Location::Application;
};

QVector<Candidate> discover(const QString &applicationRoot,
                            const QString &userRoot);

bool identify(const Candidate &candidate, Identity *identity, QString *error = nullptr);

class TrustStore final
{
public:
    explicit TrustStore(QSettings *settings);

    bool isTrusted(const Identity &identity) const;
    void trust(const Identity &identity);
    void revoke(const Identity &identity);
    void revokeAll();
    QVector<Identity> entries() const;

    bool userPluginsEnabled() const;
    void setUserPluginsEnabled(bool enabled);

private:
    QSettings *settings = nullptr;
};
}

#endif // NPPPLUGINTRUST_H
