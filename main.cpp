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
#include <csignal>
#include <GLES3/gl32.h>
#include <QDebug>
#include <GLES3/gl32.h>

// -------- iScreenDF --------
#include "iScreenDF/iScreenDF.h"
#include "iScreenDF/ImageProviderDF.h"
#include "iScreenDF/iClockOrin_types.h"
// #include "/home/only/Music/iScanMR10_DF/iScanMR10/iScreenDF/iScreenDF.h"
// #include "ImageProviderDF.h"
// #include "iClockOrin_types.h"

// -------- App Controllers --------
#include "Mainwindows.h"
#include "NetworkController.h"
#include "ReceiverConfigManager.h"
#include "ReceiverRecorderConfigManager.h"
#include "websocketclient.h"
#include "screencapture.h"

// -------- iRecordManage ---------------
#include "iRecordManage/mainwindowsiRec.h"
#include "iRecordManage/FileReader.h"

// -------- DOAViewer ---------------
#include "DoaViewer/DoaClient.h"
// ======================================================
// GLOBALS

// ======================================================
static QWindow* gMainWin = nullptr;
static QLocalServer* gServer = nullptr;

static const char* SOCKET_NAME = "ifz_app1.sock";
static const char* APP_TITLE   = "App iScan";

static QTextStream qout(stdout);

// ======================================================
// GPS DEBUG PRINT
// ======================================================
static void printGps(const char* tag, const GPSInfo& g) {
    qout << "[" << tag << "] "
         << (g.locked ? "LOCK" : "NOLOCK")
         << "  time=" << g.date << " " << g.time
         << "  lat=" << g.lat << " lon=" << g.lon
         << "  alt=" << g.alt << "m"
         << "  speed=" << g.speed << "m/s"
         << "  sat(use/vis)=" << g.satUse << "/" << g.sat
         << "\n   constel: ";

    for (auto it = g.constelCounts.cbegin(); it != g.constelCounts.cend(); ++it)
        qout << it.key() << ":" << it.value() << " ";

    qout << "\n";
    qout.flush();
}

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
// MAIN
// ======================================================
int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
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

    qputenv("QTWEBGL_PORT", QByteArray("8081"));

    // qputenv("QT_LOGGING_RULES", QByteArray("*.debug=false;*.info=false;*.warning=false"));

    // qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Global font
    QFont fon("Kinnari");

    QGuiApplication app(argc, argv);
    app.setFont(fon);
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));

    // Soft-kill handler
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT,  handleSignal);

    // ==================================================
    // Register QML Types
    // ==================================================
    qmlRegisterType<ReceiverConfigManager>("Receiver", 1, 0, "ReceiverConfigManager");
    qmlRegisterType<WebSocketClient>("WebSocketClient", 1, 0, "WebSocketClient");

    NetworkController* netCtrl = new NetworkController();
    qmlRegisterSingletonInstance("App", 1, 0, "NetworkController", netCtrl);

    // Image Provider (global)
    ImageProvider *imageProvider = new ImageProvider();
    qmlRegisterSingletonInstance("App1", 1, 0, "Screenshots", imageProvider);

    ReceiverRecorderConfigManager *recCtrl = new ReceiverRecorderConfigManager();
    qmlRegisterSingletonInstance("App2", 1, 0, "ReceiverRecorderConfigManager", recCtrl);

    // ==================================================
    // QML ENGINE
    // ==================================================
    QQmlApplicationEngine engine;

    // iScreenDF (Map, Compass, GPS)
    ImageProviderDF *imageProviderDF = new ImageProviderDF();
    iScreenDF *kraken = new iScreenDF(imageProviderDF);
    engine.rootContext()->setContextProperty("Krakenmapval", kraken);

    // Mainwindows (Websocket + UI Commands)
    Mainwindows mainWindows;
    engine.rootContext()->setContextProperty("mainWindows", &mainWindows);
    engine.rootContext()->setContextProperty("wsClient",  &mainWindows.wsClient);

    // Mainwindows (iRecordManage)
    mainwindowsiRec recMain("desktop");
    engine.rootContext()->setContextProperty("mainwindows", &recMain);
    engine.rootContext()->setContextProperty("Backend",     &recMain);
    QObject::connect(&mainWindows, &Mainwindows::frequencyChangedToQml,
                     &recMain,     &mainwindowsiRec::onFrequencyChangedFromMain);
    QObject::connect(&mainWindows, &Mainwindows::commandMainCppToRecCpp,
                     &recMain,     &mainwindowsiRec::RecevieCommandMainCpp);
    // DOAViewer

    DoaClient doaClient;
    engine.rootContext()->setContextProperty("doaClient", &doaClient);

    // FileReader (iRecordManage)
    FileReader fileReader;
    engine.rootContext()->setContextProperty("fileReader", &fileReader);

    engine.rootContext()->setContextProperty("applicationDirPath", QGuiApplication::applicationDirPath());

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.setOutputWarningsToStandardError(false);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    // ==================================================
    // IPC (Inter-process)
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
                    c->write("pong"); c->flush();
                }
            });
        }
    });

    if (!gServer->listen(SOCKET_NAME))
        qWarning() << "IPC listen failed";

    // ==================================================
    // Connect QML Signals <-> C++ Slots
    // ==================================================
    QObject *topLevel = engine.rootObjects().value(0);
    QQuickWindow *qmlWindow = qobject_cast<QQuickWindow *>(topLevel);

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

    // Screenshot
    QObject::connect(qmlWindow, SIGNAL(getScreenshot()),
                     imageProvider, SLOT(makeScreenshot()));


    // ===== startRuntime(iRecordManage) =====
    QMetaObject::invokeMethod(&recMain, "startRuntime", Qt::QueuedConnection);
    QQuickWindow *view = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!view) {
        qWarning() << "Root QML is not a QQuickWindow.";
        return -1;
    }

    QObject::connect(view, SIGNAL(qmlCommand(QString)),
                     &recMain,  SLOT(cppSubmitTextFiled(QString)));
    QObject::connect(&recMain,  SIGNAL(cppCommand(QVariant)),
                     view, SLOT(qmlSubmitTextFiled(QVariant)));

    QQuickWindow *view2 = dynamic_cast<QQuickWindow *>(engine.rootObjects().at(0));
    QObject::connect(view2, SIGNAL(getScreenshot()),
                     imageProviderDF, SLOT(makeScreenshot()));


    return app.exec();
}


/*
echo "SETFREQ 0"         | nc 127.0.0.1 6000
    echo "SETFREQ 1000000"   | nc 127.0.0.1 6000
    echo "SETFREQ 2000000"   | nc 127.0.0.1 6000
    echo "SETFREQ 100000000" | nc 127.0.0.1 6000
    echo "GETFREQ"           | nc 127.0.0.1 6000
*/
