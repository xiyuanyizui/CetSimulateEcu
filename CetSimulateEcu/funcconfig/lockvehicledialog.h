#ifndef LOCKVEHICLEDIALOG_H
#define LOCKVEHICLEDIALOG_H

#include "ecuconfigdialog.h"
#include "lockdatabasedialog.h"

#include "includes.h"
#include "ControlCAN.h"
#include <QDialog>

namespace Ui {
class LockVehicleDialog;
}

class LockVehicleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LockVehicleDialog(QWidget *parent = 0);
    ~LockVehicleDialog();
    enum LockStep {
        LOCK_STEP_NULL = 0,
        LOCK_STEP_CHECK,
        LOCK_STEP_SEED,
        LOCK_STEP_KEY,
        LOCK_STEP_HANDSHAKE_ER,
        LOCK_STEP_HANDSHAKE_OK,
        LOCK_STEP_COMMUNICATE,
        LOCK_STEP_BIND,
        LOCK_STEP_UNBIND,
        LOCK_STEP_LOCK,
        LOCK_STEP_UNLOCK,
        LOCK_STEP_HEARTBEAT,
        LOCK_STEP_ACTIVE_MODE,
        LOCK_STEP_HANDSHAKE_MODE
    };

    enum LockStatus {
        LOCK_STATUS_INIT = 0,
        LOCK_STATUS_HANDSHAKING,
        LOCK_STATUS_HANDSHAKED,
        LOCK_STATUS_UNBINDING,
        LOCK_STATUS_UNBINDED,
        LOCK_STATUS_BINDING = 5,
        LOCK_STATUS_BINDED,
        LOCK_STATUS_LOCKING,
        LOCK_STATUS_LOCKED,
        LOCK_STATUS_UNLOCKING,
        LOCK_STATUS_UNLOCKED,
    };

    typedef void  ( LockVehicleDialog:: *init_t )(const QString &);
    typedef void  ( LockVehicleDialog:: *seed_t )(const QString &, const QString &);
    typedef QByteArray  ( LockVehicleDialog:: *key_t )(const QString &, const QByteArray &);
    typedef void  ( LockVehicleDialog:: *power_t )(bool);
    typedef void  ( LockVehicleDialog:: *timeout_t )(void);
    typedef void  ( LockVehicleDialog:: *stateTimeout_t )(void);
    typedef void  ( LockVehicleDialog:: *identifyTimeout_t )(void);
    typedef void  ( LockVehicleDialog:: *logic_t )(const QString &, const QString &);

    struct LockLogicAssemble {
        QString     ecuBrand;
        QString     engineVendor;
        init_t      init;
        seed_t      seed;
        key_t       key;
        power_t     power;
        timeout_t   timeout;
        stateTimeout_t stateTimeout;
        identifyTimeout_t identifyTimeout;
        logic_t     logic;
    };

public:
    void setEcuLockLogics(const QString &ecuBrand, const QString &engineVendor);
    void controlStateChanged(bool acc, bool ig, bool can);
    void lockVehicleCanEntry(const VCI_CAN_OBJ &can);
    QString vehicleVendor();
    
public slots:
    void showDialog();
    void setLockVehicleButton(bool enable);

signals:
    void signalSendCANData(int chanel, const VCI_CAN_OBJ &can);
    void signalEventInfo(const QString &info);
    void signalMaxRpmChanged(int maxRpm);

private slots:
    void doEcuTimeout();
    void doEcuStateTimeout();
    void doEcuIdentifyTimeout();

    void on_saveToDBButton_clicked();

    void on_openDBButton_clicked();

    void on_identifyCheckBox_stateChanged(int arg1);

    void on_stateCheckBox_stateChanged(int arg1);

    void on_vehicleVendorComboBox_currentTextChanged(const QString &arg1);

    void on_handshakeRecvCanidLineEdit_textChanged(const QString &arg1);

    void on_identifyPeriodSpinBox_valueChanged(int arg1);

    void on_statePeriodSpinBox_valueChanged(int arg1);

    void on_editCheckBox_clicked();

    void on_clearGpsidButton_clicked();

public:
    LockLogicAssemble getLockLogicAssemble(const QString &ecuBrand, 
                                    const QString &engineVendor);

    void updateLockLogicTable(const QString &locktable, const QString &vehicleVendor);
    void packetSendCanData(const QString &canid, const QString &data);
    void sendCanDataUInt8(const QString &canid, const uint8_t data[8]);

    QByteArray boschManKey(const QString &engineVendor, const QByteArray &seed);
    void boschManInit(const QString &vehicleVendor);
    void boschManPower(bool onOff);
    void boschManTimeout(void);
    void boschManLogic(const QString &frameid, const QString &data);

    void boschWeiChaiSeed(const QString &engineVendor, const QString &solidkey);
    void boschWeiChaiInit(const QString &vehicleVendor);
    void boschWeiChaiPower(bool onOff);
    void boschWeiChaiTimeout(void);
    void boschWeiChaiStateTimeout(void);
    void boschWeiChaiIdentifyTimeout(void);
    void boschWeiChaiLogic(const QString &frameid, const QString &data);

    void boschYuChaiSeed(const QString &engineVendor, const QString &gpsid);
    void boschYuChaiInit(const QString &vehicleVendor);
    void boschYuChaiPower(bool onOff);
    void boschYuChaiTimeout(void);
    void boschYuChaiStateTimeout(void);
    void boschYuChaiIdentifyTimeout(void);
    void boschYuChaiLogic(const QString &frameid, const QString &data);

    void boschKangMingSiInit(const QString &vehicleVendor);
    void boschKangMingSiPower(bool onOff);
    void boschKangMingSiTimeout(void);
    void boschKangMingSiStateTimeout(void);
    void boschKangMingSiIdentifyTimeout(void);
    void boschKangMingSiLogic(const QString &frameid, const QString &data);

    void boschYunNeiInit(const QString &vehicleVendor);
    void boschYunNeiPower(bool onOff);
    void boschYunNeiTimeout(void);
    void boschYunNeiStateTimeout(void);
    void boschYunNeiIdentifyTimeout(void);
    void boschYunNeiLogic(const QString &frameid, const QString &data);

