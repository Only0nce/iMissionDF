#ifndef NETWORKCONTROLLER_H
#define NETWORKCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>

class NetworkController : public QObject
{
    Q_OBJECT
public:
    explicit NetworkController(QObject *parent = nullptr);

    Q_INVOKABLE void applyNetworkConfig(const QString &iface,
                                        const QString &mode,
                                        const QString &ipWithCidr,
                                        const QString &gateway,
                                        const QString &dnsList);

    Q_INVOKABLE QVariantMap loadConfig(const QString &iface);
    Q_INVOKABLE QVariantMap loadAllLanConfig();
    Q_INVOKABLE QVariantMap queryDhcpInfo(const QString &iface);

    // ===== WiFi =====
    Q_INVOKABLE QVariantMap loadWifiConfig();
    Q_INVOKABLE QVariantList scanWifi(const QString &iface = QStringLiteral("wlan0"));
    Q_INVOKABLE QVariantMap wifiStatus(const QString &iface = QStringLiteral("wlan0"));
    Q_INVOKABLE void connectWifi(const QString &iface,
                                 const QString &ssid,
                                 const QString &password,
                                 bool autoConnect = true);
    Q_INVOKABLE void disconnectWifi(const QString &iface = QStringLiteral("wlan0"));

    // ===== 5G / Cellular =====
    // These functions are always declared so QML never breaks.
    // If built with HW_NONE_5G, cellular functions return disabled/no-op.
    Q_INVOKABLE QVariantMap loadCellularConfig();
    Q_INVOKABLE QVariantList listModems();
    Q_INVOKABLE QVariantMap cellularStatus();
    Q_INVOKABLE void connectCellular(const QString &apn,
                                     const QString &iface = QStringLiteral("*"),
                                     bool autoConnect = true);
    Q_INVOKABLE void disconnectCellular(const QString &connectionName = QStringLiteral("cellular-5g"));

    // ===== NTP =====
    void setNtpServer(const QString &ntpServer);
    void resetNtp();

    // ===== Timezone =====
    QString getTimezone() const;
    QJsonObject getNtpConfig() const;

signals:
    void applyNetworkConfigStarted(const QString &iface);
    void applyNetworkConfigFinished(const QString &iface,
                                    bool ok,
                                    const QString &message,
                                    const QString &gateway,
                                    const QString &dns);

    // แจ้งผล apply ด้วย nmcli แบบ background
    void applyNetworkConfigNmcliFinished(const QString &iface,
                                         bool ok,
                                         const QString &message);

    // New operation result signals for QML refresh/toast.
    void wifiOperationFinished(const QString &action, bool ok, const QString &message);
    void cellularOperationFinished(const QString &action, bool ok, const QString &message);

private:
    void runCommand(const QString &cmd) const;
    void saveConfigToJson(const QJsonObject &obj);
    void runNmcliCommand(const QStringList &args);
};

#endif // NETWORKCONTROLLER_H
