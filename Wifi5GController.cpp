#include "Wifi5GController.h"

#include "NetworkController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

#ifndef HARDWARE_HAS_5G
#define HARDWARE_HAS_5G 0
#endif

#ifndef HARDWARE_HAS_WIFI
#define HARDWARE_HAS_WIFI 0
#endif

#ifndef HARDWARE_HAS_WIRELESS
#define HARDWARE_HAS_WIRELESS 0
#endif

namespace {

QString hardwareVersionName()
{
#if HARDWARE_HAS_5G
    return QStringLiteral("5G");
#else
    return QStringLiteral("NONE_5G");
#endif
}

QJsonObject toObject(const QVariantMap &map)
{
    return QJsonObject::fromVariantMap(map);
}

QJsonArray toArray(const QVariantList &list)
{
    return QJsonArray::fromVariantList(list);
}

QString stringValue(const QJsonObject &obj,
                    const QString &key,
                    const QString &fallback = QString())
{
    const QString value = obj.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

bool boolValue(const QJsonObject &obj, const QString &key, bool fallback)
{
    return obj.contains(key) ? obj.value(key).toBool(fallback) : fallback;
}

template <typename Builder>
void runNetworkQuery(QObject *owner, Builder builder)
{
    QPointer<QObject> safeOwner(owner);

    QThread *thread = QThread::create([safeOwner, builder]() {
        // A short-lived worker keeps query calls away from the UI thread.
        // NetworkController remains the single implementation of nmcli/mmcli logic.
        NetworkController network;
        const QJsonObject response = builder(network);

        if (!safeOwner)
            return;

        QMetaObject::invokeMethod(safeOwner, [safeOwner, response]() {
            if (!safeOwner)
                return;

            auto *controller = qobject_cast<Wifi5GController *>(safeOwner.data());
            if (controller) {
                const QString json = QString::fromUtf8(
                    QJsonDocument(response).toJson(QJsonDocument::Compact));
                emit controller->responseReady(json);
            }
        }, Qt::QueuedConnection);
    });

    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

} // namespace

Wifi5GController::Wifi5GController(NetworkController *networkController,
                                   QObject *parent)
    : QObject(parent),
      m_networkController(networkController)
{
    if (!m_networkController)
        return;

    connect(m_networkController, &NetworkController::wifiOperationFinished,
            this, &Wifi5GController::onWifiOperationFinished);
    connect(m_networkController, &NetworkController::cellularOperationFinished,
            this, &Wifi5GController::onCellularOperationFinished);
}

bool Wifi5GController::canHandle(const QString &menuId) const
{
    return menuId == QStringLiteral("getWifi5GPage")
           || menuId == QStringLiteral("wifi_state")
           || menuId == QStringLiteral("scan")
           || menuId == QStringLiteral("join")
           || menuId == QStringLiteral("disconnect")
           || menuId == QStringLiteral("forget")
           || menuId == QStringLiteral("wifi_password")
           || menuId == QStringLiteral("advinfo")
           || menuId == QStringLiteral("apply_ipv4")
           || menuId == QStringLiteral("wifi_advanced_save")
           || menuId == QStringLiteral("wifi_toggle")
           || menuId == QStringLiteral("lte_state")
           || menuId == QStringLiteral("wifiScan")
           || menuId == QStringLiteral("wifiStatus")
           || menuId == QStringLiteral("wifiConnect")
           || menuId == QStringLiteral("wifiDisconnect")
           || menuId == QStringLiteral("cellularStatus")
           || menuId == QStringLiteral("cellularConnect")
           || menuId == QStringLiteral("cellularDisconnect")
           || menuId == QStringLiteral("listModems");
}

bool Wifi5GController::handleCommand(const QJsonObject &command)
{
    const QString menuId = command.value(QStringLiteral("menuID")).toString();
    if (!canHandle(menuId))
        return false;

    if (!m_networkController) {
        emitError(menuId, QStringLiteral("NetworkController is not available"));
        return true;
    }

    if (menuId == QStringLiteral("getWifi5GPage")) {
        sendSnapshot();
    } else if (menuId == QStringLiteral("wifi_state")) {
        sendWifiState(stringValue(command, QStringLiteral("iface")), menuId);
    } else if (menuId == QStringLiteral("scan")) {
        sendWifiScan(stringValue(command, QStringLiteral("iface")), menuId);
    } else if (menuId == QStringLiteral("join")) {
        m_networkController->connectWifi(
            stringValue(command, QStringLiteral("iface")),
            stringValue(command, QStringLiteral("ssid")),
            command.value(QStringLiteral("password")).toString(),
            boolValue(command, QStringLiteral("autoConnect"), true),
            stringValue(command, QStringLiteral("bssid")));
    } else if (menuId == QStringLiteral("disconnect")) {
        m_networkController->disconnectWifi(
            stringValue(command, QStringLiteral("device"),
                        stringValue(command, QStringLiteral("iface"))));
    } else if (menuId == QStringLiteral("forget")) {
        sendWifiForget(stringValue(command, QStringLiteral("profileName")),
                       stringValue(command, QStringLiteral("ssid")),
                       stringValue(command, QStringLiteral("bssid")),
                       menuId);
    } else if (menuId == QStringLiteral("wifi_password")) {
        sendWifiPassword(stringValue(command, QStringLiteral("profileName")),
                         stringValue(command, QStringLiteral("ssid")),
                         stringValue(command, QStringLiteral("bssid")),
                         menuId);
    } else if (menuId == QStringLiteral("advinfo")) {
        sendWifiAdvancedInfo(stringValue(command, QStringLiteral("profileName")),
                             stringValue(command, QStringLiteral("ssid")),
                             stringValue(command, QStringLiteral("iface")),
                             menuId);
    } else if (menuId == QStringLiteral("apply_ipv4")
               || menuId == QStringLiteral("wifi_advanced_save")) {
        sendWifiApplyIpv4(command, menuId);
    } else if (menuId == QStringLiteral("wifi_toggle")) {
        sendWifiToggle(boolValue(command, QStringLiteral("on"), true), menuId);
    } else if (menuId == QStringLiteral("lte_state")) {
        sendCellularStatus(menuId);
    } else if (menuId == QStringLiteral("wifiScan")) {
        sendWifiScan(stringValue(command, QStringLiteral("iface")), menuId);
    } else if (menuId == QStringLiteral("wifiStatus")) {
        sendWifiState(stringValue(command, QStringLiteral("iface")), menuId);
    } else if (menuId == QStringLiteral("wifiConnect")) {
        m_networkController->connectWifi(
            stringValue(command, QStringLiteral("iface")),
            stringValue(command, QStringLiteral("ssid")),
            command.value(QStringLiteral("password")).toString(),
            boolValue(command, QStringLiteral("autoConnect"), true),
            stringValue(command, QStringLiteral("bssid")));
    } else if (menuId == QStringLiteral("wifiDisconnect")) {
        m_networkController->disconnectWifi(
            stringValue(command, QStringLiteral("iface")));
    } else if (menuId == QStringLiteral("cellularStatus")) {
        sendCellularStatus(menuId);
    } else if (menuId == QStringLiteral("cellularConnect")) {
        m_networkController->connectCellular(
            stringValue(command, QStringLiteral("apn"), QStringLiteral("internet")),
            stringValue(command, QStringLiteral("iface"), QStringLiteral("*")),
            boolValue(command, QStringLiteral("autoConnect"), true));
    } else if (menuId == QStringLiteral("cellularDisconnect")) {
        m_networkController->disconnectCellular(
            stringValue(command, QStringLiteral("connectionName"), QStringLiteral("cellular-5g")));
    } else if (menuId == QStringLiteral("listModems")) {
        sendModemList();
    }

    return true;
}

void Wifi5GController::emitJson(const QJsonObject &obj)
{
    emit responseReady(QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void Wifi5GController::emitError(const QString &menuId, const QString &message)
{
    QJsonObject obj;
    obj[QStringLiteral("menuID")] = menuId;
    obj[QStringLiteral("ok")] = false;
    obj[QStringLiteral("message")] = message;
    emitJson(obj);
}

void Wifi5GController::sendSnapshot()
{
    runNetworkQuery(this, [](NetworkController &network) {
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = QStringLiteral("wifi5g");
        obj[QStringLiteral("hardwareHas5G")] = bool(HARDWARE_HAS_5G);
        obj[QStringLiteral("hardwareHasWifi")] = bool(HARDWARE_HAS_WIFI);
        obj[QStringLiteral("hardwareHasWireless")] = bool(HARDWARE_HAS_WIRELESS);
        obj[QStringLiteral("hardwareVersion")] = hardwareVersionName();
        obj[QStringLiteral("wifiConfig")] = toObject(network.loadWifiConfig());

        const QString wifiIface =
            obj.value(QStringLiteral("wifiConfig")).toObject()
                .value(QStringLiteral("interface")).toString();

        obj[QStringLiteral("wifiStatus")] = toObject(network.wifiState(wifiIface));
        obj[QStringLiteral("cellularConfig")] = toObject(network.loadCellularConfig());
        obj[QStringLiteral("cellularStatus")] = toObject(network.cellularStatus());
        obj[QStringLiteral("modems")] = toArray(network.listModems());
        obj[QStringLiteral("moduleLogs")] = QJsonArray::fromStringList(network.cellularModuleLogs(120));
        return obj;
    });
}

void Wifi5GController::sendWifiScan(const QString &iface, const QString &menuId)
{
    runNetworkQuery(this, [iface, menuId](NetworkController &network) {
        const QVariantMap data = network.scanWifiPage(iface);
        const QJsonObject dataObj = toObject(data);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = !data.contains(QStringLiteral("error"));
        obj[QStringLiteral("data")] = dataObj;
        const QString device = data.value(QStringLiteral("device")).toString();
        obj[QStringLiteral("iface")] = device.isEmpty() ? iface : device;
        obj[QStringLiteral("device")] =
            device;
        obj[QStringLiteral("rows")] = toArray(data.value(QStringLiteral("rows")).toList());
        obj[QStringLiteral("networks")] = obj.value(QStringLiteral("rows")).toArray();
        obj[QStringLiteral("count")] = data.value(QStringLiteral("count")).toInt();
        obj[QStringLiteral("enabled")] =
            data.contains(QStringLiteral("enabled"))
                ? data.value(QStringLiteral("enabled")).toBool()
                : true;
        obj[QStringLiteral("active_ssid")] =
            data.value(QStringLiteral("active_ssid")).toString();
        obj[QStringLiteral("current_ip")] =
            data.value(QStringLiteral("current_ip")).toString();
        obj[QStringLiteral("current_gateway")] =
            data.value(QStringLiteral("current_gateway")).toString();
        obj[QStringLiteral("current_netmask")] =
            data.value(QStringLiteral("current_netmask")).toString();
        if (data.contains(QStringLiteral("error")))
            obj[QStringLiteral("message")] = data.value(QStringLiteral("error")).toString();
        return obj;
    });
}

void Wifi5GController::sendWifiState(const QString &iface, const QString &menuId)
{
    runNetworkQuery(this, [iface, menuId](NetworkController &network) {
        const QVariantMap state = network.wifiState(iface);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = true;
        obj[QStringLiteral("data")] = toObject(state);
        obj[QStringLiteral("status")] = toObject(state);
        const QString device = state.value(QStringLiteral("device")).toString();
        obj[QStringLiteral("iface")] = device.isEmpty() ? iface : device;
        obj[QStringLiteral("device")] =
            device;
        return obj;
    });
}

void Wifi5GController::sendWifiToggle(bool enabled, const QString &menuId)
{
    runNetworkQuery(this, [enabled, menuId](NetworkController &network) {
        const QVariantMap data = network.wifiToggle(enabled);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = data.value(QStringLiteral("ok")).toBool();
        obj[QStringLiteral("data")] = toObject(data);
        obj[QStringLiteral("message")] =
            data.value(QStringLiteral("message")).toString();
        obj[QStringLiteral("enabled")] =
            data.value(QStringLiteral("enabled")).toBool();
        obj[QStringLiteral("device")] =
            data.value(QStringLiteral("device")).toString();
        return obj;
    });
}

void Wifi5GController::sendWifiForget(const QString &profileName,
                                      const QString &ssid,
                                      const QString &bssid,
                                      const QString &menuId)
{
    runNetworkQuery(this, [profileName, ssid, bssid, menuId](NetworkController &network) {
        const QVariantMap data = network.forgetWifiProfile(profileName, ssid, bssid);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = data.value(QStringLiteral("ok")).toBool();
        obj[QStringLiteral("data")] = toObject(data);
        obj[QStringLiteral("message")] =
            data.value(QStringLiteral("message")).toString();
        obj[QStringLiteral("ssid")] = data.value(QStringLiteral("ssid")).toString();
        obj[QStringLiteral("connection_name")] =
            data.value(QStringLiteral("connection_name")).toString();
        return obj;
    });
}

void Wifi5GController::sendWifiPassword(const QString &profileName,
                                        const QString &ssid,
                                        const QString &bssid,
                                        const QString &menuId)
{
    runNetworkQuery(this, [profileName, ssid, bssid, menuId](NetworkController &network) {
        const QVariantMap data = network.wifiSavedPassword(profileName, ssid, bssid);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = data.value(QStringLiteral("ok")).toBool();
        obj[QStringLiteral("data")] = toObject(data);
        obj[QStringLiteral("ssid")] = data.value(QStringLiteral("ssid")).toString();
        obj[QStringLiteral("profileName")] =
            data.value(QStringLiteral("connection_name")).toString();
        obj[QStringLiteral("hasPassword")] =
            data.value(QStringLiteral("has_password")).toBool();
        obj[QStringLiteral("password")] = data.value(QStringLiteral("password")).toString();
        obj[QStringLiteral("message")] =
            data.value(QStringLiteral("message")).toString();
        return obj;
    });
}

void Wifi5GController::sendWifiAdvancedInfo(const QString &profileName,
                                            const QString &ssid,
                                            const QString &iface,
                                            const QString &menuId)
{
    runNetworkQuery(this, [profileName, ssid, iface, menuId](NetworkController &network) {
        const QVariantMap data = network.wifiAdvancedInfoForProfile(profileName, ssid, iface);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = data.value(QStringLiteral("ok"), true).toBool();
        obj[QStringLiteral("data")] = toObject(data);
        obj[QStringLiteral("info")] = toObject(data);
        obj[QStringLiteral("message")] =
            data.value(QStringLiteral("message")).toString();
        return obj;
    });
}

void Wifi5GController::sendWifiApplyIpv4(const QJsonObject &command,
                                         const QString &menuId)
{
    const QString profileName = stringValue(command, QStringLiteral("profileName"));
    const QString ssid = stringValue(command, QStringLiteral("ssid"));
    const QString iface = stringValue(command, QStringLiteral("iface"));
    QString method = stringValue(command, QStringLiteral("method"),
                                 stringValue(command, QStringLiteral("ipv4Mode"),
                                             QStringLiteral("auto")));
    if (method == QStringLiteral("dhcp"))
        method = QStringLiteral("auto");
    const QString ip = stringValue(command, QStringLiteral("ip"),
                                   stringValue(command, QStringLiteral("ipAddress")));
    const QString mask = stringValue(command, QStringLiteral("mask"),
                                     stringValue(command, QStringLiteral("subnetMask")));
    const QString gateway = stringValue(command, QStringLiteral("gw"),
                                        stringValue(command, QStringLiteral("gateway")));
    const bool dnsAuto = boolValue(command, QStringLiteral("dns_auto"),
                                   boolValue(command, QStringLiteral("dnsAuto"),
                                             boolValue(command, QStringLiteral("dnsAutomatic"), true)));
    const QString dns = stringValue(command, QStringLiteral("dns"),
                                    stringValue(command, QStringLiteral("dnsServers")));

    runNetworkQuery(this, [profileName, ssid, iface, method, ip, mask, gateway, dnsAuto, dns, menuId](NetworkController &network) {
        const QVariantMap data = network.applyWifiIpv4ForProfile(profileName, ssid, iface, method, ip, mask, gateway, dnsAuto, dns);
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = data.value(QStringLiteral("ok")).toBool();
        obj[QStringLiteral("data")] = toObject(data);
        obj[QStringLiteral("message")] =
            data.value(QStringLiteral("message")).toString();
        obj[QStringLiteral("warning")] =
            data.value(QStringLiteral("warning")).toString();
        return obj;
    });
}

void Wifi5GController::sendCellularStatus(const QString &menuId)
{
    runNetworkQuery(this, [menuId](NetworkController &network) {
        const QVariantMap status = network.cellularStatus();
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = menuId;
        obj[QStringLiteral("ok")] = true;
        obj[QStringLiteral("data")] = toObject(status);
        obj[QStringLiteral("status")] = toObject(status);
        obj[QStringLiteral("modems")] = toArray(network.listModems());
        obj[QStringLiteral("moduleLogs")] = QJsonArray::fromStringList(network.cellularModuleLogs(120));
        return obj;
    });
}

void Wifi5GController::sendModemList()
{
    runNetworkQuery(this, [](NetworkController &network) {
        QJsonObject obj;
        obj[QStringLiteral("menuID")] = QStringLiteral("listModems");
        obj[QStringLiteral("modems")] = toArray(network.listModems());
        return obj;
    });
}

void Wifi5GController::onWifiOperationFinished(const QString &action,
                                               bool ok,
                                               const QString &message)
{
    QJsonObject obj;
    obj[QStringLiteral("menuID")] = QStringLiteral("wifiOperationResult");
    obj[QStringLiteral("action")] = action;
    obj[QStringLiteral("ok")] = ok;
    obj[QStringLiteral("message")] = message;
    emitJson(obj);

    // Refresh the page after the async operation result so QML gets current state.
    sendSnapshot();
}

void Wifi5GController::onCellularOperationFinished(const QString &action,
                                                   bool ok,
                                                   const QString &message)
{
    QJsonObject obj;
    obj[QStringLiteral("menuID")] = QStringLiteral("cellularOperationResult");
    obj[QStringLiteral("action")] = action;
    obj[QStringLiteral("ok")] = ok;
    obj[QStringLiteral("message")] = message;
    emitJson(obj);

    // Refresh the page after the async operation result so QML gets current state.
    sendSnapshot();
}
