/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STICKYSCROLL_H
#define STICKYSCROLL_H

#include <QWidget>
#include <QVector>

#include "ScintillaTypes.h"

class ScintillaNext;

class StickyScroll final : public QWidget
{
    Q_OBJECT

public:
    explicit StickyScroll(ScintillaNext *editor);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void refresh();
    void editorUpdated(Scintilla::Update updated);

private:
    QVector<QString> currentLabels() const;
    void updateGeometryForViewport();

    ScintillaNext *editor;
    QVector<QString> labels;
};

#endif // STICKYSCROLL_H
