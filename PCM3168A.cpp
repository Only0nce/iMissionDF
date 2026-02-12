#include "PCM3168A.h"

PCM3168A::PCM3168A(QString i2cdevice, int address)
{
    i2cDev = i2cdevice;
    i2cAddress = address;
    dataReg = new CodecDataReg;
    i2cCodecCtrl_CODEC = new I2CReadWrite(i2cDev.toStdString().c_str(),i2cAddress);
    active = writeBytes(0x40,0xc1);
    if (active) initCodec();
}

PCM3168A::~PCM3168A()
{
    delete dataReg;
    delete i2cCodecCtrl_CODEC;
}

void PCM3168A::initCodec()
{
    writeReg40h(1,1,0,0,0,0,0,1); //DAC Single rate
    writeReg50h(0,0,0,0,0,0,0,1); //ADC Single rate
    writeReg41h(0,0,0,0,0,1,1,0); //FMTDA 0110 24-bit I2S mode TDM format
    writeReg51h(0,0,0,0,0,1,1,0); //FMTDA 0110 24-bit I2S mode TDM format

}
bool PCM3168A::setInputGain(uint8_t channel, uint8_t AINX_VOL)
{
    if (AINX_VOL <= 8) AINX_VOL = 0;
    AINX_VOL = 255 - AINX_VOL;
    uint8_t reg = 0x58+channel;
    return writeBytes(reg,AINX_VOL);
}

bool PCM3168A::setOutputGainWithoutDSP(uint8_t channel, uint8_t AOUTX_VOL){
    channel = channel - 1;
    uint8_t reg = 0x47+channel;
    return writeBytes(reg,AOUTX_VOL);
}

bool PCM3168A::setOutputGain(uint8_t channel, uint8_t AOUTX_VOL)
{
    if (AOUTX_VOL <= 8) AOUTX_VOL = 0;
    AOUTX_VOL = 255 - AOUTX_VOL;
    uint8_t reg = 0x47+channel;
    return writeBytes(reg,AOUTX_VOL);
}
bool PCM3168A::writeBytes(uint8_t reg, uint8_t data)
{
    i2cCodecCtrl_CODEC->buffer[0] = reg;
    i2cCodecCtrl_CODEC->buffer[1] = data;
    return i2cCodecCtrl_CODEC->writeBytes();
}

uint8_t PCM3168A::buildRegisterValue(std::initializer_list<uint8_t> bits)
{
    uint8_t value = 0;
    int shift = int(bits.size()) - 1;
    for (auto bit : bits) {
        value |= (bit & 0x01) << shift;
        --shift;
    }
    return value;
}

bool PCM3168A::writeReg40h(uint8_t MRST, uint8_t SRST, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t SRDA1, uint8_t SRDA0)
{
    dataReg->Reg40h = buildRegisterValue({MRST, SRST, B5, B4, B3, B2, SRDA1, SRDA0});
    return writeBytes(0x40, dataReg->Reg40h);
}

//Table 15. Register: DAC Control 1
//FMTDA 0100 24-bit I2S mode DSP form

bool PCM3168A::writeReg41h(uint8_t PSMDA, uint8_t MSDA2, uint8_t MSDA1, uint8_t MSDA0, uint8_t FMTDA3, uint8_t FMTDA2, uint8_t FMTDA1, uint8_t FMTDA0){
    dataReg->Reg41h = buildRegisterValue({PSMDA, MSDA2, MSDA1, MSDA0, FMTDA3, FMTDA2, FMTDA1, FMTDA0});
    return writeBytes(0x41,dataReg->Reg41h);
}

bool PCM3168A::writeReg42h(uint8_t OPEDA3, uint8_t OPEDA2, uint8_t OPEDA1, uint8_t OPEDA0, uint8_t FLT3, uint8_t FLT2, uint8_t FLT1, uint8_t FLT0){
    dataReg->Reg42h = buildRegisterValue({OPEDA3, OPEDA2, OPEDA1, OPEDA0, FLT3, FLT2, FLT1, FLT0});
    return writeBytes(0x42,dataReg->Reg42h);
}

bool PCM3168A::writeReg43h(uint8_t REVDA8, uint8_t REVDA7, uint8_t REVDA6, uint8_t REVDA5, uint8_t REVDA4, uint8_t REVDA3, uint8_t REVDA2, uint8_t REVDA1){
    dataReg->Reg43h = buildRegisterValue({REVDA8, REVDA7, REVDA6, REVDA5, REVDA4, REVDA3, REVDA2, REVDA1});
    return writeBytes(0x43,dataReg->Reg43h);
}

