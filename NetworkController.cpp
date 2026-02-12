#include "NetworkController.h"
#include <QPointer>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QThread>
#include <QRegularExpression>
#include <QTextStream>
#include <QRegExp>

NetworkController::NetworkController(QObject *parent) : QObject(parent) {}

static inline bool isBlank(const QString &s)
{
    return s.trimmed().isEmpty();
}

static inline QString pick(const QString &newVal, const QString &oldVal)
{
    return isBlank(newVal) ? oldVal : newVal.trimmed();
}

// =========================================================
// ✅ Save-syntax normalizers
//   - DNS:     blank -> "0.0.0.0,0.0.0.0" (ตาม requirement)
//   - Gateway: blank -> "0.0.0.0"         (ตาม requirement)
// =========================================================
static inline QString normalizeDnsForSave(const QString &in)
{
    const QString kDefaultDns = QStringLiteral("0.0.0.0,0.0.0.0");

    QString s = in.trimmed();
    if (s.isEmpty())
        return kDefaultDns;

    // allow spaces and commas
    s.replace(',', ' ');
    const QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // normalize common "zero" inputs to 0.0.0.0
    QStringList out;
    out.reserve(parts.size());

    for (const QString &p : parts) {
        QString d = p.trimmed();
        if (d.isEmpty())
            continue;

        // accept "0" / "0,0" style
        if (d == "0" || d == "0.0.0.0")
            d = "0.0.0.0";

        if (!out.contains(d))
            out << d;
    }

    if (out.isEmpty())
        return kDefaultDns;

    // if only one DNS and it's 0.0.0.0 -> expand to two entries
    if (out.size() == 1 && out[0] == "0.0.0.0")
        return kDefaultDns;

    // if user provided one real DNS, keep it as-is; if two+, join with comma
    return out.join(",");
}


static inline QString normalizeGatewayForSave(const QString &in)
{
    const QString s = in.trimmed();
    return s.isEmpty() ? QStringLiteral("0.0.0.0") : s;
}

