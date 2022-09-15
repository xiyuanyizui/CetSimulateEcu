#include "canconfigdialog.h"
#include "ui_canconfigdialog.h"

#include "lockvehicledialog.h"

#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include <QDebug>

static const QString g_tim0[10]=
{
    "00",       // 1000Kbps
    "00",       // 800Kbps
    "00",       // 500Kbps
    "01",       // 250Kbps
    "03",       // 125Kbps
    "04",       // 100Kbps
    "09",       // 50Kbps
    "18",       // 20Kbps
    "31",       // 10Kbps
    "BF"        // 5Kbps
};

static const QString g_tim1[10]=
{
    "14",       // 1000Kbps
    "16",       // 800Kbps
    "1C",       // 500Kbps
    "1C",       // 250Kbps
    "1C",       // 125Kbps
    "1C",       // 100Kbps
    "1C",       // 50Kbps
    "1C",       // 20Kbps
    "1C",       // 10Kbps
    "FF"        // 5Kbps
};

CanConfigDialog::CanConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CanConfigDialog)
{
    ui->setupUi(this);

#if 1
    ui->devComboBox->addItem(tr("VCI_USBCAN1(I+)"), VCI_USBCAN1);
    ui->devComboBox->addItem(tr("VCI_USBCAN2(II+)"), VCI_USBCAN2);
    ui->devComboBox->addItem(tr("VCI_USBCAN2A"), VCI_USBCAN2A);
    ui->devComboBox->addItem(tr("VCI_USBCAN_E_U"), VCI_USBCAN_E_U);
    ui->devComboBox->addItem(tr("VCI_USBCAN_2E_U"), VCI_USBCAN_2E_U);
#else
    ui->devComboBox->addItem(tr("VCI_PCI5121"), VCI_PCI5121);
    ui->devComboBox->addItem(tr("VCI_PCI9810"), VCI_PCI9810);
    ui->devComboBox->addItem(tr("VCI_USBCAN1"), VCI_USBCAN1);
    ui->devComboBox->addItem(tr("VCI_USBCAN2"), VCI_USBCAN2);
    ui->devComboBox->addItem(tr("VCI_USBCAN2A"), VCI_USBCAN2A);
    ui->devComboBox->addItem(tr("VCI_PCI9820"), VCI_PCI9820);
    ui->devComboBox->addItem(tr("VCI_CAN232"), VCI_CAN232);
    ui->devComboBox->addItem(tr("VCI_PCI5110"), VCI_PCI5110);
    ui->devComboBox->addItem(tr("VCI_CANLITE"), VCI_CANLITE);
    ui->devComboBox->addItem(tr("VCI_ISA9620"), VCI_ISA9620);
    ui->devComboBox->addItem(tr("VCI_ISA5420"), VCI_ISA5420);
    ui->devComboBox->addItem(tr("VCI_PC104CAN"), VCI_PC104CAN);
    ui->devComboBox->addItem(tr("VCI_CANETUDP"), VCI_CANETUDP);
    ui->devComboBox->addItem(tr("VCI_CANETE"), VCI_CANETE);
    ui->devComboBox->addItem(tr("VCI_DNP9810"), VCI_DNP9810);
    ui->devComboBox->addItem(tr("VCI_PCI9840"), VCI_PCI9840);
    ui->devComboBox->addItem(tr("VCI_PC104CAN2"), VCI_PC104CAN2);
    ui->devComboBox->addItem(tr("VCI_PCI9820I"), VCI_PCI9820I);
    ui->devComboBox->addItem(tr("VCI_CANETTCP"), VCI_CANETTCP);
    ui->devComboBox->addItem(tr("VCI_PEC9920"), VCI_PEC9920);
    ui->devComboBox->addItem(tr("VCI_PCIE_9220"), VCI_PCIE_9220);
    ui->devComboBox->addItem(tr("VCI_PCI5010U"), VCI_PCI5010U);
    ui->devComboBox->addItem(tr("VCI_USBCAN_E_U"), VCI_USBCAN_E_U);
    ui->devComboBox->addItem(tr("VCI_USBCAN_2E_U"), VCI_USBCAN_2E_U);
    ui->devComboBox->addItem(tr("VCI_PCI5020U"), VCI_PCI5020U);
    ui->devComboBox->addItem(tr("VCI_EG20T_CAN"), VCI_EG20T_CAN);
    ui->devComboBox->addItem(tr("VCI_PCIE9221"), VCI_PCIE9221);
    ui->devComboBox->addItem(tr("VCI_WIFICAN_TCP"), VCI_WIFICAN_TCP);
    ui->devComboBox->addItem(tr("VCI_WIFICAN_UDP"), VCI_WIFICAN_UDP);
    ui->devComboBox->addItem(tr("VCI_PCIe9120"), VCI_PCIe9120);
    ui->devComboBox->addItem(tr("VCI_PCIe9110"), VCI_PCIe9110);
    ui->devComboBox->addItem(tr("VCI_PCIe9140"), VCI_PCIe9140);
#endif

    ui->timer00LineEdit->setEnabled(false);
    ui->timer01LineEdit->setEnabled(false);
    ui->timer10LineEdit->setEnabled(false);
    ui->timer11LineEdit->setEnabled(false);
    ui->accessCode0LineEdit->setInputMask("HHHHHHHH");
    ui->maskCode0LineEdit->setInputMask("HHHHHHHH");
    ui->accessCode1LineEdit->setInputMask("HHHHHHHH");
    ui->maskCode1LineEdit->setInputMask("HHHHHHHH");

    ui->rpmCanidLineEdit->setEnabled(false);
    ui->speedCanidLineEdit->setEnabled(false);
    ui->rpmCanidLineEdit->setInputMask("HHHHHHHH");
    ui->speedCanidLineEdit->setInputMask("HHHHHHHH");
    ui->rpmDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->speedDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");

    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.beginGroup(QLatin1String("CanParamCfg"));
    m_devtypeIndex = settings.value("devtypeIndex").toInt();
    m_deviceIndex = settings.value("deviceIndex").toInt();
    m_canParamList.clear();
    for (int i = CAN_CH0; i < CAN_CHX_MAX; ++i) {
        CAN_PARAM_T param;
        param.chanelIndex   = settings.value(tr("chanelIndex%1").arg(i), i).toInt();
        param.accessCode    = settings.value(tr("accessCode%1").arg(i), "00000000").toString();
        param.maskCode      = settings.value(tr("maskCode%1").arg(i), "FFFFFFFF").toString();
        param.filterType    = settings.value(tr("filterType%1").arg(i), 1).toInt();
        param.workMode      = settings.value(tr("workMode%1").arg(i), 0).toInt();
        param.baudrateId    = settings.value(tr("baudrateId%1").arg(i), 0).toInt();
        m_canParamList << param;
    }
    settings.endGroup();

    connect(ui->baudrate0ComboBox, static_cast<void (QComboBox:: *)(int index)>
        (&QComboBox::currentIndexChanged), this, &CanConfigDialog::doCurrentIndex0Changed);
    connect(ui->baudrate1ComboBox, static_cast<void (QComboBox:: *)(int index)>
        (&QComboBox::currentIndexChanged), this, &CanConfigDialog::doCurrentIndex1Changed);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CanConfigDialog::slotOkButtonPressed);

    ui->devComboBox->setCurrentIndex(m_devtypeIndex);
    ui->indexComboBox->setCurrentIndex(m_deviceIndex);
    
    CAN_PARAM_T param0 = m_canParamList.at(CAN_CH0);
    ui->accessCode0LineEdit->setText(param0.accessCode);
    ui->maskCode0LineEdit->setText(param0.maskCode);
    ui->filterType0ComboBox->setCurrentIndex(param0.filterType);
    ui->workMode0ComboBox->setCurrentIndex(param0.workMode);
    ui->baudrate0ComboBox->setCurrentIndex(param0.baudrateId);
    
    CAN_PARAM_T param1 = m_canParamList.at(CAN_CH1);
    ui->accessCode1LineEdit->setText(param1.accessCode);
    ui->maskCode1LineEdit->setText(param1.maskCode);
    ui->filterType1ComboBox->setCurrentIndex(param1.filterType);
    ui->workMode1ComboBox->setCurrentIndex(param1.workMode);
    ui->baudrate1ComboBox->setCurrentIndex(param1.baudrateId);
}