bool PCM3168A::writeReg44h(uint8_t MUTDA8, uint8_t MUTDA7, uint8_t MUTDA6, uint8_t MUTDA5, uint8_t MUTDA4, uint8_t MUTDA3, uint8_t MUTDA2, uint8_t MUTDA1){
    dataReg->Reg44h = buildRegisterValue({MUTDA8, MUTDA7, MUTDA6, MUTDA5, MUTDA4, MUTDA3, MUTDA2, MUTDA1});
    return writeBytes(0x44,dataReg->Reg44h);
}

bool PCM3168A::writeReg45h(uint8_t ZERO8, uint8_t ZERO7, uint8_t ZERO6, uint8_t ZERO5, uint8_t ZERO4, uint8_t ZERO3, uint8_t ZERO2, uint8_t ZERO1){
    dataReg->Reg45h = buildRegisterValue({ZERO8, ZERO7, ZERO6, ZERO5, ZERO4, ZERO3, ZERO2, ZERO1});
    return writeBytes(0x45,dataReg->Reg45h);
}

bool PCM3168A::writeReg46h(uint8_t ATMDDA, uint8_t ATSPDA, uint8_t DEMP1, uint8_t DEMP0, uint8_t AZRO2, uint8_t AZRO1, uint8_t AZRO0, uint8_t ZREV){
    dataReg->Reg46h = buildRegisterValue({ATMDDA, ATSPDA, DEMP1, DEMP0, AZRO2, AZRO1, AZRO0, ZREV});
    return writeBytes(0x46,dataReg->Reg46h);
}

bool PCM3168A::writeReg47h(uint8_t ATDA07, uint8_t ATDA06, uint8_t ATDA05, uint8_t ATDA04, uint8_t ATDA03, uint8_t ATDA02, uint8_t ATDA01, uint8_t ATDA00){
    dataReg->Reg47h = buildRegisterValue({ATDA07, ATDA06, ATDA05, ATDA04, ATDA03, ATDA02, ATDA01, ATDA00});
    return writeBytes(0x47,dataReg->Reg47h);
}

bool PCM3168A::writeReg48h(uint8_t ATDA17, uint8_t ATDA16, uint8_t ATDA15, uint8_t ATDA14, uint8_t ATDA13, uint8_t ATDA12, uint8_t ATDA11, uint8_t ATDA10){
    dataReg->Reg48h = buildRegisterValue({ATDA17, ATDA16, ATDA15, ATDA14, ATDA13, ATDA12, ATDA11, ATDA10});
    return writeBytes(0x48,dataReg->Reg48h);
}

bool PCM3168A::writeReg49h(uint8_t ATDA27, uint8_t ATDA26, uint8_t ATDA25, uint8_t ATDA24, uint8_t ATDA23, uint8_t ATDA22, uint8_t ATDA21, uint8_t ATDA20){
    dataReg->Reg49h = buildRegisterValue({ATDA27, ATDA26, ATDA25, ATDA24, ATDA23, ATDA22, ATDA21, ATDA20});
    return writeBytes(0x49,dataReg->Reg49h);
}

bool PCM3168A::writeReg4Ah(uint8_t ATDA37, uint8_t ATDA36, uint8_t ATDA35, uint8_t ATDA34, uint8_t ATDA33, uint8_t ATDA32, uint8_t ATDA31, uint8_t ATDA30){
    dataReg->Reg4ah = buildRegisterValue({ATDA37, ATDA36, ATDA35, ATDA34, ATDA33, ATDA32, ATDA31, ATDA30});
    return writeBytes(0x4a,dataReg->Reg4ah);
}

bool PCM3168A::writeReg4Bh(uint8_t ATDA47, uint8_t ATDA46, uint8_t ATDA45, uint8_t ATDA44, uint8_t ATDA43, uint8_t ATDA42, uint8_t ATDA41, uint8_t ATDA40){
    dataReg->Reg4bh = buildRegisterValue({ATDA47, ATDA46, ATDA45, ATDA44, ATDA43, ATDA42, ATDA41, ATDA40});
    return writeBytes(0x4b,dataReg->Reg4bh);
}

bool PCM3168A::writeReg4Ch(uint8_t ATDA57, uint8_t ATDA56, uint8_t ATDA55, uint8_t ATDA54, uint8_t ATDA53, uint8_t ATDA52, uint8_t ATDA51, uint8_t ATDA50){
    dataReg->Reg4ch = buildRegisterValue({ATDA57, ATDA56, ATDA55, ATDA54, ATDA53, ATDA52, ATDA51, ATDA50});
    return writeBytes(0x4c,dataReg->Reg4ch);
}

