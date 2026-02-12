#ifndef IMAGEPROVIDERDF_H
#define IMAGEPROVIDERDF_H

#include <QQuickImageProvider>
#include <QQuickView>
#include <QImage>
#include <QMap>
#include <QObject>

// #include "Mainwindows.h"

class ImageProviderDF : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    ImageProviderDF(QObject *parent = 0, Flags flags = Flags());
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);

    void Gethost(const QString &ip);
    void partImage(const QString &Image);

public slots:
    void makeScreenshot();

signals:
    void sendpartImage(const QString &image);

protected:
    QMap<int, QImage> m_images;
    static QString iphost;
};


#endif // IMAGEPROVIDERDF_H
