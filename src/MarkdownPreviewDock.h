/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MARKDOWNPREVIEWDOCK_H
#define MARKDOWNPREVIEWDOCK_H

#include <QDockWidget>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVector>

class ScintillaNext;

class MarkdownPreviewDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit MarkdownPreviewDock(QWidget *parent = nullptr);

public slots:
    void setEditor(ScintillaNext *editor);
    void editorClosed(ScintillaNext *editor);
    void refresh();

private:
    void scheduleRefresh();
    void clearPreview(const QString &message);
    void setPreviewHtml(const QString &html, const QUrl &baseUrl);
    QUrl baseUrlForEditor() const;

    QPointer<ScintillaNext> editor;
    QVector<QMetaObject::Connection> editorConnections;
    QTimer refreshTimer;
    QWidget *previewView = nullptr;
};

#endif // MARKDOWNPREVIEWDOCK_H
