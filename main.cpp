// main.cpp  (FULL FILE)
// ✅ iRecordManage only under #ifdef PLATFORM_JETSON
// ✅ Ubuntu: prefer Wayland if present; else xcb
// ✅ Ubuntu: FORCE override if QT_QPA_PLATFORM is eglfs/linuxfb/offscreen (env ค้างจาก Jetson)
// ✅ Ubuntu: disable MIT-SHM to stop MESA spam
// ✅ HARD STOP on QML load fail (avoid nullptr connects)
// ✅ x86 => show mouse cursor (do NOT blank); Jetson => blank cursor
// ✅ NEW: expose HardwareHas5G / HardwareVersionName to QML

#include <QGuiApplication>
#include <QEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QDebug>
#include <QFont>
#include <QTimer>
#include <QDateTime>
#include <QNetworkProxyFactory>
#include <QTextStream>
#include <QWindow>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlError>
#include <QProcessEnvironment>
#include <QCursor>
#include <QSocketNotifier>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

#ifndef HARDWARE_HAS_5G
#define HARDWARE_HAS_5G 0
#endif

#ifndef HARDWARE_HAS_WIFI
#define HARDWARE_HAS_WIFI 0
#endif

#ifndef HARDWARE_HAS_WIRELESS
#define HARDWARE_HAS_WIRELESS 0
#endif

#ifndef FEATURE_TOP_NETWORK_DRAWER
#define FEATURE_TOP_NETWORK_DRAWER 0
#endif

// -------- iScreenDF --------
#include "iScreenDF/iScreenDF.h"
#include "iScreenDF/ImageProviderDF.h"
#include "iScreenDF/iClockOrin_types.h"

// -------- App Controllers --------
#include "Mainwindows.h"
#include "CrashDiagnostics.h"
#include "NetworkController.h"
#include "NetworkSecurityController.h"
#include "ReceiverConfigManager.h"
#include "ReceiverRecorderConfigManager.h"
#include "websocketclient.h"
#include "FftDisplayItem.h"
#include "screencapture.h"

// -------- iRecordManage (JETSON ONLY) --------
#ifdef PLATFORM_JETSON
#include "iRecordManage/mainwindowsiRec.h"
#include "iRecordManage/FileReader.h"
#endif

// -------- DOAViewer --------
#include "DoaViewer/DoaClient.h"

// ======================================================
// GLOBALS
// ======================================================
static QWindow*      gMainWin = nullptr;
static QLocalServer* gServer  = nullptr;

static const char* SOCKET_NAME = "ifz_app1.sock";
static const char* APP_TITLE   = "App iScan";

static QTextStream qout(stdout);
static int gSignalPipe[2] = {-1, -1};

// ======================================================
// Save state before exit
// ======================================================
static void saveStateAndQuit() {
    QCoreApplication::quit();
}

// POSIX signal handlers may only call async-signal-safe functions. The old
// implementation called QMetaObject::invokeMethod() directly from SIGTERM/INT,
// which is undefined behavior and can fail during Qt Creator remote cancel.
// Write the signal number to a non-blocking pipe and let the Qt event loop do
// the real shutdown work.
static void handleSignal(int signalNumber, siginfo_t *info, void *)
{
    const int savedErrno = errno;
    CrashDiagnostics::signalSafeTermination(signalNumber,
                                            info ? info->si_pid : 0,
                                            info ? static_cast<unsigned>(info->si_uid) : 0U,
                                            info ? info->si_code : 0);
    if (gSignalPipe[1] >= 0) {
        const int value = signalNumber;
        (void)::write(gSignalPipe[1], &value, sizeof(value));
    }
    errno = savedErrno;
}

