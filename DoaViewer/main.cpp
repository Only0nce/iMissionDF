#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "DoaClient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    DoaClient doaClient;
    engine.rootContext()->setContextProperty("doaClient", &doaClient);

    engine.load(QUrl(QStringLiteral("qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
