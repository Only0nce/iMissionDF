#ifndef NETWORKSECURITYCONTROLLER_H
#define NETWORKSECURITYCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

class NetworkSecurityController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool lockedOut READ lockedOut NOTIFY lockoutChanged)

public:
    explicit NetworkSecurityController(QObject *parent = nullptr);

    Q_INVOKABLE bool verifyPassword(const QString &password);
    Q_INVOKABLE bool lockedOut() const;
    Q_INVOKABLE int lockoutRemainingSeconds() const;
    Q_INVOKABLE int remainingAttempts() const;

signals:
    void lockoutChanged();

private:
    static constexpr int kMaxFailedAttempts = 5;
    static constexpr qint64 kLockoutDurationMs = 30000;

    static bool isValidSha256Hex(const QByteArray &value);
    static bool constantTimeEquals(const QByteArray &lhs, const QByteArray &rhs);
    qint64 nowMs() const;
    void resetFailures();

    QByteArray m_expectedHash;
    int m_failedAttempts = 0;
    qint64 m_lockoutUntilMs = 0;
};

#endif // NETWORKSECURITYCONTROLLER_H
