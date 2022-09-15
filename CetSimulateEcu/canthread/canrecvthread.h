#ifndef CANRECVTHREAD_H
#define CANRECVTHREAD_H

#include "includes.h"
#include "ControlCAN.h"
#include <QThread>

class CanRecvThread : public QThread
{
    Q_OBJECT
    
public:
    CanRecvThread();

protected:
    void run();

public slots:
    void setCANDevice(int devtype, int device);

signals:
    void signalRecvCANData(int chanel, const VCI_CAN_OBJ &can);
    void signalCANErrorInfo(int chanel, const VCI_ERR_INFO &err);

private:
    // CAN参数配置
    DWORD m_devtype;
    DWORD m_device;
};

#endif // CANRECVTHREAD_H