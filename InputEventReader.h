#ifndef INPUTEVENTREADER_H
#define INPUTEVENTREADER_H

#include <QObject>
#include <QSocketNotifier>
#include <linux/input.h>

class InputEventReader : public QObject
{
    Q_OBJECT
public:
    explicit InputEventReader(const QString &devicePath, QObject *parent = nullptr);
    ~InputEventReader();

signals:
    void rotaryTurned(int direction); // 1 = CCW, -1 = CW
    void keyPressed(int keyCode);
    void keyReleased(int keyCode);

private slots:
    void onReadyRead();

private:
    int m_fd;
    QSocketNotifier *m_notifier;
};

#endif // INPUTEVENTREADER_H
