/*
 * This file is part of Notepad Next.
 * Copyright 2026 SysAdminDoc
 */

#include "AtomicFileWriter.h"
#include "CapabilityTrust.h"
#include "DirectoryDropScanner.h"
#include "EditorManager.h"
#include "MainWindow.h"
#include "NotepadNextApplication.h"
#include "NppPluginTrust.h"
#include "ScintillaNext.h"
#include "SessionManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUuid>

#include <memory>

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    return file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    return file.readAll();
}

void lifecycleTestTrace(const char *message)
{
    QFile trace(QDir::temp().filePath(QStringLiteral("NotepadNextLifecycleTrace.txt")));
    const QIODevice::OpenMode mode = qstrcmp(message, "test:begin") == 0
        ? QIODevice::WriteOnly
        : QIODevice::WriteOnly | QIODevice::Append;
    if (trace.open(mode)) {
        trace.write(message);
        trace.write("\n");
    }
}

void lifecycleTestTraceText(const QString &message)
{
    lifecycleTestTrace(message.toUtf8().constData());
}

class LifecycleEventFilter final : public QObject
{
protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::DeferredDelete && watched != nullptr) {
            lifecycleTestTraceText(
                QStringLiteral("deferred:%1:%2")
                    .arg(QString::fromLatin1(watched->metaObject()->className()), watched->objectName()));
        }
        return QObject::eventFilter(watched, event);
    }
};

void traceObjectDestruction(QObject *root, QObject *context)
{
    const auto objects = root->findChildren<QObject *>();
    for (QObject *object : objects) {
        const QString description = QStringLiteral("%1:%2")
            .arg(QString::fromLatin1(object->metaObject()->className()), object->objectName());
        QObject::connect(object, &QObject::destroyed, context, [description]() {
            lifecycleTestTraceText(QStringLiteral("destroyed:%1").arg(description));
        });
    }
}

void processEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void closeEditors(MainWindow *window)
{
    lifecycleTestTrace("test:close-begin");
    const QVector<ScintillaNext *> editors = window->editors();
    lifecycleTestTraceText(QStringLiteral("test:close-count:%1").arg(editors.size()));
    for (ScintillaNext *editor : editors) {
        editor->omitModifications();
        lifecycleTestTrace("test:close-editor");
        editor->close();
        processEvents();
    }
    processEvents();
    lifecycleTestTrace("test:close-done");
}

QString executableName(const QString &baseName)
{
#ifdef Q_OS_WIN
    return baseName + QStringLiteral(".exe");
#else
    return baseName;
#endif
}
}

class LifecycleRegressionTests final : public QObject
{
    Q_OBJECT

public:
    explicit LifecycleRegressionTests(NotepadNextApplication *application,
                                      QTemporaryDir *workspace)
        : application(application)
        , workspace(workspace)
    {
    }

private slots:
    void initTestCase();
    void mainWindowFileLifecycle();
    void sessionRestoreAndCapabilityBoundaries();
    void componentAndPackagingSmoke();
    void cleanupTestCase();

private:
    NotepadNextApplication *application;
    QTemporaryDir *workspace;
    MainWindow *window = nullptr;
};

void LifecycleRegressionTests::initTestCase()
{
    QVERIFY(application->isPrimary());
    QVERIFY(application->init());

    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *candidate = qobject_cast<MainWindow *>(widget)) {
            window = candidate;
            break;
        }
    }

    QVERIFY(window != nullptr);
    traceObjectDestruction(window, application);
    QVERIFY(application->getSettings() != nullptr);
    application->getSettings()->setRestorePreviousSession(false);
    application->getSettings()->setRestoreUnsavedFiles(false);
    application->getSettings()->setRestoreTempFiles(false);
    application->getSettings()->setExitOnLastTabClosed(false);
    application->getSessionManager()->clear();
}

