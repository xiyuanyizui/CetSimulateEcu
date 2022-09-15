#include "lockvehicledialog.h"
#include "ui_lockvehicledialog.h"

#include <QtSql>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

/**
    潍柴锁车定义
*/
/*
    Byte6 应答帧
    0x01    GPS 功能激活        首次激活时有应答帧，重复绑定时有应答帧。 
    0x02    GPS 功能关闭
    0x03    握手校验成功          显示校验通过与否用来判断校验通过没有，如果为4可以考虑GPS被互换了，进行重新绑定
    0x04    握手校验失败
*/
#define WEICHAI_ACK_GPSFUNCTION_ACTIVE      0x01
#define WEICHAI_ACK_GPSFUNCTION_CLOSE       0x02
#define WEICHAI_ACK_HANDSHAKE_SUCCESS       0x03
#define WEICHAI_ACK_HANDSHAKE_FAILURE       0x04

/*
    Byte3 状态帧
    Bit1=1  GPS 功能激活        显示 GPS 激活与否
    Bit1=0  GPS 功能关闭
    Bit2=1  锁车
    Bit2=0  未锁车
    Bit3=1  Key 正确
    Bit3=0  Key 不正确
    Bit4=1  GPS ID匹配
    Bit4=0  GPS ID不匹配
*/
#define WEICHAI_STATE_GPSFUNCTION_ACTIVE    0x01
#define WEICHAI_STATE_GPSFUNCTION_CLOSE     0x00
#define WEICHAI_STATE_LOCKED                0x01
#define WEICHAI_STATE_NOT_LOCK              0x00
#define WEICHAI_STATE_KEY_OK                0x01
#define WEICHAI_STATE_KEY_ER                0x00
#define WEICHAI_STATE_GPSID_MATCHED         0x01
#define WEICHAI_STATE_GPSID_NOT_MATCH       0x00

typedef struct { // 字节对齐
    uint8_t     : 8;  // byte1
    uint8_t     : 8;  // byte2
    uint8_t     activeStatus: 1;    // byte3
    uint8_t     lockStatus : 1;
    uint8_t     keyStatus: 1;
    uint8_t     gpsidMatchStatus : 1;
    uint8_t     : 4;
    uint8_t     : 8;  // byte4 
    uint8_t     : 8;  // byte5
    uint8_t     ackStatus; // byte6
    uint16_t    : 16;
} wcresponse_t;

// 重汽潍柴锁车参数定义
typedef struct {
    int         seedCount;
    int         hsErrorCount;
    int         maxRpm;
    QString     gpsid;
    QString     solidKey;
    uint16_t    lockRpm;
    QString     md5key;
    union {
        wcresponse_t v;
        uint8_t val[8];
    } response;                 /**< 默认状态响应18FD0100 */
} BocshWeiChaiLock_t;

/**
    玉柴锁车定义
*/
/*  BYTE6
    激活          校验          故障判断                                    请求信号
    00 未激活 00 未通过           00 由于等待请求信号过久而出现的超时                     00 未收到请求信号
    01 激活     01 通过         01 由于等待 key 返回过久出现的超时                   01 收到请求信号
                            10 双方的 key 不一致
              11 未校验        11 没有出现任何的超时
*/
#define YUCHAI_ACTIVE_STATUS_NOT_ACTIVED     0x00
#define YUCHAI_ACTIVE_STATUS_ACTIVED         0x01

#define YUCHAI_CHECK_STATUS_NOT_PASS         0x00
#define YUCHAI_CHECK_STATUS_PASS             0x01
#define YUCHAI_CHECK_STATUS_NOT_CHECK        0x03

/*  BYTE8
*/
#define YUCHAI_FAULT_STATUS_SIGNAL_TIMEOUT   0x00
#define YUCHAI_FAULT_STATUS_KEY_TIMEOUT      0x01
#define YUCHAI_FAULT_STATUS_KEY_INCONSISTENT 0x02
#define YUCHAI_FAULT_STATUS_NOT_TIMEOUT      0x03

#define YUCHAI_REQUST_SIGNAL_STATUS_NOT_RECEIVED     0x00
#define YUCHAI_REQUST_SIGNAL_STATUS_RECEIVED         0x01

/*  BYTE5
    主动锁车            被动锁车
    00 解除限制         00 未锁车
    01 停机           01 停机
    10 限制           10 限制
    11 not used     11 not used
*/
#define YUCHAI_ACTIVE_LOCK_STATUS_NOT_LIMIT     0x00
#define YUCHAI_ACTIVE_LOCK_STATUS_LIMIT_0RPM    0x01
#define YUCHAI_ACTIVE_LOCK_STATUS_LIMIT         0x02

#define YUCHAI_PASSIVE_LOCK_STATUS_NOT_LOCK      0x00
#define YUCHAI_PASSIVE_LOCK_STATUS_LIMIT_0RPM    0x01
#define YUCHAI_PASSIVE_LOCK_STATUS_LIMIT         0x02

typedef struct {
    uint8_t seed[4]; // byte1-byte4
    uint8_t : 2;     // byte5
    uint8_t passiveLock : 2;
    uint8_t : 2;
    uint8_t activeLock : 2;
    uint8_t : 4;     // byte6
    uint8_t checkStatus : 2;
    uint8_t activeStatus : 2;
    uint8_t : 8;     // byte7
    uint8_t requestSignalStatus : 2; // byte8
    uint8_t faultStatus : 2;
} ycresponse_t;

// 大运玉柴锁车参数定义
typedef struct {
    int         maxRpm;
    QString     gpsid;          /**< GPSID */
    uint8_t     lockcmd;        /**< 锁车命令 */
    uint16_t    lockRpm;
    QString     md5pwd;         /**< MD5密码 */
    QString     handpwd;        /**< 握手校验密码 */
    QString     unbindpwd;      /**< 解绑密码 */
    QString     lockpwd;        /**< 主动锁车密码 */
    QString     unlockpwd;      /**< 紧急解锁密码(手动设置) */
    union {
        ycresponse_t v;
        uint8_t val[8];
    } response;                 /**< 默认状态响应18FD0100 */
} BocshYuChaiLock_t;

/**
    康明斯锁车定义
*/
#define KANGMINGSI_ACTIVE_MODE_HEX     "A03DD10AE053D349"
#define KANGMINGSI_HANDSHAKE_MODE_HEX  "B13DD1C62527E95B"

#define KANGMINGSI_CONTROLBYTE_X3V1   0xA1    // X3 lockout Version 1
#define KANGMINGSI_CONTROLBYTE_X3V2   0xA2    // X3 lockout Version 2
#define KANGMINGSI_CONTROLBYTE_X3V3   0xA3    // X3 lockout Version 3
#define KANGMINGSI_CONTROLBYTE_X3V4   0xA4    // X3 lockout Version 4
#define KANGMINGSI_CONTROLBYTE_X3V5   0xA5    // X3 lockout Version 5
#define KANGMINGSI_CONTROLBYTE_X3V6   0xA6    // X3 lockout Version 6
#define KANGMINGSI_CONTROLBYTE_X3V7   0xA7    // X3 lockout Version 7
#define KANGMINGSI_CONTROLBYTE_X3V8   0xA8    // X3 lockout Version 8
#define KANGMINGSI_CONTROLBYTE_X3V9   0xA9    // X3 lockout Version 9
#define KANGMINGSI_CONTROLBYTE_ECMSTATUS   0xB1    // ECM Status

/*
    Engine Activate status 2.1 2 bits 
    00: Not Activated;01: Activated;10: Reserve; 11:NA
*/
#define KANGMINGSI_ACTIVATE_STATUS_NOT_ACTIVATED   0x00
#define KANGMINGSI_ACTIVATE_STATUS_ACTIVATED       0x01
#define KANGMINGSI_ACTIVATE_STATUS_NA              0x03

/*
    Timeout status 2.3 2 bits 
    00: No Timeout; 01: In Timeout;10: Reserve; 11: NA
*/
#define KANGMINGSI_TIMEOUT_STATUS_NO_TIMEOUT       0x00
#define KANGMINGSI_TIMEOUT_STATUS_IN_TIMEOUT       0x01
#define KANGMINGSI_TIMEOUT_STATUS_NA               0x03

/*
    Lockout status 2.5 2 bits
    00: Not Locked; 01: Locked by command;10: Default Lock; 11: NA
*/
#define KANGMINGSI_LOCKOUT_STATUS_NOT_LOCKED           0x00
#define KANGMINGSI_LOCKOUT_STATUS_LOCKED_BY_COMMAND    0x01
#define KANGMINGSI_LOCKOUT_STATUS_DEFAULT_LOCK         0x02
#define KANGMINGSI_LOCKOUT_STATUS_NA                   0x03

/*
    OEM Data status 2.7 2 bits
    00: Wrong data; 01: Right data;10: Reserved;11: NA
*/
#define KANGMINGSI_OEMDATA_STATUS_WRONG_DATA       0x00
#define KANGMINGSI_OEMDATA_STATUS_RIGHT_DATA       0x01
#define KANGMINGSI_OEMDATA_STATUS_NA               0x03

/*
    Current Activated lockout feature: 3.1 4 bits
    0000: No Lockout feature has been Activated
    0001: Lockout feature has Activated by TY
    0010: Lockout feature has Activated by X1
    0011: Lockout feature has Activated by X2
    0100: Lockout feature has Activated by X3
    1111: NA
*/
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_NO_ACTIVATED  0x00
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_TY_ACTIVATED  0x01
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_X1_ACTIVATED  0x02
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_X2_ACTIVATED  0x03
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_X3_ACTIVATED  0x04
#define KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_NA            0x0F

/*
    Manual Unlock Status 3.5 2 bits
    00: Not in Manual Unlock Mode;01: In Manual Unlock Mode;10: Rserve 11:NA
*/
#define KANGMINGSI_MANUAL_UNLOCK_STATUS_NOT_IN_MODE    0x00
#define KANGMINGSI_MANUAL_UNLOCK_STATUS_IN_MODE        0x01
#define KANGMINGSI_MANUAL_UNLOCK_STATUS_NA             0x03

/*
    Manual unlock valid key numbers 6.1 3 bits
    000: 0 valid key
    001: 1 valid key
    010: 2 valid keys
    011: 3 valid keys
    100: 4 valid keys
    101-110: Reserve
    111: NA
*/
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_0  0x00
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_1  0x01
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_2  0x02
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_3  0x03
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_4  0x04
#define KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NA        0x07

/*
    Deactive Command 1.1 2 bits 
    00: Not Deactive 01: Deactive; 10: Reserved 11: NA
*/
#define KANGMINGSI_DEACTIVE_COMMAND_NOT_DEAVTIVE   0x00
#define KANGMINGSI_DEACTIVE_COMMAND_DEAVTIVE       0x01
#define KANGMINGSI_DEACTIVE_COMMAND_NA             0x03

typedef struct {
    uint8_t     controlByte; // byte1
    uint8_t     engineActivateStatus : 2; // byte2
    uint8_t     timeoutStatus : 2;
    uint8_t     lockoutStatus : 2;
    uint8_t     oemDataStatus : 2;
    uint8_t     activatedLockoutFeature : 4; // byte3
    uint8_t     manualUnlockStatus : 2;
    uint8_t     : 2;
    uint8_t     derateSpeedLow; // byte4
    uint8_t     derateSpeedHigh; // byte5
    uint8_t     manualUnlockValidKeyNumbers : 3; // byte6
    uint8_t     : 5;
    uint16_t    : 16;
} kmsresponse_t;

// 大运康明斯锁车参数定义
typedef struct {
    int         msecCount;
    int         maxRpm;
    QByteArray  gpsid;          /**< GPSID */
    uint16_t    lockRpm;
    uint16_t    mkey[4];
    QByteArray  seed;
    union {
        kmsresponse_t v;
        uint8_t val[8];
    } response; /**< 默认状态响应00FFCA00 */
} BocshKangMingSiLock_t;

// 重汽曼锁车参数定义
typedef struct {
    int         hsErrorCount;
    int         maxRpm;
    QByteArray  seed;
} BocshManLock_t;

/////////////////////////////////////////////////////////////////////

#define POWERUP_SEED_CNT (4)

#define HEARTBEAT_PERIOD    20000

