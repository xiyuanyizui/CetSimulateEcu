#include "cansendthread.h"

#include <QTime>
#include <QFile>
#include <QDebug>

#define THREAD_SLEEP_US     (20)

CanSendThread::CanSendThread()
{
    m_chlist.clear();
    m_sendlist.clear();

    QFile file("./ControlCAN.dll");
    ControlCAN_dll_Size = file.size();
    qDebug() << "ControlCAN_dll_Size" << ControlCAN_dll_Size;
}

void CanSendThread::run()
{
    while (true) {
        usleep(THREAD_SLEEP_US);

        mutex.lock();
        if (m_sendlist.size() > 0) {
            if (ControlCAN_dll_Size < 40000) { // 36KB的库需要清缓存
                VCI_ClearBuffer(m_devtype, m_device, CAN_CH0);
                VCI_ClearBuffer(m_devtype, m_device, CAN_CH1);
            }
            for (int i = 0; i < m_sendlist.size(); ++i) {
                int ch = m_chlist.at(i);
                VCI_CAN_OBJ can = m_sendlist.at(i);
                
                emit signalSentCANData(ch, can);
                //qDebug("CanSendThread size:%d ch:%d canid:0x%08X", m_sendlist.size(), ch, can.ID);
                if (1 != VCI_Transmit(m_devtype, m_device, ch, &can, 1)) {
                    emit signalErrorInfo(tr("CAN%1 ID:0x%2 发送失败 !").arg(ch).arg(can.ID, 8, 16, QChar('0')));
                    if (i > 3) break;
                    continue;
                }
            }
            m_chlist.clear();
            m_sendlist.clear();
        }
        mutex.unlock();
    }
}


void CanSendThread::setCANDevice(int devtype, int device)
{
    m_devtype = devtype;
    m_device  = device;
}

void CanSendThread::slotSendCANData(int chanel, const VCI_CAN_OBJ &can)
{
    if (0x18FEF100 != can.ID && 0x0CF00400 != can.ID) {
        //qDebug("[send]frameid:0x%08X, data:%02X %02X %02X %02X %02X %02X %02X %02X",
        //    can.ID, can.Data[0], can.Data[1], can.Data[2], can.Data[3], can.Data[4],
        //    can.Data[5], can.Data[6], can.Data[7]);
    }

    mutex.lock();
    m_chlist << chanel;
    m_sendlist << can;
    //qDebug() << "currentTime" << QTime::currentTime() << "m_sendlist.size()" << m_sendlist.size();
    mutex.unlock();
}


