/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include <QFont>
#include <QtTest>

class FontLigatureTests final : public QObject
{
    Q_OBJECT

private slots:
    void programmingOpenTypeFeaturesAreAvailable();
};

void FontLigatureTests::programmingOpenTypeFeaturesAreAvailable()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QFont font(QStringLiteral("Cascadia Code"));
    font.setFeature(QFont::Tag("liga"), 1);
    font.setFeature(QFont::Tag("clig"), 1);
    font.setFeature(QFont::Tag("calt"), 1);

    QCOMPARE(font.featureValue(QFont::Tag("liga")), quint32(1));
    QCOMPARE(font.featureValue(QFont::Tag("clig")), quint32(1));
    QCOMPARE(font.featureValue(QFont::Tag("calt")), quint32(1));
#else
    QSKIP("Explicit QFont OpenType feature control requires Qt 6.7 or newer; Qt's default shaping remains active.");
#endif
}

QTEST_GUILESS_MAIN(FontLigatureTests)
#include "FontLigatureTests.moc"