void LifecycleRegressionTests::mainWindowFileLifecycle()
{
    lifecycleTestTrace("test:begin");
    closeEditors(window);
    lifecycleTestTrace("test:after-close");
    window->newFile();
    lifecycleTestTrace("test:after-new");

    const QString documentPath = workspace->filePath(QStringLiteral("lifecycle.txt"));
    const QString renamedPath = workspace->filePath(QStringLiteral("renamed.txt"));
    const QString dropDirectory = workspace->filePath(QStringLiteral("drop"));
    const QString droppedOne = QDir(dropDirectory).filePath(QStringLiteral("one.txt"));
    const QString droppedTwo = QDir(dropDirectory).filePath(QStringLiteral("two.txt"));

    QVERIFY(writeFile(documentPath, QByteArrayLiteral("original")));
    QVERIFY(QDir().mkpath(dropDirectory));
    QVERIFY(writeFile(droppedOne, QByteArrayLiteral("one")));
    QVERIFY(writeFile(droppedTwo, QByteArrayLiteral("two")));

    window->openFile(documentPath);
    lifecycleTestTrace("test:after-open");
    processEvents();

    ScintillaNext *editor = application->getEditorManager()->getEditorByFilePath(documentPath);
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("original"));

    editor->setText("saved");
    QVERIFY(window->saveCurrentFile());
    QCOMPARE(readFile(documentPath), QByteArrayLiteral("saved"));

    QVERIFY(writeFile(documentPath, QByteArrayLiteral("external")));
    QCOMPARE(editor->checkFileForStateChange(), ScintillaNext::Modified);
    QString reloadError;
    QVERIFY(editor->reload(&reloadError));
    QVERIFY2(reloadError.isEmpty(), qPrintable(reloadError));
    QCOMPARE(editor->getText(editor->textLength()), QByteArrayLiteral("external"));

    editor->setText("renamed");
    QVERIFY(editor->rename(renamedPath));
    QVERIFY(!QFileInfo::exists(documentPath));
    QVERIFY(QFileInfo::exists(renamedPath));
    QCOMPARE(readFile(renamedPath), QByteArrayLiteral("renamed"));

    const QString atomicPath = workspace->filePath(QStringLiteral("atomic.txt"));
    QVERIFY(writeFile(atomicPath, QByteArrayLiteral("before")));
    const AtomicFileWriter::Result failedWrite = AtomicFileWriter::write(
        atomicPath,
        [](QIODevice *device, QString *error) {
            if (device->write("partial") != 7) {
                return false;
            }
            if (error) {
                *error = QStringLiteral("intentional integration failure");
            }
            return false;
        });
    QVERIFY(!failedWrite.succeeded());
    QCOMPARE(readFile(atomicPath), QByteArrayLiteral("before"));

    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile(dropDirectory)});
    QDropEvent dropEvent(QPointF(0, 0), Qt::CopyAction, &mimeData,
                         Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &dropEvent);

    QTRY_VERIFY_WITH_TIMEOUT(
        application->getEditorManager()->getEditorByFilePath(droppedOne) != nullptr &&
            application->getEditorManager()->getEditorByFilePath(droppedTwo) != nullptr,
        5000);
    QVERIFY(dropEvent.isAccepted());
    lifecycleTestTrace("test:after-drop");
}

void LifecycleRegressionTests::sessionRestoreAndCapabilityBoundaries()
{
    lifecycleTestTrace("session:begin");
    closeEditors(window);
    lifecycleTestTrace("session:after-close");
    lifecycleTestTrace("session:before-new1");
    window->newFile();
    lifecycleTestTrace("session:after-new1");

    const QString savedPath = workspace->filePath(QStringLiteral("session-saved.txt"));
    QVERIFY(writeFile(savedPath, QByteArrayLiteral("saved session content")));
    lifecycleTestTrace("session:before-open");
    window->openFile(savedPath);
    processEvents();
    lifecycleTestTrace("session:after-open");

    lifecycleTestTrace("session:before-new2");
    window->newFile();
    lifecycleTestTrace("session:after-new2");
    ScintillaNext *temporary = window->currentEditor();
    QVERIFY(temporary != nullptr);
    temporary->setText("temporary session content");
    temporary->languageName = QStringLiteral("Text");
    temporary->setCurrentPos(9);

    application->getSettings()->setRestorePreviousSession(true);
    application->getSettings()->setRestoreUnsavedFiles(true);
    application->getSettings()->setRestoreTempFiles(true);
    SessionManager *sessionManager = application->getSessionManager();
    sessionManager->clear();
    lifecycleTestTrace("session:before-save");
    sessionManager->saveSession(window);
    lifecycleTestTrace("session:after-save");

    const QString manifestPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("session/session-manifest.json"));
    QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(manifestPath));

    closeEditors(window);
    lifecycleTestTrace("session:after-close2");
    lifecycleTestTrace("session:before-new3");
    window->newFile();
    lifecycleTestTrace("session:after-new3");
    sessionManager->loadSession(window);
    processEvents();
    lifecycleTestTrace("session:after-load");

    ScintillaNext *restoredFile = application->getEditorManager()->getEditorByFilePath(savedPath);
    QVERIFY(restoredFile != nullptr);
    QCOMPARE(restoredFile->getText(restoredFile->textLength()), QByteArrayLiteral("saved session content"));

    ScintillaNext *restoredTemporary = nullptr;
    for (ScintillaNext *editor : window->editors()) {
        if (!editor->isFile() &&
            editor->getText(editor->textLength()) == QByteArrayLiteral("temporary session content")) {
            restoredTemporary = editor;
            break;
        }
    }
    QVERIFY(restoredTemporary != nullptr);

    const QString workspaceRoot = CapabilityTrust::Manager::workspaceRootForPath(savedPath);
    CapabilityTrust::Manager *trust = application->getCapabilityTrust();
    QVERIFY(trust != nullptr);
    trust->revokeAll(workspaceRoot);
    QVERIFY(!trust->authorize(nullptr, workspaceRoot,
                              CapabilityTrust::Capability::ScriptDocumentSave,
                              QStringLiteral("integration.denied")));

    trust->grant(workspaceRoot, CapabilityTrust::Capability::ScriptDocumentSave, false);
    QVERIFY(trust->isGranted(workspaceRoot, CapabilityTrust::Capability::ScriptDocumentSave));
    trust->record(workspaceRoot, CapabilityTrust::Capability::ScriptDocumentSave,
                  QStringLiteral("integration.save"), QStringLiteral("saved"));
    QVERIFY(!trust->auditEntries().isEmpty());
    trust->revokeAll(workspaceRoot);

    const QString pluginRoot = workspace->filePath(QStringLiteral("plugins"));
    const QString pluginPath = QDir(pluginRoot).filePath(QStringLiteral("Example.dll"));
    QVERIFY(QDir().mkpath(pluginRoot));
    QVERIFY(writeFile(pluginPath, QByteArrayLiteral("plugin fixture")));
    const QVector<NppPluginTrust::Candidate> candidates = NppPluginTrust::discover(pluginRoot, {});
    QCOMPARE(candidates.size(), 1);
    NppPluginTrust::Identity identity;
    QVERIFY(NppPluginTrust::identify(candidates.constFirst(), &identity));
    NppPluginTrust::TrustStore store(application->getSettings());
    QVERIFY(!store.isTrusted(identity));
    store.trust(identity);
    QVERIFY(store.isTrusted(identity));
    store.revoke(identity);
    QVERIFY(!store.isTrusted(identity));

    application->getSettings()->setRestorePreviousSession(false);
    application->getSettings()->setRestoreUnsavedFiles(false);
    application->getSettings()->setRestoreTempFiles(false);
    sessionManager->clear();
}

