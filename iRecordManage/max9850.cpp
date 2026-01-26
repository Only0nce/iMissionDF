#include "max9850.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <cstring>


Max9850::Max9850(const QString &i2cBusPath, uint8_t deviceAddr)
    : m_i2cFile(-1),
    m_deviceAddr(deviceAddr),
    m_i2cBusPath(i2cBusPath),
    m_sysclk(0)
{
}

Max9850::~Max9850()
{
    closeBus();
}

QString Max9850::i2cDevPath(const QString &busPath)
{
    return QString("/dev/i2c-%1").arg(busPath);
}

bool Max9850::openBus(const QString &busPath)
{
    QString path = i2cDevPath(busPath);
    m_i2cFile = open(path.toLocal8Bit().constData(), O_RDWR);
    if (m_i2cFile < 0) {
        qWarning() << "Failed to open I2C device:" << path << "-" << strerror(errno);
        return false;
    }

    if (ioctl(m_i2cFile, I2C_SLAVE, m_deviceAddr) < 0) {
        //        qWarning() << "Failed to set I2C address:" << Qt::hex << m_deviceAddr << "-" << strerror(errno);
        close(m_i2cFile);
        m_i2cFile = -1;
        return false;
    }

    return true;
}

void Max9850::closeBus()
{
    if (m_i2cFile >= 0) {
        close(m_i2cFile);
        m_i2cFile = -1;
    }
}

bool Max9850::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = { reg, value };
    if (write(m_i2cFile, buffer, 2) != 2) {
        qWarning() << "Failed to write register";
        return false;
    }
    return true;
}

bool Max9850::readRegister(uint8_t reg, uint8_t &value)
{
    if (write(m_i2cFile, &reg, 1) != 1) {
        qWarning() << "Failed to select register";
        return false;
    }

    if (read(m_i2cFile, &value, 1) != 1) {
        qWarning() << "Failed to read register";
        return false;
    }

    return true;
}

bool Max9850::updateBits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current;
    if (!readRegister(reg, current))
        return false;
    uint8_t newVal = (current & ~mask) | (value & mask);
    return writeRegister(reg, newVal);
}

bool Max9850::setSysClock(uint32_t freq)
{
    m_sysclk = freq;

    if (freq <= 13000000)
        return writeRegister(MAX9850_CLOCK, 0x00);
    else if (freq <= 26000000)
        return writeRegister(MAX9850_CLOCK, 0x04);
    else if (freq <= 40000000)
        return writeRegister(MAX9850_CLOCK, 0x08);

    return false;
}

bool Max9850::setFormat(uint8_t fmt)
{
    return writeRegister(MAX9850_DIGITAL_AUDIO, fmt);
}

bool Max9850::setVolume(uint8_t vol)
{
    if (vol > 0x3F) {
        qWarning() << "Volume out of range (0–63):" << vol;
        return false;
    }

    return updateBits(MAX9850_VOLUME, 0x3F, vol);
}

bool Max9850::initialize(uint32_t sampleRate)
{
    if (!openBus(m_i2cBusPath))
        return false;
    uint8_t status0 = 0;
    uint8_t status1 = 0;
    readRegister(MAX9850_STATUSA, status0);
    readRegister(MAX9850_STATUSB, status1);
    qDebug() << "STATUSA:" << status0 << "STATUSB:" << status1;
    //    writeRegister(0x01, 0x80); // Enable PLL
    //    writeRegister(0x02, 0x10); // Left-Justified, 16-bit, Slave
    //    writeRegister(0x03, 0x00); // Default filters
    //    writeRegister(0x04, 0x00); // Default DAC gain
    //    writeRegister(0x05, 0x0F); // Mid volume
    //    writeRegister(0x06, 0x2A); // PLL config for 8kHz (tuned as per datasheet)
    //    writeRegister(0x07, 0xC0); // Enable DAC and interface

    //    if (!writeRegister(MAX9850_GENERAL_PURPOSE, 0x94)) return false;   // 0x03
    //    if (!writeRegister(MAX9850_INTERRUPT, 0x35)) return false;         // 0x04
    //    if (!writeRegister(MAX9850_ENABLE, 0xFF)) return false;            // 0x05
    //    if (!writeRegister(MAX9850_CLOCK, 0x00)) return false;             // 0x06
    //    if (!writeRegister(MAX9850_CHARGE_PUMP, 0x68)) return false;       // 0x07
    //    if (!writeRegister(MAX9850_LRCLK_MSB, 0x1E)) return false;         // 0x08
    //    if (!writeRegister(MAX9850_LRCLK_LSB, 0x00)) return false;         // 0x09
    //    if (!writeRegister(MAX9850_DIGITAL_AUDIO, 0x02)) return false;     // 0x0A
    //    if (!writeRegister(MAX9850_RESERVED, 0x01)) return false;          // 0x0B
    //    if (!writeRegister(MAX9850_VOLUME, 0x00)) return false;            // 0x02 - Set last

    if (!writeRegister(MAX9850_VOLUME, 0x0F)) return false;             // 0x02
    if (!writeRegister(MAX9850_GENERAL_PURPOSE, 0x90)) return false;   // 0x03
    if (!writeRegister(MAX9850_INTERRUPT, 0x35)) return false;         // 0x04
    if (!writeRegister(MAX9850_ENABLE, 0xFF)) return false;            // 0x05
    if (!writeRegister(MAX9850_CLOCK, 0x00)) return false;             // 0x06
    if (!writeRegister(MAX9850_CHARGE_PUMP, 0x68)) return false;       // 0x07
    if (!writeRegister(MAX9850_LRCLK_MSB, 0x1E)) return false;         // 0x08
    if (!writeRegister(MAX9850_LRCLK_LSB, 0x00)) return false;         // 0x09
    if (!writeRegister(MAX9850_DIGITAL_AUDIO, 0x02)) return false;     // 0x0A
    if (!writeRegister(MAX9850_RESERVED, 0x01)) return false;          // 0x0A
    //    writeRegister(0x0C, 0x00);
    return true;
}
