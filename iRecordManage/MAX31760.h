#ifndef MAX31760_H
#define MAX31760_H

#include <QObject>
#include <QProcess>
#include "I2CReadWrite.h"
#include <QThread>
#define MAX31760_MIN_ADDR      0x50  //Default starting address
#define MAX31760_MAX_ADDR      0x57  //Max address

// Register memory ranges ##
#define MAX31760_ALLMEM_START  0x00
#define MAX31760_ALLMEM_END    0x5F  //the end of addressable range is actually 0x5B
#define MAX31760_USERMEM_START 0x10  //8 Bytes of General-Purpose User Memory
#define MAX31760_USERMEM_END   0x17  //
#define MAX31760_LUTMEM_START  0x20  //48-Byte Lookup Table (LUT)
#define MAX31760_LUTMEM_END    0x4F  //

// Register memory map ##
#define MAX31760_CR1           0x00  //Control Register 1
#define MAX31760_CR2           0x01  //Control Register 2
#define MAX31760_CR3           0x02  //Control Register 3
#define MAX31760_FFDC          0x03  //Fan Fault Duty Cycle
#define MAX31760_MASK          0x04  //Alert Mask Register
#define MAX31760_IFR           0x05  //Ideality Factor Register
#define MAX31760_RHSH          0x06  //Remote High Set-point MSB
#define MAX31760_RHSL          0x07  //Remote High Set-point LSB
#define MAX31760_LOTSH         0x08  //Local Overtemperature Set-point MSB
#define MAX31760_LOTSL         0x09  //Local Overtemperature Set-point LSB
#define MAX31760_ROTSH         0x0A  //Remote Overtemperature Set-point MSB
#define MAX31760_ROTSL         0x0B  //Remote Overtemperature Set-point LSB
#define MAX31760_LHSH          0x0C  //Local High Set-point MSB
#define MAX31760_LHSL          0x0D  //Local High Set-point LSB
#define MAX31760_TCTH          0x0E  //TACH Count Threshold Register, MSB
#define MAX31760_TCTL          0x0F  //TACH Count Threshold Register, LSB

#define MAX31760_PWMR          0x50  //Direct Duty-Cycle Control Register
#define MAX31760_PWMV          0x51  //Current PWM Duty-Cycle Register
#define MAX31760_TC1H          0x52  //TACH1 Count Register, MSB
#define MAX31760_TC1L          0x53  //TACH1 Count Register, LSB
#define MAX31760_TC2H          0x54  //TACH2 Count Register, MSB
#define MAX31760_TC2L          0x55  //TACH2 Count Register, LSB
#define MAX31760_RTH           0x56  //Remote Temperature Reading Register, MSB
#define MAX31760_RTL           0x57  //Remote Temperature Reading Register, MSB
#define MAX31760_LTH           0x58  //Local Temperature Reading Register, MSB
#define MAX31760_LTL           0x59  //Local Temperature Reading Register, MSB
#define MAX31760_SR            0x5A  //Status Register

// Commands ##
#define MAX31760_CMD_EEPROM    0x5B

// Parameters ##
#define MAX31760_EEPROM_READ   0x9F
#define MAX31760_EEPROM_WRITE  0x1F

// Control Register 1 ##
#define MAX31760_CTRL1_MASK_TEMP_ALERTS    0x80
#define MAX31760_CTRL1_SW_POR              0x40
#define MAX31760_CTRL1_LUT_HYST_2_CELS     0x00
#define MAX31760_CTRL1_LUT_HYST_4_CELS     0x20
#define MAX31760_CTRL1_PWM_FREQ_33_HZ      0x00
#define MAX31760_CTRL1_PWM_FREQ_150_HZ     0x08
#define MAX31760_CTRL1_PWM_FREQ_1500_HZ    0x10
#define MAX31760_CTRL1_PWM_FREQ_25_KHZ     0x18
#define MAX31760_CTRL1_PWM_POL_POS         0x00
#define MAX31760_CTRL1_PWM_POL_NEG         0x04
#define MAX31760_CTRL1_TEMP_IDX_TIS        0x00
#define MAX31760_CTRL1_TEMP_IDX_GREATER    0x02
#define MAX31760_CTRL1_LUT_IDX_LOCAL_TEMP  0x00
#define MAX31760_CTRL1_LUT_IDX_REMOTE_TEMP 0x01

