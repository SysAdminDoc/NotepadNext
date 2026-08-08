/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
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


#include "ScintillaNext.h"
#include "AtomicFileWriter.h"
#include "Finder.h"
#include "ScintillaCommenter.h"

#include "ByteArrayUtils.h"
#include "uchardet.h"
#include <cinttypes>

#include <QDir>
#include <QApplication>
#include <QCryptographicHash>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QTextCodec>

#include <limits>
#include <memory>

const int CHUNK_SIZE = 1024 * 1024 * 4; // Not sure what is best

inline const QByteArray BOM_UTF8    = QByteArray::fromHex("EFBBBF");
inline const QByteArray BOM_UTF16LE = QByteArray::fromHex("FFFE");
inline const QByteArray BOM_UTF16BE = QByteArray::fromHex("FEFF");
inline const QByteArray BOM_UTF32LE = QByteArray::fromHex("FFFE0000");
inline const QByteArray BOM_UTF32BE = QByteArray::fromHex("0000FEFF");

ScintillaNext::BomType detectBom(const QByteArray& data)
{
    if (data.startsWith(BOM_UTF32LE)) return ScintillaNext::BomType::Utf32LE;
    if (data.startsWith(BOM_UTF32BE)) return ScintillaNext::BomType::Utf32BE;
    if (data.startsWith(BOM_UTF8))    return ScintillaNext::BomType::Utf8;
    if (data.startsWith(BOM_UTF16LE)) return ScintillaNext::BomType::Utf16LE;
    if (data.startsWith(BOM_UTF16BE)) return ScintillaNext::BomType::Utf16BE;

    return ScintillaNext::BomType::None;
}

QByteArray bomData(ScintillaNext::BomType bom)
{
    switch (bom) {
    case ScintillaNext::BomType::Utf8:    return BOM_UTF8;
    case ScintillaNext::BomType::Utf16LE: return BOM_UTF16LE;
    case ScintillaNext::BomType::Utf16BE: return BOM_UTF16BE;
    case ScintillaNext::BomType::Utf32LE: return BOM_UTF32LE;
    case ScintillaNext::BomType::Utf32BE: return BOM_UTF32BE;
    case ScintillaNext::BomType::None:    return QByteArray();
    }
    return QByteArray();
}

int bomLength(ScintillaNext::BomType bom)
{
    if (bom == ScintillaNext::BomType::Utf8) return BOM_UTF8.length();
    else if (bom == ScintillaNext::BomType::Utf16LE) return BOM_UTF16LE.length();
    else if (bom == ScintillaNext::BomType::Utf16BE) return BOM_UTF16BE.length();
    else if (bom == ScintillaNext::BomType::Utf32LE) return BOM_UTF32LE.length();
    else if (bom == ScintillaNext::BomType::Utf32BE) return BOM_UTF32BE.length();

    return 0;
}

static QByteArray sniffEncoding(const QByteArray &data)
{
    const QByteArray fallback = QByteArrayLiteral("UTF-8");
    uchardet_t detector = uchardet_new();
    if (detector == nullptr || data.isEmpty()) {
        if (detector != nullptr) {
            uchardet_delete(detector);
        }
        return fallback;
    }

    if (uchardet_handle_data(detector, data.constData(), static_cast<size_t>(data.size())) != 0) {
        uchardet_delete(detector);
        return fallback;
    }

    uchardet_data_end(detector);
    const QByteArray detected = QByteArray(uchardet_get_charset(detector)).trimmed();
    uchardet_delete(detector);

    const QByteArray normalized = detected.toUpper();
    if (detected.isEmpty() || normalized == QByteArrayLiteral("UNKNOWN") || normalized == QByteArrayLiteral("US-ASCII")) {
        return fallback;
    }

    if (QTextCodec::codecForName(detected) == nullptr) {
        qWarning("uchardet reported unsupported encoding %s; falling back to UTF-8", detected.constData());
        return fallback;
    }

    return detected;
}

