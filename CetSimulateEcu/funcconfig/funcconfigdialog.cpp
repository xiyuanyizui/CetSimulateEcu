#include "funcconfigdialog.h"
#include "ui_funcconfigdialog.h"

#include <QSettings>

#include <QDebug>

static uint32_t kmsSecuritykey[4] = {0};

//解密函数
static void KMS_TEA_decrypt(uint8_t *v, const uint32_t *k)
{
    uint32_t v0 = v[0], v1 = v[1], sum = 0xC6EF3720, i;  /* set up */
    uint32_t delta = 0x9E3779B9;                         /* a key schedule constant */
    uint32_t k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3]; /* cache key */

    v0 = (v[0] << 24) | (v[1] << 16) | (v[2] << 8) | (v[3] << 0);
    v1 = (v[4] << 24) | (v[5] << 16) | (v[6] << 8) | (v[7] << 0);
    for (i = 0; i < 32; ++i) {                           /* basic cycle start */
        v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
        v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        sum -= delta;
    }                                                    /* end cycle */

    v[0] = (v0 & 0xFF000000) >> 24;
    v[1] = (v0 & 0x00FF0000) >> 16;
    v[2] = (v0 & 0x0000FF00) >> 8;
    v[3] = (v0 & 0x000000FF) >> 0;
    v[4] = (v1 & 0xFF000000) >> 24;
    v[5] = (v1 & 0x00FF0000) >> 16;
    v[6] = (v1 & 0x0000FF00) >> 8;
    v[7] = (v1 & 0x000000FF) >> 0;
}

static void KMS_HandshakeDecrypt(const uint8_t content[8], const uint8_t iseed[7], 
    uint8_t *deactive, uint8_t gpsid[4], uint16_t *deratespeed)
{
    uint8_t i = 0;
    uint8_t x3send[8] = {0};
    uint8_t encrypt[8] = {0};
    uint8_t plaintext[8] = {0};

    memcpy(x3send, iseed, 7);
    x3send[7] = 0xFF;

    memcpy(encrypt, content, sizeof(encrypt));
    KMS_TEA_decrypt(encrypt, kmsSecuritykey);

    for (i = 0; i < sizeof(x3send); ++i) {
        plaintext[i] = x3send[i] ^ encrypt[i];
    }

    *deactive = plaintext[0];
    memcpy(gpsid, &plaintext[1], 4);
    *deratespeed = (plaintext[5] << 0) | (plaintext[6] << 8);
}

static void KMS_ActiveDecrypt(const uint8_t content[16], const uint8_t iseed[7], 
    uint16_t mkey[4], uint8_t gpsid[4], uint8_t oseed[4])
{
    uint8_t i = 0;
    uint8_t x3send[8] = {0};
    uint8_t plaintext1[8] = {0};
    uint8_t plaintext2[8] = {0};
    uint8_t xorval[8] = {0};

    memcpy(x3send, iseed, 7);
    x3send[7] = 0xFF;

    memcpy(plaintext1, content, sizeof(plaintext1));
    KMS_TEA_decrypt(plaintext1, kmsSecuritykey);
    memcpy(xorval, plaintext1, sizeof(xorval));
    for (i = 0; i < sizeof(x3send); ++i) {
        plaintext1[i] = x3send[i] ^ plaintext1[i];
    }
    mkey[0] = (plaintext1[0] << 0) | (plaintext1[1] << 8);
    mkey[1] = (plaintext1[2] << 0) | (plaintext1[3] << 8);
    mkey[2] = (plaintext1[4] << 0) | (plaintext1[5] << 8);
    mkey[3] = (plaintext1[6] << 0) | (plaintext1[7] << 8);

    for (i = 0; i < sizeof(plaintext1); ++i) {
        plaintext1[i] = content[i] ^ xorval[i];
    }
    memcpy(plaintext2, &content[8], sizeof(plaintext2));
    KMS_TEA_decrypt(plaintext2, kmsSecuritykey);
    for (i = 0; i < sizeof(plaintext2); ++i) {
        plaintext2[i] = plaintext2[i] ^ plaintext1[i];
    }

    memcpy(gpsid, plaintext2, 4);
    memcpy(oseed, &plaintext2[4], 4);
}