#define DATABASE_FIELD_LOCK_CFGINFO \
    "id int primary key, vehicleVendor varchar, "    \
    "seedRecvCanid varchar, seedSendCanid varchar, " \
    "seedRecvData varchar, seedSendData varchar, keyRecvData varchar, keySendData varchar, " \
    "bindRecvCanid varchar, bindSendCanid varchar, " \
    "bindRecvData varchar, bindSendData varchar, unbindRecvData varchar, unbindSendData varchar, "   \
    "lockRecvCanid varchar, lockSendCanid varchar, " \
    "lockRecvData varchar, lockSendData varchar, unlockRecvData varchar, unlockSendData varchar, "   \
    "heartbeatRecvCanid varchar, heartbeatSendCanid varchar, "   \
    "heartbeatRecvData varchar, heartbeatSendData varchar"

#define IDENTIFY_CANID      "identifyCanid"
#define IDENTIFY_CONTENT    "identifyContent"
#define IDENTIFY_PERIOD     "identifyPeriod"

#define STATE_CANID         "stateCanid"
#define STATE_CONTENT       "stateContent"
#define STATE_PERIOD        "statePeriod"

#define SETTINGS_INI_DIR    "./Settings.ini"

#define Settings_IS(ecuBrand, engineVendor, vehicleVendor) \
    QString("%1_%2_%3_IS").arg(ecuBrand, engineVendor, vehicleVendor)
#define Database_LOCKCFG(ecuBrand, engineVendor)   \
    QString("%1_%2_LOCKCFG").arg(ecuBrand, engineVendor)

static BocshManLock_t bManLock;
static BocshWeiChaiLock_t bWeiChaiLock;
static BocshYuChaiLock_t bYuChaiLock;
static BocshKangMingSiLock_t bKangMingSiLock;

static uint32_t kmsSecuritykey[4] = {0};

static QByteArray generateSeed(int len)
{
    QByteArray seed;
    while (len--) {
        seed.append(qrand() % 16);
    }
    
    return seed;
}

static QByteArray hexStr2ByteArray(const QString &hexs)
{
    QByteArray byteArray;
    QString hexstr = hexs;
    
    hexstr.replace(QLatin1String(" "), QLatin1String(""));
    for (int i = 0; i < hexstr.length(); i += 2) {
        byteArray.append(hexstr.mid(i, 2).toUShort(nullptr, 16));
    }

    return byteArray;
}

static void get_rand_str(uint8_t rand[], uint8_t newval[], int number)
{
    char str[64] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    str[62] = 0;
    str[63] = 0;
    
    for (int i = 0; i < number; ++i) {
        while (rand[i] >= 62) {
            rand[i] -= 62;
        }
        newval[i] = str[rand[i]];
    }
}

//加密函数
static void KMS_TEA_encrypt(uint8_t *v, const uint32_t *k)
{
    uint32_t v0, v1, sum = 0, i;                         /* set up */
    uint32_t delta = 0x9E3779B9;                         /* a key schedule constant */
    uint32_t k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3]; /* cache key */

    v0 = (v[0] << 24) | (v[1] << 16) | (v[2] << 8) | (v[3] << 0);
    v1 = (v[4] << 24) | (v[5] << 16) | (v[6] << 8) | (v[7] << 0);
    for (i = 0; i < 32; ++i) {                           /* basic cycle start */
        sum += delta;
        v0 += ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        v1 += ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
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

static void KMS_HandshakeEncrypt(const uint8_t iseed[7], uint8_t deactive, 
    const uint8_t gpsid[4], uint16_t deratespeed, uint8_t content[8])
{
    uint8_t i = 0;
    uint8_t x3send[8] = {0};
    uint8_t plaintext[8] = {0};

    memcpy(x3send, iseed, 7);
    x3send[7] = 0xFF;
    
    plaintext[0] = deactive;
    memcpy(&plaintext[1], gpsid, 4);
    plaintext[5] = (deratespeed & 0x00FF) >> 0;
    plaintext[6] = (deratespeed & 0xFF00) >> 8;
    plaintext[7] = 0xFF;

    for (i = 0; i < sizeof(x3send); ++i) {
        content[i] = x3send[i] ^ plaintext[i];
    }

    KMS_TEA_encrypt(content, kmsSecuritykey);
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

static void KMS_ActiveEncrypt(const uint8_t iseed[7], const uint16_t mkey[4], 
    const uint8_t gpsid[4], uint8_t content[16])
{
    uint8_t i = 0;
    uint8_t x3send[8] = {0};
    uint8_t plaintext1[8] = {0};
    uint8_t plaintext2[8] = {0};
    uint8_t xorval[8] = {0};

    memcpy(x3send, iseed, 7);
    x3send[7] = 0xFF;
    
    plaintext1[0] = (mkey[0] & 0x00FF) >> 0;
    plaintext1[1] = (mkey[0] & 0xFF00) >> 8;
    plaintext1[2] = (mkey[1] & 0x00FF) >> 0;
    plaintext1[3] = (mkey[1] & 0xFF00) >> 8;
    plaintext1[4] = (mkey[2] & 0x00FF) >> 0;
    plaintext1[5] = (mkey[2] & 0xFF00) >> 8;
    plaintext1[6] = (mkey[3] & 0x00FF) >> 0;
    plaintext1[7] = (mkey[3] & 0xFF00) >> 8;
    for (i = 0; i < sizeof(x3send); ++i) {
        xorval[i] = x3send[i] ^ plaintext1[i];
    }
    memcpy(content, xorval, sizeof(xorval));
    KMS_TEA_encrypt(content, kmsSecuritykey);

    memcpy(&plaintext2[0], gpsid, 4);
    memcpy(&plaintext2[4], x3send, 4);
    
    for (i = 0; i < sizeof(xorval); ++i) {
        xorval[i] = xorval[i] ^ content[i];
    }
    for (i = 0; i < sizeof(xorval); ++i) {
        xorval[i] = xorval[i] ^ plaintext2[i];
    }
    memcpy(&content[8], xorval, sizeof(xorval));
    KMS_TEA_encrypt(&content[8], kmsSecuritykey);
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

LockVehicleDialog::LockVehicleDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LockVehicleDialog),
    lockStep(LOCK_STEP_NULL),
    lockStatus(LOCK_STATUS_INIT)
{
    ui->setupUi(this);
    ui->lockLogicLineEdit->setEnabled(false);
    
    ui->heartbeatRecvCanidLineEdit->setEnabled(false);
    ui->heartbeatRecvDataLineEdit->setEnabled(false);
    ui->heartbeatSendCanidLineEdit->setEnabled(false);
    ui->heartbeatSendDataLineEdit->setEnabled(false);
    
    ui->handshakeRecvCanidLineEdit->setInputMask("HHHHHHHH");
    ui->handshakeSendCanidLineEdit->setInputMask("HHHHHHHH");
    ui->bindRecvCanidLineEdit->setInputMask("HHHHHHHH");
    ui->bindSendCanidLineEdit->setInputMask("HHHHHHHH");
    ui->lockRecvCanidLineEdit->setInputMask("HHHHHHHH");
    ui->lockSendCanidLineEdit->setInputMask("HHHHHHHH");
    ui->heartbeatRecvCanidLineEdit->setInputMask("HHHHHHHH");
    ui->heartbeatSendCanidLineEdit->setInputMask("HHHHHHHH");

    ui->seedRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->seedSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->keyRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->keySendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->bindRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->bindSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->unbindRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->unbindSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->lockRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->lockSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->unlockRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->unlockSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->heartbeatRecvDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->heartbeatSendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->encryptionKeyLineEdit->setInputMask("HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH");

    ui->identifyCanidLineEdit->setInputMask("HHHHHHHH");
    ui->stateCanidLineEdit->setInputMask("HHHHHHHH");
    ui->identifyDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
    ui->stateDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");

    // 创建数据库表
    QSqlQuery query;
    int len = sizeof(lockLogicAssemble)/ sizeof(lockLogicAssemble[0]);
    for (int i = 0; i < len; ++i) {
        QString info = QString("create table %1(%2)").arg(
            Database_LOCKCFG(lockLogicAssemble[i].ecuBrand, lockLogicAssemble[i].engineVendor), 
            DATABASE_FIELD_LOCK_CFGINFO);
        query.exec(info);
    }

    model = new QSqlQueryModel(this);

    lockdbdlg = new LockDatabaseDialog(this);
    connect(lockdbdlg, &LockDatabaseDialog::signalsVehicleVendor, 
        this, &LockVehicleDialog::on_vehicleVendorComboBox_currentTextChanged);

    ecuTimer = new QTimer(this);
    connect(ecuTimer, &QTimer::timeout, this, &LockVehicleDialog::doEcuTimeout);
    
    ecuStateTimer = new QTimer(this);
    connect(ecuStateTimer, &QTimer::timeout, this, &LockVehicleDialog::doEcuStateTimeout);

    ecuIdentifyTimer = new QTimer(this);
    connect(ecuIdentifyTimer, &QTimer::timeout, this, &LockVehicleDialog::doEcuIdentifyTimeout);

    bManLock.maxRpm = bWeiChaiLock.maxRpm = bYuChaiLock.maxRpm = bKangMingSiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
    memset(bWeiChaiLock.response.val, 0x00, sizeof(bWeiChaiLock.response.val));

    // 玉柴的初始化
    bYuChaiLock.gpsid.clear();
    memset(bYuChaiLock.response.val, 0xFF, sizeof(bYuChaiLock.response.val));
    bYuChaiLock.lockcmd = YUCHAI_ACTIVE_LOCK_STATUS_NOT_LIMIT;
    bYuChaiLock.response.val[5] = 0x00;
    bYuChaiLock.response.val[6] = 0xF2;
    ycresponse_t *pyv = &(bYuChaiLock.response.v);
    memset(pyv->seed, 0x00, sizeof(pyv->seed));
    pyv->passiveLock = YUCHAI_PASSIVE_LOCK_STATUS_NOT_LOCK;
    pyv->activeLock = YUCHAI_ACTIVE_LOCK_STATUS_NOT_LIMIT;
    pyv->checkStatus = YUCHAI_CHECK_STATUS_NOT_CHECK;
    pyv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_NOT_RECEIVED;
    pyv->faultStatus = YUCHAI_FAULT_STATUS_NOT_TIMEOUT;

    // 康明斯的初始化
    memset(bKangMingSiLock.response.val, 0xFF, sizeof(bKangMingSiLock.response.val));
    bKangMingSiLock.response.val[2] = 0x00; // 初始化该字节为0
    kmsresponse_t *pkv = &(bKangMingSiLock.response.v);
    pkv->controlByte = KANGMINGSI_CONTROLBYTE_ECMSTATUS; // 默认状态响应B1
    pkv->timeoutStatus = KANGMINGSI_TIMEOUT_STATUS_IN_TIMEOUT;
    pkv->manualUnlockValidKeyNumbers = KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_0;
}

LockVehicleDialog::~LockVehicleDialog()
{
    delete lockdbdlg;
    delete model;
    delete ecuTimer;
    delete ecuStateTimer;
    delete ecuIdentifyTimer;
    delete ui;
}

void LockVehicleDialog::doEcuTimeout()
{
    if (!curLockLogic.timeout)
        return;

    (this->*curLockLogic.timeout)();
}

void LockVehicleDialog::doEcuStateTimeout()
{
    if (!curLockLogic.stateTimeout)
        return;

    (this->*curLockLogic.stateTimeout)();
}

void LockVehicleDialog::doEcuIdentifyTimeout()
{
    if (!curLockLogic.identifyTimeout)
        return;

    (this->*curLockLogic.identifyTimeout)();
}

void LockVehicleDialog::updateIdentifyDataValue(bool enable, const QString &text)
{
    if (enable) {
        ui->identifyDataLineEdit->setText(text);
        if (!text.isEmpty() && !ui->identifyCanidLineEdit->text().isEmpty()) {
            QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
            settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
                curLockLogic.engineVendor, ui->vehicleVendorComboBox->currentText()));
            settings.setValue(IDENTIFY_CANID, ui->identifyCanidLineEdit->text());
            settings.setValue(IDENTIFY_CONTENT, text);
            settings.setValue(IDENTIFY_PERIOD, ui->identifyPeriodSpinBox->value());
            settings.endGroup();
        }
    } else {
        ui->identifyDataLineEdit->clear();
        QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
        settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
            curLockLogic.engineVendor, ui->vehicleVendorComboBox->currentText()));
        settings.setValue(IDENTIFY_CANID, "");
        settings.setValue(IDENTIFY_CONTENT, "");
        settings.setValue(IDENTIFY_PERIOD, 0);
        settings.endGroup();
    }
}

