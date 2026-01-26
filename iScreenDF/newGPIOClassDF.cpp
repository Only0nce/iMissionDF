#include "newGPIOClassDF.h"
#include <iostream>

newGPIOClassDF::newGPIOClassDF(const std::string &chipName, unsigned int lineNum, const std::string &consumer)
    : chipName(chipName), lineNum(lineNum), consumer(consumer) {

    chip = gpiod_chip_open_by_name(chipName.c_str());
    if (!chip) {
        std::cerr << "Failed to open GPIO chip: " << chipName << std::endl;
        return;
    }

    line = gpiod_chip_get_line(chip, lineNum);
    if (!line) {
        std::cerr << "Failed to get line " << lineNum << " from " << chipName << std::endl;
        gpiod_chip_close(chip);
        chip = nullptr;
    }
}

newGPIOClassDF::~newGPIOClassDF() {
    release();
}

bool newGPIOClassDF::requestOutput() {
    if (!line) return false;

    if (gpiod_line_request_output(line, consumer.c_str(), 0) < 0) {
        std::cerr << "Failed to request output line " << lineNum << std::endl;
        return false;
    }
    isOutput = true;
    return true;
}

bool newGPIOClassDF::requestInput() {
    if (!line) return false;

    if (gpiod_line_request_input(line, consumer.c_str()) < 0) {
        std::cerr << "Failed to request input line " << lineNum << std::endl;
        return false;
    }
    isOutput = false;
    return true;
}

bool newGPIOClassDF::setValue(bool value) {
    if (!line || !isOutput) return false;

    if (gpiod_line_set_value(line, value ? 1 : 0) < 0) {
        std::cerr << "Failed to set value on line " << lineNum << std::endl;
        return false;
    }
    return true;
}

int newGPIOClassDF::getValue(bool &value) {
    if (!line || isOutput) return -1;

    int val = gpiod_line_get_value(line);
    if (val < 0) {
        std::cerr << "Failed to get value from line " << lineNum << std::endl;
        return -1;
    }
    value = val ? true : false;
    return 0;
}

void newGPIOClassDF::release() {
    if (line) {
        gpiod_line_release(line);
        line = nullptr;
    }

    if (chip) {
        gpiod_chip_close(chip);
        chip = nullptr;
    }
}
