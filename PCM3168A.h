#ifndef PCM3168A_H
#define PCM3168A_H
#include "I2CReadWrite.h"
#include <QDebug>
#define CODEC1          1

#define CODEC1I2CDEV   "/dev/i2c-1" //I2S1

#define CODECI2CADDR_PCM3168A   0x44

#define CODECCH1_I2S1 1
#define CODECCH2_I2S1 2
#define CODECCH3_I2S1 3
#define CODECCH4_I2S1 4
#define CODECCH5_I2S1 5
// #define CODECCH1_I2S1 0
// #define CODECCH2_I2S1 1
// #define CODECCH3_I2S1 2
// #define CODECCH4_I2S1 3
#define SPK_MUTE      false
#define SPK_UNMUTE    true
#define AMP_SHUTDOWN  false
#define AMP_STANDBY    true
#define HEADPHONE_STANDBY    true
#define HEADPHONE_SHUTDOWN   false



class PCM3168A
{
public:
    PCM3168A(QString i2cdevice, int address);
    ~PCM3168A(); // เพิ่ม destructor
    bool setInputGain(uint8_t channel, uint8_t AINX_VOL);
    bool setOutputGain(uint8_t channel, uint8_t AOUTX_VOL);
    bool setOutputGainWithoutDSP(uint8_t channel, uint8_t AOUTX_VOL);

    bool writeReg40h(uint8_t MRST, uint8_t SRST, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t SRDA1, uint8_t SRDA0);
    bool writeReg41h(uint8_t PSMDA, uint8_t MSDA2, uint8_t MSDA1, uint8_t MSDA0, uint8_t FMTDA3, uint8_t FMTDA2, uint8_t FMTDA1, uint8_t FMTDA0);
    bool writeReg42h(uint8_t OPEDA3, uint8_t OPEDA2, uint8_t OPEDA1, uint8_t OPEDA0, uint8_t FLT3, uint8_t FLT2, uint8_t FLT1, uint8_t FLT0);
    bool writeReg43h(uint8_t REVDA8, uint8_t REVDA7, uint8_t REVDA6, uint8_t REVDA5, uint8_t REVDA4, uint8_t REVDA3, uint8_t REVDA2, uint8_t REVDA1);
    bool writeReg44h(uint8_t MUTDA8, uint8_t MUTDA7, uint8_t MUTDA6, uint8_t MUTDA5, uint8_t MUTDA4, uint8_t MUTDA3, uint8_t MUTDA2, uint8_t MUTDA1);
    bool writeReg45h(uint8_t ZERO8, uint8_t ZERO7, uint8_t ZERO6, uint8_t ZERO5, uint8_t ZERO4, uint8_t ZERO3, uint8_t ZERO2, uint8_t ZERO1);
    bool writeReg46h(uint8_t ATMDDA, uint8_t ATSPDA, uint8_t DEMP1, uint8_t DEMP0, uint8_t AZRO2, uint8_t AZRO1, uint8_t AZRO0, uint8_t ZREV);
    bool writeReg47h(uint8_t ATDA07, uint8_t ATDA06, uint8_t ATDA05, uint8_t ATDA04, uint8_t ATDA03, uint8_t ATDA02, uint8_t ATDA01, uint8_t ATDA00);
    bool writeReg48h(uint8_t ATDA17, uint8_t ATDA16, uint8_t ATDA15, uint8_t ATDA14, uint8_t ATDA13, uint8_t ATDA12, uint8_t ATDA11, uint8_t ATDA10);
    bool writeReg49h(uint8_t ATDA27, uint8_t ATDA26, uint8_t ATDA25, uint8_t ATDA24, uint8_t ATDA23, uint8_t ATDA22, uint8_t ATDA21, uint8_t ATDA20);
    bool writeReg4Ah(uint8_t ATDA37, uint8_t ATDA36, uint8_t ATDA35, uint8_t ATDA34, uint8_t ATDA33, uint8_t ATDA32, uint8_t ATDA31, uint8_t ATDA30);
    bool writeReg4Bh(uint8_t ATDA47, uint8_t ATDA46, uint8_t ATDA45, uint8_t ATDA44, uint8_t ATDA43, uint8_t ATDA42, uint8_t ATDA41, uint8_t ATDA40);
    bool writeReg4Ch(uint8_t ATDA57, uint8_t ATDA56, uint8_t ATDA55, uint8_t ATDA54, uint8_t ATDA53, uint8_t ATDA52, uint8_t ATDA51, uint8_t ATDA50);
    bool writeReg4Dh(uint8_t ATDA67, uint8_t ATDA66, uint8_t ATDA65, uint8_t ATDA64, uint8_t ATDA63, uint8_t ATDA62, uint8_t ATDA61, uint8_t ATDA60);
    bool writeReg4Eh(uint8_t ATDA77, uint8_t ATDA76, uint8_t ATDA75, uint8_t ATDA74, uint8_t ATDA73, uint8_t ATDA72, uint8_t ATDA71, uint8_t ATDA70);
    bool writeReg4Fh(uint8_t ATDA87, uint8_t ATDA86, uint8_t ATDA85, uint8_t ATDA84, uint8_t ATDA83, uint8_t ATDA82, uint8_t ATDA81, uint8_t ATDA80);
    bool writeReg50h(uint8_t B7, uint8_t B6, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t SRAD1, uint8_t SRAD0);
    bool writeReg51h(uint8_t B7, uint8_t MSAD2, uint8_t MSAD1, uint8_t MSAD0, uint8_t B3, uint8_t FMTAD2, uint8_t FMTAD1, uint8_t FMTAD0);
    bool writeReg52h(uint8_t B7, uint8_t PSVAD2, uint8_t PSVAD1, uint8_t PSVAD0, uint8_t B3, uint8_t BYP2, uint8_t BYP1, uint8_t BYP0);
    bool writeReg53h(uint8_t B7, uint8_t B6, uint8_t SEAD6, uint8_t SEAD5, uint8_t SEAD4, uint8_t SEAD3, uint8_t SEAD2, uint8_t SEAD1);
    bool writeReg54h(uint8_t B7, uint8_t B6, uint8_t REVAD6, uint8_t REVAD5, uint8_t REVAD4, uint8_t REVAD3, uint8_t REVAD2, uint8_t REVAD1);
    bool writeReg55h(uint8_t B7, uint8_t B6, uint8_t MUTAD6, uint8_t MUTAD5, uint8_t MUTAD4, uint8_t MUTAD3, uint8_t MUTAD2, uint8_t MUTAD1);
    bool writeReg56h(uint8_t B7, uint8_t B6, uint8_t OVF6, uint8_t OVF5, uint8_t OVF4, uint8_t OVF3, uint8_t OVF2, uint8_t OVF1);
    bool writeReg57h(uint8_t ATMDAD, uint8_t ATSPAD, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t B1, uint8_t OVFP);
    bool writeReg58h(uint8_t ATAD07, uint8_t ATAD06, uint8_t ATAD05, uint8_t ATAD04, uint8_t ATAD03, uint8_t ATAD02, uint8_t ATAD01, uint8_t ATAD00);
    bool writeReg59h(uint8_t ATAD17, uint8_t ATAD16, uint8_t ATAD15, uint8_t ATAD14, uint8_t ATAD13, uint8_t ATAD12, uint8_t ATAD11, uint8_t ATAD10);
    bool writeReg5Ah(uint8_t ATAD27, uint8_t ATAD26, uint8_t ATAD25, uint8_t ATAD24, uint8_t ATAD23, uint8_t ATAD22, uint8_t ATAD21, uint8_t ATAD20);
    bool writeReg5Bh(uint8_t ATAD37, uint8_t ATAD36, uint8_t ATAD35, uint8_t ATAD34, uint8_t ATAD33, uint8_t ATAD32, uint8_t ATAD31, uint8_t ATAD30);
    bool writeReg5Ch(uint8_t ATAD47, uint8_t ATAD46, uint8_t ATAD45, uint8_t ATAD44, uint8_t ATAD43, uint8_t ATAD42, uint8_t ATAD41, uint8_t ATAD40);
    bool writeReg5Dh(uint8_t ATAD57, uint8_t ATAD56, uint8_t ATAD55, uint8_t ATAD54, uint8_t ATAD53, uint8_t ATAD52, uint8_t ATAD51, uint8_t ATAD50);
    bool writeReg5Eh(uint8_t ATAD67, uint8_t ATAD66, uint8_t ATAD65, uint8_t ATAD64, uint8_t ATAD63, uint8_t ATAD62, uint8_t ATAD61, uint8_t ATAD60);

