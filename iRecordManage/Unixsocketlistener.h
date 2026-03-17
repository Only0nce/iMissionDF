#pragma once

#include <QObject>
#include <QSocketNotifier>
#include "Unixsocketlistener.h"
#include <QDebug>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>

class UnixSocketListener : public QObject {
    Q_OBJECT
public:
    explicit UnixSocketListener(QObject* parent = nullptr);
    ~UnixSocketListener();

signals:
    void messageReceived(const QString& message);

private:
    int socketFd;
    QSocketNotifier* notifier= nullptr;;
    QString socketPath = "/tmp/recd_status.sock";

    void setupSocket();
    bool makeNonBlocking(int fd);
private slots:
    void handleReadyRead();
};