// Control Register 2 ##
#define MAX31760_CTRL2_NORMAL_MODE         0x00
#define MAX31760_CTRL2_STANDBY_MODE        0x80
#define MAX31760_CTRL2_ALERT_INTERR        0x00
#define MAX31760_CTRL2_ALERT_COMP          0x40
#define MAX31760_CTRL2_SPIN_UP_EN          0x20
#define MAX31760_CTRL2_FF_OUTPUT_INTERR    0x00
#define MAX31760_CTRL2_FF_OUTPUT_COMP      0x10
#define MAX31760_CTRL2_FS_INPUT_EN         0x08
#define MAX31760_CTRL2_RD_ACTIVE_LOW       0x00
#define MAX31760_CTRL2_RD_ACTIVE_HIGH      0x04
#define MAX31760_CTRL2_TACHO_SQUARE_WAVE   0x00
#define MAX31760_CTRL2_TACHO_RD            0x02
#define MAX31760_CTRL2_DIRECT_FAN_CTRL_EN  0x01

// Control Register 3 ##
#define MAX31760_CTRL3_CLR_FAIL                  0x80
#define MAX31760_CTRL3_FF_DETECT_EN              0x40
#define MAX31760_CTRL3_PWM_RAMP_RATE_SLOW        0x00
#define MAX31760_CTRL3_PWM_RAMP_RATE_SLOW_MEDIUM 0x10
#define MAX31760_CTRL3_PWM_RAMP_RATE_MEDIUM_FAST 0x20
#define MAX31760_CTRL3_PWM_RAMP_RATE_FAST        0x30
#define MAX31760_CTRL3_TACHFULL_EN               0x08
#define MAX31760_CTRL3_PULSE_STRETCH_EN          0x04
#define MAX31760_CTRL3_TACH2_EN                  0x02
#define MAX31760_CTRL3_TACH1_EN                  0x01

// Alert Mask ##
#define MAX31760_ALERT_MASK_LOCAL_TEMP_HIGH      0x20
#define MAX31760_ALERT_MASK_LOCAL_OVERTEMP       0x10
#define MAX31760_ALERT_MASK_REMOTE_TEMP_HIGH     0x08
#define MAX31760_ALERT_MASK_REMOTE_OVERTEMP      0x04
#define MAX31760_ALERT_MASK_TACH2                0x02
#define MAX31760_ALERT_MASK_TACH1                0x01
#define MAX31760_ALERT_MASK_RESERVED_BITS        0xC0
#define MAX31760_ALERT_MASK_NONE                 0x00

// Status ##
#define MAX31760_STAT_FLAG_PROGRAM_CORRUPT        0x80
#define MAX31760_STAT_FLAG_REMOTE_DIODE_FAULT     0x40
#define MAX31760_STAT_FLAG_LOCAL_HIGH_TEMP_ALARM  0x20
#define MAX31760_STAT_FLAG_LOCAL_OVERTEMP_ALARM   0x10
#define MAX31760_STAT_FLAG_REMOTE_HIGH_TEMP_ALARM 0x08
#define MAX31760_STAT_FLAG_REMOTE_OVERTEMP_ALARM  0x04
#define MAX31760_STAT_FLAG_TACH2_ALARM            0x02
#define MAX31760_STAT_FLAG_TACH1_ALARM            0x01

// Temperature ##
#define MAX31760_MAX_TEMP_CELS   = 125
#define MAX31760_ZERO_TEMP_CELS  = 0
#define MAX31760_MIN_TEMP_CELS   = -55

// LUT bytes ##
#define MAX31760_LUT_NBYTES      = 48

// Temp and PWM speed register resolution ##
#define MAX31760_RESOL_TEMP_CELS = 0.125
#define MAX31760_RESOL_SPEED_PER = 0.392156862745098

class MAX31760 : public QObject
{
    Q_OBJECT
public:
    explicit MAX31760(QObject *parent = nullptr);
    int readFanRpm1();
    int tempDetect();
    MAX31760();
    float gputemp = 0;
    float cputemp = 0;
    int count = 0;
signals:
    void transferdata(QString);
public slots:
    void safemode();
    void normalmode();
private:
    I2CReadWrite *ReadWrite;
    QProcess* Process;
    QString i2cDev;
    QString checkDevice();


};

#endif // MAX31760_H