CanConfigDialog::~CanConfigDialog()
{
    delete ui;
}

void CanConfigDialog::doCurrentIndex0Changed(int index)
{
    if (index < 0 || index >= 10)
        return;

    ui->timer00LineEdit->setText(g_tim0[index]);
    ui->timer01LineEdit->setText(g_tim1[index]);
}

void CanConfigDialog::doCurrentIndex1Changed(int index)
{
    if (index < 0 || index >= 10)
        return;

    ui->timer10LineEdit->setText(g_tim0[index]);
    ui->timer11LineEdit->setText(g_tim1[index]);
}

int CanConfigDialog::openCANDevice(void)
{
    DWORD m_devtype = ui->devComboBox->itemData(m_devtypeIndex).toInt();
    /* 打开设备 */
    if (STATUS_OK != VCI_OpenDevice(m_devtype, m_deviceIndex, 0)) {
        return -1;
    }
    
    for (int i = CAN_CH0; i < CAN_CHX_MAX; ++i) {
        CAN_PARAM_T param = m_canParamList.at(i);
        /* 初始化CAN设备 */
        VCI_INIT_CONFIG config;
        config.AccCode = param.accessCode.toUInt(nullptr, 16);
        config.AccMask = param.maskCode.toUInt(nullptr, 16);
        config.Filter  = param.filterType;
        config.Mode    = param.workMode;
        config.Timing0 = g_tim0[param.baudrateId].toInt(nullptr, 16);
        config.Timing1 = g_tim1[param.baudrateId].toInt(nullptr, 16);
        qDebug("AccCode:0x%08X, AccMask:0x%08X, Filter:0x%02X, Mode:0x%02X, Timing0:0x%02X Timing1:0x%02X",
            config.AccCode, config.AccMask, config.Filter, config.Mode, config.Timing0, config.Timing1);
        qDebug() << "m_devtype" << m_devtype << "m_deviceIndex" << m_deviceIndex
                 << "param.chanelIndex" << param.chanelIndex;

        if (STATUS_OK != VCI_InitCAN(m_devtype, m_deviceIndex, param.chanelIndex, &config)) {
            return -2;
        }

        if (STATUS_OK != VCI_StartCAN(m_devtype, m_deviceIndex, param.chanelIndex)) {
            return -3;
        }
    }

    ui->canConfigGroupBox->setEnabled(false);
    return 0;
}

