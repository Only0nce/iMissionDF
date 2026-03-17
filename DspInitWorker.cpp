#include "DspInitWorker.h"

#include <QProcess>
#include <QThread>
#include <QDebug>

#include <SigmaStudioFW.h>
#include <PCM3168A.h>

DspInitWorker::DspInitWorker(QObject *parent) : QObject(parent) {}

void DspInitWorker::setPaths(const QString &spidevPath,
                             const QString &i2cDev,
                             int i2cAddr)
{
    m_spidevPath = spidevPath;
    m_i2cDev     = i2cDev;
    m_i2cAddr    = i2cAddr;
}

void DspInitWorker::setInitialGains(unsigned char in1, unsigned char in2, unsigned char in3, unsigned char in4,
                                    unsigned char out1, unsigned char out2, unsigned char out3, unsigned char out4)
{
    m_in1 = in1; m_in2 = in2; m_in3 = in3; m_in4 = in4;
    m_out1 = out1; m_out2 = out2; m_out3 = out3; m_out4 = out4;
}

void DspInitWorker::setRunSpeakerTest(bool enable)
{
    m_runSpeakerTest = enable;
}

void DspInitWorker::init()
{
    qDebug() << "[DspInitWorker] init thread=" << QThread::currentThread();

    // 1) speaker-test (หนัก) ให้รันใน worker
    if (m_runSpeakerTest) {
        // NOTE: ใช้ startDetached เพื่อไม่ค้างแม้ speaker-test ดื้อ
        QProcess::startDetached("speaker-test",
                                {"-Dhw:APE,0","-r8000","-c8","-S0",
                                 "--nloops","3","-s","1","-tsine","-f1000"});
        // ถ้าต้องการ “รอให้จบ” จริง ๆ ค่อยเปลี่ยนเป็น waitForFinished
        QThread::msleep(200);
    }

    ADAU1467 *sigma = nullptr;
    PCM3168A *codec = nullptr;

    try {
        // 2) DSP firmware
        sigma = new ADAU1467(m_spidevPath.toUtf8().constData());
        sigma->default_download_IC_1();
        QThread::msleep(200);

        sigma->setToneVolume(TONE_CH1_ADDRESS,0);
        sigma->setToneVolume(TONE_CH2_ADDRESS,0);
        sigma->setToneVolume(TONE_CH3_ADDRESS,0);
        sigma->setToneVolume(TONE_CH4_ADDRESS,0);

        sigma->setToneVolume(TONE_SERVER_CH1_ADDRESS,0);
        sigma->setToneVolume(TONE_SERVER_CH2_ADDRESS,0);
        sigma->setToneVolume(TONE_SERVER_CH3_ADDRESS,0);
        sigma->setToneVolume(TONE_SERVER_CH4_ADDRESS,0);

        // 3) CODEC
        codec = new PCM3168A(m_i2cDev.toUtf8().constData(), m_i2cAddr);

        sigma->setMixerVolume(AUDIOIN_VOLUME_CH1_ADDRESS,SIDETONE_VOLUME_CH1_MODE_ADDRESS,SIDETONE_VOLUME_CH1_MODE_VALUE,1);
        sigma->setMixerVolume(AUDIOIN_VOLUME_CH2_ADDRESS,SIDETONE_VOLUME_CH2_MODE_ADDRESS,SIDETONE_VOLUME_CH2_MODE_VALUE,1);
        sigma->setMixerVolume(AUDIOIN_VOLUME_CH3_ADDRESS,SIDETONE_VOLUME_CH3_MODE_ADDRESS,SIDETONE_VOLUME_CH3_MODE_VALUE,1);
        sigma->setMixerVolume(AUDIOIN_VOLUME_CH4_ADDRESS,SIDETONE_VOLUME_CH4_MODE_ADDRESS,SIDETONE_VOLUME_CH4_MODE_VALUE,1);

        sigma->setFIRfilter(MOD_FIR1_ALG0_FIRSIGMA300ALG1FIRCOEFF0_ADDR,MOD_FIR1_COUNT,nullptr); // (คุณ set จริงใน main ต่อ)
        sigma->setFIRfilter(MOD_FIR2_ALG0_FIRSIGMA300ALG5FIRCOEFF0_ADDR,MOD_FIR2_COUNT,nullptr);

        // input gains
        codec->setInputGain(CODECCH1_I2S1, m_in1);
        codec->setInputGain(CODECCH2_I2S1, m_in2);
        codec->setInputGain(CODECCH3_I2S1, m_in3);
        codec->setInputGain(CODECCH4_I2S1, m_in4);

        // output gains
        codec->setOutputGain(CODECCH1_I2S1, m_out1);
        codec->setOutputGain(CODECCH2_I2S1, m_out2);
        codec->setOutputGain(CODECCH3_I2S1, m_out3);
        codec->setOutputGain(CODECCH4_I2S1, m_out4);

        // mute 5-8
        codec->setOutputGain(5,0);
        codec->setOutputGain(6,0);
        codec->setOutputGain(7,0);
        codec->setOutputGain(8,0);

        emit dspReady(reinterpret_cast<quintptr>(sigma),
                      reinterpret_cast<quintptr>(codec),
                      true,
                      "DSP/CODEC init ok");
        return;

    } catch (...) {
        delete sigma;
        delete codec;
        emit dspReady(0,0,false,"DSP/CODEC init exception");
        return;
    }
}
