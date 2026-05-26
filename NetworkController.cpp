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
#include <QDateTime>

// ============================================================
// Local helpers
// ============================================================
static inline bool isBlank(const QString &s)
{
    return s.trimmed().isEmpty();
}

static inline QString pick(const QString &newVal, const QString &oldVal)
{
    return isBlank(newVal) ? oldVal : newVal.trimmed();
}

static inline QString normalizeDnsForSave(const QString &in)
{
    const QString kDefaultDns = QStringLiteral("0.0.0.0,0.0.0.0");

    QString s = in.trimmed();
    if (s.isEmpty())
        return kDefaultDns;

    s.replace(',', ' ');
    const QStringList parts = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    QStringList out;
    out.reserve(parts.size());

    for (const QString &p : parts) {
        QString d = p.trimmed();
        if (d.isEmpty())
            continue;

        if (d == "0" || d == "0.0.0.0")
            d = "0.0.0.0";

        if (!out.contains(d))
            out << d;
    }

    if (out.isEmpty())
        return kDefaultDns;

    if (out.size() == 1 && out[0] == "0.0.0.0")
        return kDefaultDns;

    return out.join(",");
}

static inline QString normalizeGatewayForSave(const QString &in)
{
    const QString s = in.trimmed();
    return s.isEmpty() ? QStringLiteral("0.0.0.0") : s;
}

static QString ifaceToLanKey(const QString &iface)
{
    if (iface == "enP1p1s0") return "lan2";
    if (iface == "enP8p1s0") return "lan1";
    if (iface == "end0")     return "rfsoc1";
    if (iface == "end1")     return "rfsoc2";
    return iface;
}

static QJsonObject readNetworkConfigRoot()
{
    QFile file("/etc/network_config.json");
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();

    return doc.object();
}