void LockVehicleDialog::updateStateDataValue(bool enable, const QString &text)
{
    if (enable) {
        ui->stateDataLineEdit->setText(text);
        
        if (!text.isEmpty() && !ui->stateCanidLineEdit->text().isEmpty()) {
            QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
            settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
                curLockLogic.engineVendor, ui->vehicleVendorComboBox->currentText()));
            settings.setValue(STATE_CANID, ui->stateCanidLineEdit->text());
            settings.setValue(STATE_CONTENT, text);
            settings.setValue(STATE_PERIOD, ui->statePeriodSpinBox->value());
            settings.endGroup();
        }
    } else {
        ui->stateDataLineEdit->clear();
        QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
        settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
            curLockLogic.engineVendor, ui->vehicleVendorComboBox->currentText()));
        settings.setValue(STATE_CANID, "");
        settings.setValue(STATE_CONTENT, "");
        settings.setValue(STATE_PERIOD, 0);
        settings.endGroup();
    }
}

void LockVehicleDialog::showDialog()
{
    if (curLockLogic.ecuBrand.isEmpty() || curLockLogic.engineVendor.isEmpty())
        return;
    
    QString locktable = Database_LOCKCFG(curLockLogic.ecuBrand, curLockLogic.engineVendor);
    updateLockLogicTable(locktable, ui->vehicleVendorComboBox->currentText());
    show();
}

void LockVehicleDialog::setLockVehicleButton(bool enable)
{
    ui->vehicleVendorComboBox->setDisabled(enable);
}

void LockVehicleDialog::packetSendCanData(const QString &canid, const QString &data)
{
    int count = 0;
    uint8_t udata[8] = {0};
    for (int i = 0; i < data.length(); i += 2) {
        udata[count++] = data.mid(i, 2).toInt(nullptr, 16);
    }

    sendCanDataUInt8(canid, udata);
}

void LockVehicleDialog::sendCanDataUInt8(const QString &canid, const uint8_t data[8])
{
    if (!hasPowerup)
        return;

    VCI_CAN_OBJ can;
    memset(&can, 0, sizeof(VCI_CAN_OBJ));

    can.ID         = canid.toUInt(nullptr, 16);
    can.SendType   = 0;
    can.ExternFlag = (can.ID > 0xFFFF) ? 1 : 0;
    can.RemoteFlag = 0;

    for (int i = 0; i < 8; ++i) {
        can.Data[i] = data[i];
    }
    can.DataLen = 8;

    //qDebug() << "canid" << canid;
    //qDebug("data:%02X %02X %02X %02X %02X %02X %02X %02X", data[0], data[1], 
    //    data[2], data[3], data[4], data[5], data[6], data[7]);
    emit signalSendCANData(CAN_CH0, can);
}

//===================== 锁车功能接口 =============================//

/////////////////////// 博世 - 曼 START /////////////////////////////
bool LockVehicleDialog::boschManHandshake(const QString &frameid, const QString &data)
{
    QString seedRecvData = ui->seedRecvDataLineEdit->text().replace(" ", "");
    QString seedSendData = ui->seedSendDataLineEdit->text().replace(" ", "");
    if (!frameid.compare(ui->handshakeRecvCanidLineEdit->text(), Qt::CaseInsensitive)
         && data.contains(seedRecvData, Qt::CaseInsensitive)) {
        bManLock.seed = generateSeed(4);
        seedSendData.replace(4, 8, bManLock.seed.toHex().toUpper());
        packetSendCanData(ui->handshakeSendCanidLineEdit->text(), seedSendData);
        lockStep = LOCK_STEP_KEY;
        ecuTimer->start(100);
        bManLock.hsErrorCount++;
        
        emit signalEventInfo(tr("握手中 - 随机值：0x%1").arg(seedSendData.mid(4, 8)));
    }

    if (LOCK_STEP_KEY == lockStep) {
        QString keyRecvData = ui->keyRecvDataLineEdit->text().replace(" ", "");
        if (!frameid.compare(ui->handshakeRecvCanidLineEdit->text(), Qt::CaseInsensitive)
            && data.contains(keyRecvData, Qt::CaseInsensitive)) {
            if (!curLockLogic.key) {
                lockStep = LOCK_STEP_HANDSHAKE_ER;
                emit signalEventInfo(tr("握手失败 - 逻辑对象错误"));
                return false;
            }
            
            QString key = (this->*curLockLogic.key)(ui->vehicleVendorComboBox->currentText(), bManLock.seed).toHex();
            //qDebug() << "seed" << seed.toHex() << "key" << key;
            if (!data.mid(4, 8).compare(key, Qt::CaseInsensitive)) {
                packetSendCanData(ui->handshakeSendCanidLineEdit->text(), ui->keySendDataLineEdit->text());
                lockStep = LOCK_STEP_HANDSHAKE_OK;
                
                emit signalEventInfo(tr("握手成功 - 密钥：0x%1").arg(key.toUpper()));
                bManLock.hsErrorCount = 0;
                // 799 被动锁车采用799转速 用于区分主动锁车的800
                int lockrpm = ui->lockRpmLineEdit->text().toInt();
                if (LOCK_STATUS_LOCKED == lockStatus && (lockrpm - 1) == bManLock.maxRpm) { 
                    bManLock.maxRpm = ECU_DEFAULT_MAX_RPM;
                    lockStatus = LOCK_STATUS_UNLOCKED;
                    emit signalEventInfo(tr("被动锁车 - 解除限速"));
                }
            } else {
                //packetSendCanData(ui->handshakeSendCanidLineEdit->text(), "3FFFFFFFFFFFFFFF");
                lockStep = LOCK_STEP_HANDSHAKE_ER;
                
                emit signalEventInfo(tr("握手失败 - 密钥不匹配"));
            }
        }
    }

    return (lockStep >= LOCK_STEP_HANDSHAKE_OK);
}

QByteArray LockVehicleDialog::boschManKey(const QString &engineVendor, const QByteArray &seed)
{
    Q_UNUSED(engineVendor);

    const uint8_t a = 0x7D;
    const uint8_t b = 0xC6;
    const uint8_t c = 0x20;
    const uint8_t d = 0xAA;

    QByteArray key;
    key.append((a * (seed.at(0) * seed.at(0))) + (b * (seed.at(1) * seed.at(1))) + (c * (seed.at(0) * seed.at(1))));
    key.append((a * seed.at(0)) + (b * seed.at(1)) + (d * (seed[0] * seed.at(1))));
    key.append((a * (seed.at(2) * seed.at(3))) + (b * (seed.at(3) * seed.at(3))) + (c * (seed.at(2) * seed.at(3))));
    key.append((a * (seed.at(2) * seed.at(3))) + (b * seed.at(3)) + (d * (seed.at(2) * seed.at(3))));

    return key;
}

void LockVehicleDialog::boschManInit(const QString &vehicleVendor)
{
    Q_UNUSED(vehicleVendor);

    ui->gpsidLabel->setVisible(false);
    ui->gpsidLineEdit->setVisible(false);
    ui->clearGpsidButton->setVisible(false);

    ui->identifyCheckBox->setVisible(false);
    ui->identifyCanidLineEdit->setVisible(false);
    ui->identifyDataLineEdit->setVisible(false);
    ui->identifyPeriodSpinBox->setVisible(false);
    ui->stateCheckBox->setVisible(false);
    ui->stateCanidLineEdit->setVisible(false);
    ui->stateDataLineEdit->setVisible(false);
    ui->statePeriodSpinBox->setVisible(false);

    ui->encryptionKeyLabel->setVisible(false);
    ui->encryptionKeyLineEdit->setVisible(false);
}

void LockVehicleDialog::boschManPower(bool onOff)
{
    if (onOff) {
        ecuTimer->start(5000);
        if (bManLock.maxRpm < ECU_DEFAULT_MAX_RPM) {
            lockStatus = LOCK_STATUS_LOCKED;
            int lockrpm = ui->lockRpmLineEdit->text().toInt();
            if (bManLock.maxRpm == (lockrpm - 1)) {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(lockrpm));
            } else {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(bManLock.maxRpm));
            }
        }
        emit signalMaxRpmChanged(bManLock.maxRpm);
    }
}

void LockVehicleDialog::boschManTimeout(void)
{
    if (lockStatus >= LOCK_STATUS_BINDED && bManLock.hsErrorCount > 3) {
        lockStatus = LOCK_STATUS_LOCKED;
        int lockrpm = ui->lockRpmLineEdit->text().toInt();
        bManLock.maxRpm = lockrpm - 1; // 减1用于区分被动锁车的800 和主动锁车的800
        emit signalEventInfo(tr("被动锁车 - 限速%1rpm").arg(lockrpm));
    }
}

void LockVehicleDialog::boschManLogic(const QString &frameid, const QString &data)
{
    if (!boschManHandshake(frameid, data))
        return;

    switch (lockStep) {
        case LOCK_STEP_HANDSHAKE_OK:
            lockStep = LOCK_STEP_COMMUNICATE;
            break;
        case LOCK_STEP_COMMUNICATE: {
            QString bindRecvData = ui->bindRecvDataLineEdit->text().replace(" ", "");
            QString unbindRecvData = ui->unbindRecvDataLineEdit->text().replace(" ", "");
            QString lockRecvData = ui->lockRecvDataLineEdit->text().replace(" ", "");
            QString unlockRecvData = ui->unlockRecvDataLineEdit->text().replace(" ", "");

            #if 0
            qDebug() << "bindRecvCanid" << ui->bindRecvCanidLineEdit->text()
                     << "bindRecvData" << bindRecvData << "unbindRecvData" << unbindRecvData;
            qDebug() << "lockRecvCanid" << ui->lockRecvCanidLineEdit->text()
                     << "lockRecvData" << lockRecvData << "unlockRecvData" << unlockRecvData;
            #endif
            if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)
                && data.contains(bindRecvData, Qt::CaseInsensitive)) {
                lockStatus = LOCK_STATUS_BINDED;
                packetSendCanData(ui->bindSendCanidLineEdit->text(), ui->bindSendDataLineEdit->text());
                emit signalEventInfo(tr("已绑定"));
            } else if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)
                && data.contains(unbindRecvData, Qt::CaseInsensitive)) {
                ecuTimer->stop();
                lockStatus = LOCK_STATUS_UNBINDED;
                packetSendCanData(ui->bindSendCanidLineEdit->text(), ui->unbindSendDataLineEdit->text());
                bManLock.maxRpm = ECU_DEFAULT_MAX_RPM;
                emit signalEventInfo(tr("已解绑"));
            } else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)
                && data.contains(lockRecvData, Qt::CaseInsensitive)) {
                if (lockStatus < LOCK_STATUS_BINDED) {
                    emit signalEventInfo(tr("未绑定"));
                    //packetSendCanData(ui->lockSendCanidLineEdit->text(), "F2FFFFFFFFFFFFFF");
                    return;
                }
                
                lockStatus = LOCK_STATUS_LOCKED;
                packetSendCanData(ui->lockSendCanidLineEdit->text(), ui->lockSendDataLineEdit->text());

                QString mode = data.mid(2, 2);
                if (mode.contains("AF", Qt::CaseInsensitive)) {
                    bManLock.maxRpm = 1200;
                    emit signalEventInfo(tr("锁车配置 - 限速1200rpm"));
                } else if (mode.contains("C3", Qt::CaseInsensitive)) {
                    bManLock.maxRpm = 1500;
                    emit signalEventInfo(tr("锁车配置 - 限速1500rpm"));
                } else if (mode.contains("7D", Qt::CaseInsensitive)) {
                    bManLock.maxRpm = 0;
                    emit signalEventInfo(tr("锁车配置 - 发动机熄火"));
                } else {
                    emit signalEventInfo(tr("锁车配置 - 模式错误：0x%1").arg(mode.toUpper()));
                }
            } else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)
                && data.contains(unlockRecvData, Qt::CaseInsensitive)) {
                if (lockStatus < LOCK_STATUS_BINDED) {
                    emit signalEventInfo(tr("未绑定"));
                    //packetSendCanData(ui->lockSendCanidLineEdit->text(), "F3FFFFFFFFFFFFFF");
                    return;
                }
                
                lockStatus = LOCK_STATUS_UNLOCKED;
                packetSendCanData(ui->lockSendCanidLineEdit->text(), ui->unlockSendDataLineEdit->text());
                bManLock.maxRpm = ECU_DEFAULT_MAX_RPM;
                emit signalEventInfo(tr("锁车配置 - 解除限制"));
            }
            break;
         }
         default: break;
    }
}

