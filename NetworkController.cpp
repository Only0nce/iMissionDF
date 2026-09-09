#include "NetworkController.h"

#include <QPointer>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QThread>
#include <QRegularExpression>
#include <QTextStream>
#include <QRegExp>
#include <QDateTime>
#include <QTimer>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
    const QStringList parts = s.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);

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

static QString preferredWifiInterface()
{
    // Mirrors /home/only/Documents/remote/api.php.
    // resolveWifiInterface() still falls back to the first real WiFi device.
    return QStringLiteral("wlP9p1s0");
}

static QStringList splitNmcliEscaped(const QString &line, int limit = 0)
{
    QStringList parts;
    QString buffer;
    bool escaped = false;

    for (const QChar ch : line) {
        if (escaped) {
            buffer.append(ch);
            escaped = false;
            continue;
        }

        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }

        if (ch == QLatin1Char(':') && (limit <= 0 || parts.size() < limit - 1)) {
            parts << buffer;
            buffer.clear();
            continue;
        }

        buffer.append(ch);
    }

    if (escaped)
        buffer.append(QLatin1Char('\\'));

    parts << buffer;
    return parts;
}


static QString findNmEthernetConnectionForInterface(const QString &iface)
{
    const QString device = iface.trimmed();
    if (device.isEmpty())
        return QString();

    QString out;
    QString err;

    // Prefer the deterministic profile name when it already exists.
    if (runProcessBlocking(QStringLiteral("nmcli"),
                           {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME"),
                            QStringLiteral("connection"), QStringLiteral("show"), device},
                           &out, &err, 10000)
            && !out.trimmed().isEmpty()) {
        return device;
    }

    // Then reuse the profile currently active on this device. This avoids creating
    // repeated NetworkManager profiles such as iface-<uuid>.nmconnection.
    if (runProcessBlocking(QStringLiteral("nmcli"),
                           {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,DEVICE"),
                            QStringLiteral("connection"), QStringLiteral("show"), QStringLiteral("--active")},
                           &out, &err, 10000)) {
        for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
            const QStringList fields = splitNmcliEscaped(line, 2);
            if (fields.value(1).trimmed() == device)
                return fields.value(0).trimmed();
        }
    }

    // Finally reuse an inactive Ethernet profile already pinned to this interface.
    if (runProcessBlocking(QStringLiteral("nmcli"),
                           {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,connection.interface-name"),
                            QStringLiteral("connection"), QStringLiteral("show")},
                           &out, &err, 10000)) {
        for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
            const QStringList fields = splitNmcliEscaped(line, 2);
            if (fields.value(1).trimmed() == device)
                return fields.value(0).trimmed();
        }
    }

    return QString();
}

static bool commandExists(const QString &command)
{
    QString out, err;
    return runProcessBlocking("bash",
                              {"-lc", QStringLiteral("command -v %1").arg(command)},
                              &out, &err, 5000)
           && !out.trimmed().isEmpty();
}

static QStringList wifiDevices()
{
    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-t", "-f", "DEVICE,TYPE", "device", "status"},
                            &out, &err, 10000)) {
        return {};
    }

    QStringList devices;
    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 2);
        const QString device = parts.value(0).trimmed();
        const QString type = parts.value(1).trimmed();
        if (device.isEmpty())
            continue;

        if (type == QStringLiteral("wifi") || type == QStringLiteral("802-11-wireless"))
            devices << device;
    }
    return devices;
}

static QString resolveWifiInterface(const QString &preferred)
{
    const QString requested = preferred.trimmed().isEmpty()
    ? preferredWifiInterface()
    : preferred.trimmed();
    const QStringList devices = wifiDevices();

    for (const QString &device : devices) {
        if (device == requested)
            return device;
    }

    for (const QString &device : devices) {
        if (device.compare(requested, Qt::CaseInsensitive) == 0)
            return device;
    }

    return devices.isEmpty() ? requested : devices.first();
}

static bool wifiRadioEnabled(QString *message = nullptr)
{
    QString out, err;
    if (!runProcessBlocking("nmcli", {"radio", "wifi"}, &out, &err, 10000)) {
        if (message)
            *message = err.isEmpty() ? out : err;
        return false;
    }

    const QString value = out.trimmed().toLower();
    return value == QStringLiteral("enabled") || value == QStringLiteral("on");
}

static QVariantMap activeWifiConnection(const QString &iface)
{
    QVariantMap result;

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-t", "-f", "GENERAL.CONNECTION,GENERAL.DEVICE",
                             "device", "show", iface},
                            &out, &err, 10000)) {
        return result;
    }

    QString connectionName;
    QString deviceName;
    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 2);
        const QString key = parts.value(0).trimmed();
        const QString value = parts.value(1).trimmed();

        if (key == QStringLiteral("GENERAL.CONNECTION"))
            connectionName = value;
        else if (key == QStringLiteral("GENERAL.DEVICE"))
            deviceName = value;
    }

    if (connectionName.isEmpty() || connectionName == QStringLiteral("--"))
        return result;

    result[QStringLiteral("name")] = connectionName;
    result[QStringLiteral("device")] = deviceName.isEmpty() ? iface : deviceName;
    return result;
}

static QString activeConnectionSsid(const QString &connectionName)
{
    if (connectionName.trimmed().isEmpty())
        return QString();

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-g", "802-11-wireless.ssid",
                             "connection", "show", connectionName},
                            &out, &err, 10000)) {
        return QString();
    }

    return out.split('\n', QString::SkipEmptyParts).value(0).trimmed();
}

static QMap<QString, QString> wifiProfilesBySsid()
{
    QMap<QString, QString> profiles;

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-t", "-f", "NAME,TYPE", "connection", "show"},
                            &out, &err, 10000)) {
        return profiles;
    }

    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 2);
        const QString name = parts.value(0).trimmed();
        const QString type = parts.value(1).trimmed();
        if (name.isEmpty())
            continue;
        if (type != QStringLiteral("802-11-wireless") && type != QStringLiteral("wifi"))
            continue;

        const QString ssid = activeConnectionSsid(name);
        if (!ssid.isEmpty() && !profiles.contains(ssid))
            profiles.insert(ssid, name);
    }

    return profiles;
}

static QString findWifiConnectionNameBySsid(const QString &ssid)
{
    const QString cleanSsid = ssid.trimmed();
    if (cleanSsid.isEmpty())
        return QString();

    return wifiProfilesBySsid().value(cleanSsid);
}

static bool isWifiConnectionProfile(const QString &connectionName,
                                    const QString &expectedSsid = QString())
{
    const QString cleanName = connectionName.trimmed();
    if (cleanName.isEmpty())
        return false;

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-g", "connection.type,802-11-wireless.ssid",
                             "connection", "show", cleanName},
                            &out, &err, 10000)) {
        return false;
    }

    const QStringList values = out.split('\n');
    const QString type = values.value(0).trimmed();
    const QString ssid = values.value(1).trimmed();
    if (type != QStringLiteral("802-11-wireless") && type != QStringLiteral("wifi"))
        return false;

    const QString cleanExpectedSsid = expectedSsid.trimmed();
    return cleanExpectedSsid.isEmpty() || ssid == cleanExpectedSsid;
}

static QString findWifiConnectionNameByProfileOrSsid(const QString &profileName,
                                                     const QString &ssid)
{
    const QString cleanProfileName = profileName.trimmed();
    const QString cleanSsid = ssid.trimmed();

    if (!cleanProfileName.isEmpty()
        && isWifiConnectionProfile(cleanProfileName, cleanSsid)) {
        return cleanProfileName;
    }

    if (!cleanSsid.isEmpty())
        return findWifiConnectionNameBySsid(cleanSsid);

    return QString();
}

static QString bandLabelFromFrequency(const QString &frequency)
{
    const int mhz = frequency.trimmed().toInt();
    if (mhz >= 4900)
        return QStringLiteral("5 GHz");
    if (mhz >= 2400)
        return QStringLiteral("2.4 GHz");
    if (mhz > 0)
        return QStringLiteral("%1 MHz").arg(mhz);
    return QString();
}

static QString wifiRowKey(const QString &ssid,
                          const QString &bssid,
                          const QString &frequency,
                          const QString &channel)
{
    const QString cleanBssid = bssid.trimmed();
    if (!cleanBssid.isEmpty())
        return cleanBssid.toLower();

    return QStringLiteral("%1|%2|%3")
        .arg(ssid.trimmed(), frequency.trimmed(), channel.trimmed())
        .toLower();
}

static QString prefixToMask(int prefix)
{
    if (prefix < 0 || prefix > 32)
        return QString();

    QStringList octets;
    for (int i = 0; i < 4; ++i) {
        int value = 0;
        if (prefix >= 8) {
            value = 255;
            prefix -= 8;
        } else if (prefix > 0) {
            value = 256 - (1 << (8 - prefix));
            prefix = 0;
        }
        octets << QString::number(value);
    }
    return octets.join('.');
}

static int maskToPrefix(const QString &mask)
{
    const QStringList parts = mask.trimmed().split('.');
    if (parts.size() != 4)
        return -1;

    QString bits;
    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        if (!ok || value < 0 || value > 255)
            return -1;
        bits += QString::number(value, 2).rightJustified(8, QLatin1Char('0'));
    }

    if (!QRegularExpression(QStringLiteral("^1*0*$")).match(bits).hasMatch())
        return -1;

    return bits.count(QLatin1Char('1'));
}

static QVariantMap parseConnectionIpv4(const QString &connectionName, QString *error = nullptr)
{
    QVariantMap info;
    info[QStringLiteral("ipv4_method")] = QStringLiteral("auto");
    info[QStringLiteral("ipv4_addresses")] = QString();
    info[QStringLiteral("ipv4_gateway")] = QString();
    info[QStringLiteral("dns")] = QString();
    info[QStringLiteral("dns_auto")] = true;

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-t", "-f",
                             "ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.dns,ipv4.ignore-auto-dns",
                             "connection", "show", connectionName},
                            &out, &err, 10000)) {
        if (error)
            *error = err.isEmpty() ? out : err;
        return info;
    }

    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 2);
        const QString key = parts.value(0).trimmed();
        QString value = parts.value(1).trimmed();

        if (key == QStringLiteral("ipv4.method")) {
            info[QStringLiteral("ipv4_method")] = value.isEmpty() ? QStringLiteral("auto") : value;
        } else if (key == QStringLiteral("ipv4.addresses")) {
            info[QStringLiteral("ipv4_addresses")] = value;
        } else if (key == QStringLiteral("ipv4.gateway")) {
            info[QStringLiteral("ipv4_gateway")] = value;
        } else if (key == QStringLiteral("ipv4.dns")) {
            value.replace(QLatin1Char(';'), QStringLiteral(", "));
            info[QStringLiteral("dns")] = value;
        } else if (key == QStringLiteral("ipv4.ignore-auto-dns")) {
            const QString lower = value.toLower();
            info[QStringLiteral("dns_auto")] =
                !(lower == QStringLiteral("yes")
                  || lower == QStringLiteral("true")
                  || lower == QStringLiteral("1"));
        }
    }

    return info;
}

static QVariantMap parseDeviceIpv4(const QString &iface)
{
    QVariantMap info;
    info[QStringLiteral("dev_ip4_address")] = QString();
    info[QStringLiteral("dev_ip4_gateway")] = QString();
    info[QStringLiteral("dev_ip4_plain")] = QString();
    info[QStringLiteral("dev_ip4_prefix")] = QString();
    info[QStringLiteral("dev_ip4_netmask")] = QString();

    QString out, err;
    if (!runProcessBlocking("nmcli", {"-t", "device", "show", iface}, &out, &err, 10000))
        return info;

    QString ipWithPrefix;
    QString gateway;
    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 2);
        const QString key = parts.value(0).trimmed();
        const QString value = parts.value(1).trimmed();

        if (key.startsWith(QStringLiteral("IP4.ADDRESS")) && ipWithPrefix.isEmpty())
            ipWithPrefix = value;
        else if (key == QStringLiteral("IP4.GATEWAY") && gateway.isEmpty())
            gateway = value;
    }

    info[QStringLiteral("dev_ip4_address")] = ipWithPrefix;
    info[QStringLiteral("dev_ip4_gateway")] = gateway;

    if (!ipWithPrefix.isEmpty()) {
        const QStringList parts = ipWithPrefix.split('/');
        const QString plainIp = parts.value(0).trimmed();
        const QString prefixText = parts.value(1).trimmed();
        info[QStringLiteral("dev_ip4_plain")] = plainIp;
        info[QStringLiteral("dev_ip4_prefix")] = prefixText;
        if (!prefixText.isEmpty())
            info[QStringLiteral("dev_ip4_netmask")] = prefixToMask(prefixText.toInt());
    }

    return info;
}


static QString readTrimmedTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    return QString::fromUtf8(file.readAll()).trimmed();
}

static QString friendlyLanStatus(const QString &state,
                                 const QString &carrier,
                                 const QString &flags)
{
    const QString s = state.trimmed().toLower();
    const QString c = carrier.trimmed().toLower();
    const QString f = flags.trimmed().toLower();

    if (c == QStringLiteral("0") || c == QStringLiteral("off"))
        return QStringLiteral("No cable");

    if (s.contains(QStringLiteral("connected")) ||
        s == QStringLiteral("up") ||
        s == QStringLiteral("1") ||
        c == QStringLiteral("1") ||
        c == QStringLiteral("on") ||
        f.contains(QStringLiteral("lower_up"))) {
        return QStringLiteral("Connected");
    }

    if (s.contains(QStringLiteral("unavailable")))
        return QStringLiteral("No cable");

    if (s.contains(QStringLiteral("disconnected")) ||
        s == QStringLiteral("down") ||
        f.contains(QStringLiteral("no-carrier"))) {
        return QStringLiteral("Disconnected");
    }

    if (!state.trimmed().isEmpty())
        return state.trimmed();

    return QStringLiteral("Unknown");
}