static bool installSignalBridge(QCoreApplication *app)
{
    if (::pipe(gSignalPipe) != 0) {
        qCritical() << "[SIGNAL] pipe creation failed errno=" << errno;
        return false;
    }

    for (int fd : gSignalPipe) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        const int fdFlags = ::fcntl(fd, F_GETFD, 0);
        if (fdFlags >= 0)
            ::fcntl(fd, F_SETFD, fdFlags | FD_CLOEXEC);
    }

    auto *notifier = new QSocketNotifier(gSignalPipe[0], QSocketNotifier::Read, app);
    QObject::connect(notifier, &QSocketNotifier::activated, app, [notifier](int) {
        notifier->setEnabled(false);
        int signalNumber = 0;
        while (::read(gSignalPipe[0], &signalNumber, sizeof(signalNumber)) == sizeof(signalNumber)) {
            qWarning() << "[SIGNAL] graceful shutdown requested signal=" << signalNumber;
        }
        saveStateAndQuit();
    });

    struct sigaction action {};
    action.sa_sigaction = handleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    if (::sigaction(SIGTERM, &action, nullptr) != 0 ||
        ::sigaction(SIGINT, &action, nullptr) != 0) {
        qCritical() << "[SIGNAL] sigaction install failed errno=" << errno;
        return false;
    }
    return true;
}

// ======================================================
// Runtime env setup
// ======================================================
static void setupRuntimeEnv()
{
    if (qEnvironmentVariableIsEmpty("QT_X11_NO_MITSHM")) {
        qputenv("QT_X11_NO_MITSHM", "1");
    }

#ifdef PLATFORM_JETSON
    // R20.3 / KP-09SEP2026 : ARM64 QV4 JIT safety guard
    //
    // Native core dumps consistently show SIGSEGV in
    // QV4::MemoryManager::collectFromJSStack() while the QML engine is
    // performing garbage collection.  This target runs Qt 5.15.2 on AArch64,
    // a combination with a known V4 JIT failure mode where generated code can
    // corrupt JavaScript stack slots and the damage is discovered later by GC.
    //
    // Disable the QML/JavaScript JIT before QGuiApplication creates the QV4
    // engine.  This does not alter QML layout, bindings, signals, application
    // data, or user interaction; only the JavaScript execution backend changes
    // from JIT to interpreter.
    //
    // Engineering A/B escape hatch only:
    //   ISCAN_QML_JIT=1  -> allow the legacy JIT path again.
    const QByteArray qmlJitOverride = qgetenv("ISCAN_QML_JIT").trimmed().toLower();
    const bool allowLegacyQmlJit =
            qmlJitOverride == "1"
            || qmlJitOverride == "true"
            || qmlJitOverride == "yes"
            || qmlJitOverride == "on";

    if (allowLegacyQmlJit) {
        qunsetenv("QV4_FORCE_INTERPRETER");
    } else {
        qputenv("QV4_FORCE_INTERPRETER", QByteArray("1"));
    }

    if (qEnvironmentVariableIsEmpty("DISPLAY"))
        qputenv("DISPLAY", QByteArray(":0"));

    qputenv("QT_QPA_PLATFORM", QByteArray("eglfs"));
    qputenv("QT_QPA_EGLFS_INTEGRATION", QByteArray("eglfs_x11"));
    qputenv("QT_QPA_EGLFS_DEPTH", QByteArray("4"));
    qputenv("QT_QPA_GENERIC_PLUGINS", QByteArray("evdevtouch"));
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));

    qputenv("QT_QPA_EGLFS_NO_LIBINPUT", "1");
    qputenv("QT_QPA_EGLFS_DISABLE_INPUT", "1");

    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    qputenv("QT_NO_KEYBOARD", "1");

#else
    const QByteArray curPlat = qgetenv("QT_QPA_PLATFORM").trimmed().toLower();

    auto unsetIfSet = [](const char* k){
        if (!qEnvironmentVariableIsEmpty(k)) qunsetenv(k);
    };

    const bool looksLikeEmbedded =
        curPlat.contains("eglfs") ||
        curPlat.contains("linuxfb") ||
        curPlat.contains("offscreen") ||
        curPlat.contains("minimal");

    if (looksLikeEmbedded) {
        qWarning().noquote()
        << "[ENV] QT_QPA_PLATFORM was" << curPlat
        << "=> overriding to desktop platform";

        unsetIfSet("QT_QPA_EGLFS_INTEGRATION");
        unsetIfSet("QT_QPA_EGLFS_DEPTH");
        unsetIfSet("QT_QPA_GENERIC_PLUGINS");
        unsetIfSet("QSG_RENDER_LOOP");
        unsetIfSet("QT_QPA_EGLFS_NO_LIBINPUT");
        unsetIfSet("QT_QPA_EGLFS_DISABLE_INPUT");
        unsetIfSet("QT_IM_MODULE");
        unsetIfSet("QT_NO_KEYBOARD");
        qunsetenv("QT_QPA_PLATFORM");
    }

    const bool hasWayland =
        !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") ||
        qgetenv("XDG_SESSION_TYPE").toLower() == "wayland";

    const bool hasX11 = !qEnvironmentVariableIsEmpty("DISPLAY");

    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (hasWayland) {
            qputenv("QT_QPA_PLATFORM", "wayland");
        } else if (hasX11) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        } else {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
    }
