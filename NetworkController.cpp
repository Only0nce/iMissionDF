#include "NetworkController.h"
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>        // ✅ ADD THIS

NetworkController::NetworkController(QObject *parent) : QObject(parent) {}

static inline bool isBlank(const QString &s)
{
    return s.trimmed().isEmpty();
}

static inline QString pick(const QString &newVal, const QString &oldVal)
{
    return isBlank(newVal) ? oldVal : newVal.trimmed();
}


void NetworkController::applyNetworkConfig(const QString &iface,
                                           const QString &mode,
                                           const QString &ipWithCidr,
                                           const QString &gateway,
                                           const QString &dnsList)
{
    // =========================================================
    // ✅ Normalize IP: if "a.b.c.d" -> "a.b.c.d/24"
    // =========================================================
    auto normalizeIpWithCidr = [](const QString &in, int defaultPrefix) -> QString {
        QString s = in.trimmed();
        if (s.isEmpty())
            return s;
        if (s.contains('/'))
            return s; // already has prefix
        return s + "/" + QString::number(defaultPrefix);
    };

    // =========================================================
    // ✅ Normalize DNS: allow "," or spaces -> output "a,b,c"
    // =========================================================
    auto normalizeDnsCsv = [](const QString &in) -> QString {
        QString s = in.trimmed();
        if (s.isEmpty())
            return QString();

        // unify separators: comma -> space, then split by whitespace
        s.replace(',', ' ');
        const QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        // trim + dedup (optional but useful)
        QStringList out;
        out.reserve(parts.size());
        for (const QString &p : parts) {
            const QString d = p.trimmed();
            if (!d.isEmpty() && !out.contains(d))
                out << d;
        }

        return out.join(",");
    };

    const QString ipNorm  = normalizeIpWithCidr(ipWithCidr, 24);
    const QString dnsNorm = normalizeDnsCsv(dnsList);

    qDebug() << "Applying network config:"
             << "iface=" << iface
             << "mode=" << mode
             << "ip=" << ipNorm
             << "gateway=" << gateway
             << "dns=" << dnsNorm;

    const QString modeLower = mode.trimmed().toLower();
    const bool isDhcp = (modeLower == "dhcp" ||
                         modeLower == "auto" ||
                         modeLower == "automatic");

    // ---------- Check if connection exists ----------
    QProcess check;
    check.start("nmcli", { "connection", "show", iface });
    check.waitForFinished();
    const bool connectionExists = (check.exitCode() == 0);

    if (iface.contains("end")) {
        qDebug() << "if has end word";
    } else {
        qDebug() << "else doesn't has end word";

        // ---------- Apply nmcli ----------
        if (connectionExists) {
            if (isDhcp) {
                // IMPORTANT: clear old static values to avoid leftovers
                runNmcliCommand({ "connection", "modify", iface,
                                 "connection.interface-name", iface,
                                 "ipv4.method", "auto",
                                 "ipv4.addresses", "",
                                 "ipv4.gateway", "",
                                 "ipv4.dns", "" });
            } else {
                runNmcliCommand({ "connection", "modify", iface,
                                 "connection.interface-name", iface,
                                 "ipv4.method", "manual",
                                 "ipv4.addresses", ipNorm,
                                 "ipv4.gateway", gateway,
                                 "ipv4.dns", dnsNorm }); // ✅ use normalized dns
            }
        } else {
            // New connection
            if (isDhcp) {
                runNmcliCommand({ "connection", "add", "type", "ethernet",
                                 "ifname", iface,
                                 "con-name", iface,
                                 "connection.interface-name", iface,
                                 "ipv4.method", "auto" });
            } else {
                runNmcliCommand({ "connection", "add", "type", "ethernet",
                                 "ifname", iface,
                                 "con-name", iface,
                                 "connection.interface-name", iface,
                                 "ipv4.method", "manual",
                                 "ipv4.addresses", ipNorm,
                                 "ipv4.gateway", gateway,
                                 "ipv4.dns", dnsNorm }); // ✅ use normalized dns
            }
        }

        runNmcliCommand({ "connection", "up", iface });
    }

    // ---------- Load existing JSON (to keep lan1/lan2) ----------
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

    // ---------- Choose lanKey ----------
    QString lanKey = "lan1";
    if (iface == "enP1p1s0") lanKey = "lan2";
    else if (iface == "enP8p1s0") lanKey = "lan1";
    else if (iface == "end0") lanKey = "rfsoc1";
    else if (iface == "end1") lanKey = "rfsoc2";

    // ---------- Get old object ----------
    QJsonObject oldLan = lanObj.value(lanKey).toObject();

    // old values (string)
    const QString oldMode = oldLan.value("mode").toString();
    const QString oldIp   = oldLan.value("ip").toString();
    const QString oldGw   = oldLan.value("gateway").toString();

    // old dns (array -> "a,b")
    QString oldDnsCsv;
    if (oldLan.value("dns").isArray()) {
        const QJsonArray a = oldLan.value("dns").toArray();
        QStringList tmp;
        for (const auto &v : a)
            tmp << v.toString().trimmed();
        oldDnsCsv = tmp.join(",");
    } else {
        oldDnsCsv = oldLan.value("dns").toString();
    }

    // ---------- Merge incoming with old ----------
    const bool reqDhcp = (modeLower == "dhcp" ||
                          modeLower == "auto" ||
                          modeLower == "automatic");

    const QString newMode = isBlank(mode) ? oldMode : (reqDhcp ? "dhcp" : "static");
    const QString newIp   = pick(ipNorm, oldIp);
    const QString newGw   = pick(gateway, oldGw);

    // ✅ ใช้ dnsNorm (ผ่าน normalize แล้ว)
    const QString newDns  = pick(dnsNorm, oldDnsCsv);

    // ---------- Build oneLan (merged) ----------
    QJsonObject oneLan;
    oneLan["interface"] = iface;
    oneLan["mode"]      = newMode;
    oneLan["ip"]        = newIp;
    oneLan["gateway"]   = newGw;

    // dns csv -> array
    QJsonArray dnsArray;
    for (const QString &d : newDns.split(",", Qt::SkipEmptyParts))
        dnsArray.append(d.trimmed());
    oneLan["dns"] = dnsArray;

    // write back
    lanObj[lanKey] = oneLan;
    rootObj["lan"] = lanObj;

    saveConfigToJson(rootObj);
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

        // dns array → QStringList
        QStringList dnsList;
        for (const QJsonValue &v : lan.value("dns").toArray())
            dnsList << v.toString();

        oneLan["dns"] = dnsList;

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

// QVariantMap NetworkController::loadConfig(const QString &iface)
// {
//     // map iface -> lanKey ในไฟล์
//     QString lanKey;
//     if (iface == "enP1p1s0") lanKey = "lan2";
//     else if (iface == "enP8p1s0") lanKey = "lan1";
//     else if (iface == "end0")     lanKey = "rfsoc1";
//     else if (iface == "end1")     lanKey = "rfsoc2";
//     else                          lanKey = iface;

//     QVariantMap result;

//     QFile file("/etc/network_config.json");
//     if (!file.open(QIODevice::ReadOnly)) {
//         return result;
//     }

//     QJsonParseError perr;
//     QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &perr);
//     file.close();

//     if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
//         return result;
//     }

//     QJsonObject root = doc.object();
//     QJsonObject lanObj = root.value("lan").toObject();
//     if (lanObj.isEmpty()) {
//         return result;
//     }

//     QJsonObject lan = lanObj.value(lanKey).toObject();
//     if (lan.isEmpty()) {
//         return result;
//     }

//     // ---- basic fields ----
//     result["menuID"]     = "network";
//     result["lan"]     = lanKey;
//     result["interface"]  = lan.value("interface").toString();
//     result["mode"]       = lan.value("mode").toString();
//     result["ip"]         = lan.value("ip").toString();
//     result["gateway"]    = lan.value("gateway").toString();

//     // ---- dns: array OR legacy string ----
//     QStringList dnsList;

//     QJsonValue dnsVal = lan.value("dns");
//     if (dnsVal.isArray()) {
//         for (const QJsonValue &v : dnsVal.toArray()) {
//             const QString d = v.toString().trimmed();
//             if (!d.isEmpty()) dnsList << d;
//         }
//     } else {
//         // เผื่อไฟล์เก่าเก็บเป็น string "8.8.8.8,8.8.4.4" หรือ "8.8.8.8 8.8.4.4"
//         QString s = dnsVal.toString().trimmed();
//         if (!s.isEmpty()) {
//             s.replace(',', ' ');
//             const QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
//             for (const QString &p : parts) {
//                 const QString d = p.trimmed();
//                 if (!d.isEmpty()) dnsList << d;
//             }
//         }
//     }

//     result["dns"] = dnsList;

//     // convenience for UI
//     result["dns1"] = (dnsList.size() > 0) ? dnsList.at(0) : "";
//     result["dns2"] = (dnsList.size() > 1) ? dnsList.at(1) : "";

//     return result;
// }


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

    // Optional: infer netmask from CIDR (always /24 here for demo)
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

    // รอให้ service stabilize (แทน busy loop)
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