static bool writeNetworkConfigRoot(const QJsonObject &root, QString *outMsg = nullptr)
{
    QFile file("/etc/network_config.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outMsg) *outMsg = QStringLiteral("Failed to write /etc/network_config.json");
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

static bool runProcessBlocking(const QString &program,
                               const QStringList &args,
                               QString *stdOut = nullptr,
                               QString *stdErr = nullptr,
                               int timeoutMs = 30000)
{
    QProcess p;
    p.start(program, args);

    if (!p.waitForStarted(timeoutMs)) {
        if (stdErr) *stdErr = QStringLiteral("%1 waitForStarted failed").arg(program);
        return false;
    }

    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (stdErr) *stdErr = QStringLiteral("%1 timeout").arg(program);
        return false;
    }

    if (stdOut) *stdOut = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (stdErr) *stdErr = QString::fromUtf8(p.readAllStandardError()).trimmed();

    return p.exitCode() == 0;
}

static QVariantMap parseDeviceShow(const QString &iface)
{
    QVariantMap result;

    QString out, err;
    runProcessBlocking("nmcli", {"device", "show", iface}, &out, &err, 10000);

    QRegExp ipRegex("IP4.ADDRESS\\[\\d+\\]:\\s+([\\d.]+)/\\d+");
    QRegExp gwRegex("IP4.GATEWAY:\\s+([\\d.]+)");
    QRegExp dnsRegex("IP4.DNS\\[\\d+\\]:\\s+([\\d.]+)");
    QRegExp stateRegex("GENERAL.STATE:\\s+(.+)");
    QRegExp connRegex("GENERAL.CONNECTION:\\s+(.+)");

    QStringList dnsList;
    for (const QString &line : out.split('\n')) {
        const QString l = line.trimmed();

        if (ipRegex.indexIn(l) != -1)
            result["ip"] = ipRegex.cap(1);
        else if (gwRegex.indexIn(l) != -1)
            result["gateway"] = gwRegex.cap(1);
        else if (dnsRegex.indexIn(l) != -1)
            dnsList << dnsRegex.cap(1);
        else if (stateRegex.indexIn(l) != -1)
            result["state"] = stateRegex.cap(1).trimmed();
        else if (connRegex.indexIn(l) != -1)
            result["connection"] = connRegex.cap(1).trimmed();
    }

    if (!dnsList.isEmpty()) result["dns"] = dnsList.value(0);
    if (dnsList.size() > 1) result["dns2"] = dnsList.value(1);

    result["netmask"] = "255.255.255.0";
    return result;
}

// ============================================================
// NetworkController
// ============================================================
NetworkController::NetworkController(QObject *parent) : QObject(parent) {}

// ============================================================
// LAN apply
// ============================================================
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
            QString out, err;
            const bool ok = runProcessBlocking("nmcli", args, &out, &err, 45000);
            if (!ok) {
                QString m = QStringLiteral("nmcli failed: ");
                if (!err.isEmpty()) m += err;
                else if (!out.isEmpty()) m += out;
                else m += args.join(' ');
                if (outMsg) *outMsg = m;
                return false;
            }
            return true;
        };

        const QString ipNorm  = normalizeIpWithCidr(ipWithCidr, 24);
        const QString gwNorm  = normalizeGatewayForSave(gateway);
        const QString dnsNorm = normalizeDnsForSave(dnsList);

        const QString modeLower = mode.trimmed().toLower();
        const bool isDhcp = (modeLower == "dhcp" ||
                             modeLower == "auto" ||
                             modeLower == "automatic");

        // 1) Save JSON first so UI does not block on nmcli.
        bool jsonOk = true;
        QString jsonMsg;

        QJsonObject rootObj = readNetworkConfigRoot();
        QJsonObject lanObj = rootObj.value("lan").toObject();

        const QString lanKey = ifaceToLanKey(iface);
        QJsonObject oldLan = lanObj.value(lanKey).toObject();

        const QString oldMode = oldLan.value("mode").toString();
        const QString oldIp   = oldLan.value("ip").toString();
        const QString oldGw   = oldLan.value("gateway").toString();
        const QString oldDns  = oldLan.value("dns").toString();

        const QString newMode = isBlank(mode) ? oldMode : (isDhcp ? "dhcp" : "static");
        const QString newIp   = pick(ipNorm, oldIp);
        const QString newGw   = isBlank(gateway) ? normalizeGatewayForSave(oldGw) : gwNorm;
        const QString newDns  = isBlank(dnsList) ? normalizeDnsForSave(oldDns)    : dnsNorm;

        QJsonObject oneLan;
        oneLan["interface"] = iface;
        oneLan["mode"]      = newMode;
        oneLan["ip"]        = newIp;
        oneLan["gateway"]   = newGw;
        oneLan["dns"]       = newDns;

        lanObj[lanKey] = oneLan;
        rootObj["lan"] = lanObj;

        jsonOk = writeNetworkConfigRoot(rootObj, &jsonMsg);

        if (self) {
            QMetaObject::invokeMethod(self, [self, iface, jsonOk, jsonMsg, newGw, newDns]() {
                if (!self) return;
                const QString msg = jsonOk
                                        ? QStringLiteral("Saved /etc/network_config.json (nmcli running in background)")
                                        : (jsonMsg.isEmpty() ? QStringLiteral("Failed to save JSON") : jsonMsg);
                emit self->applyNetworkConfigFinished(iface, jsonOk, msg, newGw, newDns);
            }, Qt::QueuedConnection);
        }

        // 2) Apply nmcli in background.
        bool nmOk = true;
        QString nmMsg;

        if (iface.contains("end")) {
            nmOk = true;
            nmMsg = QStringLiteral("nmcli skipped for end* iface");
        } else {
            bool connectionExists = false;
            {
                QString out, err;
                connectionExists = runProcessBlocking("nmcli",
                                                      {"connection", "show", iface},
                                                      &out, &err, 10000);
            }

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
    QString out, err;
    const bool ok = runProcessBlocking("nmcli", args, &out, &err, 30000);
    qDebug() << "nmcli" << args << "ok:" << ok << "output:" << out << "error:" << err;
}

void NetworkController::saveConfigToJson(const QJsonObject &obj)
{
    QString msg;
    if (!writeNetworkConfigRoot(obj, &msg))
        qWarning() << msg;
}

