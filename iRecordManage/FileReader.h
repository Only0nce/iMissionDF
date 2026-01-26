#pragma once
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QVariantMap>
#include <QDebug>

class FileReader : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QByteArray readFile(const QString &path) {
        QString fixedPath = normalizePath(path);
        qDebug() << "[FileReader.readFile] fixedPath:" << fixedPath;

        QFile f(fixedPath);
        if (!f.exists()) {
            qWarning() << "[FileReader] File not found:" << fixedPath;
            return QByteArray();
        }
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "[FileReader] Cannot open file:" << fixedPath << f.errorString();
            return QByteArray();
        }
        QByteArray data = f.readAll();
        f.close();
        qDebug() << "[FileReader] Read" << data.size() << "bytes from" << fixedPath;
        return data;
    }

    // คืนความยาวไฟล์ .wav (ms)  ถ้าอ่านไม่ได้คืน -1
    Q_INVOKABLE int wavDurationMs(const QString &path) {
        QString p = normalizePath(path);
        return readWavDurationMsImpl(p);
    }

    // คืนเมตาแบบครบ (ใช้สะดวกใน QML)
    // { ok:bool, duration_ms:int, sample_rate:int, channels:int, bits:int }
    Q_INVOKABLE QVariantMap wavMeta(const QString &path) {
        QString p = normalizePath(path);
        QVariantMap m;
        m["ok"] = false;
        m["duration_ms"] = -1;
        m["sample_rate"] = 0;
        m["channels"]    = 0;
        m["bits"]        = 0;

        QFile f(p);
        if (!f.open(QIODevice::ReadOnly) || f.size() < 44) {
            qWarning() << "[FileReader.wavMeta] open fail or too small:" << p;
            return m;
        }

        auto rdU8  = [&](qint64 off) -> quint8  {
            f.seek(off);
            char b;
            if (f.read(&b, 1) != 1) return 0;
            return (quint8)(uchar)b;
        };
        auto rdU16 = [&](qint64 off) -> quint16 {
            f.seek(off);
            unsigned char b[2];
            if (f.read((char*)b, 2) != 2) return 0;
            return (quint16)b[0] | ((quint16)b[1] << 8);
        };
        auto rdU32 = [&](qint64 off) -> quint32 {
            f.seek(off);
            unsigned char b[4];
            if (f.read((char*)b, 4) != 4) return 0;
            return  (quint32)b[0]
                   | ((quint32)b[1] << 8)
                   | ((quint32)b[2] << 16)
                   | ((quint32)b[3] << 24);
        };

        // ตรวจ RIFF/WAVE
        if (!(rdU8(0)=='R' && rdU8(1)=='I' && rdU8(2)=='F' && rdU8(3)=='F')) return m;
        if (!(rdU8(8)=='W' && rdU8(9)=='A' && rdU8(10)=='V' && rdU8(11)=='E')) return m;

        qint64 off = 12;
        int sampleRate = 0, channels = 0, bits = 0;
        qint64 dataOff = -1;
        quint32 dataSizeFromHeader = 0;

        while (off + 8 <= f.size()) {
            char id[4];
            f.seek(off);
            if (f.read(id, 4) != 4) break;

            quint32 sz = rdU32(off + 4);
            QByteArray cid(id, 4);

            off += 8; // now off points to chunk data

            if (cid == "fmt ") {
                channels   = rdU16(off + 2);
                sampleRate = (int)rdU32(off + 4);
                bits       = rdU16(off + 14);
            } else if (cid == "data") {
                dataOff = off;
                dataSizeFromHeader = sz;
                break;
            }

            // ✅ move to next chunk (RIFF chunks are word-aligned)
            off += sz;
            if (sz & 1) off += 1;   // ✅ padding
        }

        if (dataOff < 0 || sampleRate <= 0 || channels <= 0 || bits <= 0) return m;

        const int bytesPerSample = bits / 8;
        if (bytesPerSample <= 0) return m;

        // ✅ ใช้ขนาดข้อมูลจริงจากไฟล์ ป้องกัน header เพี้ยน
        qint64 maxPossible = f.size() - dataOff;
        if (maxPossible < 0) maxPossible = 0;

        quint32 realDataSize = dataSizeFromHeader;
        if ((qint64)realDataSize > maxPossible)
            realDataSize = (quint32)maxPossible;

        const double seconds = double(realDataSize) / double(sampleRate * channels * bytesPerSample);
        const int durMs = (int)(seconds * 1000.0 + 0.5);

        m["ok"]          = true;
        m["duration_ms"] = durMs;
        m["sample_rate"] = sampleRate;
        m["channels"]    = channels;
        m["bits"]        = bits;

        qDebug() << "[wavMeta]" << p
                 << "ok=1"
                 << "sr=" << sampleRate
                 << "ch=" << channels
                 << "bits=" << bits
                 << "data_hdr=" << dataSizeFromHeader
                 << "data_real=" << realDataSize
                 << "dur_ms=" << durMs;

        return m;

    }

    // ===================== SAVE SELECTED WAV FILES =====================
    Q_INVOKABLE bool saveWaveSelectionState(
        const QVariantList &files,
        int totalFiles,
        int sampleCount,
        int totalMs,
        double totalDurationSec,
        int samplesLength,
        double totalSizeKB,
        const QString &txtPath = "/home/orinnx/saveFileName/filesNameWave.txt")
    {
        const QString p = normalizePath(txtPath);

        qDebug() << "================ saveWaveSelectionState ================";
        qDebug() << "[PATH]" << p;
        qDebug() << "[INPUT] files.size=" << files.size()
                 << "totalFiles=" << totalFiles
                 << "sampleCount=" << sampleCount
                 << "totalMs=" << totalMs
                 << "totalDurationSec=" << totalDurationSec
                 << "samplesLength=" << samplesLength
                 << "totalSizeKB=" << totalSizeKB;;

        QFile f(p);

        // 1) open
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qCritical() << "[ERROR] open failed:" << p << f.errorString();
            qDebug() << "========================================================";
            return false;
        }
        qDebug() << "[OPEN OK] writing...";

        QTextStream ts(&f);
        ts.setCodec("UTF-8");

        // 2) write files section
        ts << "#FILES\n";

        int countWrite = 0;
        for (const QVariant &v : files) {
            QString s;

            // รองรับทั้ง string list และ list ของ object/map (เผื่อส่งมาแบบนั้น)
            if (v.type() == QVariant::String) {
                s = v.toString().trimmed();
            } else if (v.type() == QVariant::Map) {
                const QVariantMap m = v.toMap();
                s = m.value("full_path").toString().trimmed();
                if (s.isEmpty()) s = m.value("path").toString().trimmed();
                if (s.isEmpty()) s = m.value("filename").toString().trimmed();
            } else {
                s = v.toString().trimmed();
            }

            s = normalizePath(s);

            if (s.isEmpty())
                continue;

            ts << s << "\n";
            qDebug() << "[WRITE FILE]" << s;
            countWrite++;
        }

        // 3) write summary section
        // ---- section: summary ----
        ts << "#SUMMARY "
           << "totalFiles=" << totalFiles
           << " sampleCount=" << sampleCount
           << " totalMs=" << totalMs
           << " totalDurationSec=" << QString::number(totalDurationSec, 'f', 3)
           << " samplesLength=" << samplesLength
           << " totalSizeKB=" << QString::number(totalSizeKB, 'f', 3)   // ✅ เพิ่ม
           << "\n";

        // 4) flush + close
        ts.flush();
        qDebug() << "[FLUSH] done";

        f.close();
        qDebug() << "[CLOSE FILE] success";
        qDebug() << "[DONE] wroteFiles=" << countWrite << "closeOK";
        qDebug() << "========================================================";
        return true;
    }

    //    Q_INVOKABLE bool saveWaveSelectionState(
    //            const QVariantList &files,
    //            int totalFiles,
    //            int sampleCount,
    //            int totalMs,
    //            double totalDurationSec,
    //            int samplesLength,
    //            const QString &txtPath = "/home/orinnx/saveFileName/filesNameWave.txt")
    //    {
    //        const QString p = normalizePath(txtPath);

    //        qDebug() << "================ saveWaveSelectionState ================";
    //        qDebug() << "[PATH]" << p;
    //        qDebug() << "[INPUT] files.size=" << files.size()
    //                 << "totalFiles=" << totalFiles
    //                 << "sampleCount=" << sampleCount
    //                 << "totalMs=" << totalMs
    //                 << "totalDurationSec=" << totalDurationSec
    //                 << "samplesLength=" << samplesLength;

    //        QFile f(p);
    //        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    //            qCritical() << "[ERROR] open failed:" << p << f.errorString();
    //            qDebug() << "========================================================";
    //            return false;
    //        }

    //        QTextStream ts(&f);
    //        ts.setCodec("UTF-8");

    //        // ---- section: files ----
    //        ts << "#FILES\n";

    //        int countWrite = 0;
    //        for (const QVariant &v : files) {
    //            QString s = v.toString().trimmed();   // ✅ QML ส่งเป็น string list มาเลยดีที่สุด
    //            if (s.isEmpty())
    //                continue;
    //            ts << s << "\n";
    //            qDebug() << "[WRITE FILE]" << s;
    //            countWrite++;
    //        }

    //        // ---- section: summary ----
    //        ts << "#SUMMARY "
    //           << "totalFiles=" << totalFiles
    //           << " sampleCount=" << sampleCount
    //           << " totalMs=" << totalMs
    //           << " totalDurationSec=" << QString::number(totalDurationSec, 'f', 3)
    //           << " samplesLength=" << samplesLength
    //           << "\n";

    //        ts.flush();
    //        f.close();

    //        qDebug() << "[DONE] wroteFiles=" << countWrite << "closeOK";
    //        qDebug() << "========================================================";
    //        return true;
    //    }

    // ===================== LOAD SELECTED WAV FILES + SUMMARY =====================
    // return: { ok:bool, files:[string...], summary:{totalFiles, sampleCount, totalMs, totalDurationSec, samplesLength} }
    Q_INVOKABLE QVariantMap loadWaveSelectionState(
        const QString &txtPath = "/home/orinnx/saveFileName/filesNameWave.txt")
    {
        QVariantMap ret;
        QVariantList outFiles;
        QVariantMap summary;

        ret["ok"] = false;
        ret["files"] = outFiles;
        ret["summary"] = summary;

        const QString p = normalizePath(txtPath);

        qDebug() << "================ loadWaveSelectionState ================";
        qDebug() << "[PATH]" << p;

        QFile f(p);
        if (!f.exists()) {
            qWarning() << "[ERROR] file not exists:" << p;
            qDebug() << "========================================================";
            return ret;
        }
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "[ERROR] open failed:" << p << f.errorString();
            qDebug() << "========================================================";
            return ret;
        }

        QTextStream ts(&f);
        ts.setCodec("UTF-8");

        bool inFiles = false;
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty())
                continue;

            if (line == "#FILES") {
                inFiles = true;
                continue;
            }

            if (line.startsWith("#SUMMARY")) {
                inFiles = false;

                // parse simple key=value
                // example: #SUMMARY totalFiles=4 sampleCount=... totalDurationSec=29.620 samplesLength=...
                const QString rest = line.mid(QString("#SUMMARY").length()).trimmed();
                const QStringList parts = rest.split(' ', Qt::SkipEmptyParts);

                for (const QString &kv : parts) {
                    const int eq = kv.indexOf('=');
                    if (eq <= 0) continue;
                    const QString k = kv.left(eq).trimmed();
                    const QString v = kv.mid(eq+1).trimmed();
                    summary[k] = v;
                }
                continue;
            }

            if (inFiles) {
                outFiles.append(line);
                qDebug() << "[READ FILE]" << (outFiles.size()-1) << line;
            }
        }

        f.close();

        ret["ok"] = (outFiles.size() > 0);
        ret["files"] = outFiles;
        ret["summary"] = summary;

        qDebug() << "[DONE] files=" << outFiles.size() << "summaryKeys=" << summary.keys();
        qDebug() << "========================================================";
        return ret;
    }
    //================================ClearWaveSelectionState===================================
    Q_INVOKABLE bool clearWaveSelectionState(
        const QString &txtPath = "/home/orinnx/saveFileName/filesNameWave.txt")
    {
        const QString p = normalizePath(txtPath);
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "[clearWaveSelectionState] open failed:" << p << f.errorString();
            return false;
        }
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << "#FILES\n";
        ts << "#SUMMARY totalFiles=0 sampleCount=0 totalMs=0 totalDurationSec=0.000 samplesLength=0 totalSizeKB=0.000\n";
        ts.flush();
        f.close();
        qDebug() << "[clearWaveSelectionState] cleared:" << p;
        return true;
    }

    //===========================================================================================