static QFileDevice::FileError writeToDisk(const QByteArray &data, const QString &path, ScintillaNext::BomType &bom, QByteArray &encodingName)
{
    qInfo(Q_FUNC_INFO);

    QTextCodec *codec = QTextCodec::codecForName(encodingName);
    if (codec == nullptr) {
        codec = QTextCodec::codecForName("UTF-8");
        encodingName = codec->name();
    }

    const QString unicodeData = QString::fromUtf8(data);
    if (!codec->canEncode(unicodeData)) {
        qWarning("Encoding %s cannot represent the edited document; saving as UTF-8", encodingName.constData());
        codec = QTextCodec::codecForName("UTF-8");
        encodingName = codec->name();
        bom = ScintillaNext::BomType::None;
    }

    const std::unique_ptr<QTextEncoder> encoder(codec->makeEncoder(QTextCodec::ConversionFlags{}));
    const QByteArray encodedData = encoder->fromUnicode(unicodeData);
    if (encoder->hasFailure()) {
        qWarning("Failed to encode document as %s", encodingName.constData());
        return QFileDevice::WriteError;
    }

    const QByteArray bomBytes = bomData(bom);
    const AtomicFileWriter::Result result = AtomicFileWriter::write(
        path,
        [&bomBytes, &encodedData](QIODevice *device, QString *errorString) {
            if (!bomBytes.isEmpty() && device->write(bomBytes) != bomBytes.size()) {
                if (errorString) {
                    *errorString = QStringLiteral("The file BOM could not be written completely.");
                }
                return false;
            }

            if (device->write(encodedData) != encodedData.size()) {
                if (errorString) {
                    *errorString = QStringLiteral("The document contents could not be written completely.");
                }
                return false;
            }

            return true;
        });

    if (!result.succeeded()) {
        qWarning("writeToDisk() failed for %s: %s", qPrintable(path), qPrintable(result.errorString));
        return result.error;
    }

    return QFileDevice::NoError;
}

static bool isNewlineCharacter(char c)
{
    return c == '\n' || c == '\r';
}

ScintillaNext::ScintillaNext(QString name, QWidget *parent) :
    ScintillaEdit(parent),
    name(name),
    indicatorResources(INDICATOR_MAX + 1)
{
    // Per the scintilla documentation, some parts of the range are not generally available
    indicatorResources.disableRange(0, 7);
    indicatorResources.disableRange(INDICATOR_IME, INDICATOR_IME_MAX);
    indicatorResources.disableRange(INDICATOR_HISTORY_REVERTED_TO_ORIGIN_INSERTION, INDICATOR_HISTORY_REVERTED_TO_MODIFIED_DELETION);
}

ScintillaNext::~ScintillaNext()
{
}

void ScintillaNext::mousePressEvent(QMouseEvent *event)
{
    const bool altOnly = (event->modifiers() & Qt::AltModifier) &&
                         !(event->modifiers() & Qt::ControlModifier);
    if (event->button() == Qt::LeftButton && altOnly) {
        altClickPending = true;
        altClickDragging = false;
        altClickStart = event->position().toPoint();
        return;
    }

    ScintillaEdit::mousePressEvent(event);
}

void ScintillaNext::mouseMoveEvent(QMouseEvent *event)
{
    if (altClickPending && (event->buttons() & Qt::LeftButton)) {
        const QPoint currentPosition = event->position().toPoint();
        const int distance = (currentPosition - altClickStart).manhattanLength();
        if (distance >= QApplication::startDragDistance()) {
            QMouseEvent pressEvent(QEvent::MouseButtonPress,
                                   QPointF(altClickStart),
                                   Qt::LeftButton,
                                   Qt::LeftButton,
                                   event->modifiers());
            altClickPending = false;
            altClickDragging = true;
            ScintillaEdit::mousePressEvent(&pressEvent);
            ScintillaEdit::mouseMoveEvent(event);
            return;
        }
    }

    if (altClickDragging) {
        ScintillaEdit::mouseMoveEvent(event);
        return;
    }

    if (!altClickPending) {
        ScintillaEdit::mouseMoveEvent(event);
    }
}