static void KMS_SetSecurityKey(uint32_t securitykey[4])
{
    memcpy(kmsSecuritykey, securitykey, sizeof(kmsSecuritykey));
}


QString FuncConfigDialog::usetime(int timestamp)
{
    int day = timestamp / 86400;
    int hour = (timestamp % 86400) / 3600;
    int minute = ((timestamp % 86400) % 3600) / 60;
    int second = ((timestamp % 86400) % 3600) % 60;

    return QString("%1 %2:%3:%4").arg(day, 3, 10, QLatin1Char('0'))
                    .arg(hour, 2, 10, QLatin1Char('0'))
                    .arg(minute, 2, 10, QLatin1Char('0'))
                    .arg(second, 2, 10, QLatin1Char('0'));
}

FuncConfigDialog::FuncConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FuncConfigDialog),
    m_maxEcuRpm(ECU_DEFAULT_MAX_RPM),
    m_runSeconds(0),
    m_restSeconds(0)
{
    ui->setupUi(this);

    ui->encryptKeyLineEdit->setInputMask("HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH");
    ui->ciphertext1LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->ciphertext2LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->ciphertext3LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->encryptSeedLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->plaintext1LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->plaintext2LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->plaintext3LineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->gpsidLineEdit->setInputMask("HH HH HH HH");
    ui->decryptSeedLineEdit->setInputMask("HH HH HH HH");
    ui->manualKeyLineEdit->setInputMask("HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH");

    loDlg = new LockOperateDialog(this);
    connect(loDlg, &LockOperateDialog::signalSendCANData, this, &FuncConfigDialog::signalSendCANData);

    lvDlg = new LockVehicleDialog(this);
    connect(ui->lockVehicleButton, &QToolButton::clicked, lvDlg, &LockVehicleDialog::showDialog);
    connect(lvDlg, &LockVehicleDialog::signalSendCANData, this, &FuncConfigDialog::signalSendCANData);
    connect(lvDlg, &LockVehicleDialog::signalEventInfo, this, &FuncConfigDialog::doEventInfo);
    connect(lvDlg, &LockVehicleDialog::signalMaxRpmChanged, this, &FuncConfigDialog::doMaxRpmChanged);

    connect(ui->lockFuncCheckBox, &QCheckBox::clicked, lvDlg, &LockVehicleDialog::setLockVehicleButton);
    connect(ui->lockFuncCheckBox, &QCheckBox::clicked, this, &FuncConfigDialog::doFuncConfigChanged);
    connect(ui->tiredFuncCheckBox, &QCheckBox::clicked, this, &FuncConfigDialog::doFuncConfigChanged);
}

FuncConfigDialog::~FuncConfigDialog()
{
    delete loDlg;
    delete lvDlg;
    delete ui;
}

void FuncConfigDialog::doFuncConfigChanged(bool checked)
{
    QCheckBox *checkBox = static_cast<QCheckBox *>(sender());
    if (ui->lockFuncCheckBox == checkBox) {
        loDlg->setVisible(checked);
        emit signalFuncTypeChanged(FUNC_LOCK, lvDlg->vehicleVendor(), checked);
    } else if (ui->tiredFuncCheckBox == checkBox) {
        ui->maxRunLineEdit->setDisabled(checked);
        ui->minRestLineEdit->setDisabled(checked);
        ui->preWarnLineEdit->setDisabled(checked);
        emit signalFuncTypeChanged(FUNC_TIRED, "", checked);
    }
}

void FuncConfigDialog::vehicleCanDataEntry(const VCI_CAN_OBJ &can)
{
    lvDlg->lockVehicleCanEntry(can);
}

