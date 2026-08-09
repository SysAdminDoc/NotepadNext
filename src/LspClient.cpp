/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LspClient.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QProcess>
#include <QUrl>

namespace
{
QString normalizedLanguage(const QString &language)
{
    return language.trimmed().toLower();
}

QJsonObject positionObject(const LspPosition &position)
{
    return QJsonObject{
        {QStringLiteral("line"), position.line},
        {QStringLiteral("character"), position.character},
    };
}

LspPosition parsePosition(const QJsonObject &object)
{
    LspPosition position;
    position.line = qMax(0, object.value(QStringLiteral("line")).toInt());
    position.character = qMax(0, object.value(QStringLiteral("character")).toInt());
    return position;
}

LspRange parseRange(const QJsonObject &object)
{
    return {
        parsePosition(object.value(QStringLiteral("start")).toObject()),
        parsePosition(object.value(QStringLiteral("end")).toObject()),
    };
}
}

QByteArray LspMessageParser::encode(const QJsonObject &message)
{
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
    return QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size()) +
           QByteArrayLiteral("\r\n\r\n") + body;
}

void LspMessageParser::reset()
{
    buffer.clear();
    errorText.clear();
    failed = false;
}

void LspMessageParser::fail(const QString &message)
{
    failed = true;
    errorText = message;
    buffer.clear();
}

QList<QJsonObject> LspMessageParser::consume(const QByteArray &data)
{
    QList<QJsonObject> messages;
    if (failed) {
        return messages;
    }

    buffer.append(data);

    while (true) {
        const int headerEnd = buffer.indexOf(QByteArrayLiteral("\r\n\r\n"));
        if (headerEnd < 0) {
            if (buffer.size() > 64 * 1024) {
                fail(QStringLiteral("LSP header exceeds 64 KiB"));
            }
            break;
        }

        const QByteArray header = buffer.left(headerEnd);
        qint64 contentLength = -1;
        const QList<QByteArray> lines = header.split('\n');
        for (QByteArray line : lines) {
            line = line.trimmed();
            const int colon = line.indexOf(':');
            if (colon <= 0) {
                continue;
            }

            const QByteArray name = line.left(colon).trimmed().toLower();
            if (name == QByteArrayLiteral("content-length")) {
                bool ok = false;
                contentLength = line.mid(colon + 1).trimmed().toLongLong(&ok);
                if (!ok || contentLength < 0) {
                    fail(QStringLiteral("Invalid LSP Content-Length header"));
                    return messages;
                }
            }
        }

        if (contentLength < 0) {
            fail(QStringLiteral("LSP message is missing Content-Length"));
            return messages;
        }
        if (contentLength > MaximumMessageBytes) {
            fail(QStringLiteral("LSP message exceeds the 16 MiB limit"));
            return messages;
        }

        const qint64 messageEnd = static_cast<qint64>(headerEnd) + 4 + contentLength;
        if (buffer.size() < messageEnd) {
            break;
        }

        const QByteArray body = buffer.mid(headerEnd + 4, static_cast<qsizetype>(contentLength));
        buffer.remove(0, static_cast<qsizetype>(messageEnd));

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fail(QStringLiteral("Invalid JSON-RPC message: %1").arg(parseError.errorString()));
            return messages;
        }

        messages.append(document.object());
    }

    return messages;
}

LspClient::LspClient(QString program, QStringList arguments, QObject *parent)
    : QObject(parent), program(std::move(program)), arguments(std::move(arguments))
{
    requestTimer.setInterval(250);
    connect(&requestTimer, &QTimer::timeout, this, &LspClient::expireRequests);
}

LspClient::~LspClient()
{
    stop();
}

