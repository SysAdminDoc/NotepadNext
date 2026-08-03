/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "QRegexSearch.h"

#include <QtTest>

using namespace Scintilla;
using namespace Scintilla::Internal;

class QRegexSearchTests final : public QObject
{
    Q_OBJECT

private slots:
    void forwardSearchHonorsWordBoundaries();
    void backwardSearchReturnsLastMatch();
    void caseSensitivityUsesDocumentContract();
    void dotMatchesNewlineFlagIsHonored();
    void posixCharacterClassesRemainSupported();
    void utf8PositionsRemainScintillaByteOffsets();
    void patternLengthIsExplicit();
    void replacementSupportsBoostStyleReferences();
    void replacementSupportsEscapedCharacters();
    void invalidPatternsClearMatchLength();
    void replacementExpandsCaptureGroups();
};

namespace
{
void setDocumentText(Document &document, const QByteArray &text)
{
    document.InsertString(0, text.constData(), text.size());
}
}

void QRegexSearchTests::forwardSearchHonorsWordBoundaries()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "food foo");
    QRegexSearch search;
    Sci::Position length = 3;

    const Sci::Position position = search.FindText(&document, 0, document.Length(), "foo",
                                                    true, true, false, FindOption::RegExp, &length);

    QCOMPARE(position, Sci::Position(5));
    QCOMPARE(length, Sci::Position(3));
}

void QRegexSearchTests::backwardSearchReturnsLastMatch()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "foo food foo");
    QRegexSearch search;
    Sci::Position length = 3;

    const Sci::Position position = search.FindText(&document, document.Length(), 0, "foo",
                                                    true, false, false, FindOption::RegExp, &length);

    QCOMPARE(position, Sci::Position(9));
    QCOMPARE(length, Sci::Position(3));
}

void QRegexSearchTests::caseSensitivityUsesDocumentContract()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "Foo foo");
    QRegexSearch search;
    Sci::Position length = 3;

    QCOMPARE(search.FindText(&document, 0, document.Length(), "foo", true, false, false,
                             FindOption::RegExp, &length), Sci::Position(4));

    QCOMPARE(search.FindText(&document, 0, document.Length(), "foo", false, false, false,
                             FindOption::RegExp, &length), Sci::Position(0));
}

void QRegexSearchTests::dotMatchesNewlineFlagIsHonored()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "a\nb");
    QRegexSearch search;
    Sci::Position length = 3;

    QCOMPARE(search.FindText(&document, 0, document.Length(), "a.b", true, false, false,
                             FindOption::RegExp, &length), Sci::Position(-1));

    length = 3;
    const Sci::Position position = search.FindText(&document, 0, document.Length(), "a.b",
                                                    true, false, false,
                                                    FindOption::RegExp | FindOption::Cxx11RegEx,
                                                    &length);
    QCOMPARE(position, Sci::Position(0));
    QCOMPARE(length, Sci::Position(3));
}

void QRegexSearchTests::posixCharacterClassesRemainSupported()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "id=42");
    QRegexSearch search;
    const QByteArray pattern = QByteArrayLiteral("[[:digit:]]+");
    Sci::Position length = pattern.size();

    const Sci::Position position = search.FindText(&document, 0, document.Length(), pattern.constData(),
                                                    true, false, false,
                                                    FindOption::RegExp | FindOption::Posix,
                                                    &length);

    QCOMPARE(position, Sci::Position(3));
    QCOMPARE(length, Sci::Position(2));
}

void QRegexSearchTests::utf8PositionsRemainScintillaByteOffsets()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "\xC3\xA9 foo");
    QRegexSearch search;
    Sci::Position length = 3;

    const Sci::Position position = search.FindText(&document, 0, document.Length(), "foo",
                                                    true, false, false, FindOption::RegExp,
                                                    &length);

    QCOMPARE(position, Sci::Position(3));
    QCOMPARE(length, Sci::Position(3));
}

void QRegexSearchTests::patternLengthIsExplicit()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "prefix foo suffix");
    QRegexSearch search;
    Sci::Position length = 3;
    const QByteArray pattern("foo\0ignored", 11);

    const Sci::Position position = search.FindText(&document, 0, document.Length(), pattern.constData(),
                                                    true, false, false, FindOption::RegExp, &length);

    QCOMPARE(position, Sci::Position(7));
    QCOMPARE(length, Sci::Position(3));
}

void QRegexSearchTests::replacementSupportsBoostStyleReferences()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "foo-bar");
    QRegexSearch search;
    const QByteArray pattern = QByteArrayLiteral("(?<left>foo)-(bar)");
    Sci::Position length = pattern.size();

    QCOMPARE(search.FindText(&document, 0, document.Length(), pattern.constData(), true, false, false,
                             FindOption::RegExp, &length), Sci::Position(0));

    const QByteArray replacement = QByteArrayLiteral("${left}/$2/$0/$MATCH/\\1");
    length = replacement.size();
    const char *result = search.SubstituteByPosition(&document, replacement.constData(), &length);

    QCOMPARE(QByteArray(result, length), QByteArray("foo/bar/foo-bar/foo-bar/foo"));
}

void QRegexSearchTests::replacementSupportsEscapedCharacters()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "foo");
    QRegexSearch search;
    const QByteArray pattern = QByteArrayLiteral("(foo)");
    Sci::Position length = pattern.size();

    QCOMPARE(search.FindText(&document, 0, document.Length(), pattern.constData(), true, false, false,
                             FindOption::RegExp, &length), Sci::Position(0));

    const QByteArray replacement = QByteArrayLiteral("\\1\\n\\t\\\\");
    length = replacement.size();
    const char *result = search.SubstituteByPosition(&document, replacement.constData(), &length);

    QCOMPARE(QByteArray(result, length), QByteArray("foo\n\t\\"));
}

void QRegexSearchTests::invalidPatternsClearMatchLength()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "foo");
    QRegexSearch search;
    Sci::Position length = 4;

    QCOMPARE(search.FindText(&document, 0, document.Length(), "(", true, false, false,
                             FindOption::RegExp, &length), Sci::Position(-1));
    QCOMPARE(length, Sci::Position(0));
}

void QRegexSearchTests::replacementExpandsCaptureGroups()
{
    Document document(DocumentOption::Default);
    setDocumentText(document, "foo");
    QRegexSearch search;
    const QByteArray pattern = QByteArrayLiteral("(foo)");
    Sci::Position length = pattern.size();

    QCOMPARE(search.FindText(&document, 0, document.Length(), pattern.constData(), true, false, false,
                             FindOption::RegExp, &length), Sci::Position(0));

    const QByteArray replacement = "\\1!";
    const char *result = search.SubstituteByPosition(&document, replacement.constData(), &length);

    QCOMPARE(QByteArray(result, length), QByteArray("foo!"));
}

QTEST_APPLESS_MAIN(QRegexSearchTests)
#include "QRegexSearchTests.moc"
