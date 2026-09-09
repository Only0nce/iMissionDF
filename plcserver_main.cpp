#include <QCoreApplication>
#include "PLCServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    PLCServer plc;
    return a.exec();
}
