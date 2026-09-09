#include "I2CReadWrite.h"
#include "QtDebug"
#include <cstdio>

I2CReadWrite::I2CReadWrite(const char* i2c_dev, int i2c_address)
{
    i2cAddress = i2c_address;
    std::snprintf(i2cdevice,
                  sizeof(i2cdevice),
                  "%s",
                  i2c_dev ? i2c_dev : "");

    if (!i2c_dev || i2c_dev[0] == '\0') {
        qWarning() << "I2C device path is empty";
        return;
    }

    file_i2c = open(i2c_dev, O_RDWR);
    if (file_i2c < 0) {
        qWarning() << "Failed to open I2C device" << i2cdevice;
        return;
    }

    if (ioctl(file_i2c, I2C_SLAVE, i2c_address) < 0) {
        qWarning() << "Failed to select I2C slave"
                   << i2cdevice
                   << QString::number(i2c_address, 16);
        close(file_i2c);
        file_i2c = -1;
        return;
    }

    active = true;
    qDebug() << "I2C ready" << i2cdevice << file_i2c;
}

I2CReadWrite::~I2CReadWrite()
{
    active = false;
    if (file_i2c >= 0) {
        close(file_i2c);
        file_i2c = -1;
    }
}

bool I2CReadWrite::readi2cData()
{
    if (!active || file_i2c < 0 || length <= 0 ||
        length > static_cast<int>(sizeof(r_buffer))) {
        return false;
    }

    int len = read(file_i2c, r_buffer, length);
    if (len != length) {
        printf("Failed to read from the i2c bus %s 0x%02x. 0x%02x 0x%02x 0x%02x 0x%02x\n",i2cdevice, i2cAddress, r_buffer[0],r_buffer[1], length, len);
        return  false;
    }
    else {
//        for (int i=0;i<60;i++) {
//            if (r_buffer[i] == 0x40)
//                qDebug() << i << r_buffer[i] ;
//        }
        //qDebug() << r_buffer[0] << r_buffer[1] << r_buffer[2] << r_buffer[3] << r_buffer[4] << r_buffer[5] << r_buffer[6] << r_buffer[7] << r_buffer[8] << r_buffer[9] << r_buffer[10];
//        printf("Data read: %d %d\n", r_buffer[0],r_buffer[1]);
    }
    return true;
}
bool I2CReadWrite::writeBytes(){
    if (!active || file_i2c < 0 || length <= 0 ||
        length > static_cast<int>(sizeof(buffer))) {
        return false;
    }

    int len = write(file_i2c, buffer, length);
    if (len != length) {
        printf("Failed to write to the i2c bus %s 0x%02x. address 0x%02x data 0x%02x \n",i2cdevice, i2cAddress, buffer[0], buffer[1]);
        return false;
    }
    else {
//        printf("Write to the i2c bus. %d %d %d\n",buffer[0], buffer[1], len);
    }
    return true;
}