bool PCM3168A::writeReg4Dh(uint8_t ATDA67, uint8_t ATDA66, uint8_t ATDA65, uint8_t ATDA64, uint8_t ATDA63, uint8_t ATDA62, uint8_t ATDA61, uint8_t ATDA60){
    dataReg->Reg4dh = buildRegisterValue({ATDA67, ATDA66, ATDA65, ATDA64, ATDA63, ATDA62, ATDA61, ATDA60});
    return writeBytes(0x4d,dataReg->Reg4dh);
}

bool PCM3168A::writeReg4Eh(uint8_t ATDA77, uint8_t ATDA76, uint8_t ATDA75, uint8_t ATDA74, uint8_t ATDA73, uint8_t ATDA72, uint8_t ATDA71, uint8_t ATDA70){
    dataReg->Reg4eh = buildRegisterValue({ATDA77, ATDA76, ATDA75, ATDA74, ATDA73, ATDA72, ATDA71, ATDA70});
    return writeBytes(0x4e,dataReg->Reg4eh);
}

bool PCM3168A::writeReg4Fh(uint8_t ATDA87, uint8_t ATDA86, uint8_t ATDA85, uint8_t ATDA84, uint8_t ATDA83, uint8_t ATDA82, uint8_t ATDA81, uint8_t ATDA80){
    dataReg->Reg4fh = buildRegisterValue({ATDA87, ATDA86, ATDA85, ATDA84, ATDA83, ATDA82, ATDA81, ATDA80});
    return writeBytes(0x4f,dataReg->Reg4fh);
}

bool PCM3168A::writeReg50h(uint8_t B7, uint8_t B6, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t SRAD1, uint8_t SRAD0){
    dataReg->Reg50h = buildRegisterValue({B7, B6, B5, B4, B3, B2, SRAD1, SRAD0});
    return writeBytes(0x50,dataReg->Reg50h);
}

bool PCM3168A::writeReg51h(uint8_t B7, uint8_t MSAD2, uint8_t MSAD1, uint8_t MSAD0, uint8_t B3, uint8_t FMTAD2, uint8_t FMTAD1, uint8_t FMTAD0){
    dataReg->Reg51h = buildRegisterValue({B7, MSAD2, MSAD1, MSAD0, B3, FMTAD2, FMTAD1, FMTAD0});
    return writeBytes(0x51,dataReg->Reg51h);
}

bool PCM3168A::writeReg52h(uint8_t B7, uint8_t PSVAD2, uint8_t PSVAD1, uint8_t PSVAD0, uint8_t B3, uint8_t BYP2, uint8_t BYP1, uint8_t BYP0){
    dataReg->Reg52h = buildRegisterValue({B7, PSVAD2, PSVAD1, PSVAD0, B3, BYP2, BYP1, BYP0});
    return writeBytes(0x52,dataReg->Reg52h);
}

bool PCM3168A::writeReg53h(uint8_t B7, uint8_t B6, uint8_t SEAD6, uint8_t SEAD5, uint8_t SEAD4, uint8_t SEAD3, uint8_t SEAD2, uint8_t SEAD1){
    dataReg->Reg53h = buildRegisterValue({B7, B6, SEAD6, SEAD5, SEAD4, SEAD3, SEAD2, SEAD1});
    return writeBytes(0x53,dataReg->Reg53h);
}

bool PCM3168A::writeReg54h(uint8_t B7, uint8_t B6, uint8_t REVAD6, uint8_t REVAD5, uint8_t REVAD4, uint8_t REVAD3, uint8_t REVAD2, uint8_t REVAD1){
    dataReg->Reg54h = buildRegisterValue({B7, B6, REVAD6, REVAD5, REVAD4, REVAD3, REVAD2, REVAD1});
    return writeBytes(0x54,dataReg->Reg54h);
}

bool PCM3168A::writeReg55h(uint8_t B7, uint8_t B6, uint8_t MUTAD6, uint8_t MUTAD5, uint8_t MUTAD4, uint8_t MUTAD3, uint8_t MUTAD2, uint8_t MUTAD1){
    dataReg->Reg55h = buildRegisterValue({B7, B6, MUTAD6, MUTAD5, MUTAD4, MUTAD3, MUTAD2, MUTAD1});
    return writeBytes(0x55,dataReg->Reg55h);
}

