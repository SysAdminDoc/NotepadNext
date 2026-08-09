/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "LspClient.h"

#include <algorithm>
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
    const QStringList arguments = QCoreApplication::arguments();
    const bool outOfOrderHover = arguments.contains(QStringLiteral("--out-of-order-hover"));
    const bool dropHover = arguments.contains(QStringLiteral("--drop-hover"));
    QJsonObject delayedHover;

    const auto writeHoverResponse = [](const QJsonObject &request) {
        writeFakeMessage(QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), request.value(QStringLiteral("id"))},
            {QStringLiteral("result"), QJsonObject{
                {QStringLiteral("contents"), QJsonObject{
                    {QStringLiteral("kind"), QStringLiteral("markdown")},
                    {QStringLiteral("value"), QStringLiteral("fake hover")},
                }},
            }},
        });
    };

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
                const QJsonObject textDocument = params.value(QStringLiteral("textDocument")).toObject();
                const QString uri = textDocument.value(QStringLiteral("uri")).toString();
                writeFakeMessage(QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("method"), QStringLiteral("textDocument/publishDiagnostics")},
                    {QStringLiteral("params"), QJsonObject{
                        {QStringLiteral("uri"), uri},
                        {QStringLiteral("version"), textDocument.value(QStringLiteral("version"))},
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
                if (dropHover) {
                    continue;
                }
                if (outOfOrderHover && delayedHover.isEmpty()) {
                    delayedHover = message;
                    continue;
                }
                writeHoverResponse(message);
                if (!delayedHover.isEmpty()) {
                    writeHoverResponse(delayedHover);
                    delayedHover = {};
                }
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
    void supportsMultipleDocuments();
    void cancelsStaleRequestsAndTimesOut();
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
    QCOMPARE(hoverSpy.at(0).at(1).toInt(), 2);
    QCOMPARE(qvariant_cast<LspPosition>(hoverSpy.at(0).at(2)).character, 1);
    QCOMPARE(hoverSpy.at(0).at(3).toString(), QStringLiteral("fake hover"));

    client.requestDefinition(uri, {0, 1});
    QTRY_COMPARE_WITH_TIMEOUT(definitionSpy.count(), 1, 3000);
    QCOMPARE(definitionSpy.at(0).at(0).toString(), uri);
    QCOMPARE(definitionSpy.at(0).at(1).toInt(), 2);
    QCOMPARE(definitionSpy.at(0).at(2).toString(), uri);
    QCOMPARE(qvariant_cast<LspRange>(definitionSpy.at(0).at(3)).start.character, 0);
    QCOMPARE(errorSpy.count(), 0);

    client.stop();
    QVERIFY(!client.isRunning());
}

void LspProtocolTests::supportsMultipleDocuments()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QTemporaryDir otherRoot;
    QVERIFY(otherRoot.isValid());

    LspClient client(QCoreApplication::applicationFilePath(), {QStringLiteral("--fake-lsp-server")});
    QSignalSpy initializedSpy(&client, &LspClient::initialized);
    QSignalSpy diagnosticsSpy(&client, &LspClient::diagnosticsReady);
    const QString firstUri = QUrl::fromLocalFile(root.filePath(QStringLiteral("first.py"))).toString(QUrl::FullyEncoded);
    const QString secondUri = QUrl::fromLocalFile(root.filePath(QStringLiteral("second.py"))).toString(QUrl::FullyEncoded);

    QVERIFY(client.start(root.path()));
    client.openDocument(firstUri, QStringLiteral("python"), QByteArrayLiteral("one\n"), 1, root.path());
    client.openDocument(secondUri, QStringLiteral("python"), QByteArrayLiteral("two\n"), 1, root.path());
    QTRY_COMPARE_WITH_TIMEOUT(diagnosticsSpy.count(), 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 3000);
    QCOMPARE(client.documentCount(), 2);

    client.changeDocument(firstUri, QByteArrayLiteral("changed\n"), 2);
    QTRY_COMPARE_WITH_TIMEOUT(diagnosticsSpy.count(), 3, 3000);
    QCOMPARE(diagnosticsSpy.at(2).at(0).toString(), firstUri);
    QCOMPARE(diagnosticsSpy.at(2).at(1).toInt(), 2);

    client.closeDocument(firstUri);
    QCOMPARE(client.documentCount(), 1);

    const QString otherUri = QUrl::fromLocalFile(otherRoot.filePath(QStringLiteral("other.py"))).toString(QUrl::FullyEncoded);
    client.openDocument(otherUri, QStringLiteral("python"), QByteArrayLiteral("other\n"), 1, otherRoot.path());
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 2, 3000);
    QCOMPARE(client.documentCount(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(diagnosticsSpy.cbegin(), diagnosticsSpy.cend(),
                                         [&otherUri](const QList<QVariant> &arguments) {
        return !arguments.isEmpty() && arguments.first().toString() == otherUri;
    }), 3000);
    client.stop();
}

void LspProtocolTests::cancelsStaleRequestsAndTimesOut()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString uri = QUrl::fromLocalFile(root.filePath(QStringLiteral("main.py"))).toString(QUrl::FullyEncoded);

    LspClient outOfOrder(QCoreApplication::applicationFilePath(),
                         {QStringLiteral("--fake-lsp-server"), QStringLiteral("--out-of-order-hover")});
    QSignalSpy initializedSpy(&outOfOrder, &LspClient::initialized);
    QSignalSpy hoverSpy(&outOfOrder, &LspClient::hoverReady);
    QVERIFY(outOfOrder.start(root.path()));
    outOfOrder.openDocument(uri, QStringLiteral("python"), QByteArrayLiteral("abc\n"), 1, root.path());
    QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, 3000);

    outOfOrder.requestHover(uri, {0, 1}, 1);
    outOfOrder.requestHover(uri, {0, 2}, 1);
    QTRY_COMPARE_WITH_TIMEOUT(hoverSpy.count(), 1, 3000);
    QCOMPARE(qvariant_cast<LspPosition>(hoverSpy.at(0).at(2)).character, 2);
    outOfOrder.stop();

    LspClient timeoutClient(QCoreApplication::applicationFilePath(),
                            {QStringLiteral("--fake-lsp-server"), QStringLiteral("--drop-hover")});
    QSignalSpy timeoutInitializedSpy(&timeoutClient, &LspClient::initialized);
    QSignalSpy errorSpy(&timeoutClient, &LspClient::serverError);
    QVERIFY(timeoutClient.start(root.path()));
    timeoutClient.openDocument(uri, QStringLiteral("python"), QByteArrayLiteral("abc\n"), 1, root.path());
    QTRY_COMPARE_WITH_TIMEOUT(timeoutInitializedSpy.count(), 1, 3000);
    timeoutClient.requestHover(uri, {0, 1}, 1);
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(errorSpy.cbegin(), errorSpy.cend(), [](const QList<QVariant> &arguments) {
        return !arguments.isEmpty() && arguments.first().toString().contains(QStringLiteral("timed out"));
    }), LspClient::RequestTimeoutMs + 2000);
    timeoutClient.stop();
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
