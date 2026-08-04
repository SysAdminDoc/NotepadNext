/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ScriptConsoleDock.h"

#include "MainWindow.h"
#include "NotepadNextApplication.h"
#include "ScriptConsoleBridge.h"

#include <quickjs.h>

#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace {

ScriptConsoleBridge *bridgeForContext(JSContext *context)
{
    return static_cast<ScriptConsoleBridge *>(JS_GetContextOpaque(context));
}

JSValue jsText(JSContext *context, JSValueConst, int, JSValueConst *)
{
    const QByteArray value = bridgeForContext(context) ? bridgeForContext(context)->text().toUtf8() : QByteArray();
    return JS_NewStringLen(context, value.constData(), static_cast<size_t>(value.size()));
}

bool stringArgument(JSContext *context, JSValueConst value, QString *result)
{
    size_t length = 0;
    const char *text = JS_ToCStringLen(context, &length, value);
    if (!text) {
        return false;
    }

    *result = QString::fromUtf8(text, static_cast<qsizetype>(length));
    JS_FreeCString(context, text);
    return true;
}

JSValue jsSetText(JSContext *context, JSValueConst, int argc, JSValueConst *argv)
{
    QString value;
    if (argc < 1 || !stringArgument(context, argv[0], &value)) {
        return JS_ThrowTypeError(context, "setText(value) expects a string");
    }

    if (ScriptConsoleBridge *bridge = bridgeForContext(context)) {
        bridge->setText(value);
    }
    return JS_UNDEFINED;
}

JSValue jsSelectedText(JSContext *context, JSValueConst, int, JSValueConst *)
{
    const QByteArray value = bridgeForContext(context) ? bridgeForContext(context)->selectedText().toUtf8() : QByteArray();
    return JS_NewStringLen(context, value.constData(), static_cast<size_t>(value.size()));
}

JSValue jsReplaceSelection(JSContext *context, JSValueConst, int argc, JSValueConst *argv)
{
    QString value;
    if (argc < 1 || !stringArgument(context, argv[0], &value)) {
        return JS_ThrowTypeError(context, "replaceSelection(value) expects a string");
    }

    if (ScriptConsoleBridge *bridge = bridgeForContext(context)) {
        bridge->replaceSelection(value);
    }
    return JS_UNDEFINED;
}

JSValue jsInsertText(JSContext *context, JSValueConst, int argc, JSValueConst *argv)
{
    QString value;
    if (argc < 1 || !stringArgument(context, argv[0], &value)) {
        return JS_ThrowTypeError(context, "insertText(value) expects a string");
    }

    if (ScriptConsoleBridge *bridge = bridgeForContext(context)) {
        bridge->insertText(value);
    }
    return JS_UNDEFINED;
}

JSValue jsFilePath(JSContext *context, JSValueConst, int, JSValueConst *)
{
    const QByteArray value = bridgeForContext(context) ? bridgeForContext(context)->filePath().toUtf8() : QByteArray();
    return JS_NewStringLen(context, value.constData(), static_cast<size_t>(value.size()));
}

JSValue jsSave(JSContext *context, JSValueConst, int, JSValueConst *)
{
    return JS_NewBool(context, bridgeForContext(context) && bridgeForContext(context)->save());
}

JSValue jsOpenFile(JSContext *context, JSValueConst, int argc, JSValueConst *argv)
{
    QString path;
    if (argc < 1 || !stringArgument(context, argv[0], &path)) {
        return JS_ThrowTypeError(context, "openFile(path) expects a string");
    }

    if (ScriptConsoleBridge *bridge = bridgeForContext(context)) {
        bridge->openFile(path);
    }
    return JS_UNDEFINED;
}

JSValue jsLog(JSContext *context, JSValueConst, int argc, JSValueConst *argv)
{
    QString value;
    if (argc < 1 || !stringArgument(context, argv[0], &value)) {
        return JS_ThrowTypeError(context, "log(value) expects a value");
    }

    if (ScriptConsoleBridge *bridge = bridgeForContext(context)) {
        bridge->log(value);
    }
    return JS_UNDEFINED;
}

void addBridgeFunction(JSContext *context, JSValue object, const char *name, JSCFunction *function, int argumentCount)
{
    JS_SetPropertyStr(context, object, name, JS_NewCFunction(context, function, name, argumentCount));
}

}

