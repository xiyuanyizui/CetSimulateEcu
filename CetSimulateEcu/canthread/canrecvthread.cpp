#include "canrecvthread.h"

#include <QDebug>

#define THREAD_SLEEP_US     (20)
#define RECV_MAX_NUM        (500)

CanRecvThread::CanRecvThread()
{

}

void CanRecvThread::run()
{
    VCI_ERR_INFO errInfo;
    VCI_CAN_OBJ data[RECV_MAX_NUM];
    ULONG num = 0;
    int len = 0;

    for (int ch = CAN_CH0; ch < CAN_CHX_MAX; ++ch) {
        // 开启CAN首次会有总线上之前的缓存数据 清空
        VCI_ClearBuffer(m_devtype, m_device, ch);
    }
    
    while (true) {
        usleep(THREAD_SLEEP_US);
        for (int ch = CAN_CH0; ch < CAN_CHX_MAX; ++ch) {
            num = VCI_GetReceiveNum(m_devtype, m_device, ch);
            if (num < 0) continue;

            len = VCI_Receive(m_devtype, m_device, ch, data, num, 50);
            if (len <= 0) {
                VCI_ReadErrInfo(m_devtype, m_device, ch, &errInfo);
                if (errInfo.ErrCode) {
                    emit signalCANErrorInfo(ch, errInfo);
                }
            } else {
                for (int i = 0; i < len; ++i) {
                    #if 0
                    qWarning("ch:%d, len:%d, i:%d, id:0x%08x, Data:%02x %02x %02x %02x", \
                        ch, len, i, data[i].ID, data[i].Data[0], data[i].Data[1],
                        data[i].Data[2], data[i].Data[3]);
                    #endif
                    
                    emit signalRecvCANData(ch, data[i]);
                }
            }
        }
    }
}

void CanRecvThread::setCANDevice(int devtype, int device)
{
    m_devtype = devtype;
    m_device  = device;
    //qDebug() << "CanRecvThread:m_devtype" << m_devtype << "m_device" << m_device;
}

