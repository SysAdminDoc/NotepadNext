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

#pragma once

#include <QString>

namespace PortableMode {

// Detect a removable launch volume or an explicit portable directory beside
// the executable, then route INI settings into that directory.
void configure(const QString &executablePath);

bool isEnabled();
QString profileDirectory();

// These helpers keep the detection rules testable without changing the host's
// storage configuration.
QString profileDirectoryForApplicationDirectory(const QString &applicationDirectory);
bool hasPortableDirectoryMarker(const QString &applicationDirectory);
bool isRemovableApplicationDirectory(const QString &applicationDirectory);

}
