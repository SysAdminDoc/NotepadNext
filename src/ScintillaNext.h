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


#ifndef SCINTILLANEXT_H
#define SCINTILLANEXT_H

#include "RangeAllocator.h"
#include "ScintillaEdit.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QPoint>




class ScintillaNext : public ScintillaEdit
{
    Q_OBJECT

public:
    static constexpr qint64 LargeFileThresholdBytes = 50LL * 1024 * 1024;
    static constexpr qint64 MaximumTextFileSizeBytes = 128LL * 1024 * 1024;

    static bool isRangeValid(const Sci_CharacterRange &range)
    {
        return range.cpMin != INVALID_POSITION && range.cpMax != INVALID_POSITION;
    }

    explicit ScintillaNext(QString name, QWidget *parent = Q_NULLPTR);
    virtual ~ScintillaNext();

    static ScintillaNext *fromFile(const QString &filePath, bool tryToCreate=false, QString *error = nullptr);
    static QString eolModeToString(int eolMode);
    static int stringToEolMode(QString eolMode);

    int allocateIndicator(const QString &name);

    template<typename Func>
    void forEachMatch(const QString &text, Func callback) { forEachMatch(text.toUtf8(), callback); }

    template<typename Func>
    void forEachMatch(const QByteArray &byteArray, Func callback) { forEachMatchInRange(byteArray, callback, {0, (Sci_PositionCR)length()}); }

    template<typename Func>
    void forEachMatchInRange(const QByteArray &byteArray, Func callback, Sci_CharacterRange range);

    template<typename Func>
    void forEachLineInSelection(int selection, Func callback);

    void goToRange(const Sci_CharacterRange &range);

    QByteArray eolString() const;

    bool lineIsEmpty(int line);

    void deleteLine(int line);

    void cutAllowLine();

    void modifyFoldLevels(int level, int action);
    void foldAllLevels(int level);
    void unFoldAllLevels(int level);

    void deleteLeadingEmptyLines();
    void deleteTrailingEmptyLines();

    bool isFile() const;
    QFileInfo getFileInfo() const;

    bool isSavedToDisk() const;
    bool canSaveToDisk() const;
    bool isLargeFileMode() const { return largeFileMode; }

    QString getName() const { return name; }
    void setName(const QString &name);
    QString getPath() const;
    QString getFilePath() const;

    // NOTE: this is dangerous and should only be used in very rare situations
    void setFileInfo(const QString &filePath);

    void detachFileInfo(const QString &newName);

    enum FileStateChange {
        NoChange,
        Modified,
        Conflict,
        Deleted,
        Restored,
    };

    enum BufferType {
        New, // A temporary buffer, e.g. "New 1"
        File, // Buffer tied to a file on the file system
        FileMissing, // Buffer with a missing file on the file system
    };

    enum class BomType {
        None,
        Utf8,
        Utf16LE,
        Utf16BE,
        Utf32LE,
        Utf32BE
    };

    BomType bom() const { return bomType; }
    QByteArray encoding() const { return encodingName; }
    bool encodingWasDetected() const { return encodingAutoDetected; }
    bool setEncoding(const QByteArray &name, BomType bom, QString *error = nullptr);

    bool isTemporary() const { return temporary; }
    void setTemporary(bool temp);

    void setFoldMarkers(const QString &type);

    QString languageName;
    QByteArray languageSingleLineComment;

    #include "ScintillaEnums.h"


public slots:
    void close();
    QFileDevice::FileError save(bool allowExternalChange = false);
    bool reload(QString *error = nullptr);
    void omitModifications();
    QFileDevice::FileError saveAs(const QString &newFilePath);
    QFileDevice::FileError saveCopyAs(const QString &filePath);
    bool rename(const QString &newFilePath);
    ScintillaNext::FileStateChange checkFileForStateChange();
    bool moveToTrash();

    bool hasExternalChangePending() const { return externalChangePending; }
    bool lastSaveWasConflict() const { return lastSaveConflict; }
    QString lastFileError() const { return lastFileErrorMessage; }

    void toggleCommentSelection();
    void commentLineSelection();
    void uncommentLineSelection();

    void removeDuplicateLines();
    void removeConsecutiveDuplicateLines();

signals:
    void aboutToSave();
    void saved();
    void closed();
    void renamed();
    void encodingChanged();
    void largeFileModeChanged(bool enabled);

    void lexerChanged();
    void reloaded();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QString name;
    BufferType bufferType = BufferType::New;
    BomType bomType = BomType::None;
    QByteArray encodingName = QByteArrayLiteral("UTF-8");
    bool encodingAutoDetected = false;
    bool encodingDirty = false;
    bool largeFileMode = false;
    QFileInfo fileInfo;
    QDateTime modifiedTime;
    RangeAllocator indicatorResources;

    bool temporary = false; // Temporary file loaded from a session. It can either be a 'New' file or actual 'File'

    bool altClickPending = false;
    bool altClickDragging = false;
    QPoint altClickStart;

    bool readFromDisk(QFile &file, QString *error = nullptr);
    QDateTime fileTimestamp();
    void updateTimestamp();
    void updateDiskSnapshot();
    bool diskMatchesSnapshot(bool verifyContent) const;
    static QByteArray fingerprintForFile(const QString &filePath);

    qint64 fileSize = -1;
    QByteArray diskFingerprint;
    bool externalChangePending = false;
    bool lastSaveConflict = false;
    QString lastFileErrorMessage;

};

template<typename Func>
void ScintillaNext::forEachLineInSelection(int selection, Func callback)
{
    int lineStart = lineFromPosition(selectionNStart(selection));
    int lineEnd = lineFromPosition(selectionNEnd(selection));

    for (int curLine = lineStart; curLine <= lineEnd; ++curLine) {
        callback(curLine);
    }
}

// Stick this in the header file...because C++, that's why
template<typename Func>
void ScintillaNext::forEachMatchInRange(const QByteArray &text, Func callback, Sci_CharacterRange range)
{
    Sci_TextToFind ttf {range, text.constData(), {-1, -1}};
    int flags = searchFlags();

    while (send(SCI_FINDTEXT, flags, reinterpret_cast<sptr_t>(&ttf)) != -1) {
        const Sci_PositionCR previousStart = ttf.chrg.cpMin;
        const Sci_PositionCR matchStart = ttf.chrgText.cpMin;
        const Sci_PositionCR matchEnd = ttf.chrgText.cpMax;
        const Sci_PositionCR requestedStart = callback(matchStart, matchEnd);

        if (requestedStart > previousStart) {
            ttf.chrg.cpMin = requestedStart;
        }
        else if (matchStart == matchEnd) {
            // A zero-length regular-expression match must still advance the
            // search, otherwise callers that return the match end will loop.
            if (matchStart >= range.cpMax) {
                break;
            }

            const Sci_PositionCR nextPosition = positionAfter(matchStart);
            if (nextPosition <= matchStart) {
                break;
            }
            ttf.chrg.cpMin = nextPosition;
        }
        else {
            // Keep a malformed callback from sending the search backwards.
            ttf.chrg.cpMin = matchEnd;
        }

        if (ttf.chrg.cpMin > range.cpMax) {
            break;
        }
    }
}

#endif // SCINTILLANEXT_H
