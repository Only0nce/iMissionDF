#ifndef GETINPUTEVENT_H
#define GETINPUTEVENT_H
#include <QKeyEvent>
#include <QObject>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <linux/fb.h>
#include <sys/mman.h>


class GetInputEvent : public QObject
{
    Q_OBJECT
public:
    explicit GetInputEvent(char *inputdev, int codedetect, int InputEventID, QObject *parent = nullptr);
    void run();
    void print_event(struct input_event *ie);
    int codeDetect;
    int GetInputEventID;
    char *inputevent;

signals:
    void eventCode(int evCode, int value, int InputEventID);
public slots:

private:
    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    pthread_t idThread;
};

#endif // GETINPUTEVENT_H
