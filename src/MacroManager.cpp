/*
 * This file is part of Notepad Next.
 * Copyright 2024 Justin Dailey
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

#include "MacroManager.h"
#include "ApplicationSettings.h"

MacroManager::MacroManager(QObject *parent) :
    QObject{parent}
{
    qInfo(Q_FUNC_INFO);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators<Macro>("Macro");
#else
    // HACK: For some reason this is required to make QVariant recognize it as a valid type
    // see https://stackoverflow.com/q/70974383
    QMetaType::fromType<Macro>().hasRegisteredDataStreamOperators();
#endif

    loadSettings();
}

MacroManager::~MacroManager()
{
    if (_isRecording) {
        delete recorder.stopRecording();
        _isRecording = false;
    }

    saveSettings();

    if (currentMacro != Q_NULLPTR && !macros.contains(currentMacro)) {
        delete currentMacro;
    }
    qDeleteAll(macros);
}

bool MacroManager::isRecording() const
{
    return _isRecording;
}

void MacroManager::startRecording(ScintillaNext *editor)
{
    qInfo(Q_FUNC_INFO);
    if (_isRecording || editor == Q_NULLPTR) {
        qWarning("MacroManager: recording is already active or has no editor");
        return;
    }

    _isRecording = true;

    recorder.startRecording(editor);

    emit recordingStarted();
}

void MacroManager::stopRecording()
{
    qInfo(Q_FUNC_INFO);
    if (!_isRecording) {
        qWarning("MacroManager: ignoring stop without an active recording");
        return;
    }

    _isRecording = false;

    Macro *m = recorder.stopRecording();

    if (m == Q_NULLPTR || m->size() == 0) {
        // If there were no actions recorded, delete it
        delete m;
    }
    else {
        if (isCurrentMacroSaved == false) {
            // The previous current macro wasn't saved and we are getting ready to point to something else, delete it
            delete currentMacro;
        }

        isCurrentMacroSaved = false;
        currentMacro = m;
    }

    emit recordingStopped();
}

void MacroManager::loadSettings()
{
    qInfo(Q_FUNC_INFO);

    ApplicationSettings settings;

    int size = settings.beginReadArray("Macros");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        if (settings.value("Macro").canConvert<Macro>()) {
            Macro *m = new Macro(settings.value("Macro").value<Macro>());
            m->setShortcut(QKeySequence(settings.value("Shortcut").toString()));
            macros.append(m);
        }
        else {
            qWarning("MacroManager: Skipping invalid Macro");
        }
    }
    settings.endArray();
}

void MacroManager::saveSettings() const
{
    qInfo(Q_FUNC_INFO);

    ApplicationSettings settings;

    settings.remove("Macros");

    if (macros.size() > 0) {
        settings.beginWriteArray("Macros");
        for (int i = 0; i < macros.size(); ++i) {
            settings.setArrayIndex(i);
            settings.setValue("Macro", QVariant::fromValue(*macros.at(i)));
            settings.setValue("Shortcut", macros.at(i)->getShortcut().toString());
        }
        settings.endArray();
    }

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("MacroManager: failed to persist macros (%d)", settings.status());
    }
}

void MacroManager::replayCurrentMacro(ScintillaNext *editor)
{
    qInfo(Q_FUNC_INFO);

    if (currentMacro == Q_NULLPTR || editor == Q_NULLPTR) {
        qWarning("MacroManager: no current macro or editor to replay");
        return;
    }

    currentMacro->replay(editor);
}

bool MacroManager::saveCurrentMacro(const QString &macroName, const QKeySequence &shortcut)
{
    qInfo(Q_FUNC_INFO);

    const QString name = macroName.trimmed();
    if (currentMacro == Q_NULLPTR || isCurrentMacroSaved || name.isEmpty()
            || !isMacroNameAvailable(name) || !isMacroShortcutAvailable(shortcut)) {
        return false;
    }

    isCurrentMacroSaved = true;

    currentMacro->setName(name);
    currentMacro->setShortcut(shortcut);
    macros.append(currentMacro);
    saveSettings();
    return true;
}

bool MacroManager::isMacroNameAvailable(const QString &macroName) const
{
    const QString name = macroName.trimmed();
    if (name.isEmpty()) {
        return false;
    }

    for (const Macro *macro : macros) {
        if (macro != Q_NULLPTR && QString::compare(macro->getName(), name, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }

    return true;
}

bool MacroManager::isMacroShortcutAvailable(const QKeySequence &shortcut) const
{
    if (shortcut.isEmpty()) {
        return true;
    }

    for (const Macro *macro : macros) {
        if (macro != Q_NULLPTR && macro->getShortcut() == shortcut) {
            return false;
        }
    }

    return true;
}

bool MacroManager::hasCurrentUnsavedMacro() const
{
    return currentMacro != Q_NULLPTR && isCurrentMacroSaved == false;
}
