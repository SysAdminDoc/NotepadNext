/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <tree_sitter/api.h>

#include "TreeSitterJsonLexer.h"

#include <QtTest>

#include <algorithm>
#include <cstring>
#include <utility>

extern "C" const TSLanguage *tree_sitter_json();

class TreeSitterJsonTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesValidDocument();
    void reportsSyntaxErrors();
    void lexerStylesJsonTokens();
};

class FakeDocument final : public Scintilla::IDocument
{
public:
    explicit FakeDocument(QByteArray source)
        : source(std::move(source)), styles(this->source.size(), 0)
    {
    }

    int SCI_METHOD Version() const override { return Scintilla::dvRelease4; }
    void SCI_METHOD SetErrorStatus(int status) override { errorStatus = status; }
    Sci_Position SCI_METHOD Length() const override { return source.size(); }
    void SCI_METHOD GetCharRange(char *buffer, Sci_Position position, Sci_Position lengthRetrieve) const override
    {
        std::memcpy(buffer, source.constData() + position, static_cast<size_t>(lengthRetrieve));
    }
    char SCI_METHOD StyleAt(Sci_Position position) const override
    {
        return position >= 0 && position < styles.size() ? styles.at(static_cast<int>(position)) : 0;
    }
    Sci_Position SCI_METHOD LineFromPosition(Sci_Position position) const override
    {
        return source.left(static_cast<int>(position)).count('\n');
    }
    Sci_Position SCI_METHOD LineStart(Sci_Position line) const override
    {
        Sci_Position currentLine = 0;
        for (int index = 0; index < source.size(); ++index) {
            if (currentLine == line) {
                return index;
            }
            if (source.at(index) == '\n') {
                ++currentLine;
            }
        }
        return source.size();
    }
    int SCI_METHOD GetLevel(Sci_Position line) const override { Q_UNUSED(line); return 0; }
    int SCI_METHOD SetLevel(Sci_Position line, int level) override { Q_UNUSED(line); Q_UNUSED(level); return 0; }
    int SCI_METHOD GetLineState(Sci_Position line) const override { Q_UNUSED(line); return 0; }
    int SCI_METHOD SetLineState(Sci_Position line, int state) override { Q_UNUSED(line); Q_UNUSED(state); return 0; }
    void SCI_METHOD StartStyling(Sci_Position position) override { stylingPosition = position; }
    bool SCI_METHOD SetStyleFor(Sci_Position length, char style) override
    {
        const Sci_Position end = std::min<Sci_Position>(stylingPosition + length, styles.size());
        for (Sci_Position position = stylingPosition; position < end; ++position) {
            styles[static_cast<int>(position)] = style;
        }
        stylingPosition = end;
        return true;
    }
    bool SCI_METHOD SetStyles(Sci_Position length, const char *styleBytes) override
    {
        const Sci_Position end = std::min<Sci_Position>(stylingPosition + length, styles.size());
        for (Sci_Position position = stylingPosition; position < end; ++position) {
            styles[static_cast<int>(position)] = styleBytes[position - stylingPosition];
        }
        stylingPosition = end;
        return true;
    }
    void SCI_METHOD DecorationSetCurrentIndicator(int indicator) override { Q_UNUSED(indicator); }
    void SCI_METHOD DecorationFillRange(Sci_Position position, int value, Sci_Position fillLength) override
    {
        Q_UNUSED(position); Q_UNUSED(value); Q_UNUSED(fillLength);
    }
    void SCI_METHOD ChangeLexerState(Sci_Position start, Sci_Position end) override { Q_UNUSED(start); Q_UNUSED(end); }
    int SCI_METHOD CodePage() const override { return 65001; }
    bool SCI_METHOD IsDBCSLeadByte(char ch) const override { Q_UNUSED(ch); return false; }
    const char * SCI_METHOD BufferPointer() override { return source.constData(); }
    int SCI_METHOD GetLineIndentation(Sci_Position line) override { Q_UNUSED(line); return 0; }
    Sci_Position SCI_METHOD LineEnd(Sci_Position line) const override
    {
        const Sci_Position start = LineStart(line);
        const int end = source.indexOf('\n', static_cast<int>(start));
        return end < 0 ? source.size() : end;
    }
    Sci_Position SCI_METHOD GetRelativePosition(Sci_Position positionStart, Sci_Position characterOffset) const override
    {
        return std::clamp<Sci_Position>(positionStart + characterOffset, 0, source.size());
    }
    int SCI_METHOD GetCharacterAndWidth(Sci_Position position, Sci_Position *width) const override
    {
        *width = 1;
        return static_cast<unsigned char>(source.at(static_cast<int>(position)));
    }

    QByteArray source;
    QByteArray styles;
    Sci_Position stylingPosition = 0;
    int errorStatus = 0;
};

void TreeSitterJsonTests::parsesValidDocument()
{
    TSParser *parser = ts_parser_new();
    QVERIFY(parser);
    QVERIFY(ts_parser_set_language(parser, tree_sitter_json()));

    const QByteArray source = QByteArrayLiteral("{\"name\":\"Notepad Next\",\"items\":[true,42]}");
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.constData(), static_cast<uint32_t>(source.size()));
    QVERIFY(tree);

    const TSNode root = ts_tree_root_node(tree);
    QCOMPARE(QString::fromLatin1(ts_node_type(root)), QStringLiteral("document"));
    QVERIFY(!ts_node_has_error(root));
    QVERIFY(ts_node_end_byte(root) == static_cast<uint32_t>(source.size()));

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

void TreeSitterJsonTests::reportsSyntaxErrors()
{
    TSParser *parser = ts_parser_new();
    QVERIFY(parser);
    QVERIFY(ts_parser_set_language(parser, tree_sitter_json()));

    const QByteArray source = QByteArrayLiteral("{\"name\":}");
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.constData(), static_cast<uint32_t>(source.size()));
    QVERIFY(tree);
    QVERIFY(ts_node_has_error(ts_tree_root_node(tree)));

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

void TreeSitterJsonTests::lexerStylesJsonTokens()
{
    const QByteArray source = QByteArrayLiteral("{\"name\":\"Notepad Next\",\"count\":42,\"enabled\":true}");
    FakeDocument document(source);
    TreeSitterJsonLexer lexer;

    QCOMPARE(QString::fromLatin1(lexer.GetName()), QStringLiteral("tree-sitter-json"));
    lexer.Lex(0, document.Length(), 0, &document);
    QCOMPARE(document.errorStatus, 0);
    QCOMPARE(document.styles.at(source.indexOf("\"name\"")), char(3));
    QCOMPARE(document.styles.at(source.indexOf("\"Notepad Next\"")), char(1));
    QCOMPARE(document.styles.at(source.indexOf("42")), char(2));
    QCOMPARE(document.styles.at(source.indexOf("true")), char(4));
}

QTEST_APPLESS_MAIN(TreeSitterJsonTests)
#include "TreeSitterJsonTests.moc"