ScriptConsoleDock::ScriptConsoleDock(NotepadNextApplication *app, QWidget *parent)
    : QDockWidget(tr("Scripting Console"), parent)
    , app(app)
    , bridge(new ScriptConsoleBridge(parent ? qobject_cast<MainWindow *>(parent) : nullptr, this))
    , output(new QPlainTextEdit(this))
    , input(new QPlainTextEdit(this))
{
    setObjectName(QStringLiteral("scriptingConsoleDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    output->setObjectName(QStringLiteral("scriptConsoleOutput"));
    output->setAccessibleName(tr("JavaScript console output"));
    output->setAccessibleDescription(tr("Read-only output from JavaScript execution."));
    output->setReadOnly(true);
    output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    output->setMaximumBlockCount(2000);

    input->setObjectName(QStringLiteral("scriptConsoleInput"));
    input->setAccessibleName(tr("JavaScript console input"));
    input->setAccessibleDescription(tr("Enter JavaScript to run against the active document. Press Ctrl+Return to run."));
    input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    input->setPlaceholderText(tr("Use notepad.text(), notepad.setText(...), and notepad.log(...)."));
    input->setMinimumHeight(90);

    auto *languageLabel = new QLabel(tr("Engine: %1").arg(languageDescription()), this);
    languageLabel->setObjectName(QStringLiteral("scriptConsoleEngineLabel"));
    languageLabel->setAccessibleName(tr("JavaScript engine"));

    auto *runButton = new QPushButton(tr("Run"), this);
    runButton->setObjectName(QStringLiteral("scriptConsoleRun"));
    runButton->setAccessibleDescription(tr("Run the JavaScript in the input editor."));
    runButton->setDefault(true);
    auto *runFileButton = new QPushButton(tr("Run Script File..."), this);
    runFileButton->setObjectName(QStringLiteral("scriptConsoleRunFile"));
    runFileButton->setAccessibleDescription(tr("Choose and run a JavaScript file."));
    auto *clearButton = new QPushButton(tr("Clear"), this);
    clearButton->setObjectName(QStringLiteral("scriptConsoleClear"));
    clearButton->setAccessibleDescription(tr("Clear JavaScript console output."));

    auto *controls = new QHBoxLayout;
    controls->addWidget(languageLabel);
    controls->addStretch();
    controls->addWidget(runFileButton);
    controls->addWidget(clearButton);
    controls->addWidget(runButton);

    auto *layout = new QVBoxLayout;
    layout->addWidget(output, 1);
    layout->addLayout(controls);
    layout->addWidget(input);
    layout->setContentsMargins(4, 4, 4, 4);

    QWidget::setTabOrder(output, runFileButton);
    QWidget::setTabOrder(runFileButton, clearButton);
    QWidget::setTabOrder(clearButton, runButton);
    QWidget::setTabOrder(runButton, input);

    auto *container = new QWidget(this);
    container->setLayout(layout);
    setWidget(container);

    connect(runButton, &QPushButton::clicked, this, &ScriptConsoleDock::runScript);
    connect(runFileButton, &QPushButton::clicked, this, &ScriptConsoleDock::runScriptFile);
    connect(clearButton, &QPushButton::clicked, this, &ScriptConsoleDock::clearOutput);
    connect(bridge, &ScriptConsoleBridge::outputMessage, this, [this](const QString &message) {
        appendOutput(message);
    });

    auto *runShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), input);
    runShortcut->setObjectName(QStringLiteral("scriptConsoleRunShortcut"));
    connect(runShortcut, &QShortcut::activated, this, &ScriptConsoleDock::runScript);

    javascriptRuntime = JS_NewRuntime();
    if (javascriptRuntime) {
        JS_SetMemoryLimit(javascriptRuntime, 32 * 1024 * 1024);
        JS_SetMaxStackSize(javascriptRuntime, 1024 * 1024);
        JS_SetInterruptHandler(javascriptRuntime, ScriptConsoleDock::interruptHandler, this);
        javascriptContext = JS_NewContext(javascriptRuntime);
        if (javascriptContext) {
            JS_SetContextOpaque(javascriptContext, bridge);

            JSValue global = JS_GetGlobalObject(javascriptContext);
            JSValue notepad = JS_NewObject(javascriptContext);
            addBridgeFunction(javascriptContext, notepad, "text", jsText, 0);
            addBridgeFunction(javascriptContext, notepad, "setText", jsSetText, 1);
            addBridgeFunction(javascriptContext, notepad, "selectedText", jsSelectedText, 0);
            addBridgeFunction(javascriptContext, notepad, "replaceSelection", jsReplaceSelection, 1);
            addBridgeFunction(javascriptContext, notepad, "insertText", jsInsertText, 1);
            addBridgeFunction(javascriptContext, notepad, "filePath", jsFilePath, 0);
            addBridgeFunction(javascriptContext, notepad, "save", jsSave, 0);
            addBridgeFunction(javascriptContext, notepad, "openFile", jsOpenFile, 1);
            addBridgeFunction(javascriptContext, notepad, "log", jsLog, 1);
            JS_SetPropertyStr(javascriptContext, global, "notepad", notepad);
            JS_FreeValue(javascriptContext, global);

            const QByteArray bootstrap = QByteArrayLiteral(
                "function print(value) { notepad.log(String(value)); }\n"
                "var console = { log: function(value) { notepad.log(String(value)); } };");
            JSValue bootstrapResult = JS_Eval(javascriptContext, bootstrap.constData(), static_cast<size_t>(bootstrap.size()),
                                              "<bootstrap>", JS_EVAL_TYPE_GLOBAL);
            JS_FreeValue(javascriptContext, bootstrapResult);
        }
    }

    appendOutput(tr("Ready. Run a script with Ctrl+Enter."));
}

