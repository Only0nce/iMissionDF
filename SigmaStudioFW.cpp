#include "SigmaStudioFW.h"
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <DesignDSP_REC_V1/DesignDSP_REC_V1_IC_1.h>
ADAU1467::ADAU1467(const std::string& spidev)
    : spiDev(spidev) // ใช้ initializer list
{

    DSPSPI = new SPIClass (spiDev);
//    spi_init();
    for (int i=0;i<DSPDATABUF;i++) {
        tx[i] = 0;
    }
}
void ADAU1467::setModuleSingleVolume(uint16_t targetAddress, uint16_t slewModeAddress, uint32_t modeValue, double value)
{
    blokcWrite(slewModeAddress,modeValue);
    saveLoadWrite0x6000(value);
    blokcWrite(targetAddress);
    qDebug() << "setModuleSingleVolume" << "address" << QString::number(targetAddress,16) << "modeValue" << modeValue << QString::number(slewModeAddress,16) << "value" << value;
}

void ADAU1467::setMixerVolume(uint16_t address, uint16_t modeAddress, uint32_t modeValue, double value)
{
    saveLoadWrite0x6000(value);
    QThread::msleep(150);
    blokcWrite(address);
    QThread::msleep(150);
    blokcWrite(modeAddress,modeValue);
    // qDebug() << "setVolume" << "address" << QString::number(address,16) << "modeValue" << QString::number(modeAddress,16) << "value" << value;
}

void ADAU1467::setToneVolume(uint16_t address, double value)
{
    byte dest[4];
    int32_t value32 = SIGMASTUDIOTYPE_8_24_CONVERT(value);
    SIGMASTUDIOTYPE_REGISTER_CONVERT(value32, dest);
    // qDebug() << "setToneVolume" << dest[3] << dest[2] << dest[1] << dest[0];
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, address, 4, dest );
//    saveLoadWrite0x6000(value);
//    blokcWrite(address);
    // qDebug() << "setToneVolume" << "address" << QString::number(address,16) << "value" << value32;
}

void ADAU1467::setDSPSplitVolume(uint16_t address, double value)
{
    byte dest[4];
    int32_t value32 = SIGMASTUDIOTYPE_8_24_CONVERT(value);
    SIGMASTUDIOTYPE_REGISTER_CONVERT(value32, dest);
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, address, 4, dest );
}

void ADAU1467::setFIRfilter(int fir_start_addr, int fir_filter_length, double* coefficients){
    for(int i = 0; i < fir_filter_length; i++)
    {
        SIGMA_WRITE_REGISTER_FLOAT(fir_start_addr + i , coefficients[fir_filter_length - i - 1]);
    }
}


void ADAU1467::saveLoadWrite0x6000(double dValue)
{
    byte dest[4];
    int32_t value = SIGMASTUDIOTYPE_8_24_CONVERT(dValue);
    SIGMASTUDIOTYPE_REGISTER_CONVERT(value, dest);
    // qDebug() << "saveLoadWrite0x6000" << dest[3] << dest[2] << dest[1] << dest[0];
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, 0x6000, 4, dest );
}

