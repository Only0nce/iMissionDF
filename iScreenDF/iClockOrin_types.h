#ifndef ICLOCKORIN_TYPES_H
#define ICLOCKORIN_TYPES_H

#include <QString>
#include <QList>
#include <QMap>

// เก็บข้อมูลดาวเทียมแต่ละดวง
struct SatInfo {
    int     prn    = 0;     // legacy PRN / slot id
    int     gnssid = -1;    // 0=GPS,1=SBAS,2=Galileo,3=BeiDou,4=IMES,5=QZSS,6=GLONASS,7=NavIC, -1=unknown
    int     svid   = -1;    // Space vehicle id ต่อ GNSS
    double  snr    = 0.0;   // dB-Hz
    double  elev   = 0.0;   // elevation degrees
    double  az     = 0.0;   // azimuth degrees
    bool    used   = false; // ถูกนำไปคำนวณ fix หรือไม่
    QString constel;        // ชื่อกลุ่มดาว เช่น "GPS","GLO","GAL","BDS","QZSS","SBAS","NavIC","IMES","UNK"
};

// เก็บข้อมูล fix และดาวเทียมทั้งหมดของ 1 receiver
struct GPSInfo {
    QString date   = "";   // UTC YYYY-MM-DD
    QString time   = "";   // UTC HH:mm:ss.zzz
    double  lat    = 0;    // latitude
    double  lon    = 0;    // longitude
    double  alt    = 0;    // altitude (m)
    int     sat    = 0;    // จำนวนดาวเทียมที่เห็น
    int     satUse = 0;    // จำนวนดาวเทียมที่ใช้จริง
    double  speed  = 0;    // m/s
    int    locked = 0;// fix ได้หรือไม่

    QList<SatInfo> sats;          // รายการดาวเทียมทั้งหมด
    QMap<QString,int> constelCounts; // นับจำนวนต่อกลุ่มดาว
};

#endif // ICLOCKORIN_TYPES_H