private:
    bool boschManHandshake(const QString &frameid, const QString &data);
    bool boschWeiChaiHandshake(const QString &frameid, const QString &data);
    bool boschYuChaiHandshake(const QString &frameid, const QString &data);
    bool boschKangMingSiHandshake(const QString &frameid, const QString &data);
    bool boschYunNeiHandshake(const QString &frameid, const QString &data);

    void updateIdentifyDataValue(bool enable, const QString &text);
    void updateStateDataValue(bool enable, const QString &text);

private:
    Ui::LockVehicleDialog *ui;
    LockDatabaseDialog *lockdbdlg;

    QSqlQueryModel *model;
    
    LockStep lockStep;
    LockStatus lockStatus;

    bool hasPowerup;
    
    QTimer *ecuTimer;
    QTimer *ecuStateTimer;
    QTimer *ecuIdentifyTimer;

    LockLogicAssemble curLockLogic;
};

// 锁车逻辑集合
static const LockVehicleDialog::LockLogicAssemble lockLogicAssemble[] = {
    {   /* 博世 */
        ECUBRAND_BOSCH, ECUVENDOR_MAN,
        &LockVehicleDialog::boschManInit,
        nullptr,
        &LockVehicleDialog::boschManKey,
        &LockVehicleDialog::boschManPower,
        &LockVehicleDialog::boschManTimeout,
        nullptr,
        nullptr,
        &LockVehicleDialog::boschManLogic
    },
    { 
        ECUBRAND_BOSCH, ECUVENDOR_WEICHAI, 
        &LockVehicleDialog::boschWeiChaiInit,
        &LockVehicleDialog::boschWeiChaiSeed,
        nullptr, 
        &LockVehicleDialog::boschWeiChaiPower,
        &LockVehicleDialog::boschWeiChaiTimeout,
        &LockVehicleDialog::boschWeiChaiStateTimeout,
        &LockVehicleDialog::boschWeiChaiIdentifyTimeout,
        &LockVehicleDialog::boschWeiChaiLogic
    },
    { 
        ECUBRAND_BOSCH, ECUVENDOR_YUCHAI, 
        &LockVehicleDialog::boschYuChaiInit,
        &LockVehicleDialog::boschYuChaiSeed,
        nullptr,
        &LockVehicleDialog::boschYuChaiPower,
        &LockVehicleDialog::boschYuChaiTimeout,
        &LockVehicleDialog::boschYuChaiStateTimeout,
        &LockVehicleDialog::boschYuChaiIdentifyTimeout,
        &LockVehicleDialog::boschYuChaiLogic
    },
    { 
        ECUBRAND_BOSCH, ECUVENDOR_XICHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_BOSCH, ECUVENDOR_YUNNEI, 
        &LockVehicleDialog::boschYunNeiInit,
        nullptr,
        nullptr,
        &LockVehicleDialog::boschYunNeiPower,
        &LockVehicleDialog::boschYunNeiTimeout,
        &LockVehicleDialog::boschYunNeiStateTimeout,
        &LockVehicleDialog::boschYunNeiIdentifyTimeout,
        &LockVehicleDialog::boschYunNeiLogic,
    },
    { 
        ECUBRAND_BOSCH, ECUVENDOR_KANGMINGSI, 
        &LockVehicleDialog::boschKangMingSiInit,
        nullptr,
        nullptr,
        &LockVehicleDialog::boschKangMingSiPower,
        &LockVehicleDialog::boschKangMingSiTimeout,
        &LockVehicleDialog::boschKangMingSiStateTimeout,
        &LockVehicleDialog::boschKangMingSiIdentifyTimeout,
        &LockVehicleDialog::boschKangMingSiLogic
    },
    {   /* 电装 */
        ECUBRAND_DENSO, ECUVENDOR_MAN, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_DENSO, ECUVENDOR_WEICHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_DENSO, ECUVENDOR_YUCHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_DENSO, ECUVENDOR_XICHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_DENSO, ECUVENDOR_YUNNEI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    {   /* 大陆 */
        ECUBRAND_VDO, ECUVENDOR_MAN, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_VDO, ECUVENDOR_WEICHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_VDO, ECUVENDOR_YUCHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_VDO, ECUVENDOR_XICHAI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
    { 
        ECUBRAND_VDO, ECUVENDOR_YUNNEI, 
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    },
};

#endif // LOCKVEHICLEDIALOG_H