void ADAU1467::blokcWrite(uint16_t address)
{
    byte dest[12];
    dest[0] = 0x00;
    dest[1] = 0x00;
    dest[2] = address >> 8;
    dest[3] = address & 0xFF;
    dest[4] = 0x00;
    dest[5] = 0x00;
    dest[6] = 0x00;
    dest[7] = 0x01;
    dest[8] = 0x00;
    dest[9] = 0x00;
    dest[10] = 0x00;
    dest[11] = 0x00;
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, 0x6005, 12, dest );
}
void ADAU1467::blokcWrite(uint16_t modeAddress, uint32_t modeValue)
{
    byte dest[4];
    SIGMASTUDIOTYPE_REGISTER_CONVERT(modeValue, dest);
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, modeAddress, 4, dest );
}
void ADAU1467::WriteSelfBootMemoryToDSP()
{
    //    qDebug() << "WriteSelfBootMemoryToDSP" << QDateTime::currentDateTime().toString("dd MMM yyyy hh:mm:ss");
    //    SIGMA_WRITE_REGISTER_BLOCK (SPIToggleAddress, SPIToggleBytes, W001ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress, SPIToggleDelayBytes, W002ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK (SPIToggleAddress, SPIToggleBytes, W003ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress, SPIToggleDelayBytes, W004ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK (SPIToggleAddress, SPIToggleBytes, W005ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress, SPIToggleDelayBytes, W006ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK (SECOND_PAGE_ENABLEAddress, SECOND_PAGE_ENABLEBytes, W007ParamDataSECOND_PAGE_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK (SECOND_PAGE_ENABLEAddress, SECOND_PAGE_ENABLEBytes, W008ParamDataSECOND_PAGE_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK (HIBERNATEAddress, HIBERNATEBytes, W009ParamDataHIBERNATE);
    //    SIGMA_WRITE_DELAY (HibernateDelayAddress, HibernateDelayBytes, W010ParamDataHibernateDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK (KILL_COREAddress, KILL_COREBytes, W011ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK (KILL_COREAddress, KILL_COREBytes, W012ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK (PLL_ENABLEAddress, PLL_ENABLEBytes, W013ParamDataPLL_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK (PLL_CTRL1RegisterAddress, PLL_CTRL1RegisterBytes, W014ParamDataPLL_CTRL1Register);
    //    SIGMA_WRITE_REGISTER_BLOCK (PLL_CLK_SRCRegisterAddress, PLL_CLK_SRCRegisterBytes, W015ParamDataPLL_CLK_SRCRegister);
    //    SIGMA_WRITE_REGISTER_BLOCK (MCLK_OUTRegisterAddress, MCLK_OUTRegisterBytes, W016ParamDataMCLK_OUTRegister);
    //    SIGMA_WRITE_REGISTER_BLOCK (PLL_ENABLERegisterAddress, PLL_ENABLERegisterBytes, W017ParamDataPLL_ENABLERegister);
    //    SIGMA_WRITE_DELAY (Sigma350_SelfbootProgrammerS350_PLLLockDelayAddress, Sigma350_SelfbootProgrammerS350_PLLLockDelayBytes, W018ParamDataSigma350_SelfbootProgrammerS350_PLLLockDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK (POWER_ENABLE0RegisterAddress, POWER_ENABLE0RegisterBytes, W019ParamDataPOWER_ENABLE0Register);
    //    SIGMA_WRITE_REGISTER_BLOCK (POWER_ENABLE1RegisterAddress, POWER_ENABLE1RegisterBytes, W020ParamDataPOWER_ENABLE1Register);
    //    SIGMA_WRITE_REGISTER_BLOCK (KILL_COREAddress, KILL_COREBytes, W021ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK (KILL_COREAddress, KILL_COREBytes, W022ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN1_MAddress,	CLK_GEN1_MBytes,	W023ParamDataCLK_GEN1_M);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN3_MAddress,	CLK_GEN3_MBytes,	W024ParamDataCLK_GEN3_M);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN3_NAddress,	CLK_GEN3_NBytes,	W025ParamDataCLK_GEN3_N);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP6_WRITEAddress,	MP6_WRITEBytes,	W026ParamDataMP6_WRITE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP7_WRITEAddress,	MP7_WRITEBytes,	W027ParamDataMP7_WRITE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE0Address,	SOUT_SOURCE0Bytes,	W028ParamDataSOUT_SOURCE0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE1Address,	SOUT_SOURCE1Bytes,	W029ParamDataSOUT_SOURCE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE2Address,	SOUT_SOURCE2Bytes,	W030ParamDataSOUT_SOURCE2);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE3Address,	SOUT_SOURCE3Bytes,	W031ParamDataSOUT_SOURCE3);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE4Address,	SOUT_SOURCE4Bytes,	W032ParamDataSOUT_SOURCE4);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE5Address,	SOUT_SOURCE5Bytes,	W033ParamDataSOUT_SOURCE5);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE6Address,	SOUT_SOURCE6Bytes,	W034ParamDataSOUT_SOURCE6);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE7Address,	SOUT_SOURCE7Bytes,	W035ParamDataSOUT_SOURCE7);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE8Address,	SOUT_SOURCE8Bytes,	W036ParamDataSOUT_SOURCE8);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE9Address,	SOUT_SOURCE9Bytes,	W037ParamDataSOUT_SOURCE9);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE10Address,	SOUT_SOURCE10Bytes,	W038ParamDataSOUT_SOURCE10);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE11Address,	SOUT_SOURCE11Bytes,	W039ParamDataSOUT_SOURCE11);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE12Address,	SOUT_SOURCE12Bytes,	W040ParamDataSOUT_SOURCE12);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE13Address,	SOUT_SOURCE13Bytes,	W041ParamDataSOUT_SOURCE13);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE14Address,	SOUT_SOURCE14Bytes,	W042ParamDataSOUT_SOURCE14);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE15Address,	SOUT_SOURCE15Bytes,	W043ParamDataSOUT_SOURCE15);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE16Address,	SOUT_SOURCE16Bytes,	W044ParamDataSOUT_SOURCE16);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE17Address,	SOUT_SOURCE17Bytes,	W045ParamDataSOUT_SOURCE17);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE18Address,	SOUT_SOURCE18Bytes,	W046ParamDataSOUT_SOURCE18);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE19Address,	SOUT_SOURCE19Bytes,	W047ParamDataSOUT_SOURCE19);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE20Address,	SOUT_SOURCE20Bytes,	W048ParamDataSOUT_SOURCE20);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE21Address,	SOUT_SOURCE21Bytes,	W049ParamDataSOUT_SOURCE21);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE22Address,	SOUT_SOURCE22Bytes,	W050ParamDataSOUT_SOURCE22);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE23Address,	SOUT_SOURCE23Bytes,	W051ParamDataSOUT_SOURCE23);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_0_0Address,	SERIAL_BYTE_0_0Bytes,	W052ParamDataSERIAL_BYTE_0_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_1_0Address,	SERIAL_BYTE_1_0Bytes,	W053ParamDataSERIAL_BYTE_1_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_1_1Address,	SERIAL_BYTE_1_1Bytes,	W054ParamDataSERIAL_BYTE_1_1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_2_0Address,	SERIAL_BYTE_2_0Bytes,	W055ParamDataSERIAL_BYTE_2_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_3_0Address,	SERIAL_BYTE_3_0Bytes,	W056ParamDataSERIAL_BYTE_3_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_4_0Address,	SERIAL_BYTE_4_0Bytes,	W057ParamDataSERIAL_BYTE_4_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_5_0Address,	SERIAL_BYTE_5_0Bytes,	W058ParamDataSERIAL_BYTE_5_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_5_1Address,	SERIAL_BYTE_5_1Bytes,	W059ParamDataSERIAL_BYTE_5_1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_6_0Address,	SERIAL_BYTE_6_0Bytes,	W060ParamDataSERIAL_BYTE_6_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_7_0Address,	SERIAL_BYTE_7_0Bytes,	W061ParamDataSERIAL_BYTE_7_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP0_WRITE1Address,	MP0_WRITE1Bytes,	W062ParamDataMP0_WRITE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP1_WRITE1Address,	MP1_WRITE1Bytes,	W063ParamDataMP1_WRITE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(ProgramAddress,	ProgramBytes,	W064ParamDataProgram);
    //    SIGMA_WRITE_REGISTER_BLOCK	(DM0DataAddress,	DM0DataBytes,	W065ParamDataDM0Data);
    //    SIGMA_WRITE_REGISTER_BLOCK	(DM1DataAddress,	DM1DataBytes,	W066ParamDataDM1Data);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W067ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_ADDRESSAddress,	START_ADDRESSBytes,	W068ParamDataSTART_ADDRESS);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_PULSEAddress,	START_PULSEBytes,	W069ParamDataSTART_PULSE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_COREAddress,	START_COREBytes,	W070ParamDataSTART_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_COREAddress,	START_COREBytes,	W071ParamDataSTART_CORE);
    //    SIGMA_WRITE_DELAY (StartDelayAddress,	StartDelayBytes,	W072ParamDataStartDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(HIBERNATEAddress,	HIBERNATEBytes,	W073ParamDataHIBERNATE);
    //    QThread::msleep(1000);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_MasterModeAddress,	g_MasterModeBytes,	W074ParamData_g_MasterMode);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_modeAddress,	g_spi_modeBytes,	W075ParamData_g_spi_mode);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_speedAddress,	g_spi_speedBytes,	W076ParamData_g_spi_speed);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_address_bytesAddress,	g_spi_address_bytesBytes,	W077ParamData_g_spi_address_bytes);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_chip_erase_commandAddress,	g_spi_chip_erase_commandBytes,	W078ParamData_g_spi_chip_erase_command);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_wren_commandAddress,	g_spi_wren_commandBytes,	W079ParamData_g_spi_wren_command);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_read_commandAddress,	g_spi_read_commandBytes,	W080ParamData_g_spi_read_command);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_write_commandAddress,	g_spi_write_commandBytes,	W081ParamData_g_spi_write_command);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_spi_slave_clatchAddress,	g_spi_slave_clatchBytes,	W082ParamData_g_spi_slave_clatch);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_initializedAddress,	g_initializedBytes,	W083ParamData_g_initialized);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_EraseAddress,	g_EraseBytes,	W084ParamData_g_Erase);
    //    QThread::msleep(5000);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W085ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A0_S256_N10_idx0Address,	Page_A0_S256_N10_idx0Bytes,	W086ParamDataPage_A0_S256_N10_idx0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W087ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W088ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A256_S256_N10_idx1Address,	Page_A256_S256_N10_idx1Bytes,	W089ParamDataPage_A256_S256_N10_idx1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W090ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W091ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A512_S256_N10_idx2Address,	Page_A512_S256_N10_idx2Bytes,	W092ParamDataPage_A512_S256_N10_idx2);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W093ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W094ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A768_S256_N10_idx3Address,	Page_A768_S256_N10_idx3Bytes,	W095ParamDataPage_A768_S256_N10_idx3);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W096ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W097ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A1024_S256_N10_idx4Address,	Page_A1024_S256_N10_idx4Bytes,	W098ParamDataPage_A1024_S256_N10_idx4);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W099ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W100ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A1280_S256_N10_idx5Address,	Page_A1280_S256_N10_idx5Bytes,	W101ParamDataPage_A1280_S256_N10_idx5);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W102ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W103ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A1536_S256_N10_idx6Address,	Page_A1536_S256_N10_idx6Bytes,	W104ParamDataPage_A1536_S256_N10_idx6);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W105ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W106ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A1792_S256_N10_idx7Address,	Page_A1792_S256_N10_idx7Bytes,	W107ParamDataPage_A1792_S256_N10_idx7);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W108ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W109ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A2048_S256_N10_idx8Address,	Page_A2048_S256_N10_idx8Bytes,	W110ParamDataPage_A2048_S256_N10_idx8);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W111ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageAddressAddress,	g_PageAddressBytes,	W112ParamData_g_PageAddress);
    //    SIGMA_WRITE_REGISTER_BLOCK	(Page_A2304_S256_N10_idx9Address,	Page_A2304_S256_N10_idx9Bytes,	W113ParamDataPage_A2304_S256_N10_idx9);
    //    SIGMA_WRITE_REGISTER_BLOCK	(g_PageSizeAddress,	g_PageSizeBytes,	W114ParamData_g_PageSize);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SPIToggleAddress,	SPIToggleBytes,	W115ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress,	SPIToggleDelayBytes,	W116ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SPIToggleAddress,	SPIToggleBytes,	W117ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress,	SPIToggleDelayBytes,	W118ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SPIToggleAddress,	SPIToggleBytes,	W119ParamDataSPIToggle);
    //    SIGMA_WRITE_DELAY (SPIToggleDelayAddress,	SPIToggleDelayBytes,	W120ParamDataSPIToggleDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SECOND_PAGE_ENABLEAddress,	SECOND_PAGE_ENABLEBytes,	W121ParamDataSECOND_PAGE_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SECOND_PAGE_ENABLEAddress,	SECOND_PAGE_ENABLEBytes,	W122ParamDataSECOND_PAGE_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(HIBERNATEAddress,	HIBERNATEBytes,	W123ParamDataHIBERNATE);
    //    SIGMA_WRITE_DELAY (HibernateDelayAddress,	HibernateDelayBytes,	W124ParamDataHibernateDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W125ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W126ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(PLL_ENABLEAddress,	PLL_ENABLEBytes,	W127ParamDataPLL_ENABLE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(PLL_CTRL1RegisterAddress,	PLL_CTRL1RegisterBytes,	W128ParamDataPLL_CTRL1Register);
    //    SIGMA_WRITE_REGISTER_BLOCK	(PLL_CLK_SRCRegisterAddress,	PLL_CLK_SRCRegisterBytes,	W129ParamDataPLL_CLK_SRCRegister);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MCLK_OUTRegisterAddress,	MCLK_OUTRegisterBytes,	W130ParamDataMCLK_OUTRegister);
    //    SIGMA_WRITE_REGISTER_BLOCK	(PLL_ENABLERegisterAddress,	PLL_ENABLERegisterBytes,	W131ParamDataPLL_ENABLERegister);
    //    SIGMA_WRITE_DELAY (PLLLockDelayAddress,	PLLLockDelayBytes,	W132ParamDataPLLLockDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(POWER_ENABLE0RegisterAddress,	POWER_ENABLE0RegisterBytes,	W133ParamDataPOWER_ENABLE0Register);
    //    SIGMA_WRITE_REGISTER_BLOCK	(POWER_ENABLE1RegisterAddress,	POWER_ENABLE1RegisterBytes,	W134ParamDataPOWER_ENABLE1Register);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W135ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W136ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN1_MAddress,	CLK_GEN1_MBytes,	W137ParamDataCLK_GEN1_M);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN3_MAddress,	CLK_GEN3_MBytes,	W138ParamDataCLK_GEN3_M);
    //    SIGMA_WRITE_REGISTER_BLOCK	(CLK_GEN3_NAddress,	CLK_GEN3_NBytes,	W139ParamDataCLK_GEN3_N);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP6_WRITEAddress,	MP6_WRITEBytes,	W140ParamDataMP6_WRITE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP7_WRITEAddress,	MP7_WRITEBytes,	W141ParamDataMP7_WRITE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE0Address,	SOUT_SOURCE0Bytes_2ND,	W142ParamDataSOUT_SOURCE0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE1Address,	SOUT_SOURCE1Bytes_2ND,	W143ParamDataSOUT_SOURCE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE2Address,	SOUT_SOURCE2Bytes_2ND,	W144ParamDataSOUT_SOURCE2);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE3Address,	SOUT_SOURCE3Bytes_2ND,	W145ParamDataSOUT_SOURCE3);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE4Address,	SOUT_SOURCE4Bytes_2ND,	W146ParamDataSOUT_SOURCE4);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE5Address,	SOUT_SOURCE5Bytes_2ND,	W147ParamDataSOUT_SOURCE5);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE6Address,	SOUT_SOURCE6Bytes_2ND,	W148ParamDataSOUT_SOURCE6);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE7Address,	SOUT_SOURCE7Bytes_2ND,	W149ParamDataSOUT_SOURCE7);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE8Address,	SOUT_SOURCE8Bytes_2ND,	W150ParamDataSOUT_SOURCE8);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE9Address,	SOUT_SOURCE9Bytes_2ND,	W151ParamDataSOUT_SOURCE9);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE10Address,	SOUT_SOURCE10Bytes_2ND,	W152ParamDataSOUT_SOURCE10);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE11Address,	SOUT_SOURCE11Bytes_2ND,	W153ParamDataSOUT_SOURCE11);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE12Address,	SOUT_SOURCE12Bytes_2ND,	W154ParamDataSOUT_SOURCE12);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE13Address,	SOUT_SOURCE13Bytes_2ND,	W155ParamDataSOUT_SOURCE13);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE14Address,	SOUT_SOURCE14Bytes_2ND,	W156ParamDataSOUT_SOURCE14);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE15Address,	SOUT_SOURCE15Bytes_2ND,	W157ParamDataSOUT_SOURCE15);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE16Address,	SOUT_SOURCE16Bytes_2ND,	W158ParamDataSOUT_SOURCE16);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE17Address,	SOUT_SOURCE17Bytes_2ND,	W159ParamDataSOUT_SOURCE17);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE18Address,	SOUT_SOURCE18Bytes_2ND,	W160ParamDataSOUT_SOURCE18);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE19Address,	SOUT_SOURCE19Bytes_2ND,	W161ParamDataSOUT_SOURCE19);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE20Address,	SOUT_SOURCE20Bytes_2ND,	W162ParamDataSOUT_SOURCE20);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE21Address,	SOUT_SOURCE21Bytes_2ND,	W163ParamDataSOUT_SOURCE21);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE22Address,	SOUT_SOURCE22Bytes_2ND,	W164ParamDataSOUT_SOURCE22);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SOUT_SOURCE23Address,	SOUT_SOURCE23Bytes_2ND,	W165ParamDataSOUT_SOURCE23);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_0_0Address,	SERIAL_BYTE_0_0Bytes,	W166ParamDataSERIAL_BYTE_0_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_1_0Address,	SERIAL_BYTE_1_0Bytes,	W167ParamDataSERIAL_BYTE_1_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_1_1Address,	SERIAL_BYTE_1_1Bytes,	W168ParamDataSERIAL_BYTE_1_1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_2_0Address,	SERIAL_BYTE_2_0Bytes,	W169ParamDataSERIAL_BYTE_2_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_3_0Address,	SERIAL_BYTE_3_0Bytes,	W170ParamDataSERIAL_BYTE_3_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_4_0Address,	SERIAL_BYTE_4_0Bytes,	W171ParamDataSERIAL_BYTE_4_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_5_0Address,	SERIAL_BYTE_5_0Bytes,	W172ParamDataSERIAL_BYTE_5_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_5_1Address,	SERIAL_BYTE_5_1Bytes,	W173ParamDataSERIAL_BYTE_5_1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_6_0Address,	SERIAL_BYTE_6_0Bytes,	W174ParamDataSERIAL_BYTE_6_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(SERIAL_BYTE_7_0Address,	SERIAL_BYTE_7_0Bytes,	W175ParamDataSERIAL_BYTE_7_0);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP0_WRITE1Address,	MP0_WRITE1Bytes,	W176ParamDataMP0_WRITE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(MP1_WRITE1Address,	MP1_WRITE1Bytes,	W177ParamDataMP1_WRITE1);
    //    SIGMA_WRITE_REGISTER_BLOCK	(ProgramDataAddress,	ProgramDataBytes,	W178ParamDataProgramData);
    //    SIGMA_WRITE_REGISTER_BLOCK	(DM0DataAddress,	DM0DataBytes_2ND,	W179ParamDataDM0Data);
    //    SIGMA_WRITE_REGISTER_BLOCK	(DM1DataAddress,	DM1DataBytes_2ND,	W180ParamDataDM1Data);
    //    SIGMA_WRITE_REGISTER_BLOCK	(KILL_COREAddress,	KILL_COREBytes,	W181ParamDataKILL_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_ADDRESSAddress,	START_ADDRESSBytes,	W182ParamDataSTART_ADDRESS);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_PULSEAddress,	START_PULSEBytes,	W183ParamDataSTART_PULSE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_COREAddress,	START_COREBytes,	W184ParamDataSTART_CORE);
    //    SIGMA_WRITE_REGISTER_BLOCK	(START_COREAddress,	START_COREBytes,	W185ParamDataSTART_CORE);
    //    SIGMA_WRITE_DELAY (StartDelayAddress,	StartDelayBytes,	W186ParamDataStartDelay);
    //    SIGMA_WRITE_REGISTER_BLOCK	(HIBERNATEAddress,	HIBERNATEBytes,	W187ParamDataHIBERNATE);
    //    qDebug() << "WriteSelfBootMemoryToDSP Finised" << QDateTime::currentDateTime().toString("dd MMM yyyy hh:mm:ss");
}
void ADAU1467::default_download_IC_1()
{
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, 0x4FFF, 4, tx );
    QThread::msleep(260);
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, 0x4FFF, 4, tx );
    QThread::msleep(260);
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, 0x4FFF, 4, tx );
    QThread::msleep(260);

    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOFT_RESET_IC_1_ADDR, REG_SOFT_RESET_IC_1_BYTE, R0_SOFT_RESET_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOFT_RESET_IC_1_ADDR, REG_SOFT_RESET_IC_1_BYTE, R1_SOFT_RESET_IC_1_Default );
    SIGMA_WRITE_DELAY( DEVICE_ADDR_IC_1, R2_RESET_DELAY_IC_1_SIZE, R2_RESET_DELAY_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_HIBERNATE_IC_1_ADDR, REG_HIBERNATE_IC_1_BYTE, R3_HIBERNATE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_HIBERNATE_IC_1_ADDR, REG_HIBERNATE_IC_1_BYTE, R4_HIBERNATE_IC_1_Default );
    SIGMA_WRITE_DELAY( DEVICE_ADDR_IC_1, R5_HIBERNATE_DELAY_IC_1_SIZE, R5_HIBERNATE_DELAY_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_KILL_CORE_IC_1_ADDR, REG_KILL_CORE_IC_1_BYTE, R6_KILL_CORE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_KILL_CORE_IC_1_ADDR, REG_KILL_CORE_IC_1_BYTE, R7_KILL_CORE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_PLL_ENABLE_IC_1_ADDR, REG_PLL_ENABLE_IC_1_BYTE, R8_PLL_ENABLE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_PLL_CTRL1_IC_1_ADDR, REG_PLL_CTRL1_IC_1_BYTE, R9_PLL_CTRL1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_PLL_CLK_SRC_IC_1_ADDR, REG_PLL_CLK_SRC_IC_1_BYTE, R10_PLL_CLK_SRC_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_MCLK_OUT_IC_1_ADDR, REG_MCLK_OUT_IC_1_BYTE, R11_MCLK_OUT_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_PLL_ENABLE_IC_1_ADDR, REG_PLL_ENABLE_IC_1_BYTE, R12_PLL_ENABLE_IC_1_Default );
    SIGMA_WRITE_DELAY( DEVICE_ADDR_IC_1, R13_PLL_LOCK_DELAY_IC_1_SIZE, R13_PLL_LOCK_DELAY_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_POWER_ENABLE0_IC_1_ADDR, REG_POWER_ENABLE0_IC_1_BYTE, R14_POWER_ENABLE0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_POWER_ENABLE1_IC_1_ADDR, REG_POWER_ENABLE1_IC_1_BYTE, R15_POWER_ENABLE1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_CLK_GEN1_M_IC_1_ADDR, REG_CLK_GEN1_M_IC_1_BYTE, R16_CLK_GEN1_M_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_CLK_GEN3_M_IC_1_ADDR, REG_CLK_GEN3_M_IC_1_BYTE, R17_CLK_GEN3_M_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_CLK_GEN3_N_IC_1_ADDR, REG_CLK_GEN3_N_IC_1_BYTE, R18_CLK_GEN3_N_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_MP6_WRITE_IC_1_ADDR, REG_MP6_WRITE_IC_1_BYTE, R19_MP6_WRITE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_MP7_WRITE_IC_1_ADDR, REG_MP7_WRITE_IC_1_BYTE, R20_MP7_WRITE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE0_IC_1_ADDR, REG_SOUT_SOURCE0_IC_1_BYTE, R21_SOUT_SOURCE0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE1_IC_1_ADDR, REG_SOUT_SOURCE1_IC_1_BYTE, R22_SOUT_SOURCE1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE2_IC_1_ADDR, REG_SOUT_SOURCE2_IC_1_BYTE, R23_SOUT_SOURCE2_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE3_IC_1_ADDR, REG_SOUT_SOURCE3_IC_1_BYTE, R24_SOUT_SOURCE3_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE4_IC_1_ADDR, REG_SOUT_SOURCE4_IC_1_BYTE, R25_SOUT_SOURCE4_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE5_IC_1_ADDR, REG_SOUT_SOURCE5_IC_1_BYTE, R26_SOUT_SOURCE5_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE6_IC_1_ADDR, REG_SOUT_SOURCE6_IC_1_BYTE, R27_SOUT_SOURCE6_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE7_IC_1_ADDR, REG_SOUT_SOURCE7_IC_1_BYTE, R28_SOUT_SOURCE7_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE8_IC_1_ADDR, REG_SOUT_SOURCE8_IC_1_BYTE, R29_SOUT_SOURCE8_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE9_IC_1_ADDR, REG_SOUT_SOURCE9_IC_1_BYTE, R30_SOUT_SOURCE9_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE10_IC_1_ADDR, REG_SOUT_SOURCE10_IC_1_BYTE, R31_SOUT_SOURCE10_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE11_IC_1_ADDR, REG_SOUT_SOURCE11_IC_1_BYTE, R32_SOUT_SOURCE11_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE12_IC_1_ADDR, REG_SOUT_SOURCE12_IC_1_BYTE, R33_SOUT_SOURCE12_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE13_IC_1_ADDR, REG_SOUT_SOURCE13_IC_1_BYTE, R34_SOUT_SOURCE13_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE14_IC_1_ADDR, REG_SOUT_SOURCE14_IC_1_BYTE, R35_SOUT_SOURCE14_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE15_IC_1_ADDR, REG_SOUT_SOURCE15_IC_1_BYTE, R36_SOUT_SOURCE15_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE16_IC_1_ADDR, REG_SOUT_SOURCE16_IC_1_BYTE, R37_SOUT_SOURCE16_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE17_IC_1_ADDR, REG_SOUT_SOURCE17_IC_1_BYTE, R38_SOUT_SOURCE17_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE18_IC_1_ADDR, REG_SOUT_SOURCE18_IC_1_BYTE, R39_SOUT_SOURCE18_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE19_IC_1_ADDR, REG_SOUT_SOURCE19_IC_1_BYTE, R40_SOUT_SOURCE19_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE20_IC_1_ADDR, REG_SOUT_SOURCE20_IC_1_BYTE, R41_SOUT_SOURCE20_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE21_IC_1_ADDR, REG_SOUT_SOURCE21_IC_1_BYTE, R42_SOUT_SOURCE21_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE22_IC_1_ADDR, REG_SOUT_SOURCE22_IC_1_BYTE, R43_SOUT_SOURCE22_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SOUT_SOURCE23_IC_1_ADDR, REG_SOUT_SOURCE23_IC_1_BYTE, R44_SOUT_SOURCE23_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_0_0_IC_1_ADDR, REG_SERIAL_BYTE_0_0_IC_1_BYTE, R45_SERIAL_BYTE_0_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_1_0_IC_1_ADDR, REG_SERIAL_BYTE_1_0_IC_1_BYTE, R46_SERIAL_BYTE_1_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_1_1_IC_1_ADDR, REG_SERIAL_BYTE_1_1_IC_1_BYTE, R47_SERIAL_BYTE_1_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_2_0_IC_1_ADDR, REG_SERIAL_BYTE_2_0_IC_1_BYTE, R48_SERIAL_BYTE_2_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_2_1_IC_1_ADDR, REG_SERIAL_BYTE_2_1_IC_1_BYTE, R49_SERIAL_BYTE_2_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_3_0_IC_1_ADDR, REG_SERIAL_BYTE_3_0_IC_1_BYTE, R50_SERIAL_BYTE_3_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_3_1_IC_1_ADDR, REG_SERIAL_BYTE_3_1_IC_1_BYTE, R51_SERIAL_BYTE_3_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_4_0_IC_1_ADDR, REG_SERIAL_BYTE_4_0_IC_1_BYTE, R52_SERIAL_BYTE_4_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_5_0_IC_1_ADDR, REG_SERIAL_BYTE_5_0_IC_1_BYTE, R53_SERIAL_BYTE_5_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_5_1_IC_1_ADDR, REG_SERIAL_BYTE_5_1_IC_1_BYTE, R54_SERIAL_BYTE_5_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_6_0_IC_1_ADDR, REG_SERIAL_BYTE_6_0_IC_1_BYTE, R55_SERIAL_BYTE_6_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_6_1_IC_1_ADDR, REG_SERIAL_BYTE_6_1_IC_1_BYTE, R56_SERIAL_BYTE_6_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_7_0_IC_1_ADDR, REG_SERIAL_BYTE_7_0_IC_1_BYTE, R57_SERIAL_BYTE_7_0_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_SERIAL_BYTE_7_1_IC_1_ADDR, REG_SERIAL_BYTE_7_1_IC_1_BYTE, R58_SERIAL_BYTE_7_1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_MP0_WRITE1_IC_1_ADDR, REG_MP0_WRITE1_IC_1_BYTE, R59_MP0_WRITE1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_MP1_WRITE1_IC_1_ADDR, REG_MP1_WRITE1_IC_1_BYTE, R60_MP1_WRITE1_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, PROGRAM_ADDR_IC_1, PROGRAM_SIZE_IC_1, Program_Data_IC_1 );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, PARAM_ADDR_IC_1, PARAM_SIZE_IC_1, Param_Data_IC_1 );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, DM1_DATA_ADDR_IC_1, DM1_DATA_SIZE_IC_1, DM1_DATA_Data_IC_1 );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_KILL_CORE_IC_1_ADDR, REG_KILL_CORE_IC_1_BYTE, R64_KILL_CORE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_START_ADDRESS_IC_1_ADDR, REG_START_ADDRESS_IC_1_BYTE, R65_START_ADDRESS_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_START_PULSE_IC_1_ADDR, REG_START_PULSE_IC_1_BYTE, R66_START_PULSE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_START_CORE_IC_1_ADDR, REG_START_CORE_IC_1_BYTE, R67_START_CORE_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_START_CORE_IC_1_ADDR, REG_START_CORE_IC_1_BYTE, R68_START_CORE_IC_1_Default );
    SIGMA_WRITE_DELAY( DEVICE_ADDR_IC_1, R69_START_DELAY_IC_1_SIZE, R69_START_DELAY_IC_1_Default );
    SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1, REG_HIBERNATE_IC_1_ADDR, REG_HIBERNATE_IC_1_BYTE, R70_HIBERNATE_IC_1_Default );
}

