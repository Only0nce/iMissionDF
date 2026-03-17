#include "GpsdReader.h"
#include <QDateTime>
#include <QtMath>
#include <cmath>
#include <ctime>
#include <signal.h>
#include <cstring>
#include <QFileInfo>

// -------------------- helpers --------------------
static inline QString toUtcDate(double tsec) {
    if (!(tsec > 0)) return {};
    const qint64 ms = static_cast<qint64>(tsec * 1000.0);
    return QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC).date().toString("yyyy-MM-dd");
}
static inline QString toUtcTime(double tsec) {
    if (!(tsec > 0)) return {};
    const qint64 ms = static_cast<qint64>(tsec * 1000.0);
    return QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC).time().toString("HH:mm:ss.zzz");
}

// helper: แปลงเวลาจาก libgps รุ่นใหม่/เก่า
static inline double fixTimeToSec(const gps_fix_t& fix) {
#if defined(GPSD_API_MAJOR_VERSION) && (GPSD_API_MAJOR_VERSION >= 9)
    return (double)fix.time.tv_sec + (double)fix.time.tv_nsec * 1e-9;
#else
    return fix.time;
#endif
}

// -------------------- GpsdReader --------------------
GpsdReader::GpsdReader(QObject* parent) : QObject(parent) {}
GpsdReader::~GpsdReader() { stop(); }

void GpsdReader::setGpsdEndpoint(const QString& host, quint16 port) {
    {
        QMutexLocker lk(&m_);
        gpsdHost_ = host.trimmed();
        gpsdPort_ = port;
    }
    if (worker_) {
        reconnectRequested_.store(true);
    }
}

void GpsdReader::setDeviceMap(const QString& dev1, const QString& dev2) {
    QMutexLocker lk(&m_);
    dev1Path_ = dev1;
    dev2Path_ = dev2;
}

void GpsdReader::start() {
    if (worker_) return;
    running_.store(true);
    worker_ = new Worker(this);
    worker_->setObjectName("GpsdReaderWorker");
    worker_->start();
}

void GpsdReader::stop() {
    if (!worker_) return;
    running_.store(false);
    worker_->quit();
    worker_->wait();
    delete worker_;
    worker_ = nullptr;
}

// -------------------- Worker --------------------
void GpsdReader::Worker::run() {
    signal(SIGPIPE, SIG_IGN);

    int backoff_ms = 500;
    const int backoff_ms_max = 10000;

    const int idle_us = 500000;
    const qint64 no_data_timeout_ms = 5000;

    while (outer_->running_.load()) {

        gps_data_t gd;
        std::memset(&gd, 0, sizeof(gd));

        // --- อ่าน endpoint ล่าสุด ---
        QString host;
        QString portStr;
        {
            QMutexLocker lk(&outer_->m_);
            host = outer_->gpsdHost_;
            portStr = QString::number(outer_->gpsdPort_);
        }

        // 1) connect (with backoff)
        while (outer_->running_.load()) {

            if (outer_->reconnectRequested_.exchange(false)) {
                QMutexLocker lk(&outer_->m_);
                host = outer_->gpsdHost_;
                portStr = QString::number(outer_->gpsdPort_);
            }

            if (gps_open(host.toUtf8().constData(), portStr.toUtf8().constData(), &gd) == 0) {
                if (gps_stream(&gd, WATCH_ENABLE | WATCH_JSON, nullptr) == 0) {
                    emit outer_->gpsdConnected(true);
                    backoff_ms = 500;
                    break;
                } else {
                    emit outer_->errorOccurred(QString("gps_stream: %1").arg(gps_errstr(errno)));
                    gps_close(&gd);
                }
            } else {
                emit outer_->errorOccurred(QString("gps_open(%1:%2): %3")
                                               .arg(host).arg(portStr).arg(gps_errstr(errno)));
            }

            if (!outer_->running_.load()) return;
            QThread::msleep(backoff_ms);
            backoff_ms = qMin(backoff_ms * 2, backoff_ms_max);
        }
        if (!outer_->running_.load()) break;

        // 2) read loop (with watchdog)
        bool need_reconnect = false;
        qint64 last_rx_ms = QDateTime::currentMSecsSinceEpoch();

        while (outer_->running_.load() && !need_reconnect) {

            if (outer_->reconnectRequested_.exchange(false)) {
                need_reconnect = true;
                break;
            }

            if (gd.gps_fd < 0) {
                emit outer_->errorOccurred("gps_fd invalid -> reconnect");
                need_reconnect = true;
                break;
            }

            const bool hasData = gps_waiting(&gd, idle_us);

            if (!hasData) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (now - last_rx_ms > no_data_timeout_ms) {
                    emit outer_->errorOccurred(
                        QString("gps_waiting timeout %1 ms -> reconnect").arg(no_data_timeout_ms));
                    need_reconnect = true;
                }
                continue;
            }

            errno = 0;
            int r = gps_read(&gd, nullptr, 0);

            if (r > 0) {
                last_rx_ms = QDateTime::currentMSecsSinceEpoch();
                const char* dev = (gd.dev.path[0] ? gd.dev.path : nullptr);
                outer_->handleFixAndSky(gd, dev);
            } else if (r == 0) {
                continue;
            } else {
                const int e = errno;
                emit outer_->errorOccurred(
                    QString("gps_read error r=%1 errno=%2 (%3)")
                        .arg(r).arg(e).arg(gps_errstr(e)));
                need_reconnect = true;
            }
        }

        // 3) cleanup ก่อนวน reconnect
        gps_stream(&gd, WATCH_DISABLE, nullptr);
        gps_close(&gd);
        emit outer_->gpsdConnected(false);
    }
}