static void mergeSysfsLanInfo(QVariantMap &result, const QString &iface)
{
    const QString base = QStringLiteral("/sys/class/net/%1/").arg(iface);

    const QString mac = readTrimmedTextFile(base + QStringLiteral("address"));
    if (!mac.isEmpty()) {
        result[QStringLiteral("mac")] = mac;
        result[QStringLiteral("macAddress")] = mac;
        result[QStringLiteral("address")] = mac;
    }

    const QString operState = readTrimmedTextFile(base + QStringLiteral("operstate"));
    const QString carrier = readTrimmedTextFile(base + QStringLiteral("carrier"));
    const QString speed = readTrimmedTextFile(base + QStringLiteral("speed"));
    const QString duplex = readTrimmedTextFile(base + QStringLiteral("duplex"));

    if (!operState.isEmpty()) {
        result[QStringLiteral("operstate")] = operState;
        if (!result.contains(QStringLiteral("state")))
            result[QStringLiteral("state")] = operState;
    }

    if (!carrier.isEmpty()) {
        result[QStringLiteral("carrier")] = carrier;
        result[QStringLiteral("carrierOn")] = (carrier == QStringLiteral("1"));
    }

    if (!speed.isEmpty() && speed != QStringLiteral("-1")) {
        const QString speedText = speed.endsWith(QStringLiteral("Mb/s"))
                                  ? speed
                                  : QStringLiteral("%1 Mb/s").arg(speed);
        result[QStringLiteral("speed")] = speedText;
        result[QStringLiteral("linkSpeed")] = speedText;
    }

    if (!duplex.isEmpty())
        result[QStringLiteral("duplex")] = duplex.left(1).toUpper() + duplex.mid(1).toLower();
}

static QVariantMap parseDeviceShow(const QString &iface)
{
    QVariantMap result;
    result[QStringLiteral("iface")] = iface;
    result[QStringLiteral("interface")] = iface;

    QString out, err;
    if (!runProcessBlocking(QStringLiteral("nmcli"),
                            {QStringLiteral("device"), QStringLiteral("show"), iface},
                            &out, &err, 10000)) {
        result[QStringLiteral("error")] = err.isEmpty() ? out : err;
        mergeSysfsLanInfo(result, iface);
        result[QStringLiteral("status")] = friendlyLanStatus(
            result.value(QStringLiteral("state")).toString(),
            result.value(QStringLiteral("carrier")).toString(),
            result.value(QStringLiteral("flags")).toString());
        result[QStringLiteral("link")] = result.value(QStringLiteral("status"));
        result[QStringLiteral("linkStatus")] = result.value(QStringLiteral("status"));
        return result;
    }

    QRegExp ipRegex(QStringLiteral("IP4.ADDRESS\\[\\d+\\]:\\s+([\\d.]+)/(\\d+)"));
    QRegExp gwRegex(QStringLiteral("IP4.GATEWAY:\\s+([\\d.]+)"));
    QRegExp dnsRegex(QStringLiteral("IP4.DNS\\[\\d+\\]:\\s+([\\d.]+)"));
    QRegExp stateRegex(QStringLiteral("GENERAL.STATE:\\s+(.+)"));
    QRegExp connRegex(QStringLiteral("GENERAL.CONNECTION:\\s+(.+)"));
    QRegExp macRegex(QStringLiteral("GENERAL.HWADDR:\\s+(.+)"));
    QRegExp typeRegex(QStringLiteral("GENERAL.TYPE:\\s+(.+)"));
    QRegExp mtuRegex(QStringLiteral("GENERAL.MTU:\\s+(\\d+)"));
    QRegExp speedRegex(QStringLiteral("GENERAL.SPEED:\\s+(.+)"));
    QRegExp carrierRegex(QStringLiteral("WIRED-PROPERTIES.CARRIER:\\s+(.+)"));

    QStringList dnsList;
    for (const QString &line : out.split(QLatin1Char('\n'))) {
        const QString l = line.trimmed();

        if (ipRegex.indexIn(l) != -1) {
            result[QStringLiteral("ip")] = ipRegex.cap(1);
            result[QStringLiteral("dev_ip4_plain")] = ipRegex.cap(1);
            result[QStringLiteral("dev_ip4_prefix")] = ipRegex.cap(2);
            result[QStringLiteral("dev_ip4_address")] = ipRegex.cap(1) + QStringLiteral("/") + ipRegex.cap(2);
            result[QStringLiteral("netmask")] = prefixToMask(ipRegex.cap(2).toInt());
            result[QStringLiteral("dev_ip4_netmask")] = result.value(QStringLiteral("netmask"));
        } else if (gwRegex.indexIn(l) != -1) {
            result[QStringLiteral("gateway")] = gwRegex.cap(1);
            result[QStringLiteral("dev_ip4_gateway")] = gwRegex.cap(1);
        } else if (dnsRegex.indexIn(l) != -1) {
            dnsList << dnsRegex.cap(1);
        } else if (stateRegex.indexIn(l) != -1) {
            result[QStringLiteral("state")] = stateRegex.cap(1).trimmed();
            result[QStringLiteral("rawState")] = stateRegex.cap(1).trimmed();
        } else if (connRegex.indexIn(l) != -1) {
            result[QStringLiteral("connection")] = connRegex.cap(1).trimmed();
        } else if (macRegex.indexIn(l) != -1) {
            const QString mac = macRegex.cap(1).trimmed();
            result[QStringLiteral("mac")] = mac;
            result[QStringLiteral("macAddress")] = mac;
            result[QStringLiteral("address")] = mac;
        } else if (typeRegex.indexIn(l) != -1) {
            result[QStringLiteral("type")] = typeRegex.cap(1).trimmed();
        } else if (mtuRegex.indexIn(l) != -1) {
            result[QStringLiteral("mtu")] = mtuRegex.cap(1).trimmed();
        } else if (speedRegex.indexIn(l) != -1) {
            const QString speed = speedRegex.cap(1).trimmed();
            if (!speed.isEmpty() && speed != QStringLiteral("unknown")) {
                result[QStringLiteral("speed")] = speed;
                result[QStringLiteral("linkSpeed")] = speed;
            }
        } else if (carrierRegex.indexIn(l) != -1) {
            const QString carrier = carrierRegex.cap(1).trimmed();
            result[QStringLiteral("carrier")] = carrier;
            result[QStringLiteral("carrierOn")] = (carrier.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0 ||
                                                     carrier == QStringLiteral("1"));
        }
    }

    if (!dnsList.isEmpty())
        result[QStringLiteral("dns")] = dnsList.value(0);
    if (dnsList.size() > 1)
        result[QStringLiteral("dns2")] = dnsList.value(1);

    /*
     * Preserve legacy queryDhcpInfo() keys while also merging newer parsed
     * device IPv4 fields used elsewhere in this file.
     */
    const QVariantMap dev = parseDeviceIpv4(iface);
    if (!dev.isEmpty()) {
        result.unite(dev);

        const QString plainIp = dev.value(QStringLiteral("dev_ip4_plain")).toString().trimmed();
        const QString gw = dev.value(QStringLiteral("dev_ip4_gateway")).toString().trimmed();
        const QString mask = dev.value(QStringLiteral("dev_ip4_netmask")).toString().trimmed();

        if (!plainIp.isEmpty())
            result[QStringLiteral("ip")] = plainIp;
        if (!gw.isEmpty())
            result[QStringLiteral("gateway")] = gw;
        if (!mask.isEmpty())
            result[QStringLiteral("netmask")] = mask;
    }

    mergeSysfsLanInfo(result, iface);

    if (!result.contains(QStringLiteral("netmask")))
        result[QStringLiteral("netmask")] = QStringLiteral("255.255.255.0");

    const QString status = friendlyLanStatus(result.value(QStringLiteral("state")).toString(),
                                             result.value(QStringLiteral("carrier")).toString(),
                                             result.value(QStringLiteral("flags")).toString());
    result[QStringLiteral("status")] = status;
    result[QStringLiteral("link")] = status;
    result[QStringLiteral("linkStatus")] = status;
    result[QStringLiteral("connected")] = (status == QStringLiteral("Connected"));

    return result;
}

static QVariantMap parseKeyValueLines(const QStringList &lines)
{
    QVariantMap map;
    for (const QString &line : lines) {
        const int idx = line.indexOf(QLatin1Char(':'));
        if (idx < 0)
            continue;
        const QString key = line.left(idx).trimmed();
        const QString value = line.mid(idx + 1).trimmed();
        if (!key.isEmpty())
            map[key] = value;
    }
    return map;
}

static QString pickFirstValue(const QVariantMap &map, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = map.value(key).toString().trimmed();
        if (!value.isEmpty() && value != QStringLiteral("--"))
            return value;
    }
    return QString();
}

static QVariantMap findLteNmDevice()
{
    QVariantMap fallback;

    QString out, err;
    if (!runProcessBlocking("nmcli",
                            {"-t", "-f", "DEVICE,TYPE,STATE,CONNECTION",
                             "device", "status"},
                            &out, &err, 10000)) {
        return fallback;
    }

    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 4);
        const QString type = parts.value(1).trimmed();
        if (type != QStringLiteral("gsm")
            && type != QStringLiteral("cdma")
            && type != QStringLiteral("wwan")
            && type != QStringLiteral("modem")) {
            continue;
        }

        QVariantMap row;
        row[QStringLiteral("device")] = parts.value(0).trimmed();
        row[QStringLiteral("type")] = type;
        row[QStringLiteral("state")] = parts.value(2).trimmed();
        row[QStringLiteral("connection")] = parts.value(3).trimmed();

        const QString state = row.value(QStringLiteral("state")).toString().toLower();
        if (state == QStringLiteral("connected") || state == QStringLiteral("connecting"))
            return row;

        if (fallback.isEmpty())
            fallback = row;
    }

    return fallback;
}

static QString findFirstModemId()
{
    if (!commandExists(QStringLiteral("mmcli")))
        return QString();

    QString out, err;
    if (!runProcessBlocking("mmcli", {"-L"}, &out, &err, 10000))
        return QString();

    const QRegularExpression re(QStringLiteral("/Modem/(\\d+)"));
    const QRegularExpressionMatch match = re.match(out);
    return match.hasMatch() ? match.captured(1) : QString();
}

static QVariantMap parseIfaceSnapshot(const QString &iface)
{
    QVariantMap result;

    QString out, err;
    if (!runProcessBlocking("ifconfig", {iface}, &out, &err, 10000) || out.trimmed().isEmpty())
        return result;

    auto capture = [&out](const QString &pattern) -> QString {
        const QRegularExpression re(pattern,
                                    QRegularExpression::CaseInsensitiveOption
                                        | QRegularExpression::MultilineOption);
        const QRegularExpressionMatch match = re.match(out);
        return match.hasMatch() ? match.captured(1).trimmed() : QString();
    };

    result[QStringLiteral("iface")] = iface;
    result[QStringLiteral("interface")] = iface;
    result[QStringLiteral("flags")] = capture(QStringLiteral("flags=\\d+<([^>]+)>"));
    result[QStringLiteral("mtu")] = capture(QStringLiteral("\\bmtu\\s+(\\d+)"));
    result[QStringLiteral("ipv4")] = capture(QStringLiteral("\\binet\\s+([0-9.]+)"));
    result[QStringLiteral("ipv6")] = capture(QStringLiteral("\\binet6\\s+([0-9a-f:]+)"));
    result[QStringLiteral("address")] =
        capture(QStringLiteral("\\b(?:ether|unspec)\\s+([^\\n]+)"))
            .replace(QRegularExpression(QStringLiteral("\\s+txqueuelen\\s+\\d+.*$")),
                     QString());
    result[QStringLiteral("tx_queue")] = capture(QStringLiteral("\\btxqueuelen\\s+(\\d+)"));
    result[QStringLiteral("txqueuelen")] = result.value(QStringLiteral("tx_queue"));
    result[QStringLiteral("rx_packets")] =
        capture(QStringLiteral("RX packets\\s+(\\d+)\\s+bytes\\s+\\d+"));
    result[QStringLiteral("rx_bytes")] =
        capture(QStringLiteral("RX packets\\s+\\d+\\s+bytes\\s+(\\d+)"));
    result[QStringLiteral("tx_packets")] =
        capture(QStringLiteral("TX packets\\s+(\\d+)\\s+bytes\\s+\\d+"));
    result[QStringLiteral("tx_bytes")] =
        capture(QStringLiteral("TX packets\\s+\\d+\\s+bytes\\s+(\\d+)"));

    QString routeOut, routeErr;
    if (runProcessBlocking("ip",
                           {"-4", "route", "show", "default", "dev", iface},
                           &routeOut, &routeErr, 10000)) {
        const QRegularExpression re(QStringLiteral("\\bvia\\s+([0-9.]+)"));
        const QRegularExpressionMatch match = re.match(routeOut);
        if (match.hasMatch())
            result[QStringLiteral("gateway")] = match.captured(1).trimmed();
    }

    return result;
}