//void ADAU1467::spi_init()
//{
//    fd_spi = open(spiDev, O_RDWR);
//    if (fd_spi < 0)
//        qDebug() << "can't open device";
//    ret_spi = ioctl(fd_spi, SPI_IOC_WR_MODE, &mode_spi);
//    if (ret_spi == -1)
//        qDebug() << ("can't set spi mode");
//    ret_spi = ioctl(fd_spi, SPI_IOC_RD_MODE, &mode_spi);
//    if (ret_spi == -1)
//        qDebug() << ("can't get spi mode");
//    ret_spi = ioctl(fd_spi, SPI_IOC_WR_BITS_PER_WORD, &bits);
//    if (ret_spi == -1)
//        qDebug() << ("can't set bits per word");
//    ret_spi = ioctl(fd_spi, SPI_IOC_RD_BITS_PER_WORD, &bits);
//    if (ret_spi == -1)
//        qDebug() << ("can't get bits per word");
//    ret_spi = ioctl(fd_spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
//    if (ret_spi == -1)
//        qDebug() << ("can't set max speed hz");
//    ret_spi = ioctl(fd_spi, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
//    if (ret_spi == -1)
//        qDebug() << ("can't get max speed hz");

//    printf("spi mode: 0x%x\n", mode_spi);
//    printf("bits per word: %d\n", bits);
//    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
//}

