#ifndef DATASTORAGE_H
#define DATASTORAGE_H
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QSqlQuery>
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>
#include <QtSql>
#include <QSqlDatabase>
#include <QSqlQueryModel>
//#include <QQmlApplicationEngine>
#include <QVariant>
#include "Database.h"

class DataStorage : public QObject
{
    Q_OBJECT

    public slots:
        void scanFiles();
        void saveCsvFileAndUpdateDb(const QString &fileName, const QString &category, const QDate &date);
        void deleteCsvFileAndFolder(const QString &fileName, const QString &category, const QDate &date);
        void renameFileAndUpdateDb(const QString &oldFileName, const QString &newFileName, const QString &category, const QDate &date);
        void updateQuery(const QString &queryStr);
        void searchByName(const QString &name);
        void searchByDate(const QString &date);
        void sortByName(bool ascending);
        void sortByDate(bool descending);
        void RecalculateWithMargin(QString msg);


    public:
        explicit DataStorage(QObject *parent = nullptr);
        static DataStorage *instance();
        int getCategoryId(const QString &category);


        // void scanFiles();
        // void saveCsvFileAndUpdateDb(const QString &fileName, const QString &category, const QDate &date);
        // void deleteCsvFileAndFolder(const QString &fileName, const QString &category, const QDate &date);
        // void renameFileAndUpdateDb(const QString &oldFileName, const QString &newFileName, const QString &category, const QDate &date);
        // void updateQuery(const QString &queryStr);
        // void searchByName(const QString &name);
        // void searchByDate(const QString &date);
        // void sortByName(bool ascending);
        // void sortByDate(bool descending);

    private:
        QSqlDatabase m_db;
        QSqlDatabase db;
        QSqlQueryModel *queryModel;
//        QQmlApplicationEngine engine;

};

#endif // DATASTORAGE_H
