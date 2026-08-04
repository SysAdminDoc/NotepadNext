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

#include "PortableMode.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStorageInfo>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {

bool portableEnabled = false;
QString portableProfile;

QString applicationDirectoryFromPath(const QString &executablePath)
{
    QFileInfo executableInfo(executablePath);
    if (!executableInfo.isAbsolute()) {
        executableInfo = QFileInfo(QDir::current().absoluteFilePath(executablePath));
    }

    const QString canonicalPath = executableInfo.canonicalFilePath();
    return QFileInfo(canonicalPath.isEmpty() ? executableInfo.absoluteFilePath() : canonicalPath).absolutePath();
}

#ifdef Q_OS_WIN
bool isRemovableWindowsDirectory(const QString &applicationDirectory)
{
    const QString nativeDirectory = QDir::toNativeSeparators(applicationDirectory);
    wchar_t volumePath[MAX_PATH] = {};

    if (!GetVolumePathNameW(reinterpret_cast<LPCWSTR>(nativeDirectory.utf16()), volumePath, MAX_PATH)) {
        return false;
    }

    return GetDriveTypeW(volumePath) == DRIVE_REMOVABLE;
}
#endif

}

namespace PortableMode {

QString profileDirectoryForApplicationDirectory(const QString &applicationDirectory)
{
    return QDir(applicationDirectory).filePath(QStringLiteral("portable"));
}

bool hasPortableDirectoryMarker(const QString &applicationDirectory)
{
    return QFileInfo(profileDirectoryForApplicationDirectory(applicationDirectory)).isDir();
}

bool isRemovableApplicationDirectory(const QString &applicationDirectory)
{
#ifdef Q_OS_WIN
    return isRemovableWindowsDirectory(applicationDirectory);
#else
    const QStorageInfo storage(applicationDirectory);
    if (!storage.isValid() || !storage.isReady()) {
        return false;
    }

    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(applicationDirectory));
#if defined(Q_OS_MACOS)
    return path.startsWith(QStringLiteral("/Volumes/"));
#else
    return path.startsWith(QStringLiteral("/media/"))
        || path.startsWith(QStringLiteral("/run/media/"))
        || path.startsWith(QStringLiteral("/mnt/"));
#endif
#endif
}

void configure(const QString &executablePath)
{
    portableEnabled = false;
    portableProfile.clear();

    const QString applicationDirectory = applicationDirectoryFromPath(executablePath);
    const bool hasMarker = hasPortableDirectoryMarker(applicationDirectory);
    const bool isRemovable = isRemovableApplicationDirectory(applicationDirectory);

    if (!hasMarker && !isRemovable) {
        return;
    }

    const QString profile = profileDirectoryForApplicationDirectory(applicationDirectory);
    if (!QDir().mkpath(profile)) {
        qWarning("Portable mode detected, but the profile directory could not be created: %s",
                 qUtf8Printable(profile));
        return;
    }

    portableProfile = QFileInfo(profile).absoluteFilePath();
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, portableProfile);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, portableProfile);
    portableEnabled = true;

    qInfo("Portable mode enabled: %s (%s)",
          qUtf8Printable(portableProfile),
          isRemovable ? "removable launch volume" : "portable directory marker");
}

bool isEnabled()
{
    return portableEnabled;
}

QString profileDirectory()
{
    return portableProfile;
}

}
