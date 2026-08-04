/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "LspClient.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <iostream>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
void writeFakeMessage(const QJsonObject &message)
{
    const QByteArray frame = LspMessageParser::encode(message);
    std::cout.write(frame.constData(), static_cast<std::streamsize>(frame.size()));
    std::cout.flush();
}

int runFakeServer()
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    LspMessageParser parser;
    char character = 0;
    while (std::cin.get(character)) {
        const QList<QJsonObject> messages = parser.consume(QByteArray(1, character));
        for (const QJsonObject &message : messages) {
            const QString method = message.value(QStringLiteral("method")).toString();
            const QJsonValue id = message.value(QStringLiteral("id"));
            if (method == QStringLiteral("initialize")) {
                writeFakeMessage(QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{{QStringLiteral("capabilities"), QJsonObject{}}}},
                });
            }
            else if (method == QStringLiteral("textDocument/didOpen") ||
                     method == QStringLiteral("textDocument/didChange")) {
                const QJsonObject params = message.value(QStringLiteral("params")).toObject();
                const QString uri = params.value(QStringLiteral("textDocument")).toObject().value(QStringLiteral("uri")).toString();
                writeFakeMessage(QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("method"), QStringLiteral("textDocument/publishDiagnostics")},
                    {QStringLiteral("params"), QJsonObject{
                        {QStringLiteral("uri"), uri},
                        {QStringLiteral("diagnostics"), QJsonArray{
                            QJsonObject{
                                {QStringLiteral("range"), QJsonObject{
                                    {QStringLiteral("start"), QJsonObject{{QStringLiteral("line"), 0}, {QStringLiteral("character"), 0}}},
                                    {QStringLiteral("end"), QJsonObject{{QStringLiteral("line"), 0}, {QStringLiteral("character"), 2}}},
                                }},
                                {QStringLiteral("severity"), 2},
                                {QStringLiteral("message"), method == QStringLiteral("textDocument/didOpen")
                                    ? QStringLiteral("opened") : QStringLiteral("changed")},
                            },
                        }},
                    }},
                });
            }
            else if (method == QStringLiteral("textDocument/hover")) {
                writeFakeMessage(QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{
                        {QStringLiteral("contents"), QJsonObject{
                            {QStringLiteral("kind"), QStringLiteral("markdown")},
                            {QStringLiteral("value"), QStringLiteral("fake hover")},
                        }},
                    }},
                });
            }
            else if (method == QStringLiteral("textDocument/definition")) {
                const QString uri = message.value(QStringLiteral("params")).toObject()
                                         .value(QStringLiteral("textDocument")).toObject()
                                         .value(QStringLiteral("uri")).toString();
                writeFakeMessage(QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("result"), QJsonObject{
                        {QStringLiteral("uri"), uri},
                        {QStringLiteral("range"), QJsonObject{
                            {QStringLiteral("start"), QJsonObject{{QStringLiteral("line"), 0}, {QStringLiteral("character"), 0}}},
                            {QStringLiteral("end"), QJsonObject{{QStringLiteral("line"), 0}, {QStringLiteral("character"), 2}}},
                        }},
                    }},
                });
            }
        }
    }
    return parser.hasError() ? 2 : 0;
}
}

class LspProtocolTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesPartialAndCoalescedMessages();
    void rejectsMalformedMessages();
    void mapsSupportedLanguageServers();
    void exchangesDocumentAndLanguageRequests();
};