bool LspClient::configurationForLanguage(const QString &language,
                                         QString *languageId,
                                         QString *program,
                                         QStringList *arguments)
{
    const QString normalized = normalizedLanguage(language);
    QString selectedLanguageId;
    QString selectedProgram;
    QStringList selectedArguments;

    if (normalized == QStringLiteral("c") || normalized == QStringLiteral("c++") ||
        normalized == QStringLiteral("cpp") || normalized == QStringLiteral("objective-c") ||
        normalized == QStringLiteral("objective-c++")) {
        selectedLanguageId = normalized == QStringLiteral("c") ? QStringLiteral("c") : QStringLiteral("cpp");
        selectedProgram = QStringLiteral("clangd");
        selectedArguments = {QStringLiteral("--header-insertion=never")};
    }
    else if (normalized == QStringLiteral("python")) {
        selectedLanguageId = QStringLiteral("python");
        selectedProgram = QStringLiteral("pyright-langserver");
        selectedArguments = {QStringLiteral("--stdio")};
    }
    else if (normalized == QStringLiteral("rust")) {
        selectedLanguageId = QStringLiteral("rust");
        selectedProgram = QStringLiteral("rust-analyzer");
    }
    else if (normalized == QStringLiteral("go")) {
        selectedLanguageId = QStringLiteral("go");
        selectedProgram = QStringLiteral("gopls");
        selectedArguments = {QStringLiteral("serve")};
    }
    else {
        return false;
    }

    if (languageId) {
        *languageId = selectedLanguageId;
    }
    if (program) {
        *program = selectedProgram;
    }
    if (arguments) {
        *arguments = selectedArguments;
    }
    return true;
}

bool LspClient::start(const QString &newRootPath)
{
    const QString normalizedRootPath = newRootPath.isEmpty()
        ? QString()
        : QFileInfo(newRootPath).absoluteFilePath();
    const bool rootChanged = !rootPath.isEmpty() && rootPath != normalizedRootPath;
    if (rootChanged) {
        stop();
        documents.clear();
        pendingRequests.clear();
        requestTimer.stop();
    }
    rootPath = normalizedRootPath;
    if (program.isEmpty()) {
        emit serverError(QStringLiteral("LSP server command is empty"));
        return false;
    }

    if (!process) {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::SeparateChannels);
        connect(process, &QProcess::started, this, &LspClient::sendInitialize);
        connect(process, &QProcess::readyReadStandardOutput, this, &LspClient::readStandardOutput);
        connect(process, &QProcess::readyReadStandardError, this, &LspClient::readStandardError);
        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                emit serverError(QStringLiteral("Unable to start LSP server %1").arg(program));
            }
            else {
                emit serverError(QStringLiteral("LSP server %1 reported process error %2").arg(program).arg(static_cast<int>(error)));
            }
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int exitCode, QProcess::ExitStatus) {
            initializationComplete = false;
            pendingRequests.clear();
            requestTimer.stop();
            const QString message = stopping
                ? QStringLiteral("LSP server stopped")
                : QStringLiteral("LSP server exited unexpectedly with code %1").arg(exitCode);
            emit statusChanged(message);
            emit stopped(exitCode);
            stopping = false;
        });
    }

    if (process->state() == QProcess::NotRunning) {
        stopping = false;
        parser.reset();
        initializationComplete = false;
        pendingRequests.clear();
        requestTimer.stop();
        process->start(program, arguments);
        emit statusChanged(QStringLiteral("Starting LSP server %1").arg(program));
    }

    return true;
}

void LspClient::stop()
{
    if (!process) {
        return;
    }

    stopping = true;
    const QList<int> requestIds = pendingRequests.keys();
    for (const int id : requestIds) {
        cancelRequest(id);
    }
    if (initializationComplete) {
        const QStringList uris = documents.keys();
        for (const QString &uri : uris) {
            sendNotification(QStringLiteral("textDocument/didClose"), QJsonObject{
                {QStringLiteral("textDocument"), QJsonObject{{QStringLiteral("uri"), uri}}},
            });
        }
    }

    initializationComplete = false;
    pendingRequests.clear();
    requestTimer.stop();

    if (process->state() != QProcess::NotRunning) {
        process->terminate();
        if (!process->waitForFinished(500)) {
            process->kill();
            process->waitForFinished(500);
        }
    }
}