static QVariantMap parseIfaceIpSnapshot(const QString &iface)
{
    QVariantMap result;

    QString linkOut, linkErr;
    if (runProcessBlocking(QStringLiteral("ip"),
                           {QStringLiteral("-o"), QStringLiteral("link"),
                            QStringLiteral("show"), QStringLiteral("dev"), iface},
                           &linkOut, &linkErr, 10000)) {
        const QRegularExpression flagsRe(QStringLiteral("<([^>]+)>"));
        const QRegularExpressionMatch flagsMatch = flagsRe.match(linkOut);
        if (flagsMatch.hasMatch())
            result[QStringLiteral("flags")] = flagsMatch.captured(1).trimmed();

        const QRegularExpression stateRe(QStringLiteral("\\bstate\\s+(\\S+)"),
                                         QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch stateMatch = stateRe.match(linkOut);
        if (stateMatch.hasMatch())
            result[QStringLiteral("link_state")] = stateMatch.captured(1).trimmed();
    }

    QString addrOut, addrErr;
    if (runProcessBlocking(QStringLiteral("ip"),
                           {QStringLiteral("-o"), QStringLiteral("-4"), QStringLiteral("addr"),
                            QStringLiteral("show"), QStringLiteral("dev"), iface},
                           &addrOut, &addrErr, 10000)) {
        const QRegularExpression addrRe(QStringLiteral("\\binet\\s+([0-9.]+)/(\\d+)"));
        const QRegularExpressionMatch match = addrRe.match(addrOut);
        if (match.hasMatch()) {
            result[QStringLiteral("ipv4")] = match.captured(1).trimmed();
            result[QStringLiteral("dev_ip4_plain")] = match.captured(1).trimmed();
            result[QStringLiteral("dev_ip4_prefix")] = match.captured(2).trimmed();
            result[QStringLiteral("dev_ip4_address")] =
                QStringLiteral("%1/%2").arg(match.captured(1).trimmed(),
                                            match.captured(2).trimmed());
            result[QStringLiteral("dev_ip4_netmask")] =
                prefixToMask(match.captured(2).toInt());
        }
    }

    QString routeOut, routeErr;
    if (runProcessBlocking(QStringLiteral("ip"),
                           {QStringLiteral("-4"), QStringLiteral("route"),
                            QStringLiteral("show"), QStringLiteral("dev"), iface},
                           &routeOut, &routeErr, 10000)) {
        QRegularExpression re(QStringLiteral("\\bdefault\\s+via\\s+([0-9.]+)"));
        QRegularExpressionMatch match = re.match(routeOut);
        if (!match.hasMatch()) {
            re.setPattern(QStringLiteral("\\bvia\\s+([0-9.]+)"));
            match = re.match(routeOut);
        }
        if (match.hasMatch())
            result[QStringLiteral("gateway")] = match.captured(1).trimmed();
    }

    if (result.isEmpty())
        return result;

    result[QStringLiteral("iface")] = iface;
    result[QStringLiteral("interface")] = iface;
    return result;
}

static bool isUnsetCellularText(const QString &value)
{
    const QString s = value.trimmed().toLower();
    return s.isEmpty()
           || s == QStringLiteral("-")
           || s == QStringLiteral("--")
           || s == QStringLiteral("unknown")
           || s == QStringLiteral("no data")
           || s == QStringLiteral("(null)")
           || s == QStringLiteral("null");
}

static bool hasUsableIpv4Address(const QString &value)
{
    const QString s = value.trimmed();
    if (isUnsetCellularText(s))
        return false;

    const QString lower = s.toLower();
    if (lower.contains(QStringLiteral("no ipv4"))
        || lower.contains(QStringLiteral("not assigned"))
        || lower == QStringLiteral("0.0.0.0")) {
        return false;
    }

    const QRegularExpression re(QStringLiteral("^\\d{1,3}(?:\\.\\d{1,3}){3}$"));
    return re.match(s).hasMatch();
}

static bool cellularTextSuggestsNoSim(const QString &text)
{
    const QString s = text.toLower();
    return s.contains(QStringLiteral("sim not found"))
           || s.contains(QStringLiteral("no sim"))
           || s.contains(QStringLiteral("sim missing"))
           || s.contains(QStringLiteral("sim-missing"))
           || s.contains(QStringLiteral("sim card not inserted"))
           || s.contains(QStringLiteral("not inserted"))
           || s.contains(QStringLiteral("not present"))
           || s.contains(QStringLiteral("not detected"));
}

static bool cellularTextSuggestsRegistrationTimeout(const QString &text)
{
    const QString s = text.toLower();
    return s.contains(QStringLiteral("requestregistrationstate2 err = 110"))
           || s.contains(QStringLiteral("registration timeout"))
           || s.contains(QStringLiteral("message timeout"));
}

static QString readLastTextFileBytes(const QString &path, qint64 maxBytes = 128 * 1024)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();

    const qint64 size = f.size();
    if (size > maxBytes)
        f.seek(size - maxBytes);

    return QString::fromUtf8(f.readAll());
}


static QStringList newestFirstFromText(const QString &text,
                                       int maxLines,
                                       const QString &prefix = QString())
{
    QStringList parsed;
    const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!prefix.isEmpty())
            trimmed = prefix + trimmed;
        parsed << trimmed;
    }

    QStringList newestFirst;
    newestFirst.reserve(qMin(parsed.size(), maxLines));
    for (int i = parsed.size() - 1; i >= 0 && newestFirst.size() < maxLines; --i)
        newestFirst << parsed.at(i);
    return newestFirst;
}

static bool isCacheFresh(qint64 timestampMs, int ttlMs)
{
    return timestampMs > 0 &&
           (QDateTime::currentMSecsSinceEpoch() - timestampMs) >= 0 &&
           (QDateTime::currentMSecsSinceEpoch() - timestampMs) < ttlMs;
}

static QString normalizeQuectelSimStatus(const QString &raw)
{
    const QString s = raw.trimmed().toUpper();

    if (s.contains(QStringLiteral("SIM_READY")) || s == QStringLiteral("READY"))
        return QStringLiteral("Ready");

    if (s.contains(QStringLiteral("SIM_PIN")))
        return QStringLiteral("PIN required");

    if (s.contains(QStringLiteral("SIM_PUK")))
        return QStringLiteral("PUK required");

    if (s.contains(QStringLiteral("SIM_ABSENT")) ||
        s.contains(QStringLiteral("SIM_NOT_INSERTED")) ||
        s.contains(QStringLiteral("SIM_MISSING")) ||
        s.contains(QStringLiteral("SIM_REMOVED")) ||
        s.contains(QStringLiteral("NO_SIM")) ||
        s.contains(QStringLiteral("NO SIM"))) {
        return QStringLiteral("Not found");
    }

    if (s.contains(QStringLiteral("SIM_NOT_READY")) ||
        s.contains(QStringLiteral("NOT_READY"))) {
        return QStringLiteral("Not ready");
    }

    return QString();
}

static QString normalizeQuectelRegState(const QString &raw)
{
    const QString s = raw.trimmed();
    const QString l = s.toLower();

    if (l.contains(QStringLiteral("attached")))
        return QStringLiteral("Attached");
    if (l.contains(QStringLiteral("registered")))
        return QStringLiteral("Registered");
    if (l.contains(QStringLiteral("search")))
        return QStringLiteral("Searching");
    if (l.contains(QStringLiteral("denied")))
        return QStringLiteral("Denied");
    if (l.contains(QStringLiteral("detach")))
        return QStringLiteral("Detached");

    return s;
}

static QString serviceIsActiveText(const QString &unit)
{
    /*
     * systemctl is relatively expensive on embedded Linux. Cache for a short
     * time so frequent 5G status polling keeps the same output contract without
     * repeatedly forking systemctl.
     */
    static QMutex mutex;
    static QMap<QString, QPair<qint64, QString> > cache;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (cache.contains(unit) && (now - cache.value(unit).first) < 10000)
            return cache.value(unit).second;
    }

    QString out, err;
    QString state = QStringLiteral("inactive");
    if (runProcessBlocking(QStringLiteral("systemctl"),
                           {QStringLiteral("is-active"), unit},
                           &out, &err, 1000)) {
        state = out.trimmed().isEmpty() ? QStringLiteral("active") : out.trimmed();
    } else if (!out.trimmed().isEmpty()) {
        state = out.trimmed();
    }

    {
        QMutexLocker locker(&mutex);
        cache[unit] = qMakePair(now, state);
    }

    return state;
}

static bool pcieQuectelDetected()
{
    /*
     * Avoid lspci on every poll. Reading sysfs vendor/device is much cheaper
     * and gives the same PCIe detection result for the UI.
     */
    static QMutex mutex;
    static qint64 lastMs = 0;
    static bool lastValue = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (lastMs > 0 && (now - lastMs) < 5000)
            return lastValue;
    }

    bool found = false;
    QDir pciDir(QStringLiteral("/sys/bus/pci/devices"));
    const QStringList entries = pciDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QFile vendorFile(pciDir.absoluteFilePath(entry + QStringLiteral("/vendor")));
        if (!vendorFile.open(QIODevice::ReadOnly))
            continue;
        const QString vendor = QString::fromLatin1(vendorFile.readAll()).trimmed().toLower();
        vendorFile.close();
        if (vendor == QStringLiteral("0x1eac")) {
            found = true;
            break;
        }
    }

    {
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastValue = found;
    }

    return found;
}

static bool isQmiCharDevice(const QString &path = QStringLiteral("/dev/mhi_QMI0"))
{
    struct stat st;
    if (::stat(path.toLocal8Bit().constData(), &st) != 0)
        return false;
    return S_ISCHR(st.st_mode);
}

static bool qmiDeviceOpenableNoCreate()
{
    /*
     * Avoid shell redirection here. It used to be expensive and can be unsafe if
     * called with a missing node. This open never creates /dev/mhi_QMI0.
     */
    static QMutex mutex;
    static qint64 lastMs = 0;
    static bool lastValue = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (lastMs > 0 && (now - lastMs) < 3000)
            return lastValue;
    }

    bool ok = false;
    const QByteArray path = QByteArrayLiteral("/dev/mhi_QMI0");
    struct stat st;
    if (::stat(path.constData(), &st) == 0 && S_ISCHR(st.st_mode)) {
        const int fd = ::open(path.constData(), O_RDWR | O_NONBLOCK);
        if (fd >= 0) {
            ok = true;
            ::close(fd);
        }
    }

    {
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastValue = ok;
    }

    return ok;
}

