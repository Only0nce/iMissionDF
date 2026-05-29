#include "NetworkController.h"

#include <QPointer>
#include <QProcess>
#include <QFile>
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

#include <algorithm>

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

static QVariantMap readLteSignalFromCsq()
{
    QVariantMap result;
    result[QStringLiteral("ok")] = false;
    result[QStringLiteral("signal")] = QString();
    result[QStringLiteral("csq")] = QString();
    result[QStringLiteral("dbm")] = QString();
    result[QStringLiteral("raw")] = QString();
    result[QStringLiteral("error")] = QString();

    if (!commandExists(QStringLiteral("socat"))) {
        result[QStringLiteral("error")] = QStringLiteral("socat command not found");
        return result;
    }

    if (!QFile::exists(QStringLiteral("/dev/mhi_DUN"))) {
        result[QStringLiteral("error")] = QStringLiteral("/dev/mhi_DUN not found");
        return result;
    }

    const QString shell = QStringLiteral("printf \"AT+CSQ\\r\" | socat - /dev/mhi_DUN,crnl");
    QString out, err;
    if (!runProcessBlocking("bash", {"-lc", shell}, &out, &err, 10000)) {
        result[QStringLiteral("raw")] = out;
        result[QStringLiteral("error")] = err.isEmpty() ? out : err;
        return result;
    }

    result[QStringLiteral("raw")] = out;
    const QRegularExpression re(QStringLiteral("\\+CSQ:\\s*(\\d+)\\s*,\\s*(\\d+)"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(out);
    if (!match.hasMatch()) {
        result[QStringLiteral("error")] = QStringLiteral("Unable to parse +CSQ response");
        return result;
    }

    const int csq = match.captured(1).toInt();
    if (csq >= 0 && csq <= 31) {
        const int dbm = -113 + (2 * csq);
        result[QStringLiteral("ok")] = true;
        result[QStringLiteral("csq")] = QString::number(csq);
        result[QStringLiteral("dbm")] = QString::number(dbm);
        result[QStringLiteral("signal")] = QStringLiteral("%1 dBm").arg(dbm);
    } else if (csq == 99) {
        result[QStringLiteral("error")] = QStringLiteral("CSQ unknown");
    } else {
        result[QStringLiteral("error")] = QStringLiteral("CSQ out of range: %1").arg(csq);
    }

    return result;
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

        const QStringList dnsParts = dnsText.split(',', QString::SkipEmptyParts);
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

    for (const QString &line : out.split('\n', QString::SkipEmptyParts)) {
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
    result[QStringLiteral("connected")] = false;
    result[QStringLiteral("modemName")] = QString();
    result[QStringLiteral("interface")] = QString();
    result[QStringLiteral("device")] = QString();
    result[QStringLiteral("operator")] = QString();
    result[QStringLiteral("operator_code")] = QString();
    result[QStringLiteral("plmn")] = QString();
    result[QStringLiteral("state")] = QStringLiteral("Unknown");
    result[QStringLiteral("registration_state")] = QString();
    result[QStringLiteral("accessTech")] = QString();
    result[QStringLiteral("access_technology")] = QString();
    result[QStringLiteral("signal")] = QStringLiteral("--");
    result[QStringLiteral("imei")] = QStringLiteral("-");
    result[QStringLiteral("simStatus")] = QStringLiteral("Unknown");
    result[QStringLiteral("sim_status")] = QStringLiteral("Unknown");
    result[QStringLiteral("simIccid")] = QStringLiteral("-");
    result[QStringLiteral("iccid")] = QStringLiteral("-");
    result[QStringLiteral("dataState")] = QStringLiteral("Disconnected");
    result[QStringLiteral("ipAddress")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("ip_address")] = QStringLiteral("No IPv4 assigned");
    result[QStringLiteral("gateway")] = QStringLiteral("--");
    result[QStringLiteral("lastError")] = QString();
    result[QStringLiteral("note")] = QStringLiteral("Structured cellular status generated from ip/nmcli/mmcli.");

    const QString primaryIface = QStringLiteral("rmnet_mhi0.1");
    const QString fallbackIface = QStringLiteral("rmnet_mhi0");

    QVariantMap snapshot = parseIfaceIpSnapshot(primaryIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceSnapshot(primaryIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceIpSnapshot(fallbackIface);
    if (snapshot.isEmpty())
        snapshot = parseIfaceSnapshot(fallbackIface);

    if (!snapshot.isEmpty()) {
        const QString iface = snapshot.value(QStringLiteral("iface")).toString();
        const QString flags = snapshot.value(QStringLiteral("flags")).toString();
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

        if (!flags.isEmpty())
            result[QStringLiteral("registration_state")] = flags;

        const QString lowerFlags = flags.toLower();
        QStringList noteParts;
        noteParts << QStringLiteral("Using interface %1 (fallback %2)").arg(iface, fallbackIface);
        if (!snapshot.value(QStringLiteral("mtu")).toString().isEmpty())
            noteParts << QStringLiteral("MTU %1").arg(snapshot.value(QStringLiteral("mtu")).toString());
        if (!snapshot.value(QStringLiteral("tx_queue")).toString().isEmpty())
            noteParts << QStringLiteral("Queue %1").arg(snapshot.value(QStringLiteral("tx_queue")).toString());
        if (!lowerFlags.isEmpty())
            noteParts << QStringLiteral("Flags %1").arg(flags);
        result[QStringLiteral("note")] = noteParts.join(QStringLiteral(" · "));
    }

    const QVariantMap nmDevice = findLteNmDevice();
    if (!nmDevice.isEmpty()) {
        const QString device = nmDevice.value(QStringLiteral("device")).toString();
        if (result.value(QStringLiteral("device")).toString().isEmpty()) {
            result[QStringLiteral("device")] = device;
            result[QStringLiteral("interface")] = device;
        }
        if (!device.isEmpty()) {
            const QVariantMap live = parseDeviceIpv4(device);
            const QString liveIp = live.value(QStringLiteral("dev_ip4_plain")).toString();
            if (hasUsableIpv4Address(liveIp)) {
                result[QStringLiteral("ipAddress")] = liveIp;
                result[QStringLiteral("ip_address")] = liveIp;
            }
            if (!live.value(QStringLiteral("dev_ip4_gateway")).toString().isEmpty())
                result[QStringLiteral("gateway")] =
                    live.value(QStringLiteral("dev_ip4_gateway")).toString();
        }
        result[QStringLiteral("connection")] =
            nmDevice.value(QStringLiteral("connection")).toString();
        result[QStringLiteral("nmState")] =
            nmDevice.value(QStringLiteral("state")).toString();
    }

    const QVariantMap csqSignal = readLteSignalFromCsq();
    if (csqSignal.value(QStringLiteral("ok")).toBool())
        result[QStringLiteral("signal")] = csqSignal.value(QStringLiteral("signal")).toString();

    if (commandExists(QStringLiteral("mmcli"))) {
        const QString modemId = findFirstModemId();
        if (!modemId.isEmpty()) {
            QString out, err;
            if (runProcessBlocking("mmcli", {"-m", modemId, "-K"}, &out, &err, 10000)) {
                const QVariantMap modem = parseKeyValueLines(out.split('\n', QString::SkipEmptyParts));
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
                const QString failedReason = pickFirstValue(modem, {
                                                                    QStringLiteral("modem.generic.failed-reason"),
                                                                    QStringLiteral("modem.failed-reason")
                                                                });
                const QString operatorName = pickFirstValue(modem, {
                                                                       QStringLiteral("modem.3gpp.operator-name"),
                                                                       QStringLiteral("modem.3gpp.operator-code")
                                                                   });
                const QString operatorCode = pickFirstValue(modem, {
                                                                    QStringLiteral("modem.3gpp.operator-code"),
                                                                    QStringLiteral("modem.3gpp.plmn")
                                                                });
                const QString signal = pickFirstValue(modem, {
                                                                 QStringLiteral("modem.generic.signal-quality.value"),
                                                                 QStringLiteral("modem.signal-quality.value")
                                                             });
                const QString access = pickFirstValue(modem, {
                                                                 QStringLiteral("modem.generic.access-technologies"),
                                                                 QStringLiteral("modem.3gpp.packet-service-state")
                                                             });
                const QString imei = pickFirstValue(modem, {
                                                               QStringLiteral("modem.3gpp.imei"),
                                                               QStringLiteral("modem.generic.equipment-identifier")
                                                           });

                if (!operatorName.isEmpty() && result.value(QStringLiteral("operator")).toString().isEmpty())
                    result[QStringLiteral("operator")] = operatorName;
                if (!operatorCode.isEmpty()) {
                    result[QStringLiteral("operator_code")] = operatorCode;
                    result[QStringLiteral("plmn")] = operatorCode;
                }
                if (!modemState.isEmpty())
                    result[QStringLiteral("state")] = modemState;
                if (!registrationState.isEmpty())
                    result[QStringLiteral("registration_state")] = registrationState;
                if (result.value(QStringLiteral("signal")).toString().isEmpty() && !signal.isEmpty())
                    result[QStringLiteral("signal")] = signal.endsWith(QLatin1Char('%')) ? signal : signal + "%";
                if (result.value(QStringLiteral("access_technology")).toString().isEmpty() && !access.isEmpty())
                    result[QStringLiteral("access_technology")] = access;
                if (!imei.isEmpty())
                    result[QStringLiteral("imei")] = imei;
                if (!failedReason.isEmpty() && !isUnsetCellularText(failedReason))
                    result[QStringLiteral("lastError")] = failedReason;
                result[QStringLiteral("modemIndex")] = modemId.toInt();
                result[QStringLiteral("modemName")] =
                    pickFirstValue(modem, {QStringLiteral("modem.generic.model"),
                                           QStringLiteral("modem.generic.manufacturer")});

                if (!simPath.isEmpty()) {
                    result[QStringLiteral("simStatus")] = QStringLiteral("Ready");
                    result[QStringLiteral("sim_status")] = QStringLiteral("Ready");

                    QString simOut, simErr;
                    if (runProcessBlocking("mmcli", {"-i", simPath, "-K"}, &simOut, &simErr, 10000)) {
                        const QVariantMap sim = parseKeyValueLines(simOut.split('\n', QString::SkipEmptyParts));
                        const QString iccid = pickFirstValue(sim, {
                                                                      QStringLiteral("sim.properties.iccid"),
                                                                      QStringLiteral("sim.iccid")
                                                                  });
                        if (!iccid.isEmpty()) {
                            result[QStringLiteral("iccid")] = iccid;
                            result[QStringLiteral("simIccid")] = iccid;
                        }
                    }
                } else if (cellularTextSuggestsNoSim(failedReason)
                           || modemState.toLower().contains(QStringLiteral("failed"))) {
                    result[QStringLiteral("simStatus")] = QStringLiteral("Not found");
                    result[QStringLiteral("sim_status")] = QStringLiteral("Not found");
                }
            }
        }
    }

    if (isUnsetCellularText(result.value(QStringLiteral("signal")).toString()))
        result[QStringLiteral("signal")] = QStringLiteral("--");

    const QStringList moduleLogs = cellularModuleLogs(80);
    result[QStringLiteral("moduleLogs")] = moduleLogs;

    const QString logText = moduleLogs.join(QLatin1Char('\n'));
    const bool directStatusHasIp =
        hasUsableIpv4Address(result.value(QStringLiteral("ipAddress")).toString())
        || hasUsableIpv4Address(result.value(QStringLiteral("ip_address")).toString());

    // Logs are diagnostic fallback only. Do not let stale log lines override
    // direct IP/mmcli/nmcli evidence that the modem currently has service.
    if ((isUnsetCellularText(result.value(QStringLiteral("simStatus")).toString())
         || result.value(QStringLiteral("simStatus")).toString() == QStringLiteral("Unknown"))
        && !directStatusHasIp
        && cellularTextSuggestsNoSim(logText)) {
        result[QStringLiteral("simStatus")] = QStringLiteral("Not found");
        result[QStringLiteral("sim_status")] = QStringLiteral("Not found");
        result[QStringLiteral("lastError")] = QStringLiteral("SIM not found");
    }

    if (!directStatusHasIp && cellularTextSuggestsRegistrationTimeout(logText)) {
        result[QStringLiteral("lastError")] = QStringLiteral("Registration timeout");
    }

    QString simStatus = result.value(QStringLiteral("simStatus")).toString();
    if (isUnsetCellularText(simStatus))
        simStatus = result.value(QStringLiteral("sim_status")).toString();
    if (isUnsetCellularText(simStatus))
        simStatus = QStringLiteral("Unknown");

    QString ipAddress = result.value(QStringLiteral("ipAddress")).toString();
    if (!hasUsableIpv4Address(ipAddress))
        ipAddress = result.value(QStringLiteral("ip_address")).toString();

    const bool hasIp = hasUsableIpv4Address(ipAddress);
    if (!hasIp)
        ipAddress = QStringLiteral("No IPv4 assigned");

    QString gateway = result.value(QStringLiteral("gateway")).toString();
    if (isUnsetCellularText(gateway))
        gateway = QStringLiteral("--");

    const QString allStatusText =
        (simStatus + QLatin1Char('\n')
         + result.value(QStringLiteral("lastError")).toString() + QLatin1Char('\n')
         + result.value(QStringLiteral("state")).toString() + QLatin1Char('\n')
         + result.value(QStringLiteral("registration_state")).toString() + QLatin1Char('\n')
         + result.value(QStringLiteral("nmState")).toString()).toLower();

    const bool noSim = cellularTextSuggestsNoSim(allStatusText);
    const bool registrationTimeout = cellularTextSuggestsRegistrationTimeout(allStatusText);
    const bool simUnknown = simStatus.trimmed().toLower() == QStringLiteral("unknown")
                            || simStatus.trimmed().toLower() == QStringLiteral("no data");
    const bool simOk = !noSim && (!simUnknown || hasIp);
    const bool modemRegistered =
        allStatusText.contains(QStringLiteral("connected"))
        || allStatusText.contains(QStringLiteral("registered"))
        || allStatusText.contains(QStringLiteral("home"))
        || allStatusText.contains(QStringLiteral("roaming"))
        || allStatusText.contains(QStringLiteral("attached"));

    const bool connected = simOk && hasIp && modemRegistered && !registrationTimeout;
    QString dataState;
    QString displayState = result.value(QStringLiteral("state")).toString();

    if (noSim) {
        simStatus = QStringLiteral("Not found");
        dataState = QStringLiteral("No SIM");
        displayState = QStringLiteral("No SIM");
        if (result.value(QStringLiteral("lastError")).toString().isEmpty())
            result[QStringLiteral("lastError")] = QStringLiteral("SIM not found");
    } else if (registrationTimeout) {
        dataState = QStringLiteral("Disconnected");
        displayState = QStringLiteral("Registration timeout");
    } else if (!hasIp) {
        dataState = QStringLiteral("No IPv4 assigned");
    } else if (connected) {
        dataState = QStringLiteral("Connected");
    } else {
        dataState = QStringLiteral("Disconnected");
    }

    result[QStringLiteral("connected")] = connected;
    result[QStringLiteral("dataState")] = dataState;
    result[QStringLiteral("data_state")] = dataState;
    result[QStringLiteral("ipAddress")] = ipAddress;
    result[QStringLiteral("ip_address")] = ipAddress;
    result[QStringLiteral("gateway")] = gateway;
    result[QStringLiteral("simStatus")] = simStatus;
    result[QStringLiteral("sim_status")] = simStatus;
    result[QStringLiteral("state")] = isUnsetCellularText(displayState) ? dataState : displayState;
    result[QStringLiteral("accessTech")] = result.value(QStringLiteral("access_technology")).toString();
    if (isUnsetCellularText(result.value(QStringLiteral("accessTech")).toString())) {
        result[QStringLiteral("accessTech")] = QStringLiteral("-");
        result[QStringLiteral("access_technology")] = QStringLiteral("-");
    }

    if (isUnsetCellularText(result.value(QStringLiteral("operator")).toString()))
        result[QStringLiteral("operator")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("plmn")).toString()))
        result[QStringLiteral("plmn")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("imei")).toString()))
        result[QStringLiteral("imei")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("iccid")).toString()))
        result[QStringLiteral("iccid")] = QStringLiteral("-");
    if (isUnsetCellularText(result.value(QStringLiteral("simIccid")).toString()))
        result[QStringLiteral("simIccid")] = result.value(QStringLiteral("iccid")).toString();
