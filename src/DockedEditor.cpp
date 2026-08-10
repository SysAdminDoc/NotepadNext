/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "DockedEditor.h"
#include "DockAreaTabBar.h"
#include "DockAreaWidget.h"
#include "DockWidgetTab.h"
#include "DockComponentsFactory.h"
#include "DockedEditorTitleBar.h"
#include "DockAreaTitleBar.h"

#include "ScintillaNext.h"

#include <QUuid>

#ifdef NOTEPADNEXT_LIFECYCLE_TRACE
#include <QDir>
#include <QFile>

static void lifecycleDockTrace(const char *message)
{
    QFile trace(QDir::temp().filePath(QStringLiteral("NotepadNextLifecycleTrace.txt")));
    if (trace.open(QIODevice::WriteOnly | QIODevice::Append)) {
        trace.write(message);
        trace.write("\n");
    }
}
#endif


class DockedEditorComponentsFactory : public ads::CDockComponentsFactory
{
public:
    ads::CDockAreaTitleBar* createDockAreaTitleBar(ads::CDockAreaWidget* DockArea) const {
        DockedEditorTitleBar *titleBar = new DockedEditorTitleBar(DockArea);

        // Disable the built in context menu for the title bar since it has options we don't want
        titleBar->setContextMenuPolicy(Qt::NoContextMenu);

        return titleBar;
    }
};


DockedEditor::DockedEditor(QWidget *parent) : QObject(parent)
{
    ads::CDockComponentsFactory::setFactory(new DockedEditorComponentsFactory());

    ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewIsDynamic, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewShowsContentPixmap, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);
    // When tabs title/text elide disabled and lots of tabs opened, tabs menu button will not show
    // as it only shows when tab title elided. 
    // So disable dynamic tabs menu visibility.
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::MiddleMouseButtonClosesTab, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);

    dockManager = new ads::CDockManager(parent);
    dockManager->setStyleSheet("");

    connect(dockManager, &ads::CDockManager::focusedDockWidgetChanged, this, [=](ads::CDockWidget* old, ads::CDockWidget* now) {
        Q_UNUSED(old)
        lifecycleDockTrace(now ? (now->widget() ? "dock:focus-widget" : "dock:focus-no-widget") : "dock:focus-none");

        if (!now || now->isClosed()) {
            lifecycleDockTrace("dock:clear-none");
            currentEditor = nullptr;
            latestDockArea = nullptr;
            return;
        }

        ScintillaNext *editor = qobject_cast<ScintillaNext *>(now->widget());
        if (!editor) {
            lifecycleDockTrace("dock:clear-no-editor");
            currentEditor = nullptr;
            latestDockArea = nullptr;
            return;
        }

        currentEditor = editor;
        lifecycleDockTrace("dock:before-focus");
        editor->grabFocus();
        lifecycleDockTrace("dock:after-focus");
        emit editorActivated(editor);
        lifecycleDockTrace("dock:after-activate");
    });

    connect(dockManager, &ads::CDockManager::dockAreaCreated, this, [=](ads::CDockAreaWidget* DockArea) {
        DockedEditorTitleBar *titleBar = qobject_cast<DockedEditorTitleBar *>(DockArea->titleBar());
        connect(titleBar, &DockedEditorTitleBar::doubleClicked, this, &DockedEditor::titleBarDoubleClicked);

        connect(DockArea->titleBar()->tabBar(), &ads::CDockAreaTabBar::tabMoved, this, [=](int from, int to) {
            Q_UNUSED(from);
            Q_UNUSED(to);

            emit editorOrderChanged();
        });

        // In theory the order changes when a new dock area is created (e.g. editor is dragged and dropped),
        // but the dockAreaCreated() signal is triggered before it is actually added to the CDockManager,
        // so interrogating the dock manager during the signal doesn't help.
        //emit editorOrderChanged();
    });
}


ScintillaNext *DockedEditor::getCurrentEditor() const
{
    return currentEditor;
}

int DockedEditor::count() const
{
    int total = 0;

    for (ads::CDockWidget *dockWidget : dockManager->dockWidgetsMap()) {
        if (dockWidget && !dockWidget->isClosed() &&
            qobject_cast<ScintillaNext *>(dockWidget->widget())) {
            ++total;
        }
    }

    return total;
}

QVector<ScintillaNext *> DockedEditor::editors() const
{
    QVector<ScintillaNext *> editors;

    // Use the manager's registered widgets rather than only opened areas.  A
    // dock can be between area transitions while its editor is still alive;
    // omitting it here desynchronizes the editor manager and session/close
    // workflows.
    for (ads::CDockWidget *dockWidget : dockManager->dockWidgetsMap()) {
        if (!dockWidget || dockWidget->isClosed()) {
            continue;
        }

        if (ScintillaNext *editor = qobject_cast<ScintillaNext *>(dockWidget->widget());
            editor) {
            editors.append(editor);
        }
    }

    return editors;
}

void DockedEditor::switchToEditor(const ScintillaNext *editor)
{
    ads::CDockWidget *dockWidget = qobject_cast<ads::CDockWidget *>(editor->parentWidget());

    if (dockWidget == Q_NULLPTR) {
        qWarning() << "Expected editor's parent to be CDockWidget";
    }
    else {
        dockWidget->raise();
    }
}

void DockedEditor::dockWidgetCloseRequested()
{
    ads::CDockWidget *dockWidget = qobject_cast<ads::CDockWidget *>(sender());
    ScintillaNext *editor = qobject_cast<ScintillaNext *>(dockWidget->widget());

    emit editorCloseRequested(editor);
}

