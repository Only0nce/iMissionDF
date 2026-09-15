#ifndef LOGWATCHER_H
#define LOGWATCHER_H

#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>

class LogWatcher : public QObject
{
    Q_OBJECT
public:
    explicit LogWatcher(QObject *parent = nullptr);
    ~LogWatcher() override;

    void startWatching(const QString &logFile);
    void stopWatching();

signals:
    void stateChanged(const QString &alsaId, const QString &conn, const QString &state);

private slots:
    void handleReadyRead();

private:
    QProcess *tailProcess;
    QRegularExpression logRegex;
};

#endif // LOGWATCHER_H
