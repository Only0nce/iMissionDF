#ifndef HMC253CONTROLLER_H
#define HMC253CONTROLLER_H

#include "newGPIOClass.h"
#include <array>
#include <QDebug>

// RF Switch GPIO definitions
#define GPIO_A_SW_IN  "gpiochip3", 9
#define GPIO_B_SW_IN  "gpiochip3", 8
#define GPIO_C_SW_IN  "gpiochip3", 10

#define GPIO_A_SW_OUT "gpiochip3", 12
#define GPIO_B_SW_OUT "gpiochip3", 14
#define GPIO_C_SW_OUT "gpiochip3", 15

// Mapping RF port to actual control bit pattern (ABC)
enum class RFPort : uint8_t {
    RF1 = 0b000,
    RF2 = 0b100,
    RF3 = 0b010,
    RF4 = 0b110,
    RF5 = 0b001,
    RF6 = 0b101,
    RF7 = 0b011,
    RF8 = 0b111
};

class HMC253Controller {
public:
    HMC253Controller() {
        inPins = {
            new newGPIOClass(GPIO_A_SW_IN),
            new newGPIOClass(GPIO_B_SW_IN),
            new newGPIOClass(GPIO_C_SW_IN)
        };

        outPins = {
            new newGPIOClass(GPIO_A_SW_OUT),
            new newGPIOClass(GPIO_B_SW_OUT),
            new newGPIOClass(GPIO_C_SW_OUT)
        };

        for (auto pin : inPins)
            if (!pin->requestOutput())
                qWarning("Failed to request output for input pin");

        for (auto pin : outPins)
            if (!pin->requestOutput())
                qWarning("Failed to request output for output pin");
    }

    ~HMC253Controller() {
        for (auto pin : inPins) delete pin;
        for (auto pin : outPins) delete pin;
    }

    void selectRF(RFPort port) {
        uint8_t value = static_cast<uint8_t>(port);
        uint8_t bitA = (value >> 2) & 1;
        uint8_t bitB = (value >> 1) & 1;
        uint8_t bitC = (value >> 0) & 1;

        qDebug() << "Selected RF:" << toString(port)
                 << QString(" -> A:%1 B:%2 C:%3").arg(bitA).arg(bitB).arg(bitC);

        setPins(inPins, value);
        setPins(outPins, value);
    }
    RFPort mapInputToOutput(RFPort inPort) {
        switch (inPort) {
        case RFPort::RF1: return RFPort::RF7;
        case RFPort::RF2: return RFPort::RF6;
        case RFPort::RF3: return RFPort::RF5;
        case RFPort::RF4: return RFPort::RF4;
        case RFPort::RF5: return RFPort::RF3;
        case RFPort::RF6: return RFPort::RF2;
        case RFPort::RF7: return RFPort::RF1;
        case RFPort::RF8: return RFPort::RF8; // NC
        default:         return RFPort::RF8;
        }
    }

    void selectRFPair(RFPort inputRF) {
        RFPort outputRF = mapInputToOutput(inputRF);

        qDebug() << "Input RF:" << toString(inputRF)
                 << "→ Output RF:" << toString(outputRF);

        setPins(inPins, static_cast<uint8_t>(inputRF));
        setPins(outPins, static_cast<uint8_t>(outputRF));

        // Debug bits
        auto dbgBits = [](uint8_t value) {
            return QString("A:%1 B:%2 C:%3")
            .arg((value >> 2) & 1)
                .arg((value >> 1) & 1)
                .arg(value & 1);
        };

        qDebug() << "IN  bits:" << dbgBits(static_cast<uint8_t>(inputRF));
        qDebug() << "OUT bits:" << dbgBits(static_cast<uint8_t>(outputRF));
    }

private:
    std::array<newGPIOClass*, 3> inPins;
    std::array<newGPIOClass*, 3> outPins;


    void setPins(const std::array<newGPIOClass*, 3>& pins, uint8_t value) {
        pins[0]->setValue((value >> 2) & 1);  // A
        pins[1]->setValue((value >> 1) & 1);  // B
        pins[2]->setValue((value >> 0) & 1);  // C
    }

    QString toString(RFPort port) const {
        switch (port) {
        case RFPort::RF1: return "RF1";
        case RFPort::RF2: return "RF2";
        case RFPort::RF3: return "RF3";
        case RFPort::RF4: return "RF4";
        case RFPort::RF5: return "RF5";
        case RFPort::RF6: return "RF6";
        case RFPort::RF7: return "RF7";
        case RFPort::RF8: return "RF8";
        default: return "Unknown";
        }
    }
};

#endif // HMC253CONTROLLER_H