int CanConfigDialog::closeCANDevice(void)
{
    ui->canConfigGroupBox->setEnabled(true);

    DWORD m_devtype = ui->devComboBox->itemData(m_devtypeIndex).toInt();
    /* 关闭设备 */
    if (STATUS_OK != VCI_CloseDevice(m_devtype, m_deviceIndex)) {
        //QMessageBox::critical(this, tr("错误"), tr("设备关闭失败 !"), QMessageBox::Ok);
        return -1;
    }

    return 0;
}

void CanConfigDialog::slotOkButtonPressed(void)
{
    m_devtypeIndex = ui->devComboBox->currentIndex();
    m_deviceIndex = ui->indexComboBox->currentIndex();

    CAN_PARAM_T param0;
    param0.chanelIndex = CAN_CH0;
    param0.accessCode = ui->accessCode0LineEdit->text();
    param0.maskCode   = ui->maskCode0LineEdit->text();
    param0.filterType = ui->filterType0ComboBox->currentIndex();
    param0.workMode   = ui->workMode0ComboBox->currentIndex();
    param0.baudrateId = ui->baudrate0ComboBox->currentIndex();
    m_canParamList.replace(CAN_CH0, param0);

    CAN_PARAM_T param1;
    param1.chanelIndex = CAN_CH1;
    param1.accessCode = ui->accessCode1LineEdit->text();
    param1.maskCode   = ui->maskCode1LineEdit->text();
    param1.filterType = ui->filterType1ComboBox->currentIndex();
    param1.workMode   = ui->workMode1ComboBox->currentIndex();
    param1.baudrateId = ui->baudrate1ComboBox->currentIndex();
    m_canParamList.replace(CAN_CH1, param1);

    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.beginGroup(QLatin1String("CanParamCfg"));
    settings.setValue("devtypeIndex", m_devtypeIndex);
    settings.setValue("deviceIndex", m_deviceIndex);
    for (int i = 0; i < CAN_CHX_MAX; ++i) {
        CAN_PARAM_T param = m_canParamList.at(i);
        settings.setValue(tr("chanelIndex%1").arg(i), param.chanelIndex);
        settings.setValue(tr("accessCode%1").arg(i), param.accessCode);
        settings.setValue(tr("maskCode%1").arg(i), param.maskCode);
        settings.setValue(tr("filterType%1").arg(i), param.filterType);
        settings.setValue(tr("workMode%1").arg(i), param.workMode);
        settings.setValue(tr("baudrateId%1").arg(i), param.baudrateId);
    }
    settings.endGroup();

    emit signalCANDevice(ui->devComboBox->currentText(), 
        ui->devComboBox->itemData(m_devtypeIndex).toInt(), m_deviceIndex);
}

