// main.cpp  (FULL FILE)
// ✅ iRecordManage only under #ifdef PLATFORM_JETSON
// ✅ Ubuntu: prefer Wayland if present; else xcb
// ✅ Ubuntu: FORCE override if QT_QPA_PLATFORM is eglfs/linuxfb/offscreen (env ค้างจาก Jetson)
// ✅ Ubuntu: disable MIT-SHM to stop MESA spam
// ✅ HARD STOP on QML load fail (avoid nullptr connects)
// ✅ x86 => show mouse cursor (do NOT blank); Jetson => blank cursor
// ✅ NEW: expose HardwareHas5G / HardwareVersionName to QML

#include <QGuiApplication>
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
#include <csignal>

#ifndef HARDWARE_HAS_5G
#define HARDWARE_HAS_5G 0
#endif

#ifndef HARDWARE_HAS_WIFI
#define HARDWARE_HAS_WIFI 0
#endif

#ifndef HARDWARE_HAS_WIRELESS
#define HARDWARE_HAS_WIRELESS 0
#endif

// -------- iScreenDF --------
#include "iScreenDF/iScreenDF.h"
#include "iScreenDF/ImageProviderDF.h"
#include "iScreenDF/iClockOrin_types.h"

// -------- App Controllers --------
#include "Mainwindows.h"
#include "NetworkController.h"
#include "ReceiverConfigManager.h"
#include "ReceiverRecorderConfigManager.h"
#include "websocketclient.h"
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

// ======================================================
// Save state before exit
// ======================================================
static void saveStateAndQuit() {
    QCoreApplication::quit();
}

static void handleSignal(int) {
    QMetaObject::invokeMethod(qApp, [] { saveStateAndQuit(); }, Qt::QueuedConnection);
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

    qputenv("QT_LOGGING_RULES", QByteArray("*.debug=false;*.info=false;*.warning=false"));
}

// ======================================================
// MAIN
// ======================================================
int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    setupRuntimeEnv();

    QFont fon("Kinnari");

    QGuiApplication app(argc, argv);
    app.setFont(fon);

#ifdef PLATFORM_JETSON
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#else
    while (QGuiApplication::overrideCursor())
        QGuiApplication::restoreOverrideCursor();
#endif

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){
        while (QGuiApplication::overrideCursor())
            QGuiApplication::restoreOverrideCursor();
    });

    app.setApplicationDisplayName(APP_TITLE);

    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT,  handleSignal);

    qInfo().noquote() << "[ENV] DISPLAY=" << qgetenv("DISPLAY");
    qInfo().noquote() << "[ENV] WAYLAND_DISPLAY=" << qgetenv("WAYLAND_DISPLAY");
    qInfo().noquote() << "[ENV] XDG_SESSION_TYPE=" << qgetenv("XDG_SESSION_TYPE");
    qInfo().noquote() << "[ENV] QT_QPA_PLATFORM=" << qgetenv("QT_QPA_PLATFORM");
    qInfo().noquote() << "[ENV] QT_X11_NO_MITSHM=" << qgetenv("QT_X11_NO_MITSHM");

#ifdef PLATFORM_JETSON
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

    // ==================================================
    // Register QML Types
    // ==================================================
    qmlRegisterType<ReceiverConfigManager>("Receiver", 1, 0, "ReceiverConfigManager");
    qmlRegisterType<WebSocketClient>("WebSocketClient", 1, 0, "WebSocketClient");

    NetworkController* netCtrl = new NetworkController();
    qmlRegisterSingletonInstance("App", 1, 0, "NetworkController", netCtrl);

    ImageProvider *imageProvider = new ImageProvider();
    qmlRegisterSingletonInstance("App1", 1, 0, "Screenshots", imageProvider);

    ReceiverRecorderConfigManager *recCtrl = new ReceiverRecorderConfigManager();
    qmlRegisterSingletonInstance("App2", 1, 0, "ReceiverRecorderConfigManager", recCtrl);

    // ==================================================
    // QML ENGINE
    // ==================================================
    QQmlApplicationEngine engine;
    engine.setOutputWarningsToStandardError(true);

#if HARDWARE_HAS_5G
    engine.rootContext()->setContextProperty("HardwareHas5G", true);
    engine.rootContext()->setContextProperty("HardwareVersionName", QStringLiteral("5G"));
#else
    engine.rootContext()->setContextProperty("HardwareHas5G", false);
    engine.rootContext()->setContextProperty("HardwareVersionName", QStringLiteral("NONE_5G"));
#endif
    engine.rootContext()->setContextProperty("HardwareHasWifi", bool(HARDWARE_HAS_WIFI));
    engine.rootContext()->setContextProperty("HardwareHasWireless", bool(HARDWARE_HAS_WIRELESS));
    // Runtime QML pages use this context property, while Design mode can omit it.
    engine.rootContext()->setContextProperty("networkController", netCtrl);

    ImageProviderDF *imageProviderDF = new ImageProviderDF();
    iScreenDF *kraken = new iScreenDF(imageProviderDF);
    engine.rootContext()->setContextProperty("Krakenmapval", kraken);

    Mainwindows mainWindows;
    engine.rootContext()->setContextProperty("mainWindows", &mainWindows);
    engine.rootContext()->setContextProperty("wsClient",  &mainWindows.wsClient);

#ifdef PLATFORM_JETSON
    mainwindowsiRec recMain("desktop");
    engine.rootContext()->setContextProperty("mainwindows", &recMain);
    engine.rootContext()->setContextProperty("Backend",     &recMain);

    QObject::connect(&mainWindows, &Mainwindows::frequencyChangedToQml,
                     &recMain,     &mainwindowsiRec::onFrequencyChangedFromMain);
    QObject::connect(&mainWindows, &Mainwindows::commandMainCppToRecCpp,
                     &recMain,     &mainwindowsiRec::RecevieCommandMainCpp);

    FileReader fileReader;
    engine.rootContext()->setContextProperty("fileReader", &fileReader);
    engine.rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());
#endif

    DoaClient doaClient;
    engine.rootContext()->setContextProperty("doaClient", &doaClient);

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl) {
                             qCritical() << "QML objectCreated failed for:" << url;
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML load failed (rootObjects empty)";
        return -1;
    }

    QObject *topLevel = engine.rootObjects().first();
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

    return app.exec();
}