/////////////////////// 博世 - 曼 END /////////////////////////////


/////////////////////// 博世 - 潍柴 START /////////////////////////////
bool LockVehicleDialog::boschWeiChaiHandshake(const QString &frameid, const QString &data)
{
    if (frameid.compare(ui->handshakeRecvCanidLineEdit->text(), Qt::CaseInsensitive))
        return false;

    wcresponse_t *pv = &(bWeiChaiLock.response.v);
    if (data.left(16).compare(bWeiChaiLock.md5key, Qt::CaseInsensitive)) {
        emit signalEventInfo(tr("握手失败 - 密钥不匹配"));
        pv->ackStatus = WEICHAI_ACK_HANDSHAKE_FAILURE;
        pv->keyStatus = WEICHAI_STATE_KEY_ER;
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bWeiChaiLock.response.val);
        return false;
    }

    ecuTimer->setInterval(HEARTBEAT_PERIOD);
    pv->ackStatus = WEICHAI_ACK_HANDSHAKE_SUCCESS;
    pv->keyStatus = WEICHAI_STATE_KEY_OK;
    emit signalEventInfo(tr("握手成功 - 密钥：0x%1").arg(data.toUpper()));

    bWeiChaiLock.hsErrorCount = 0;
    int lockrpm = ui->lockRpmLineEdit->text().toInt();
    if (LOCK_STATUS_LOCKED == lockStatus && (lockrpm - 1) == bWeiChaiLock.maxRpm) {
        bWeiChaiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
        lockStatus = LOCK_STATUS_UNLOCKED;
        emit signalEventInfo(tr("被动锁车 - 解除限速"));
    }

    return true;
}

void LockVehicleDialog::boschWeiChaiSeed(const QString &engineVendor, const QString &solidkey)
{
    Q_UNUSED(engineVendor);

    QString seedSendData = ui->seedSendDataLineEdit->text().replace(" ", "");

    QByteArray seed = generateSeed(5);
    seedSendData.replace(0, 10, seed.toHex().toUpper());
    packetSendCanData(ui->handshakeSendCanidLineEdit->text(), seedSendData);
    emit signalEventInfo(tr("握手中 - 随机值：0x%1, 固定密钥：0x%2")
        .arg(seedSendData.mid(0, 10)).arg(solidkey));

    seedSendData = seedSendData.insert(0, solidkey).left(16);
    bWeiChaiLock.md5key = QCryptographicHash::hash(hexStr2ByteArray(seedSendData), 
        QCryptographicHash::Md5).toHex().toUpper().left(16);
    qDebug() << "solidkey" << solidkey << "seedSendData" << seedSendData 
             << "bWeiCaiLock.md5key" << bWeiChaiLock.md5key;

    bWeiChaiLock.hsErrorCount++;
    if (!bWeiChaiLock.solidKey.isEmpty()) {
        if (bWeiChaiLock.seedCount <= POWERUP_SEED_CNT && (bWeiChaiLock.hsErrorCount > 2)) {
            int lockrpm = ui->lockRpmLineEdit->text().toInt();
            bWeiChaiLock.maxRpm = lockrpm - 1;
            lockStatus = LOCK_STATUS_LOCKED;
            emit signalEventInfo(tr("被动锁车 - 限速%1rpm").arg(lockrpm));
        }
    }
}

void LockVehicleDialog::boschWeiChaiInit(const QString &vehicleVendor)
{
    Q_UNUSED(vehicleVendor);

    ui->gpsidLabel->setVisible(false);
    ui->gpsidLineEdit->setVisible(false);
    ui->clearGpsidButton->setVisible(false);

    ui->identifyCheckBox->setVisible(true);
    ui->identifyCanidLineEdit->setVisible(true);
    ui->identifyDataLineEdit->setVisible(true);
    ui->identifyPeriodSpinBox->setVisible(true);
    ui->stateCheckBox->setVisible(true);
    ui->stateCanidLineEdit->setVisible(true);
    ui->stateDataLineEdit->setVisible(true);
    ui->statePeriodSpinBox->setVisible(true);

    ui->encryptionKeyLabel->setVisible(false);
    ui->encryptionKeyLineEdit->setVisible(false);

    bWeiChaiLock.solidKey.clear();
    QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
    settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
        curLockLogic.engineVendor, vehicleVendor));
    QString identifyCanid = settings.value(IDENTIFY_CANID).toString();
    QString identifyContent = settings.value(IDENTIFY_CONTENT).toString();
    int identifyPeriod = settings.value(IDENTIFY_PERIOD).toInt();
    QString stateCanid = settings.value(STATE_CANID).toString();
    QString stateContent = settings.value(STATE_CONTENT).toString();
    int statePeriod = settings.value(STATE_PERIOD).toInt();
    settings.endGroup();

    if (!identifyCanid.isEmpty()) {
        ui->identifyCheckBox->setChecked(true);
        ui->identifyCanidLineEdit->setText(identifyCanid);
        ui->identifyDataLineEdit->setText(identifyContent);
        ui->identifyPeriodSpinBox->setValue(identifyPeriod);
    }

    if (!stateCanid.isEmpty()) {
        ui->stateCheckBox->setChecked(true);
        ui->stateCanidLineEdit->setText(stateCanid);
        ui->stateDataLineEdit->setText(stateContent);
        ui->statePeriodSpinBox->setValue(statePeriod);
    }
}

void LockVehicleDialog::boschWeiChaiPower(bool onOff)
{
    if (onOff) {
        ecuTimer->start(1270); // 1.27 S
        if (ui->stateCheckBox->isChecked()) {
            ecuStateTimer->start(ui->statePeriodSpinBox->value());
        }
        if (ui->identifyCheckBox->isChecked()) {
            ecuIdentifyTimer->start(ui->identifyPeriodSpinBox->value());
        }

        bWeiChaiLock.seedCount = 0;
        wcresponse_t *pv = &(bWeiChaiLock.response.v);
        if (bWeiChaiLock.maxRpm < ECU_DEFAULT_MAX_RPM) {
            lockStatus = LOCK_STATUS_LOCKED;
            int lockrpm = ui->lockRpmLineEdit->text().toInt();
            if (bWeiChaiLock.maxRpm == (lockrpm - 1)) {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(lockrpm));
            } else {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(bWeiChaiLock.maxRpm));
            }
            pv->lockStatus = WEICHAI_STATE_LOCKED;
        } else {
            pv->lockStatus = WEICHAI_STATE_NOT_LOCK;
        }
        
        emit signalMaxRpmChanged(bWeiChaiLock.maxRpm);
    }
}

void LockVehicleDialog::boschWeiChaiTimeout(void)
{
    if (lockStatus >= LOCK_STATUS_BINDED) {
        // 上电前3次是2s
        if (++bWeiChaiLock.seedCount < POWERUP_SEED_CNT) { 
            ecuTimer->start(2000);   // 2 S
        } else {
            ecuTimer->start(HEARTBEAT_PERIOD); // // 真实ECU是10min 模拟统一20s一次握手
        }

        if (curLockLogic.seed) {
            (this->*curLockLogic.seed)(ui->vehicleVendorComboBox->currentText(), 
                bWeiChaiLock.solidKey.toUpper());
            lockStep = LOCK_STEP_KEY;
        }
    }
}

void LockVehicleDialog::boschWeiChaiStateTimeout(void)
{
    sendCanDataUInt8(ui->stateCanidLineEdit->text(), bWeiChaiLock.response.val);
}

void LockVehicleDialog::boschWeiChaiIdentifyTimeout(void)
{
    packetSendCanData(ui->identifyCanidLineEdit->text(), ui->identifyDataLineEdit->text());
}

void LockVehicleDialog::boschWeiChaiLogic(const QString &frameid, const QString &data)
{
    QString bindRecvData = ui->bindRecvDataLineEdit->text().replace(" ", "");
    QString unbindRecvData = ui->unbindRecvDataLineEdit->text().replace(" ", "");
    QString lockRecvData = ui->lockRecvDataLineEdit->text().replace(" ", "");
    QString unlockRecvData = ui->unlockRecvDataLineEdit->text().replace(" ", "");

    ecuStateTimer->start(ui->statePeriodSpinBox->value());

    if (LOCK_STEP_KEY == lockStep) {
        lockStep = boschWeiChaiHandshake(frameid, data)? LOCK_STEP_HANDSHAKE_OK : LOCK_STEP_HANDSHAKE_ER;
        if (LOCK_STEP_HANDSHAKE_OK == lockStep) {
            return;
        }
    }

#if 0
    qDebug() << "bindRecvCanid" << ui->bindRecvCanidLineEdit->text()
             << "bindRecvData" << bindRecvData << "unbindRecvData" << unbindRecvData;
    qDebug() << "lockRecvCanid" << ui->lockRecvCanidLineEdit->text()
             << "lockRecvData" << lockRecvData << "unlockRecvData" << unlockRecvData;
#endif

    wcresponse_t *pv = &(bWeiChaiLock.response.v);
    if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)
        && data.contains(bindRecvData, Qt::CaseInsensitive)) {
        bWeiChaiLock.gpsid = data.mid(4, 6);
        bWeiChaiLock.solidKey = data.mid(10, 6);
        pv->ackStatus = WEICHAI_ACK_GPSFUNCTION_ACTIVE;
        pv->keyStatus = WEICHAI_STATE_KEY_OK;
        pv->gpsidMatchStatus = WEICHAI_STATE_GPSID_MATCHED;
        lockStatus = LOCK_STATUS_BINDED;
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bWeiChaiLock.response.val);
        pv->ackStatus = 0;
        pv->activeStatus = WEICHAI_STATE_GPSFUNCTION_ACTIVE;
        emit signalEventInfo(tr("已绑定"));
    } else if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)
        && data.contains(unbindRecvData, Qt::CaseInsensitive)) {
        if (data.mid(10, 6).compare(bWeiChaiLock.solidKey, Qt::CaseInsensitive)) { // GPSID不匹配
            pv->gpsidMatchStatus = WEICHAI_STATE_GPSID_NOT_MATCH;
            sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bWeiChaiLock.response.val);
            emit signalEventInfo(tr("解绑失败"));
            return;
        }
        
        ecuTimer->stop();
        pv->ackStatus = WEICHAI_ACK_GPSFUNCTION_CLOSE;
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bWeiChaiLock.response.val);
        pv->ackStatus = 0;
        pv->activeStatus = WEICHAI_STATE_GPSFUNCTION_CLOSE;

        bWeiChaiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
        lockStatus = LOCK_STATUS_UNBINDED;
        emit signalEventInfo(tr("已解绑"));
    } else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        if (lockStatus < LOCK_STATUS_BINDED) {
            emit signalEventInfo(tr("未绑定"));
            sendCanDataUInt8(ui->lockSendCanidLineEdit->text(), bWeiChaiLock.response.val);
            return;
        }

        QString mode = data.mid(2, 2) + data.mid(0, 2);
        QString gpsid = data.mid(10, 6);
        if (bWeiChaiLock.gpsid != gpsid) {
            emit signalEventInfo(tr("锁车错误 GPSID不一致 - 0x%1 0x%2").arg(gpsid, bWeiChaiLock.gpsid));
            return;
        }
        
        uint16_t lockRpm = mode.toUShort(nullptr, 16) / 8;
        qDebug() << "mode" << mode << "lockRpm" << lockRpm;
        if (lockRpm >= ECU_DEFAULT_MAX_RPM) {
            bWeiChaiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
            lockStatus = LOCK_STATUS_UNLOCKED;
        } else {
            bWeiChaiLock.maxRpm = lockRpm;
            lockStatus = LOCK_STATUS_LOCKED;
        }
        sendCanDataUInt8(ui->lockSendCanidLineEdit->text(), bWeiChaiLock.response.val);

        if (bWeiChaiLock.maxRpm >= ECU_DEFAULT_MAX_RPM) {
            emit signalEventInfo(tr("锁车配置 - 解除限制"));
        } else if (0 == bWeiChaiLock.maxRpm) {
            emit signalEventInfo(tr("锁车配置 - 发动机熄火"));
        } else {
            emit signalEventInfo(tr("锁车配置 - 限速%1rpm").arg(bWeiChaiLock.maxRpm));
        }
    }
    // 更新状态帧的值
    static uint8_t response[8];
    if (memcmp(response, bWeiChaiLock.response.val, 8)) {
        memcpy(response, bWeiChaiLock.response.val, 8);
        QString text;
        for (int i = 0; i < 8; ++i) {
            text += QString("%1").arg(response[i], 2, 16, QLatin1Char('0')) + " ";
        }
        updateStateDataValue(true, text.toUpper());
    }
}