bool ADAU1467::send_byte_data(uint8_t const *byteData, uint32_t len)
{
    return DSPSPI->send_byte_data(tx,rx,len);

//    struct spi_ioc_transfer tr = {
//        tr.tx_buf = (unsigned long)byteData,
//        tr.rx_buf = (unsigned long)rx,
//        tr.len = len,
//        tr.delay_usecs = delay,
//        static_cast<__u16>(tr.speed_hz = speed),
//        tr.bits_per_word = bits,
//    };
//    if (mode_spi & SPI_TX_QUAD)
//        tr.tx_nbits = 4;
//    else if (mode_spi & SPI_TX_DUAL)
//        tr.tx_nbits = 2;
//    if (mode_spi & SPI_RX_QUAD)
//        tr.rx_nbits = 4;
//    else if (mode_spi & SPI_RX_DUAL)
//        tr.rx_nbits = 2;
//    if (!(mode_spi & SPI_LOOP)) {
//        if (mode_spi & (SPI_TX_QUAD | SPI_TX_DUAL))
//            tr.rx_buf = 0;
//        else if (mode_spi & (SPI_RX_QUAD | SPI_RX_DUAL))
//            tr.tx_buf = 0;
//    }
//    ret_spi = ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr);
//    if (ret_spi < 0){
//        qDebug() << "can't send_byte_data" << ret_spi;
//        return  false;
//    }
//    return  true;
}

