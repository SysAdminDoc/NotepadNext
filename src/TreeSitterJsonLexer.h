/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TREESITTERJSONLEXER_H
#define TREESITTERJSONLEXER_H

#include "ILexer.h"

#include <tree_sitter/api.h>

#include <cstdint>
#include <vector>

class TreeSitterJsonLexer final : public Scintilla::ILexer5
{
public:
    TreeSitterJsonLexer();
    ~TreeSitterJsonLexer();

    int SCI_METHOD Version() const override;
    void SCI_METHOD Release() override;
    const char * SCI_METHOD PropertyNames() override;
    int SCI_METHOD PropertyType(const char *name) override;
    const char * SCI_METHOD DescribeProperty(const char *name) override;
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *value) override;
    const char * SCI_METHOD DescribeWordListSets() override;
    Sci_Position SCI_METHOD WordListSet(int index, const char *wordList) override;
    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, Scintilla::IDocument *pAccess) override;
    void * SCI_METHOD PrivateCall(int operation, void *pointer) override;
    int SCI_METHOD LineEndTypesSupported() override;
    int SCI_METHOD AllocateSubStyles(int styleBase, int numberStyles) override;
    int SCI_METHOD SubStylesStart(int styleBase) override;
    int SCI_METHOD SubStylesLength(int styleBase) override;
    int SCI_METHOD StyleFromSubStyle(int subStyle) override;
    int SCI_METHOD PrimaryStyleFromStyle(int style) override;
    void SCI_METHOD FreeSubStyles() override;
    void SCI_METHOD SetIdentifiers(int style, const char *identifiers) override;
    int SCI_METHOD DistanceToSecondaryStyles() override;
    const char * SCI_METHOD GetSubStyleBases() override;
    int SCI_METHOD NamedStyles() override;
    const char * SCI_METHOD NameOfStyle(int style) override;
    const char * SCI_METHOD TagsOfStyle(int style) override;
    const char * SCI_METHOD DescriptionOfStyle(int style) override;
    const char * SCI_METHOD GetName() override;
    int SCI_METHOD GetIdentifier() override;
    const char * SCI_METHOD PropertyGet(const char *key) override;

private:
    struct StyleSpan
    {
        Sci_Position start;
        Sci_Position end;
        int style;
    };

    void collectSpans(TSNode node, std::vector<StyleSpan> &spans) const;
    void collectValueSpans(TSNode node, std::vector<StyleSpan> &spans) const;
    void styleDocument(Sci_PositionU startPos, Sci_Position lengthDoc, Scintilla::IDocument *pAccess);

    TSParser *parser = nullptr;
    TSTree *tree = nullptr;
    Sci_Position cachedLength = -1;
    std::uint64_t cachedHash = 0;
};

#endif // TREESITTERJSONLEXER_H