void LifecycleRegressionTests::componentAndPackagingSmoke()
{
    const QStringList componentTests = {
        QStringLiteral("NotepadNextAtomicFileWriterTests"),
        QStringLiteral("NotepadNextCapabilityTrustTests"),
        QStringLiteral("NotepadNextDirectoryDropScannerTests"),
        QStringLiteral("NotepadNextExternalFileChangeTests"),
        QStringLiteral("NotepadNextLspProtocolTests"),
        QStringLiteral("NotepadNextPluginTrustTests"),
        QStringLiteral("NotepadNextSessionJournalTests"),
        QStringLiteral("NotepadNextSftpClientTests"),
    };

    for (const QString &testName : componentTests) {
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        process.setProcessEnvironment(environment);
        process.setProgram(QDir(QCoreApplication::applicationDirPath()).filePath(executableName(testName)));
        process.start();
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(120000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    QProcess applicationVersion;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    applicationVersion.setProcessEnvironment(environment);
    applicationVersion.setProgram(QDir(QCoreApplication::applicationDirPath())
                                      .filePath(executableName(QStringLiteral("NotepadNext"))));
    applicationVersion.setArguments({QStringLiteral("--version")});
    applicationVersion.start();
    QVERIFY2(applicationVersion.waitForStarted(5000), qPrintable(applicationVersion.errorString()));
    QVERIFY2(applicationVersion.waitForFinished(30000), qPrintable(applicationVersion.errorString()));
    QCOMPARE(applicationVersion.exitStatus(), QProcess::NormalExit);
    QCOMPARE(applicationVersion.exitCode(), 0);
    QVERIFY2(applicationVersion.readAllStandardOutput().contains(QByteArray(APP_VERSION)),
              applicationVersion.readAllStandardError().constData());
}

void LifecycleRegressionTests::cleanupTestCase()
{
    if (window != nullptr) {
        closeEditors(window);
        lifecycleTestTrace("test:cleanup-before-close");
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->close();
        lifecycleTestTrace("test:cleanup-after-close");
        window->setParent(nullptr);
        processEvents();
        lifecycleTestTrace("test:cleanup-after-events");
        window = nullptr;
    }
}

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("NotepadNextLifecycleTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("NotepadNextLifecycleTests-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) {
        return 2;
    }
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory.path());

    QByteArray applicationName = QByteArrayLiteral("NotepadNextLifecycleTests");
    QByteArray safeMode = QByteArrayLiteral("--safe-mode");
    char *applicationArguments[] = {applicationName.data(), safeMode.data(), nullptr};
    int applicationArgumentCount = 2;
    // The offscreen Qt platform cannot safely destroy the ADS top-level widget
    // after the close path has completed.  Keep the application alive until the
    // harness process exits; all child processes and temporary directories are
    // still cleaned up normally, and the real close/save lifecycle is exercised.
    auto *application = new NotepadNextApplication(applicationArgumentCount, applicationArguments);

    LifecycleEventFilter eventFilter;
    application->installEventFilter(&eventFilter);

    QTemporaryDir workspace;
    if (!workspace.isValid() || !application->isPrimary()) {
        return 2;
    }

    LifecycleRegressionTests tests(application, &workspace);
    const int result = QTest::qExec(&tests, argc, argv);
    application->quit();
    processEvents();
    return result;
}

#include "LifecycleRegressionTests.moc"
