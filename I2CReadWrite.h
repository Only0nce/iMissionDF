#ifndef I2CREADWRITE_H
#define I2CREADWRITE_H
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string>

class I2CReadWrite
{
public:
    I2CReadWrite(const char* i2c_dev, int i2c_address);
    bool readi2cData();
    bool writeBytes();
    int file_i2c;
    unsigned char buffer[60];
    unsigned char r_buffer[100];
    int length=2;
    char i2cdevice[16];
    int i2cAddress;
    bool active = false;
//    void setup(const char* i2c_dev, int i2c_address);
};

#endif // I2CREADWRITE_H
