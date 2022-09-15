#include "lockoperatedialog.h"
#include "ui_lockoperatedialog.h"

#include <QDebug>

LockOperateDialog::LockOperateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LockOperateDialog)
{
    ui->setupUi(this);

    ui->lockTriggerCanidLineEdit->setInputMask("HHHHHHHH");
    ui->lockTriggerDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->GPSIDLineEdit->setInputMask("HHHHHHHHHHHH");
    ui->solidkeyLineEdit->setInputMask("HHHHHHHHHHHH");
}

LockOperateDialog::~LockOperateDialog()
{
    delete ui;
}

void LockOperateDialog::assembleSendCANData(int chanel, const QString &canid, const QString &data)
{
    VCI_CAN_OBJ can;
    memset(&can, 0, sizeof(VCI_CAN_OBJ));

    //qDebug() << "canid" << canid << "data" << data;
    
    can.ID         = canid.toUInt(nullptr, 16);
    can.SendType   = 0;
    can.ExternFlag = (can.ID > 0xFFFF) ? 1 : 0;
    can.RemoteFlag = 0;

    QString send = data;
    if (send.length() < 16) {
        send.append(QString(16 - send.length(), QChar('0')));
    }
    send.replace(QLatin1String(" "), QLatin1String(""));
    int count = 0;
    for (int i = 0; i < send.length(); i += 2) {
        QString byte = send.mid(i, 2);
        can.Data[count++] = byte.toInt(nullptr, 16);
    }
    can.DataLen = count;

    emit signalSendCANData(chanel, can);
}


void LockOperateDialog::on_bindButton_clicked()
{
    QString canid = ui->lockTriggerCanidLineEdit->text();
    QString data = ui->lockTriggerDataLineEdit->text();

    data.replace(0, 2, "01");
    assembleSendCANData(CAN_CH0, canid, data);
    ui->lockTriggerDataLineEdit->setText(data);
}

void LockOperateDialog::on_unbindButton_clicked()
{
    QString canid = ui->lockTriggerCanidLineEdit->text();
    QString data = ui->lockTriggerDataLineEdit->text();

    data.replace(0, 2, "02");
    assembleSendCANData(CAN_CH0, canid, data);
    ui->lockTriggerDataLineEdit->setText(data);
}

void LockOperateDialog::on_unlockButton_clicked()
{
    QString canid = ui->lockTriggerCanidLineEdit->text();
    QString data = ui->lockTriggerDataLineEdit->text();

    data.replace(0, 2, "03");
    assembleSendCANData(CAN_CH0, canid, data);
    ui->lockTriggerDataLineEdit->setText(data);
}

void LockOperateDialog::on_lockButton_clicked()
{
    QString canid = ui->lockTriggerCanidLineEdit->text();
    QString data = ui->lockTriggerDataLineEdit->text();

    data.replace(0, 2, "04");
    assembleSendCANData(CAN_CH0, canid, data);
    ui->lockTriggerDataLineEdit->setText(data);
}

void LockOperateDialog::on_assignGPSIDCheckBox_clicked(bool checked)
{
    QString gpsid = ui->GPSIDLineEdit->text();
    int gpsidlen = gpsid.length();

    if (!checked) {
        gpsid.fill('0', gpsidlen);
    }

    //qDebug() << "gpsidlen" << gpsidlen << "gpsid" << gpsid;
    QString data = ui->lockTriggerDataLineEdit->text().replace(" ", "");
    data.replace(4, gpsidlen, gpsid);
    qDebug() << "data" << data;
    ui->lockTriggerDataLineEdit->setText(data);
}

void LockOperateDialog::on_solidkeyCheckBox_clicked(bool checked)
{
    QString solidkey = ui->solidkeyLineEdit->text();
    int gpsidlen = ui->GPSIDLineEdit->text().length();
    int solidkeylen = solidkey.length();

    if (!checked) {
        solidkey.fill('0', solidkeylen);
    }

    //qDebug() << "solidkeylen" << solidkeylen << "solidkey" << solidkey;
    QString data = ui->lockTriggerDataLineEdit->text().replace(" ", "");
    data.replace(4 + gpsidlen, solidkeylen, solidkey);
    ui->lockTriggerDataLineEdit->setText(data);
}

