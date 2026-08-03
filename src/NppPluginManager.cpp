/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "NppPluginManager.h"

#include "EditorManager.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QVector>

#include <algorithm>

#ifdef Q_OS_WIN

#include "NppPluginInterface.h"

#include <QLibrary>

#include <windows.h>

namespace
{
constexpr UINT NppMessageBase = WM_USER + 1000;
constexpr UINT NppGetCurrentScintilla = NppMessageBase + 4;
constexpr UINT NppGetCurrentLangType = NppMessageBase + 5;
constexpr UINT NppGetNbOpenFiles = NppMessageBase + 7;
constexpr UINT NppGetCurrentDocIndex = NppMessageBase + 23;
constexpr UINT NppSetMenuItemCheck = NppMessageBase + 40;
constexpr UINT NppMakeCurrentBufferDirty = NppMessageBase + 44;
constexpr UINT NppGetPluginConfigDir = NppMessageBase + 46;
constexpr UINT NppMenuCommand = NppMessageBase + 48;
constexpr UINT NppGetNppVersion = NppMessageBase + 50;
constexpr UINT NppGetSettingsDirPath = NppMessageBase + 119;

constexpr UINT RunCommandUser = WM_USER + 3000;
constexpr UINT NppGetFullCurrentPath = RunCommandUser + 1;
constexpr UINT NppGetCurrentDirectory = RunCommandUser + 2;
constexpr UINT NppGetFileName = RunCommandUser + 3;
constexpr UINT NppGetNamePart = RunCommandUser + 4;
constexpr UINT NppGetExtPart = RunCommandUser + 5;
constexpr UINT NppGetCurrentWord = RunCommandUser + 6;
constexpr UINT NppGetNppDirectory = RunCommandUser + 7;
constexpr UINT NppGetNppFullFilePath = RunCommandUser + 10;
constexpr UINT NppGetCurrentLineString = RunCommandUser + 12;
constexpr UINT NppGetCurrentLine = RunCommandUser + 8;
constexpr UINT NppGetCurrentColumn = RunCommandUser + 9;

constexpr unsigned int NppnReady = 1001;
constexpr unsigned int NppnFileOpened = 1004;
constexpr unsigned int NppnFileClosed = 1005;
constexpr unsigned int NppnFileBeforeSave = 1007;
constexpr unsigned int NppnFileSaved = 1008;
constexpr unsigned int NppnBufferActivated = 1010;
constexpr unsigned int NppnLangChanged = 1011;
constexpr unsigned int NppnShutdown = 1009;

template<typename Function>
Function resolve(QLibrary &library, const char *name)
{
    return reinterpret_cast<Function>(library.resolve(name));
}

HWND nativeHandle(QWidget *widget)
{
    return widget ? reinterpret_cast<HWND>(widget->winId()) : nullptr;
}

HWND editorHandle(ScintillaNext *editor)
{
    return editor ? nativeHandle(editor->viewport()) : nullptr;
}

QStringList pluginFiles()
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList roots = {
        QDir(applicationDirectory).filePath(QStringLiteral("plugins")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("plugins")),
    };

    QStringList files;
    QSet<QString> seen;
    for (const QString &root : roots) {
        if (!QDir(root).exists()) {
            continue;
        }

        QDirIterator iterator(root, {QStringLiteral("*.dll")}, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString file = QFileInfo(iterator.next()).canonicalFilePath();
            if (!file.isEmpty() && !seen.contains(file)) {
                seen.insert(file);
                files.append(file);
            }
        }
    }