ads::CDockAreaWidget *DockedEditor::currentDockArea() const
{
    if (!dockManager) {
        return nullptr;
    }

    if (ads::CDockWidget *focused = dockManager->focusedDockWidget();
        focused && !focused->isClosed()) {
        if (ads::CDockAreaWidget *area = focused->dockAreaWidget(); area) {
            return area;
        }
    }

    const auto areas = dockManager->openedDockAreas();
    return areas.isEmpty() ? nullptr : areas.constLast();
}

void DockedEditor::addEditor(ScintillaNext *editor)
{
    qInfo(Q_FUNC_INFO);

    Q_ASSERT(editor != Q_NULLPTR);

    if (currentEditor == Q_NULLPTR) {
        currentEditor = editor;
    }

    // Create the dock widget for the editor
    ads::CDockWidget *dockWidget = dockManager->createDockWidget(editor->getName());

    // Disable elide, elided file names not readable when lots of files opened
    dockWidget->tabWidget()->setElideMode(Qt::ElideNone);

    // We need a unique object name. Can't use the name or file path so use a uuid
    dockWidget->setObjectName(QUuid::createUuid().toString());

    dockWidget->setWidget(editor);
    dockWidget->setFeature(ads::CDockWidget::DockWidgetFeature::DockWidgetDeleteOnClose, true);
    dockWidget->setFeature(ads::CDockWidget::DockWidgetFeature::CustomCloseHandling, true);
    dockWidget->setFeature(ads::CDockWidget::DockWidgetFeature::DockWidgetFloatable, false);

    dockWidget->tabWidget()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dockWidget->tabWidget(), &QWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        Q_UNUSED(pos)

        emit contextMenuRequestedForEditor(editor);
    });

    // Set the tooltip based on the buffer
    if (editor->isFile()) {
        dockWidget->tabWidget()->setToolTip(editor->getFilePath());
    }
    else {
        dockWidget->tabWidget()->setToolTip(editor->getName());
    }

    // Set the icon
    updateDockIcon(editor, dockWidget);
    if (!editor->readOnly()) {
        connect(editor, &ScintillaNext::savePointChanged, dockWidget, [this, editor, dockWidget](bool dirty) {
            Q_UNUSED(dirty)
            updateDockIcon(editor, dockWidget);
        });
    }

    connect(editor, &ScintillaNext::closed, dockWidget, &ads::CDockWidget::closeDockWidget);
    connect(editor, &ScintillaNext::closed, this, [this, editor]() {
        if (currentEditor == editor) {
            currentEditor = nullptr;
        }
        emit editorClosed(editor);
    });
    connect(editor, &ScintillaNext::renamed, this, [=]() { editorRenamed(editor); });

    connect(dockWidget, &ads::CDockWidget::closeRequested, this, &DockedEditor::dockWidgetCloseRequested);

    latestDockArea = dockManager->addDockWidget(ads::CenterDockWidgetArea, dockWidget, currentDockArea());

    emit editorAdded(editor);
}

void DockedEditor::setIconTheme(IconThemeManager::Pack pack)
{
    iconTheme = pack;
    for (ads::CDockWidget *dockWidget : dockManager->dockWidgetsMap()) {
        auto *editor = qobject_cast<ScintillaNext *>(dockWidget->widget());
        if (editor) {
            updateDockIcon(editor, dockWidget);
        }
    }
}

void DockedEditor::updateDockIcon(ScintillaNext *editor, ads::CDockWidget *dockWidget)
{
    QString iconPath;
    QString semanticKey;
    if (editor->readOnly()) {
        iconPath = QStringLiteral(":/icons/readonly.png");
        semanticKey = QStringLiteral("readonly");
    }
    else if (editor->canSaveToDisk()) {
        iconPath = QStringLiteral(":/icons/unsaved.png");
        semanticKey = QStringLiteral("unsaved");
    }
    else {
        iconPath = QStringLiteral(":/icons/saved.png");
        semanticKey = QStringLiteral("saved");
    }

    dockWidget->tabWidget()->setIcon(IconThemeManager::recolor(QIcon(iconPath), iconTheme, semanticKey));
}

void DockedEditor::editorRenamed(ScintillaNext *editor)
{
    Q_ASSERT(editor != Q_NULLPTR);

    ads::CDockWidget *dockWidget = qobject_cast<ads::CDockWidget *>(editor->parentWidget());

    dockWidget->setWindowTitle(editor->getName());

    if (editor->isFile()) {
        dockWidget->tabWidget()->setToolTip(editor->getFilePath());
    }
    else {
        dockWidget->tabWidget()->setToolTip(editor->getName());
    }
}

void DockedEditor::splitToRight(ScintillaNext *editor)
{
    Q_ASSERT(editor != Q_NULLPTR);

    ads::CDockWidget *newDockWidget = qobject_cast<ads::CDockWidget *>(editor->parentWidget());
    if (newDockWidget) {
        ads::CDockAreaWidget *currentArea = currentDockArea();
        if (currentArea) {
            dockManager->addDockWidget(ads::RightDockWidgetArea, newDockWidget, currentArea);
        }
    }
}

void DockedEditor::splitToBottom(ScintillaNext *editor)
{
    Q_ASSERT(editor != Q_NULLPTR);

    ads::CDockWidget *newDockWidget = qobject_cast<ads::CDockWidget *>(editor->parentWidget());
    if (newDockWidget) {
        ads::CDockAreaWidget *currentArea = currentDockArea();
        if (currentArea) {
            dockManager->addDockWidget(ads::BottomDockWidgetArea, newDockWidget, currentArea);
        }
    }
}
