/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef NPPPLUGINMANAGER_H
#define NPPPLUGINMANAGER_H

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QSettings>
#include <QVector>

#include "NppPluginTrust.h"

#include <memory>
#include <vector>

class EditorManager;
class MainWindow;
class QMenu;
class ScintillaNext;

namespace Scintilla
{
struct NotificationData;
}

class NppPluginManager final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit NppPluginManager(MainWindow *mainWindow,
                              EditorManager *editorManager,
                              bool safeMode = false,
                              QObject *parent = nullptr);
    ~NppPluginManager() override;

    void load();
    int loadedPluginCount() const;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    struct LoadedPlugin;

    void attachEditor(ScintillaNext *editor);

#ifdef Q_OS_WIN
    struct BlockedPlugin
    {
        NppPluginTrust::Identity identity;
        QString reason;
    };

    void refreshTrustMenu();
    void scanAndLoadPlugins();
    bool loadPlugin(const NppPluginTrust::Identity &identity, QString *error);
    void enableUserPlugins();
    void trustAndLoad(int index);
    void revokeTrust(const NppPluginTrust::Identity &identity);
    void revokeAllTrust();

    void notifyEditor(ScintillaNext *editor, Scintilla::NotificationData *notification);
    void notifyApplication(unsigned int code);
#endif

    MainWindow *mainWindow = nullptr;
    EditorManager *editorManager = nullptr;
    QMenu *pluginMenu = nullptr;
    QMenu *pluginTrustMenu = nullptr;
    bool safeMode = false;
    QSettings pluginSettings;
    NppPluginTrust::TrustStore trustStore;
    std::vector<std::unique_ptr<LoadedPlugin>> plugins;
#ifdef Q_OS_WIN
    QVector<BlockedPlugin> blockedPlugins;
#endif
    bool initialized = false;
};

#endif // NPPPLUGINMANAGER_H