void LspProtocolTests::parsesPartialAndCoalescedMessages()
{
    const QByteArray first = LspMessageParser::encode(QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 1},
        {QStringLiteral("result"), QJsonObject{{QStringLiteral("ok"), true}}},
    });
    const QByteArray second = LspMessageParser::encode(QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), QStringLiteral("window/logMessage")},
        {QStringLiteral("params"), QJsonObject{{QStringLiteral("message"), QStringLiteral("ready")}}},
    });

    LspMessageParser parser;
    QVERIFY(parser.consume(first.left(7)).isEmpty());
    const QList<QJsonObject> messages = parser.consume(first.mid(7) + second);

    QCOMPARE(messages.size(), 2);
    QCOMPARE(messages.at(0).value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(messages.at(0).value(QStringLiteral("result")).toObject().value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(messages.at(1).value(QStringLiteral("method")).toString(), QStringLiteral("window/logMessage"));
    QVERIFY(!parser.hasError());
}

void LspProtocolTests::rejectsMalformedMessages()
{
    LspMessageParser missingLength;
    QVERIFY(missingLength.consume(QByteArrayLiteral("Content-Type: application/json\r\n\r\n{}" )).isEmpty());
    QVERIFY(missingLength.hasError());
    QVERIFY(missingLength.errorString().contains(QStringLiteral("Content-Length")));

    LspMessageParser invalidJson;
    QVERIFY(invalidJson.consume(QByteArrayLiteral("Content-Length: 8\r\n\r\n{broken}" )).isEmpty());
    QVERIFY(invalidJson.hasError());

    LspMessageParser oversized;
    const QByteArray oversizedHeader = QByteArrayLiteral("Content-Length: ") +
                                       QByteArray::number(LspMessageParser::MaximumMessageBytes + 1) +
                                       QByteArrayLiteral("\r\n\r\n");
    QVERIFY(oversized.consume(oversizedHeader).isEmpty());
    QVERIFY(oversized.hasError());
}

void LspProtocolTests::mapsSupportedLanguageServers()
{
    QString languageId;
    QString program;
    QStringList arguments;

    QVERIFY(LspClient::configurationForLanguage(QStringLiteral("C++"), &languageId, &program, &arguments));
    QCOMPARE(languageId, QStringLiteral("cpp"));
    QCOMPARE(program, QStringLiteral("clangd"));
    QVERIFY(arguments.contains(QStringLiteral("--header-insertion=never")));

    QVERIFY(LspClient::configurationForLanguage(QStringLiteral("Python"), &languageId, &program, &arguments));
    QCOMPARE(languageId, QStringLiteral("python"));
    QCOMPARE(program, QStringLiteral("pyright-langserver"));
    QCOMPARE(arguments, QStringList{QStringLiteral("--stdio")});

    QVERIFY(LspClient::configurationForLanguage(QStringLiteral("Rust"), &languageId, &program, &arguments));
    QCOMPARE(languageId, QStringLiteral("rust"));
    QCOMPARE(program, QStringLiteral("rust-analyzer"));

    QVERIFY(LspClient::configurationForLanguage(QStringLiteral("Go"), &languageId, &program, &arguments));
    QCOMPARE(languageId, QStringLiteral("go"));
    QCOMPARE(program, QStringLiteral("gopls"));
    QCOMPARE(arguments, QStringList{QStringLiteral("serve")});

    QVERIFY(!LspClient::configurationForLanguage(QStringLiteral("Markdown"), &languageId, &program, &arguments));
}

void LspProtocolTests::exchangesDocumentAndLanguageRequests()
{
    qRegisterMetaType<LspPosition>();
    qRegisterMetaType<LspRange>();
    qRegisterMetaType<QVector<LspDiagnostic>>();

    QTemporaryDir root;
    QVERIFY(root.isValid());

    LspClient client(QCoreApplication::applicationFilePath(), {QStringLiteral("--fake-lsp-server")});
    QSignalSpy initializedSpy(&client, &LspClient::initialized);
    QSignalSpy diagnosticsSpy(&client, &LspClient::diagnosticsReady);
    QSignalSpy hoverSpy(&client, &LspClient::hoverReady);
    QSignalSpy definitionSpy(&client, &LspClient::definitionReady);
    QSignalSpy errorSpy(&client, &LspClient::serverError);

    const QString uri = QUrl::fromLocalFile(root.filePath(QStringLiteral("main.py"))).toString(QUrl::FullyEncoded);
    QVERIFY(client.start(root.path()));
    client.openDocument(uri, QStringLiteral("python"), QByteArrayLiteral("ab\n"), 1, root.path());

    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(diagnosticsSpy.count(), 1, 3000);
    QCOMPARE(diagnosticsSpy.at(0).at(0).toString(), uri);

    client.changeDocument(uri, QByteArrayLiteral("abc\n"), 2);
    QTRY_COMPARE_WITH_TIMEOUT(diagnosticsSpy.count(), 2, 3000);

    client.requestHover(uri, {0, 1});
    QTRY_COMPARE_WITH_TIMEOUT(hoverSpy.count(), 1, 3000);
    QCOMPARE(hoverSpy.at(0).at(0).toString(), uri);
    QCOMPARE(hoverSpy.at(0).at(2).toString(), QStringLiteral("fake hover"));

    client.requestDefinition(uri, {0, 1});
    QTRY_COMPARE_WITH_TIMEOUT(definitionSpy.count(), 1, 3000);
    QCOMPARE(definitionSpy.at(0).at(0).toString(), uri);
    QCOMPARE(qvariant_cast<LspRange>(definitionSpy.at(0).at(1)).start.character, 0);
    QCOMPARE(errorSpy.count(), 0);

    client.stop();
    QVERIFY(!client.isRunning());
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains(QStringLiteral("--fake-lsp-server"))) {
        return runFakeServer();
    }
    LspProtocolTests test;
    return QTest::qExec(&test, argc, argv);
}

#include "LspProtocolTests.moc"
