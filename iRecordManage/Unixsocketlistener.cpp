#include "Unixsocketlistener.h"
#include <QDebug>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

UnixSocketListener::UnixSocketListener(QObject* parent)
    : QObject(parent)
{
    setupSocket();
}

UnixSocketListener::~UnixSocketListener() {
    if (notifier)
        delete notifier;
    if (socketFd >= 0)
        close(socketFd);
    if (m_bound)
        ::unlink(socketPath.toLocal8Bit().constData());
}
bool UnixSocketListener::makeNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return false;
    return true;
}
void UnixSocketListener::setupSocket() {
    socketFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socketFd < 0) {
        qCritical() << "[UnixSocketListener] socket() failed:" << strerror(errno)
                    << "-- recorder status integration disabled";
        return;
    }

    if (!makeNonBlocking(socketFd)) {
        qWarning() << "Failed to make socket non-blocking";
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.toUtf8().constData(), sizeof(addr.sun_path) - 1);

    ::unlink(addr.sun_path);  // Remove old socket file
    socklen_t addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

    if (bind(socketFd, (struct sockaddr*)&addr, addr_len) < 0) {
        qCritical() << "[UnixSocketListener] bind failed" << addr.sun_path
                    << strerror(errno)
                    << "-- recorder status integration disabled";
        close(socketFd);
        socketFd = -1;
        return;
    }
    m_bound = true;

    notifier = new QSocketNotifier(socketFd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated,
            this, &UnixSocketListener::handleReadyRead);

    qDebug() << "Listening on Unix socket:" << socketPath;
}

void UnixSocketListener::handleReadyRead()
{
    while (true) {
        char buffer[1024];
        ssize_t len = recvfrom(socketFd, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            qWarning() << "recvfrom error:" << strerror(errno);
            break;
        }
        if (len == 0) {
            break;
        }

        buffer[len] = '\0';
        QString msg = QString::fromUtf8(buffer).trimmed();
        qDebug() << "Received message:" << msg;
        emit messageReceived(msg);
    }
}