#endif

    if (qEnvironmentVariableIsEmpty("QTWEBGL_PORT")) {
        qputenv("QTWEBGL_PORT", QByteArray("8081"));
    }

    // Keep info/warning/critical output available for remote stability faults.
    // Suppress only verbose debug messages unless the deployment overrides it.
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES"))
        qputenv("QT_LOGGING_RULES", QByteArray("*.debug=false"));
}

// ======================================================
// MAIN
// ======================================================

class DiagnosticGuiApplication final : public QGuiApplication
{
public:
    DiagnosticGuiApplication(int &argc, char **argv)
        : QGuiApplication(argc, argv)
    {
    }

    bool notify(QObject *receiver, QEvent *event) override
    {
        const char *className = receiver ? receiver->metaObject()->className() : "<null>";
        CrashDiagnostics::qtEventEnter(receiver,
                                       event ? static_cast<int>(event->type()) : -1,
                                       className);
        struct LeaveGuard {
            ~LeaveGuard() { CrashDiagnostics::qtEventLeave(); }
        } guard;
        return QGuiApplication::notify(receiver, event);
    }
};

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    setupRuntimeEnv();

    QFont fon("Kinnari");

    DiagnosticGuiApplication app(argc, argv);
    app.setFont(fon);

    // Qt.labs.settings requires stable application identifiers before any
    // QML Settings object is instantiated.
    QCoreApplication::setOrganizationName(QStringLiteral("IFZTeam"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("ifzteam.local"));
    QCoreApplication::setApplicationName(QStringLiteral("iScanMR10"));

#ifdef PLATFORM_JETSON
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#else
    while (QGuiApplication::overrideCursor())
        QGuiApplication::restoreOverrideCursor();
#endif

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){
        CrashDiagnostics::event(CrashDiagnostics::CpAppQuit, "APP", "aboutToQuit");
        while (QGuiApplication::overrideCursor())
            QGuiApplication::restoreOverrideCursor();
        CrashDiagnostics::shutdown();
    });

    app.setApplicationDisplayName(APP_TITLE);

    CrashDiagnostics::init();
    CrashDiagnostics::installFatalSignalHandlers();
    CrashDiagnostics::event(CrashDiagnostics::CpBoot, "BOOT", "R20.1 diagnostics initialized");
    installSignalBridge(&app);
    std::set_terminate([]() {
        CrashDiagnostics::fatalLiteral("std::terminate invoked");
        std::abort();
    });

    qInfo().noquote() << "[ENV] DISPLAY=" << qgetenv("DISPLAY");
    qInfo().noquote() << "[ENV] WAYLAND_DISPLAY=" << qgetenv("WAYLAND_DISPLAY");
    qInfo().noquote() << "[ENV] XDG_SESSION_TYPE=" << qgetenv("XDG_SESSION_TYPE");
    qInfo().noquote() << "[ENV] QT_QPA_PLATFORM=" << qgetenv("QT_QPA_PLATFORM");
    qInfo().noquote() << "[ENV] QT_X11_NO_MITSHM=" << qgetenv("QT_X11_NO_MITSHM");

#ifdef PLATFORM_JETSON
    qInfo().noquote() << "[R20.3 QML-JIT-SAFETY] revision=20260909-arm64-qv4-interpreter-no-ui";
    qInfo().noquote() << "[R20.4 NATIVE FFT] revision=20260909-native-fft-qv4-pressure-no-ui";
    qInfo().noquote() << "[QML ENGINE] QV4_FORCE_INTERPRETER="
                      << qgetenv("QV4_FORCE_INTERPRETER")
                      << "ISCAN_QML_JIT=" << qgetenv("ISCAN_QML_JIT");
    qInfo().noquote() << "[CURSOR] PLATFORM_JETSON => BlankCursor";
