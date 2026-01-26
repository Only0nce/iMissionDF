#ifndef GPIO_CLASS_H
#define GPIO_CLASS_H

#include <string>
#include <QDebug>
using namespace std;
/* GPIO Class
 * Purpose: Each object instantiated from this class will control a GPIO pin
 * The GPIO pin number must be passed to the overloaded class constructor
 */
class GPIOClass
{
public:
    GPIOClass();  // create a GPIO object that controls GPIO4 (default
    GPIOClass(string x); // create a GPIO object that controls GPIOx, where x is passed to this constructor
    GPIOClass(int gnum);
    int export_gpio(); // exports GPIO
    int unexport_gpio(); // unexport GPIO
    int set_edge(string edge);
    int setdir_gpio(string dir); // Set GPIO Direction
    //    int setval_gpio(string val); // Set GPIO Value (putput pins)
    int setval_gpio(bool val);
    int setGpio();
    int resetGpio();
    int getval_gpio(string& val); // Get GPIO Value (input/ output pins)
    bool getGpioVal();
    string get_gpionum(); // return the GPIO number associated with the instance of an object

private:
    string gpionum; // GPIO number associated with the instance of an object
    string gpioname = "";

};

#endif