    std::sort(files.begin(), files.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    return files;
}

QKeySequence shortcutFor(const NppPlugin::ShortcutKey *shortcut)
{
    if (!shortcut || shortcut->key == 0) {
        return {};
    }

    Qt::KeyboardModifiers modifiers;
    if (shortcut->isCtrl) {
        modifiers |= Qt::ControlModifier;
    }
    if (shortcut->isAlt) {
        modifiers |= Qt::AltModifier;
    }
    if (shortcut->isShift) {
        modifiers |= Qt::ShiftModifier;
    }

    return QKeySequence(modifiers | static_cast<Qt::Key>(shortcut->key));
}

int languageType(const QString &language)
{
    static const QHash<QString, int> values = {
        {QStringLiteral("Text"), 0},
        {QStringLiteral("PHP"), 1},
        {QStringLiteral("C"), 2},
        {QStringLiteral("C++"), 3},
        {QStringLiteral("C#"), 4},
        {QStringLiteral("Objective-C"), 5},
        {QStringLiteral("Java"), 6},
        {QStringLiteral("RC"), 7},
        {QStringLiteral("HTML"), 8},
        {QStringLiteral("XML"), 9},
        {QStringLiteral("Python"), 23},
        {QStringLiteral("Lua"), 24},
        {QStringLiteral("JavaScript"), 57},
        {QStringLiteral("JSON"), 58},
        {QStringLiteral("Rust"), 75},
        {QStringLiteral("Nim"), 74},
        {QStringLiteral("TOML"), 89},
    };

    return values.value(language, 0);
}

bool writeWideString(const QString &value, WPARAM capacity, LPARAM destination)
{
    if (destination == 0 || capacity <= static_cast<WPARAM>(value.size())) {
        return false;
    }

    wchar_t *buffer = reinterpret_cast<wchar_t *>(destination);
    const int copied = value.toWCharArray(buffer);
    buffer[copied] = L'\0';
    return true;
}

QString currentPath(MainWindow *window)
{
    const ScintillaNext *editor = window ? window->currentEditor() : nullptr;
    return editor && editor->isFile() ? editor->getFilePath() : QString();
}

bool markDirty(ScintillaNext *editor)
{
    if (!editor || editor->readOnly()) {
        return false;
    }

    const sptr_t position = editor->currentPos();
    const sptr_t anchor = editor->anchor();
    editor->beginUndoAction();
    editor->insertText(position, " ");
    editor->deleteRange(position, 1);
    editor->endUndoAction();
    editor->setCurrentPos(position);
    editor->setAnchor(anchor);
    return editor->modify();
}
}

struct NppPluginManager::LoadedPlugin
{
    QLibrary library;
    QString path;
    QString name;
    NppPlugin::SetInfo setInfo = nullptr;
    NppPlugin::GetFuncsArray getFuncsArray = nullptr;
    NppPlugin::BeNotified beNotified = nullptr;
    NppPlugin::MessageProc messageProc = nullptr;
    QHash<int, QAction *> actions;
};

#else

struct NppPluginManager::LoadedPlugin
{
};

#endif

NppPluginManager::NppPluginManager(MainWindow *mainWindow, EditorManager *editorManager, QObject *parent)
    : QObject(parent),
      mainWindow(mainWindow),
      editorManager(editorManager)
{
#ifdef Q_OS_WIN
    pluginMenu = mainWindow->menuBar()->addMenu(tr("Plugins"));
    pluginMenu->setObjectName(QStringLiteral("menuPlugins"));
    pluginMenu->setVisible(false);
#endif

    if (editorManager) {
        connect(editorManager, &EditorManager::editorCreated, this, &NppPluginManager::attachEditor);
    }
    if (mainWindow) {
        connect(mainWindow, &MainWindow::editorActivated, this, [this](ScintillaNext *) {
#ifdef Q_OS_WIN
            if (initialized) {
                notifyApplication(NppnBufferActivated);
            }
#endif
        });
    }
}

NppPluginManager::~NppPluginManager()
{
#ifdef Q_OS_WIN
    if (initialized) {
        notifyApplication(NppnShutdown);
    }
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
#endif
}