bool LspClient::isRunning() const
{
    return process && process->state() != QProcess::NotRunning;
}

void LspClient::openDocument(const QString &uri,
                             const QString &languageId,
                             const QByteArray &text,
                             int version,
                             const QString &newRootPath)
{
    if (uri.isEmpty()) {
        return;
    }

    if ((!isRunning() || rootPath != QFileInfo(newRootPath).absoluteFilePath()) && !start(newRootPath)) {
        return;
    }

    const bool wasOpen = documents.contains(uri);
    documents.insert(uri, {uri, languageId, text, version});

    if (initializationComplete) {
        const DocumentState &document = documents.constFind(uri).value();
        if (wasOpen) {
            cancelRequestsForDocument(uri);
            sendDidChange(document);
        }
        else {
            sendDidOpen(document);
        }
    }
}

void LspClient::changeDocument(const QString &uri, const QByteArray &text, int version)
{
    auto it = documents.find(uri);
    if (!initializationComplete || it == documents.end()) {
        return;
    }

    cancelRequestsForDocument(uri);
    it->text = text;
    it->version = version;
    sendDidChange(it.value());
}

void LspClient::saveDocument(const QString &uri)
{
    if (!initializationComplete || !documents.contains(uri)) {
        return;
    }

    sendNotification(QStringLiteral("textDocument/didSave"), QJsonObject{
        {QStringLiteral("textDocument"), QJsonObject{{QStringLiteral("uri"), uri}}},
    });
}

void LspClient::closeDocument(const QString &uri)
{
    if (!documents.contains(uri)) {
        return;
    }

    cancelRequestsForDocument(uri);

    if (initializationComplete) {
        sendNotification(QStringLiteral("textDocument/didClose"), QJsonObject{
            {QStringLiteral("textDocument"), QJsonObject{{QStringLiteral("uri"), uri}}},
        });
    }
    documents.remove(uri);
}

void LspClient::requestHover(const QString &uri, const LspPosition &position, int documentVersion)
{
    const auto it = documents.constFind(uri);
    if (!initializationComplete || it == documents.constEnd()) {
        return;
    }

    cancelRequestsForDocument(uri, RequestType::Hover);

    const QJsonObject params{
        {QStringLiteral("textDocument"), QJsonObject{{QStringLiteral("uri"), uri}}},
        {QStringLiteral("position"), positionObject(position)},
    };
    const int version = documentVersion >= 0 ? documentVersion : it->version;
    sendRequest(QStringLiteral("textDocument/hover"), params, RequestType::Hover, uri, position, version);
}

void LspClient::requestDefinition(const QString &uri, const LspPosition &position, int documentVersion)
{
    const auto it = documents.constFind(uri);
    if (!initializationComplete || it == documents.constEnd()) {
        return;
    }

    cancelRequestsForDocument(uri, RequestType::Definition);

    const QJsonObject params{
        {QStringLiteral("textDocument"), QJsonObject{{QStringLiteral("uri"), uri}}},
        {QStringLiteral("position"), positionObject(position)},
    };
    const int version = documentVersion >= 0 ? documentVersion : it->version;
    sendRequest(QStringLiteral("textDocument/definition"), params, RequestType::Definition, uri, position, version);
}

int LspClient::sendRequest(const QString &method, const QJsonObject &params, RequestType type,
                           const QString &uri, const LspPosition &position, int documentVersion)
{
    if (!process || process->state() == QProcess::NotRunning) {
        return -1;
    }

    const int id = nextRequestId++;
    pendingRequests.insert(id, {type, method, uri, position, documentVersion,
                                QDateTime::currentMSecsSinceEpoch() + RequestTimeoutMs});

    const QJsonObject request{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
    };
    process->write(LspMessageParser::encode(request));
    if (!requestTimer.isActive()) {
        requestTimer.start();
    }
    return id;
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }

    const QJsonObject notification{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
    };
    process->write(LspMessageParser::encode(notification));
}