void NetworkController::applyNetworkConfig(const QString &iface,
                                           const QString &mode,
                                           const QString &ipWithCidr,
                                           const QString &gateway,
                                           const QString &dnsList)
{
    emit applyNetworkConfigStarted(iface);

    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, iface, mode, ipWithCidr, gateway, dnsList]() {

        auto normalizeIpWithCidr = [](const QString &in, int defaultPrefix) -> QString {
            QString s = in.trimmed();
            if (s.isEmpty()) return s;
            if (s.contains('/')) return s;
            return s + "/" + QString::number(defaultPrefix);
        };

        auto runNmcliBlocking = [](const QStringList &args, QString *outMsg) -> bool {
            QProcess p;
            p.start("nmcli", args);
            if (!p.waitForFinished(-1)) {
                if (outMsg) *outMsg = QStringLiteral("nmcli waitForFinished failed");
                return false;
            }
            const int ec = p.exitCode();
            const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();

            if (ec != 0) {
                if (outMsg) {
                    QString m = QStringLiteral("nmcli failed: ");
                    if (!err.isEmpty()) m += err;
                    else if (!out.isEmpty()) m += out;
                    else m += QStringLiteral("exitCode=%1").arg(ec);
                    *outMsg = m;
                }
                return false;
            }
            return true;
        };

        auto saveJsonIndented = [](const QJsonObject &obj, QString *outMsg) -> bool {
            QFile wf("/etc/network_config.json");
            if (!wf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (outMsg) *outMsg = QStringLiteral("Failed to write /etc/network_config.json");
                return false;
            }
            wf.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
            wf.close();
            return true;
        };

        // ---------- normalize inputs ----------
        const QString ipNorm  = normalizeIpWithCidr(ipWithCidr, 24);
        const QString gwNorm  = normalizeGatewayForSave(gateway); // blank -> 0.0.0.0
        const QString dnsNorm = normalizeDnsForSave(dnsList);      // blank -> 0.0.0.0,0.0.0.0

        const QString modeLower = mode.trimmed().toLower();
        const bool isDhcp = (modeLower == "dhcp" ||
                             modeLower == "auto" ||
                             modeLower == "automatic");

        // =========================================================
        // ✅ 1) SAVE JSON FIRST (NO WAIT NMCLI)
        // =========================================================
        bool jsonOk = true;
        QString jsonMsg;

        QJsonObject rootObj;
        QFile rf("/etc/network_config.json");
        if (rf.exists() && rf.open(QIODevice::ReadOnly)) {
            QJsonParseError perr;
            const QJsonDocument oldDoc = QJsonDocument::fromJson(rf.readAll(), &perr);
            rf.close();
            if (perr.error == QJsonParseError::NoError && oldDoc.isObject())
                rootObj = oldDoc.object();
        }

        QJsonObject lanObj = rootObj.value("lan").toObject();

        QString lanKey = "lan1";
        if (iface == "enP1p1s0") lanKey = "lan2";
        else if (iface == "enP8p1s0") lanKey = "lan1";
        else if (iface == "end0") lanKey = "rfsoc1";
        else if (iface == "end1") lanKey = "rfsoc2";

        QJsonObject oldLan = lanObj.value(lanKey).toObject();
        const QString oldMode = oldLan.value("mode").toString();
        const QString oldIp   = oldLan.value("ip").toString();
        const QString oldGw   = oldLan.value("gateway").toString();
        const QString oldDns  = oldLan.value("dns").toString();

        const QString newMode = isBlank(mode) ? oldMode : (isDhcp ? "dhcp" : "static");
        const QString newIp   = pick(ipNorm, oldIp);

        // ✅ requirement: ถ้าว่าง -> ต้อง save default ไม่ใช่ keep old
        const QString newGw   = isBlank(gateway) ? normalizeGatewayForSave(oldGw) : gwNorm;
        const QString newDns  = isBlank(dnsList) ? normalizeDnsForSave(oldDns)    : dnsNorm;

        QJsonObject oneLan;
        oneLan["interface"] = iface;
        oneLan["mode"]      = newMode;
        oneLan["ip"]        = newIp;
        oneLan["gateway"]   = newGw;
        oneLan["dns"]       = newDns; // keep schema string

        lanObj[lanKey] = oneLan;
        rootObj["lan"] = lanObj;

        jsonOk = saveJsonIndented(rootObj, &jsonMsg);

        // ✅ emit "finished" NOW (save done) -> UI ไปต่อได้ทันที
        if (self) {
            QMetaObject::invokeMethod(self, [self, iface, jsonOk, jsonMsg, newGw, newDns]() {
                if (!self) return;
                const QString msg = jsonOk
                                        ? QStringLiteral("Saved /etc/network_config.json (nmcli running in background)")
                                        : (jsonMsg.isEmpty() ? QStringLiteral("Failed to save JSON") : jsonMsg);
                emit self->applyNetworkConfigFinished(iface, jsonOk, msg, newGw, newDns);
            }, Qt::QueuedConnection);
        }

        // =========================================================
        // ✅ 2) THEN APPLY NMCLI (BACKGROUND)
        // =========================================================
        bool nmOk = true;
        QString nmMsg;

        // skip nmcli when iface contains "end" (ตาม behavior เดิมคุณ)
        if (iface.contains("end")) {
            nmOk = true;
            nmMsg = QStringLiteral("nmcli skipped for end* iface");
        } else {
            bool connectionExists = false;
            {
                QProcess check;
                check.start("nmcli", { "connection", "show", iface });
                if (!check.waitForFinished(-1)) {
                    nmOk = false;
                    nmMsg = QStringLiteral("nmcli connection show timed out");
                } else {
                    connectionExists = (check.exitCode() == 0);
                }
            }

            if (nmOk) {
                if (connectionExists) {
                    if (isDhcp) {
                        nmOk = nmOk && runNmcliBlocking({ "connection", "modify", iface,
                                                         "connection.interface-name", iface,
                                                         "ipv4.method", "auto",
                                                         "ipv4.addresses", "",
                                                         "ipv4.gateway", "",
                                                         "ipv4.dns", "" }, &nmMsg);
                    } else {
                        nmOk = nmOk && runNmcliBlocking({ "connection", "modify", iface,
                                                         "connection.interface-name", iface,
                                                         "ipv4.method", "manual",
                                                         "ipv4.addresses", ipNorm,
                                                         "ipv4.gateway", gwNorm,
                                                         "ipv4.dns", dnsNorm }, &nmMsg);
                    }
                } else {
                    if (isDhcp) {
                        nmOk = nmOk && runNmcliBlocking({ "connection", "add", "type", "ethernet",
                                                         "ifname", iface,
                                                         "con-name", iface,
                                                         "connection.interface-name", iface,
                                                         "ipv4.method", "auto" }, &nmMsg);
                    } else {
                        nmOk = nmOk && runNmcliBlocking({ "connection", "add", "type", "ethernet",
                                                         "ifname", iface,
                                                         "con-name", iface,
                                                         "connection.interface-name", iface,
                                                         "ipv4.method", "manual",
                                                         "ipv4.addresses", ipNorm,
                                                         "ipv4.gateway", gwNorm,
                                                         "ipv4.dns", dnsNorm }, &nmMsg);
                    }
                }

                nmOk = nmOk && runNmcliBlocking({ "connection", "up", iface }, &nmMsg);
                if (nmMsg.isEmpty())
                    nmMsg = nmOk ? QStringLiteral("nmcli applied OK") : QStringLiteral("nmcli apply failed");
            }
        }

        // ✅ emit nmcli result later
        if (self) {
            QMetaObject::invokeMethod(self, [self, iface, nmOk, nmMsg]() {
                if (!self) return;
                emit self->applyNetworkConfigNmcliFinished(iface, nmOk, nmMsg);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void NetworkController::runNmcliCommand(const QStringList &args)
{
    QProcess p;
    p.start("nmcli", args);
    p.waitForFinished();
    qDebug() << "nmcli" << args << "output:" << p.readAllStandardOutput();
    qDebug() << "nmcli error:" << p.readAllStandardError();
}

void NetworkController::saveConfigToJson(const QJsonObject &obj)
{
    QFile file("/etc/network_config.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
    } else {
        qWarning() << "Failed to write /etc/network_config.json";
    }
}

QVariantMap NetworkController::loadAllLanConfig()
{
    QVariantMap result;
    QFile file("/etc/network_config.json");

    if (!file.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return result;

    QJsonObject root = doc.object();
    QJsonObject lanObj = root.value("lan").toObject();

    QVariantMap lanMap;

    for (const QString &lanKey : lanObj.keys()) {
        QJsonObject lan = lanObj.value(lanKey).toObject();

        QVariantMap oneLan;
        oneLan["interface"] = lan.value("interface").toString();
        oneLan["mode"]      = lan.value("mode").toString();
        oneLan["ip"]        = lan.value("ip").toString();
        oneLan["gateway"]   = lan.value("gateway").toString();

        // dns: รองรับทั้ง string ("a,b") และ array (["a","b"])
        QString dnsCsv;
        const QJsonValue dnsVal = lan.value("dns");
        if (dnsVal.isString()) {
            dnsCsv = dnsVal.toString();
        } else if (dnsVal.isArray()) {
            QStringList tmp;
            for (const QJsonValue &v : dnsVal.toArray())
                tmp << v.toString().trimmed();
            dnsCsv = tmp.join(",");
        } else {
            dnsCsv = "0,0";
        }

        oneLan["dns"] = dnsCsv;   // ส่งกลับให้ QML/JS เป็น string


        lanMap[lanKey] = oneLan;
    }

    result["menuID"] = "network";
    result["lan"] = lanMap;
    return result;
}

QVariantMap NetworkController::loadConfig(const QString &iface)
{
    QString lanKey;
    if (iface == "enP1p1s0") lanKey = "lan2";
    else if (iface == "enP8p1s0") lanKey = "lan1";
    else if (iface == "end0") lanKey = "rfsoc1";
    else if (iface == "end1") lanKey = "rfsoc2";
    else lanKey = iface;

    QVariantMap result;
    QFile file("/etc/network_config.json");
    if (!file.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return result;

    // NOTE: ฟังก์ชันนี้เดิมในไฟล์ของคุณเป็นแบบ legacy (ไม่ใช้งานโครงสร้าง "lan")
    //       คงไว้ตามเดิมเพื่อไม่ให้กระทบส่วนอื่น
    QJsonObject obj = doc.object();
    if (obj["interface"].toString() != iface)
        return result;

    result["menuID"] = "network";
    result["mode"] = obj["mode"].toString();
    result["ip"] = obj["ip"].toString();
    result["gateway"] = obj["gateway"].toString();
    result["dns"] = obj["dns"].toString();
    result["dns2"] = obj["dns2"].toString();
    return result;
}

QVariantMap NetworkController::queryDhcpInfo(const QString &iface)
{
    QVariantMap result;
    QProcess p;
    p.start("nmcli", { "device", "show", iface });
    p.waitForFinished();
    QString output = p.readAllStandardOutput();

    QRegExp ipRegex("IP4.ADDRESS\\[\\d+\\]:\\s+([\\d.]+)/\\d+");
    QRegExp gwRegex("IP4.GATEWAY:\\s+([\\d.]+)");
    QRegExp dnsRegex("IP4.DNS\\[\\d+\\]:\\s+([\\d.]+)");

    QStringList dnsList;
    for (const QString &line : output.split('\n')) {
        if (ipRegex.indexIn(line) != -1)
            result["ip"] = ipRegex.cap(1);
        else if (gwRegex.indexIn(line) != -1)
            result["gateway"] = gwRegex.cap(1);
        else if (dnsRegex.indexIn(line) != -1)
            dnsList << dnsRegex.cap(1);
    }

    if (!dnsList.isEmpty()) result["dns"] = dnsList.value(0);
    if (dnsList.size() > 1) result["dns2"] = dnsList.value(1);

    result["netmask"] = "255.255.255.0";
    return result;
}

// ------------------------------------------------------
// Helper: run shell command
// ------------------------------------------------------
void NetworkController::runCommand(const QString &cmd) const
{
    QProcess proc;
    proc.start("/bin/bash", { "-c", cmd });
    proc.waitForFinished();

    if (proc.exitCode() != 0) {
        qWarning() << "[NetworkController] Command failed:"
                   << cmd
                   << proc.readAllStandardError();
    }
}

// ------------------------------------------------------
// Reset NTP service
// ------------------------------------------------------
void NetworkController::resetNtp()
{
    runCommand("systemctl daemon-reload");
    runCommand("systemctl restart systemd-timesyncd");

    QThread::msleep(500);

    runCommand("systemctl restart systemd-timesyncd");

    qDebug() << "[NetworkController] systemd-timesyncd restarted";
}

// ------------------------------------------------------
// Set NTP Server
// ------------------------------------------------------
void NetworkController::setNtpServer(const QString &ntpServer)
{
    const QString filename = "/etc/systemd/timesyncd.conf";

    QString data;
    if (ntpServer != "0.0.0.0") {
        data =
            "[Time]\n"
            "NTP=" + ntpServer + "\n"
                          "FallbackNTP=0.debian.pool.ntp.org "
                          "1.debian.pool.ntp.org "
                          "2.debian.pool.ntp.org "
                          "3.debian.pool.ntp.org\n";
    } else {
        data =
            "[Time]\n"
            "#NTP=0.0.0.0\n"
            "#FallbackNTP=0.debian.pool.ntp.org "
            "1.debian.pool.ntp.org "
            "2.debian.pool.ntp.org "
            "3.debian.pool.ntp.org\n";
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[NetworkController] Cannot open" << filename;
        return;
    }

    QTextStream out(&file);
    out << data;
    file.close();

    qDebug() << "[NetworkController] NTP Server set to:" << ntpServer;

    resetNtp();
}

// ------------------------------------------------------
// Get current timezone
// ------------------------------------------------------
QString NetworkController::getTimezone() const
{
    QProcess proc;
    proc.start("/bin/bash", {
                                "-c",
                                "ls -la /etc/localtime | grep '/usr/share/zoneinfo/' | awk '{print $11}'"
                            });
    proc.waitForFinished();

    QString tz = QString(proc.readAllStandardOutput()).trimmed();
    tz.replace("/usr/share/zoneinfo/", "");

    qDebug() << "[NetworkController] Timezone:" << tz;
    return tz;
}

QJsonObject NetworkController::getNtpConfig() const
{
    QJsonObject obj;
    QFile file("/etc/systemd/timesyncd.conf");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        obj["error"] = "cannot_open_timesyncd.conf";
        return obj;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();

        if (line.startsWith("NTP=")) {
            obj["NTP"] = line.mid(4).trimmed();
        } else if (line.startsWith("FallbackNTP=")) {
            obj["FallbackNTP"] = line.mid(QString("FallbackNTP=").length()).trimmed();
        }
    }

    file.close();
    return obj;
}
