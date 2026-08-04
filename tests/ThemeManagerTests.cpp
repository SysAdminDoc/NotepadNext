/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "ThemeManager.h"

#include <QtTest>

class ThemeManagerTests final : public QObject
{
    Q_OBJECT

private slots:
    void namesRoundTrip();
    void invalidValuesFallBackToFusion();
    void palettesExposeDistinctAccents();
    void styleSheetsExposeVariantOverrides();
};

void ThemeManagerTests::namesRoundTrip()
{
    const QList<ThemeManager::Variant> variants = {
        ThemeManager::Variant::Fusion,
        ThemeManager::Variant::Material,
        ThemeManager::Variant::Fluent,
    };

    for (const ThemeManager::Variant variant : variants) {
        QCOMPARE(ThemeManager::variantFromName(ThemeManager::name(variant)), variant);
        QCOMPARE(ThemeManager::variantFromValue(ThemeManager::value(variant)), variant);
    }

    QCOMPARE(ThemeManager::variantFromName(QStringLiteral("material")), ThemeManager::Variant::Material);
    QCOMPARE(ThemeManager::variantFromName(QStringLiteral("FLUENT")), ThemeManager::Variant::Fluent);
}

void ThemeManagerTests::invalidValuesFallBackToFusion()
{
    QCOMPARE(ThemeManager::variantFromValue(-1), ThemeManager::Variant::Fusion);
    QCOMPARE(ThemeManager::variantFromValue(99), ThemeManager::Variant::Fusion);
    QCOMPARE(ThemeManager::variantFromName(QStringLiteral("unknown")), ThemeManager::Variant::Fusion);
}

void ThemeManagerTests::palettesExposeDistinctAccents()
{
    const QPalette base;
    const QPalette fusion = ThemeManager::palette(ThemeManager::Variant::Fusion, base);
    const QPalette material = ThemeManager::palette(ThemeManager::Variant::Material, base);
    const QPalette fluent = ThemeManager::palette(ThemeManager::Variant::Fluent, base);

    QCOMPARE(fusion, base);
    QCOMPARE(material.color(QPalette::Highlight), QColor(QStringLiteral("#6750a4")));
    QCOMPARE(fluent.color(QPalette::Highlight), QColor(QStringLiteral("#60cdff")));
    QVERIFY(material.color(QPalette::Window) != fluent.color(QPalette::Window));
}

void ThemeManagerTests::styleSheetsExposeVariantOverrides()
{
    QVERIFY(ThemeManager::styleSheet(ThemeManager::Variant::Fusion).isEmpty());

    const QString material = ThemeManager::styleSheet(ThemeManager::Variant::Material);
    const QString fluent = ThemeManager::styleSheet(ThemeManager::Variant::Fluent);

    QVERIFY(material.contains(QStringLiteral("#6750a4")));
    QVERIFY(material.contains(QStringLiteral("ads--CDockWidgetTab")));
    QVERIFY(fluent.contains(QStringLiteral("#60cdff")));
    QVERIFY(fluent.contains(QStringLiteral("#202020")));
}

QTEST_GUILESS_MAIN(ThemeManagerTests)
#include "ThemeManagerTests.moc"
