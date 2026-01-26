#include "GPIOClass.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <QString>
#include <QStringList>
#include <QDebug>

#include <unistd.h>
#include <ios>
#include <fstream>


using namespace std;

GPIOClass::GPIOClass()
{
    this->gpionum = "4"; //GPIO4 is default
}

GPIOClass::GPIOClass(string gnum)
{
    this->gpionum = gnum;
    this->gpioname = gpionum;
    export_gpio();
}
GPIOClass::GPIOClass(int gnum)
{
    this->gpionum = QString::number(gnum).toStdString();
    this->gpioname = "gpio"+gpionum;
    export_gpio();
}
int GPIOClass::export_gpio()
{
    string export_str = "/sys/class/gpio/export";
    ofstream exportgpio(export_str.c_str()); // Open "export" file. Convert C++ string to C string. Required for all Linux pathnames
    if (exportgpio.fail()){
        cout << " OPERATION FAILED: Unable to export GPIO \""<< this->gpionum <<"\" ."<< endl;
        return -1;
    }

    exportgpio << this->gpionum ; //write GPIO number to export
    exportgpio.close(); //close export file
    return 0;
}

int GPIOClass::unexport_gpio()
{
    string unexport_str = "/sys/class/gpio/unexport";
    ofstream unexportgpio(unexport_str.c_str()); //Open unexport file
    if (unexportgpio.fail()){
        cout << " OPERATION FAILED: Unable to unexport GPIO"<< this->gpionum <<" ."<< endl;
        return -1;
    }

    unexportgpio << this->gpionum ; //write GPIO number to unexport
    unexportgpio.close(); //close unexport file
    return 0;
}
int GPIOClass::set_edge(string edge)
{
    string setdir_str ="/sys/class/gpio/" + this->gpioname + "/edge";
    ofstream setdirgpio(setdir_str.c_str()); // open edge file for gpio
    if (setdirgpio.fail()){
        cout << " OPERATION FAILED: Unable to set edge of GPIO"<< this->gpioname <<" ."<< endl;
        return -1;
    }

    setdirgpio << edge; //write edge to direction file
    setdirgpio.close(); // close direction file
    return 0;
}
int GPIOClass::setdir_gpio(string dir)
{

    string setdir_str ="/sys/class/gpio/" + this->gpioname + "/direction";
    ofstream setdirgpio(setdir_str.c_str()); // open direction file for gpio
    if (setdirgpio.fail()){
        cout << " OPERATION FAILED: Unable to set direction of GPIO "<< this->gpioname << ": " << setdir_str << endl;
        return -1;
    }

    setdirgpio << dir; //write direction to direction file
    setdirgpio.close(); // close direction file
    return 0;
}
int GPIOClass::setGpio(){
    return setval_gpio(true);
}
int GPIOClass::resetGpio(){
    return setval_gpio(false);
}

//int GPIOClass::setval_gpio(string val)
//{

//    string setval_str = "/sys/class/gpio/" + this->gpioname + "/value";
//    qDebug() << QString::fromStdString(setval_str);
//    ofstream setvalgpio(setval_str.c_str()); // open value file for gpio
//        if (setvalgpio.fail()){
//            cout << " OPERATION FAILED: Unable to set the value of GPIO"<< this->gpionum <<" ."<< endl;
//            return -1;
//        }

//        setvalgpio << val ;//write value to value file
//        setvalgpio.close();// close value file
//        return 0;
//}
int GPIOClass::setval_gpio(bool val)
{
    QString strVal = "0";
    if (val) strVal = "1";
    string setval_str = "/sys/class/gpio/" + this->gpioname + "/value";
    //    qDebug() << QString::fromStdString(setval_str) << strVal;
    ofstream setvalgpio(setval_str.c_str()); // open value file for gpio
    if (setvalgpio.fail()){
        cout << " OPERATION FAILED: Unable to set the value of GPIO"<< this->gpioname <<" ."<< endl;
        return -1;
    }

    setvalgpio << val ;//write value to value file
    setvalgpio.close();// close value file
    return 0;
}
int GPIOClass::getval_gpio(string& val){
    string getval_str = "/sys/class/gpio/" + this->gpioname + "/value";
    ifstream getvalgpio(getval_str.c_str());// open value file for gpio
    if (getvalgpio.fail()){
        //        cout << " OPERATION FAILED: Unable to get value of GPIO"<< this->gpionum <<" ."<< endl;
        return -1;
    }

    getvalgpio >> val ;  //read gpio value

    if(val != "0")
        val = "1";
    else
        val = "0";

    getvalgpio.close(); //close the value file
    return 0;
}
bool GPIOClass::getGpioVal(){
    string val;
    string getval_str = "/sys/class/gpio/" + this->gpioname + "/value";
    ifstream getvalgpio(getval_str.c_str());// open value file for gpio
    if (getvalgpio.fail()){
        //        cout << " OPERATION FAILED: Unable to get value of GPIO"<< this->gpionum <<" ."<< endl;
        return true;
    }

    getvalgpio >> val ;  //read gpio value
    getvalgpio.close(); //close the value file
    if(val != "0"){
        val = "1";
        return true;
    }
    else{
        val = "0";
        return false;
    }
    return true;
}


string GPIOClass::get_gpionum(){

    return this->gpionum;

}