static QVariantMap parseQuectelCmLogStatus(int maxLines = 500)
{
    QVariantMap st;

    QString text = readLastTextFileBytes(QStringLiteral("/tmp/quectel-CM.log"));
    if (text.trimmed().isEmpty())
        return st;

    QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    if (lines.size() > maxLines)
        lines = lines.mid(lines.size() - maxLines);

    /*
     * Read newest -> oldest, because /tmp/quectel-CM.log may contain stale
     * messages from previous reset attempts. The newest matching line wins.
     */
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines.at(i).trimmed();
        const QString lower = line.toLower();

        if (line.isEmpty())
            continue;

        if (!st.contains(QStringLiteral("simStatus"))) {
            QRegularExpression simRe(
                QStringLiteral("SIMStatus\\s*:\\s*([A-Za-z0-9_\\-]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch m = simRe.match(line);

            if (m.hasMatch()) {
                const QString sim = normalizeQuectelSimStatus(m.captured(1));
                if (!sim.isEmpty()) {
                    st[QStringLiteral("simStatus")] = sim;
                    st[QStringLiteral("sim_status")] = sim;
                }
            } else if (cellularTextSuggestsNoSim(line)) {
                st[QStringLiteral("simStatus")] = QStringLiteral("Not found");
                st[QStringLiteral("sim_status")] = QStringLiteral("Not found");
                st[QStringLiteral("lastError")] = QStringLiteral("SIM not found");
            }
        }

        if (!st.contains(QStringLiteral("registration_state")) ||
            !st.contains(QStringLiteral("access_technology")) ||
            !st.contains(QStringLiteral("plmn"))) {
            QRegularExpression regRe(
                QStringLiteral("MCC\\s*:\\s*(\\d+)\\s*,\\s*MNC\\s*:\\s*(\\d+)\\s*,\\s*PS\\s*:\\s*([^,]+)\\s*,\\s*DataCap\\s*:\\s*([^\\]\\r\\n]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch m = regRe.match(line);

            if (m.hasMatch()) {
                const QString mcc = m.captured(1).trimmed();
                const QString mncRaw = m.captured(2).trimmed();
                const QString mnc = mncRaw.rightJustified(2, QLatin1Char('0'));
                const QString ps = normalizeQuectelRegState(m.captured(3));
                const QString rat = m.captured(4).trimmed();
                const QString plmn = mcc + mnc;

                st[QStringLiteral("operator_code")] = plmn;
                st[QStringLiteral("plmn")] = plmn;
                st[QStringLiteral("operator")] = QStringLiteral("PLMN %1").arg(plmn);
                st[QStringLiteral("registration_state")] = ps;
                st[QStringLiteral("accessTech")] = rat;
                st[QStringLiteral("access_technology")] = rat;

                if (ps.toLower().contains(QStringLiteral("attached")) ||
                    ps.toLower().contains(QStringLiteral("registered"))) {
                    st[QStringLiteral("state")] = QStringLiteral("Registered");
                } else if (!ps.isEmpty()) {
                    st[QStringLiteral("state")] = ps;
                }
            }
        }

        if (!st.contains(QStringLiteral("modemName"))) {
            QRegularExpression fwRe(
                QStringLiteral("requestBaseBandVersion\\s+([^\\s]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch m = fwRe.match(line);
            if (m.hasMatch())
                st[QStringLiteral("modemName")] = QStringLiteral("Quectel %1").arg(m.captured(1).trimmed());
        }

        if (!st.contains(QStringLiteral("qmi_mode")) &&
            lower.contains(QStringLiteral("modem works in qmi mode"))) {
            st[QStringLiteral("qmi_mode")] = QStringLiteral("QMI");
        }

        if (!st.contains(QStringLiteral("qmap_netcard"))) {
            QRegularExpression qmapRe(
                QStringLiteral("qmap_netcard\\s*=\\s*([^,\\s]+)"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch m = qmapRe.match(line);
            if (m.hasMatch())
                st[QStringLiteral("qmap_netcard")] = m.captured(1).trimmed();
        }

        if (!st.contains(QStringLiteral("lastError"))) {
            if (cellularTextSuggestsRegistrationTimeout(line)) {
                st[QStringLiteral("lastError")] = QStringLiteral("Registration timeout");
            } else if (lower.contains(QStringLiteral("failed to open /dev/mhi_qmi0"))) {
                st[QStringLiteral("lastError")] = QStringLiteral("QMI device open failed");
            } else if (lower.contains(QStringLiteral("qmidevice_detect failed"))) {
                st[QStringLiteral("lastError")] = QStringLiteral("QMI device not detected");
            } else if (lower.contains(QStringLiteral("atdevice_detect failed"))) {
                st[QStringLiteral("lastError")] = QStringLiteral("AT device not detected");
            }
        }

        if (st.contains(QStringLiteral("simStatus")) &&
            st.contains(QStringLiteral("registration_state")) &&
            st.contains(QStringLiteral("access_technology")) &&
            st.contains(QStringLiteral("modemName")) &&
            st.contains(QStringLiteral("qmap_netcard"))) {
            break;
        }
    }

    return st;
}


static QVariantMap readLteSignalFromCsq()
{
    QVariantMap result;
    result[QStringLiteral("ok")] = false;
    result[QStringLiteral("signal")] = QString();
    result[QStringLiteral("csq")] = QString();
    result[QStringLiteral("dbm")] = QString();
    result[QStringLiteral("raw")] = QString();
    result[QStringLiteral("error")] = QString();

    /*
     * AT+CSQ through /dev/mhi_DUN can block or wake the modem. Cache it and
     * keep the same output keys. Signal is slow-status, not a 1 Hz metric.
     */
    static QMutex mutex;
    static qint64 lastMs = 0;
    static QVariantMap lastResult;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (lastMs > 0 && (now - lastMs) < 10000)
            return lastResult;
    }

    if (!QFile::exists(QStringLiteral("/dev/mhi_DUN"))) {
        result[QStringLiteral("error")] = QStringLiteral("/dev/mhi_DUN not found");
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastResult = result;
        return result;
    }

    // socat lookup itself is also a fork; do it only when DUN exists.
    if (!commandExists(QStringLiteral("socat"))) {
        result[QStringLiteral("error")] = QStringLiteral("socat command not found");
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastResult = result;
        return result;
    }

    const QString shell = QStringLiteral("printf \"AT+CSQ\\r\" | timeout 2 socat - /dev/mhi_DUN,crnl");
    QString out, err;
    if (!runProcessBlocking(QStringLiteral("bash"), {QStringLiteral("-lc"), shell}, &out, &err, 2500)) {
        result[QStringLiteral("raw")] = out;
        result[QStringLiteral("error")] = err.isEmpty() ? out : err;
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastResult = result;
        return result;
    }

    result[QStringLiteral("raw")] = out;
    const QRegularExpression re(QStringLiteral("\\+CSQ:\\s*(\\d+)\\s*,\\s*(\\d+)"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(out);
    if (!match.hasMatch()) {
        result[QStringLiteral("error")] = QStringLiteral("AT+CSQ response parse failed");
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastResult = result;
        return result;
    }

    const int csq = match.captured(1).toInt();
    result[QStringLiteral("csq")] = QString::number(csq);

    if (csq >= 0 && csq <= 31) {
        const int dbm = -113 + (2 * csq);
        result[QStringLiteral("dbm")] = QString::number(dbm);
        result[QStringLiteral("signal")] = QStringLiteral("%1 dBm").arg(dbm);
        result[QStringLiteral("ok")] = true;
    } else if (csq == 99) {
        result[QStringLiteral("signal")] = QStringLiteral("Unknown");
        result[QStringLiteral("error")] = QStringLiteral("CSQ unknown");
    } else {
        result[QStringLiteral("signal")] = QString::number(csq);
        result[QStringLiteral("ok")] = true;
    }

    {
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastResult = result;
    }

    return result;
}

// ============================================================
// NetworkController
// ============================================================
NetworkController::NetworkController(QObject *parent) : QObject(parent)
{
#if HARDWARE_HAS_5G
    m_cellularRealtimeTimer = new QTimer(this);
    m_cellularRealtimeTimer->setSingleShot(false);
    m_cellularRealtimeTimer->setInterval(1500);

    connect(m_cellularRealtimeTimer, &QTimer::timeout,
            this, &NetworkController::pollCellularRealtime);
#endif
}

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
            QString connectionName = findNmEthernetConnectionForInterface(iface);
            const bool connectionExists = !connectionName.isEmpty();
            if (!connectionExists)
                connectionName = iface;

            if (connectionExists) {
                if (isDhcp) {
                    nmOk = nmOk && runNmcliBlocking({ "connection", "modify", connectionName,
                                                     "connection.interface-name", iface,
                                                     "ipv4.method", "auto",
                                                     "ipv4.addresses", "",
                                                     "ipv4.gateway", "",
                                                     "ipv4.dns", "" }, &nmMsg);
                } else {
                    nmOk = nmOk && runNmcliBlocking({ "connection", "modify", connectionName,
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
                                                     "con-name", connectionName,
                                                     "connection.interface-name", iface,
                                                     "connection.autoconnect", "yes",
                                                     "ipv4.method", "auto" }, &nmMsg);
                } else {
                    nmOk = nmOk && runNmcliBlocking({ "connection", "add", "type", "ethernet",
                                                     "ifname", iface,
                                                     "con-name", connectionName,
                                                     "connection.interface-name", iface,
                                                     "connection.autoconnect", "yes",
                                                     "ipv4.method", "manual",
                                                     "ipv4.addresses", ipNorm,
                                                     "ipv4.gateway", gwNorm,
                                                     "ipv4.dns", dnsNorm }, &nmMsg);
                }
            }

            nmOk = nmOk && runNmcliBlocking({ "connection", "up", connectionName }, &nmMsg);
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
static int lanKeyOrder(const QString &key)
{
    if (key == QStringLiteral("lan1"))   return 0;
    if (key == QStringLiteral("lan2"))   return 1;
    if (key == QStringLiteral("rfsoc1")) return 2;
    if (key == QStringLiteral("rfsoc2")) return 3;
    return 100;
}

static QString lanDisplayNameFromKey(const QString &key, int fallbackIndex)
{
    if (key == QStringLiteral("lan1"))   return QStringLiteral("LAN1");
    if (key == QStringLiteral("lan2"))   return QStringLiteral("LAN2");
    if (key == QStringLiteral("rfsoc1")) return QStringLiteral("LAN3");
    if (key == QStringLiteral("rfsoc2")) return QStringLiteral("LAN4");

    if (fallbackIndex >= 0 && fallbackIndex < 100)
        return QStringLiteral("LAN%1").arg(fallbackIndex + 1);

    return key.toUpper();
}

static QStringList dnsListFromJsonValue(const QJsonValue &dnsVal)
{
    QString dnsText;
    if (dnsVal.isArray()) {
        QStringList tmp;
        for (const QJsonValue &v : dnsVal.toArray()) {
            const QString d = v.toString().trimmed();
            if (!d.isEmpty())
                tmp << d;
        }
        dnsText = tmp.join(',');
    } else {
        dnsText = dnsVal.toString().trimmed();
    }

    dnsText.replace(';', ',');
    dnsText.replace(' ', ',');
    QStringList dnsParts;
    for (const QString &part : dnsText.split(',', QString::SkipEmptyParts)) {
        const QString d = part.trimmed();
        if (!d.isEmpty())
            dnsParts << d;
    }
    return dnsParts;
}

static void mergeNonEmpty(QVariantMap &dst, const QVariantMap &src)
{
    for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
        const QString value = it.value().toString().trimmed();
        if (!it.value().isNull() && (!value.isEmpty() || it.value().type() == QVariant::Bool))
            dst[it.key()] = it.value();
    }
}

static QVariantMap lanObjectToMap(const QString &lanKey,
                                  const QJsonObject &lan,
                                  int displayIndex,
                                  bool includeLive)
{
    QVariantMap oneLan;
    const QString iface = lan.value(QStringLiteral("interface")).toString().trimmed();
    const QString configuredIp = lan.value(QStringLiteral("ip")).toString().trimmed();
    const QString configuredGateway = lan.value(QStringLiteral("gateway")).toString().trimmed();
    const QString configuredMode = lan.value(QStringLiteral("mode")).toString(QStringLiteral("dhcp")).trimmed();
    const QStringList dnsParts = dnsListFromJsonValue(lan.value(QStringLiteral("dns")));

    oneLan[QStringLiteral("key")] = lanKey;
    oneLan[QStringLiteral("name")] = lanDisplayNameFromKey(lanKey, displayIndex);
    oneLan[QStringLiteral("label")] = oneLan.value(QStringLiteral("name"));
    oneLan[QStringLiteral("iface")] = iface;
    oneLan[QStringLiteral("interface")] = iface;
    oneLan[QStringLiteral("mode")] = configuredMode.isEmpty() ? QStringLiteral("dhcp") : configuredMode;
    oneLan[QStringLiteral("configuredIp")] = configuredIp;
    oneLan[QStringLiteral("configuredGateway")] = configuredGateway;
    oneLan[QStringLiteral("ip")] = configuredIp;
    oneLan[QStringLiteral("gateway")] = configuredGateway;
    oneLan[QStringLiteral("dns")] = dnsParts.value(0);
    oneLan[QStringLiteral("dns2")] = dnsParts.value(1);
    oneLan[QStringLiteral("dnsList")] = dnsParts.join(',');

    if (includeLive && !iface.isEmpty()) {
        const QVariantMap live = parseDeviceShow(iface);
        oneLan[QStringLiteral("liveIp")] = live.value(QStringLiteral("ip")).toString();
        oneLan[QStringLiteral("liveGateway")] = live.value(QStringLiteral("gateway")).toString();
        oneLan[QStringLiteral("liveNetmask")] = live.value(QStringLiteral("netmask")).toString();
        mergeNonEmpty(oneLan, live);

        // For DHCP, the saved IP is normally blank/placeholder. Prefer the real runtime IP.
        const QString runtimeIp = live.value(QStringLiteral("ip")).toString().trimmed();
        if (!runtimeIp.isEmpty())
            oneLan[QStringLiteral("ip")] = runtimeIp;

        const QString runtimeGateway = live.value(QStringLiteral("gateway")).toString().trimmed();
        if (!runtimeGateway.isEmpty())
            oneLan[QStringLiteral("gateway")] = runtimeGateway;

        const QString runtimeDns = live.value(QStringLiteral("dns")).toString().trimmed();
        const QString runtimeDns2 = live.value(QStringLiteral("dns2")).toString().trimmed();
        if (!runtimeDns.isEmpty())
            oneLan[QStringLiteral("dns")] = runtimeDns;
        if (!runtimeDns2.isEmpty())
            oneLan[QStringLiteral("dns2")] = runtimeDns2;
    }

    return oneLan;
}

static QStringList discoverEthernetInterfaces()
{
    QString out, err;
    QStringList interfaces;

    if (runProcessBlocking(QStringLiteral("nmcli"),
                           {QStringLiteral("-t"), QStringLiteral("-f"),
                            QStringLiteral("DEVICE,TYPE"), QStringLiteral("device"), QStringLiteral("status")},
                           &out, &err, 10000)) {
        for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
            const QStringList parts = splitNmcliEscaped(line, 2);
            const QString iface = parts.value(0).trimmed();
            const QString type = parts.value(1).trimmed();
            if (iface.isEmpty() || iface == QStringLiteral("lo"))
                continue;
            if (type == QStringLiteral("ethernet") || type == QStringLiteral("802-3-ethernet"))
                interfaces << iface;
        }
    }

    if (interfaces.isEmpty()) {
        QDir netDir(QStringLiteral("/sys/class/net"));
        for (const QString &iface : netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (iface == QStringLiteral("lo") ||
                iface.startsWith(QStringLiteral("wl")) ||
                iface.startsWith(QStringLiteral("wlan")) ||
                iface.startsWith(QStringLiteral("rmnet")) ||
                iface.startsWith(QStringLiteral("wwan")) ||
                iface.startsWith(QStringLiteral("docker")) ||
                iface.startsWith(QStringLiteral("br-")) ||
                iface.startsWith(QStringLiteral("veth"))) {
                continue;
            }
            interfaces << iface;
        }
    }

    interfaces.removeDuplicates();
    std::sort(interfaces.begin(), interfaces.end());
    return interfaces;
}

QVariantMap NetworkController::loadAllLanConfig()
{
    QVariantMap result;
    const QJsonObject root = readNetworkConfigRoot();
    const QJsonObject lanObj = root.value(QStringLiteral("lan")).toObject();

    QVariantMap lanMap;
    QVariantList lanList;

    QStringList orderedKeys = lanObj.keys();
    std::sort(orderedKeys.begin(), orderedKeys.end(), [](const QString &a, const QString &b) {
        const int ao = lanKeyOrder(a);
        const int bo = lanKeyOrder(b);
        return ao == bo ? a < b : ao < bo;
    });

    int displayIndex = 0;
    for (const QString &lanKey : orderedKeys) {
        const QJsonObject lan = lanObj.value(lanKey).toObject();
        const QVariantMap oneLan = lanObjectToMap(lanKey, lan, displayIndex, true);
        if (oneLan.value(QStringLiteral("iface")).toString().trimmed().isEmpty())
            continue;

        lanMap[lanKey] = oneLan;
        lanList << oneLan;
        ++displayIndex;
    }

    // Safety fallback: if the JSON has no LAN block, still show real Linux ethernet devices.
    if (lanList.isEmpty()) {
        const QStringList ifaces = discoverEthernetInterfaces();
        for (const QString &iface : ifaces) {
            QJsonObject lan;
            lan[QStringLiteral("interface")] = iface;
            lan[QStringLiteral("mode")] = QStringLiteral("dhcp");
            const QString lanKey = ifaceToLanKey(iface);
            const QVariantMap oneLan = lanObjectToMap(lanKey, lan, displayIndex, true);
            lanMap[lanKey] = oneLan;
            lanList << oneLan;
            ++displayIndex;
        }
    }

    result[QStringLiteral("menuID")] = QStringLiteral("network");
    result[QStringLiteral("lan")] = lanMap;
    result[QStringLiteral("lanList")] = lanList;
    return result;
}

QVariantMap NetworkController::loadConfig(const QString &iface)
{
    QVariantMap result;
    const QString lanKey = ifaceToLanKey(iface);

    const QJsonObject root = readNetworkConfigRoot();

    // New schema: { "lan": { "lan1": {...} } }
    const QJsonObject lanObj = root.value(QStringLiteral("lan")).toObject();
    QJsonObject lan = lanObj.value(lanKey).toObject();
    QString resolvedKey = lanKey;

    // Fallback: find by interface.
    if (lan.isEmpty()) {
        for (const QString &key : lanObj.keys()) {
            const QJsonObject candidate = lanObj.value(key).toObject();
            if (candidate.value(QStringLiteral("interface")).toString() == iface) {
                lan = candidate;
                resolvedKey = key;
                break;
            }
        }
    }

    if (!lan.isEmpty()) {
        result = lanObjectToMap(resolvedKey, lan, lanKeyOrder(resolvedKey), true);
        result[QStringLiteral("menuID")] = QStringLiteral("network");
        return result;
    }

    // Legacy schema fallback.
    if (root.value(QStringLiteral("interface")).toString() == iface) {
        QJsonObject legacyLan;
        legacyLan[QStringLiteral("interface")] = iface;
        legacyLan[QStringLiteral("mode")] = root.value(QStringLiteral("mode")).toString();
        legacyLan[QStringLiteral("ip")] = root.value(QStringLiteral("ip")).toString();
        legacyLan[QStringLiteral("gateway")] = root.value(QStringLiteral("gateway")).toString();
        legacyLan[QStringLiteral("dns")] = root.value(QStringLiteral("dns")).toString();
        result = lanObjectToMap(lanKey, legacyLan, lanKeyOrder(lanKey), true);
        result[QStringLiteral("dns2")] = root.value(QStringLiteral("dns2")).toString();
        result[QStringLiteral("menuID")] = QStringLiteral("network");
        return result;
    }

    // Last fallback: requested iface exists in the OS but has no saved JSON entry.
    const QVariantMap live = parseDeviceShow(iface);
    if (!live.isEmpty()) {
        result = live;
        result[QStringLiteral("menuID")] = QStringLiteral("network");
        result[QStringLiteral("key")] = lanKey;
        result[QStringLiteral("name")] = lanDisplayNameFromKey(lanKey, lanKeyOrder(lanKey));
        result[QStringLiteral("iface")] = iface;
        result[QStringLiteral("interface")] = iface;
        result[QStringLiteral("mode")] = QStringLiteral("dhcp");
    }

    return result;
}

QVariantMap NetworkController::queryDhcpInfo(const QString &iface)
{
    return parseDeviceShow(iface);
}

void NetworkController::requestLoadAllLanConfig()
{
    QPointer<NetworkController> self(this);
    QThread *thread = QThread::create([self]() {
        NetworkController worker;
        const QVariantMap result = worker.loadAllLanConfig();
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, result]() {
            if (self)
                emit self->lanConfigReady(result);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void NetworkController::requestLoadConfig(const QString &iface)
{
    QPointer<NetworkController> self(this);
    QThread *thread = QThread::create([self, iface]() {
        NetworkController worker;
        const QVariantMap result = worker.loadConfig(iface);
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, iface, result]() {
            if (self)
                emit self->lanInterfaceConfigReady(iface, result);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void NetworkController::requestDhcpInfo(const QString &iface)
{
    QPointer<NetworkController> self(this);
    QThread *thread = QThread::create([self, iface]() {
        NetworkController worker;
        const QVariantMap result = worker.queryDhcpInfo(iface);
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, iface, result]() {
            if (self)
                emit self->dhcpInfoReady(iface, result);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
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
    result["interface"] = resolveWifiInterface(wifi.value("interface").toString(preferredWifiInterface()));
    result["preferredInterface"] = preferredWifiInterface();
    result["ssid"] = wifi.value("ssid").toString();
    result["mode"] = wifi.value("mode").toString("dhcp");
    result["autoConnect"] = wifi.value("autoConnect").toBool(true);
    return result;
}

QVariantMap NetworkController::wifiState(const QString &iface)
{
    const QString wifiIface = resolveWifiInterface(iface);
    const bool enabled = wifiRadioEnabled();
    const QVariantMap active = activeWifiConnection(wifiIface);
    const QVariantMap live = active.isEmpty() ? QVariantMap() : parseDeviceIpv4(wifiIface);
    const QString connectionName = active.value(QStringLiteral("name")).toString();
    const QString activeSsid = active.isEmpty() ? QString() : activeConnectionSsid(connectionName);

    QVariantMap result;
    result[QStringLiteral("enabled")] = enabled;
    result[QStringLiteral("device")] = wifiIface;
    result[QStringLiteral("interface")] = wifiIface;
    result[QStringLiteral("active")] = !active.isEmpty();
    result[QStringLiteral("connection_name")] = connectionName;
    result[QStringLiteral("connection")] = connectionName;
    result[QStringLiteral("active_ssid")] = activeSsid;
    result[QStringLiteral("ssid")] = activeSsid;
    result[QStringLiteral("current_ip")] = live.value(QStringLiteral("dev_ip4_plain")).toString();
    result[QStringLiteral("current_gateway")] = live.value(QStringLiteral("dev_ip4_gateway")).toString();
    result[QStringLiteral("current_netmask")] = live.value(QStringLiteral("dev_ip4_netmask")).toString();
    result[QStringLiteral("warning")] = QString();

    // Backward-compatible keys used by the earlier QML.
    result[QStringLiteral("connected")] = !active.isEmpty();
    result[QStringLiteral("ip")] = result.value(QStringLiteral("current_ip"));
    result[QStringLiteral("gateway")] = result.value(QStringLiteral("current_gateway"));
    result[QStringLiteral("netmask")] = result.value(QStringLiteral("current_netmask"));
    return result;
}

QVariantMap NetworkController::scanWifiPage(const QString &iface)
{
    const QString wifiIface = resolveWifiInterface(iface);
    QVariantMap result;
    result[QStringLiteral("enabled")] = wifiRadioEnabled();
    result[QStringLiteral("device")] = wifiIface;
    result[QStringLiteral("interface")] = wifiIface;
    result[QStringLiteral("count")] = 0;
    result[QStringLiteral("rows")] = QVariantList();
    result[QStringLiteral("active_ssid")] = QString();
    result[QStringLiteral("current_ip")] = QString();
    result[QStringLiteral("current_gateway")] = QString();
    result[QStringLiteral("current_netmask")] = QString();

    if (!result.value(QStringLiteral("enabled")).toBool())
        return result;

    const QMap<QString, QString> profiles = wifiProfilesBySsid();
    const QVariantMap active = activeWifiConnection(wifiIface);
    const QString activeName = active.value(QStringLiteral("name")).toString();
    const QString activeSsid = active.isEmpty() ? QString() : activeConnectionSsid(activeName);
    const QVariantMap live = active.isEmpty() ? QVariantMap() : parseDeviceIpv4(wifiIface);

    result[QStringLiteral("active_ssid")] = activeSsid;
    result[QStringLiteral("current_ip")] = live.value(QStringLiteral("dev_ip4_plain")).toString();
    result[QStringLiteral("current_gateway")] = live.value(QStringLiteral("dev_ip4_gateway")).toString();
    result[QStringLiteral("current_netmask")] = live.value(QStringLiteral("dev_ip4_netmask")).toString();

    QString out, err;
    runProcessBlocking("nmcli",
                       {"device", "wifi", "rescan", "ifname", wifiIface},
                       nullptr, nullptr, 15000);
    QThread::msleep(900);

    const bool ok = runProcessBlocking("nmcli",
                                       {"-t", "-f",
                                        "IN-USE,BSSID,SSID,CHAN,FREQ,SIGNAL,SECURITY",
                                        "device", "wifi", "list", "ifname", wifiIface},
                                       &out, &err, 15000);

    if (!ok) {
        result[QStringLiteral("error")] =
            err.isEmpty() ? QStringLiteral("WiFi scan failed") : err;
        return result;
    }

    QVariantList rows;
    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
        const QStringList parts = splitNmcliEscaped(line, 7);
        const QString inUse = parts.value(0).trimmed();
        const QString bssid = parts.value(1).trimmed();
        const QString ssid = parts.value(2).trimmed();
        const QString channel = parts.value(3).trimmed();
        const QString frequency = parts.value(4).trimmed();
        const int signal = qBound(0, parts.value(5).trimmed().toInt(), 100);
        const QString security = parts.value(6).trimmed();
        if (ssid.isEmpty())
            continue;

        QVariantMap row;
        row[QStringLiteral("key")] = wifiRowKey(ssid, bssid, frequency, channel);
        row[QStringLiteral("ssid")] = ssid;
        row[QStringLiteral("bssid")] = bssid;
        row[QStringLiteral("channel")] = channel;
        row[QStringLiteral("frequency")] = frequency;
        row[QStringLiteral("band")] = bandLabelFromFrequency(frequency);
        row[QStringLiteral("signal")] = signal;
        row[QStringLiteral("secure")] = !security.isEmpty() && security != QStringLiteral("--");
        row[QStringLiteral("security")] = security;
        row[QStringLiteral("active")] = (inUse == QStringLiteral("*"));
        row[QStringLiteral("known")] = profiles.contains(ssid);
        row[QStringLiteral("profile_name")] = profiles.value(ssid);
        row[QStringLiteral("device")] = wifiIface;
        rows << row;
    }

    std::sort(rows.begin(), rows.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();

        if (a.value(QStringLiteral("active")).toBool() != b.value(QStringLiteral("active")).toBool())
            return a.value(QStringLiteral("active")).toBool();
        if (a.value(QStringLiteral("known")).toBool() != b.value(QStringLiteral("known")).toBool())
            return a.value(QStringLiteral("known")).toBool();
        if (a.value(QStringLiteral("signal")).toInt() != b.value(QStringLiteral("signal")).toInt())
            return a.value(QStringLiteral("signal")).toInt() > b.value(QStringLiteral("signal")).toInt();

        const int ssidCompare = QString::compare(
            a.value(QStringLiteral("ssid")).toString(),
            b.value(QStringLiteral("ssid")).toString(),
            Qt::CaseInsensitive);
        if (ssidCompare != 0)
            return ssidCompare < 0;

        const int af = a.value(QStringLiteral("frequency")).toString().toInt();
        const int bf = b.value(QStringLiteral("frequency")).toString().toInt();
        if (af != bf)
            return af > bf;

        return QString::compare(
                   a.value(QStringLiteral("bssid")).toString(),
                   b.value(QStringLiteral("bssid")).toString(),
                   Qt::CaseInsensitive) < 0;
    });

    result[QStringLiteral("count")] = rows.size();
    result[QStringLiteral("rows")] = rows;
    return result;
}

QVariantList NetworkController::scanWifi(const QString &iface)
{
    return scanWifiPage(iface).value(QStringLiteral("rows")).toList();
}

QVariantMap NetworkController::wifiStatus(const QString &iface)
{
    QVariantMap result = wifiState(iface);
    const QVariantList wifiList = scanWifi(iface);
    for (const QVariant &rowValue : wifiList) {
        const QVariantMap row = rowValue.toMap();
        if (!row.value(QStringLiteral("active")).toBool())
            continue;

        result[QStringLiteral("ssid")] = row.value(QStringLiteral("ssid")).toString();
        result[QStringLiteral("active_ssid")] = row.value(QStringLiteral("ssid")).toString();
        result[QStringLiteral("signal")] = row.value(QStringLiteral("signal")).toInt();
        result[QStringLiteral("security")] = row.value(QStringLiteral("security")).toString();
        result[QStringLiteral("bssid")] = row.value(QStringLiteral("bssid")).toString();
        result[QStringLiteral("band")] = row.value(QStringLiteral("band")).toString();
        break;
    }
    return result;
}

QVariantMap NetworkController::wifiToggle(bool enabled)
{
    QVariantMap result;
    QString out, err;
    const bool ok = runProcessBlocking("nmcli",
                                       {"radio", "wifi", enabled ? "on" : "off"},
                                       &out, &err, 30000);

    result[QStringLiteral("ok")] = ok;
    result[QStringLiteral("enabled")] = enabled;
    result[QStringLiteral("device")] = resolveWifiInterface(QString());
    result[QStringLiteral("output")] = out;
    result[QStringLiteral("message")] =
        ok ? (enabled ? QStringLiteral("WiFi radio turned on")
                      : QStringLiteral("WiFi radio turned off"))
           : (err.isEmpty() ? out : err);
    return result;
}

QVariantMap NetworkController::forgetWifi(const QString &ssid)
{
    return forgetWifiProfile(QString(), ssid, QString());
}

QVariantMap NetworkController::forgetWifiProfile(const QString &profileName,
                                                 const QString &ssid,
                                                 const QString &bssid)
{
    QVariantMap result;
    Q_UNUSED(bssid)

    const QString cleanProfileName = profileName.trimmed();
    const QString cleanSsid = ssid.trimmed();
    const QString connectionName =
        findWifiConnectionNameByProfileOrSsid(cleanProfileName, cleanSsid);

    result[QStringLiteral("ssid")] = cleanSsid;
    result[QStringLiteral("connection_name")] = connectionName;

    if (cleanSsid.isEmpty() && cleanProfileName.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] = QStringLiteral("Missing WiFi profile");
        return result;
    }

    if (connectionName.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] =
            QStringLiteral("Saved WiFi profile was not found");
        return result;
    }

    QString out, err;
    const bool ok = runProcessBlocking("nmcli",
                                       {"connection", "delete", connectionName},
                                       &out, &err, 30000);

    result[QStringLiteral("ok")] = ok;
    result[QStringLiteral("connection_name")] = connectionName;
    result[QStringLiteral("output")] = out;
    result[QStringLiteral("message")] =
        ok ? QStringLiteral("Removed saved WiFi profile")
           : (err.isEmpty() ? out : err);
    return result;
}

QVariantMap NetworkController::wifiSavedPassword(const QString &profileName,
                                                 const QString &ssid,
                                                 const QString &bssid)
{
    QVariantMap result;
    Q_UNUSED(bssid)

    const QString cleanProfileName = profileName.trimmed();
    const QString cleanSsid = ssid.trimmed();
    const QString connectionName =
        findWifiConnectionNameByProfileOrSsid(cleanProfileName, cleanSsid);

    result[QStringLiteral("ssid")] = cleanSsid;
    result[QStringLiteral("connection_name")] = connectionName;
    result[QStringLiteral("password")] = QString();
    result[QStringLiteral("has_password")] = false;

    if (connectionName.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] =
            QStringLiteral("Saved WiFi profile was not found");
        return result;
    }

    QString out, err;
    const bool ok = runProcessBlocking("nmcli",
                                       {"-s", "-g", "802-11-wireless-security.psk",
                                        "connection", "show", connectionName},
                                       &out, &err, 10000);

    if (!ok) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] =
            err.isEmpty() ? QStringLiteral("Saved WiFi password is not available") : err;
        return result;
    }

    const QString password = out.split('\n', QString::SkipEmptyParts).value(0).trimmed();
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("password")] = password;
    result[QStringLiteral("has_password")] = !password.isEmpty();
    result[QStringLiteral("message")] =
        password.isEmpty()
            ? QStringLiteral("Saved WiFi password is empty or not available")
            : QStringLiteral("Saved WiFi password loaded");
    return result;
}

QVariantMap NetworkController::wifiAdvancedInfo(const QString &ssid, const QString &iface)
{
    return wifiAdvancedInfoForProfile(QString(), ssid, iface);
}

QVariantMap NetworkController::wifiAdvancedInfoForProfile(const QString &profileName,
                                                          const QString &ssid,
                                                          const QString &iface)
{
    const QString wifiIface = resolveWifiInterface(iface);
    const QString cleanProfileName = profileName.trimmed();
    const QString cleanSsid = ssid.trimmed();
    const QVariantMap active = activeWifiConnection(wifiIface);
    const QString activeName = active.value(QStringLiteral("name")).toString();
    const QString activeSsid = active.isEmpty() ? QString() : activeConnectionSsid(activeName);

    QString connectionName;
    if (!cleanProfileName.isEmpty() || !cleanSsid.isEmpty()) {
        connectionName = findWifiConnectionNameByProfileOrSsid(cleanProfileName, cleanSsid);
        if (connectionName.isEmpty() && !activeName.isEmpty() && activeSsid == cleanSsid)
            connectionName = activeName;
    } else if (!activeName.isEmpty()) {
        connectionName = activeName;
    }

    const QString resolvedSsid =
        cleanSsid.isEmpty() && !connectionName.isEmpty()
            ? activeConnectionSsid(connectionName)
            : cleanSsid;

    QVariantMap result;
    result[QStringLiteral("ssid")] = resolvedSsid.isEmpty() ? activeSsid : resolvedSsid;
    result[QStringLiteral("device")] = wifiIface;
    result[QStringLiteral("connection_name")] = connectionName;
    result[QStringLiteral("active")] = !connectionName.isEmpty() && connectionName == activeName;

    if (connectionName.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] =
            QStringLiteral("No saved or active profile was found for this SSID");
        return result;
    }

    QString error;
    result.unite(parseConnectionIpv4(connectionName, &error));
    if (!error.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] = error;
        return result;
    }

    if (result.value(QStringLiteral("active")).toBool())
        result.unite(parseDeviceIpv4(wifiIface));

    result[QStringLiteral("ok")] = true;
    return result;
}

QVariantMap NetworkController::applyWifiIpv4(const QString &ssid,
                                             const QString &method,
                                             const QString &ip,
                                             const QString &mask,
                                             const QString &gateway,
                                             bool dnsAuto,
                                             const QString &dns)
{
    return applyWifiIpv4ForProfile(QString(), ssid, QString(), method, ip, mask,
                                   gateway, dnsAuto, dns);
}

QVariantMap NetworkController::applyWifiIpv4ForProfile(const QString &profileName,
                                                       const QString &ssid,
                                                       const QString &iface,
                                                       const QString &method,
                                                       const QString &ip,
                                                       const QString &mask,
                                                       const QString &gateway,
                                                       bool dnsAuto,
                                                       const QString &dns)
{
    const QString wifiIface = resolveWifiInterface(iface);
    const QVariantMap active = activeWifiConnection(wifiIface);
    const QString activeName = active.value(QStringLiteral("name")).toString();
    const QString activeSsid = active.isEmpty() ? QString() : activeConnectionSsid(activeName);

    const QString cleanProfileName = profileName.trimmed();
    QString cleanSsid = ssid.trimmed();
    QString connectionName;
    if (!cleanProfileName.isEmpty() || !cleanSsid.isEmpty()) {
        connectionName = findWifiConnectionNameByProfileOrSsid(cleanProfileName, cleanSsid);
        if (connectionName.isEmpty() && activeSsid == cleanSsid)
            connectionName = activeName;
    } else if (!activeName.isEmpty()) {
        connectionName = activeName;
        cleanSsid = activeSsid;
    }

    if (cleanSsid.isEmpty() && !connectionName.isEmpty())
        cleanSsid = activeConnectionSsid(connectionName);

    QVariantMap result;
    result[QStringLiteral("ssid")] = cleanSsid;
    result[QStringLiteral("device")] = wifiIface;
    result[QStringLiteral("connection_name")] = connectionName;

    if (connectionName.isEmpty()) {
        result[QStringLiteral("ok")] = false;
        result[QStringLiteral("message")] =
            QStringLiteral("Saved WiFi profile was not found for applying IPv4 settings");
        return result;
    }

    const QString methodValue = method.trimmed().toLower() == QStringLiteral("manual")
                                    ? QStringLiteral("manual")
                                    : QStringLiteral("auto");

    auto runApply = [&result](const QStringList &args) -> bool {
        QString out, err;
        const bool ok = runProcessBlocking("nmcli", args, &out, &err, 30000);
        if (!ok) {
            result[QStringLiteral("ok")] = false;
            result[QStringLiteral("message")] = err.isEmpty() ? out : err;
            return false;
        }
        return true;
    };

    if (methodValue == QStringLiteral("manual")) {
        const QString cleanIp = ip.trimmed();
        const QString cleanMask = mask.trimmed();
        if (cleanIp.isEmpty() || cleanMask.isEmpty()) {
            result[QStringLiteral("ok")] = false;
            result[QStringLiteral("message")] =
                QStringLiteral("Manual IPv4 requires both IP Address and Subnet Mask");
            return result;
        }

        const int prefix = maskToPrefix(cleanMask);
        if (prefix < 0) {
            result[QStringLiteral("ok")] = false;
            result[QStringLiteral("message")] = QStringLiteral("Invalid Subnet Mask format");
            return result;
        }

        if (!runApply({"connection", "modify", connectionName,
                       "ipv4.method", "manual",
                       "ipv4.addresses", QStringLiteral("%1/%2").arg(cleanIp).arg(prefix)})) {
            return result;
        }

        if (!runApply({"connection", "modify", connectionName,
                       "ipv4.gateway", gateway.trimmed()})) {
            return result;
        }
    } else {
        if (!runApply({"connection", "modify", connectionName,
                       "ipv4.method", "auto",
                       "ipv4.addresses", "",
                       "ipv4.gateway", ""})) {
            return result;
        }
    }

    if (dnsAuto || dns.trimmed().isEmpty()) {
        if (!runApply({"connection", "modify", connectionName,
                       "ipv4.dns", "",
                       "ipv4.ignore-auto-dns", "no"})) {
            return result;
        }
    } else {
        if (!runApply({"connection", "modify", connectionName,
                       "ipv4.dns", dns.trimmed(),
                       "ipv4.ignore-auto-dns", "yes"})) {
            return result;
        }
    }

    const bool targetIsActive = (!activeName.isEmpty() && activeName == connectionName);
    bool reapplied = false;
    QString warning;
    if (targetIsActive) {
        QString out, err;
        reapplied = runProcessBlocking("nmcli",
                                       {"connection", "up", connectionName,
                                        "ifname", wifiIface},
                                       &out, &err, 45000);
        if (!reapplied)
            warning = err.isEmpty() ? out : err;
    }

    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("method")] = methodValue;
    result[QStringLiteral("active")] = targetIsActive;
    result[QStringLiteral("reapplied")] = reapplied;
    result[QStringLiteral("warning")] = warning;
    result[QStringLiteral("message")] =
        warning.isEmpty() ? QStringLiteral("WiFi IPv4 settings saved")
                          : QStringLiteral("WiFi IPv4 settings saved with warning");
    return result;
}

void NetworkController::connectWifi(const QString &iface,
                                    const QString &ssid,
                                    const QString &password,
                                    bool autoConnect,
                                    const QString &bssid)
{
    QPointer<NetworkController> self(this);

    QThread *t = QThread::create([self, iface, ssid, password, autoConnect, bssid]() {
        bool ok = false;
        QString out, err;

        const QString wifiIface = resolveWifiInterface(iface);
        const QString trimmedSsid = ssid.trimmed();
        if (trimmedSsid.isEmpty()) {
            err = QStringLiteral("SSID is empty");
        } else {
            runProcessBlocking("nmcli", {"radio", "wifi", "on"}, nullptr, nullptr, 10000);

            QStringList args = {"device", "wifi", "connect", trimmedSsid};
            const QString cleanBssid = bssid.trimmed();
            if (!cleanBssid.isEmpty())
                args << "bssid" << cleanBssid;
            args << "ifname" << wifiIface;
            if (!password.isEmpty())
                args << "password" << password;

            ok = runProcessBlocking("nmcli", args, &out, &err, 45000);

            QString profileName = findWifiConnectionNameBySsid(trimmedSsid);
            if (!ok && !profileName.isEmpty()) {
                QString upOut, upErr;
                ok = runProcessBlocking("nmcli",
                                        {"connection", "up", profileName,
                                         "ifname", wifiIface},
                                        &upOut, &upErr, 45000);
                if (ok) {
                    out = upOut;
                    err.clear();
                } else if (err.isEmpty()) {
                    err = upErr;
                }
            }

            if (ok) {
                if (profileName.isEmpty())
                    profileName = findWifiConnectionNameBySsid(trimmedSsid);

                // Save safe WiFi config. Do not save password here.
                QJsonObject root = readNetworkConfigRoot();
                QJsonObject wifi;
                wifi["enabled"] = true;
                wifi["interface"] = wifiIface;
                wifi["ssid"] = trimmedSsid;
                wifi["mode"] = "dhcp";
                wifi["autoConnect"] = autoConnect;
                root["wifi"] = wifi;
                QString saveMsg;
                writeNetworkConfigRoot(root, &saveMsg);

                // Make active connection autoconnect setting best effort.
                const QString conToModify = profileName.isEmpty() ? trimmedSsid : profileName;
                runProcessBlocking("nmcli",
                                   {"connection", "modify", conToModify,
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
        const QString wifiIface = resolveWifiInterface(iface);
        const bool ok = runProcessBlocking("nmcli",
                                           {"device", "disconnect", wifiIface},
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
    /*
     * mmcli -L is a slow fallback on this product because the real datapath is
     * quectel-CM over PCIe/MHI. Keep the output key compatible but throttle the
     * command heavily and return the cached result when possible.
     */
    static QMutex mutex;
    static qint64 lastMs = 0;
    static QVariantList lastList;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (lastMs > 0 && (now - lastMs) < 30000)
            return lastList;
    }

    QString out, err;
    const bool ok = runProcessBlocking(QStringLiteral("mmcli"), {QStringLiteral("-L")}, &out, &err, 3000);

    if (!ok) {
        QVariantMap row;
        row[QStringLiteral("index")] = -1;
        row[QStringLiteral("path")] = QString();
        row[QStringLiteral("name")] = QStringLiteral("Quectel RM520N-GL");
        row[QStringLiteral("vendor")] = QStringLiteral("Quectel");
        row[QStringLiteral("error")] = err.isEmpty() ? QStringLiteral("mmcli -L unavailable") : err;
        row[QStringLiteral("source")] = QStringLiteral("quectel-CM");
        list << row;
    } else {
        QRegularExpression re(QStringLiteral("/org/freedesktop/ModemManager1/Modem/(\\d+)\\s+\\[(.*?)\\]\\s+(.+)$"));

        for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
            QRegularExpressionMatch m = re.match(line.trimmed());
            if (!m.hasMatch())
                continue;

            QVariantMap row;
            row[QStringLiteral("index")] = m.captured(1).toInt();
            row[QStringLiteral("vendor")] = m.captured(2).trimmed();
            row[QStringLiteral("name")] = m.captured(3).trimmed();
            row[QStringLiteral("path")] = QStringLiteral("/org/freedesktop/ModemManager1/Modem/%1").arg(row[QStringLiteral("index")].toInt());
            list << row;
        }

        if (list.isEmpty()) {
            QVariantMap row;
            row[QStringLiteral("index")] = -1;
            row[QStringLiteral("path")] = QString();
            row[QStringLiteral("name")] = QStringLiteral("Quectel RM520N-GL");
            row[QStringLiteral("vendor")] = QStringLiteral("Quectel");
            row[QStringLiteral("source")] = QStringLiteral("quectel-CM");
            list << row;
        }
    }

    {
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastList = list;
    }
#else
    QVariantMap row;
    row[QStringLiteral("index")] = -1;
    row[QStringLiteral("path")] = QString();
    row[QStringLiteral("name")] = QStringLiteral("5G disabled by hardware macro");
    row[QStringLiteral("disabled")] = true;
    list << row;
#endif

    return list;
}

QVariantMap NetworkController::cellularStatus()
{
    QVariantMap result;
    result[QStringLiteral("hardwareHas5G")] = bool(HARDWARE_HAS_5G);

#if HARDWARE_HAS_5G
    const QString primaryIface = QStringLiteral("rmnet_mhi0.1");
    const QString fallbackIface = QStringLiteral("rmnet_mhi0");

    result[QStringLiteral("connected")] = false;
    result[QStringLiteral("modemName")] = QStringLiteral("Quectel RM520N-GL");
    result[QStringLiteral("interface")] = primaryIface;
    result[QStringLiteral("device")] = primaryIface;
    result[QStringLiteral("operator")] = QStringLiteral("-");
    result[QStringLiteral("operator_code")] = QStringLiteral("-");
    result[QStringLiteral("plmn")] = QStringLiteral("-");
    result[QStringLiteral("state")] = QStringLiteral("Unknown");
    result[QStringLiteral("registration_state")] = QStringLiteral("Unknown");
    result[QStringLiteral("accessTech")] = QStringLiteral("-");
    result[QStringLiteral("access_technology")] = QStringLiteral("-");
    result[QStringLiteral("signal")] = QStringLiteral("--");
    result[QStringLiteral("imei")] = QStringLiteral("-");
    result[QStringLiteral("simStatus")] = QStringLiteral("Unknown");
    result[QStringLiteral("sim_status")] = QStringLiteral("Unknown");
    result[QStringLiteral("simIccid")] = QStringLiteral("-");
    result[QStringLiteral("iccid")] = QStringLiteral("-");
    result[QStringLiteral("dataState")] = QStringLiteral("Disconnected");
    result[QStringLiteral("data_state")] = QStringLiteral("Disconnected");
    result[QStringLiteral("ipAddress")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("ip_address")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("gateway")] = QStringLiteral("--");
    result[QStringLiteral("lastError")] = QString();
    result[QStringLiteral("source")] = QStringLiteral("quectel-CM + pcie_mhi + rmnet_mhi0.1");

    const QString qcmServiceState = serviceIsActiveText(QStringLiteral("quectel-cm.service"));
    const QString recoverServiceState = serviceIsActiveText(QStringLiteral("5g-pcie-recover.service"));
    const bool qcmActive = (qcmServiceState == QStringLiteral("active"));
    const bool recoverActive = (recoverServiceState == QStringLiteral("active"));
    const bool pcieDetected = pcieQuectelDetected();
    const bool qmiOpenable = qmiDeviceOpenableNoCreate();

    result[QStringLiteral("qcmServiceState")] = qcmServiceState;
    result[QStringLiteral("recoverServiceState")] = recoverServiceState;
    result[QStringLiteral("qcmServiceActive")] = qcmActive;
    result[QStringLiteral("recoverServiceActive")] = recoverActive;
    result[QStringLiteral("pcieDetected")] = pcieDetected;
    result[QStringLiteral("qmiDeviceReady")] = qmiOpenable;

    QVariantMap snapshot = parseIfaceIpSnapshot(primaryIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceSnapshot(primaryIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceIpSnapshot(fallbackIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceSnapshot(fallbackIface);

    if (!snapshot.isEmpty()) {
        const QString iface = snapshot.value(QStringLiteral("iface")).toString();

        result.unite(snapshot);

        result[QStringLiteral("device")] = iface;
        result[QStringLiteral("interface")] = iface;

        const QString ipv4 = snapshot.value(QStringLiteral("ipv4")).toString();
        if (hasUsableIpv4Address(ipv4)) {
            result[QStringLiteral("ipAddress")] = ipv4;
            result[QStringLiteral("ip_address")] = ipv4;
        }

        const QString gateway = snapshot.value(QStringLiteral("gateway")).toString();
        if (!isUnsetCellularText(gateway))
            result[QStringLiteral("gateway")] = gateway;
    }

    const bool rmnetReady = QFile::exists(QStringLiteral("/sys/class/net/%1").arg(primaryIface));

    result[QStringLiteral("rmnetReady")] = rmnetReady;

    const QVariantMap qcmLogStatus = parseQuectelCmLogStatus(500);

    auto setFromQcm = [&](const QString &key) {
        const QString v = qcmLogStatus.value(key).toString().trimmed();
        if (!v.isEmpty())
            result[key] = v;
    };

    setFromQcm(QStringLiteral("simStatus"));
    setFromQcm(QStringLiteral("sim_status"));
    setFromQcm(QStringLiteral("registration_state"));
    setFromQcm(QStringLiteral("state"));
    setFromQcm(QStringLiteral("operator"));
    setFromQcm(QStringLiteral("operator_code"));
    setFromQcm(QStringLiteral("plmn"));
    setFromQcm(QStringLiteral("accessTech"));
    setFromQcm(QStringLiteral("access_technology"));
    setFromQcm(QStringLiteral("modemName"));
    setFromQcm(QStringLiteral("lastError"));
    setFromQcm(QStringLiteral("qmi_mode"));
    setFromQcm(QStringLiteral("qmap_netcard"));

    const QVariantMap csqSignal = readLteSignalFromCsq();
    if (csqSignal.value(QStringLiteral("ok")).toBool()) {
        result[QStringLiteral("signal")] = csqSignal.value(QStringLiteral("signal")).toString();
        result[QStringLiteral("csq")] = csqSignal.value(QStringLiteral("csq")).toString();
        result[QStringLiteral("dbm")] = csqSignal.value(QStringLiteral("dbm")).toString();
    }

    /*
     * mmcli is only a fallback here. This product uses quectel-CM on PCIe/MHI,
     * so stale ModemManager values must not override fresh quectel-CM log data.
     */
    if (qcmLogStatus.isEmpty() && commandExists(QStringLiteral("mmcli"))) {
        const QString modemId = findFirstModemId();
        if (!modemId.isEmpty()) {
            QString out, err;
            if (runProcessBlocking(QStringLiteral("mmcli"),
                                   {QStringLiteral("-m"), modemId, QStringLiteral("-K")},
                                   &out, &err, 10000)) {
                const QVariantMap modem =
                    parseKeyValueLines(out.split('\n', QString::SkipEmptyParts));

                const QString simPath = pickFirstValue(modem, {
                                                                  QStringLiteral("modem.generic.sim"),
                                                                  QStringLiteral("modem.3gpp.sim")
                                                              });
                const QString modemState = pickFirstValue(modem, {
                                                                     QStringLiteral("modem.generic.state"),
                                                                     QStringLiteral("modem.state")
                                                                 });
                const QString registrationState = pickFirstValue(modem, {
                                                                            QStringLiteral("modem.3gpp.registration-state"),
                                                                            QStringLiteral("modem.generic.state")
                                                                        });
                const QString operatorName = pickFirstValue(modem, {
                                                                       QStringLiteral("modem.3gpp.operator-name"),
                                                                       QStringLiteral("modem.3gpp.operator-code")
                                                                   });
                const QString operatorCode = pickFirstValue(modem, {
                                                                       QStringLiteral("modem.3gpp.operator-code"),
                                                                       QStringLiteral("modem.3gpp.plmn")
                                                                   });
                const QString imei = pickFirstValue(modem, {
                                                               QStringLiteral("modem.3gpp.imei"),
                                                               QStringLiteral("modem.generic.equipment-identifier")
                                                           });

                if (!simPath.isEmpty()) {
                    result[QStringLiteral("simStatus")] = QStringLiteral("Ready");
                    result[QStringLiteral("sim_status")] = QStringLiteral("Ready");
                }
                if (!modemState.isEmpty())
                    result[QStringLiteral("state")] = modemState;
                if (!registrationState.isEmpty())
                    result[QStringLiteral("registration_state")] = registrationState;
                if (!operatorName.isEmpty())
                    result[QStringLiteral("operator")] = operatorName;
                if (!operatorCode.isEmpty()) {
                    result[QStringLiteral("operator_code")] = operatorCode;
                    result[QStringLiteral("plmn")] = operatorCode;
                }
                if (!imei.isEmpty())
                    result[QStringLiteral("imei")] = imei;
            }
        }
    }

    QString ipAddress = result.value(QStringLiteral("ipAddress")).toString().trimmed();
    if (!hasUsableIpv4Address(ipAddress))
        ipAddress = result.value(QStringLiteral("ip_address")).toString().trimmed();

    const bool hasIp = hasUsableIpv4Address(ipAddress);
    if (!hasIp)
        ipAddress = QStringLiteral("No IPv4 assigned");

    QString simStatus = result.value(QStringLiteral("simStatus")).toString().trimmed();
    if (isUnsetCellularText(simStatus))
        simStatus = result.value(QStringLiteral("sim_status")).toString().trimmed();
    if (isUnsetCellularText(simStatus))
        simStatus = QStringLiteral("Unknown");

    QString registration = result.value(QStringLiteral("registration_state")).toString().trimmed();
    QString displayState = result.value(QStringLiteral("state")).toString().trimmed();

    const QString simLowerBeforeIpFix = simStatus.toLower();
    bool noSim = simLowerBeforeIpFix.contains(QStringLiteral("not found"))
                 || simLowerBeforeIpFix.contains(QStringLiteral("no sim"))
                 || cellularTextSuggestsNoSim(simStatus);

    /*
     * Important rule for this hardware:
     *
     * If rmnet_mhi0.1 has a usable IPv4 address, the data call is already up.
     * In that state, stale old log lines like "No SIM", "Registration timeout",
     * or an old QMI detect error must not override the live rmnet state.
     */
    if (hasIp) {
        noSim = false;

        if (isUnsetCellularText(simStatus) ||
            simStatus.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0 ||
            simStatus.compare(QStringLiteral("Not found"), Qt::CaseInsensitive) == 0 ||
            simStatus.compare(QStringLiteral("No SIM"), Qt::CaseInsensitive) == 0) {
            simStatus = QStringLiteral("Ready");
        }

        const QString regLower = registration.toLower();
        if (isUnsetCellularText(registration) ||
            registration.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0 ||
            regLower.contains(QStringLiteral("timeout")) ||
            regLower.contains(QStringLiteral("detach"))) {
            registration = QStringLiteral("Attached");
        }

        if (isUnsetCellularText(displayState) ||
            displayState.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0 ||
            displayState.compare(QStringLiteral("No SIM"), Qt::CaseInsensitive) == 0 ||
            displayState.toLower().contains(QStringLiteral("timeout"))) {
            displayState = QStringLiteral("Registered");
        }

        const QString lastError = result.value(QStringLiteral("lastError")).toString().trimmed();
        if (cellularTextSuggestsNoSim(lastError) ||
            cellularTextSuggestsRegistrationTimeout(lastError) ||
            lastError.compare(QStringLiteral("SIM not found"), Qt::CaseInsensitive) == 0 ||
            lastError.compare(QStringLiteral("QMI device not detected"), Qt::CaseInsensitive) == 0 ||
            lastError.compare(QStringLiteral("AT device not detected"), Qt::CaseInsensitive) == 0 ||
            lastError.compare(QStringLiteral("QMI device open failed"), Qt::CaseInsensitive) == 0) {
            result[QStringLiteral("lastError")] = QString();
        }
    }

    const QString registrationLower = registration.toLower();
    bool registered = registrationLower.contains(QStringLiteral("attached"))
                      || registrationLower.contains(QStringLiteral("registered"))
                      || registrationLower.contains(QStringLiteral("home"))
                      || registrationLower.contains(QStringLiteral("roaming"));

    if (hasIp && !registered) {
        registration = QStringLiteral("Attached");
        registered = true;
        result[QStringLiteral("registration_state")] = registration;
    }

    const bool registrationTimeout =
        !hasIp &&
        (cellularTextSuggestsRegistrationTimeout(result.value(QStringLiteral("lastError")).toString()) ||
         cellularTextSuggestsRegistrationTimeout(registration));

    /*
     * For quectel-CM PCIe mode, a usable IPv4 address on rmnet_mhi0.1 is the
     * strongest evidence that the data call is connected. Do not require
     * ModemManager registration state, and do not require qcmServiceActive here
     * because the process/service state can lag while the interface still has IP.
     */
    const bool connected = hasIp && rmnetReady && !noSim && !registrationTimeout;

    QString dataState;

    if (connected) {
        dataState = QStringLiteral("Connected");
        simStatus = QStringLiteral("Ready");

        if (isUnsetCellularText(registration) ||
            registration.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
            registration = QStringLiteral("Attached");
        }

        if (isUnsetCellularText(displayState) ||
            displayState.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
            displayState = QStringLiteral("Registered");
        }
    } else if (noSim) {
        simStatus = QStringLiteral("Not found");
        dataState = QStringLiteral("No SIM");
        displayState = QStringLiteral("No SIM");
        if (result.value(QStringLiteral("lastError")).toString().trimmed().isEmpty())
            result[QStringLiteral("lastError")] = QStringLiteral("SIM not found");
    } else if (registrationTimeout) {
        dataState = QStringLiteral("Disconnected");
        displayState = QStringLiteral("Registration timeout");
    } else if (!pcieDetected) {
        dataState = QStringLiteral("PCIe Not Detected");
        if (isUnsetCellularText(displayState) ||
            displayState.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
            displayState = QStringLiteral("PCIe Not Detected");
        }
    } else if (!qmiOpenable && !rmnetReady) {
        dataState = QStringLiteral("Modem Not Ready");
        if (isUnsetCellularText(displayState) ||
            displayState.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
            displayState = QStringLiteral("Modem Not Ready");
        }
    } else if (!hasIp && (qcmActive || qmiOpenable || rmnetReady)) {
        dataState = QStringLiteral("Connecting");
        if (isUnsetCellularText(displayState) ||
            displayState.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) == 0) {
            displayState = qmiOpenable ? QStringLiteral("QMI Ready") : QStringLiteral("Connecting");
        }
    } else {
        dataState = QStringLiteral("Disconnected");
    }

    QString gateway = result.value(QStringLiteral("gateway")).toString().trimmed();
    if (isUnsetCellularText(gateway))
        gateway = QStringLiteral("--");

    QString accessTech = result.value(QStringLiteral("access_technology")).toString().trimmed();
    if (isUnsetCellularText(accessTech))
        accessTech = result.value(QStringLiteral("accessTech")).toString().trimmed();
    if (isUnsetCellularText(accessTech) && connected)
        accessTech = QStringLiteral("LTE");
    if (isUnsetCellularText(accessTech))
        accessTech = QStringLiteral("-");

    result[QStringLiteral("connected")] = connected;
    result[QStringLiteral("dataState")] = dataState;
    result[QStringLiteral("data_state")] = dataState;
    result[QStringLiteral("ipAddress")] = ipAddress;
    result[QStringLiteral("ip_address")] = ipAddress;
    result[QStringLiteral("gateway")] = gateway;
    result[QStringLiteral("simStatus")] = simStatus;
    result[QStringLiteral("sim_status")] = simStatus;
    result[QStringLiteral("state")] = isUnsetCellularText(displayState) ? dataState : displayState;
    result[QStringLiteral("registration_state")] =
        isUnsetCellularText(registration) ? QStringLiteral("Unknown") : registration;
    result[QStringLiteral("accessTech")] = accessTech;
    result[QStringLiteral("access_technology")] = accessTech;

    if (isUnsetCellularText(result.value(QStringLiteral("operator")).toString()))
        result[QStringLiteral("operator")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("operator_code")).toString()))
        result[QStringLiteral("operator_code")] = result.value(QStringLiteral("plmn")).toString();
    if (isUnsetCellularText(result.value(QStringLiteral("plmn")).toString()))
        result[QStringLiteral("plmn")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("signal")).toString()))
        result[QStringLiteral("signal")] = QStringLiteral("--");
    if (isUnsetCellularText(result.value(QStringLiteral("imei")).toString()))
        result[QStringLiteral("imei")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("iccid")).toString()))
        result[QStringLiteral("iccid")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("simIccid")).toString()))
        result[QStringLiteral("simIccid")] = result.value(QStringLiteral("iccid")).toString();

    result[QStringLiteral("moduleLogs")] = cellularModuleLogs(80);
    result[QStringLiteral("note")] =
        QStringLiteral("Realtime status from /tmp/quectel-CM.log + rmnet_mhi0.1 + service state; rmnet IPv4 wins over stale modem log errors");
#else
    result[QStringLiteral("connected")] = false;
    result[QStringLiteral("state")] = QStringLiteral("disabled");
    result[QStringLiteral("dataState")] = QStringLiteral("Disabled");
    result[QStringLiteral("data_state")] = QStringLiteral("Disabled");
    result[QStringLiteral("simStatus")] = QStringLiteral("Disabled");
    result[QStringLiteral("sim_status")] = QStringLiteral("Disabled");
    result[QStringLiteral("ipAddress")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("ip_address")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("message")] = QStringLiteral("Build is HW_NONE_5G");
#endif

    return result;
}

QStringList NetworkController::cellularModuleLogs(int maxLines)
{
    QStringList logs;

#if HARDWARE_HAS_5G
    const int limit = qBound(20, maxLines, 300);

    /*
     * This function is called by the 5G UI. Avoid journalctl/tail forks on every
     * refresh. Read the two real log files directly and cache the formatted list.
     * Fallback commands are only used when both files are empty/unavailable.
     */
    static QMutex mutex;
    static qint64 lastMs = 0;
    static int lastLimit = 0;
    static QStringList lastLogs;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex);
        if (lastMs > 0 && lastLimit >= limit && (now - lastMs) < 5000)
            return lastLogs.mid(0, limit);
    }

    auto appendLimited = [&](const QStringList &src, int maxAdd) {
        for (const QString &line : src) {
            if (logs.size() >= limit || maxAdd <= 0)
                break;
            if (!line.trimmed().isEmpty()) {
                logs << line;
                --maxAdd;
            }
        }
    };

    const QString qcmText = readLastTextFileBytes(QStringLiteral("/tmp/quectel-CM.log"), 96 * 1024);
    appendLimited(newestFirstFromText(qcmText, qMax(10, (limit * 2) / 3)), limit);

    if (logs.size() < limit) {
        const QString recoverText = readLastTextFileBytes(QStringLiteral("/var/log/5g-pcie-recover.log"), 64 * 1024);
        appendLimited(newestFirstFromText(recoverText,
                                          limit - logs.size(),
                                          QStringLiteral("[recover] ")),
                      limit - logs.size());
    }

    if (logs.isEmpty()) {
        QString out, err;
        if (runProcessBlocking(QStringLiteral("journalctl"),
                               {QStringLiteral("-u"), QStringLiteral("quectel-cm.service"),
                                QStringLiteral("-n"), QString::number(limit),
                                QStringLiteral("--no-pager"),
                                QStringLiteral("-o"), QStringLiteral("short-iso")},
                               &out, &err, 1500)) {
            logs = newestFirstFromText(out, limit);
        }
    }

    if (logs.isEmpty()) {
        QString out, err;
        if (runProcessBlocking(QStringLiteral("dmesg"), {}, &out, &err, 1500)
            && !out.trimmed().isEmpty()) {
            const QStringList keywords = {
                QStringLiteral("quectel"), QStringLiteral("mhi"),
                QStringLiteral("rmnet"), QStringLiteral("wwan"),
                QStringLiteral("qmi"), QStringLiteral("pcie"),
                QStringLiteral("1eac"), QStringLiteral("100b"),
                QStringLiteral("aer"), QStringLiteral("fatal"),
                QStringLiteral("reset"), QStringLiteral("sim")
            };

            QStringList matchedLines;
            const QStringList lines = out.split(QLatin1Char('\n'), QString::SkipEmptyParts);
            for (const QString &line : lines) {
                const QString lower = line.toLower();
                bool matched = false;
                for (const QString &keyword : keywords) {
                    if (lower.contains(keyword)) {
                        matched = true;
                        break;
                    }
                }
                if (matched) {
                    const QString trimmed = line.trimmed();
                    if (!trimmed.isEmpty())
                        matchedLines << QStringLiteral("[dmesg] ") + trimmed;
                }
            }

            for (int i = matchedLines.size() - 1; i >= 0 && logs.size() < limit; --i)
                logs << matchedLines.at(i);
        }
    }

    if (logs.isEmpty()) {
        logs << QStringLiteral("No 5G logs found from /tmp/quectel-CM.log, /var/log/5g-pcie-recover.log, journalctl, or dmesg");
    }

    {
        QMutexLocker locker(&mutex);
        lastMs = now;
        lastLimit = limit;
        lastLogs = logs;
    }
#else
    Q_UNUSED(maxLines)
    logs << QStringLiteral("5G is disabled by HW_NONE_5G build macro");
#endif

    return logs;
}


