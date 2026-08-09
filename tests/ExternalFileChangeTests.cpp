/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "ScintillaNext.h"

#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QByteArray bomData(ScintillaNext::BomType bom)
{
    switch (bom) {
    case ScintillaNext::BomType::None: return {};
    case ScintillaNext::BomType::Utf8: return QByteArray::fromHex("EFBBBF");
    case ScintillaNext::BomType::Utf16LE: return QByteArray::fromHex("FFFE");
    case ScintillaNext::BomType::Utf16BE: return QByteArray::fromHex("FEFF");
    case ScintillaNext::BomType::Utf32LE: return QByteArray::fromHex("FFFE0000");
    case ScintillaNext::BomType::Utf32BE: return QByteArray::fromHex("0000FEFF");
    }
    return {};
}

QByteArray encode(QTextCodec *codec, const QString &text)
{
    std::unique_ptr<QTextEncoder> encoder(codec->makeEncoder(QTextCodec::ConversionFlags{}));
    if (!encoder) {
        return {};
    }
    return encoder->fromUnicode(text);
}
}

class ExternalFileChangeTests final : public QObject
{
    Q_OBJECT

private slots:
    void reloadFailurePreservesInMemoryDocument();
    void fileStateTransitionsIncludeConflictAndRestore();
    void saveRefusesExternalChangeUntilExplicitOverride();
    void encodingMatrixRoundTrips();
    void encodingConversionReportsLossBeforeSave();
    void invalidBytesDoNotCrashAndReloadUpdatesEncoding();
};

void ExternalFileChangeTests::reloadFailurePreservesInMemoryDocument()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("on disk")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("on disk"));

    editor->setText("in memory");
    QVERIFY(QFile::remove(path));

    QString error;
    QVERIFY(!editor->reload(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("in memory"));
}

void ExternalFileChangeTests::fileStateTransitionsIncludeConflictAndRestore()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("original")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::NoChange);

    QVERIFY(writeFile(path, QByteArrayLiteral("external")));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Modified);

    editor->omitModifications();
    QVERIFY(editor->hasExternalChangePending());
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Conflict);

    QString error;
    QVERIFY(editor->reload(&error));
    QVERIFY(!editor->hasExternalChangePending());
    QVERIFY(QFile::remove(path));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Deleted);

    QVERIFY(writeFile(path, QByteArrayLiteral("restored")));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Restored);
    QVERIFY(editor->hasExternalChangePending());
}

void ExternalFileChangeTests::saveRefusesExternalChangeUntilExplicitOverride()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("document.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("original")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    editor->setText("mine");
    QVERIFY(writeFile(path, QByteArrayLiteral("theirs")));

    QCOMPARE(editor->save(), QFileDevice::ResourceError);
    QVERIFY(editor->lastSaveWasConflict());
    QCOMPARE(readFile(path), QByteArrayLiteral("theirs"));

    QCOMPARE(editor->save(true), QFileDevice::NoError);
    QVERIFY(!editor->lastSaveWasConflict());
    QCOMPARE(readFile(path), QByteArrayLiteral("mine"));
}

void ExternalFileChangeTests::encodingMatrixRoundTrips()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    struct Fixture {
        QByteArray codecName;
        ScintillaNext::BomType bom;
        QString text;
    };
    const QVector<Fixture> fixtures = {
        {QByteArrayLiteral("UTF-8"), ScintillaNext::BomType::None, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-8"), ScintillaNext::BomType::Utf8, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-16LE"), ScintillaNext::BomType::None, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-16LE"), ScintillaNext::BomType::Utf16LE, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-16BE"), ScintillaNext::BomType::None, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-16BE"), ScintillaNext::BomType::Utf16BE, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-32LE"), ScintillaNext::BomType::None, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-32LE"), ScintillaNext::BomType::Utf32LE, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-32BE"), ScintillaNext::BomType::None, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("UTF-32BE"), ScintillaNext::BomType::Utf32BE, QStringLiteral("Hello € 𝄞")},
        {QByteArrayLiteral("Windows-1252"), ScintillaNext::BomType::None, QStringLiteral("caf\u00e9 €")},
        {QByteArrayLiteral("ISO-8859-1"), ScintillaNext::BomType::None, QStringLiteral("caf\u00e9")},
    };

    int verified = 0;
    for (int index = 0; index < fixtures.size(); ++index) {
        const Fixture &fixture = fixtures.at(index);
        QTextCodec *codec = QTextCodec::codecForName(fixture.codecName);
        if (codec == nullptr || !codec->canEncode(fixture.text)) {
            continue;
        }

        const QString fileName = QStringLiteral("encoding-%1.txt").arg(index);
        const QString path = root.filePath(fileName);
        QVERIFY(writeFile(path, bomData(fixture.bom) + encode(codec, fixture.text)));

        std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->bom(), fixture.bom);
        QCOMPARE(editor->encodingWasDetected(), fixture.bom == ScintillaNext::BomType::None);
        if (fixture.bom != ScintillaNext::BomType::None) {
            QCOMPARE(editor->encoding().toUpper(), codec->name().toUpper());
            QCOMPARE(editor->getText(editor->textLength()), fixture.text.toUtf8());
        }
        ++verified;
    }

    QVERIFY2(verified >= 8, "The available Qt codecs did not cover the Unicode encoding matrix");
}

