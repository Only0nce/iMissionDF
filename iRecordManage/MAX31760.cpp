#include "MAX31760.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QtDebug>

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

constexpr int kControlByteIndex = 2;
constexpr int kProfileByteIndex = 81;

// Exact 93-byte payload historically sent through:
//   i2ctransfer -y 0 w93@<addr> ...
// Keep the payload contract unchanged; only byte 81 selects the requested
// fan/mode profile.
const std::array<unsigned char, 93> kProfilePayload = {{
    0x00, 0x80, 0x11, 0x10, 0xee, 0xce, 0x18, 0x55,
    0x00, 0x46, 0x00, 0x6e, 0x00, 0x32, 0x00, 0xff,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55,
    0x55, 0x2a, 0x35, 0x40, 0x4b, 0x56, 0x61, 0x6c,
    0x77, 0x82, 0x8d, 0x98, 0xa3, 0xae, 0xb9, 0xc4,
    0xcf, 0xda, 0xe5, 0xf0, 0xfb, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xc8, 0x6c, 0x02, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x1b, 0x40, 0x40, 0x1f
}};

bool readThermalCelsius(const QString &path, double *temperatureC)
{
    if (!temperatureC)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    bool ok = false;
    const double milliC = QString::fromLatin1(file.readAll()).trimmed().toDouble(&ok);
    if (!ok)
        return false;

    *temperatureC = milliC / 1000.0;
    return true;
}

} // namespace

MAX31760::MAX31760(QObject *parent)
    : QObject(parent)
{
    i2cDev = QStringLiteral("/dev/i2c-0");
    qDebug() << "MAX31760 i2cDev" << i2cDev;

    ReadWrite = new I2CReadWrite(i2cDev.toLocal8Bit().constData(), MAX31760_MIN_ADDR);

    // Preserve the historical startup profile, but execute it through a
    // bounded in-process I2C write instead of system("i2ctransfer ...").
    if (writeProfilePayload(0x54, 0x11, 0xc8, "startup-profile"))
        m_lastFanProfile = 0;
}

MAX31760::~MAX31760()
{
    delete ReadWrite;
    ReadWrite = nullptr;
}

