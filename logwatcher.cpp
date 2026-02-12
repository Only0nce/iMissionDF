#include "logwatcher.h"
#include <QDebug>

LogWatcher::LogWatcher(QObject *parent) : QObject(parent)
{
    tailProcess = new QProcess(this);

    // Match: ALSA_ID[0]:[recin1], conn[4], state[RECORD]
    logRegex = QRegularExpression(R"(ALSA_ID\[\d+\]:\[(.*?)\], conn\[(\d+)\], state\[(.*?)\])");

    connect(tailProcess, &QProcess::readyReadStandardOutput,
            this, &LogWatcher::handleReadyRead);
}

void LogWatcher::startWatching(const QString &logFile)
{
    QStringList args = {"-f", logFile};
    tailProcess->start("tail", args);
    if (!tailProcess->waitForStarted()) {
        qWarning() << "Failed to start tail process";
    }
}

void LogWatcher::handleReadyRead()
{
    while (tailProcess->canReadLine()) {
        QString line = QString::fromUtf8(tailProcess->readLine()).trimmed();
        QRegularExpressionMatch match = logRegex.match(line);
        if (match.hasMatch()) {
            QString alsaId = match.captured(1);
            QString conn = match.captured(2);
            QString state = match.captured(3);
            emit stateChanged(alsaId, conn, state);
        }
    }
}