#else
    qInfo().noquote() << "[CURSOR] x86/desktop => normal mouse cursor";
#endif

#if HARDWARE_HAS_5G
    qInfo().noquote() << "[HW] HARDWARE_VERSION_5G";
#else
    qInfo().noquote() << "[HW] HARDWARE_VERSION_NONE_5G";
#endif
    qInfo().noquote() << "[HW] Wireless=" << bool(HARDWARE_HAS_WIRELESS)
                      << "WiFi=" << bool(HARDWARE_HAS_WIFI)
                      << "5G=" << bool(HARDWARE_HAS_5G);
    qInfo().noquote() << "[FEATURE] TopNetworkDrawer="
                      << bool(FEATURE_TOP_NETWORK_DRAWER);

    // ==================================================
    // Register QML Types
    // ==================================================
    qmlRegisterType<ReceiverConfigManager>("Receiver", 1, 0, "ReceiverConfigManager");
    qmlRegisterType<WebSocketClient>("WebSocketClient", 1, 0, "WebSocketClient");
    qmlRegisterType<FftDisplayItem>("iScan.Display", 1, 0, "FftDisplayItem");

    NetworkController* netCtrl = new NetworkController(&app);
    qmlRegisterSingletonInstance("App", 1, 0, "NetworkController", netCtrl);

    // KP-6JUL2026 : Centralized password verification for protected network changes.
    // QML never receives the expected plaintext password.
    NetworkSecurityController networkSecurity;

    ImageProvider *imageProvider = new ImageProvider();
    qmlRegisterSingletonInstance("App1", 1, 0, "Screenshots", imageProvider);

    ReceiverRecorderConfigManager *recCtrl = new ReceiverRecorderConfigManager();
    qmlRegisterSingletonInstance("App2", 1, 0, "ReceiverRecorderConfigManager", recCtrl);

    // ==================================================
    // QML ENGINE
    // ==================================================
    auto engine = std::make_unique<QQmlApplicationEngine>();
    engine->setOutputWarningsToStandardError(true);

#if HARDWARE_HAS_5G
    engine->rootContext()->setContextProperty("HardwareHas5G", true);
    engine->rootContext()->setContextProperty("HardwareVersionName", QStringLiteral("5G"));
#else
    engine->rootContext()->setContextProperty("HardwareHas5G", false);
    engine->rootContext()->setContextProperty("HardwareVersionName", QStringLiteral("NONE_5G"));
#endif
    engine->rootContext()->setContextProperty("HardwareHasWifi", bool(HARDWARE_HAS_WIFI));
    engine->rootContext()->setContextProperty("HardwareHasWireless", bool(HARDWARE_HAS_WIRELESS));
    engine->rootContext()->setContextProperty("FeatureTopNetworkDrawer",
                                             bool(FEATURE_TOP_NETWORK_DRAWER));
    engine->rootContext()->setContextProperty("networkSecurity", &networkSecurity);
    // Runtime QML pages use this context property, while Design mode can omit it.
    engine->rootContext()->setContextProperty("networkController", netCtrl);

    ImageProviderDF *imageProviderDF = new ImageProviderDF();
    iScreenDF *kraken = new iScreenDF(imageProviderDF);
    engine->rootContext()->setContextProperty("Krakenmapval", kraken);

    Mainwindows mainWindows(netCtrl, nullptr);
    engine->rootContext()->setContextProperty("mainWindows", &mainWindows);
    engine->rootContext()->setContextProperty("wsClient",  &mainWindows.wsClient);

#ifdef PLATFORM_JETSON
    mainwindowsiRec recMain("desktop");
    engine->rootContext()->setContextProperty("mainwindows", &recMain);
    engine->rootContext()->setContextProperty("Backend",     &recMain);

    QObject::connect(&mainWindows, &Mainwindows::frequencyChangedToQml,
                     &recMain,     &mainwindowsiRec::onFrequencyChangedFromMain);
    QObject::connect(&mainWindows, &Mainwindows::commandMainCppToRecCpp,
                     &recMain,     &mainwindowsiRec::RecevieCommandMainCpp);

    FileReader fileReader;
    engine->rootContext()->setContextProperty("fileReader", &fileReader);
    engine->rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());