void ScintillaNext::mouseReleaseEvent(QMouseEvent *event)
{
    if (altClickPending && event->button() == Qt::LeftButton) {
        const int position = static_cast<int>(positionFromPoint(altClickStart.x(), altClickStart.y()));
        if (position >= 0) {
            addSelection(position, position);
            setFocus(Qt::MouseFocusReason);
        }
        altClickPending = false;
        return;
    }

    if (altClickDragging && event->button() == Qt::LeftButton) {
        altClickDragging = false;
        ScintillaEdit::mouseReleaseEvent(event);
        return;
    }

    ScintillaEdit::mouseReleaseEvent(event);
}

ScintillaNext *ScintillaNext::fromFile(const QString &filePath, bool tryToCreate)
{
    QFile file(filePath);
    ScintillaNext *editor = new ScintillaNext(file.fileName());

    if(tryToCreate && !file.exists()) {
        qInfo("Trying to create %s", qUtf8Printable(filePath));
        QDir d;
        d.mkpath(QFileInfo(file).path());

        QFile f(filePath);
        f.open(QIODevice::WriteOnly);
        f.close();
    }

    bool readSuccessful = editor->readFromDisk(file);

    if (!readSuccessful) {
        delete editor;
        return Q_NULLPTR;
    }

    editor->setFileInfo(filePath);

    return editor;
}

QString ScintillaNext::eolModeToString(int eolMode)
{
    if (eolMode == SC_EOL_CRLF)
        return QStringLiteral("crlf");
    else if (eolMode == SC_EOL_CR)
        return QStringLiteral("cr");
    else if (eolMode == SC_EOL_LF)
        return QStringLiteral("lf");
    else
        return QString(); // unknown
}

int ScintillaNext::stringToEolMode(QString eolMode)
{
    if (eolMode == QStringLiteral("crlf"))
        return SC_EOL_CRLF;
    else if (eolMode == QStringLiteral("cr"))
        return SC_EOL_CR;
    else if (eolMode == QStringLiteral("lf"))
        return SC_EOL_LF;
    else
        return -1;
}

int ScintillaNext::allocateIndicator(const QString &name)
{
    return indicatorResources.requestResource(name);
}

void ScintillaNext::goToRange(const Sci_CharacterRange &range)
{
    qInfo(Q_FUNC_INFO);

    if (isRangeValid(range)) {
        // Lines can be folded so make sure they are visible
        ensureVisible(lineFromPosition(range.cpMin));
        ensureVisible(lineFromPosition(range.cpMax));

        setSelection(range.cpMax, range.cpMin);
        scrollRange(range.cpMax, range.cpMin);
    }
}

QByteArray ScintillaNext::eolString() const
{
    const int eol = eOLMode();

    if (eol == SC_EOL_LF) return QByteArrayLiteral("\n");
    else if (eol == SC_EOL_CRLF) return QByteArrayLiteral("\r\n");
    else return QByteArrayLiteral("\r");
}

bool ScintillaNext::lineIsEmpty(int line)
{
    return (lineEndPosition(line) - positionFromLine(line)) == 0;
}

void ScintillaNext::deleteLine(int line)
{
    deleteRange(positionFromLine(line), lineLength(line));
}

void ScintillaNext::cutAllowLine()
{
    if (selectionEmpty()) {
        copyAllowLine();
        lineDelete();
    }
    else {
        cut();
    }
}

