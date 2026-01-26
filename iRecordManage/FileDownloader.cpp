#include "FileDownloader.h"

FileDownloader::FileDownloader(QObject *parent)
    : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &FileDownloader::fileDownloaded);
}

void FileDownloader::downloadFile(const QString &url, const QString &outputPath)
{
    qDebug() << "[downloadFile] START";
    qDebug() << "[downloadFile] URL:" << url;
    qDebug() << "[downloadFile] Output path:" << outputPath;

    OutputPath = outputPath;
    qDebug() << "OutputPath:" << OutputPath;

    QNetworkRequest request(url);
    qDebug() << "QNetworkRequest:";

    QNetworkReply *reply = manager->get(request);
    qDebug() << "manager" << reply;

    if (!reply) {
        qDebug() << "[downloadFile] ERROR: reply is null";
        return;
    }

    qDebug() << "[downloadFile] QNetworkReply object created:" << reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, outputPath]() {
        qDebug() << "[downloadFile] reply->finished triggered";

        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[downloadFile] No error from reply";

            QByteArray fileData = reply->readAll();
            QFile file(outputPath);

            if (file.open(QIODevice::WriteOnly)) {
                qDebug() << "[downloadFile] File opened for writing";

                file.write(fileData);
                file.close();

                qDebug() << "[downloadFile] Download complete!";
                emit downloadCompleted(OutputPath);
            } else {
                qDebug() << "[downloadFile] ERROR: Failed to open file for writing!";
            }

        } else {
            qDebug() << "[downloadFile] ERROR: Download error:" << reply->errorString();
        }

        reply->deleteLater();
        qDebug() << "[downloadFile] reply deleted";
    });
}


void FileDownloader::fileDownloaded(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Download error:" << reply->errorString();
    } else {
        qDebug() << "Download complete!";

    }
}
