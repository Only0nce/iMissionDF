#ifndef MAX9850_H
#define MAX9850_H

#include <QObject>
#include <QString>

#define MAX9850_STATUSA			0x00
#define MAX9850_STATUSB			0x01
#define MAX9850_VOLUME			0x02
#define MAX9850_GENERAL_PURPOSE	0x03
#define MAX9850_INTERRUPT		0x04
#define MAX9850_ENABLE			0x05
#define MAX9850_CLOCK			0x06
#define MAX9850_CHARGE_PUMP		0x07
#define MAX9850_LRCLK_MSB		0x08
#define MAX9850_LRCLK_LSB		0x09
#define MAX9850_DIGITAL_AUDIO		0x0a
#define MAX9850_RESERVED		0x0b

// Digital Audio Settings
#define MAX9850_MASTER  0x80
#define MAX9850_SLAVE   0x04

#define MAX9850_INV     0x40
#define MAX9850_BCINV   0x20
#define MAX9850_RTJ     0x10
#define MAX9850_DLY     0x08
#define MAX9850_LTJ     0x00

class Max9850
{
public:
    Max9850(const QString &i2cBusPath, uint8_t deviceAddr);
    ~Max9850();

    bool initialize(uint32_t sampleRate);
    bool setVolume(uint8_t vol);           // 0x00 - 0x3F
    bool setFormat(uint8_t fmt);
    bool setSysClock(uint32_t freqHz);

private:
    int m_i2cFile;
    uint8_t m_deviceAddr;
    QString m_i2cBusPath;
    uint32_t m_sysclk;

    QString i2cDevPath(const QString &busPath);
    bool openBus(const QString &busPath);
    void closeBus();
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t &value);
    bool updateBits(uint8_t reg, uint8_t mask, uint8_t value);
    bool check=false;
};

#endif // MAX9850_H
