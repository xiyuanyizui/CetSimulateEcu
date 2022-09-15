#ifndef LOCKOPERATEDIALOG_H
#define LOCKOPERATEDIALOG_H

#include "includes.h"
#include "ControlCAN.h"
#include <QDialog>

namespace Ui {
class LockOperateDialog;
}

class LockOperateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LockOperateDialog(QWidget *parent = nullptr);
    ~LockOperateDialog();

signals:
    void signalSendCANData(int chanel, const VCI_CAN_OBJ &can);

private:
    void assembleSendCANData(int chanel, const QString &canid, const QString &data);
    
private slots:
    void on_bindButton_clicked();

    void on_unbindButton_clicked();

    void on_unlockButton_clicked();

    void on_lockButton_clicked();

    void on_assignGPSIDCheckBox_clicked(bool checked);

    void on_solidkeyCheckBox_clicked(bool checked);

private:
    Ui::LockOperateDialog *ui;
};

#endif // LOCKOPERATEDIALOG_H