void CanConfigDialog::slotSendCANData(int chanel, const QString &canid, const QString &data)
{
    VCI_CAN_OBJ can;
    memset(&can, 0, sizeof(VCI_CAN_OBJ));

    //qDebug() << "CanConfigDialog:chanel" << chanel << "canid" << canid << "data" << data;
    
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

void CanConfigDialog::sendRpmCANData(int rpm)
{
    QString text = ui->rpmDataLineEdit->text();
    QByteArray data = QByteArray::fromHex(text.toLatin1().replace(" ", ""));

    rpm = rpm * 8;
    data[3] = (rpm & 0xFF);
    data[4] = ((rpm >> 8) & 0xFF);

    ui->rpmDataLineEdit->setText(data.toHex().toUpper());
    slotSendCANData(CAN_CH0, ui->rpmCanidLineEdit->text(), data.toHex());
}

void CanConfigDialog::sendSpeedCANData(double speed)
{
    QString text = ui->speedDataLineEdit->text();
    QByteArray data = QByteArray::fromHex(text.toLatin1().replace(" ", ""));
    
    int canspeed = speed * 256;
    data[0] = ui->brakeCheckBox->isChecked()? (data[0] | 0x04) : (data[0] & 0xFB);
    data[1] = (canspeed & 0xFF);
    data[2] = ((canspeed >> 8) & 0xFF);
    data[3] = ui->clutchCheckBox->isChecked()? (data[3] | 0x40) : (data[3] & 0xBF);
    
    ui->speedDataLineEdit->setText(data.toHex().toUpper());
    slotSendCANData(CAN_CH0, ui->speedCanidLineEdit->text(), data.toHex());
}

bool CanConfigDialog::isBrake(int &minRpm)
{
    minRpm = ui->minRpmLineEdit->text().toInt();
    return ui->brakeCheckBox->isChecked();
}

void CanConfigDialog::slotShowDialog(void)
{
    ui->devComboBox->setCurrentIndex(m_devtypeIndex);
    ui->indexComboBox->setCurrentIndex(m_deviceIndex);
    
    CAN_PARAM_T param0 = m_canParamList.at(CAN_CH0);
    ui->accessCode0LineEdit->setText(param0.accessCode);
    ui->maskCode0LineEdit->setText(param0.maskCode);
    ui->filterType0ComboBox->setCurrentIndex(param0.filterType);
    ui->workMode0ComboBox->setCurrentIndex(param0.workMode);
    ui->baudrate0ComboBox->setCurrentIndex(param0.baudrateId);
    
    CAN_PARAM_T param1 = m_canParamList.at(CAN_CH1);
    ui->accessCode1LineEdit->setText(param1.accessCode);
    ui->maskCode1LineEdit->setText(param1.maskCode);
    ui->filterType1ComboBox->setCurrentIndex(param1.filterType);
    ui->workMode1ComboBox->setCurrentIndex(param1.workMode);
    ui->baudrate1ComboBox->setCurrentIndex(param1.baudrateId);
    
    show();
}

