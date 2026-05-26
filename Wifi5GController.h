#ifndef WIFI5GCONTROLLER_H
#define WIFI5GCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class NetworkController;

class Wifi5GController : public QObject
{
    Q_OBJECT

public:
    explicit Wifi5GController(NetworkController *networkController,
                              QObject *parent = nullptr);

    bool canHandle(const QString &menuId) const;
    bool handleCommand(const QJsonObject &command);

signals:
    void responseReady(const QString &jsonMessage);

private:
    NetworkController *m_networkController = nullptr;

    void emitJson(const QJsonObject &obj);
    void emitError(const QString &menuId, const QString &message);

    // Query commands can call nmcli/mmcli and may block for seconds.
    // Keep them off the UI thread and only emit compact JSON back to QML.
    void sendSnapshot();
    void sendWifiScan(const QString &iface,
                      const QString &menuId = QStringLiteral("wifiScan"));
    void sendWifiState(const QString &iface, const QString &menuId);
    void sendWifiToggle(bool enabled, const QString &menuId);
    void sendWifiForget(const QString &ssid, const QString &menuId);
    void sendWifiAdvancedInfo(const QString &ssid,
                              const QString &iface,
                              const QString &menuId);
    void sendWifiApplyIpv4(const QJsonObject &command, const QString &menuId);
    void sendCellularStatus(const QString &menuId = QStringLiteral("cellularStatus"));
    void sendModemList();

private slots:
    void onWifiOperationFinished(const QString &action, bool ok, const QString &message);
    void onCellularOperationFinished(const QString &action, bool ok, const QString &message);
};

#endif // WIFI5GCONTROLLER_H