void LspClient::sendResponse(const QJsonValue &id, const QJsonValue &result)
{
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }

    const QJsonObject response{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("result"), result},
    };
    process->write(LspMessageParser::encode(response));
}

void LspClient::sendInitialize()
{
    const QString rootUri = rootPath.isEmpty()
        ? QString()
        : QUrl::fromLocalFile(QFileInfo(rootPath).absoluteFilePath()).toString(QUrl::FullyEncoded);

    const QJsonObject capabilities{
        {QStringLiteral("textDocument"), QJsonObject{
            {QStringLiteral("synchronization"), QJsonObject{
                {QStringLiteral("dynamicRegistration"), false},
                {QStringLiteral("willSave"), false},
                {QStringLiteral("didSave"), true},
            }},
            {QStringLiteral("hover"), QJsonObject{{QStringLiteral("dynamicRegistration"), false}}},
            {QStringLiteral("definition"), QJsonObject{{QStringLiteral("dynamicRegistration"), false}}},
        }},
        {QStringLiteral("workspace"), QJsonObject{{QStringLiteral("workspaceFolders"), true}}},
    };

    QJsonObject params{
        {QStringLiteral("processId"), static_cast<int>(QCoreApplication::applicationPid())},
        {QStringLiteral("clientInfo"), QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Notepad Next")},
            {QStringLiteral("version"), QCoreApplication::applicationVersion()},
        }},
        {QStringLiteral("rootUri"), rootUri.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(rootUri)},
        {QStringLiteral("capabilities"), capabilities},
    };

    if (!rootUri.isEmpty()) {
        params.insert(QStringLiteral("workspaceFolders"), QJsonArray{
            QJsonObject{{QStringLiteral("uri"), rootUri}, {QStringLiteral("name"), QFileInfo(rootPath).fileName()}},
        });
    }

    sendRequest(QStringLiteral("initialize"), params, RequestType::Initialize);
}

void LspClient::sendDidOpen(const DocumentState &document)
{
    sendNotification(QStringLiteral("textDocument/didOpen"), QJsonObject{
        {QStringLiteral("textDocument"), QJsonObject{
            {QStringLiteral("uri"), document.uri},
            {QStringLiteral("languageId"), document.languageId},
            {QStringLiteral("version"), document.version},
            {QStringLiteral("text"), QString::fromUtf8(document.text)},
        }},
    });
}

void LspClient::sendDidChange(const DocumentState &document)
{
    sendNotification(QStringLiteral("textDocument/didChange"), QJsonObject{
        {QStringLiteral("textDocument"), QJsonObject{
            {QStringLiteral("uri"), document.uri},
            {QStringLiteral("version"), document.version},
        }},
        {QStringLiteral("contentChanges"), QJsonArray{
            QJsonObject{{QStringLiteral("text"), QString::fromUtf8(document.text)}},
        }},
    });
}

void LspClient::readStandardOutput()
{
    const QList<QJsonObject> messages = parser.consume(process->readAllStandardOutput());
    if (parser.hasError()) {
        emit serverError(parser.errorString());
        process->kill();
        return;
    }

    for (const QJsonObject &message : messages) {
        handleMessage(message);
    }
}

void LspClient::readStandardError()
{
    const QByteArray output = process->readAllStandardError().trimmed();
    if (!output.isEmpty()) {
        qWarning("LSP server %s: %s", qUtf8Printable(program), output.constData());
    }
}

