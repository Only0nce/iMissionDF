#include "SPI.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
SPIClass::SPIClass(const std::string& spidev)
    : spiDev(spidev)
{
    spi_dev = new Linux_SPI;
    spi_init();
}

SPIClass::~SPIClass()
{
    delete spi_dev;
    spi_dev = nullptr;
}
void SPIClass::delay_mSec(int mSec)
 {
  QTime dieTime= QTime::currentTime().addMSecs(mSec);
  while( QTime::currentTime() < dieTime )
  QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void SPIClass::spi_init()
{
    m_ready = false;
    if (!spi_dev) {
        qCritical() << "[SPI] backend object is null for" << QString::fromStdString(spiDev);
        return;
    }

    const auto fail = [this](const char *stage) {
        qCritical().noquote() << "[SPI] initialization failed"
                              << "stage=" << stage
                              << "device=" << QString::fromStdString(spiDev)
                              << "error=" << spi_dev->strerror(spi_dev->get_errno())
                              << "(continuing in degraded audio-DSP mode)";
        spi_dev->dev_close();
    };

    if (spi_dev->dev_open(spiDev.c_str()) != 0) {
        fail("open");
        return;
    }
    if (spi_dev->set_mode(SPI_MODE) != 0) {
        fail("mode");
        return;
    }
    if (spi_dev->set_bits_per_word(bits) != 0) {
        fail("bits-per-word");
        return;
    }
    if (spi_dev->set_max_speed_hz(speed) != 0) {
        fail("speed");
        return;
    }

    m_ready = true;
    qInfo() << "[SPI] ready" << QString::fromStdString(spiDev)
            << "mode=" << SPI_MODE << "bits=" << bits << "speed=" << speed;
}
void SPIClass::clear_rx(){
    for (int i = 0; i < 4096; i++){
        rx[i]  = 0;
    }
}

int SPIClass::send_byte_data(uint8_t *txByteData,uint8_t *rxByteData, uint32_t len)
{
    if (!m_ready || !spi_dev || !txByteData || len == 0)
        return -1;

    struct spi_ioc_transfer mesg[2];
    uint16_t val = 1600;
    int ret, i;
    uint16_t buf[4096];
    memset(buf,  0, sizeof(buf));
    memset(mesg, 0, sizeof(mesg));


    mesg[0].bits_per_word = 8;
    mesg[0].rx_buf        = (uintptr_t)rxByteData;
    mesg[0].tx_buf        = (uintptr_t)txByteData;
    mesg[0].len           = len;
    mesg[0].cs_change     = 0;
//    mesg[0].delay_usecs   = 10000;


//    mesg[1].bits_per_word = 16;
//    mesg[1].rx_buf        = (uintptr_t)buf;
//    mesg[1].tx_buf        = (uintptr_t)NULL;
//    mesg[1].len           = 4096;
//    mesg[1].cs_change     = 0;



    ret = spi_dev->send_tr(mesg, 1);
//    qDebug() << "spi_dev->send_tr(mesg, 1)" << rxByteData[5];

    return ret;
}
void SPIClass::init(void)
{
  spi_init();
}