void FuncConfigDialog::tiredDriveSecEntry(bool havecan, double speed)
{
    static const int interval = 3;
    static int runcount = 0;
    static int precount = 0;
    
    if (havecan && speed > 0) {
        if (m_runSeconds) {
            m_runSeconds += m_restSeconds;
        }
        m_runSeconds++;
        m_restSeconds = 0;
    } else {
        m_restSeconds++;
    }
    
    int maxRunSeconds = ui->maxRunLineEdit->text().toInt();
    int minRestSeconds = ui->minRestLineEdit->text().toInt();
    int preWarnSeconds = ui->preWarnLineEdit->text().toInt();

    if (m_restSeconds > minRestSeconds) {
        emit signalEventInfo(FUNC_TIRED, tr("您已休息满%1分钟").arg(minRestSeconds/60));
        runcount = 0;
        precount = 0;
        m_runSeconds = 0;
        m_restSeconds = 0;
    } else if (m_runSeconds >= maxRunSeconds) {
        if (runcount % interval == 0 && runcount < 7) {
            emit signalEventInfo(FUNC_TIRED, tr("您已疲劳行驶 - %1").arg(usetime(m_runSeconds)));
        }
        precount = 0;
        runcount++;
    } else if (m_runSeconds >= (maxRunSeconds - preWarnSeconds)) {
        if (precount % interval == 0 && precount < 7) {
            emit signalEventInfo(FUNC_TIRED, tr("您即将疲劳行驶 - %1").arg(usetime(m_runSeconds)));
        }
        runcount = 0;
        precount++;
    }
}

void FuncConfigDialog::tiredDriveParam(int *runSeconds, int *restSeconds)
{
    *runSeconds = m_runSeconds;
    *restSeconds = m_restSeconds;
}

void FuncConfigDialog::doEventInfo(const QString &info)
{
    QObject *object = sender();
    if (lvDlg == object) {
        emit signalEventInfo(FUNC_LOCK, info);
    }
}

void FuncConfigDialog::doMaxRpmChanged(int maxRpm)
{
    m_maxEcuRpm = maxRpm;
}

int FuncConfigDialog::maxEcuLockRpm()
{
    return m_maxEcuRpm;
}

void FuncConfigDialog::slotControlStateChanged(bool acc, bool ig, bool can)
{
    lvDlg->controlStateChanged(acc, ig, can);
}

void FuncConfigDialog::slotEcuTypeChanged(const QString &ecuBrand, const QString &engineVendor)
{
    ui->lockFuncCheckBox->setChecked(false);
    ui->tiredFuncCheckBox->setChecked(false);
    lvDlg->setEcuLockLogics(ecuBrand, engineVendor);
    lvDlg->setLockVehicleButton(false);
}


void FuncConfigDialog::on_activeRadioButton_clicked(bool checked)
{
    if (checked) {
        ui->ciphertext2Label->setVisible(true);
        ui->ciphertext2LineEdit->setVisible(true);
        ui->ciphertext3Label->setVisible(true);
        ui->ciphertext3LineEdit->setVisible(true);

        ui->plaintext2Label->setVisible(true);
        ui->plaintext2LineEdit->setVisible(true);
        ui->plaintext3Label->setVisible(true);
        ui->plaintext3LineEdit->setVisible(true);

        ui->decryptSeedLabel->setText(tr("SEED：0x"));
        ui->manualKeyLabel->setText(tr("手动密钥：0x"));
    }
}

void FuncConfigDialog::on_handshakeRadioButton_clicked(bool checked)
{
    if (checked) {
        ui->ciphertext2Label->setVisible(false);
        ui->ciphertext2LineEdit->setVisible(false);
        ui->ciphertext3Label->setVisible(false);
        ui->ciphertext3LineEdit->setVisible(false);
        
        ui->plaintext2Label->setVisible(false);
        ui->plaintext2LineEdit->setVisible(false);
        ui->plaintext3Label->setVisible(false);
        ui->plaintext3LineEdit->setVisible(false);
        
        ui->decryptSeedLabel->setText(tr("反激活：0x"));
        ui->manualKeyLabel->setText(tr("限速值：0x"));
    }
}

