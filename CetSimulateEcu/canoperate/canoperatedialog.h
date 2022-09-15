#ifndef CANOPERATEDIALOG_H
#define CANOPERATEDIALOG_H

#include "includes.h"
#include "ControlCAN.h"

#include <QDialog>
#include <QFile>

namespace Ui {
class CanOperateDialog;
}

class CanOperateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CanOperateDialog(QWidget *parent = nullptr);
    ~CanOperateDialog();

signals:
    void signalSendCANData(int chanel, const VCI_CAN_OBJ &can);

private:
    bool isCanDataValid(const QByteArray &data, int &chanel, QString &canid, QString &candata);
    bool isTimeoutValid(const QByteArray &data, int &val);

private slots:
    void doFileSendTimeout();
    void doPeriodCheckBox(bool checked);
    void doPeriodTimeout();
    void doSendTimeout();

    void on_openFileToolButton_clicked();

    void on_playButton_clicked();

    void on_sendDataButton_clicked();

    void on_playTypeComboBox_currentIndexChanged(const QString &arg1);

    void on_sendDataLineEdit_textChanged(const QString &arg1);

public slots:
    void slotControlStateChanged(bool acc, bool ig, bool can);
    void slotSendCANData(int chanel, const QString &canid, const QString &data);

private:
    Ui::CanOperateDialog *ui;

    int m_sendCount;
    int m_chanel;
    VCI_CAN_OBJ m_can;
    QTimer *m_sendTimer;
    QTimer *m_periodTimer;
    QTimer *m_fileSendTimer;
    QFile m_fileSend;
};

#endif // CANOPERATEDIALOG_H