void NppPluginManager::load()
{
#ifdef Q_OS_WIN
    if (!mainWindow || initialized) {
        return;
    }

    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

    for (const QString &path : pluginFiles()) {
        auto plugin = std::make_unique<LoadedPlugin>();
        plugin->path = path;
        plugin->library.setFileName(path);
        plugin->library.setLoadHints(QLibrary::PreventUnloadHint);
        if (!plugin->library.load()) {
            qWarning("Notepad++ plugin load failed for %s: %s", qUtf8Printable(path), qUtf8Printable(plugin->library.errorString()));
            continue;
        }

        plugin->setInfo = resolve<NppPlugin::SetInfo>(plugin->library, "setInfo");
        if (!plugin->setInfo) {
            qWarning("Notepad++ plugin %s has no setInfo export", qUtf8Printable(path));
            continue;
        }

        const NppPlugin::IsUnicode isUnicode = resolve<NppPlugin::IsUnicode>(plugin->library, "isUnicode");
        if (isUnicode && !isUnicode()) {
            qWarning("Skipping ANSI-only Notepad++ plugin %s", qUtf8Printable(path));
            continue;
        }

        plugin->getFuncsArray = resolve<NppPlugin::GetFuncsArray>(plugin->library, "getFuncsArray");
        plugin->beNotified = resolve<NppPlugin::BeNotified>(plugin->library, "beNotified");
        plugin->messageProc = resolve<NppPlugin::MessageProc>(plugin->library, "messageProc");

        const NppPlugin::GetName getName = resolve<NppPlugin::GetName>(plugin->library, "getName");
        if (getName) {
            if (const wchar_t *name = getName()) {
                plugin->name = QString::fromWCharArray(name);
            }
        }
        if (plugin->name.isEmpty()) {
            plugin->name = QFileInfo(path).completeBaseName();
        }

        plugin->setInfo({
            nativeHandle(mainWindow),
            editorHandle(mainWindow->editors().value(0)),
            editorHandle(mainWindow->editors().value(1)),
        });

        if (plugin->getFuncsArray && pluginMenu) {
            int functionCount = 0;
            NppPlugin::FuncItem *functions = plugin->getFuncsArray(&functionCount);
            if (functions && functionCount > 0 && functionCount <= 1024) {
                for (int index = 0; index < functionCount; ++index) {
                    NppPlugin::FuncItem &function = functions[index];
                    if (!function.function) {
                        pluginMenu->addSeparator();
                        continue;
                    }

                    auto *action = new QAction(QString::fromWCharArray(function.itemName), pluginMenu);
                    action->setObjectName(QStringLiteral("plugin_%1_%2").arg(plugin->name).arg(function.commandId));
                    action->setCheckable(function.initialChecked);
                    action->setShortcut(shortcutFor(function.shortcut));
                    const NppPlugin::Func callback = function.function;
                    connect(action, &QAction::triggered, this, [callback]() {
                        callback();
                    });
                    pluginMenu->addAction(action);
                    plugin->actions.insert(function.commandId, action);
                }
            }
        }

        qInfo("Loaded Notepad++ plugin %s from %s", qUtf8Printable(plugin->name), qUtf8Printable(path));
        plugins.push_back(std::move(plugin));
    }

    for (ScintillaNext *editor : mainWindow->editors()) {
        attachEditor(editor);
    }

    initialized = true;
    if (pluginMenu) {
        pluginMenu->setVisible(!plugins.empty());
    }

    for (ScintillaNext *editor : mainWindow->editors()) {
        if (editor && editor->isFile()) {
            notifyApplication(NppnFileOpened);
        }
    }
    notifyApplication(NppnReady);
#else
    qInfo("Notepad++ DLL plugins are only supported on Windows builds");
#endif
}

int NppPluginManager::loadedPluginCount() const
{
    return static_cast<int>(plugins.size());
}