// -------------------- constellation helpers --------------------
QString GpsdReader::constelNameFromGnssid(int g) {
    switch (g) {
    case 0:  return "GPS";
    case 1:  return "SBAS";
    case 2:  return "GAL";
    case 3:  return "BDS";
    case 4:  return "IMES";
    case 5:  return "QZSS";
    case 6:  return "GLO";
    case 7:  return "NAVIC";
    default: return "UNK";
    }
}

QString GpsdReader::constelNameHeuristicFromPRN(int prn) {
    if (prn >= 1   && prn <= 32)  return "GPS";
    if (prn >= 65  && prn <= 96)  return "GLO";
    if (prn >= 120 && prn <= 158) return "SBAS";
    if (prn >= 193 && prn <= 200) return "QZSS";
    if (prn >= 201 && prn <= 237) return "BDS";
    if (prn >= 301 && prn <= 336) return "GAL";
    return "UNK";
}

// -------------------- core: handleFixAndSky --------------------
void GpsdReader::handleFixAndSky(const gps_data_t& gd, const char* devPathC) {
    if (!devPathC) return;
    QString devPath(devPathC);

    // device mapping
    QString dev1, dev2;
    {
        QMutexLocker lk(&m_);
        dev1 = dev1Path_;
        dev2 = dev2Path_;
    }

    GPSInfo* st = nullptr;
    bool isDev1 = false;

    const QString dev1Base = dev1.isEmpty() ? QString() : QFileInfo(dev1).fileName();
    const QString dev2Base = dev2.isEmpty() ? QString() : QFileInfo(dev2).fileName();

    const bool matchDev1 =
        (!dev1.isEmpty() && (devPath == dev1 || devPath.endsWith(dev1Base)));
    const bool matchDev2 =
        (!dev2.isEmpty() && (devPath == dev2 || devPath.endsWith(dev2Base)));

    if (matchDev1) { st = &state1_; isDev1 = true; }
    else if (matchDev2) { st = &state2_; }
    else {
        // ✅ fallback: ถ้าใช้ device เดียว (dev2 ว่าง) แล้ว path รายงานเปลี่ยน
        if (!dev1.isEmpty() && dev2.isEmpty()) {
            st = &state1_;
            isDev1 = true;
        } else {
            return;
        }
    }

    bool changed = false;

    #ifdef PLATFORM_JETSON
    // ---------- FIX/TPV ----------
    if (gd.set & (TIME_SET | LATLON_SET | ALTITUDE_SET | SPEED_SET | MODE_SET | STATUS_SET)) {

        // time (อัปเดตได้แม้ไม่ lock)
        const double tsec = fixTimeToSec(gd.fix);
        const QString newDate = (tsec > 0) ? toUtcDate(tsec) : QString();
        const QString newTime = (tsec > 0) ? toUtcTime(tsec) : QString();
        if (st->date != newDate) { st->date = newDate; changed = true; }
        if (st->time != newTime) { st->time = newTime; changed = true; }

        // lock 판단
        const bool modeLock = (gd.fix.mode >= MODE_2D);
        const bool statusOk = (gd.status == STATUS_FIX || gd.status == STATUS_DGPS_FIX);
        const bool hasFix   = (modeLock && statusOk);

        const int newLocked = hasFix ? 1 : 0;
        if (st->locked != newLocked) { st->locked = newLocked; changed = true; }

        // ✅ สำคัญ: อัปเดต lat/lon/alt/speed เฉพาะเมื่อ hasFix + ค่าเป็น finite
        if (hasFix &&
            std::isfinite(gd.fix.latitude) &&
            std::isfinite(gd.fix.longitude))
        {
            const double newLat = gd.fix.latitude;
            const double newLon = gd.fix.longitude;

            if (st->lat != newLat) { st->lat = newLat; changed = true; }
            if (st->lon != newLon) { st->lon = newLon; changed = true; }

            if (std::isfinite(gd.fix.altitude)) {
                const double newAlt = gd.fix.altitude;
                if (st->alt != newAlt) { st->alt = newAlt; changed = true; }
            }
            if (std::isfinite(gd.fix.speed)) {
                const double newSpd = gd.fix.speed;
                if (st->speed != newSpd) { st->speed = newSpd; changed = true; }
            }
        }
        // ถ้าไม่มี fix: ห้ามไป set lat/lon=0 ทับค่าที่เคยได้
    }
#else
    if (gd.set & (TIME_SET | LATLON_SET | ALTITUDE_SET | SPEED_SET | MODE_SET | STATUS_SET)) {

        const double tsec = fixTimeToSec(gd.fix);
        st->date = (tsec > 0) ? QDateTime::fromMSecsSinceEpoch((qint64)(tsec * 1000.0), Qt::UTC)
                                    .date().toString("yyyy-MM-dd")
                              : QString();
        st->time = (tsec > 0) ? QDateTime::fromMSecsSinceEpoch((qint64)(tsec * 1000.0), Qt::UTC)
                                    .time().toString("HH:mm:ss.zzz")
                              : QString();

        // ✅ gpsd 3.20: ใช้ mode อย่างเดียว (ไม่มี gd.status แล้ว)
        const bool hasFix = (gd.fix.mode >= MODE_2D);
        st->locked = hasFix ? 1 : 0;

        // ✅ สำคัญ: อัปเดต lat/lon/alt เฉพาะตอนมี fix และค่าเป็น finite (กัน NaN->0)
        if (hasFix &&
            std::isfinite(gd.fix.latitude) &&
            std::isfinite(gd.fix.longitude))
        {
            st->lat   = gd.fix.latitude;
            st->lon   = gd.fix.longitude;

            if (std::isfinite(gd.fix.altitude))
                st->alt = gd.fix.altitude;

            if (std::isfinite(gd.fix.speed))
                st->speed = gd.fix.speed;
        }

        changed = true;
    }
#endif

// ---------- SKY ----------
#ifdef SATELLITE_SET
    if (gd.set & SATELLITE_SET)
#else
    if (gd.satellites_visible >= 0)
#endif
    {
        const int newSat = gd.satellites_visible;
        if (st->sat != newSat) { st->sat = newSat; changed = true; }

        // rebuild sat list
        st->satUse = 0;
        st->sats.clear();
        st->constelCounts.clear();

        const int n = qMin(gd.satellites_visible, (int)MAXCHANNELS);
        for (int i = 0; i < n; ++i) {
            SatInfo s;
            const int prn = gd.skyview[i].PRN;

            s.prn  = prn;
            s.snr  = std::isfinite(gd.skyview[i].ss)        ? gd.skyview[i].ss        : 0.0;
            s.elev = std::isfinite(gd.skyview[i].elevation) ? gd.skyview[i].elevation : 0.0;
            s.az   = std::isfinite(gd.skyview[i].azimuth)   ? gd.skyview[i].azimuth   : 0.0;
            s.used = (gd.skyview[i].used != 0);
            if (s.used) st->satUse++;

            int gnssid = -1, svid = -1;
#if defined(STRUCT_SKYVIEW_T_HAS_GNSSID)
            gnssid = gd.skyview[i].gnssid;
            svid   = gd.skyview[i].svid;
#endif
            s.gnssid = gnssid;
            s.svid   = svid;

            s.constel = (gnssid >= 0 && gnssid <= 7)
                            ? constelNameFromGnssid(gnssid)
                            : constelNameHeuristicFromPRN(prn);

            st->sats.append(s);
            st->constelCounts[s.constel] = st->constelCounts.value(s.constel, 0) + 1;
        }

        changed = true;
    }

    // ---------- emit ----------
    if (changed) {
        if (isDev1) emit gps1Updated(*st);
        else        emit gps2Updated(*st);
    }
}
