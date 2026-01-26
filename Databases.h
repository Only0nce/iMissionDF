#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>


// =========================
//   ต้องอยู่ตรงนี้!
// =========================

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QString dbName, QString user, QString password, QString host, QObject *parent = nullptr);
    ~Database();
    bool database_createConnection();
    void restartMysql();
    void getFrequency();
    void updateFrequency();
    void insertFrequency();

    struct ScanCard
    {
        int         id;
        double      freq;
        QString     unit;
        QString     bw;
        QString     mode;
        int         low_cut;
        int         high_cut;
        QString     path;
        QDateTime   created_at;
        QDateTime   time;
    };

    void getAllScanCards();

public slots:
    void insertScanCard(double freq, const QString &unit, const QString &bw, const QString &mode, int lowCut, int highCut, const QString &path, QString time);
    void deleteScanCardAll();
    void deleteScanCardById(int id);
    void deleteScanCardGroup(const QString &groupDateTime);

signals:
    void initValue(QVector<ScanCard>);
    void initValueJson(const QJsonArray &jsonArray);  // <<<<< add

private:
    QString m_connName;
    QSqlDatabase db;
};

#endif // DATABASE_H