#endif

    DoaClient doaClient;
    engine->rootContext()->setContextProperty("doaClient", &doaClient);

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    QObject::connect(engine.get(), &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl) {
                             qCritical() << "QML objectCreated failed for:" << url;
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);

    engine->load(url);

    if (engine->rootObjects().isEmpty()) {
        qCritical() << "QML load failed (rootObjects empty)";
        return -1;
    }

    QObject *topLevel = engine->rootObjects().first();
    QQuickWindow *qmlWindow = qobject_cast<QQuickWindow *>(topLevel);
    if (!qmlWindow) {
        qCritical() << "Root QML is not a QQuickWindow. type=" << topLevel->metaObject()->className();
        return -1;
    }

    gMainWin = qmlWindow;

    // ==================================================
    // IPC
    // ==================================================
    QLocalServer::removeServer(SOCKET_NAME);
    gServer = new QLocalServer(&app);

    QObject::connect(gServer, &QLocalServer::newConnection, [] {
        if (auto *c = gServer->nextPendingConnection()) {
            QObject::connect(c, &QLocalSocket::readyRead, [c] {
                const auto msg = QString::fromUtf8(c->readAll()).trimmed();

                if (msg == "quit") saveStateAndQuit();
                else if (msg == "show" || msg == "raise") {
                    if (gMainWin) {
                        gMainWin->show();
                        gMainWin->raise();
                        gMainWin->requestActivate();
                    }
                }
                else if (msg == "ping") {
                    c->write("pong");
                    c->flush();
                }
            });
        }
    });

    if (!gServer->listen(SOCKET_NAME))
        qWarning() << "IPC listen failed on" << SOCKET_NAME;

    // ==================================================
    // Connect QML Signals <-> C++ Slots
    // ==================================================
    QObject::connect(recCtrl, SIGNAL(onRecorderConfigSaved()),
                     &mainWindows, SLOT(onRecorderConfigSaved()));

    QObject::connect(qmlWindow, SIGNAL(qmlCommand(QString)),
                     &mainWindows, SLOT(cppSubmitTextFiled(QString)));

    QObject::connect(qmlWindow, SIGNAL(sCan(QString)),
                     &mainWindows, SLOT(sCan(QString)));

    QObject::connect(qmlWindow, SIGNAL(profileWeb(QString)),
                     &mainWindows, SLOT(profileWeb(QString)));

    QObject::connect(qmlWindow, SIGNAL(signalProfileCards()),
                     &mainWindows, SLOT(profiles()));

    QObject::connect(&mainWindows, SIGNAL(cppCommand(QVariant)),
                     qmlWindow, SLOT(qmlSubmitTextFiled(QVariant)));

    QObject::connect(qmlWindow, SIGNAL(getScreenshot()),
                     imageProvider, SLOT(makeScreenshot()));

#ifdef PLATFORM_JETSON
    QMetaObject::invokeMethod(&recMain, "startRuntime", Qt::QueuedConnection);

    QObject::connect(qmlWindow, SIGNAL(qmlCommand(QString)),
                     &recMain,  SLOT(cppSubmitTextFiled(QString)));
    QObject::connect(&recMain,  SIGNAL(cppCommand(QVariant)),
                     qmlWindow, SLOT(qmlSubmitTextFiled(QVariant)));

    // QObject::connect(qmlWindow, SIGNAL(getScreenshot()),
    //                  imageProviderDF, SLOT(makeScreenshot()));
#endif

    const int exitCode = app.exec();

    // Explicitly destroy the QML engine while every C++ context backend is
    // still alive. This gives QML destruction callbacks a valid Mainwindows,
    // recorder, network controller, and DOA backend during remote stop/restart.
    gMainWin = nullptr;
    engine.reset();

    if (gSignalPipe[0] >= 0) { ::close(gSignalPipe[0]); gSignalPipe[0] = -1; }
    if (gSignalPipe[1] >= 0) { ::close(gSignalPipe[1]); gSignalPipe[1] = -1; }

    return exitCode;
}
