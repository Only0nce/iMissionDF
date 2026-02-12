#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QDebug>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject *parent = nullptr);
    void downloadFile(const QString &url, const QString &outputPath);

signals:
    void downloadCompleted(const QString &outputPath);

private slots:
    void fileDownloaded(QNetworkReply *reply);

private:
    QString OutputPath;
    QNetworkAccessManager *manager;
    QFile file;
};

#endif // FILEDOWNLOADER_H