/////////////////////// 博世 - 潍柴 END /////////////////////////////


/////////////////////// 博世 - 玉柴 START /////////////////////////////
bool LockVehicleDialog::boschYuChaiHandshake(const QString &frameid, const QString &data)
{
    Q_UNUSED(frameid);
    
    ycresponse_t *pv = &(bYuChaiLock.response.v);
    if (data.contains("0000000000000000")) {
        bYuChaiLock.handpwd.clear();
    } else {
        ecuTimer->start(1000);
        pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_RECEIVED;
        if (bYuChaiLock.handpwd.contains(data, Qt::CaseInsensitive)) {
            pv->checkStatus = YUCHAI_CHECK_STATUS_PASS;
            pv->faultStatus = YUCHAI_FAULT_STATUS_NOT_TIMEOUT;
            emit signalEventInfo(tr("握手成功 - 密钥：0x%1").arg(data.toUpper()));
        } else {
            pv->checkStatus = YUCHAI_CHECK_STATUS_NOT_PASS;
            pv->faultStatus = YUCHAI_FAULT_STATUS_NOT_TIMEOUT;
            emit signalEventInfo(tr("握手失败 - 密钥不匹配 0x%1 0x%2")
                .arg(bYuChaiLock.handpwd.toUpper(), data.toUpper()));
        }
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bYuChaiLock.response.val);
    }

    if (YUCHAI_CHECK_STATUS_PASS == pv->checkStatus) {
        int lockrpm = ui->lockRpmLineEdit->text().toInt();
        if (LOCK_STATUS_LOCKED == lockStatus && (lockrpm - 1) == bYuChaiLock.maxRpm) {
            bYuChaiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
            lockStatus = LOCK_STATUS_UNLOCKED;
            emit signalEventInfo(tr("被动锁车 - 解除限速"));
        }
    }

    return (YUCHAI_CHECK_STATUS_PASS == pv->checkStatus);
}

void LockVehicleDialog::boschYuChaiSeed(const QString &engineVendor, const QString &gpsid)
{
    Q_UNUSED(engineVendor);

    ycresponse_t *pv = &(bYuChaiLock.response.v);
    if (YUCHAI_CHECK_STATUS_PASS != pv->checkStatus && bYuChaiLock.handpwd.isEmpty()) {
        lockStep = LOCK_STEP_SEED;
        ecuTimer->start(3000);
        
        uint32_t mask = 0xAB48EF35;
        QByteArray seed = generateSeed(4);
        memcpy(pv->seed, seed.data(), seed.size());
        QString value = seed.toHex().toUpper();
        emit signalEventInfo(tr("握手中 - 随机值：0x%1, 终端ID：0x%2").arg(value).arg(gpsid.toUpper()));

        QByteArray rand;
        rand.append(seed);
        rand.append((char *)&mask, sizeof(mask));
        //qDebug() << "rand" << rand.toHex().toUpper();
        uint8_t newval[8] = {0};
        get_rand_str((uint8_t *)rand.data(), newval, rand.size());
        //qDebug("newval:%s", (char *)newval);
        QByteArray baval((char *)newval, sizeof(newval));
        QByteArray md5 = QCryptographicHash::hash(baval, QCryptographicHash::Md5).toHex().toUpper();
        bYuChaiLock.md5pwd = md5.mid(8, 16);
        bYuChaiLock.handpwd = bYuChaiLock.md5pwd.mid(4, 8);
        for (int i = 0; i < gpsid.length(); i += 2) {
            bYuChaiLock.handpwd.insert(2 + i * 2, gpsid.mid(i, 2));
        }
        bYuChaiLock.unbindpwd = bYuChaiLock.md5pwd.right(4);
        bYuChaiLock.lockpwd = bYuChaiLock.md5pwd.left(8);
        qDebug() << "gpsid" << gpsid
                 << "md5pwd" << bYuChaiLock.md5pwd
                 << "handpwd" << bYuChaiLock.handpwd
                 << "unbindpwd" << bYuChaiLock.unbindpwd
                 << "lockpwd" << bYuChaiLock.lockpwd;
    }  
    sendCanDataUInt8(ui->handshakeSendCanidLineEdit->text(), bYuChaiLock.response.val);
    pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_RECEIVED;
}

void LockVehicleDialog::boschYuChaiInit(const QString &vehicleVendor)
{
    Q_UNUSED(vehicleVendor);

    ui->gpsidLabel->setVisible(true);
    ui->gpsidLineEdit->setVisible(true);
    ui->clearGpsidButton->setVisible(true);

    ui->identifyCheckBox->setVisible(false);
    ui->identifyCanidLineEdit->setVisible(false);
    ui->identifyDataLineEdit->setVisible(false);
    ui->identifyPeriodSpinBox->setVisible(false);
    ui->stateCheckBox->setVisible(false);
    ui->stateCanidLineEdit->setVisible(false);
    ui->stateDataLineEdit->setVisible(false);
    ui->statePeriodSpinBox->setVisible(false);
    
    ui->encryptionKeyLabel->setVisible(false);
    ui->encryptionKeyLineEdit->setVisible(false);

    bYuChaiLock.gpsid.clear();
    QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
    settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
        curLockLogic.engineVendor, vehicleVendor));
    QString stateCanid = settings.value(STATE_CANID).toString();
    QString stateContent = settings.value(STATE_CONTENT).toString();
    int statePeriod = settings.value(STATE_PERIOD).toInt();
    settings.endGroup();

    if (!stateCanid.isEmpty()) {
        ui->stateCheckBox->setChecked(true);
        ui->stateCanidLineEdit->setText(stateCanid);
        ui->stateDataLineEdit->setText(stateContent);
        ui->statePeriodSpinBox->setValue(statePeriod);
    }
}

void LockVehicleDialog::boschYuChaiPower(bool onOff)
{
    if (onOff) {
        if (bYuChaiLock.maxRpm < ECU_DEFAULT_MAX_RPM) {
            lockStatus = LOCK_STATUS_LOCKED;
            int lockrpm = ui->lockRpmLineEdit->text().toInt();
            if (bYuChaiLock.maxRpm == (lockrpm - 1)) {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(lockrpm));
            } else {
                emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(bYuChaiLock.maxRpm));
            }
        }

        ycresponse_t *pv = &(bYuChaiLock.response.v);
        pv->checkStatus = YUCHAI_CHECK_STATUS_NOT_CHECK;
        pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_NOT_RECEIVED;
        pv->activeLock = bYuChaiLock.lockcmd; // 重新上电锁车状态才变化
        ecuTimer->start(3000);
        emit signalMaxRpmChanged(bYuChaiLock.maxRpm);
    }
}

void LockVehicleDialog::boschYuChaiTimeout(void)
{
    ycresponse_t *pv = &(bYuChaiLock.response.v);
    if (bYuChaiLock.gpsid.isEmpty())
        return;

    static int heartbeatcnt = 0;
    heartbeatcnt += ecuTimer->interval();
    //qDebug() << "heartbeatcnt" << heartbeatcnt << "ecuTimer->interval()" << ecuTimer->interval();
    if (heartbeatcnt > HEARTBEAT_PERIOD) { // 真实ECU是30s-10min 模拟统一20s一次握手
        heartbeatcnt = 0;
        pv->checkStatus = YUCHAI_CHECK_STATUS_NOT_CHECK;
        bYuChaiLock.handpwd.clear();
        return;
    }

    if (YUCHAI_FAULT_STATUS_KEY_TIMEOUT == pv->faultStatus 
        || YUCHAI_FAULT_STATUS_SIGNAL_TIMEOUT == pv->faultStatus)
        return;

    bool result = true;
    switch (lockStep) {
        case LOCK_STEP_CHECK:
            pv->activeStatus = YUCHAI_ACTIVE_STATUS_ACTIVED;
            pv->checkStatus = YUCHAI_CHECK_STATUS_NOT_PASS;
            pv->faultStatus = YUCHAI_FAULT_STATUS_SIGNAL_TIMEOUT;
            pv->passiveLock = YUCHAI_PASSIVE_LOCK_STATUS_LIMIT;
            pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_NOT_RECEIVED;
            emit signalEventInfo(tr("请求信号超时"));
            break;
        case LOCK_STEP_SEED:
            pv->activeStatus = YUCHAI_ACTIVE_STATUS_ACTIVED;
            pv->passiveLock = YUCHAI_PASSIVE_LOCK_STATUS_LIMIT;
            pv->faultStatus = YUCHAI_FAULT_STATUS_KEY_TIMEOUT;
            pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_RECEIVED;
            emit signalEventInfo(tr("握手超时"));
            break;
        default:
            result = false;
            break;
    }

    if (result) {
        lockStatus = LOCK_STATUS_LOCKED;
        int lockrpm = ui->lockRpmLineEdit->text().toInt();
        bYuChaiLock.maxRpm = lockrpm - 1;
        emit signalEventInfo(tr("被动锁车 - 限速%1rpm").arg(lockrpm));
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bYuChaiLock.response.val);
    }
}


void LockVehicleDialog::boschYuChaiStateTimeout(void)
{
    ;
}

void LockVehicleDialog::boschYuChaiIdentifyTimeout(void)
{
    ;
}