private:
    static QString normalizePath(QString p) {
        if (p.startsWith("file://")) p = p.mid(7);
        return p;
    }

    static int readWavDurationMsImpl(const QString &p) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly) || f.size() < 44) return -1;

        auto rdU8  = [&](qint64 off) -> quint8  {
            f.seek(off);
            char b; if (f.read(&b, 1) != 1) return 0; return (quint8)(uchar)b;
        };
        auto rdU16 = [&](qint64 off) -> quint16 {
            f.seek(off);
            unsigned char b[2]; if (f.read((char*)b, 2) != 2) return 0;
            return (quint16)b[0] | ((quint16)b[1] << 8);
        };
        auto rdU32 = [&](qint64 off) -> quint32 {
            f.seek(off);
            unsigned char b[4]; if (f.read((char*)b, 4) != 4) return 0;
            return  (quint32)b[0]
                   | ((quint32)b[1] << 8)
                   | ((quint32)b[2] << 16)
                   | ((quint32)b[3] << 24);
        };

        if (!(rdU8(0)=='R' && rdU8(1)=='I' && rdU8(2)=='F' && rdU8(3)=='F')) return -1;
        if (!(rdU8(8)=='W' && rdU8(9)=='A' && rdU8(10)=='V' && rdU8(11)=='E')) return -1;

        qint64 off = 12;
        int sampleRate = 0, channels = 0, bits = 0;
        qint64 dataOff = -1; quint32 dataSize = 0;

        while (off + 8 <= f.size()) {
            char id[4];
            f.seek(off);
            if (f.read(id, 4) != 4) break;
            quint32 sz = rdU32(off + 4);
            QByteArray cid(id, 4);
            off += 8;

            if (cid == "fmt ") {
                channels   = rdU16(off + 2);
                sampleRate = (int)rdU32(off + 4);
                bits       = rdU16(off + 14);
            } else if (cid == "data") {
                dataOff  = off;
                dataSize = sz;
                break;
            }
            off += sz;
        }

        if (dataOff < 0 || sampleRate <= 0 || channels <= 0 || bits <= 0) return -1;
        const int bytesPerSample = bits / 8;
        if (bytesPerSample <= 0) return -1;

        const double seconds = double(dataSize) / double(sampleRate * channels * bytesPerSample);
        return (int)(seconds * 1000.0 + 0.5);
    }



};