bool NetworkController::isCellularRealtimeActive() const
{
#if HARDWARE_HAS_5G
    return m_cellularRealtimeTimer && m_cellularRealtimeTimer->isActive();
#else
    return false;
#endif
}

int NetworkController::cellularRealtimeIntervalMs() const
{
#if HARDWARE_HAS_5G
    return m_cellularRealtimeTimer ? m_cellularRealtimeTimer->interval() : 8000;
#else
    return 8000;
#endif
}

void NetworkController::startCellularRealtime(int intervalMs)
{
#if HARDWARE_HAS_5G
    if (!m_cellularRealtimeTimer)
        return;

    intervalMs = qBound(5000, intervalMs, 30000);
    m_cellularRealtimeDesiredActive = true;

    if (m_cellularRealtimeTimer->interval() != intervalMs)
        m_cellularRealtimeTimer->setInterval(intervalMs);

    if (m_cellularRealtimeSuspended) {
        qWarning() << "[5G] startCellularRealtime deferred while reset is active intervalMs =" << intervalMs;
        return;
    }

    qWarning() << "[5G] startCellularRealtime intervalMs =" << intervalMs;
    // R20.2: callers already request an explicit asynchronous status snapshot.
    // Do not launch a second duplicate cellularStatus() query here. The timer
    // owns subsequent periodic refreshes.
    if (!m_cellularRealtimeTimer->isActive())
        m_cellularRealtimeTimer->start();
#else
    Q_UNUSED(intervalMs)
#endif
}

