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

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "PortableMode.h"

class PortableModeTests : public QObject
{
    Q_OBJECT

private slots:
    void profileDirectoryIsStable()
    {
        QCOMPARE(PortableMode::profileDirectoryForApplicationDirectory(QStringLiteral("C:/Tools/NotepadNext")),
                 QDir::cleanPath(QStringLiteral("C:/Tools/NotepadNext/portable")));
    }

    void portableDirectoryActsAsMarker()
    {
        QTemporaryDir applicationDirectory;
        QVERIFY(applicationDirectory.isValid());

        QVERIFY(!PortableMode::hasPortableDirectoryMarker(applicationDirectory.path()));
        QVERIFY(QDir().mkpath(PortableMode::profileDirectoryForApplicationDirectory(applicationDirectory.path())));
        QVERIFY(PortableMode::hasPortableDirectoryMarker(applicationDirectory.path()));
    }

    void configureRoutesSettingsToPortableProfile()
    {
        QTemporaryDir applicationDirectory;
        QVERIFY(applicationDirectory.isValid());
        const QString portableDirectory = PortableMode::profileDirectoryForApplicationDirectory(applicationDirectory.path());
        QVERIFY(QDir().mkpath(portableDirectory));

        QSettings::setDefaultFormat(QSettings::IniFormat);
        PortableMode::configure(QDir(applicationDirectory.path()).filePath(QStringLiteral("NotepadNext.exe")));

        QVERIFY(PortableMode::isEnabled());
        QCOMPARE(QDir::cleanPath(PortableMode::profileDirectory()), QDir::cleanPath(portableDirectory));

        QSettings settings;
        QVERIFY(QDir::cleanPath(settings.fileName()).startsWith(QDir::cleanPath(portableDirectory)));
    }
};

QTEST_MAIN(PortableModeTests)
#include "PortableModeTests.moc"
