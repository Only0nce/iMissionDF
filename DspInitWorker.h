#pragma once

#include <QObject>
#include <QString>

class DspInitWorker : public QObject
{
    Q_OBJECT
public:
    explicit DspInitWorker(QObject *parent = nullptr);

    void setPaths(const QString &spidevPath,
                  const QString &i2cDev,
                  int i2cAddr);

    void setInitialGains(unsigned char in1, unsigned char in2, unsigned char in3, unsigned char in4,
                         unsigned char out1, unsigned char out2, unsigned char out3, unsigned char out4);

    void setRunSpeakerTest(bool enable);

public slots:
    void init(); // ✅ run in worker thread

signals:
    void dspReady(quintptr sigmaPtr,
                  quintptr codecPtr,
                  bool ok,
                  const QString &detail);

private:
    QString m_spidevPath = "/dev/spidev0.0";
    QString m_i2cDev = "/dev/i2c-1";
    int     m_i2cAddr = 0;

    bool m_runSpeakerTest = true;

    unsigned char m_in1 = 10, m_in2 = 10, m_in3 = 10, m_in4 = 10;
    unsigned char m_out1 = 30, m_out2 = 30, m_out3 = 30, m_out4 = 30;
};
