/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef NPPPLUGININTERFACE_H
#define NPPPLUGININTERFACE_H

#ifdef Q_OS_WIN

#include <windows.h>

#include "Scintilla.h"

namespace NppPlugin
{
constexpr int menuItemSize = 64;

struct NppData
{
    HWND nppHandle = nullptr;
    HWND scintillaMainHandle = nullptr;
    HWND scintillaSecondHandle = nullptr;
};

struct ShortcutKey
{
    bool isCtrl = false;
    bool isAlt = false;
    bool isShift = false;
    UCHAR key = 0;
};

using Func = void (__cdecl *)();

struct FuncItem
{
    wchar_t itemName[menuItemSize] = {L'\0'};
    Func function = nullptr;
    int commandId = 0;
    bool initialChecked = false;
    ShortcutKey *shortcut = nullptr;
};

using GetName = const wchar_t * (__cdecl *)();
using SetInfo = void (__cdecl *)(NppData);
using GetFuncsArray = FuncItem * (__cdecl *)(int *);
using BeNotified = void (__cdecl *)(SCNotification *);
using MessageProc = LRESULT (__cdecl *)(UINT, WPARAM, LPARAM);
using IsUnicode = BOOL (__cdecl *)();
}

#endif

#endif // NPPPLUGININTERFACE_H
