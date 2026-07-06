#include "NetworkSecurityController.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>

namespace {
// SHA-256 of the legacy TopNetworkDrawer password.
// The plaintext password is intentionally not stored in C++ or QML anymore.
const QByteArray kLegacyNetworkPasswordSha256(
    "fd2825f94b55eb6ee5eb0628e6d7dac72e0569d0edd8847ac8d1d75f47fa6224");

const char kPasswordHashEnv[] = "ISCAN_NETWORK_ADMIN_PASSWORD_SHA256";
}

NetworkSecurityController::NetworkSecurityController(QObject *parent)
    : QObject(parent)
{
    const QByteArray configuredHash = qgetenv(kPasswordHashEnv).trimmed().toLower();

    if (isValidSha256Hex(configuredHash)) {
        m_expectedHash = QByteArray::fromHex(configuredHash);
        qInfo().noquote() << "[SECURITY] Network admin password hash loaded from environment";
    } else {
        if (!configuredHash.isEmpty()) {
            qWarning().noquote()
                << "[SECURITY] Ignoring invalid ISCAN_NETWORK_ADMIN_PASSWORD_SHA256;"
                << "expected 64 hexadecimal characters";
        }
        m_expectedHash = QByteArray::fromHex(kLegacyNetworkPasswordSha256);
        qInfo().noquote() << "[SECURITY] Network admin password uses embedded legacy hash";
    }
}

bool NetworkSecurityController::verifyPassword(const QString &password)
{
    if (lockedOut())
        return false;

    // Expired lockout starts a fresh attempt window.
    if (m_lockoutUntilMs > 0) {
        m_lockoutUntilMs = 0;
        m_failedAttempts = 0;
        emit lockoutChanged();
    }

    const QByteArray candidateHash = QCryptographicHash::hash(
        password.toUtf8(), QCryptographicHash::Sha256);

    if (constantTimeEquals(candidateHash, m_expectedHash)) {
        resetFailures();
        return true;
    }

    ++m_failedAttempts;
    if (m_failedAttempts >= kMaxFailedAttempts) {
        m_lockoutUntilMs = nowMs() + kLockoutDurationMs;
        emit lockoutChanged();
    }

    return false;
}

bool NetworkSecurityController::lockedOut() const
{
    return m_lockoutUntilMs > nowMs();
}

int NetworkSecurityController::lockoutRemainingSeconds() const
{
    const qint64 remainingMs = m_lockoutUntilMs - nowMs();
    if (remainingMs <= 0)
        return 0;
    return static_cast<int>((remainingMs + 999) / 1000);
}

int NetworkSecurityController::remainingAttempts() const
{
    if (lockedOut())
        return 0;
    const int remaining = kMaxFailedAttempts - m_failedAttempts;
    return remaining > 0 ? remaining : 0;
}

bool NetworkSecurityController::isValidSha256Hex(const QByteArray &value)
{
    if (value.size() != 64)
        return false;

    for (const char ch : value) {
        const bool digit = ch >= '0' && ch <= '9';
        const bool lowerHex = ch >= 'a' && ch <= 'f';
        const bool upperHex = ch >= 'A' && ch <= 'F';
        if (!digit && !lowerHex && !upperHex)
            return false;
    }
    return true;
}

bool NetworkSecurityController::constantTimeEquals(const QByteArray &lhs,
                                                   const QByteArray &rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    unsigned char diff = 0;
    for (int i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<unsigned char>(lhs.at(i))
              ^ static_cast<unsigned char>(rhs.at(i));
    }
    return diff == 0;
}

qint64 NetworkSecurityController::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

void NetworkSecurityController::resetFailures()
{
    const bool wasLocked = lockedOut();
    m_failedAttempts = 0;
    m_lockoutUntilMs = 0;
    if (wasLocked)
        emit lockoutChanged();
}
