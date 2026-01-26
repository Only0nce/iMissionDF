#pragma once
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QAtomicInteger>
#include "iScreenDF/iClockOrin_types.h"

extern "C" {
#include <gps.h>
}

class GpsdReader : public QObject {
    Q_OBJECT
public:
    explicit GpsdReader(QObject* parent=nullptr);
    ~GpsdReader();

    void setDeviceMap(const QString& dev1, const QString& dev2);

    // ✅ NEW: ตั้งปลายทาง gpsd
    void setGpsdEndpoint(const QString& host, quint16 port = 2947);

    void start();
    void stop();

signals:
    void gpsdConnected(bool ok);
    void errorOccurred(const QString& err);

    // ของเดิมของคุณ
    void gps1Updated(const GPSInfo& info);
    void gps2Updated(const GPSInfo& info);

private:
    class Worker : public QThread {
    public:
        explicit Worker(GpsdReader* outer) : outer_(outer) {}
        void run() override;
    private:
        GpsdReader* outer_ = nullptr;
    };

    void handleFixAndSky(const gps_data_t& gd, const char* devPathC);
    static QString constelNameFromGnssid(int g);
    static QString constelNameHeuristicFromPRN(int prn);

private:
    QMutex m_;
    Worker* worker_ = nullptr;
    std::atomic<bool> running_{false};

    // ✅ NEW endpoint
    QString gpsdHost_ = "127.0.0.1";
    quint16 gpsdPort_ = 2947;

    // ✅ NEW: สั่งให้ worker reconnect
    std::atomic<bool> reconnectRequested_{false};

    QString dev1Path_;
    QString dev2Path_;

    GPSInfo state1_;
    GPSInfo state2_;
};