void ScintillaNext::modifyFoldLevels(int level, int action)
{
    const int totalLines = lineCount();

    int line = 0;
    while (line < totalLines) {
        int foldFlags = foldLevel(line); // Even though its called fold level it contains several other flags
        bool isHeader = foldFlags & SC_FOLDLEVELHEADERFLAG;
        int actualLevel = (foldFlags & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE;

        if (isHeader && actualLevel == level) {
            foldLine(line, action);
            line = lastChild(line, -1) + 1;
        }
        else {
            ++line;
        }
    }
}

void ScintillaNext::foldAllLevels(int level)
{
    modifyFoldLevels(level, SC_FOLDACTION_CONTRACT);
}

void ScintillaNext::unFoldAllLevels(int level)
{
    modifyFoldLevels(level, SC_FOLDACTION_EXPAND);
}

void ScintillaNext::deleteLeadingEmptyLines()
{
    while (lineCount() > 1 && lineIsEmpty(0)) {
        deleteLine(0);
    }
}

void ScintillaNext::deleteTrailingEmptyLines()
{
    const int docLength = length();
    int position = docLength;

    while (position > 0 && isNewlineCharacter(charAt(position - 1))) {
        position--;
    }

    deleteRange(position, docLength - position);
}

bool ScintillaNext::isSavedToDisk() const
{
    return !canSaveToDisk();
}

bool ScintillaNext::canSaveToDisk() const
{
    // The buffer can be saved if:
    // - It is marked as a temporary since as soon as it gets saved it is no longer a temporary buffer
    // - A modified file
    // - A missing file since as soon as it is saved it is no longer missing.
    return externalChangePending ||
           temporary ||
           (bufferType == ScintillaNext::New && modify()) ||
           (bufferType == ScintillaNext::File && modify()) ||
            (bufferType == ScintillaNext::FileMissing);
}

void ScintillaNext::setName(const QString &name)
{
    this->name = name;

    emit renamed();
}

bool ScintillaNext::isFile() const
{
    return bufferType == ScintillaNext::File || bufferType == ScintillaNext::FileMissing;
}

QFileInfo ScintillaNext::getFileInfo() const
{
    Q_ASSERT(isFile());

    return fileInfo;
}

QString ScintillaNext::getPath() const
{
    Q_ASSERT(isFile());

    return QDir::toNativeSeparators(fileInfo.canonicalPath());
}

QString ScintillaNext::getFilePath() const
{
    Q_ASSERT(isFile());

    return QDir::toNativeSeparators(fileInfo.canonicalFilePath());
}

void ScintillaNext::setFoldMarkers(const QString &type)
{
    QMap<QString, QList<int>> map{
        {"simple", {SC_MARK_MINUS, SC_MARK_PLUS, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY}},
        {"arrow",  {SC_MARK_ARROWDOWN, SC_MARK_ARROW, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY}},
        {"circle", {SC_MARK_CIRCLEMINUS, SC_MARK_CIRCLEPLUS, SC_MARK_VLINE, SC_MARK_LCORNERCURVE, SC_MARK_CIRCLEPLUSCONNECTED, SC_MARK_CIRCLEMINUSCONNECTED, SC_MARK_TCORNERCURVE }},
        {"box",    {SC_MARK_BOXMINUS, SC_MARK_BOXPLUS, SC_MARK_VLINE, SC_MARK_LCORNER, SC_MARK_BOXPLUSCONNECTED, SC_MARK_BOXMINUSCONNECTED, SC_MARK_TCORNER }},
    };

    if (!map.contains(type))
        return;

    const auto types = map[type];
    markerDefine(SC_MARKNUM_FOLDEROPEN, types[0]);
    markerDefine(SC_MARKNUM_FOLDER, types[1]);
    markerDefine(SC_MARKNUM_FOLDERSUB, types[2]);
    markerDefine(SC_MARKNUM_FOLDERTAIL, types[3]);
    markerDefine(SC_MARKNUM_FOLDEREND, types[4]);
    markerDefine(SC_MARKNUM_FOLDEROPENMID, types[5]);
    markerDefine(SC_MARKNUM_FOLDERMIDTAIL, types[6]);
}

void ScintillaNext::close()
{
    emit closed();

    deleteLater();
}

QFileDevice::FileError ScintillaNext::save(bool allowExternalChange)
{
    qInfo(Q_FUNC_INFO);

    Q_ASSERT(isFile());

    lastSaveConflict = false;
    lastFileErrorMessage.clear();

    if (!allowExternalChange && bufferType == BufferType::File
        && (externalChangePending || !diskMatchesSnapshot(true))) {
        externalChangePending = true;
        lastSaveConflict = true;
        lastFileErrorMessage = tr("The file changed outside Notepad Next. Reload it, save a copy, or explicitly overwrite the external change.");
        qWarning("Refusing to overwrite an externally changed file: %s", qUtf8Printable(fileInfo.filePath()));
        return QFileDevice::ResourceError;
    }

    emit aboutToSave();

    const QByteArray data = QByteArray::fromRawData((char*)characterPointer(), textLength());
    const QString path = fileInfo.filePath();
    QFileDevice::FileError writeSuccessful = writeToDisk(data, path, bomType, encodingName);

    if (writeSuccessful == QFileDevice::NoError) {
        updateDiskSnapshot();
        externalChangePending = false;
        setSavePoint();

        // If this was a temporary file, make sure it is not any more
        setTemporary(false);

        emit saved();
    }

    if (writeSuccessful != QFileDevice::NoError) {
        lastFileErrorMessage = tr("Unable to save %1.").arg(path);
    }

    return writeSuccessful;
}

bool ScintillaNext::reload(QString *error)
{
    Q_ASSERT(isFile());

    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (error) {
        error->clear();
    }

    // Ensure the file still exists.
    const QString path = fileInfo.filePath();
    if (!QFileInfo::exists(path)) {
        return fail(tr("The file no longer exists on disk."));
    }

    const int line = firstVisibleLine();
    const int caret = selectionNCaret(mainSelection());
    const int anchor = selectionNAnchor(mainSelection());

    QFile f(path);
    const bool readSuccessful = readFromDisk(f, error);

    if (!readSuccessful) {
        return false;
    }

    updateDiskSnapshot();
    setSavePoint();
    externalChangePending = false;

    // If this was a temporary file, make sure it is not any more
    if (isTemporary())
        setTemporary(false);

    scrollVertical(line, 0);
    setSelection(caret, anchor);

    emit reloaded();
    return true;
}

void ScintillaNext::omitModifications()
{
    // Keep the in-memory buffer, acknowledge the observed disk version, and
    // require an explicit overwrite or Save As before replacing it.
    if (bufferType == BufferType::File) {
        updateDiskSnapshot();
        externalChangePending = true;
    }
    setTemporary(true);
}

QFileDevice::FileError ScintillaNext::saveAs(const QString &newFilePath)
{
    lastSaveConflict = false;
    lastFileErrorMessage.clear();

    const QString currentPath = isFile() ? QFileInfo(fileInfo.filePath()).absoluteFilePath() : QString();
    const QString targetPath = QFileInfo(newFilePath).absoluteFilePath();
    const bool samePath = !currentPath.isEmpty()
        && currentPath.compare(targetPath, Qt::CaseInsensitive) == 0;
    if (samePath && bufferType == BufferType::File
        && (externalChangePending || !diskMatchesSnapshot(true))) {
        externalChangePending = true;
        lastSaveConflict = true;
        lastFileErrorMessage = tr("The file changed outside Notepad Next. Choose a different path or explicitly overwrite the external change.");
        return QFileDevice::ResourceError;
    }

    const bool isRenamed = bufferType == ScintillaNext::New || !samePath;

    emit aboutToSave();

    const QByteArray data = QByteArray::fromRawData((char*)characterPointer(), textLength());
    QFileDevice::FileError saveSuccessful = writeToDisk(data, newFilePath, bomType, encodingName);

    if (saveSuccessful == QFileDevice::NoError) {
        setFileInfo(newFilePath);
        setSavePoint();
        externalChangePending = false;

        // If this was a temporary file, make sure it is not any more
        setTemporary(false);

        emit saved();

        if (isRenamed) {
            emit renamed();
        }
    }
    else {
        lastFileErrorMessage = tr("Unable to save %1.").arg(newFilePath);
    }

    return saveSuccessful;
}

QFileDevice::FileError ScintillaNext::saveCopyAs(const QString &filePath)
{
    lastFileErrorMessage.clear();
    const QByteArray data = QByteArray::fromRawData((char*)characterPointer(), textLength());
    const QFileDevice::FileError error = writeToDisk(data, filePath, bomType, encodingName);
    if (error != QFileDevice::NoError) {
        lastFileErrorMessage = tr("Unable to save %1.").arg(filePath);
    }
    return error;
}

bool ScintillaNext::rename(const QString &newFilePath)
{
    lastSaveConflict = false;
    lastFileErrorMessage.clear();

    if (bufferType == BufferType::File
        && (externalChangePending || !diskMatchesSnapshot(true))) {
        externalChangePending = true;
        lastSaveConflict = true;
        lastFileErrorMessage = tr("The file changed outside Notepad Next. Reload it or save the buffer to a different path before renaming.");
        return false;
    }

    emit aboutToSave();

    // Write out the buffer to the new path
    if (saveCopyAs(newFilePath) == QFileDevice::NoError) {
        // Remove the old file
        const QString oldPath = fileInfo.canonicalFilePath();
        QFile::remove(oldPath);

        // Everything worked fine, so update the buffer's info
        setFileInfo(newFilePath);
        setSavePoint();
        externalChangePending = false;

        // If this was a temporary file, make sure it is not any more
        setTemporary(false);

        emit saved();

        emit renamed();

        return true;
    }

    lastFileErrorMessage = tr("Unable to rename the file to %1.").arg(newFilePath);
    return false;
}

ScintillaNext::FileStateChange ScintillaNext::checkFileForStateChange()
{
    if (bufferType == BufferType::New) {
        return FileStateChange::NoChange;
    }
    else if (bufferType == BufferType::File) {
        // refresh else exists() fails to notice missing file
        fileInfo.refresh();

        if (!fileInfo.exists()) {
            bufferType = BufferType::FileMissing;

            emit savePointChanged(false);

            return FileStateChange::Deleted;
        }

        if (externalChangePending) {
            return FileStateChange::Conflict;
        }

        // See if the timestamp or size changed
        if (modifiedTime != fileTimestamp() || fileSize != fileInfo.size()) {
            return modify() ? FileStateChange::Conflict : FileStateChange::Modified;
        }
        else {
            return FileStateChange::NoChange;
        }
    }
    else if (bufferType == BufferType::FileMissing) {
        // See if it reappeared
        fileInfo.refresh();

        if (fileInfo.exists()) {
            bufferType = BufferType::File;
            externalChangePending = true;

            return modify() ? FileStateChange::Conflict : FileStateChange::Restored;
        }
        else {
            return FileStateChange::NoChange;
        }
    }

    qInfo("type() = %d", bufferType);
    Q_ASSERT(false);

    return FileStateChange::NoChange;
}

bool ScintillaNext::moveToTrash()
{
    if (QFile::exists(fileInfo.canonicalFilePath())) {
        QFile f(fileInfo.canonicalFilePath());

        return f.moveToTrash();
    }

    return false;
}

void ScintillaNext::toggleCommentSelection()
{
    ScintillaCommenter sc(this);
    sc.toggleSelection();
}

void ScintillaNext::commentLineSelection()
{
    ScintillaCommenter sc(this);
    sc.commentSelection();
}

void ScintillaNext::uncommentLineSelection()
{
    ScintillaCommenter sc(this);
    sc.uncommentSelection();
}

void ScintillaNext::removeDuplicateLines()
{
    QByteArray data = QByteArray::fromRawData((char*) characterPointer(), textLength());
    const QByteArray delim = eolString();

    auto lines = ByteArrayUtils::split(data, delim);
    int originalLineCount = lines.length();
    ByteArrayUtils::removeDuplicates(lines);

    if (originalLineCount == lines.length()){
        return; // No lines were removed
    }

    QByteArray result = ByteArrayUtils::join(lines, delim);

    const UndoAction ua(this);
    setTargetRange(0, textLength());
    replaceTarget(result.length(), result.constData());
}

void ScintillaNext::removeConsecutiveDuplicateLines()
{
    QByteArray data = QByteArray::fromRawData((char*) characterPointer(), textLength());
    const QByteArray delim = eolString();

    auto lines = ByteArrayUtils::split(data, delim);
    int originalLineCount = lines.length();
    ByteArrayUtils::removeConsecutiveDuplicates(lines);
    QByteArray result = ByteArrayUtils::join(lines, delim);

    if (originalLineCount == lines.length()){
        return; // No lines were removed
    }

    const UndoAction ua(this);
    setTargetRange(0, textLength());
    replaceTarget(result.length(), result.constData());
}

void ScintillaNext::dragEnterEvent(QDragEnterEvent *event)
{
    // Ignore all drag and drop events with urls and let the main application handle it
    if (event->mimeData()->hasUrls()) {
        return;
    }

    ScintillaEdit::dragEnterEvent(event);
}

void ScintillaNext::dropEvent(QDropEvent *event)
{
    // Ignore all drag and drop events with urls and let the main application handle it
    if (event->mimeData()->hasUrls()) {
        return;
    }

    ScintillaEdit::dropEvent(event);
}

bool ScintillaNext::readFromDisk(QFile &file, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (error) {
        error->clear();
    }

    if (!file.exists()) {
        const QString message = tr("The file does not exist.");
        qWarning("Cannot read \"%s\": doesn't exist", qUtf8Printable(file.fileName()));
        return fail(message);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        const QString message = tr("Unable to open the file: %1").arg(file.errorString());
        qWarning("QFile::open() failed when opening \"%s\" - error code %d: %s", qUtf8Printable(file.fileName()), file.error(), qUtf8Printable(file.errorString()));
        return fail(message);
    }

    const qint64 sourceSize = file.size();
    if (sourceSize > std::numeric_limits<int>::max()) {
        const QString message = tr("The file is too large to load into the editor.");
        file.close();
        return fail(message);
    }

    QByteArray encodedData;
    if (sourceSize > 0) {
        encodedData.reserve(static_cast<int>(sourceSize));
    }

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(CHUNK_SIZE);
        if (chunk.isEmpty()) {
            if (file.error() != QFileDevice::NoError) {
                const QString message = tr("Unable to read the file: %1").arg(file.errorString());
                qWarning("Something bad happened when reading disk %d %s", file.error(), qUtf8Printable(file.errorString()));
                file.close();
                return fail(message);
            }
            break;
        }
        encodedData.append(chunk);
    }

    if (file.error() != QFileDevice::NoError) {
        const QString message = tr("Unable to read the file: %1").arg(file.errorString());
        file.close();
        return fail(message);
    }

    const QFileInfo sourceInfo(file);
    const bool writable = sourceInfo.isWritable();
    file.close();

    const BomType loadedBom = detectBom(encodedData);
    if (loadedBom != BomType::None) {
        qDebug("BOM found");
    }

    QByteArray loadedEncoding;
    QTextCodec *codec = nullptr;
    if (loadedBom == BomType::Utf8) {
        codec = QTextCodec::codecForName("UTF-8");
    }
    else if (loadedBom == BomType::Utf16LE) {
        codec = QTextCodec::codecForName("UTF-16LE");
    }
    else if (loadedBom == BomType::Utf16BE) {
        codec = QTextCodec::codecForName("UTF-16BE");
    }
    else if (loadedBom == BomType::Utf32LE) {
        codec = QTextCodec::codecForName("UTF-32LE");
    }
    else if (loadedBom == BomType::Utf32BE) {
        codec = QTextCodec::codecForName("UTF-32BE");
    }
    else {
        loadedEncoding = sniffEncoding(encodedData.left(CHUNK_SIZE));
        codec = QTextCodec::codecForName(loadedEncoding);
    }

    BomType effectiveBom = loadedBom;
    if (codec == nullptr) {
        qWarning("Unable to select a codec; falling back to UTF-8");
        effectiveBom = BomType::None;
        loadedEncoding = QByteArrayLiteral("UTF-8");
        codec = QTextCodec::codecForName(loadedEncoding);
    }
    else if (loadedEncoding.isEmpty()) {
        loadedEncoding = codec->name();
    }

    if (!codec) {
        const QString message = tr("Unable to select a text codec for the file.");
        return fail(message);
    }

    const std::unique_ptr<QTextDecoder> decoder(codec->makeDecoder(QTextCodec::ConversionFlags{}));
    QByteArray decodedData;
    const int bytesToSkip = bomLength(effectiveBom);
    for (int offset = qMin(bytesToSkip, encodedData.size()); offset < encodedData.size();) {
        const int chunkLength = qMin(CHUNK_SIZE, encodedData.size() - offset);
        const QByteArray chunk = encodedData.mid(offset, chunkLength);
        decodedData.append(decoder->toUnicode(chunk).toUtf8());
        offset += chunkLength;
    }

    if (decoder->hasFailure()) {
        qWarning("The %s decoder reported invalid input", loadedEncoding.constData());
    }

    const QByteArray previousData = getText(textLength());
    const BomType previousBom = bomType;
    const QByteArray previousEncoding = encodingName;
    const bool previousReadOnly = readOnly();

    allocate(decodedData.size());
    bool applied = true;
    {
        const QSignalBlocker blocker(this);
        setUndoCollection(false);
        emptyUndoBuffer();
        setText("");
        if (!decodedData.isEmpty()) {
            appendText(static_cast<int>(decodedData.size()), decodedData.constData());
        }
        setUndoCollection(true);
        applied = status() == SC_STATUS_OK;
    }

    if (!applied) {
        qWarning("something bad happened in document->add_data() %ld", status());
        allocate(previousData.size());
        const QSignalBlocker blocker(this);
        setUndoCollection(false);
        emptyUndoBuffer();
        setText("");
        if (!previousData.isEmpty()) {
            appendText(static_cast<int>(previousData.size()), previousData.constData());
        }
        setUndoCollection(true);
        bomType = previousBom;
        encodingName = previousEncoding;
        setReadOnly(previousReadOnly);
        return fail(tr("The editor could not apply the loaded document."));
    }

    bomType = effectiveBom;
    encodingName = loadedEncoding;
    diskFingerprint = QCryptographicHash::hash(encodedData, QCryptographicHash::Sha256);
    setReadOnly(!writable);
    if (!writable) {
        qInfo("Setting file as read-only");
    }

    return true;
}