bool PCM3168A::writeReg56h(uint8_t B7, uint8_t B6, uint8_t OVF6, uint8_t OVF5, uint8_t OVF4, uint8_t OVF3, uint8_t OVF2, uint8_t OVF1){
    dataReg->Reg56h = buildRegisterValue({B7, B6, OVF6, OVF5, OVF4, OVF3, OVF2, OVF1});
    return writeBytes(0x56,dataReg->Reg56h);
}

bool PCM3168A::writeReg57h(uint8_t ATMDAD, uint8_t ATSPAD, uint8_t B5, uint8_t B4, uint8_t B3, uint8_t B2, uint8_t B1, uint8_t OVFP){
    dataReg->Reg57h = buildRegisterValue({ATMDAD, ATSPAD, B5, B4, B3, B2, B1, OVFP});
    return writeBytes(0x57,dataReg->Reg57h);
}

bool PCM3168A::writeReg58h(uint8_t ATAD07, uint8_t ATAD06, uint8_t ATAD05, uint8_t ATAD04, uint8_t ATAD03, uint8_t ATAD02, uint8_t ATAD01, uint8_t ATAD00){
    dataReg->Reg58h = buildRegisterValue({ATAD07, ATAD06, ATAD05, ATAD04, ATAD03, ATAD02, ATAD01, ATAD00});
    return writeBytes(0x58,dataReg->Reg58h);
}

bool PCM3168A::writeReg59h(uint8_t ATAD17, uint8_t ATAD16, uint8_t ATAD15, uint8_t ATAD14, uint8_t ATAD13, uint8_t ATAD12, uint8_t ATAD11, uint8_t ATAD10){
    dataReg->Reg59h = buildRegisterValue({ATAD17,  ATAD16, ATAD15, ATAD14, ATAD13, ATAD12, ATAD11, ATAD10});
    return writeBytes(0x59,dataReg->Reg59h);
}

bool PCM3168A::writeReg5Ah(uint8_t ATAD27, uint8_t ATAD26, uint8_t ATAD25, uint8_t ATAD24, uint8_t ATAD23, uint8_t ATAD22, uint8_t ATAD21, uint8_t ATAD20){
    dataReg->Reg5ah = buildRegisterValue({ATAD27, ATAD26, ATAD25, ATAD24, ATAD23,  ATAD22, ATAD21, ATAD20});
    return writeBytes(0x5a,dataReg->Reg5ah);
}

bool PCM3168A::writeReg5Bh(uint8_t ATAD37, uint8_t ATAD36, uint8_t ATAD35, uint8_t ATAD34, uint8_t ATAD33, uint8_t ATAD32, uint8_t ATAD31, uint8_t ATAD30){
    dataReg->Reg5bh = buildRegisterValue({ATAD37, ATAD36, ATAD35, ATAD34, ATAD33, ATAD32, ATAD31, ATAD30});
    return writeBytes(0x5b,dataReg->Reg5bh);
}

bool PCM3168A::writeReg5Ch(uint8_t ATAD47, uint8_t ATAD46, uint8_t ATAD45, uint8_t ATAD44, uint8_t ATAD43, uint8_t ATAD42, uint8_t ATAD41, uint8_t ATAD40){
    dataReg->Reg5ch = buildRegisterValue({ATAD47, ATAD46, ATAD45, ATAD44, ATAD43, ATAD42, ATAD41, ATAD40});
    return writeBytes(0x5c,dataReg->Reg5ch);
}

bool PCM3168A::writeReg5Dh(uint8_t ATAD57, uint8_t ATAD56, uint8_t ATAD55, uint8_t ATAD54, uint8_t ATAD53, uint8_t ATAD52, uint8_t ATAD51, uint8_t ATAD50){
    dataReg->Reg5dh = buildRegisterValue({ATAD57, ATAD56, ATAD55, ATAD54, ATAD53, ATAD52, ATAD51, ATAD50});
    return writeBytes(0x5d,dataReg->Reg5dh);
}

bool PCM3168A::writeReg5Eh(uint8_t ATAD67, uint8_t ATAD66, uint8_t ATAD65, uint8_t ATAD64, uint8_t ATAD63, uint8_t ATAD62, uint8_t ATAD61, uint8_t ATAD60){
    dataReg->Reg5eh = buildRegisterValue({ATAD67, ATAD66, ATAD65, ATAD64, ATAD63, ATAD62, ATAD61, ATAD60});
    return writeBytes(0x5e,dataReg->Reg5eh);
}
