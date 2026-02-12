#include "FileUpdateWatcher.h"
#include <QDebug>

FileUpdateWatcher::FileUpdateWatcher(const QString &dir, const QString &targetFile, QObject *parent)
    : QObject(parent), m_dir(dir), m_targetFile(targetFile)
{
    m_fullPath = m_dir + "/" + m_targetFile;

    connect(&m_timer, &QTimer::timeout, this, &FileUpdateWatcher::checkFile);
    m_timer.start(1000); // Check every 1s
}

void FileUpdateWatcher::checkFile()
{
    QFileInfo info(m_fullPath);
    if (info.exists() && info.isFile()) {
        if (info.size() != m_lastSize || info.lastModified() != m_lastModified) {
            m_lastSize = info.size();
            m_lastModified = info.lastModified();
            qDebug() << "File appeared or changed:" << m_fullPath;
            emit fileAppearedOrChanged(m_fullPath);
        }
    }
}
