#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QQuickView>
#include <QImage>

class ImageProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    ImageProvider(QObject *parent = 0, Flags flags = Flags());
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    Q_INVOKABLE void makeScreenshot();

protected:
    QMap<int, QImage> m_images;

public slots:

};

#endif // IMAGEPROVIDER_H

