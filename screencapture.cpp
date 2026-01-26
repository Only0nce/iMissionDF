#include <QPixmap>
#include <QQuickView>
#include <QString>

#include "screencapture.h"
#include "QDateTime"

ImageProvider::ImageProvider(QObject *parent, Flags flags) :
    QQuickImageProvider(QQmlImageProviderBase::Image, flags),
    QObject(parent)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool ok = false;
    QImage img = m_images[id.toInt(&ok)];
    return img;
}

void ImageProvider::makeScreenshot()
{
    QString fileName = QString::number(QDateTime::currentDateTime().toSecsSinceEpoch());
    QQuickWindow *view = static_cast<QQuickWindow *>(sender());
    static int pos = 0;
    QImage img = view->grabWindow();
    m_images.insert(pos,img);

    QVariant returnedValue;
    QMetaObject::invokeMethod(view, "setImage",
                              Q_RETURN_ARG(QVariant, returnedValue),
                              Q_ARG(QVariant, pos++));

    fileName = ("/var/www/html/image/" + fileName + ".png");
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        img.save(&file, "PNG");
    }
    else {
        qDebug() << "Can't open file: " << fileName;
    }

    qDebug() << "makeScreenshot";
}