void LspClient::handleMessage(const QJsonObject &message)
{
    const QString method = message.value(QStringLiteral("method")).toString();
    if (!method.isEmpty()) {
        if (message.contains(QStringLiteral("id"))) {
            QJsonValue result = QJsonValue(QJsonValue::Null);
            if (method == QStringLiteral("workspace/configuration")) {
                result = QJsonArray();
            }
            else if (method == QStringLiteral("workspace/workspaceFolders")) {
                result = QJsonArray();
            }
            sendResponse(message.value(QStringLiteral("id")), result);
        }
        handleNotification(method, message.value(QStringLiteral("params")).toObject());
        return;
    }

    if (message.contains(QStringLiteral("id"))) {
        handleResponse(message);
    }
}

void LspClient::handleNotification(const QString &method, const QJsonObject &params)
{
    if (method == QStringLiteral("textDocument/publishDiagnostics")) {
        const QString uri = params.value(QStringLiteral("uri")).toString();
        const int documentVersion = params.contains(QStringLiteral("version"))
            ? params.value(QStringLiteral("version")).toInt(-1)
            : -1;
        QVector<LspDiagnostic> diagnostics;
        const QJsonArray values = params.value(QStringLiteral("diagnostics")).toArray();
        diagnostics.reserve(values.size());
        for (const QJsonValue &value : values) {
            const QJsonObject diagnosticObject = value.toObject();
            LspDiagnostic diagnostic;
            diagnostic.range = parseRange(diagnosticObject.value(QStringLiteral("range")).toObject());
            diagnostic.severity = qBound(1, diagnosticObject.value(QStringLiteral("severity")).toInt(1), 4);
            diagnostic.message = diagnosticObject.value(QStringLiteral("message")).toString();
            if (!diagnostic.message.isEmpty()) {
                diagnostics.append(diagnostic);
            }
        }
        emit diagnosticsReady(uri, documentVersion, diagnostics);
    }
    else if (method == QStringLiteral("window/showMessage") || method == QStringLiteral("window/logMessage")) {
        const QString messageText = params.value(QStringLiteral("message")).toString();
        if (!messageText.isEmpty()) {
            qInfo("LSP server %s: %s", qUtf8Printable(program), qUtf8Printable(messageText));
        }
    }
}

void LspClient::handleResponse(const QJsonObject &message)
{
    const int id = message.value(QStringLiteral("id")).toInt(-1);
    if (id < 0 || !pendingRequests.contains(id)) {
        return;
    }

    const PendingRequest request = pendingRequests.take(id);
    if (pendingRequests.isEmpty()) {
        requestTimer.stop();
    }

    if (request.type != RequestType::Initialize) {
        const auto documentIt = documents.constFind(request.uri);
        if (documentIt == documents.constEnd()) {
            return;
        }
        if (request.documentVersion >= 0 && documentIt->version != request.documentVersion) {
            return;
        }
    }

    if (message.contains(QStringLiteral("error"))) {
        const QJsonObject error = message.value(QStringLiteral("error")).toObject();
        emit serverError(QStringLiteral("LSP request failed: %1").arg(error.value(QStringLiteral("message")).toString()));
        return;
    }

    if (request.type == RequestType::Initialize) {
        initializationComplete = true;
        sendNotification(QStringLiteral("initialized"), QJsonObject());
        emit statusChanged(QStringLiteral("LSP server initialized for %1").arg(rootPath));
        emit LspClient::initialized();

        const QList<DocumentState> pendingDocuments = documents.values();
        for (const DocumentState &document : pendingDocuments) {
            sendDidOpen(document);
        }
        return;
    }

    if (request.type == RequestType::Hover) {
        const QJsonObject result = message.value(QStringLiteral("result")).toObject();
        QString text;
        if (!result.isEmpty()) {
            text = hoverText(result.value(QStringLiteral("contents")));
        }
        const auto documentIt = documents.constFind(request.uri);
        const int documentVersion = request.documentVersion >= 0
            ? request.documentVersion
            : (documentIt == documents.constEnd() ? -1 : documentIt->version);
        emit hoverReady(request.uri, documentVersion, request.position, text);
        return;
    }

    const QJsonValue result = message.value(QStringLiteral("result"));
    QString uri;
    LspRange range;
    bool found = false;
    if (result.isObject()) {
        found = parseLocation(result.toObject(), &uri, &range);
    }
    else if (result.isArray()) {
        for (const QJsonValue &value : result.toArray()) {
            if (parseLocation(value.toObject(), &uri, &range)) {
                found = true;
                break;
            }
        }
    }
    if (found) {
        const auto documentIt = documents.constFind(request.uri);
        const int documentVersion = request.documentVersion >= 0
            ? request.documentVersion
            : (documentIt == documents.constEnd() ? -1 : documentIt->version);
        emit definitionReady(request.uri, documentVersion, uri, range);
    }
}

