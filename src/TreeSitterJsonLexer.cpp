/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TreeSitterJsonLexer.h"

#include "Scintilla.h"

#include <algorithm>
#include <cstring>
#include <limits>

extern "C" const TSLanguage *tree_sitter_json();

namespace
{
enum JsonStyle
{
    JsonDefault = 0,
    JsonString = 1,
    JsonNumber = 2,
    JsonKey = 3,
    JsonLiteral = 4,
    JsonComment = 5,
    JsonError = 6,
};

const char *styleName(int style)
{
    switch (style) {
    case JsonString:
        return "string";
    case JsonNumber:
        return "number";
    case JsonKey:
        return "key";
    case JsonLiteral:
        return "literal";
    case JsonComment:
        return "comment";
    case JsonError:
        return "error";
    default:
        return "default";
    }
}

int styleForNode(const char *type)
{
    if (std::strcmp(type, "string") == 0) {
        return JsonString;
    }
    if (std::strcmp(type, "number") == 0) {
        return JsonNumber;
    }
    if (std::strcmp(type, "true") == 0 || std::strcmp(type, "false") == 0 || std::strcmp(type, "null") == 0) {
        return JsonLiteral;
    }
    if (std::strcmp(type, "comment") == 0) {
        return JsonComment;
    }
    return -1;
}

std::uint64_t hashDocument(const char *buffer, Sci_Position length)
{
    constexpr std::uint64_t offset = 1469598103934665603ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    for (Sci_Position index = 0; index < length; ++index) {
        hash ^= static_cast<unsigned char>(buffer[index]);
        hash *= prime;
    }
    return hash;
}
}

TreeSitterJsonLexer::TreeSitterJsonLexer()
    : parser(ts_parser_new())
{
    if (parser) {
        ts_parser_set_language(parser, tree_sitter_json());
    }
}

TreeSitterJsonLexer::~TreeSitterJsonLexer()
{
    if (tree) {
        ts_tree_delete(tree);
    }
    if (parser) {
        ts_parser_delete(parser);
    }
}

int SCI_METHOD TreeSitterJsonLexer::Version() const
{
    return Scintilla::lvRelease5;
}

void SCI_METHOD TreeSitterJsonLexer::Release()
{
    delete this;
}

const char * SCI_METHOD TreeSitterJsonLexer::PropertyNames()
{
    return "";
}

int SCI_METHOD TreeSitterJsonLexer::PropertyType(const char *name)
{
    (void)name;
    return 0;
}

const char * SCI_METHOD TreeSitterJsonLexer::DescribeProperty(const char *name)
{
    (void)name;
    return "";
}

Sci_Position SCI_METHOD TreeSitterJsonLexer::PropertySet(const char *key, const char *value)
{
    (void)key;
    (void)value;
    return 0;
}

const char * SCI_METHOD TreeSitterJsonLexer::DescribeWordListSets()
{
    return "";
}

Sci_Position SCI_METHOD TreeSitterJsonLexer::WordListSet(int index, const char *wordList)
{
    (void)index;
    (void)wordList;
    return 0;
}

void TreeSitterJsonLexer::collectValueSpans(TSNode node, std::vector<StyleSpan> &spans) const
{
    const char *type = ts_node_type(node);
    if (ts_node_is_error(node) || ts_node_is_missing(node)) {
        spans.push_back({static_cast<Sci_Position>(ts_node_start_byte(node)),
                         static_cast<Sci_Position>(ts_node_end_byte(node)), JsonError});
        return;
    }

    if (const int style = styleForNode(type); style >= 0) {
        spans.push_back({static_cast<Sci_Position>(ts_node_start_byte(node)),
                         static_cast<Sci_Position>(ts_node_end_byte(node)), style});
        return;
    }

    const uint32_t childCount = ts_node_named_child_count(node);
    for (uint32_t index = 0; index < childCount; ++index) {
        collectSpans(ts_node_named_child(node, index), spans);
    }
}

void TreeSitterJsonLexer::collectSpans(TSNode node, std::vector<StyleSpan> &spans) const
{
    const char *type = ts_node_type(node);
    if (ts_node_is_error(node) || ts_node_is_missing(node)) {
        spans.push_back({static_cast<Sci_Position>(ts_node_start_byte(node)),
                         static_cast<Sci_Position>(ts_node_end_byte(node)), JsonError});
        return;
    }

    if (std::strcmp(type, "pair") == 0) {
        const TSNode key = ts_node_child_by_field_name(node, "key", 3);
        if (!ts_node_is_null(key)) {
            spans.push_back({static_cast<Sci_Position>(ts_node_start_byte(key)),
                             static_cast<Sci_Position>(ts_node_end_byte(key)), JsonKey});
        }

        const TSNode value = ts_node_child_by_field_name(node, "value", 5);
        if (!ts_node_is_null(value)) {
            collectValueSpans(value, spans);
        }
        return;
    }

    if (const int style = styleForNode(type); style >= 0) {
        spans.push_back({static_cast<Sci_Position>(ts_node_start_byte(node)),
                         static_cast<Sci_Position>(ts_node_end_byte(node)), style});
        return;
    }

    const uint32_t childCount = ts_node_named_child_count(node);
    for (uint32_t index = 0; index < childCount; ++index) {
        collectSpans(ts_node_named_child(node, index), spans);
    }
}