void ExternalFileChangeTests::encodingConversionReportsLossBeforeSave()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("conversion.txt"));
    QVERIFY(writeFile(path, QByteArrayLiteral("cafe")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);

    QString error;
    QTextCodec *latin = QTextCodec::codecForName("ISO-8859-1");
    if (latin == nullptr) {
        QSKIP("ISO-8859-1 is not available in this Qt build");
    }

    QVERIFY(editor->setEncoding(QByteArrayLiteral("ISO-8859-1"), ScintillaNext::BomType::None, &error));
    QVERIFY(!editor->isSavedToDisk());
    const QByteArray unrepresentableText = QStringLiteral("Euro €").toUtf8();
    editor->setText(unrepresentableText.constData());
    QCOMPARE(editor->save(), QFileDevice::WriteError);
    QVERIFY(editor->lastFileError().contains(QStringLiteral("cannot represent")));
    QCOMPARE(readFile(path), QByteArrayLiteral("cafe"));

    editor.reset(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QVERIFY(editor->setEncoding(QByteArrayLiteral("UTF-16BE"), ScintillaNext::BomType::Utf16BE, &error));
    const QString saveAsPath = root.filePath(QStringLiteral("conversion-utf16.txt"));
    QCOMPARE(editor->saveAs(saveAsPath), QFileDevice::NoError);
    QVERIFY(readFile(saveAsPath).startsWith(QByteArray::fromHex("FEFF")));
    QCOMPARE(editor->encoding(), QByteArrayLiteral("UTF-16BE"));
    QCOMPARE(editor->bom(), ScintillaNext::BomType::Utf16BE);
    QVERIFY(editor->isSavedToDisk());
}

void ExternalFileChangeTests::invalidBytesDoNotCrashAndReloadUpdatesEncoding()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = root.filePath(QStringLiteral("reload.txt"));
    QVERIFY(writeFile(path, QByteArray::fromHex("EFBBBF") + QByteArrayLiteral("initial")));

    std::unique_ptr<ScintillaNext> editor(ScintillaNext::fromFile(path));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->bom(), ScintillaNext::BomType::Utf8);

    QTextCodec *utf16Le = QTextCodec::codecForName("UTF-16LE");
    QVERIFY(utf16Le != nullptr);
    QVERIFY(writeFile(path, QByteArray::fromHex("FFFE") + encode(utf16Le, QStringLiteral("reloaded"))));
    QString error;
    QVERIFY(editor->reload(&error));
    QVERIFY(error.isEmpty());
    QCOMPARE(editor->encoding(), QByteArrayLiteral("UTF-16LE"));
    QCOMPARE(editor->bom(), ScintillaNext::BomType::Utf16LE);
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("reloaded"));

    const QString invalidPath = root.filePath(QStringLiteral("invalid.txt"));
    QVERIFY(writeFile(invalidPath, QByteArray::fromHex("EFBBBF") + QByteArray::fromHex("C328")));
    std::unique_ptr<ScintillaNext> invalid(ScintillaNext::fromFile(invalidPath));
    QVERIFY(invalid != nullptr);
    QVERIFY(invalid->textLength() >= 0);
}

QTEST_MAIN(ExternalFileChangeTests)
#include "ExternalFileChangeTests.moc"
