#ifndef NEWGPIOCLASS_H
#define NEWGPIOCLASS_H

#include <gpiod.h>
#include <string>

class newGPIOClass {
public:
    newGPIOClass(const std::string &chipName, unsigned int lineNum, const std::string &consumer = "gpioctl");
    ~newGPIOClass();

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
    // STAB2: output lines are driven exclusively by this object. Cache the
    // successfully driven logical value so callers can read state without
    // issuing an invalid input-style read on an output request.
    bool hasCachedValue = false;
    bool cachedValue = false;
};

#endif // NEWGPIOCLASS_H