void LockVehicleDialog::boschYuChaiLogic(const QString &frameid, const QString &data)
{
    QString bindRecvData = ui->bindRecvDataLineEdit->text().replace(" ", "");
    QString unbindRecvData = ui->unbindRecvDataLineEdit->text().replace(" ", "");
    QString lockRecvData = ui->lockRecvDataLineEdit->text().replace(" ", "");
    QString unlockRecvData = ui->unlockRecvDataLineEdit->text().replace(" ", "");

#if 0
    qDebug() << "bindRecvCanid" << ui->bindRecvCanidLineEdit->text()
             << "bindRecvData" << bindRecvData << "unbindRecvData" << unbindRecvData;
    qDebug() << "lockRecvCanid" << ui->lockRecvCanidLineEdit->text()
             << "lockRecvData" << lockRecvData << "unlockRecvData" << unlockRecvData;
#endif

    if (!frameid.compare(ui->handshakeRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        lockStep = boschYuChaiHandshake(frameid, data)? LOCK_STEP_HANDSHAKE_OK : LOCK_STEP_HANDSHAKE_ER;
        return;
    }

    ycresponse_t *pv = &(bYuChaiLock.response.v);
    if (!frameid.compare("18EA0021", Qt::CaseInsensitive) && data.contains("01FD", Qt::CaseInsensitive)) {
        if (bYuChaiLock.gpsid.isEmpty()) { // 没有gpsid则触发激活流程
            lockStep = LOCK_STEP_CHECK;
            lockStatus = LOCK_STATUS_UNBINDED;
            pv->checkStatus = YUCHAI_CHECK_STATUS_NOT_CHECK;
            pv->activeStatus = YUCHAI_ACTIVE_STATUS_NOT_ACTIVED;
            pv->faultStatus = YUCHAI_FAULT_STATUS_NOT_TIMEOUT;
            sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bYuChaiLock.response.val);
            pv->requestSignalStatus = YUCHAI_REQUST_SIGNAL_STATUS_RECEIVED;
            ecuTimer->start(3000);
        } else { // 有gpsid则触发握手校验流程
            boschYuChaiSeed(ui->vehicleVendorComboBox->currentText(), bYuChaiLock.gpsid);
        }
    } else if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)
        && data.contains(bindRecvData, Qt::CaseInsensitive)) {
        QString unbindpwd = data.mid(0, 4);
        QString gpsid = data.mid(4, 8);
        QString unlockpwd = data.mid(12, 4);
        qDebug() << "unbindpwd" << unbindpwd << "gpsid" << gpsid << "unlockpwd" << unlockpwd;
        if (unbindpwd.contains("0000") && gpsid.compare("00000000")) { // 修改gpsid
            if (LOCK_STEP_HANDSHAKE_OK == lockStep || gpsid == bYuChaiLock.gpsid) {
                bYuChaiLock.gpsid = gpsid;
                bYuChaiLock.unlockpwd = unlockpwd;
                ui->gpsidLineEdit->setText(bYuChaiLock.gpsid);
                pv->faultStatus = YUCHAI_FAULT_STATUS_NOT_TIMEOUT;
                emit signalEventInfo(tr("更换终端ID - GPSID 0x%1 成功").arg(gpsid));
            } else {
                pv->faultStatus = YUCHAI_FAULT_STATUS_KEY_INCONSISTENT;
                emit signalEventInfo(tr("更换终端ID - GPSID 0x%1 失败").arg(gpsid));
            }
        } else if (gpsid.contains("00000000") && unlockpwd.contains("0000")) { // 解绑
            if (lockStatus < LOCK_STATUS_BINDED || LOCK_STATUS_LOCKED == lockStatus) {
                emit signalEventInfo(tr("解绑失败 - 不在解锁状态"));
                return;
            }
            if (unbindpwd.compare(bYuChaiLock.unbindpwd, Qt::CaseInsensitive)) {
                emit signalEventInfo(tr("解绑失败 - 解绑密码错误 0x%1 0x%2")
                    .arg(bYuChaiLock.unbindpwd, unbindpwd));
                return;
            }

            ecuTimer->stop();
            on_clearGpsidButton_clicked();
            lockStatus = LOCK_STATUS_UNBINDED;
            pv->activeStatus = YUCHAI_ACTIVE_STATUS_NOT_ACTIVED;
            bYuChaiLock.gpsid.clear();
            emit signalEventInfo(tr("已解绑"));
        } else {
            if (!bYuChaiLock.gpsid.isEmpty() && bYuChaiLock.gpsid.compare(gpsid, Qt::CaseInsensitive)) {
                emit signalEventInfo(tr("绑定失败 - 已绑定 -> GPSID 0x%1").arg(bYuChaiLock.gpsid));
                return;
            }
            bYuChaiLock.gpsid = gpsid;
            bYuChaiLock.unlockpwd = unlockpwd;
            ui->gpsidLineEdit->setText(bYuChaiLock.gpsid);
            pv->activeStatus = YUCHAI_ACTIVE_STATUS_ACTIVED;
            lockStatus = LOCK_STATUS_BINDED;
            ecuTimer->start(20000);
            emit signalEventInfo(tr("已绑定"));
        }
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bYuChaiLock.response.val);
    }else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        if (lockStatus < LOCK_STATUS_BINDED) {
            emit signalEventInfo(tr("锁车配置 - 未绑定"));
            return;
        }
        
        uint8_t lockcmd = data.mid(0, 2).toUShort(nullptr, 16) & 0x03;
        QString rpm = data.mid(4, 2) + data.mid(2, 2);
        QString lockpwd = data.mid(8, 8);
        uint16_t lockRpm = rpm.toUShort(nullptr, 16) / 8;
        qDebug("lockcmd:0x%02x, lockRpm:%drpm", lockcmd, lockRpm);
        if (lockpwd.compare(bYuChaiLock.lockpwd, Qt::CaseInsensitive)) {
            emit signalEventInfo(tr("锁车配置 - 密码错误 0x%1 0x%2").arg(bYuChaiLock.lockpwd, lockpwd));
            return;
        }
        bYuChaiLock.lockcmd = lockcmd;
        if (YUCHAI_ACTIVE_LOCK_STATUS_NOT_LIMIT == lockcmd) { // 解除限制
            lockStatus = LOCK_STATUS_UNLOCKED;
            bYuChaiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
            emit signalEventInfo(tr("锁车配置 - 解除限制"));
        } else if (YUCHAI_ACTIVE_LOCK_STATUS_LIMIT_0RPM == lockcmd) { // 停机
            lockStatus = LOCK_STATUS_LOCKED;
            bYuChaiLock.maxRpm = 0;
            emit signalEventInfo(tr("锁车配置 - 停机"));
        } else if (YUCHAI_ACTIVE_LOCK_STATUS_LIMIT == lockcmd) { // 限速
            lockStatus = LOCK_STATUS_LOCKED;
            bYuChaiLock.maxRpm = lockRpm;
            emit signalEventInfo(tr("锁车配置 - 限速或扭矩%1rpm").arg(bYuChaiLock.maxRpm));
        }
        
        sendCanDataUInt8(ui->lockSendCanidLineEdit->text(), bYuChaiLock.response.val);
    }
}

/////////////////////// 博世 - 玉柴 END /////////////////////////////

/////////////////////// 博世 - 康明斯 START /////////////////////////////
bool LockVehicleDialog::boschKangMingSiHandshake(const QString &frameid, const QString &data)
{
    Q_UNUSED(frameid);

    bKangMingSiLock.msecCount = 0;
    int lockrpm = ui->lockRpmLineEdit->text().toInt();
    if (LOCK_STATUS_LOCKED == lockStatus && (lockrpm - 1) == bKangMingSiLock.maxRpm) {
        bKangMingSiLock.maxRpm = ECU_DEFAULT_MAX_RPM;
        lockStatus = LOCK_STATUS_UNLOCKED;
        emit signalEventInfo(tr("被动锁车 - 解除限速"));
    }

    QByteArray badata = hexStr2ByteArray(data);
    switch (lockStep) {
    case LOCK_STEP_ACTIVE_MODE: {
        static QByteArray content;
        if (0x01 == badata.at(0)) {
            content.clear();
            content.append(badata.right(7));
        } else if (0x02 == badata.at(0)) {
            content.append(badata.right(7));
        } else if (0x03 == badata.at(0)) {
            content.append(badata.mid(1, 2));
            uint16_t mkey[4];
            uint8_t gpsid[4], oseed[4];
            QByteArray kmsseed = bKangMingSiLock.seed;
            KMS_ActiveDecrypt((uint8_t *)content.data(), (uint8_t *)kmsseed.data(), mkey, gpsid, oseed);
            QByteArray deseed((char *)oseed, sizeof(oseed));
            kmsresponse_t *pv = &(bKangMingSiLock.response.v);
            pv->controlByte = KANGMINGSI_CONTROLBYTE_ECMSTATUS;
            if (kmsseed.left(4) != deseed) {
                QString seed1 = kmsseed.left(4).toHex().toUpper();
                QString seed2 = deseed.toHex().toUpper();
                pv->engineActivateStatus    = KANGMINGSI_ACTIVATE_STATUS_NOT_ACTIVATED;
                pv->oemDataStatus           = KANGMINGSI_OEMDATA_STATUS_WRONG_DATA;
                emit signalEventInfo(tr("激活解密错误 - SEED 0x%1 0x%2").arg(seed1, seed2));
                qDebug("mkey:0x%04X%04X%04X%04X, gpsid:0x%02X%02X%02X%02X", 
                    mkey[0], mkey[1], mkey[2], mkey[3], gpsid[0], gpsid[1], gpsid[2], gpsid[3]);
                qDebug() << "kmsseed" << kmsseed.toHex().toUpper() << "deseed" << deseed.toHex().toUpper();
            } else {
                qDebug("mkey:0x%04X%04X%04X%04X, gpsid:0x%02X%02X%02X%02X", 
                    mkey[0], mkey[1], mkey[2], mkey[3], gpsid[0], gpsid[1], gpsid[2], gpsid[3]);
                memcpy(bKangMingSiLock.mkey, mkey, sizeof(bKangMingSiLock.mkey));
                bKangMingSiLock.gpsid.replace(0, 4, (char *)gpsid, sizeof(gpsid));
                pv->engineActivateStatus    = KANGMINGSI_ACTIVATE_STATUS_ACTIVATED;
                pv->timeoutStatus           = KANGMINGSI_TIMEOUT_STATUS_NO_TIMEOUT;
                pv->lockoutStatus           = KANGMINGSI_LOCKOUT_STATUS_NA;
                pv->oemDataStatus           = KANGMINGSI_OEMDATA_STATUS_RIGHT_DATA;
                pv->activatedLockoutFeature = KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_X3_ACTIVATED;
                //pv->derateSpeedLow          = ((bKangMingSiLock.maxRpm << 3) & 0x00FF) >> 0;
                //pv->derateSpeedHigh         = ((bKangMingSiLock.maxRpm << 3) & 0xFF00) >> 8;
                pv->manualUnlockValidKeyNumbers = KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_4;
                lockStatus = LOCK_STATUS_BINDED;
                emit signalEventInfo(tr("已绑定"));
            }

            sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bKangMingSiLock.response.val);
        }
        break;
    }
    case LOCK_STEP_HANDSHAKE_MODE: {
        uint8_t deactive, gpsid[4];
        uint16_t deratespeed;
        QByteArray kmsseed = bKangMingSiLock.seed;
        KMS_HandshakeDecrypt((uint8_t *)badata.data(), (uint8_t *)kmsseed.data(), &deactive, gpsid, &deratespeed);
        qDebug("deratespeed:0x%04X", deratespeed);
        QByteArray bagpsid((char *)gpsid, sizeof(gpsid));
        qDebug() << "data" << data << "bKangMingSiLock.gpsid" << bKangMingSiLock.gpsid.toHex() 
                 << "bagpsid" << bagpsid.toHex();
        kmsresponse_t *pv = &(bKangMingSiLock.response.v);
        pv->controlByte = KANGMINGSI_CONTROLBYTE_ECMSTATUS;
        if (bKangMingSiLock.gpsid != bagpsid) {
            QString gpsid1 = bKangMingSiLock.gpsid.toHex().toUpper();
            QString gpsid2 = bagpsid.toHex().toUpper();
            pv->oemDataStatus = KANGMINGSI_OEMDATA_STATUS_WRONG_DATA;
            emit signalEventInfo(tr("握手解密错误 - GPSID 0x%1 0x%2").arg(gpsid1, gpsid2));
        } else {
            pv->oemDataStatus = KANGMINGSI_OEMDATA_STATUS_RIGHT_DATA;
            if (KANGMINGSI_DEACTIVE_COMMAND_NOT_DEAVTIVE == deactive) {
                bKangMingSiLock.maxRpm = (deratespeed >> 3);
                pv->derateSpeedLow  = ((bKangMingSiLock.maxRpm << 3) & 0x00FF) >> 0;
                pv->derateSpeedHigh = ((bKangMingSiLock.maxRpm << 3) & 0xFF00) >> 8;
                if (bKangMingSiLock.maxRpm >= ECU_DEFAULT_MAX_RPM) {
                    lockStatus = LOCK_STATUS_UNLOCKED;
                    pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_NOT_LOCKED;
                    emit signalEventInfo(tr("锁车配置 - 解锁配置"));
                } else if (0 == bKangMingSiLock.maxRpm) {
                    lockStatus = LOCK_STATUS_LOCKED;
                    pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_LOCKED_BY_COMMAND;
                    emit signalEventInfo(tr("锁车配置 - 发动机熄火"));
                } else {
                    lockStatus = LOCK_STATUS_LOCKED;
                    pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_LOCKED_BY_COMMAND;
                    emit signalEventInfo(tr("锁车配置 - 限速%1rpm").arg(bKangMingSiLock.maxRpm));
                }
            } else if (KANGMINGSI_DEACTIVE_COMMAND_DEAVTIVE == deactive) {
                lockStatus = LOCK_STATUS_UNBINDED;
                pv->engineActivateStatus    = KANGMINGSI_ACTIVATE_STATUS_NOT_ACTIVATED;
                pv->lockoutStatus           = KANGMINGSI_LOCKOUT_STATUS_NOT_LOCKED;
                pv->activatedLockoutFeature = KANGMINGSI_ACTIVATED_LOCKOUT_FEATURE_NO_ACTIVATED;
                pv->derateSpeedLow          = 0xFF;
                pv->derateSpeedHigh         = 0xFF;
                pv->manualUnlockValidKeyNumbers = KANGMINGSI_MANUAL_UNLOCK_VALID_KEY_NUMBER_0;
                emit signalEventInfo(tr("已解绑"));
                bKangMingSiLock.gpsid.clear();
            }
        }
        
        sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bKangMingSiLock.response.val);
        break;
    }
    default:
        break;
    }
    
    return true;
}

