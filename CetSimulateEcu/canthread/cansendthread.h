#ifndef CANSENDTHREAD_H
#define CANSENDTHREAD_H

#include "includes.h"
#include "ControlCAN.h"
#include <QThread>
#include <QMutex>

class CanSendThread : public QThread
{
    Q_OBJECT
    
public:
    CanSendThread();
    void setCANDevice(int devtype, int device);

protected:
    void run();

public slots:
    // 待发送
    void slotSendCANData(int chanel, const VCI_CAN_OBJ &can);

signals:
    // 发送完成
    void signalSentCANData(int chanel, const VCI_CAN_OBJ &can);
    void signalErrorInfo(const QString &info);

private:
    DWORD m_devtype;
    DWORD m_device;

    QList<DWORD> m_chlist;
    QList<VCI_CAN_OBJ> m_sendlist;

    int ControlCAN_dll_Size;
    bool isneedsend = false;
    QMutex mutex;
};

#endif // CANSENDTHREAD_H