void LspClient::cancelRequest(int id, bool notifyServer)
{
    if (!pendingRequests.contains(id)) {
        return;
    }

    pendingRequests.remove(id);
    if (notifyServer) {
        sendNotification(QStringLiteral("$/cancelRequest"), QJsonObject{{QStringLiteral("id"), id}});
    }
    if (pendingRequests.isEmpty()) {
        requestTimer.stop();
    }
}

void LspClient::cancelRequestsForDocument(const QString &uri)
{
    const QList<int> requestIds = pendingRequests.keys();
    for (const int id : requestIds) {
        const auto it = pendingRequests.constFind(id);
        if (it != pendingRequests.constEnd() && it->uri == uri) {
            cancelRequest(id);
        }
    }
}

void LspClient::cancelRequestsForDocument(const QString &uri, RequestType type)
{
    const QList<int> requestIds = pendingRequests.keys();
    for (const int id : requestIds) {
        const auto it = pendingRequests.constFind(id);
        if (it != pendingRequests.constEnd() && it->uri == uri && it->type == type) {
            cancelRequest(id);
        }
    }
}

void LspClient::expireRequests()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QList<int> requestIds = pendingRequests.keys();
    for (const int id : requestIds) {
        const auto it = pendingRequests.constFind(id);
        if (it == pendingRequests.constEnd() || it->deadlineMs > now) {
            continue;
        }

        const QString method = it->method;
        const bool initializeTimedOut = it->type == RequestType::Initialize;
        cancelRequest(id);
        const QString message = QStringLiteral("LSP request %1 timed out after %2 ms")
            .arg(method)
            .arg(RequestTimeoutMs);
        emit serverError(message);
        emit statusChanged(message);
        if (initializeTimedOut && process && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    }
}

QString LspClient::hoverText(const QJsonValue &contents) const
{
    if (contents.isString()) {
        return contents.toString();
    }
    if (contents.isObject()) {
        return contents.toObject().value(QStringLiteral("value")).toString();
    }
    if (!contents.isArray()) {
        return QString();
    }

    QStringList lines;
    for (const QJsonValue &value : contents.toArray()) {
        if (value.isString()) {
            lines.append(value.toString());
        }
        else if (value.isObject()) {
            const QString valueText = value.toObject().value(QStringLiteral("value")).toString();
            if (!valueText.isEmpty()) {
                lines.append(valueText);
            }
        }
    }
    return lines.join(QChar('\n'));
}

bool LspClient::parseLocation(const QJsonObject &location, QString *uri, LspRange *range) const
{
    QString locationUri = location.value(QStringLiteral("uri")).toString();
    QJsonObject locationRange = location.value(QStringLiteral("range")).toObject();
    if (locationUri.isEmpty() || locationRange.isEmpty()) {
        locationUri = location.value(QStringLiteral("targetUri")).toString();
        locationRange = location.value(QStringLiteral("targetSelectionRange")).toObject();
    }
    if (locationUri.isEmpty() || locationRange.isEmpty()) {
        return false;
    }

    if (uri) {
        *uri = locationUri;
    }
    if (range) {
        *range = parseRange(locationRange);
    }
    return true;
}