void LockVehicleDialog::boschKangMingSiInit(const QString &vehicleVendor)
{
    ui->gpsidLabel->setVisible(false);
    ui->gpsidLineEdit->setVisible(false);
    ui->clearGpsidButton->setVisible(false);

    ui->identifyCheckBox->setVisible(false);
    ui->identifyCanidLineEdit->setVisible(false);
    ui->identifyDataLineEdit->setVisible(false);
    ui->identifyPeriodSpinBox->setVisible(false);
    ui->stateCheckBox->setVisible(false);
    ui->stateCanidLineEdit->setVisible(false);
    ui->stateDataLineEdit->setVisible(false);
    ui->statePeriodSpinBox->setVisible(false);

    ui->encryptionKeyLabel->setVisible(true);
    ui->encryptionKeyLineEdit->setVisible(true);

    bKangMingSiLock.gpsid.clear();
    QSettings settings(SETTINGS_INI_DIR, QSettings::IniFormat);
    settings.beginGroup(Settings_IS(curLockLogic.ecuBrand, 
        curLockLogic.engineVendor, vehicleVendor));
    QString stateCanid = settings.value(STATE_CANID).toString();
    QString stateContent = settings.value(STATE_CONTENT).toString();
    int statePeriod = settings.value(STATE_PERIOD).toInt();
    settings.endGroup();

    if (!stateCanid.isEmpty()) {
        ui->stateCheckBox->setChecked(true);
        ui->stateCanidLineEdit->setText(stateCanid);
        ui->stateDataLineEdit->setText(stateContent);
        ui->statePeriodSpinBox->setValue(statePeriod);
    }
}

void LockVehicleDialog::boschKangMingSiPower(bool onOff)
{
    if (onOff) {
        QString text = ui->encryptionKeyLineEdit->text();
        uint32_t securitykey[4] = {0x32B162CD,0x729F6E13,0x5FAE2E12,0x12747A3E};

        for (int i = 0; (i*12) < text.length(); ++i) {
            QString value = text.mid(i*12+0, 2) + text.mid(i*12+3, 2) + text.mid(i*12+6, 2) + text.mid(i*12+9, 2);
            securitykey[i] = value.toUInt(nullptr, 16);
        }
        qDebug("securitykey 0x%08X 0x%08X 0x%08X 0x%08X", securitykey[0], securitykey[1], securitykey[2], securitykey[3]);
        KMS_SetSecurityKey(securitykey);

        if (!bKangMingSiLock.gpsid.isEmpty()) {
            kmsresponse_t *pv = &(bKangMingSiLock.response.v);
            pv->oemDataStatus = KANGMINGSI_OEMDATA_STATUS_NA;
            
            if (bKangMingSiLock.maxRpm < (0xFFFF >> 3)) {
                lockStatus = LOCK_STATUS_LOCKED;
                int lockrpm = ui->lockRpmLineEdit->text().toInt();
                if ((lockrpm - 1) == bKangMingSiLock.maxRpm) {
                    pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_DEFAULT_LOCK;
                    emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(lockrpm));
                } else {
                    pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_LOCKED_BY_COMMAND;
                    emit signalEventInfo(tr("已锁车 - 限速%1rpm").arg(bKangMingSiLock.maxRpm));
                }
            } else {
                pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_NA;
            }
            
            bKangMingSiLock.msecCount = 0;
            sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bKangMingSiLock.response.val);
            ecuTimer->start(100); // 100 ms
        }
        
        emit signalMaxRpmChanged(bKangMingSiLock.maxRpm);
    }
}

void LockVehicleDialog::boschKangMingSiTimeout(void)
{
    if (bKangMingSiLock.gpsid.isEmpty())
        return;

    if (bKangMingSiLock.msecCount++ == 20) {
        kmsresponse_t *pv = &(bKangMingSiLock.response.v);
        pv->timeoutStatus = KANGMINGSI_TIMEOUT_STATUS_IN_TIMEOUT;
        pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_DEFAULT_LOCK;
        lockStatus = LOCK_STATUS_LOCKED;
        ecuTimer->stop();
        int lockrpm = ui->lockRpmLineEdit->text().toInt();
        bKangMingSiLock.maxRpm = lockrpm - 1; // 减1用于区分被动锁车的800 和主动锁车的800
        emit signalEventInfo(tr("被动锁车 - 限速%1rpm").arg(lockrpm));
    }
    sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), bKangMingSiLock.response.val);
    #if 0
    if (!bKangMingSiLock.gpsid.isEmpty()) {
        lockStatus = LOCK_STATUS_HANDSHAKING;
        kmsresponse_t *pv = &(bKangMingSiLock.response.v);
        pv->timeoutStatus = KANGMINGSI_TIMEOUT_STATUS_IN_TIMEOUT;
        if (bKangMingSiLock.msecCount++ == 20) {
            pv->lockoutStatus = KANGMINGSI_LOCKOUT_STATUS_DEFAULT_LOCK;

            lockStatus = LOCK_STATUS_LOCKED;
            int lockrpm = ui->lockRpmLineEdit->text().toInt();
            bKangMingSiLock.maxRpm = lockrpm - 1;
            emit signalEventInfo(tr("被动锁车 - 限速%1rpm").arg(lockrpm));
        }

        if (bKangMingSiLock.msecCount < 100 && 0 == bKangMingSiLock.msecCount % 20) {
            sendCanDataUInt8(ui->lockSendCanidLineEdit->text(), bKangMingSiLock.response.val);
        }
    }
    #endif
}

void LockVehicleDialog::boschKangMingSiStateTimeout(void)
{
    ;
}

void LockVehicleDialog::boschKangMingSiIdentifyTimeout(void)
{
    ;
}

void LockVehicleDialog::boschKangMingSiLogic(const QString &frameid, const QString &data)
{
    QString bindRecvData = ui->bindRecvDataLineEdit->text().replace(" ", "");
    QString unbindRecvData = ui->unbindRecvDataLineEdit->text().replace(" ", "");
    QString lockRecvData = ui->lockRecvDataLineEdit->text().replace(" ", "");
    QString unlockRecvData = ui->unlockRecvDataLineEdit->text().replace(" ", "");
    QString seedRecvData = ui->seedRecvDataLineEdit->text().replace(" ", "");

#if 0
    qDebug() << "bindRecvCanid" << ui->bindRecvCanidLineEdit->text()
             << "bindRecvData" << bindRecvData << "unbindRecvData" << unbindRecvData;
    qDebug() << "lockRecvCanid" << ui->lockRecvCanidLineEdit->text()
             << "lockRecvData" << lockRecvData << "unlockRecvData" << unlockRecvData;
#endif

    if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        bool flag = false;
        //qDebug() << "data" << data << "bindRecvData" << bindRecvData;
        if (data.contains(bindRecvData, Qt::CaseInsensitive)) {
            flag = true;
            lockStep = LOCK_STEP_ACTIVE_MODE;
        } else if (data.contains(seedRecvData, Qt::CaseInsensitive)) {
            if (bKangMingSiLock.gpsid.isEmpty()) {
                emit signalEventInfo(tr("未绑定"));
                return;
            }
            flag = true;
            lockStep = LOCK_STEP_HANDSHAKE_MODE;
        }
        //qDebug() << "flag" << flag << "data" << data;
        if (flag) {
            ecuTimer->stop();
            bKangMingSiLock.seed = generateSeed(7);
            
            uint8_t handshake[8] = {0};
            handshake[0] = KANGMINGSI_CONTROLBYTE_X3V1;
            memcpy(&handshake[1], bKangMingSiLock.seed.data(), bKangMingSiLock.seed.size());
            sendCanDataUInt8(ui->bindSendCanidLineEdit->text(), handshake);
        }
    } else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        ecuTimer->stop();
        boschKangMingSiHandshake(frameid, data);
    }
}

/////////////////////// 博世 - 康明斯 END /////////////////////////////

/////////////////////// 博世 - 云内 START /////////////////////////////
bool LockVehicleDialog::boschYunNeiHandshake(const QString &frameid, const QString &data)
{
    Q_UNUSED(frameid);
    Q_UNUSED(data);

    return true;
}

void LockVehicleDialog::boschYunNeiInit(const QString &vehicleVendor)
{
    Q_UNUSED(vehicleVendor);
}

void LockVehicleDialog::boschYunNeiPower(bool onOff)
{
    if (onOff) {
        ecuTimer->start(500); // 100 ms
    }
}

void LockVehicleDialog::boschYunNeiTimeout(void)
{

}

void LockVehicleDialog::boschYunNeiStateTimeout(void)
{
    ;
}

void LockVehicleDialog::boschYunNeiIdentifyTimeout(void)
{
    ;
}