void TreeSitterJsonLexer::styleDocument(Sci_PositionU startPos, Sci_Position lengthDoc, Scintilla::IDocument *pAccess)
{
    const Sci_Position documentLength = pAccess->Length();
    const Sci_Position styleStart = std::min<Sci_Position>(static_cast<Sci_Position>(startPos), documentLength);
    const Sci_Position styleEnd = std::min(documentLength, styleStart + std::max<Sci_Position>(0, lengthDoc));
    if (styleEnd <= styleStart) {
        return;
    }

    std::vector<StyleSpan> spans;
    const char *buffer = pAccess->BufferPointer();
    if (parser && buffer && documentLength >= 0 && documentLength <= std::numeric_limits<uint32_t>::max()) {
        const std::uint64_t currentHash = hashDocument(buffer, documentLength);
        if (!tree || cachedLength != documentLength || cachedHash != currentHash) {
            if (tree) {
                ts_tree_delete(tree);
                tree = nullptr;
            }
            tree = ts_parser_parse_string(parser, nullptr, buffer, static_cast<uint32_t>(documentLength));
            cachedLength = documentLength;
            cachedHash = currentHash;
        }
        if (tree) {
            collectSpans(ts_tree_root_node(tree), spans);
        }
    }

    std::sort(spans.begin(), spans.end(), [](const StyleSpan &left, const StyleSpan &right) {
        if (left.start == right.start) {
            return left.end < right.end;
        }
        return left.start < right.start;
    });

    pAccess->StartStyling(styleStart);
    Sci_Position styled = styleStart;
    for (const StyleSpan &span : spans) {
        const Sci_Position spanStart = std::max(styleStart, span.start);
        const Sci_Position spanEnd = std::min(styleEnd, span.end);
        if (spanEnd <= spanStart || spanEnd <= styled) {
            continue;
        }

        const Sci_Position effectiveStart = std::max(styled, spanStart);
        if (effectiveStart > styled) {
            pAccess->SetStyleFor(effectiveStart - styled, static_cast<char>(JsonDefault));
        }
        pAccess->SetStyleFor(spanEnd - effectiveStart, static_cast<char>(span.style));
        styled = spanEnd;
    }

    if (styled < styleEnd) {
        pAccess->SetStyleFor(styleEnd - styled, static_cast<char>(JsonDefault));
    }
}

void SCI_METHOD TreeSitterJsonLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess)
{
    (void)initStyle;
    if (pAccess) {
        styleDocument(startPos, lengthDoc, pAccess);
    }
}

void SCI_METHOD TreeSitterJsonLexer::Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess)
{
    (void)startPos;
    (void)lengthDoc;
    (void)initStyle;
    (void)pAccess;
}

void * SCI_METHOD TreeSitterJsonLexer::PrivateCall(int operation, void *pointer)
{
    (void)operation;
    (void)pointer;
    return nullptr;
}

int SCI_METHOD TreeSitterJsonLexer::LineEndTypesSupported()
{
    return 0;
}

int SCI_METHOD TreeSitterJsonLexer::AllocateSubStyles(int styleBase, int numberStyles)
{
    (void)styleBase;
    (void)numberStyles;
    return 0;
}

int SCI_METHOD TreeSitterJsonLexer::SubStylesStart(int styleBase)
{
    (void)styleBase;
    return 0;
}

int SCI_METHOD TreeSitterJsonLexer::SubStylesLength(int styleBase)
{
    (void)styleBase;
    return 0;
}

int SCI_METHOD TreeSitterJsonLexer::StyleFromSubStyle(int subStyle)
{
    return subStyle;
}

int SCI_METHOD TreeSitterJsonLexer::PrimaryStyleFromStyle(int style)
{
    return style;
}

void SCI_METHOD TreeSitterJsonLexer::FreeSubStyles()
{
}

void SCI_METHOD TreeSitterJsonLexer::SetIdentifiers(int style, const char *identifiers)
{
    (void)style;
    (void)identifiers;
}

int SCI_METHOD TreeSitterJsonLexer::DistanceToSecondaryStyles()
{
    return 0;
}

const char * SCI_METHOD TreeSitterJsonLexer::GetSubStyleBases()
{
    return "";
}

int SCI_METHOD TreeSitterJsonLexer::NamedStyles()
{
    return 7;
}

const char * SCI_METHOD TreeSitterJsonLexer::NameOfStyle(int style)
{
    return styleName(style);
}

const char * SCI_METHOD TreeSitterJsonLexer::TagsOfStyle(int style)
{
    (void)style;
    return "";
}

const char * SCI_METHOD TreeSitterJsonLexer::DescriptionOfStyle(int style)
{
    return styleName(style);
}

const char * SCI_METHOD TreeSitterJsonLexer::GetName()
{
    return "tree-sitter-json";
}

int SCI_METHOD TreeSitterJsonLexer::GetIdentifier()
{
    return 0x4E4A534F;
}

const char * SCI_METHOD TreeSitterJsonLexer::PropertyGet(const char *key)
{
    (void)key;
    return "";
}
