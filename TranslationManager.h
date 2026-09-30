// TranslationManager.h
// iScanMR10 R1.7.0 - Qt Linguist runtime manager + glossary fallback
#pragma once

#include <QObject>
#include <QTranslator>
#include <QHash>
#include <QString>

class TranslationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    explicit TranslationManager(QObject *parent = nullptr);

    QString language() const;

    Q_INVOKABLE bool setLanguage(const QString &languageCode);
    Q_INVOKABLE QString text(const QString &source) const;

signals:
    void languageChanged();

private:
    bool loadQtTranslator(const QString &languageCode);
    static const QHash<QString, QString> &thaiGlossary();

    QString m_language;
    QTranslator m_translator;
};