#else
    result["connected"] = false;
    result["state"] = "disabled";
    result["dataState"] = "Disabled";
    result["simStatus"] = "Disabled";
    result["sim_status"] = "Disabled";
    result["ipAddress"] = "No IPv4 assigned";
    result["ip_address"] = "No IPv4 assigned";
    result["message"] = "Build is HW_NONE_5G";
#endif

    return result;
}

QStringList NetworkController::cellularModuleLogs(int maxLines)
{
    QStringList logs;

#if HARDWARE_HAS_5G
    const int limit = qBound(20, maxLines, 300);
    QString out, err;
    bool hasRealLogSource = false;

    /*
     * ตัด prefix จาก journalctl ของ quectel-cm.service
     *
     * ก่อน:
     * 2026-05-29T14:12:12+0700 ubuntu quectel-CM[1714]: [05-29_14:12:12:576] requestReg...
     *
     * หลัง:
     * [05-29_14:12:12:576] requestReg...
     */
    auto stripQuectelJournalPrefix = [](const QString &line) -> QString {
        QString s = line.trimmed();
        if (s.isEmpty())
            return s;

        const int bracketColonPos = s.indexOf(QStringLiteral("]: "));
        if (bracketColonPos >= 0) {
            const QString message = s.mid(bracketColonPos + 3).trimmed();
            if (!message.isEmpty())
                return message;
        }

        int procPos = s.indexOf(QStringLiteral("quectel-CM"));
        if (procPos < 0)
            procPos = s.indexOf(QStringLiteral("quectel-cm"));
        if (procPos < 0)
            procPos = s.indexOf(QStringLiteral("QConnectManager"));

        if (procPos >= 0) {
            const int colonPos = s.indexOf(QStringLiteral(": "), procPos);
            if (colonPos >= 0) {
                const QString message = s.mid(colonPos + 2).trimmed();
                if (!message.isEmpty())
                    return message;
            }
        }

        return s;
    };

    auto makeNewestFirst = [](const QStringList &input) -> QStringList {
        QStringList reversed;
        reversed.reserve(input.size());

        for (int i = input.size() - 1; i >= 0; --i)
            reversed << input.at(i);

        return reversed;
    };

    auto appendJournalLines = [&logs, &stripQuectelJournalPrefix](const QString &text,
                                                                  bool stripQuectelPrefix) {
        const QStringList lines = text.split(QLatin1Char('\n'), QString::SkipEmptyParts);

        for (const QString &line : lines) {
            QString trimmed = line.trimmed();

            if (trimmed.isEmpty())
                continue;

            if (trimmed.startsWith(QStringLiteral("-- No entries")))
                continue;

            if (stripQuectelPrefix)
                trimmed = stripQuectelJournalPrefix(trimmed);

            if (trimmed.isEmpty())
                continue;

            logs << trimmed;
        }
    };

    // Primary source: QConnectManager / Quectel-CM service log.
    if (runProcessBlocking(QStringLiteral("journalctl"),
                           {QStringLiteral("-u"), QStringLiteral("quectel-cm.service"),
                            QStringLiteral("-n"), QString::number(limit),
                            QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("short-iso")},
                           &out, &err, 5000)) {
        appendJournalLines(out, true);
        if (!logs.isEmpty())
            hasRealLogSource = true;
    }

    // Fallback: ModemManager logs.
    if (logs.isEmpty()) {
        out.clear();
        err.clear();

        if (runProcessBlocking(QStringLiteral("journalctl"),
                               {QStringLiteral("-u"), QStringLiteral("ModemManager.service"),
                                QStringLiteral("-n"), QString::number(limit),
                                QStringLiteral("--no-pager"), QStringLiteral("-o"), QStringLiteral("short-iso")},
                               &out, &err, 5000)) {
            appendJournalLines(out, false);
            if (!logs.isEmpty())
                hasRealLogSource = true;
        }
    }

    // Fallback: dmesg modem related lines.
    if (logs.isEmpty()) {
        out.clear();
        err.clear();

        if (runProcessBlocking(QStringLiteral("dmesg"), {}, &out, &err, 5000)
            && !out.trimmed().isEmpty()) {

            const QStringList keywords = {
                QStringLiteral("quectel"),
                QStringLiteral("mhi"),
                QStringLiteral("rmnet"),
                QStringLiteral("wwan"),
                QStringLiteral("qmi"),
                QStringLiteral("modem"),
                QStringLiteral("lte"),
                QStringLiteral("5g"),
                QStringLiteral("sim"),
                QStringLiteral("usb")
            };

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
                        logs << trimmed;
                }
            }

            if (!logs.isEmpty())
                hasRealLogSource = true;
        }
    }

    /*
     * จำกัดจำนวน log ก่อน แล้วค่อยกลับลำดับ
     * journalctl -n จะให้ลำดับเก่า -> ใหม่
     * UI ต้องการ ใหม่ -> เก่า
     */
    if (logs.size() > limit)
        logs = logs.mid(logs.size() - limit);

    if (hasRealLogSource && logs.size() > 1)
        logs = makeNewestFirst(logs);

#else
    Q_UNUSED(maxLines)
    logs << QStringLiteral("5G is disabled by HW_NONE_5G build macro");
#endif

    return logs;
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