// ============================================================
// LAN config load
// ============================================================
QVariantMap NetworkController::loadAllLanConfig()
{
    QVariantMap result;
    const QJsonObject root = readNetworkConfigRoot();
    const QJsonObject lanObj = root.value("lan").toObject();

    QVariantMap lanMap;

    for (const QString &lanKey : lanObj.keys()) {
        QJsonObject lan = lanObj.value(lanKey).toObject();

        QVariantMap oneLan;
        oneLan["interface"] = lan.value("interface").toString();
        oneLan["mode"]      = lan.value("mode").toString();
        oneLan["ip"]        = lan.value("ip").toString();
        oneLan["gateway"]   = lan.value("gateway").toString();

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
            dnsCsv = "0.0.0.0,0.0.0.0";
        }

        oneLan["dns"] = dnsCsv;
        lanMap[lanKey] = oneLan;
    }

    result["menuID"] = "network";
    result["lan"] = lanMap;
    return result;
}

QVariantMap NetworkController::loadConfig(const QString &iface)
{
    QVariantMap result;
    const QString lanKey = ifaceToLanKey(iface);

    const QJsonObject root = readNetworkConfigRoot();

    // New schema: { "lan": { "lan1": {...} } }
    const QJsonObject lanObj = root.value("lan").toObject();
    QJsonObject lan = lanObj.value(lanKey).toObject();

    // Fallback: find by interface.
    if (lan.isEmpty()) {
        for (const QString &key : lanObj.keys()) {
            const QJsonObject candidate = lanObj.value(key).toObject();
            if (candidate.value("interface").toString() == iface) {
                lan = candidate;
                break;
            }
        }
    }

    if (!lan.isEmpty()) {
        result["menuID"] = "network";
        result["mode"] = lan.value("mode").toString();
        result["ip"] = lan.value("ip").toString();
        result["gateway"] = lan.value("gateway").toString();

        const QJsonValue dnsVal = lan.value("dns");
        QString dnsText;
        if (dnsVal.isArray()) {
            QStringList tmp;
            for (const QJsonValue &v : dnsVal.toArray())
                tmp << v.toString();
            dnsText = tmp.join(",");
        } else {
            dnsText = dnsVal.toString();
        }

        const QStringList dnsParts = dnsText.split(',', Qt::SkipEmptyParts);
        result["dns"] = dnsParts.value(0).trimmed();
        result["dns2"] = dnsParts.value(1).trimmed();
        return result;
    }

    // Legacy schema fallback.
    if (root.value("interface").toString() != iface)
        return result;

    result["menuID"] = "network";
    result["mode"] = root.value("mode").toString();
    result["ip"] = root.value("ip").toString();
    result["gateway"] = root.value("gateway").toString();
    result["dns"] = root.value("dns").toString();
    result["dns2"] = root.value("dns2").toString();
    return result;
}

QVariantMap NetworkController::queryDhcpInfo(const QString &iface)
{
    return parseDeviceShow(iface);
}

// ============================================================
// WiFi
// ============================================================
QVariantMap NetworkController::loadWifiConfig()
{
    QVariantMap result;
    const QJsonObject root = readNetworkConfigRoot();
    const QJsonObject wifi = root.value("wifi").toObject();

    result["enabled"] = wifi.value("enabled").toBool(true);
    result["interface"] = wifi.value("interface").toString("wlan0");
    result["ssid"] = wifi.value("ssid").toString();
    result["mode"] = wifi.value("mode").toString("dhcp");
    result["autoConnect"] = wifi.value("autoConnect").toBool(true);
    return result;
}

