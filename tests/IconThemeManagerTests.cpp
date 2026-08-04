/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "IconThemeManager.h"

#include <QImage>
#include <QPixmap>
#include <QtTest>

class IconThemeManagerTests final : public QObject
{
    Q_OBJECT

private slots:
    void namesRoundTrip();
    void invalidValuesFallBackToDefault();
    void semanticColorsMatchPack();
    void recoloringPreservesTransparency();
};

void IconThemeManagerTests::namesRoundTrip()
{
    const QList<IconThemeManager::Pack> packs = {
        IconThemeManager::Pack::Default,
        IconThemeManager::Pack::Nord,
        IconThemeManager::Pack::Catppuccin,
        IconThemeManager::Pack::GitHubDark,
    };

    for (const IconThemeManager::Pack pack : packs) {
        QCOMPARE(IconThemeManager::packFromName(IconThemeManager::name(pack)), pack);
        QCOMPARE(IconThemeManager::packFromValue(IconThemeManager::value(pack)), pack);
    }

    QCOMPARE(IconThemeManager::packFromName(QStringLiteral("nOrD")), IconThemeManager::Pack::Nord);
    QCOMPARE(IconThemeManager::packFromName(QStringLiteral("github dark")), IconThemeManager::Pack::GitHubDark);
}

void IconThemeManagerTests::invalidValuesFallBackToDefault()
{
    QCOMPARE(IconThemeManager::packFromValue(-1), IconThemeManager::Pack::Default);
    QCOMPARE(IconThemeManager::packFromValue(99), IconThemeManager::Pack::Default);
    QCOMPARE(IconThemeManager::packFromName(QStringLiteral("unknown")), IconThemeManager::Pack::Default);
}

void IconThemeManagerTests::semanticColorsMatchPack()
{
    const IconThemeManager::Colors nord = IconThemeManager::colors(IconThemeManager::Pack::Nord);
    const IconThemeManager::Colors catppuccin = IconThemeManager::colors(IconThemeManager::Pack::Catppuccin);
    const IconThemeManager::Colors github = IconThemeManager::colors(IconThemeManager::Pack::GitHubDark);

    QCOMPARE(IconThemeManager::colorForKey(IconThemeManager::Pack::Nord, QStringLiteral("actionSave")), nord.positive);
    QCOMPARE(IconThemeManager::colorForKey(IconThemeManager::Pack::Catppuccin, QStringLiteral("actionCloseFile")), catppuccin.destructive);
    QCOMPARE(IconThemeManager::colorForKey(IconThemeManager::Pack::GitHubDark, QStringLiteral("unsaved")), github.warning);
}

void IconThemeManagerTests::recoloringPreservesTransparency()
{
    QImage image(QSize(3, 3), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    image.setPixelColor(1, 1, QColor(QStringLiteral("#ffffff")));

    const QIcon source(QPixmap::fromImage(image));
    const QIcon themed = IconThemeManager::recolor(source, IconThemeManager::Pack::Nord, QStringLiteral("actionFind"));
    const QImage result = themed.pixmap(QSize(3, 3)).toImage().convertToFormat(QImage::Format_ARGB32);

    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(result.pixelColor(1, 1), IconThemeManager::colors(IconThemeManager::Pack::Nord).accent);
}

QTEST_MAIN(IconThemeManagerTests)
#include "IconThemeManagerTests.moc"