QDateTime ScintillaNext::fileTimestamp()
{
    Q_ASSERT(bufferType != ScintillaNext::New);

    fileInfo.refresh();
    qInfo("%s last modified %s", qUtf8Printable(fileInfo.fileName()), qUtf8Printable(fileInfo.lastModified().toString()));
    return fileInfo.lastModified();
}

void ScintillaNext::updateTimestamp()
{
    modifiedTime = fileTimestamp();
    fileSize = fileInfo.size();
}

QByteArray ScintillaNext::fingerprintForFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return hash.result();
}

void ScintillaNext::updateDiskSnapshot()
{
    if (!isFile()) {
        return;
    }

    fileInfo.refresh();
    if (!fileInfo.exists()) {
        modifiedTime = {};
        fileSize = -1;
        diskFingerprint.clear();
        return;
    }

    modifiedTime = fileInfo.lastModified();
    fileSize = fileInfo.size();
    diskFingerprint = fingerprintForFile(fileInfo.filePath());
}

bool ScintillaNext::diskMatchesSnapshot(bool verifyContent) const
{
    if (bufferType != BufferType::File) {
        return bufferType == BufferType::FileMissing && !QFileInfo::exists(fileInfo.filePath());
    }

    QFileInfo current(fileInfo.filePath());
    current.refresh();
    if (!current.exists()
        || current.lastModified() != modifiedTime
        || current.size() != fileSize) {
        return false;
    }

    if (verifyContent && !diskFingerprint.isEmpty()) {
        return fingerprintForFile(current.filePath()) == diskFingerprint;
    }

    return true;
}

void ScintillaNext::setFileInfo(const QString &filePath)
{
    fileInfo.setFile(filePath);
    fileInfo.makeAbsolute();

    Q_ASSERT(fileInfo.exists());

    name = fileInfo.fileName();
    bufferType = ScintillaNext::File;

    externalChangePending = false;
    lastSaveConflict = false;
    updateDiskSnapshot();
}

void ScintillaNext::detachFileInfo(const QString &newName)
{
    setName(newName);

    bufferType = ScintillaNext::New;
    modifiedTime = {};
    fileSize = -1;
    diskFingerprint.clear();
    externalChangePending = false;
    lastSaveConflict = false;
}

void ScintillaNext::setTemporary(bool temp)
{
    temporary = temp;

    // Fake this signal
    emit savePointChanged(temporary);
}
