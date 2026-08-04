/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MINIMAP_H
#define MINIMAP_H

#include <QSet>
#include <QWidget>

#include "ScintillaTypes.h"

class ScintillaNext;

namespace Scintilla
{
struct NotificationData;
}

class Minimap final : public QWidget
{
    Q_OBJECT

public:
    explicit Minimap(ScintillaNext *editor);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void editorUpdated(Scintilla::Update updated);
    void editorModified(const Scintilla::NotificationData *notification);
    void savePointChanged(bool dirty);
    void editorReloaded();

private:
    void updateGeometryForViewport();
    void markModifiedLines(const Scintilla::NotificationData *notification);
    int yForLine(int line) const;

    ScintillaNext *editor;
    QSet<int> modifiedLines;
};

#endif // MINIMAP_H
