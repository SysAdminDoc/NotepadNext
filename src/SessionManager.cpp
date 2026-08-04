/*
 * This file is part of Notepad Next.
 * Copyright 2022 Justin Dailey
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


#include "BookMarkDecorator.h"
#include "ScintillaNext.h"
#include "MainWindow.h"
#include "SessionManager.h"
#include "SessionJournal.h"
#include "SftpManager.h"
#include "EditorManager.h"
#include "NotepadNextApplication.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>


static QString RandomSessionFileName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

static QJsonObject editorViewDetails(ScintillaNext *editor)
{
    QJsonObject details;
    details[QStringLiteral("FirstVisibleLine")] = static_cast<int>(editor->firstVisibleLine() + 1);
    details[QStringLiteral("CurrentPosition")] = static_cast<int>(editor->currentPos());

    BookMarkDecorator *decorator = editor->findChild<BookMarkDecorator*>(QString(), Qt::FindDirectChildrenOnly);
    const QList<int> bookMarkedLines = decorator->bookMarkedLines();
    if (!bookMarkedLines.isEmpty()) {
        QJsonArray bookmarks;
        for (const int line : bookMarkedLines) {
            bookmarks.append(line);
        }
        details[QStringLiteral("BookMarks")] = bookmarks;
    }

    return details;
}

static void loadJournalEditorViewDetails(ScintillaNext *editor, const QJsonObject &details)
{
    const int firstVisibleLine = qMax(0, details.value(QStringLiteral("FirstVisibleLine")).toInt(1) - 1);
    const int currentPosition = qMax(0, details.value(QStringLiteral("CurrentPosition")).toInt());

    editor->setFirstVisibleLine(firstVisibleLine);
    editor->setEmptySelection(currentPosition);

    if (details.contains(QStringLiteral("BookMarks"))) {
        QList<int> bookMarkedLines;
        for (const QJsonValue &value : details.value(QStringLiteral("BookMarks")).toArray()) {
            bookMarkedLines.append(value.toInt());
        }

        BookMarkDecorator *decorator = editor->findChild<BookMarkDecorator*>(QString(), Qt::FindDirectChildrenOnly);
        decorator->setBookMarkedLines(bookMarkedLines);
    }
}

// QList<int> cannot be automatically serialized to/from QSettings (i.e. QVariant) so turn it to a QVariantList
static QVariantList QListToQVariantList(const QList<int> intList)
{
    QVariantList vl;
    for (const int i : intList){
        vl.append(i);
    }
    return vl;
}

// Do the opposite of the function above
static QList<int> QVariantListToQList(const QVariantList &variantList) {
    QList<int> intList;
    for (const QVariant &variant : variantList) {
        intList.append(variant.toInt());
    }
    return intList;
}

SessionManager::SessionManager(NotepadNextApplication *app, SessionFileTypes types)
    : app(app)
{
    setSessionFileTypes(types);
}

void SessionManager::setSessionFileTypes(SessionFileTypes types)
{
    fileTypes = types;
}

QDir SessionManager::sessionDirectory() const
{
    QDir d(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    d.mkpath("session");
    d.cd("session");

    return d;
}

QString SessionManager::sessionManifestPath() const
{
    return sessionDirectory().filePath(SessionJournal::manifestFileName());
}

void SessionManager::saveIntoSessionDirectory(ScintillaNext *editor, const QString &sessionFileName) const
{
    editor->saveCopyAs(sessionDirectory().filePath(sessionFileName));
}

SessionManager::SessionFileType SessionManager::determineType(ScintillaNext *editor) const
{
    if (app->getSftpManager() && app->getSftpManager()->isRemote(editor)) {
        return SessionManager::None;
    }

    if (editor->isFile()) {
        if (editor->isSavedToDisk()) {
            return SessionManager::SavedFile;
        }
        else {
            return SessionManager::UnsavedFile;
        }
    }
    else {
        if (!editor->isSavedToDisk()) {
            return SessionManager::TempFile;
        }
        else {
            return SessionManager::None;
        }
    }
}

void SessionManager::clear() const
{
    clearSettings();
    clearDirectory();
}

void SessionManager::clearSettings() const
{
    ApplicationSettings settings;;

    // Clear everything out. There can be left over entries that are no longer needed
    settings.beginGroup("CurrentSession");
    settings.remove("");
}

void SessionManager::clearDirectory() const
{
    QDir d = sessionDirectory();

    for (const QString &entry : d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        const QFileInfo fileInfo(d.filePath(entry));
        if (fileInfo.isDir()) {
            QDir(fileInfo.absoluteFilePath()).removeRecursively();
        }
        else {
            d.remove(entry);
        }
    }
}

bool SessionManager::saveJournal(MainWindow *window)
{
    QDir root = sessionDirectory();
    const QString generation = QStringLiteral("generation-%1").arg(RandomSessionFileName());
    if (!root.mkpath(generation)) {
        qWarning("Unable to create session journal generation %s", qUtf8Printable(generation));
        return false;
    }

    const QDir generationDirectory(root.filePath(generation));
    const auto removeGeneration = [&generationDirectory]() {
        QDir directory(generationDirectory);
        if (!directory.removeRecursively()) {
            qWarning("Unable to remove incomplete session journal generation %s", qUtf8Printable(directory.path()));
        }
    };

    QJsonArray openedFiles;
    const ScintillaNext *currentEditor = window->currentEditor();
    int currentEditorIndex = -1;
    int index = 0;

    for (ScintillaNext *editor : window->editors()) {
        const SessionFileType editorType = determineType(editor);
        if (editorType == SessionManager::None || !fileTypes.testFlag(editorType)) {
            continue;
        }

        QJsonObject details;
        if (editorType == SessionManager::SavedFile) {
            details[QStringLiteral("Type")] = QStringLiteral("File");
            details[QStringLiteral("FilePath")] = editor->getFilePath();
        }
        else if (editorType == SessionManager::UnsavedFile) {
            details[QStringLiteral("Type")] = QStringLiteral("UnsavedFile");
            details[QStringLiteral("FilePath")] = editor->getFilePath();

            const QString sessionFileName = RandomSessionFileName();
            if (editor->saveCopyAs(generationDirectory.filePath(sessionFileName)) != QFileDevice::NoError) {
                qWarning("Unable to save unsaved editor into the session journal");
                removeGeneration();
                return false;
            }
            details[QStringLiteral("SessionFileName")] = sessionFileName;
        }
        else if (editorType == SessionManager::TempFile) {
            details[QStringLiteral("Type")] = QStringLiteral("Temp");
            details[QStringLiteral("FileName")] = editor->getName();
            details[QStringLiteral("Language")] = editor->languageName;

            const QString sessionFileName = RandomSessionFileName();
            if (editor->saveCopyAs(generationDirectory.filePath(sessionFileName)) != QFileDevice::NoError) {
                qWarning("Unable to save temporary editor into the session journal");
                removeGeneration();
                return false;
            }
            details[QStringLiteral("SessionFileName")] = sessionFileName;
        }
        else {
            qWarning("Unknown SessionFileType %d", editorType);
            removeGeneration();
            return false;
        }

        const QJsonObject viewDetails = editorViewDetails(editor);
        for (auto it = viewDetails.constBegin(); it != viewDetails.constEnd(); ++it) {
            details.insert(it.key(), it.value());
        }

        if (currentEditor == editor) {
            currentEditorIndex = index;
        }

        openedFiles.append(details);
        ++index;
    }

    SessionJournal::Manifest manifest;
    manifest.generation = generation;
    manifest.currentEditorIndex = currentEditorIndex;
    manifest.openedFiles = openedFiles;

    QSaveFile file(sessionManifestPath());
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Unable to open session journal manifest: %s", qPrintable(file.errorString()));
        removeGeneration();
        return false;
    }

    const QByteArray data = SessionJournal::serialize(manifest);
    if (file.write(data) != data.size() || !file.commit()) {
        qWarning("Unable to commit session journal manifest: %s", qPrintable(file.errorString()));
        removeGeneration();
        return false;
    }

    pruneJournalGenerations(generation);
    return true;
}

bool SessionManager::loadJournal(MainWindow *window)
{
    QFile file(sessionManifestPath());
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Unable to open session journal manifest: %s", qPrintable(file.errorString()));
        return false;
    }

    QString parseErrorString;
    const auto parsedManifest = SessionJournal::parse(file.readAll(), &parseErrorString);
    if (!parsedManifest.has_value()) {
        qWarning("Ignoring invalid session journal manifest: %s", qUtf8Printable(parseErrorString));
        return false;
    }

    const QString generation = parsedManifest->generation;
    QDir root = sessionDirectory();
    if (!generation.startsWith(QStringLiteral("generation-")) || QFileInfo(generation).fileName() != generation) {
        qWarning("Ignoring session journal with an unsafe generation name");
        return false;
    }

    const QDir generationDirectory(root.filePath(generation));
    if (!generationDirectory.exists()) {
        qWarning("Ignoring session journal with a missing generation");
        return false;
    }

    const QJsonArray openedFiles = parsedManifest->openedFiles;
    const int currentEditorIndex = parsedManifest->currentEditorIndex;
    ScintillaNext *currentEditor = Q_NULLPTR;

    for (int index = 0; index < openedFiles.size(); ++index) {
        const QJsonObject details = openedFiles.at(index).toObject();
        const QString type = details.value(QStringLiteral("Type")).toString();
        ScintillaNext *editor = Q_NULLPTR;

        if (type == QStringLiteral("File")) {
            const QString filePath = details.value(QStringLiteral("FilePath")).toString();
            if (!QFileInfo::exists(filePath)) {
                qDebug("Session file no longer exists: %s", qUtf8Printable(filePath));
            }
            else if (app->getEditorManager()->getEditorByFilePath(filePath) != Q_NULLPTR) {
                qDebug("Session file is already open, ignoring: %s", qUtf8Printable(filePath));
            }
            else {
                editor = ScintillaNext::fromFile(filePath);
            }
        }
        else if (type == QStringLiteral("UnsavedFile") || type == QStringLiteral("Temp")) {
            const QString sessionFileName = details.value(QStringLiteral("SessionFileName")).toString();
            if (!isJournalPathSafe(generationDirectory, sessionFileName)) {
                qWarning("Ignoring session journal entry with an unsafe buffer path");
                continue;
            }

            const QString sessionFilePath = generationDirectory.filePath(sessionFileName);
            if (!QFileInfo::exists(sessionFilePath)) {
                qWarning("Ignoring session journal entry with a missing buffer");
                continue;
            }

            editor = ScintillaNext::fromFile(sessionFilePath, false);
            if (editor == Q_NULLPTR) {
                continue;
            }

            if (type == QStringLiteral("UnsavedFile")) {
                const QString filePath = details.value(QStringLiteral("FilePath")).toString();
                if (!filePath.isEmpty() && app->getEditorManager()->getEditorByFilePath(filePath) != Q_NULLPTR) {
                    delete editor;
                    continue;
                }

                if (QFileInfo::exists(filePath)) {
                    editor->setFileInfo(filePath);
                }
                else {
                    QString recoveredName = QFileInfo(filePath).fileName();
                    if (recoveredName.isEmpty()) {
                        recoveredName = QStringLiteral("Recovered");
                    }
                    editor->detachFileInfo(recoveredName);
                }
            }
            else {
                editor->detachFileInfo(details.value(QStringLiteral("FileName")).toString());
            }

            editor->setTemporary(true);
        }
        else {
            qWarning("Ignoring unknown session journal entry type: %s", qUtf8Printable(type));
            continue;
        }

        if (editor == Q_NULLPTR) {
            continue;
        }

        app->getEditorManager()->manageEditor(editor);
        loadJournalEditorViewDetails(editor, details);

        if (type == QStringLiteral("Temp")) {
            const QString languageName = details.value(QStringLiteral("Language")).toString();
            if (!languageName.isEmpty()) {
                app->setEditorLanguage(editor, languageName);
            }
        }

        if (index == currentEditorIndex) {
            currentEditor = editor;
        }
    }

    if (currentEditor != Q_NULLPTR) {
        window->switchToEditor(currentEditor);
    }

    return true;
}

void SessionManager::pruneJournalGenerations(const QString &activeGeneration) const
{
    QDir root = sessionDirectory();

    for (const QString &directoryName : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (directoryName.startsWith(QStringLiteral("generation-")) && directoryName != activeGeneration) {
            QDir(root.filePath(directoryName)).removeRecursively();
        }
    }

    for (const QString &fileName : root.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (fileName != SessionJournal::manifestFileName()) {
            root.remove(fileName);
        }
    }
}

bool SessionManager::isJournalPathSafe(const QDir &generationDirectory, const QString &fileName) const
{
    return SessionJournal::isSafeBufferName(generationDirectory, fileName);
}

void SessionManager::saveSession(MainWindow *window)
{
    qInfo(Q_FUNC_INFO);

    // Early out if no flags are set
    if (fileTypes == SessionManager::None) {
        clear();
        return;
    }

    if (!saveJournal(window)) {
        qWarning("Unable to commit the session journal; retaining the previous recovery point");
        return;
    }

    // The journal is now durable. Remove only the legacy settings after the
    // atomic manifest commit so a crash during this cleanup still restores the
    // newest complete session.
    clearSettings();
    return;

}

void SessionManager::loadSession(MainWindow *window)
{
    qInfo(Q_FUNC_INFO);

    if (loadJournal(window)) {
        return;
    }

    ApplicationSettings settings;;

    settings.beginGroup("CurrentSession");

    ScintillaNext *currentEditor = Q_NULLPTR;
    const int currentEditorIndex = settings.value("CurrentEditorIndex").toInt();
    const int size = settings.beginReadArray("OpenedFiles");

    // NOTE: In theory the fileTypes should determine what is loaded, however if the session fileTypes
    // change from the last time it was saved then it means the settings were manually altered outside of the app,
    // which is non-standard behavior, so just load anything in the file

    for (int index = 0; index < size; ++index) {
        settings.setArrayIndex(index);

        ScintillaNext *editor = Q_NULLPTR;

        if (settings.contains("Type")) {
            const QString type = settings.value("Type").toString();

            if (type == QStringLiteral("File")) {
                editor = loadFileDetails(settings);
            }
            else if (type == QStringLiteral("UnsavedFile")) {
                editor = loadUnsavedFileDetails(settings);
            }
            else if (type == QStringLiteral("Temp")) {
                editor = loadTempFile(settings);
            }
            else {
                qDebug("Unknown session entry type: %s", qUtf8Printable(type));
            }

            if (editor) {
                 if (currentEditorIndex == index) {
                    currentEditor = editor;
                }
            }
        }
        else {
            qDebug("Unknown session entry type for index %d", index);
        }
    }

    settings.endArray();

    settings.endGroup();

    if (currentEditor) {
        window->switchToEditor(currentEditor);
    }
}

bool SessionManager::willFileGetStoredInSession(ScintillaNext *editor) const
{
    SessionFileType editorType = determineType(editor);

    // See if the editor type is in the currently supported file types
    return editorType != SessionManager::None && fileTypes.testFlag(editorType);
}

void SessionManager::storeFileDetails(ScintillaNext *editor, QSettings &settings)
{
    settings.setValue("Type", "File");
    settings.setValue("FilePath", editor->getFilePath());

    storeEditorViewDetails(editor, settings);
}

ScintillaNext* SessionManager::loadFileDetails(QSettings &settings)
{
    qInfo(Q_FUNC_INFO);

    const QString filePath = settings.value("FilePath").toString();

    qDebug("Session file: \"%s\"", qUtf8Printable(filePath));

    ScintillaNext *editor = app->getEditorManager()->getEditorByFilePath(filePath);
    if (editor != Q_NULLPTR) {
        qDebug("  file is already open, ignoring");
        return Q_NULLPTR;
    }

    if (QFileInfo::exists(filePath)) {
        editor = ScintillaNext::fromFile(filePath);

        app->getEditorManager()->manageEditor(editor);

        loadEditorViewDetails(editor, settings);

        return editor;
    }
    else {
        qDebug("  no longer exists on disk, ignoring");
        return Q_NULLPTR;
    }
}

void SessionManager::storeUnsavedFileDetails(ScintillaNext *editor, QSettings &settings)
{
    const QString sessionFileName = RandomSessionFileName();

    settings.setValue("Type", "UnsavedFile");
    settings.setValue("FilePath", editor->getFilePath());
    settings.setValue("SessionFileName", sessionFileName);

    storeEditorViewDetails(editor, settings);

    saveIntoSessionDirectory(editor, sessionFileName);
}

ScintillaNext *SessionManager::loadUnsavedFileDetails(QSettings &settings)
{
    qInfo(Q_FUNC_INFO);

    const QString filePath = settings.value("FilePath").toString();
    const QString sessionFileName = settings.value("SessionFileName").toString();
    const QString sessionFilePath = sessionDirectory().filePath(sessionFileName);

    qDebug("Session file: \"%s\"", qUtf8Printable(filePath));
    qDebug("  temp loc: \"%s\"", qUtf8Printable(sessionFilePath));

    ScintillaNext *editor = app->getEditorManager()->getEditorByFilePath(filePath);
    if (editor != Q_NULLPTR) {
        qDebug("  file is already open, ignoring");
        return Q_NULLPTR;
    }

    if (QFileInfo::exists(filePath) && QFileInfo::exists(sessionFilePath)) {
        ScintillaNext *editor = ScintillaNext::fromFile(sessionFilePath);

        // Since this editor has different file path info, treat this as a temporary buffer
        editor->setFileInfo(filePath);
        editor->setTemporary(true);

        app->getEditorManager()->manageEditor(editor);

        loadEditorViewDetails(editor, settings);

        return editor;
    }
    else {
        // What if just filePath exists?
        qDebug("  no longer exists on disk, ignoring this file for session loading");
        return Q_NULLPTR;
    }
}

void SessionManager::storeTempFile(ScintillaNext *editor, QSettings &settings)
{
    const QString sessionFileName = RandomSessionFileName();

    settings.setValue("Type", "Temp");
    settings.setValue("FileName", editor->getName());
    settings.setValue("SessionFileName", sessionFileName);
    settings.setValue("Language", editor->languageName);

    storeEditorViewDetails(editor, settings);

    saveIntoSessionDirectory(editor, sessionFileName);
}

ScintillaNext *SessionManager::loadTempFile(QSettings &settings)
{
    qInfo(Q_FUNC_INFO);

    const QString fileName = settings.value("FileName").toString();
    const QString sessionFileName = settings.value("SessionFileName").toString();
    const QString languageName = settings.value("Language", QString()).toString();
    const QString fullFilePath = sessionDirectory().filePath(sessionFileName);

    qDebug("Session temp file: \"%s\"", qUtf8Printable(fullFilePath));

    if (QFileInfo::exists(fullFilePath)) {
        ScintillaNext *editor = ScintillaNext::fromFile(fullFilePath, false);

        editor->detachFileInfo(fileName);
        editor->setTemporary(true);

        app->getEditorManager()->manageEditor(editor);

        loadEditorViewDetails(editor, settings);

        if (!languageName.isEmpty()) {
            qDebug("Setting session file language to \"%s\"", qUtf8Printable(languageName));
            app->setEditorLanguage(editor, languageName);
        }

        return editor;
    }
    else {
        qDebug("  no longer exists on disk, ignoring");
        return Q_NULLPTR;
    }
}

void SessionManager::storeEditorViewDetails(ScintillaNext *editor, QSettings &settings)
{
    settings.setValue("FirstVisibleLine", static_cast<int>(editor->firstVisibleLine() + 1)); // Keep it 1-based in the settings just for human-readability
    settings.setValue("CurrentPosition", static_cast<int>(editor->currentPos()));

    BookMarkDecorator *decorator = editor->findChild<BookMarkDecorator*>(QString(), Qt::FindDirectChildrenOnly);
    QList<int> bookMarkedLines = decorator->bookMarkedLines();
    if (bookMarkedLines.length() > 0)
        settings.setValue("BookMarks", QListToQVariantList(bookMarkedLines));
}

void SessionManager::loadEditorViewDetails(ScintillaNext *editor, QSettings &settings)
{
    const int firstVisibleLine = settings.value("FirstVisibleLine").toInt() - 1;
    const int currentPosition = settings.value("CurrentPosition").toInt();

    editor->setFirstVisibleLine(firstVisibleLine);
    editor->setEmptySelection(currentPosition);

    if (settings.contains("BookMarks"))
    {
        QList<int> bookMarkedLines = QVariantListToQList(settings.value("BookMarks").toList()); // just using .value<QList<int>>() does not work...possibly a Qt bug?

        BookMarkDecorator *decorator = editor->findChild<BookMarkDecorator*>(QString(), Qt::FindDirectChildrenOnly);
        decorator->setBookMarkedLines(bookMarkedLines);
    }
}
