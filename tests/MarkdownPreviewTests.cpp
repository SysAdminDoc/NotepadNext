/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "MarkdownRenderer.h"

#include <QtTest>

class MarkdownPreviewTests final : public QObject
{
    Q_OBJECT

private slots:
    void recognizesMarkdownLanguages();
    void rendersMarkdownToStructuredHtml();
    void escapesPlainText();
};

void MarkdownPreviewTests::recognizesMarkdownLanguages()
{
    QVERIFY(MarkdownRenderer::isMarkdownLanguage(QStringLiteral("Markdown")));
    QVERIFY(MarkdownRenderer::isMarkdownLanguage(QStringLiteral("mdx")));
    QVERIFY(!MarkdownRenderer::isMarkdownLanguage(QStringLiteral("JSON")));
}

void MarkdownPreviewTests::rendersMarkdownToStructuredHtml()
{
    const QString html = MarkdownRenderer::toHtml(
        QStringLiteral("# Title\n\nA **bold** [link](https://example.com).\n\n- one\n- two"),
        QColor(QStringLiteral("#202124")), QColor(QStringLiteral("#ffffff")));

    QVERIFY(html.contains(QStringLiteral("<h1")));
    QVERIFY(html.contains(QStringLiteral("Title")));
    QVERIFY(html.contains(QStringLiteral("bold")));
    QVERIFY(html.contains(QStringLiteral("href=\"https://example.com\"")));
    QVERIFY(html.contains(QStringLiteral("<ul")));
}

void MarkdownPreviewTests::escapesPlainText()
{
    const QString html = MarkdownRenderer::toHtml(QStringLiteral("<script>alert(1)</script>"), {}, {});

    QVERIFY(!html.contains(QStringLiteral("<script>alert(1)</script>")));
    QVERIFY(!html.contains(QStringLiteral("alert(1)")));
}

QTEST_MAIN(MarkdownPreviewTests)
#include "MarkdownPreviewTests.moc"