bool MAX31760::writeProfilePayload(int address, quint8 controlByte, quint8 profileByte, const char *reason)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs < m_nextI2cRetryMs)
        return false;

    std::array<unsigned char, 93> payload = kProfilePayload;
    payload[kControlByteIndex] = controlByte;
    payload[kProfileByteIndex] = profileByte;

    const QByteArray devicePath = QFile::encodeName(i2cDev);
    const int fd = ::open(devicePath.constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        logI2cFailure(reason, QStringLiteral("open %1: %2")
                                  .arg(i2cDev, QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    // Bound kernel-side I2C transactions as well. Linux i2c-dev expresses
    // I2C_TIMEOUT in 10 ms units; ignore adapters that do not implement it.
    (void)::ioctl(fd, I2C_TIMEOUT, 100); // ~1 second

    if (::ioctl(fd, I2C_SLAVE, address) < 0) {
        const int savedErrno = errno;
        ::close(fd);
        logI2cFailure(reason, QStringLiteral("select slave 0x%1: %2")
                                  .arg(address, 0, 16)
                                  .arg(QString::fromLocal8Bit(std::strerror(savedErrno))));
        return false;
    }

    const ssize_t written = ::write(fd, payload.data(), payload.size());
    const int savedErrno = errno;
    ::close(fd);

    if (written != static_cast<ssize_t>(payload.size())) {
        const QString errorText = (written < 0)
                ? QString::fromLocal8Bit(std::strerror(savedErrno))
                : QStringLiteral("short write %1/%2").arg(static_cast<qlonglong>(written)).arg(static_cast<qulonglong>(payload.size()));
        logI2cFailure(reason, QStringLiteral("write slave 0x%1: %2")
                                  .arg(address, 0, 16)
                                  .arg(errorText));
        return false;
    }

    m_nextI2cRetryMs = 0;
    m_i2cRetryDelayMs = 5000;
    if (m_i2cFaultLogged) {
        qInfo() << "[MAX31760] I2C communication recovered";
        m_i2cFaultLogged = false;
    }
    return true;
}

void MAX31760::logI2cFailure(const char *reason, const QString &detail)
{
    // A missing/unpowered optional fan controller must never terminate the UI
    // or hammer the I2C bus/journald. Retry with bounded exponential backoff.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_nextI2cRetryMs = nowMs + m_i2cRetryDelayMs;
    m_i2cRetryDelayMs = qMin(60000, m_i2cRetryDelayMs * 2);

    if (!m_i2cFaultLogged) {
        qWarning().noquote() << "[MAX31760] I2C profile write failed"
                             << "reason=" << (reason ? reason : "unknown")
                             << "detail=" << detail
                             << "retry_ms=" << m_i2cRetryDelayMs / 2;
        m_i2cFaultLogged = true;
    }
}

int MAX31760::readFanRpm1()
{
    if (!ReadWrite || !ReadWrite->active)
        return -1;

    // Select TACH1 high register, then read the two-byte tachometer count.
    ReadWrite->buffer[0] = MAX31760_TC1H;
    ReadWrite->length = 1;
    if (!ReadWrite->writeBytes())
        return -1;

    ReadWrite->length = 2;
    if (!ReadWrite->readi2cData())
        return -1;

    const quint16 count = (static_cast<quint16>(ReadWrite->r_buffer[0]) << 8)
                        | static_cast<quint16>(ReadWrite->r_buffer[1]);
    if (count == 0 || count == 0xffff)
        return -1;

    constexpr int pulsesPerRevolution = 2;
    return static_cast<int>((60.0 * (100000.0 / count)) / pulsesPerRevolution);
}

int MAX31760::tempDetect()
{
    double cpuTemp = 0.0;
    double gpuTemp = 0.0;

    if (!readThermalCelsius(QStringLiteral("/sys/devices/virtual/thermal/thermal_zone0/temp"), &cpuTemp)
            || !readThermalCelsius(QStringLiteral("/sys/devices/virtual/thermal/thermal_zone1/temp"), &gpuTemp)) {
        // The old implementation called exit(EXIT_FAILURE) from the fan worker
        // thread here, terminating the entire application. Thermal telemetry is
        // optional for UI survival: retain the last safe fan profile and retry.
        if (!m_thermalFaultLogged) {
            qWarning() << "[MAX31760] thermal telemetry unavailable; retaining last fan profile";
            m_thermalFaultLogged = true;
        }
        return -1;
    }

    if (m_thermalFaultLogged) {
        qInfo() << "[MAX31760] thermal telemetry recovered";
        m_thermalFaultLogged = false;
    }

    cputemp = static_cast<float>(cpuTemp);
    gputemp = static_cast<float>(gpuTemp);
    qDebug() << "Cpu:" << cputemp << "Gpu:" << gputemp;

    int profile = 4;
    quint8 profileByte = 0xff;

    // Preserve the historical temperature bands exactly. Values outside the
    // explicit bands use the previous fail-safe full-duty profile.
    if (cpuTemp >= 35.0 && cpuTemp <= 40.0) {
        profile = 0;
        profileByte = 0xc8;
    } else if (cpuTemp > 40.0 && cpuTemp < 45.0) {
        profile = 1;
        profileByte = 0xe1;
    } else if (cpuTemp > 45.0 && cpuTemp < 50.0) {
        profile = 2;
        profileByte = 0xe6;
    } else if (cpuTemp > 50.0 && cpuTemp < 55.0) {
        profile = 3;
        profileByte = 0xeb;
    }

    // Avoid hammering I2C every second when the requested profile did not
    // change. On failure keep m_lastFanProfile unchanged so the worker retries.
    if (profile != m_lastFanProfile) {
        if (!writeProfilePayload(0x54, 0x11, profileByte, "temperature-profile"))
            return -1;
        m_lastFanProfile = profile;
    }

    return 0;
}

void MAX31760::safemode()
{
    if (writeProfilePayload(0x50, 0x01, 0x00, "safe-mode"))
        qDebug() << "MAX31760 safe mode";
}

void MAX31760::normalmode()
{
    if (writeProfilePayload(0x50, 0x11, 0x32, "normal-mode"))
        qDebug() << "MAX31760 normal mode";
}

QString MAX31760::checkDevice()
{
    return QStringLiteral("/dev/i2c-0");
}
