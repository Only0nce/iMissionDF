#ifndef NETWORKCONTROLLER_H
#define NETWORKCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
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

    // ✅ NEW: แจ้งผลการ apply ด้วย nmcli (มาทีหลัง ไม่บล็อก UI)
    void applyNetworkConfigNmcliFinished(const QString &iface,
                                         bool ok,
                                         const QString &message);

private:
    void runCommand(const QString &cmd) const;
    void saveConfigToJson(const QJsonObject &obj);
    void runNmcliCommand(const QStringList &args);
};

#endif // NETWORKCONTROLLER_H