void NppPluginManager::attachEditor(ScintillaNext *editor)
{
#ifdef Q_OS_WIN
    if (!editor) {
        return;
    }

    connect(editor, &ScintillaNext::notify, this, [this, editor](Scintilla::NotificationData *notification) {
        notifyEditor(editor, notification);
    });
    connect(editor, &ScintillaNext::aboutToSave, this, [this]() {
        if (initialized) {
            notifyApplication(NppnFileBeforeSave);
        }
    });
    connect(editor, &ScintillaNext::saved, this, [this]() {
        if (initialized) {
            notifyApplication(NppnFileSaved);
        }
    });
    connect(editor, &ScintillaNext::closed, this, [this]() {
        if (initialized) {
            notifyApplication(NppnFileClosed);
        }
    });
    connect(editor, &ScintillaNext::lexerChanged, this, [this]() {
        if (initialized) {
            notifyApplication(NppnLangChanged);
        }
    });

    if (initialized && editor->isFile()) {
        notifyApplication(NppnFileOpened);
    }
#else
    Q_UNUSED(editor);
#endif
}

#ifdef Q_OS_WIN
void NppPluginManager::notifyEditor(ScintillaNext *editor, Scintilla::NotificationData *notification)
{
    if (!initialized || !editor || !notification) {
        return;
    }

    SCNotification legacy = {};
    legacy.nmhdr.hwndFrom = editorHandle(editor);
    legacy.nmhdr.idFrom = 0;
    legacy.nmhdr.code = static_cast<unsigned int>(notification->nmhdr.code);
    legacy.position = notification->position;
    legacy.ch = notification->ch;
    legacy.modifiers = static_cast<int>(notification->modifiers);
    legacy.modificationType = static_cast<int>(notification->modificationType);
    legacy.text = notification->text;
    legacy.length = notification->length;
    legacy.linesAdded = notification->linesAdded;
    legacy.message = static_cast<int>(notification->message);
    legacy.wParam = notification->wParam;
    legacy.lParam = notification->lParam;
    legacy.line = notification->line;
    legacy.foldLevelNow = static_cast<int>(notification->foldLevelNow);
    legacy.foldLevelPrev = static_cast<int>(notification->foldLevelPrev);
    legacy.margin = notification->margin;
    legacy.listType = notification->listType;
    legacy.x = notification->x;
    legacy.y = notification->y;
    legacy.token = notification->token;
    legacy.annotationLinesAdded = notification->annotationLinesAdded;
    legacy.updated = static_cast<int>(notification->updated);
    legacy.listCompletionMethod = static_cast<int>(notification->listCompletionMethod);
    legacy.characterSource = static_cast<int>(notification->characterSource);

    for (const auto &plugin : plugins) {
        if (plugin->beNotified) {
            plugin->beNotified(&legacy);
        }
    }
}

void NppPluginManager::notifyApplication(unsigned int code)
{
    if (!initialized && code != NppnReady) {
        return;
    }

    SCNotification notification = {};
    notification.nmhdr.hwndFrom = nativeHandle(mainWindow);
    notification.nmhdr.idFrom = 0;
    notification.nmhdr.code = code;

    for (const auto &plugin : plugins) {
        if (plugin->beNotified) {
            plugin->beNotified(&notification);
        }
    }
}