    void MuteAudioOutput(bool mute, int channel);
    bool writeBytes(uint8_t reg, uint8_t data);
    bool active = false;

private:
    uint8_t buildRegisterValue(std::initializer_list<uint8_t> bits);
    struct CodecDataReg
    {
        uint8_t Reg40h = 0xc0;
        uint8_t Reg41h = 0x00;
        uint8_t Reg42h = 0x00;
        uint8_t Reg43h = 0x00;
        uint8_t Reg44h = 0x00;
        uint8_t Reg45h = 0x00;
        uint8_t Reg46h = 0x00;
        uint8_t Reg47h = 0xff;
        uint8_t Reg48h = 0xff;
        uint8_t Reg49h = 0xff;
        uint8_t Reg4ah = 0xff;
        uint8_t Reg4bh = 0xff;
        uint8_t Reg4ch = 0xff;
        uint8_t Reg4dh = 0xff;
        uint8_t Reg4eh = 0xff;
        uint8_t Reg4fh = 0xff;
        uint8_t Reg50h = 0x00;
        uint8_t Reg51h = 0x00;
        uint8_t Reg52h = 0x00;
        uint8_t Reg53h = 0x00;
        uint8_t Reg54h = 0x00;
        uint8_t Reg55h = 0x00;
        uint8_t Reg56h = 0x00;
        uint8_t Reg57h = 0x00;
        uint8_t Reg58h = 0xd7;
        uint8_t Reg59h = 0xd7;
        uint8_t Reg5ah = 0xd7;
        uint8_t Reg5bh = 0xd7;
        uint8_t Reg5ch = 0xd7;
        uint8_t Reg5dh = 0xd7;
        uint8_t Reg5eh = 0xd7;
        uint8_t Reg5fh = 0xd7;
    };

    void initCodec();


    QString i2cDev;
    int i2cAddress = 0x44;
    I2CReadWrite *i2cCodecCtrl_CODEC;
    CodecDataReg *dataReg;

};

#endif // PCM3168A_H