void LockVehicleDialog::boschYunNeiLogic(const QString &frameid, const QString &data)
{
    Q_UNUSED(data);
    
    QString bindRecvData = ui->bindRecvDataLineEdit->text().replace(" ", "");
    QString unbindRecvData = ui->unbindRecvDataLineEdit->text().replace(" ", "");
    QString lockRecvData = ui->lockRecvDataLineEdit->text().replace(" ", "");
    QString unlockRecvData = ui->unlockRecvDataLineEdit->text().replace(" ", "");

#if 0
    qDebug() << "bindRecvCanid" << ui->bindRecvCanidLineEdit->text()
             << "bindRecvData" << bindRecvData << "unbindRecvData" << unbindRecvData;
    qDebug() << "lockRecvCanid" << ui->lockRecvCanidLineEdit->text()
             << "lockRecvData" << lockRecvData << "unlockRecvData" << unlockRecvData;
#endif

    if (!frameid.compare(ui->bindRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        ;
    } else if (!frameid.compare(ui->lockRecvCanidLineEdit->text(), Qt::CaseInsensitive)) {
        ;
    }
}

/////////////////////// 博世 - 云内 END /////////////////////////////

//===================== 锁车功能接口 =============================//


LockVehicleDialog::LockLogicAssemble
LockVehicleDialog::getLockLogicAssemble(const QString &ecuBrand, const QString &engineVendor)
{
    int len = sizeof(lockLogicAssemble)/ sizeof(lockLogicAssemble[0]);
    for (int i = 0; i < len; ++i) {
        if (ecuBrand == lockLogicAssemble[i].ecuBrand 
            && engineVendor == lockLogicAssemble[i].engineVendor) {
            return lockLogicAssemble[i];
        }
    }

    return lockLogicAssemble[0];
}

void LockVehicleDialog::setEcuLockLogics(const QString &ecuBrand, const QString &engineVendor)
{
    qDebug() << "[EcuLock]ecuBrand" << ecuBrand << "engineVendor" << engineVendor;
    curLockLogic = getLockLogicAssemble(ecuBrand, engineVendor);
    lockdbdlg->setEcuType(ecuBrand, engineVendor);

    QString locktable = Database_LOCKCFG(curLockLogic.ecuBrand, curLockLogic.engineVendor);
    ui->lockLogicLineEdit->setText(locktable);

    model->setQuery(QString("select vehicleVendor from %1").arg(locktable));
    /* setModel(model)首次调用会指示到第一个，第二次调用则不会 
       所以强制调用setCurrentIndex选择第一个 */
    ui->vehicleVendorComboBox->setModel(model);
    ui->vehicleVendorComboBox->setCurrentIndex(0);
    qDebug() << "locktable" << locktable << "currentText" << ui->vehicleVendorComboBox->currentText();
}

void LockVehicleDialog::controlStateChanged(bool acc, bool ig, bool can)
{
    Q_UNUSED(ig);
    
    qDebug() << "controlStateChanged" << acc << ig << can;
    hasPowerup = acc && can;
    if (curLockLogic.power) {
        (this->*curLockLogic.power)(hasPowerup);
    }

    if (!hasPowerup) { // ACC关电处理
        ecuTimer->stop();
        ecuStateTimer->stop();
        ecuIdentifyTimer->stop();
    }
}

void LockVehicleDialog::lockVehicleCanEntry(const VCI_CAN_OBJ &can)
{
    QString frameid = QString("%1").arg(can.ID, 8, 16, QLatin1Char('0'));
    QString data;
    for (int i = 0; i < can.DataLen; ++i) {
        data += QString("%1").arg(can.Data[i], 2, 16, QLatin1Char('0'));
    }

    //qDebug() << "ecuBrand" << curLockLogic.ecuBrand 
    //         << "engineVendor" << curLockLogic.engineVendor
    //         << "frameid" << frameid << "data" << data;

    if (curLockLogic.logic) {
        (this->*curLockLogic.logic)(frameid, data.toUpper());
    }
}

QString LockVehicleDialog::vehicleVendor()
{
    return ui->vehicleVendorComboBox->currentText();
}

///////////////////////////////// 锁车功能 //////////////////////////////////////////

void LockVehicleDialog::on_saveToDBButton_clicked()
{
    int res = QMessageBox::question(this, tr("是否更新数据库"), 
                        tr("确定要更新到数据库中吗？\t"), 
                        QMessageBox::Ok, QMessageBox::No);
    if (QMessageBox::No == res)
        return;

    { // 更新状态帧和标识帧
        updateIdentifyDataValue(ui->identifyCheckBox->isChecked(), ui->identifyDataLineEdit->text());
        updateStateDataValue(ui->stateCheckBox->isChecked(), ui->stateDataLineEdit->text());
    }

    QString locktable = Database_LOCKCFG(curLockLogic.ecuBrand, curLockLogic.engineVendor);
    QSqlQuery query;
    query.exec(QString("select * from %1 where vehicleVendor='%2'")
        .arg(locktable, ui->vehicleVendorComboBox->currentText()));
    if (query.next()) { // 已存在则更新
        // 启动一个事务操作
        QSqlDatabase::database().transaction();
        bool rtn = query.exec(QString("update %1 set seedRecvCanid='%2',"
            "seedSendCanid='%3', seedRecvData='%4', seedSendData='%5', keyRecvData='%6',"
            "keySendData='%7', bindRecvCanid='%8', bindSendCanid='%9', bindRecvData='%10',"
            "bindSendData='%11', unbindRecvData='%12', unbindSendData='%13', lockRecvCanid='%14',"
            "lockSendCanid='%15', lockRecvData='%16', lockSendData='%17', unlockRecvData='%18',"
            "unlockSendData='%19', heartbeatRecvCanid='%20', heartbeatSendCanid='%21',"
            "heartbeatRecvData='%22', heartbeatSendData='%23' where vehicleVendor='%24'")
            .arg(locktable)
            .arg(ui->handshakeRecvCanidLineEdit->text())
            .arg(ui->handshakeSendCanidLineEdit->text())
            .arg(ui->seedRecvDataLineEdit->text())
            .arg(ui->seedSendDataLineEdit->text())
            .arg(ui->keyRecvDataLineEdit->text())
            .arg(ui->keySendDataLineEdit->text())
            .arg(ui->bindRecvCanidLineEdit->text())
            .arg(ui->bindSendCanidLineEdit->text())
            .arg(ui->bindRecvDataLineEdit->text()) /* 10 */
            .arg(ui->bindSendDataLineEdit->text())
            .arg(ui->unbindRecvDataLineEdit->text())
            .arg(ui->unbindSendDataLineEdit->text())
            .arg(ui->lockRecvCanidLineEdit->text())
            .arg(ui->lockSendCanidLineEdit->text())
            .arg(ui->lockRecvDataLineEdit->text())
            .arg(ui->lockSendDataLineEdit->text())
            .arg(ui->unlockRecvDataLineEdit->text())
            .arg(ui->unlockSendDataLineEdit->text())
            .arg(ui->heartbeatRecvCanidLineEdit->text()) /* 20 */
            .arg(ui->heartbeatSendCanidLineEdit->text())
            .arg(ui->heartbeatRecvDataLineEdit->text())
            .arg(ui->heartbeatSendDataLineEdit->text())
            .arg(ui->vehicleVendorComboBox->currentText()));

        if (rtn) {
            // 执行完成，则提交该事务，这样才可继续其他事务的操作
            QSqlDatabase::database().commit();
            QMessageBox::information(this, tr("提示"), tr("更新数据库成功！"));
        } else {
            // 执行失败，则回滚
            QSqlDatabase::database().rollback();
        }
    }else { // 不存在则插入
        query.exec(QString("select count (*) from %1").arg(locktable));
        query.next(); // 索引计数
        int count = query.value(0).toInt();
        
        query.exec(QString("insert into %1 values(%2, '%3', '%4', '%5', '%6', '%7', '%8',"
                    "'%9', '%10', '%11', '%12', '%13', '%14', '%15', '%16', '%17', '%18', '%19',"
                    "'%20', '%21', '%22', '%23', '%24', '%25')")
                    .arg(locktable)
                    .arg(count + 1)
                    .arg(ui->vehicleVendorComboBox->currentText())
                    .arg(ui->handshakeRecvCanidLineEdit->text())
                    .arg(ui->handshakeSendCanidLineEdit->text())
                    .arg(ui->seedRecvDataLineEdit->text())
                    .arg(ui->seedSendDataLineEdit->text())
                    .arg(ui->keyRecvDataLineEdit->text())
                    .arg(ui->keySendDataLineEdit->text())
                    .arg(ui->bindRecvCanidLineEdit->text()) /* 10 */
                    .arg(ui->bindSendCanidLineEdit->text())
                    .arg(ui->bindRecvDataLineEdit->text())
                    .arg(ui->bindSendDataLineEdit->text())
                    .arg(ui->unbindRecvDataLineEdit->text())
                    .arg(ui->unbindSendDataLineEdit->text())
                    .arg(ui->lockRecvCanidLineEdit->text())
                    .arg(ui->lockSendCanidLineEdit->text())
                    .arg(ui->lockRecvDataLineEdit->text())
                    .arg(ui->lockSendDataLineEdit->text())
                    .arg(ui->unlockRecvDataLineEdit->text()) /* 20 */
                    .arg(ui->unlockSendDataLineEdit->text())
                    .arg(ui->heartbeatRecvCanidLineEdit->text())
                    .arg(ui->heartbeatSendCanidLineEdit->text())
                    .arg(ui->heartbeatRecvDataLineEdit->text())
                    .arg(ui->heartbeatSendDataLineEdit->text()));
        QMessageBox::information(this, tr("提示"), tr("插入数据库成功！"));
    }

    QString text = ui->vehicleVendorComboBox->currentText();
    model->setQuery(QString("select vehicleVendor from %1").arg(locktable));
    ui->vehicleVendorComboBox->setModel(model);
    ui->vehicleVendorComboBox->setCurrentText(text);
}

void LockVehicleDialog::on_openDBButton_clicked()
{
    lockdbdlg->showDialog();
}

void LockVehicleDialog::updateLockLogicTable(const QString &locktable, const QString &vehicleVendor)
{
    qDebug() << "updateLockLogicTable locktable" << locktable << "locktable" << vehicleVendor;
    QSqlQuery query;
    query.exec(QString("select * from %1 where vehicleVendor='%2'").arg(locktable, vehicleVendor));
    if (query.next()) {
        ui->handshakeRecvCanidLineEdit->setText(query.value(2).toString());
        ui->handshakeSendCanidLineEdit->setText(query.value(3).toString());
        ui->seedRecvDataLineEdit->setText(query.value(4).toString());
        ui->seedSendDataLineEdit->setText(query.value(5).toString());
        ui->keyRecvDataLineEdit->setText(query.value(6).toString());
        ui->keySendDataLineEdit->setText(query.value(7).toString());
        ui->bindRecvCanidLineEdit->setText(query.value(8).toString());
        ui->bindSendCanidLineEdit->setText(query.value(9).toString()); /* 10 */
        ui->bindRecvDataLineEdit->setText(query.value(10).toString());
        ui->bindSendDataLineEdit->setText(query.value(11).toString());
        ui->unbindRecvDataLineEdit->setText(query.value(12).toString());
        ui->unbindSendDataLineEdit->setText(query.value(13).toString());
        ui->lockRecvCanidLineEdit->setText(query.value(14).toString());
        ui->lockSendCanidLineEdit->setText(query.value(15).toString());
        ui->lockRecvDataLineEdit->setText(query.value(16).toString());
        ui->lockSendDataLineEdit->setText(query.value(17).toString());
        ui->unlockRecvDataLineEdit->setText(query.value(18).toString());
        ui->unlockSendDataLineEdit->setText(query.value(19).toString());
        ui->heartbeatRecvCanidLineEdit->setText(query.value(20).toString());
        ui->heartbeatSendCanidLineEdit->setText(query.value(21).toString());
        ui->heartbeatRecvDataLineEdit->setText(query.value(22).toString());
        ui->heartbeatSendDataLineEdit->setText(query.value(23).toString());
    } else {
        ui->handshakeRecvCanidLineEdit->clear();
        ui->handshakeSendCanidLineEdit->clear();
        ui->seedRecvDataLineEdit->clear();
        ui->seedSendDataLineEdit->clear();
        ui->keyRecvDataLineEdit->clear();
        ui->keySendDataLineEdit->clear();
        ui->bindRecvCanidLineEdit->clear();
        ui->bindSendCanidLineEdit->clear();
        ui->bindRecvDataLineEdit->clear();
        ui->bindSendDataLineEdit->clear();
        ui->unbindRecvDataLineEdit->clear();
        ui->unbindSendDataLineEdit->clear();
        ui->lockRecvCanidLineEdit->clear();
        ui->lockSendCanidLineEdit->clear();
        ui->lockRecvDataLineEdit->clear();
        ui->lockSendDataLineEdit->clear();
        ui->unlockRecvDataLineEdit->clear();
        ui->unlockSendDataLineEdit->clear();
        ui->heartbeatRecvCanidLineEdit->clear();
        ui->heartbeatSendCanidLineEdit->clear();
        ui->heartbeatRecvDataLineEdit->clear();
        ui->heartbeatSendDataLineEdit->clear();
    }
}

void LockVehicleDialog::on_identifyCheckBox_stateChanged(int arg1)
{
    if (hasPowerup && arg1) {
        ecuIdentifyTimer->start(ui->identifyPeriodSpinBox->value());
    } else {
        ecuIdentifyTimer->stop();
    }
    ui->identifyCanidLineEdit->setEnabled(!arg1);
    ui->identifyDataLineEdit->setEnabled(!arg1);
    ui->identifyPeriodSpinBox->setEnabled(!arg1);
}

void LockVehicleDialog::on_stateCheckBox_stateChanged(int arg1)
{
    if (hasPowerup && arg1) {
        ecuStateTimer->start(ui->statePeriodSpinBox->value());
    } else {
        ecuStateTimer->stop();
    }
    ui->stateCanidLineEdit->setEnabled(!arg1);
    ui->stateDataLineEdit->setEnabled(!arg1);
    ui->statePeriodSpinBox->setEnabled(!arg1);
}

void LockVehicleDialog::on_vehicleVendorComboBox_currentTextChanged(const QString &arg1)
{
    //qDebug() << "arg1" << arg1;
    if (arg1.isEmpty())
        return;

    ui->saveToDBButton->setEnabled(!arg1.isEmpty() 
        && !ui->vehicleVendorComboBox->currentText().isEmpty());

    if (ui->vehicleVendorComboBox->isEditable())
        return;

    ui->vehicleVendorComboBox->setCurrentText(arg1);
    ui->identifyCheckBox->setChecked(false);
    ui->identifyCanidLineEdit->clear();
    ui->identifyDataLineEdit->clear();
    ui->identifyPeriodSpinBox->clear();

    ui->stateCheckBox->setChecked(false);
    ui->stateCanidLineEdit->clear();
    ui->stateDataLineEdit->clear();
    ui->statePeriodSpinBox->clear();

    if (curLockLogic.init) {
        (this->*curLockLogic.init)(arg1);
    }

    QString locktable = Database_LOCKCFG(curLockLogic.ecuBrand, curLockLogic.engineVendor);
    updateLockLogicTable(locktable, arg1);
}

void LockVehicleDialog::on_handshakeRecvCanidLineEdit_textChanged(const QString &arg1)
{
    ui->saveToDBButton->setEnabled(!arg1.isEmpty() 
        && !ui->vehicleVendorComboBox->currentText().isEmpty());
}

void LockVehicleDialog::on_identifyPeriodSpinBox_valueChanged(int arg1)
{
    ecuIdentifyTimer->setInterval(arg1);
}

void LockVehicleDialog::on_statePeriodSpinBox_valueChanged(int arg1)
{
    ecuStateTimer->setInterval(arg1);
}

void LockVehicleDialog::on_editCheckBox_clicked()
{
    ui->vehicleVendorComboBox->setEditable(ui->editCheckBox->isChecked());
}

void LockVehicleDialog::on_clearGpsidButton_clicked()
{
    bYuChaiLock.gpsid.clear();
    bKangMingSiLock.gpsid.clear();
    lockStatus = LOCK_STATUS_INIT;

    ui->gpsidLineEdit->setText(bYuChaiLock.gpsid);
}

