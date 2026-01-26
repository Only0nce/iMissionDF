#include "InputEventReader.h"
#include "qtimer.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>

InputEventReader::InputEventReader(const QString &devicePath, QObject *parent)
    : QObject(parent), m_fd(-1), m_notifier(nullptr)
{
    m_fd = open(devicePath.toLocal8Bit().data(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) {
        qWarning() << "Failed to open" << devicePath;
        return;
    }
    else {
        qWarning() << devicePath << "is opened";
    }

    // QTimer *timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, this, &InputEventReader::onReadyRead);
    // timer->start(10); // 10ms interval

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &InputEventReader::onReadyRead);
}

InputEventReader::~InputEventReader()
{
    if (m_notifier)
        delete m_notifier;
    if (m_fd >= 0)
        close(m_fd);
}

void InputEventReader::onReadyRead()
{
    // qDebug() << "InputEventReader::onReadyRead";
    struct input_event ev;
    while (read(m_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        qDebug() << "ev.type" << ev.type << "ev.code"<< ev.code;
        if (ev.type == EV_REL && ev.code == REL_Y) {
            qDebug() << "ifffff ev.type" << ev.type << "ev.code"<< ev.code;
            emit rotaryTurned(ev.value); // 1 = CCW, -1 = CW
        } else if (ev.type == EV_KEY) {
            qDebug() << "elsiff ev.type" << ev.type << "ev.code"<< ev.code << "ev.value" << ev.value;
            if (ev.value == 1)
                emit keyPressed(ev.code);
            else if (ev.value == 0)
                emit keyReleased(ev.code);
        }
    }
}
