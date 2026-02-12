#ifndef NEWGPIOCLASSDF_H
#define NEWGPIOCLASSDF_H

#include <gpiod.h>
#include <string>

class newGPIOClassDF {
public:
    newGPIOClassDF(const std::string &chipName, unsigned int lineNum, const std::string &consumer = "gpioctl");
    ~newGPIOClassDF();

    bool setValue(bool value);
    int  getValue(bool &value);
    bool requestOutput();
    bool requestInput();
    void release();

private:
    std::string chipName;
    unsigned int lineNum;
    std::string consumer;

    gpiod_chip *chip = nullptr;
    gpiod_line *line = nullptr;
    bool isOutput = false;
};

#endif // NEWGPIOCLASSDF_H