ScriptConsoleDock::~ScriptConsoleDock()
{
    if (javascriptContext) {
        JS_FreeContext(javascriptContext);
    }
    if (javascriptRuntime) {
        JS_FreeRuntime(javascriptRuntime);
    }
}

QString ScriptConsoleDock::languageDescription()
{
    return QStringLiteral("JavaScript (QuickJS-NG)");
}

int ScriptConsoleDock::interruptHandler(JSRuntime *, void *opaque)
{
    const auto *dock = static_cast<const ScriptConsoleDock *>(opaque);
    return dock->executionTimer.isValid() && dock->executionTimer.elapsed() > 2000 ? 1 : 0;
}

void ScriptConsoleDock::runScript()
{
    const QString script = input->toPlainText();
    if (script.trimmed().isEmpty()) {
        return;
    }

    input->clear();
    executeScript(script);
}

void ScriptConsoleDock::runScriptFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Run Script File"),
        QString(),
        tr("JavaScript (*.js *.mjs);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        appendOutput(tr("Unable to open %1: %2").arg(path, file.errorString()));
        return;
    }

    executeScript(QString::fromUtf8(file.readAll()), path);
}

void ScriptConsoleDock::clearOutput()
{
    output->clear();
}

void ScriptConsoleDock::focusInput()
{
    show();
    raise();
    input->setFocus();
}

void ScriptConsoleDock::executeScript(const QString &script, const QString &sourceName)
{
    appendOutput(QStringLiteral(">>> %1").arg(script.trimmed()));

    if (!javascriptContext) {
        appendOutput(tr("The JavaScript engine could not be initialized."));
        return;
    }

    const QByteArray source = script.toUtf8();
    const QByteArray fileName = sourceName.toUtf8();
    executionTimer.start();
    JSValue result = JS_Eval(javascriptContext, source.constData(), static_cast<size_t>(source.size()),
                             fileName.constData(), JS_EVAL_TYPE_GLOBAL);
    executionTimer.invalidate();

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(javascriptContext);
        const char *message = JS_ToCString(javascriptContext, exception);
        appendOutput(tr("JavaScript error: %1").arg(message ? QString::fromUtf8(message) : tr("Unknown error")));
        if (message) {
            JS_FreeCString(javascriptContext, message);
        }
        JS_FreeValue(javascriptContext, exception);
    } else if (!JS_IsUndefined(result)) {
        const char *value = JS_ToCString(javascriptContext, result);
        if (value) {
            appendOutput(QString::fromUtf8(value));
            JS_FreeCString(javascriptContext, value);
        }
    }

    JS_FreeValue(javascriptContext, result);
}

void ScriptConsoleDock::appendOutput(const QString &text)
{
    output->appendPlainText(text);
    output->ensureCursorVisible();
}