void ADAU1467::SIGMASTUDIOTYPE_REGISTER_CONVERT(int32_t fixpt_val, byte dest[]) {
    dest[0] = (fixpt_val >> 24) & 0xFF;
    dest[1] = (fixpt_val >> 16) & 0xFF;
    dest[2] = (fixpt_val >> 8) & 0xFF;
    dest[3] = (fixpt_val)&0xFF;
}


void ADAU1467::SIGMASTUDIOTYPE_INTEGER_CONVERT(int32_t fixpt_val, byte dest[]) {
    dest[0] = (fixpt_val >> 24) & 0xFF;
    dest[1] = (fixpt_val >> 16) & 0xFF;
    dest[2] = (fixpt_val >> 8) & 0xFF;
    dest[3] = (fixpt_val)&0xFF;
}

void ADAU1467::SIGMA_WRITE_REGISTER_BLOCK(byte devAddress, int address, int length, byte pData[])
{
    SIGMA_WRITE_REGISTER_BLOCK(address, length, pData);
}
void ADAU1467::SIGMA_WRITE_REGISTER_BLOCK(int address, int length, byte pData[])
{
    int countData = 3;
    int dataIndex;
    int i = 3;
    tx[0] = 0x00;
    tx[1] = byte(address >> 8);
    tx[2] = byte(address & 0xff);
    for (i=3; i < length+3; i++)
    {
        countData++;
        dataIndex = i%4096;
        tx[dataIndex] = pData[i-3];
        if (countData == 4096)
        {
            send_byte_data(tx, 4096);
            countData = 0;
//            qDebug() << "SIGMA_WRITE_REGISTER_BLOCK address" << QString::number(address, 16) << "index" << i; // displayed as a hex string
        }
    }
    dataIndex = i%4096;
    send_byte_data(tx,uint32_t(dataIndex));
//    qDebug() << "SIGMA_WRITE_REGISTER_BLOCK address" << QString::number(address, 16) << "index" << i; // displayed as a hex string
}

