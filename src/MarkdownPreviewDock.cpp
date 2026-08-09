/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MarkdownPreviewDock.h"

#include "MarkdownRenderer.h"
#include "ScintillaNext.h"

#include <Scintilla.h>

#include <QFileInfo>
#include <QPalette>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>

#ifdef NOTEPADNEXT_HAS_WEBENGINE
#include <QWebEngineSettings>
#include <QWebEngineView>
#endif

namespace
{
constexpr int MarkdownPreviewDelayMs = 120;

QString plainMessage(const QString &message)
{
    return QStringLiteral("<html><body><p>%1</p></body></html>").arg(message.toHtmlEscaped());
}
}

MarkdownPreviewDock::MarkdownPreviewDock(QWidget *parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("markdownPreviewDock"));
    setWindowTitle(tr("Markdown Preview"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    setMinimumWidth(300);

#ifdef NOTEPADNEXT_HAS_WEBENGINE
    auto *view = new QWebEngineView(this);
    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    previewView = view;
#else
    auto *view = new QTextBrowser(this);
    view->setOpenExternalLinks(true);
    view->setOpenLinks(true);
    previewView = view;
#endif
    previewView->setObjectName(QStringLiteral("markdownPreviewView"));
    setWidget(previewView);

    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(MarkdownPreviewDelayMs);
    connect(&refreshTimer, &QTimer::timeout, this, &MarkdownPreviewDock::refresh);
    connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) {
            scheduleRefresh();
        }
    });

    clearPreview(tr("Open a Markdown document to see its live preview."));
}

void MarkdownPreviewDock::setEditor(ScintillaNext *newEditor)
{
    if (editor == newEditor) {
        scheduleRefresh();
        return;
    }

    for (const QMetaObject::Connection &connection : editorConnections) {
        disconnect(connection);
    }
    editorConnections.clear();
    editor = newEditor;

    if (editor) {
        editorConnections.append(connect(editor, &ScintillaNext::notify, this, [this, newEditor](Scintilla::NotificationData *notification) {
            if (!notification || editor != newEditor || notification->nmhdr.code != Scintilla::Notification::Modified) {
                return;
            }
            if (Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::InsertText) ||
                Scintilla::FlagSet(notification->modificationType, Scintilla::ModificationFlags::DeleteText)) {
                scheduleRefresh();
            }
        }));
        editorConnections.append(connect(editor, &ScintillaNext::lexerChanged, this, &MarkdownPreviewDock::scheduleRefresh));
        editorConnections.append(connect(editor, &ScintillaNext::renamed, this, &MarkdownPreviewDock::scheduleRefresh));
        editorConnections.append(connect(editor, &ScintillaNext::largeFileModeChanged, this, &MarkdownPreviewDock::scheduleRefresh));
    }

    scheduleRefresh();
}

void MarkdownPreviewDock::editorClosed(ScintillaNext *closedEditor)
{
    if (editor == closedEditor) {
        setEditor(nullptr);
    }
}

void MarkdownPreviewDock::scheduleRefresh()
{
    refreshTimer.start();
}

void MarkdownPreviewDock::refresh()
{
    if (!editor) {
        clearPreview(tr("Open a Markdown document to see its live preview."));
        return;
    }

    if (editor->isLargeFileMode()) {
        clearPreview(tr("Markdown preview is disabled in large-file safety mode."));
        return;
    }

    if (!MarkdownRenderer::isMarkdownLanguage(editor->languageName)) {
        clearPreview(tr("Markdown preview is available for Markdown and MDX documents."));
        return;
    }

    const QString html = MarkdownRenderer::toHtml(editor->getText(editor->textLength()),
                                                   palette().text().color(),
                                                   palette().base().color());
    setPreviewHtml(html, baseUrlForEditor());
}

void MarkdownPreviewDock::clearPreview(const QString &message)
{
    setPreviewHtml(plainMessage(message), QUrl());
}

void MarkdownPreviewDock::setPreviewHtml(const QString &html, const QUrl &baseUrl)
{
#ifdef NOTEPADNEXT_HAS_WEBENGINE
    qobject_cast<QWebEngineView *>(previewView)->setHtml(html, baseUrl);
#else
    auto *view = qobject_cast<QTextBrowser *>(previewView);
    view->document()->setBaseUrl(baseUrl);
    view->setHtml(html);
#endif
}

QUrl MarkdownPreviewDock::baseUrlForEditor() const
{
    if (!editor || !editor->isFile()) {
        return QUrl();
    }
    return QUrl::fromLocalFile(QFileInfo(editor->getFilePath()).absolutePath() + QLatin1Char('/'));
}
