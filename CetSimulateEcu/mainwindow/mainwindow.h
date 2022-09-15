#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "includes.h"
#include "ControlCAN.h"

#include "ecuconfigdialog.h"
#include "funcconfigdialog.h"
#include "canconfigdialog.h"
#include "cansendthread.h"
#include "canrecvthread.h"
#include "canidfilterdialog.h"
#include "canoperatedialog.h"
#include "cetlicenseinterface.h"
#include "cetupdateinterface.h"
#include "cetcaninterface.h"

#include <QMainWindow>
#include <QDateTime>

/* 开启调试重定向到文件 */
#define ENABLE_DEBUG_TOFILE     (1)

namespace Ui {
class MainWindow;
}

class QTableView;
class QStandardItemModel;
class QMessageBox;
class QDockWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void initDllPlugin();

protected:
    void keyPressEvent(QKeyEvent *event);

signals:
    void signalControlState(bool acc, bool ig, bool can);
    
private slots:
    void doCANDevice(const QString &type, int devtype, int devid);
    void doEcuTypeChanged(const QString &ecuBrand, const QString &engineVendor);
    void doSecTimeout();
    void doMSecTimeout();
    void doFuncTypeChanged(FuncConfigDialog::FuncType type, const QString &info, bool checked);
    void doRecvEventInfoShow(FuncConfigDialog::FuncType type, const QString &info);
    void doRecvCANDataShow(int chanel, const VCI_CAN_OBJ &can);
    void doRecvCANErrorShow(int chanel, const VCI_ERR_INFO &err);
    void doSentCANDataShow(int chanel, const VCI_CAN_OBJ &can);
    void doSentErrorInfoShow(const QString &info);
    void doControlStateChanged();

    void on_eventInfoClearButton_clicked();

    void on_can0DataClearButton_clicked();

    void on_eventInfoSaveButton_clicked();

    void on_can0DataSaveButton_clicked();

    void on_accSwitchButton_clicked();

    void on_igSwitchButton_clicked();

    void on_canStartButton_clicked();

    void on_eventInfoPauseButton_clicked();

    void on_can0DataPauseButton_clicked();

    void on_baseRpmVerticalSlider_sliderMoved(int position);

    void on_logToFileCheckBox_clicked(bool checked);

    void on_eventVisibleButton_clicked();

    void on_can1DataClearButton_clicked();

    void on_can1DataPauseButton_clicked();

    void on_can1DataSaveButton_clicked();

    void on_canOperateAction_triggered();

    void on_can0DataFilterButton_clicked();

    void on_can1DataFilterButton_clicked();

private:
    QObject *loadDllPlugin(const QString &dllname);
    void setEventInfoViewHeader();
    void setCANDataViewHeader(QTableView *tableView, QStandardItemModel *model);
    void insertCANDataView(QTableView *tableView, const QDateTime &dateTime, 
                                bool isSend, const VCI_CAN_OBJ &can);
    void slaveCANDataToFile(int chanel, QStandardItemModel *model);
    QString usetime(int timestamp);
    
private:
    Ui::MainWindow *ui;
    int m_baseRpm;

    CetLicenseInterface *m_cetLicIface;
    CetUpdateInterface *m_cetUpIface;
    CetCANInterface *m_cetCanIface;

    EcuConfigDialog *ecDlg;
    CanConfigDialog *ccDlg;
    FuncConfigDialog *fcDlg;
    CanOperateDialog *coDlg;

    CanIdFilterDialog *c0ifDlg;
    CanIdFilterDialog *c1ifDlg;

    CanRecvThread *crThread;
    CanSendThread *csThread;
    QStandardItemModel *can0DataModel;
    QStandardItemModel *can1DataModel;
    QStandardItemModel *eventModel;

    QTimer *secTimer;
    QTimer *msecTimer;
};

#endif // MAINWINDOW_H
