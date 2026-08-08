/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "NppPluginTrust.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace
{
constexpr auto TrustedPluginsKey = "Plugins/TrustedPlugins";
constexpr auto UserPluginsEnabledKey = "Plugins/UserPluginsEnabled";

QString pathKey(const QString &path)
{
    return path.toCaseFolded();
}

bool isWithinRoot(const QString &filePath, const QString &rootPath)
{
    const QString relative = QDir(rootPath).relativeFilePath(filePath);
    return relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))
        && !relative.startsWith(QStringLiteral("..\\"));
}

QVariantMap toVariantMap(const NppPluginTrust::Identity &identity)
{
    return {
        {QStringLiteral("pluginId"), identity.pluginId},
        {QStringLiteral("canonicalPath"), identity.canonicalPath},
        {QStringLiteral("sha256"), QString::fromLatin1(identity.sha256.toHex())},
        {QStringLiteral("abi"), identity.abi},
        {QStringLiteral("location"), static_cast<int>(identity.location)},
    };
}

bool fromVariantMap(const QVariantMap &map, NppPluginTrust::Identity *identity)
{
    if (!identity) {
        return false;
    }

    const QString pluginId = map.value(QStringLiteral("pluginId")).toString().trimmed();
    const QString canonicalPath = map.value(QStringLiteral("canonicalPath")).toString();
    const QByteArray sha256 = QByteArray::fromHex(map.value(QStringLiteral("sha256")).toString().toLatin1());
    const QString abi = map.value(QStringLiteral("abi")).toString();
    if (pluginId.isEmpty() || canonicalPath.isEmpty() || sha256.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)
        || abi != QString::fromLatin1(NppPluginTrust::CompatibilityAbi)) {
        return false;
    }

    identity->pluginId = pluginId;
    identity->canonicalPath = canonicalPath;
    identity->sha256 = sha256;
    identity->abi = abi;
    identity->location = static_cast<NppPluginTrust::Location>(map.value(QStringLiteral("location")).toInt());
    return identity->location == NppPluginTrust::Location::Application
        || identity->location == NppPluginTrust::Location::User;
}
}

namespace NppPluginTrust
{
QVector<Candidate> discover(const QString &applicationRoot, const QString &userRoot)
{
    QVector<Candidate> candidates;
    QSet<QString> seen;

    const auto appendRoot = [&](const QString &root, Location location) {
        const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
        if (canonicalRoot.isEmpty() || !QDir(canonicalRoot).exists()) {
            return;
        }

        QDirIterator iterator(canonicalRoot,
                              {QStringLiteral("*.dll")},
                              QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            const QString canonicalPath = QFileInfo(path).canonicalFilePath();
            if (canonicalPath.isEmpty() || !isWithinRoot(canonicalPath, canonicalRoot)) {
                continue;
            }

            const QString key = pathKey(canonicalPath);
            if (seen.contains(key)) {
                continue;
            }

            seen.insert(key);
            candidates.append({canonicalPath, QFileInfo(canonicalPath).completeBaseName(), location});
        }
    };

    appendRoot(applicationRoot, Location::Application);
    appendRoot(userRoot, Location::User);

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &left, const Candidate &right) {
        return left.canonicalPath.compare(right.canonicalPath, Qt::CaseInsensitive) < 0;
    });
    return candidates;
}

bool identify(const Candidate &candidate, Identity *identity, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (!identity || candidate.canonicalPath.isEmpty()) {
        return fail(QStringLiteral("The plugin path is empty."));
    }

    QFileInfo fileInfo(candidate.canonicalPath);
    if (!fileInfo.isFile()) {
        return fail(QStringLiteral("The plugin file does not exist."));
    }

    QFile file(candidate.canonicalPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Unable to read the plugin file: %1").arg(file.errorString()));
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return fail(QStringLiteral("Unable to hash the plugin file: %1").arg(file.errorString()));
    }

    identity->pluginId = candidate.displayName.isEmpty() ? fileInfo.completeBaseName() : candidate.displayName;
    identity->canonicalPath = fileInfo.canonicalFilePath();
    identity->sha256 = hash.result();
    identity->abi = QString::fromLatin1(CompatibilityAbi);
    identity->location = candidate.location;
    if (error) {
        error->clear();
    }
    return true;
}

TrustStore::TrustStore(QSettings *settings)
    : settings(settings)
{
}

QVector<Identity> TrustStore::entries() const
{
    QVector<Identity> result;
    if (!settings) {
        return result;
    }

    const QVariantList stored = settings->value(QLatin1String(TrustedPluginsKey)).toList();
    for (const QVariant &value : stored) {
        Identity identity;
        if (fromVariantMap(value.toMap(), &identity)) {
            result.append(identity);
        }
    }
    return result;
}

bool TrustStore::isTrusted(const Identity &identity) const
{
    const QVector<Identity> trusted = entries();
    return std::any_of(trusted.cbegin(), trusted.cend(), [&identity](const Identity &entry) {
        return pathKey(entry.canonicalPath) == pathKey(identity.canonicalPath)
            && entry.sha256 == identity.sha256
            && entry.abi == identity.abi;
    });
}

void TrustStore::trust(const Identity &identity)
{
    if (!settings || identity.canonicalPath.isEmpty() || identity.sha256.isEmpty()) {
        return;
    }

    QVariantList stored;
    for (const Identity &entry : entries()) {
        if (pathKey(entry.canonicalPath) != pathKey(identity.canonicalPath)
            || entry.abi != identity.abi) {
            stored.append(toVariantMap(entry));
        }
    }
    stored.append(toVariantMap(identity));
    settings->setValue(QLatin1String(TrustedPluginsKey), stored);
    settings->sync();
}

void TrustStore::revoke(const Identity &identity)
{
    if (!settings) {
        return;
    }

    QVariantList stored;
    for (const Identity &entry : entries()) {
        if (pathKey(entry.canonicalPath) != pathKey(identity.canonicalPath)
            || entry.abi != identity.abi) {
            stored.append(toVariantMap(entry));
        }
    }
    settings->setValue(QLatin1String(TrustedPluginsKey), stored);
    settings->sync();
}

void TrustStore::revokeAll()
{
    if (!settings) {
        return;
    }

    settings->remove(QLatin1String(TrustedPluginsKey));
    settings->sync();
}

bool TrustStore::userPluginsEnabled() const
{
    return settings && settings->value(QLatin1String(UserPluginsEnabledKey), false).toBool();
}

void TrustStore::setUserPluginsEnabled(bool enabled)
{
    if (!settings) {
        return;
    }

    settings->setValue(QLatin1String(UserPluginsEnabledKey), enabled);
    settings->sync();
}
}