QVariantList NetworkController::scanWifi(const QString &iface)
{
    QVariantList list;

    runProcessBlocking("nmcli", {"radio", "wifi", "on"}, nullptr, nullptr, 10000);
    runProcessBlocking("nmcli", {"device", "wifi", "rescan", "ifname", iface}, nullptr, nullptr, 15000);

    QString out, err;
    bool ok = runProcessBlocking("nmcli",
                                 {"-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi", "list", "ifname", iface},
                                 &out, &err, 15000);

    if (!ok) {
        QVariantMap row;
        row["ssid"] = "";
        row["signal"] = 0;
        row["security"] = "";
        row["active"] = false;
        row["error"] = err.isEmpty() ? QStringLiteral("nmcli wifi scan failed") : err;
        list << row;
        return list;
    }

    QSet<QString> seenSsid;

    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        // nmcli -t escapes ':' as '\:'. This simple parser is enough for common SSID.
        QStringList parts;
        QString cur;
        bool esc = false;

        for (QChar ch : line) {
            if (esc) {
                cur.append(ch);
                esc = false;
            } else if (ch == '\\') {
                esc = true;
            } else if (ch == ':') {
                parts << cur;
                cur.clear();
            } else {
                cur.append(ch);
            }
        }
        parts << cur;

        if (parts.size() < 4)
            continue;

        const QString activeText = parts.value(0).trimmed();
        const QString ssid = parts.value(1).trimmed();
        if (ssid.isEmpty())
            continue;

        // Avoid duplicate SSIDs in UI.
        if (seenSsid.contains(ssid))
            continue;
        seenSsid.insert(ssid);

        QVariantMap row;
        row["active"] = (activeText == "*");
        row["ssid"] = ssid;
        row["signal"] = parts.value(2).toInt();
        row["security"] = parts.value(3).trimmed();
        list << row;
    }

    return list;
}

QVariantMap NetworkController::wifiStatus(const QString &iface)
{
    QVariantMap result = parseDeviceShow(iface);
    result["interface"] = iface;

    QString out, err;
    runProcessBlocking("nmcli",
                       {"-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device", "status"},
                       &out, &err, 10000);

    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        const QStringList p = line.split(':');
        if (p.size() >= 4 && p.value(0) == iface) {
            result["type"] = p.value(1);
            result["deviceState"] = p.value(2);
            result["connection"] = p.value(3);
            result["connected"] = (p.value(2).toLower() == "connected");
            break;
        }
    }

    // Try to get signal of active WiFi.
    const QVariantList wifiList = scanWifi(iface);
    for (const QVariant &v : wifiList) {
        const QVariantMap row = v.toMap();
        if (row.value("active").toBool()) {
            result["ssid"] = row.value("ssid").toString();
            result["signal"] = row.value("signal").toInt();
            result["security"] = row.value("security").toString();
            break;
        }
    }

    return result;
}