bool NppPluginManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != QByteArrayLiteral("windows_generic_MSG") && eventType != QByteArrayLiteral("windows_dispatcher_MSG")) {
        return false;
    }

    auto *msg = static_cast<MSG *>(message);
    if (!msg || !result) {
        return false;
    }

    const HWND host = nativeHandle(mainWindow);
    if (msg->hwnd == host) {
        switch (msg->message) {
        case NppGetCurrentScintilla:
            if (msg->lParam != 0) {
                *reinterpret_cast<int *>(msg->lParam) = 0;
                *result = TRUE;
                return true;
            }
            break;
        case NppGetCurrentLangType:
            if (msg->lParam != 0 && mainWindow->currentEditor()) {
                *reinterpret_cast<int *>(msg->lParam) = languageType(mainWindow->currentEditor()->languageName);
                *result = TRUE;
                return true;
            }
            break;
        case NppGetNbOpenFiles:
            *result = mainWindow->editors().size();
            return true;
        case NppGetCurrentDocIndex:
            *result = msg->lParam == 0 ? mainWindow->editors().indexOf(mainWindow->currentEditor()) : -1;
            return true;
        case NppMakeCurrentBufferDirty:
            if (mainWindow->currentEditor()) {
                *result = markDirty(mainWindow->currentEditor()) ? TRUE : FALSE;
                return true;
            }
            break;
        case NppSetMenuItemCheck:
            for (const auto &plugin : plugins) {
                if (QAction *action = plugin->actions.value(static_cast<int>(msg->wParam))) {
                    action->setChecked(msg->lParam != 0);
                    *result = TRUE;
                    return true;
                }
            }
            break;
        case NppMenuCommand:
            for (const auto &plugin : plugins) {
                if (QAction *action = plugin->actions.value(static_cast<int>(msg->lParam))) {
                    action->trigger();
                    *result = TRUE;
                    return true;
                }
            }
            break;
        case NppGetNppVersion:
            *result = 0;
            return true;
        case NppGetPluginConfigDir:
        case NppGetSettingsDirPath:
        {
            const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            *result = writeWideString(directory, msg->wParam, msg->lParam);
            return true;
        }
        case NppGetFullCurrentPath:
        case NppGetCurrentDirectory:
        case NppGetFileName:
        case NppGetNamePart:
        case NppGetExtPart:
        case NppGetNppDirectory:
        case NppGetNppFullFilePath:
        case NppGetCurrentWord:
        case NppGetCurrentLineString:
        {
            ScintillaNext *editor = mainWindow->currentEditor();
            const QString path = currentPath(mainWindow);
            QString value;
            if (msg->message == NppGetFullCurrentPath) {
                value = path;
            } else if (msg->message == NppGetCurrentDirectory) {
                value = QFileInfo(path).absolutePath();
            } else if (msg->message == NppGetFileName) {
                value = QFileInfo(path).fileName();
            } else if (msg->message == NppGetNamePart) {
                value = QFileInfo(path).completeBaseName();
            } else if (msg->message == NppGetExtPart) {
                value = QFileInfo(path).suffix();
            } else if (msg->message == NppGetNppDirectory) {
                value = QCoreApplication::applicationDirPath();
            } else if (msg->message == NppGetNppFullFilePath) {
                value = QCoreApplication::applicationFilePath();
            } else if (msg->message == NppGetCurrentWord && editor) {
                value = QString::fromUtf8(editor->textRange(editor->wordStartPosition(editor->currentPos(), true),
                                                              editor->wordEndPosition(editor->currentPos(), true)));
            } else if (msg->message == NppGetCurrentLineString && editor) {
                value = QString::fromUtf8(editor->getLine(editor->lineFromPosition(editor->currentPos())));
            }

            *result = writeWideString(value, msg->wParam, msg->lParam);
            return true;
        }
        case NppGetCurrentLine:
            *result = mainWindow->currentEditor() ? mainWindow->currentEditor()->lineFromPosition(mainWindow->currentEditor()->currentPos()) : 0;
            return true;
        case NppGetCurrentColumn:
            *result = mainWindow->currentEditor() ? mainWindow->currentEditor()->column(mainWindow->currentEditor()->currentPos()) : 0;
            return true;
        default:
            break;
        }
    }

    for (ScintillaNext *editor : mainWindow->editors()) {
        if (msg->hwnd == editorHandle(editor) && msg->message >= 2000 && msg->message < 5000) {
            *result = editor->send(msg->message, msg->wParam, msg->lParam);
            return true;
        }
    }

    for (const auto &plugin : plugins) {
        if (plugin->messageProc) {
            const LRESULT pluginResult = plugin->messageProc(msg->message, msg->wParam, msg->lParam);
            if (pluginResult != 0) {
                *result = pluginResult;
                return true;
            }
        }
    }

    return false;
}

#else

bool NppPluginManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

#endif