// Write a 32-bit integer to the DSP. NOTE: 5.23 not supported quite yet.
void ADAU1467::SIGMA_WRITE_REGISTER_INTEGER(int address, int32_t pData) {
    byte byte_data[4];
    SIGMASTUDIOTYPE_REGISTER_CONVERT(pData, byte_data);
    SIGMA_WRITE_REGISTER_BLOCK(address, 4, byte_data);
}

void ADAU1467::SIGMA_WRITE_REGISTER_FLOAT(int address, double pData) {
    SIGMA_WRITE_REGISTER_INTEGER(address, SIGMASTUDIOTYPE_FIXPOINT_CONVERT(pData));
}

void ADAU1467::SIGMA_WRITE_DELAY(byte devAddress, int length, byte pData[]) {
    int delay_length = 0;    // Initialize delay length variable
    for (byte i = length; i > 0; i--) {
        // Unpack pData to calculate the delay length as an integer
        delay_length = (delay_length << 8) + pData[i];
    }
    QThread::msleep(delay_length);    // Delay this processor (not the DSP) by the appropriate time
}
void ADAU1467::SIGMA_READ_REGISTER_BYTES(uint16_t address, int length, byte *pData)
{
    tx[0] = 0x01;
    tx[1] = byte(address >> 8);
    tx[2] = byte(address & 0xff);
    for (int i = 0; i < length; i++)
    {
        tx[i+3] = 0xFF;
    }
//    send_byte_data(tx,uint32_t(length+3));
    pData = rx;
    DSPSPI->send_byte_data(tx,rx,uint32_t(length+3));

}