void NetworkController::connectWifi(const QString &iface,
                                    const QString &ssid,
                                    const QString &password,
                                    bool autoConnect)
{
    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, iface, ssid, password, autoConnect]() {
        bool ok = false;
        QString out, err;

        const QString trimmedSsid = ssid.trimmed();
        if (trimmedSsid.isEmpty()) {
            err = QStringLiteral("SSID is empty");
        } else {
            runProcessBlocking("nmcli", {"radio", "wifi", "on"}, nullptr, nullptr, 10000);

            QStringList args = {"device", "wifi", "connect", trimmedSsid, "ifname", iface};
            if (!password.isEmpty())
                args << "password" << password;

            ok = runProcessBlocking("nmcli", args, &out, &err, 45000);

            if (ok) {
                // Save safe WiFi config. Do not save password here.
                QJsonObject root = readNetworkConfigRoot();
                QJsonObject wifi;
                wifi["enabled"] = true;
                wifi["interface"] = iface;
                wifi["ssid"] = trimmedSsid;
                wifi["mode"] = "dhcp";
                wifi["autoConnect"] = autoConnect;
                root["wifi"] = wifi;
                QString saveMsg;
                writeNetworkConfigRoot(root, &saveMsg);

                // Make active connection autoconnect setting best effort.
                runProcessBlocking("nmcli",
                                   {"connection", "modify", trimmedSsid,
                                    "connection.autoconnect", autoConnect ? "yes" : "no"},
                                   nullptr, nullptr, 10000);
            }
        }

        const QString msg = ok
                                ? QStringLiteral("WiFi connected: %1").arg(trimmedSsid)
                                : (err.isEmpty() ? QStringLiteral("WiFi connect failed") : err);

        if (self) {
            QMetaObject::invokeMethod(self, [self, ok, msg]() {
                if (!self) return;
                emit self->wifiOperationFinished("connect", ok, msg);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void NetworkController::disconnectWifi(const QString &iface)
{
    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, iface]() {
        QString out, err;
        const bool ok = runProcessBlocking("nmcli",
                                           {"device", "disconnect", iface},
                                           &out, &err, 30000);

        const QString msg = ok
                                ? QStringLiteral("WiFi disconnected")
                                : (err.isEmpty() ? QStringLiteral("WiFi disconnect failed") : err);

        if (self) {
            QMetaObject::invokeMethod(self, [self, ok, msg]() {
                if (!self) return;
                emit self->wifiOperationFinished("disconnect", ok, msg);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

// ============================================================
// 5G / Cellular
// ============================================================
QVariantMap NetworkController::loadCellularConfig()
{
    QVariantMap result;

#if HARDWARE_HAS_5G
    const QJsonObject root = readNetworkConfigRoot();
    const QJsonObject cellular = root.value("cellular").toObject();

    result["enabled"] = cellular.value("enabled").toBool(true);
    result["interface"] = cellular.value("interface").toString("*");
    result["apn"] = cellular.value("apn").toString("internet");
    result["autoConnect"] = cellular.value("autoConnect").toBool(true);
    result["hardwareHas5G"] = true;
#else
    result["enabled"] = false;
    result["interface"] = "";
    result["apn"] = "";
    result["autoConnect"] = false;
    result["hardwareHas5G"] = false;
    result["message"] = "Build is HW_NONE_5G";
#endif

    return result;
}

QVariantList NetworkController::listModems()
{
    QVariantList list;

#if HARDWARE_HAS_5G
    QString out, err;
    const bool ok = runProcessBlocking("mmcli", {"-L"}, &out, &err, 10000);

    if (!ok) {
        QVariantMap row;
        row["index"] = -1;
        row["path"] = "";
        row["name"] = "";
        row["error"] = err.isEmpty() ? QStringLiteral("mmcli -L failed") : err;
        list << row;
        return list;
    }

    QRegularExpression re("/org/freedesktop/ModemManager1/Modem/(\\d+)\\s+\\[(.*?)\\]\\s+(.+)$");

    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        QRegularExpressionMatch m = re.match(line.trimmed());
        if (!m.hasMatch())
            continue;

        QVariantMap row;
        row["index"] = m.captured(1).toInt();
        row["vendor"] = m.captured(2).trimmed();
        row["name"] = m.captured(3).trimmed();
        row["path"] = QStringLiteral("/org/freedesktop/ModemManager1/Modem/%1").arg(row["index"].toInt());
        list << row;
    }
#else
    QVariantMap row;
    row["index"] = -1;
    row["path"] = "";
    row["name"] = "5G disabled by hardware macro";
    row["disabled"] = true;
    list << row;
#endif

    return list;
}

QVariantMap NetworkController::cellularStatus()
{
    QVariantMap result;
    result["hardwareHas5G"] = bool(HARDWARE_HAS_5G);

#if HARDWARE_HAS_5G
    QVariantList modems = listModems();
    if (modems.isEmpty()) {
        result["connected"] = false;
        result["state"] = "no modem";
        result["message"] = "No modem found";
        return result;
    }

    const QVariantMap first = modems.first().toMap();
    const int modemIndex = first.value("index", -1).toInt();
    result["modemIndex"] = modemIndex;
    result["modemName"] = first.value("name").toString();
    result["vendor"] = first.value("vendor").toString();

    if (modemIndex < 0) {
        result["connected"] = false;
        result["state"] = "no modem";
        return result;
    }

    QString out, err;
    runProcessBlocking("mmcli", {"-m", QString::number(modemIndex)}, &out, &err, 10000);
    result["raw"] = out;

    auto capture = [&out](const QString &pattern) -> QString {
        QRegularExpression re(pattern, QRegularExpression::MultilineOption);
        QRegularExpressionMatch m = re.match(out);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };

    result["state"] = capture("\\|\\s+state:\\s+'?([^'\\n]+)'?");
    result["accessTech"] = capture("\\|\\s+access tech:\\s+'?([^'\\n]+)'?");
    result["operator"] = capture("\\|\\s+operator name:\\s+'?([^'\\n]+)'?");
    result["signal"] = capture("\\|\\s+signal quality:\\s+'?([^'\\n]+)'?");

    QString devOut, devErr;
    runProcessBlocking("nmcli",
                       {"-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device", "status"},
                       &devOut, &devErr, 10000);

    bool connected = false;
    QString connection;
    for (const QString &line : devOut.split('\n', Qt::SkipEmptyParts)) {
        const QStringList p = line.split(':');
        if (p.size() >= 4 && (p.value(1) == "gsm" || p.value(1) == "wwan")) {
            if (p.value(2).toLower() == "connected") {
                connected = true;
                connection = p.value(3);
                break;
            }
        }
    }

    result["connected"] = connected;
    result["connection"] = connection;
#else
    result["connected"] = false;
    result["state"] = "disabled";
    result["message"] = "Build is HW_NONE_5G";
#endif

    return result;
}

void NetworkController::connectCellular(const QString &apn,
                                        const QString &iface,
                                        bool autoConnect)
{
    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, apn, iface, autoConnect]() {
        bool ok = false;
        QString msg;

#if HARDWARE_HAS_5G
        const QString conName = "cellular-5g";
        const QString apnValue = apn.trimmed().isEmpty() ? QStringLiteral("internet") : apn.trimmed();
        const QString ifName = iface.trimmed().isEmpty() ? QStringLiteral("*") : iface.trimmed();

        QString out, err;
        const bool exists = runProcessBlocking("nmcli",
                                               {"connection", "show", conName},
                                               &out, &err, 10000);

        if (exists) {
            ok = runProcessBlocking("nmcli",
                                    {"connection", "modify", conName,
                                     "gsm.apn", apnValue,
                                     "connection.autoconnect", autoConnect ? "yes" : "no"},
                                    &out, &err, 15000);
        } else {
            ok = runProcessBlocking("nmcli",
                                    {"connection", "add",
                                     "type", "gsm",
                                     "ifname", ifName,
                                     "con-name", conName,
                                     "apn", apnValue,
                                     "connection.autoconnect", autoConnect ? "yes" : "no"},
                                    &out, &err, 15000);
        }

        if (ok) {
            ok = runProcessBlocking("nmcli",
                                    {"connection", "up", conName},
                                    &out, &err, 60000);
        }

        if (ok) {
            QJsonObject root = readNetworkConfigRoot();
            QJsonObject cellular;
            cellular["enabled"] = true;
            cellular["interface"] = ifName;
            cellular["apn"] = apnValue;
            cellular["autoConnect"] = autoConnect;
            root["cellular"] = cellular;
            writeNetworkConfigRoot(root);
        }

        msg = ok ? QStringLiteral("Cellular connected")
                 : (err.isEmpty() ? QStringLiteral("Cellular connect failed") : err);
#else
        Q_UNUSED(apn)
        Q_UNUSED(iface)
        Q_UNUSED(autoConnect)
        ok = false;
        msg = QStringLiteral("5G is disabled by HW_NONE_5G build macro");
#endif

        if (self) {
            QMetaObject::invokeMethod(self, [self, ok, msg]() {
                if (!self) return;
                emit self->cellularOperationFinished("connect", ok, msg);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void NetworkController::disconnectCellular(const QString &connectionName)
{
    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, connectionName]() {
        bool ok = false;
        QString msg;

#if HARDWARE_HAS_5G
        QString out, err;
        const QString conName = connectionName.trimmed().isEmpty()
                                    ? QStringLiteral("cellular-5g")
                                    : connectionName.trimmed();

        ok = runProcessBlocking("nmcli",
                                {"connection", "down", conName},
                                &out, &err, 30000);

        msg = ok ? QStringLiteral("Cellular disconnected")
                 : (err.isEmpty() ? QStringLiteral("Cellular disconnect failed") : err);
#else
        Q_UNUSED(connectionName)
        ok = false;
        msg = QStringLiteral("5G is disabled by HW_NONE_5G build macro");
#endif

        if (self) {
            QMetaObject::invokeMethod(self, [self, ok, msg]() {
                if (!self) return;
                emit self->cellularOperationFinished("disconnect", ok, msg);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
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
