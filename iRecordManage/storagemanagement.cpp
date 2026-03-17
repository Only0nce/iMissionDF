#include "storagemanagement.h"

StorageManagement::StorageManagement(QObject *parent)
{

}


void StorageManagement::checkDiskAndFormat(QString msg) {
    qDebug() << "checkDiskAndFormat: received msg";

    QByteArray br = msg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);

    if (!doc.isArray()) {
        qWarning() << "Invalid JSON format: not an array";
        return;
    }

    QJsonArray arr = doc.array();
    int deletedCount = 0;

    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        QString device = QString::number(obj["device"].toInt());
        QString filename = obj["filename"].toString();
        QString file_path = obj["file_path"].toString();



        QStringList parts = filename.split('_');
        if (parts.size() >= 4) {
            QString frequency = parts[0];        // "121100"
            QString date = parts[1];             // "20250711"
            QString device = parts[2];           // "3"

            // สร้าง path เต็ม
            QString fullPath = QString("%1/%2/%3/%4/%5")
                                   .arg(file_path)
                                   .arg(frequency)
                                   .arg(date)
                                   .arg(device)
                                   .arg(filename);
            qDebug() << " -> Device:" << device
                     << "Filename:" << filename
                     << "FullPath:" << fullPath;
            // ลบไฟล์
            if (QFile::exists(fullPath)) {
                if (QFile::remove(fullPath)) {
                    qDebug() << "Deleted file:" << fullPath;
                } else {
                    qWarning() << "Failed to delete file:" << fullPath;
                }
            } else {
                qWarning() << "File does not exist:" << fullPath;
            }
        } else {
            qWarning() << "Invalid filename format:" << filename;
        }

    }

    qDebug() << "Total files deleted:" << deletedCount;
}