void NetworkController::stopCellularRealtime()
{
#if HARDWARE_HAS_5G
    m_cellularRealtimeDesiredActive = false;
    if (m_cellularRealtimeTimer)
        m_cellularRealtimeTimer->stop();
    qWarning() << "[5G] stopCellularRealtime";
#endif
}

void NetworkController::suspendCellularRealtime()
{
#if HARDWARE_HAS_5G
    m_cellularRealtimeSuspended = true;
    if (m_cellularRealtimeTimer)
        m_cellularRealtimeTimer->stop();
    qWarning() << "[5G] suspendCellularRealtime desiredActive =" << m_cellularRealtimeDesiredActive;
#endif
}

void NetworkController::resumeCellularRealtime(bool immediatePoll)
{
#if HARDWARE_HAS_5G
    m_cellularRealtimeSuspended = false;
    if (!m_cellularRealtimeTimer || !m_cellularRealtimeDesiredActive) {
        qWarning() << "[5G] resumeCellularRealtime no active request";
        return;
    }

    qWarning() << "[5G] resumeCellularRealtime intervalMs ="
               << m_cellularRealtimeTimer->interval()
               << "immediatePoll =" << immediatePoll;
    if (immediatePoll)
        pollCellularRealtime();
    if (!m_cellularRealtimeTimer->isActive())
        m_cellularRealtimeTimer->start();
#else
    Q_UNUSED(immediatePoll)
#endif
}