uint32_t ADAU1467::SIGMA_READ_REGISTER_INTEGER(uint16_t address, int length)
{
    uint32_t result = 0;
    byte register_value[length];
    SIGMA_READ_REGISTER_BYTES(address, length, register_value);
    for (int i = 0; i < length; i++) {
        result = (result << 8) + rx[i+3];
    }
//    printf("address 0x%02X, result 0x%02X 0x%02X  0x%02X  0x%02X 0x%02X 0x%02X  0x%02X  0x%02X ", address , rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
    return result;
}
double ADAU1467::SIGMA_READ_REGISTER_FLOAT(int address) {
    int32_t integer_val = SIGMA_READ_REGISTER_INTEGER(address, 4);
#if DSP_TYPE == DSP_TYPE_SIGMA300_350
    return double(integer_val) / (1 << 24);
#else
    return double(integer_val) / (1 << 23);
#endif
}
void ADAU1467::SIGMA_PRINT_REGISTER(int address, int dataLength)
{
    // qDebug() << QString("VALUE AT 0x%1").arg(address, 8, 16, QLatin1Char( '0' ));
    byte register_value[DSPDATABUF] = {0};
    SIGMA_READ_REGISTER_BYTES(address, dataLength, register_value);
    for (int i = 0; i < dataLength; i++) {
        qDebug() << QString("0x%1").arg(register_value[i], 8, 16, QLatin1Char( '0' ));
    }
}

/* SIGMASTUDIOTYPE_INTEGER_CONVERT is included in export files but not generally required.
 * Here it is simply a passthrough macro.
 */
