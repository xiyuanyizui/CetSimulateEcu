#ifndef FUNCCONFIGDIALOG_H
#define FUNCCONFIGDIALOG_H

#include "ControlCAN.h"

#include "lockvehicledialog.h"
#include "lockoperatedialog.h"

#include <QDialog>

namespace Ui {
class FuncConfigDialog;
}

class FuncConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FuncConfigDialog(QWidget *parent = 0);
    ~FuncConfigDialog();
    
    enum FuncType {
        FUNC_LOCK,
        FUNC_TIRED,
    };
    Q_ENUM(FuncType)

public:
    void vehicleCanDataEntry(const VCI_CAN_OBJ &can);
    void tiredDriveSecEntry(bool havecan, double speed);
    int maxEcuLockRpm();
    void tiredDriveParam(int *runSeconds, int *restSeconds);

public slots:
    void slotControlStateChanged(bool acc, bool ig, bool can);
    void slotEcuTypeChanged(const QString &ecuBrand, const QString &engineVendor);

signals:
    void signalSendCANData(int chanel, const VCI_CAN_OBJ &can);
    void signalEventInfo(FuncType type, const QString &info);
    void signalFuncTypeChanged(FuncType type, const QString &info, bool checked);

private slots:
    void doEventInfo(const QString &info);
    void doFuncConfigChanged(bool checked);
    void doMaxRpmChanged(int maxRpm);

    void on_activeRadioButton_clicked(bool checked);

    void on_handshakeRadioButton_clicked(bool checked);

    void on_decryptButton_clicked();

    void on_clearInputButton_clicked();

private:
    QString usetime(int timestamp);

    Ui::FuncConfigDialog *ui;
    int m_maxEcuRpm;
    
    // 疲劳驾驶
    int m_runSeconds;
    int m_restSeconds;
    
    LockVehicleDialog *lvDlg;
    LockOperateDialog *loDlg;
};

#endif // FUNCCONFIGDIALOG_H