QVariantMap NetworkController::cellularRealtimeSnapshot()
{
#if HARDWARE_HAS_5G
    return cellularStatus();
#else
    QVariantMap m;
    m[QStringLiteral("hardwareHas5G")] = false;
    m[QStringLiteral("connected")] = false;
    m[QStringLiteral("simStatus")] = QStringLiteral("Disabled");
    m[QStringLiteral("sim_status")] = QStringLiteral("Disabled");
    m[QStringLiteral("state")] = QStringLiteral("5G disabled by build macro");
    m[QStringLiteral("dataState")] = QStringLiteral("Disabled");
    return m;
#endif
}

void NetworkController::pollCellularRealtime()
{
#if HARDWARE_HAS_5G
    // R20.2: cellularStatus() performs several service/device/log probes and can
    // block for seconds. Never execute it on the Qt GUI/WebSocket audio thread.
    if (m_cellularRealtimeSuspended || m_cellularRealtimeQueryInFlight)
        return;

    m_cellularRealtimeQueryInFlight = true;
    QPointer<NetworkController> self(this);

    QThread *thread = QThread::create([self]() {
        NetworkController worker;
        QVariantMap status = worker.cellularStatus();

        if (!self)
            return;

        QMetaObject::invokeMethod(self, [self, status]() mutable {
            if (!self)
                return;

            self->m_cellularRealtimeQueryInFlight = false;
            if (self->m_cellularRealtimeSuspended || !self->m_cellularRealtimeDesiredActive)
                return;

            status[QStringLiteral("menuID")] = QStringLiteral("lte_state");
            status[QStringLiteral("realtime")] = true;
            status[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString(Qt::ISODate);

            const QJsonObject obj = QJsonObject::fromVariantMap(status);
            const QString compact = QString::fromUtf8(
                QJsonDocument(obj).toJson(QJsonDocument::Compact));

            if (compact != self->m_lastCellularRealtimeJson) {
                self->m_lastCellularRealtimeJson = compact;
                emit self->cellularRealtimeStatusChanged(status);
            }
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
#endif
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
