#ifndef CANCONFIGDIALOG_H
#define CANCONFIGDIALOG_H

#include "includes.h"
#include "ControlCAN.h"
#include <QDialog>

namespace Ui {
class CanConfigDialog;
}

class CanConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CanConfigDialog(QWidget *parent = 0);
    ~CanConfigDialog();

    int openCANDevice(void);
    int closeCANDevice(void);

    void sendRpmCANData(int rpm);
    void sendSpeedCANData(double speed);
    bool isBrake(int &minRpm);
    
signals:
    void signalCANDevice(const QString &type, int devtype, int devid);
    void signalSendCANData(int chanel, const VCI_CAN_OBJ &can);

public slots:
    void slotShowDialog(void);
    void slotSendCANData(int chanel, const QString &canid, const QString &data);
    void slotOkButtonPressed(void);
    
private slots:
    void doCurrentIndex0Changed(int index);
    void doCurrentIndex1Changed(int index);
    
private:
    Ui::CanConfigDialog *ui;

    // CAN的配置参数
    int m_devtypeIndex;
    int m_deviceIndex;
    typedef struct {
        int     chanelIndex;
        QString accessCode;
        QString maskCode;
        int     filterType;
        int     workMode;
        int     baudrateId;
    } CAN_PARAM_T;
    QList<CAN_PARAM_T> m_canParamList;
};

#endif // CANCONFIGDIALOG_H
