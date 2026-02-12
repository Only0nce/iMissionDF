#include <QPixmap>
#include <QQuickWindow>
#include <QString>
#include <QFile>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include "ImageProviderDF.h"

ImageProviderDF::ImageProviderDF(QObject *parent, Flags flags)
    : QQuickImageProvider(QQmlImageProviderBase::Image, flags),
    QObject(parent)
{
}

QString ImageProviderDF::iphost = "";

QImage ImageProviderDF::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id);
    Q_UNUSED(size);
    Q_UNUSED(requestedSize);

    // ถ้าไม่ได้ใช้ ImageProvider จริง ๆ → return รูปเปล่า
    return QImage();
}

void ImageProviderDF::Gethost(const QString &ip)
{
    iphost = ip;
}

void ImageProviderDF::makeScreenshot()
{
    if (iphost.isEmpty()) {
        qWarning() << "iphost is empty! Cannot send image.";
        return;
    }

    // ลบไฟล์เก่าก่อน
    QDir tmpDir("/tmp");
    QStringList oldPngs = tmpDir.entryList(QStringList() << "*.png", QDir::Files);
    for (const QString &file : oldPngs) {
        tmpDir.remove(file);
    }

    QString fileName = QString::number(QDateTime::currentDateTime().toSecsSinceEpoch()) + ".png";
    QString fullPath = "/var/www/html/image " + fileName;

    QQuickWindow *view = qobject_cast<QQuickWindow *>(sender());
    if (!view) {
        qWarning() << "Sender is not a QQuickWindow!";
        return;
    }

    // ลดขนาด screenshot → ประหยัด RAM
    QImage img = view->grabWindow().scaled(1920, 1080, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        img.save(&file, "PNG");
        file.close();
        qDebug() << "Screenshot saved to:" << fullPath;

        // เคลียร์ QImage จาก RAM ทันที
        img = QImage();

        // Upload via curl
        QString url = QString("http://%1/uploadPic.php").arg(iphost);
        QString command = QString(R"(curl -X POST -F "file=@%1" "%2")")
                              .arg(fullPath)
                              .arg(url);

        int exitCode = QProcess::execute("/bin/sh", {"-c", command});
        partImage(fileName);

        if (exitCode == 0) {
            qDebug() << "Screenshot uploaded successfully via curl.";
        } else {
            qWarning() << "Curl upload failed with exit code:" << exitCode;
        }
    } else {
        qWarning() << "Can't open file for saving screenshot:" << fullPath;
    }

    qDebug() << "makeScreenshot completed.";
}

void ImageProviderDF::partImage(const QString &image)
{
    qDebug() << "partImage" << image;
    emit sendpartImage(image);
}