void FuncConfigDialog::on_decryptButton_clicked()
{
    QString text = ui->encryptKeyLineEdit->text();
    uint32_t securitykey[4] = {0x32B162CD, 0x729F6E13, 0x5FAE2E12, 0x12747A3E};
    for (int i = 0; (i*12) < text.length(); ++i) {
        QString value = text.mid(i*12+0, 2) + text.mid(i*12+3, 2) + text.mid(i*12+6, 2) + text.mid(i*12+9, 2);
        securitykey[i] = value.toUInt(nullptr, 16);
    }
    KMS_SetSecurityKey(securitykey);

    QString ciphertext1 = ui->ciphertext1LineEdit->text().replace(" ", "");
    QString ciphertext2 = ui->ciphertext2LineEdit->text().replace(" ", "");
    QString ciphertext3 = ui->ciphertext3LineEdit->text().replace(" ", "");
    QString encryptSeed = ui->encryptSeedLineEdit->text().replace(" ", "");
    QByteArray seed = QByteArray::fromHex(encryptSeed.toLatin1()).mid(1);

    if (ui->activeRadioButton->isChecked()) { // 解密激活密文
        QByteArray cipher1 = QByteArray::fromHex(ciphertext1.toLatin1());
        QByteArray cipher2 = QByteArray::fromHex(ciphertext2.toLatin1());
        QByteArray cipher3 = QByteArray::fromHex(ciphertext3.toLatin1());

        QByteArray encrypt;
        encrypt.append(cipher1.right(7));
        encrypt.append(cipher2.right(7));
        encrypt.append(cipher3.right(7));
        
        uint16_t mkey[4] = {0};
        uint8_t gpsid[4] = {0}, oseed[4] = {0};
        KMS_ActiveDecrypt((uint8_t *)encrypt.data(), (uint8_t *)seed.data(), mkey, gpsid, oseed);

        QByteArray decrypt;
        decrypt.append((char *)mkey, sizeof(mkey));
        decrypt.append((char *)gpsid, sizeof(gpsid));
        decrypt.append((char *)oseed, sizeof(oseed));
        ui->plaintext1LineEdit->setText("01" + decrypt.mid(0, 7).toHex().toUpper());
        ui->plaintext2LineEdit->setText("02" + decrypt.mid(7, 7).toHex().toUpper());
        ui->plaintext3LineEdit->setText("03" + decrypt.mid(14, 2).toHex().toUpper() + "FFFFFFFFFF");

        ui->gpsidLineEdit->setText(decrypt.mid(8, 4).toHex().toUpper());
        ui->decryptSeedLineEdit->setText(decrypt.mid(12, 4).toHex().toUpper());
        ui->manualKeyLineEdit->setText(decrypt.mid(0, 8).toHex().toUpper());
    } else { // 解密握手密文
        uint8_t deactive = 0, gpsid[4] = {0};
        uint16_t deratespeed = 0;
        QByteArray encrypt = QByteArray::fromHex(ciphertext1.toLatin1());
        KMS_HandshakeDecrypt((uint8_t *)encrypt.data(), (uint8_t *)seed.data(), 
                            &deactive, gpsid, &deratespeed);

        QByteArray decrypt;
        decrypt.append(deactive);
        decrypt.append((char *)gpsid, sizeof(gpsid));
        decrypt.append((deratespeed & 0x00FF) >> 0);
        decrypt.append((deratespeed & 0xFF00) >> 8);
        ui->plaintext1LineEdit->setText(decrypt.toHex().toUpper() + "FF");
        
        ui->gpsidLineEdit->setText(decrypt.mid(1, 4).toHex().toUpper());
        ui->decryptSeedLineEdit->setText(decrypt.mid(0, 1).toHex().toUpper());
        ui->manualKeyLineEdit->setText(decrypt.mid(5, 2).toHex().toUpper());
    }
}

void FuncConfigDialog::on_clearInputButton_clicked()
{
    ui->ciphertext1LineEdit->clear();
    ui->ciphertext2LineEdit->clear();
    ui->ciphertext3LineEdit->clear();
    ui->encryptSeedLineEdit->clear();
}
