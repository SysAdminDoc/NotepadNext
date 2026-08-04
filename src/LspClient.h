/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVector>

class QJsonObject;
class QJsonValue;
class QProcess;

struct LspPosition
{
    int line = 0;
    int character = 0;
};

struct LspRange
{
    LspPosition start;
    LspPosition end;
};

struct LspDiagnostic
{
    LspRange range;
    int severity = 1;
    QString message;
};

Q_DECLARE_METATYPE(LspPosition)
Q_DECLARE_METATYPE(LspRange)
Q_DECLARE_METATYPE(LspDiagnostic)
Q_DECLARE_METATYPE(QVector<LspDiagnostic>)

class LspMessageParser final
{
public:
    static constexpr int MaximumMessageBytes = 16 * 1024 * 1024;

    static QByteArray encode(const QJsonObject &message);

    QList<QJsonObject> consume(const QByteArray &data);
    bool hasError() const { return failed; }
    QString errorString() const { return errorText; }
    void reset();

private:
    void fail(const QString &message);

    QByteArray buffer;
    QString errorText;
    bool failed = false;
};

class LspClient final : public QObject
{
    Q_OBJECT

public:
    explicit LspClient(QString program, QStringList arguments, QObject *parent = nullptr);
    ~LspClient() override;

    // Returns the server command and LSP language id for a supported editor language.
    static bool configurationForLanguage(const QString &language,
                                         QString *languageId,
                                         QString *program,
                                         QStringList *arguments);

    bool start(const QString &rootPath);
    void stop();

    bool isRunning() const;
    bool isInitialized() const { return initializationComplete; }

    void openDocument(const QString &uri,
                      const QString &languageId,
                      const QByteArray &text,
                      int version,
                      const QString &rootPath);
    void changeDocument(const QString &uri, const QByteArray &text, int version);
    void saveDocument(const QString &uri);
    void closeDocument(const QString &uri);

    void requestHover(const QString &uri, const LspPosition &position);
    void requestDefinition(const QString &uri, const LspPosition &position);

signals:
    void initialized();
    void stopped(int exitCode);
    void diagnosticsReady(const QString &uri, const QVector<LspDiagnostic> &diagnostics);
    void hoverReady(const QString &uri, const LspPosition &position, const QString &text);
    void definitionReady(const QString &uri, const LspRange &range);
    void serverError(const QString &message);

private:
    enum class RequestType {
        Initialize,
        Hover,
        Definition,
    };

    struct PendingRequest
    {
        RequestType type;
        QString uri;
        LspPosition position;
    };

    struct DocumentState
    {
        QString uri;
        QString languageId;
        QByteArray text;
        int version = 0;
    };

    int sendRequest(const QString &method, const QJsonObject &params, RequestType type,
                    const QString &uri = QString(), const LspPosition &position = {});
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendResponse(const QJsonValue &id, const QJsonValue &result);
    void sendInitialize();
    void sendDidOpen(const DocumentState &document);
    void sendDidChange(const DocumentState &document);
    void handleMessage(const QJsonObject &message);
    void handleNotification(const QString &method, const QJsonObject &params);
    void handleResponse(const QJsonObject &message);
    void readStandardOutput();
    void readStandardError();
    QString hoverText(const QJsonValue &contents) const;
    bool parseLocation(const QJsonObject &location, QString *uri, LspRange *range) const;

    QString program;
    QStringList arguments;
    QString rootPath;
    QProcess *process = nullptr;
    LspMessageParser parser;

    int nextRequestId = 1;
    QHash<int, PendingRequest> pendingRequests;
    DocumentState document;
    DocumentState pendingDocument;
    bool hasDocument = false;
    bool hasPendingDocument = false;
    bool initializationComplete = false;
};

#endif // LSPCLIENT_H
