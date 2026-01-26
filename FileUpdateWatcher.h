#ifndef FILEUPDATEWATCHER_H
#define FILEUPDATEWATCHER_H

#include "qdatetime.h"
#include <QObject>
#include <QTimer>
#include <QFileInfo>

class FileUpdateWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileUpdateWatcher(const QString &dir, const QString &targetFile, QObject *parent = nullptr);

signals:
    void fileAppearedOrChanged(const QString &path);

private slots:
    void checkFile();

private:
    QString m_dir;
    QString m_targetFile;
    QString m_fullPath;
    qint64 m_lastSize = -1;
    QDateTime m_lastModified;
    QTimer m_timer;
};

#endif // FILEUPDATEWATCHER_H
