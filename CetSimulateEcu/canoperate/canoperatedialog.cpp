#include "canoperatedialog.h"
#include "ui_canoperatedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QTextCodec>
#include <QTimer>
#include <QDebug>

#define CANID_WIDTH         (80)
#define PERIOD_SEND_MAX     (8)
#define PERIOD_MIN_MS       (10)

#define ID_ENABLE   0
#define ID_CHANEL   1
#define ID_CANID    3
#define ID_DATA     5
#define ID_PERIOD   7

int elapsedMs[PERIOD_SEND_MAX] = {0};

CanOperateDialog::CanOperateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CanOperateDialog)
{
    ui->setupUi(this);

    //发送操作
    ui->DLCLineEdit->setEnabled(false);
    ui->frameIDLineEdit->setInputMask("HHHHHHHH");
    ui->sendDataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");

    // 周期操作
    for (int row = 0; row < PERIOD_SEND_MAX; ++row) {
        QCheckBox *checkBox = new QCheckBox(tr("启用"));
        QComboBox *comboBox = new QComboBox;
        comboBox->addItem(tr("通道 0"), CAN_CH0);
        comboBox->addItem(tr("通道 1"), CAN_CH1);
        QLabel *canidLabel = new QLabel(tr("自定义CANID%1:0x").arg(row+1));
        QLineEdit *canidLineEdit = new QLineEdit;
        canidLineEdit->setFixedWidth(CANID_WIDTH);
        canidLineEdit->setInputMask("HHHHHHHH");
        QLabel *dataLabel = new QLabel(tr("数据："));
        QLineEdit *dataLineEdit = new QLineEdit;
        dataLineEdit->setInputMask("HH HH HH HH HH HH HH HH");
        QLabel *periodLabel = new QLabel(tr("周期(ms)："));
        QSpinBox *periodSpinBox = new QSpinBox;
        periodSpinBox->setMinimum(10);
        periodSpinBox->setMaximum(30000);
        periodSpinBox->setValue(100);

        QFrame *frame = new QFrame;
        QHBoxLayout *layout = new QHBoxLayout(frame);
        layout->addWidget(checkBox);
        layout->addWidget(comboBox);
        layout->addWidget(canidLabel);
        layout->addWidget(canidLineEdit);
        layout->addWidget(dataLabel);
        layout->addWidget(dataLineEdit);
        layout->addWidget(periodLabel);
        layout->addWidget(periodSpinBox);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(item->sizeHint().width(), 40));
        ui->periodListWidget->addItem(item);
        ui->periodListWidget->setItemWidget(item, frame);

        connect(checkBox, &QCheckBox::clicked, this, &CanOperateDialog::doPeriodCheckBox);
    }

    m_periodTimer = new QTimer(this);
    connect(m_periodTimer, &QTimer::timeout, this, &CanOperateDialog::doPeriodTimeout);
    
    m_sendTimer = new QTimer(this);
    connect(m_sendTimer, &QTimer::timeout, this, &CanOperateDialog::doSendTimeout);

    // 播放操作
    ui->filepathProgressBar->setVisible(false);
    m_fileSendTimer = new QTimer(this);
    connect(m_fileSendTimer, &QTimer::timeout, this, &CanOperateDialog::doFileSendTimeout);

    slotControlStateChanged(false, false, false);
    m_periodTimer->start(PERIOD_MIN_MS);
}

CanOperateDialog::~CanOperateDialog()
{
    delete ui;
}

void CanOperateDialog::slotControlStateChanged(bool acc, bool ig, bool can)
{
    Q_UNUSED(ig);

    if (acc && can) {
        ui->sendDataButton->setEnabled(true);
        ui->periodListWidget->setEnabled(true);
        ui->playButton->setEnabled(true);
    } else {
        ui->sendDataButton->setEnabled(false);
        ui->periodListWidget->setEnabled(false);
        ui->playButton->setEnabled(false);
        if (tr("停止") == ui->playButton->text()) {
            on_playButton_clicked();
        }
    }
}

void CanOperateDialog::slotSendCANData(int chanel, const QString &canid, const QString &data)
{
    VCI_CAN_OBJ can;
    memset(&can, 0, sizeof(VCI_CAN_OBJ));

    //qDebug() << "CanOperateDialog:chanela" << chanel << "canid" << canid << "data" << data;
    
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

void CanOperateDialog::doSendTimeout()
{
    if (++m_sendCount > ui->sendFrameNumSpinBox->value()) {
        ui->sendDataButton->setText("发送");
        ui->chanelComboBox->setEnabled(true);
        ui->frameIDLineEdit->setEnabled(true);
        ui->sendDataLineEdit->setEnabled(true);
        m_sendTimer->stop();
        return;
    }

    emit signalSendCANData(m_chanel, m_can);
}

void CanOperateDialog::on_sendDataButton_clicked()
{
    if (tr("停止") == ui->sendDataButton->text()) {
        ui->sendDataButton->setText("发送");
        ui->chanelComboBox->setEnabled(true);
        ui->frameIDLineEdit->setEnabled(true);
        ui->sendDataLineEdit->setEnabled(true);
        m_sendTimer->stop();
        return;
    }
    ui->sendDataButton->setText("停止");
    ui->chanelComboBox->setEnabled(false);
    ui->frameIDLineEdit->setEnabled(false);
    ui->sendDataLineEdit->setEnabled(false);

    int chanel = ui->chanelComboBox->currentIndex();
    QString canid = ui->frameIDLineEdit->text();
    QString data = ui->sendDataLineEdit->text();
    int sendtype = ui->sendTypeComboBox->currentIndex();
    int frametype = ui->frameTypeComboBox->currentIndex();
    int frameformat = ui->frameFormatComboBox->currentIndex();

    memset(&m_can, 0, sizeof(VCI_CAN_OBJ));

    qDebug() << "CanOperateDialog:chanelb" << chanel << "canid" << canid << "data" << data;

    m_can.ID         = canid.toUInt(nullptr, 16);
    m_can.SendType   = sendtype;
    m_can.ExternFlag = frametype;
    m_can.RemoteFlag = frameformat;

    QString send = data;
    if (send.length() < 16) {
        send.append(QString(16 - send.length(), QChar('0')));
    }
    send.replace(QLatin1String(" "), QLatin1String(""));
    int count = 0;
    for (int i = 0; i < send.length(); i += 2) {
        QString byte = send.mid(i, 2);
        m_can.Data[count++] = byte.toInt(nullptr, 16);
    }
    m_can.DataLen = count;

    m_chanel = chanel;
    m_sendCount = 0;
    qDebug() << "ui->sendIntervalMsSpinBox->value()" << ui->sendIntervalMsSpinBox->value();
    m_sendTimer->start(ui->sendIntervalMsSpinBox->value());
}


//---------------------------  周期功能 ------------------------------//
void CanOperateDialog::doPeriodCheckBox(bool checked)
{
    QCheckBox *curCheckBox = static_cast<QCheckBox *>(QObject::sender());
    for (int row = 0; row < ui->periodListWidget->count(); ++row) {
        QListWidgetItem *item = ui->periodListWidget->item(row);
        if (!item)
            break;

        QLayout *layout = ui->periodListWidget->itemWidget(item)->layout();
        if (!layout)
            continue;

        QCheckBox *checkBox = (QCheckBox *)(layout->itemAt(ID_ENABLE)->widget());
        if (checkBox && curCheckBox == checkBox) {
            QComboBox *comboBox = (QComboBox *)(layout->itemAt(ID_CHANEL)->widget());
            QLineEdit *canidLineEdit = (QLineEdit *)(layout->itemAt(ID_CANID)->widget());
            QLineEdit *dataLineEdit = (QLineEdit *)(layout->itemAt(ID_DATA)->widget());
            QSpinBox *periodSpinBox = (QSpinBox *)(layout->itemAt(ID_PERIOD)->widget());
            comboBox->setDisabled(checked);
            canidLineEdit->setDisabled(checked);
            dataLineEdit->setDisabled(checked);
            periodSpinBox->setDisabled(checked);
        }
    }
}

void CanOperateDialog::doPeriodTimeout()
{
    for (int row = 0; row < ui->periodListWidget->count(); ++row) {
        QListWidgetItem *item = ui->periodListWidget->item(row);
        if (!item)
            break;

        QLayout *layout = ui->periodListWidget->itemWidget(item)->layout();
        if (!layout)
            continue;

        QCheckBox *checkBox = (QCheckBox *)(layout->itemAt(ID_ENABLE)->widget());
        if (!checkBox->isChecked())
            continue;

        QLineEdit *canidLineEdit = (QLineEdit *)(layout->itemAt(ID_CANID)->widget());
        QLineEdit *dataLineEdit = (QLineEdit *)(layout->itemAt(ID_DATA)->widget());
        if (!canidLineEdit->text().isEmpty() && !dataLineEdit->text().isEmpty()) {
            elapsedMs[row] += PERIOD_MIN_MS;
            QSpinBox *spinBox = (QSpinBox *)(layout->itemAt(ID_PERIOD)->widget());
            if (spinBox && elapsedMs[row] >= spinBox->value()) {
                elapsedMs[row] = 0;
                
                qDebug() << "row" << row << "spinBox->value()" << spinBox->value();
                QComboBox *comboBox = (QComboBox *)(layout->itemAt(ID_CHANEL)->widget());
                slotSendCANData(comboBox->currentIndex(), canidLineEdit->text(), dataLineEdit->text());
            }
        }
    }
}

//---------------------------  播放功能 ------------------------------//
bool CanOperateDialog::isCanDataValid(const QByteArray &data, int &chanel, QString &canid, QString &candata) 
{
    QTextCodec *tc = QTextCodec::codecForName("GBK");
    QString line = tc->toUnicode(data).replace("\n", "");
    QList<QString> list = line.split('\t');

    if (list.size() < 3) { // 时间 + CANID + CANDATA
        list = line.split(',');
    }

    QString tmp;
    int validcnt = 0;
    bool ok = false;
    for (int i = 0; i < list.size(); ++i) {
        tmp = list.at(i);
        tmp = tmp.replace(" ", "");
        //qDebug() << "tmp" << tmp;
        if (tmp.contains("x", Qt::CaseInsensitive) 
            || (tmp.length() >= 7 && tmp.length() <= 10)) {
            tmp = tmp.replace("0x", "").replace("0X", "").replace("x", "").replace("X", "");
            tmp.toULong(&ok, 16);
            if (tmp.length() >= 7 && ok) {
                chanel = CAN_CH0;
                canid = tmp;
                //qDebug() << "canid" << canid;
                validcnt++;
            }
        }

        if (tmp.length() == 16) {
            tmp.toULongLong(&ok, 16);
            if (ok) {
                candata = tmp;
                //qDebug() << "candata" << candata;
                validcnt++;
                break;
            }
        }
    }

    //qDebug() << "validcnt" << validcnt;
    //   43.229557 1  18DA7FFAx       Rx   d 8 21 6A CA 6A CA 6A CA 6A  // 1  OTP(22) Atom FA->7F : CF Seq.Nr.:   1         [ 6A CA 6A CA 6A CA 6A ]\n
    if (validcnt < 2) { // asc格式
        validcnt = 0;
        //qDebug() << "line" << line;
        if (line.contains("Rx   d 8 ", Qt::CaseInsensitive) 
            && (line.contains(" 1 ", Qt::CaseInsensitive) || line.contains(" 2 ", Qt::CaseInsensitive))
            && line.contains(".", Qt::CaseInsensitive)) {
            const char *key = NULL;
            if (line.contains(" 1 ", Qt::CaseInsensitive)) {
                key = " 1 ";
                chanel = CAN_CH0;
            } else {
                key = " 2 ";
                chanel = CAN_CH1;
            }
            int idx1 = line.indexOf(key);
            int idx2 = line.indexOf("Rx", idx1);
            if (-1 != idx2) {
                idx1 += strlen(key);
                tmp = line.mid(idx1, idx2 - idx1).replace(" ", "").replace("x", "");
                tmp.toULong(&ok, 16);
                if (tmp.length() >= 2 && ok) {
                    canid = tmp;
                    //qDebug() << "asc chanel" << chanel << "canid" << canid;
                    validcnt++;
                }
            }
            
            idx1 = line.indexOf("Rx   d 8 ") + strlen("Rx   d 8 ");
            idx2 = line.indexOf("  //", idx1);
            if (-1 == idx2) {
                idx2 = line.indexOf("Length", idx1);
            }
            tmp = line.mid(idx1, idx2 - idx1).replace(" ", "");
            if (tmp.length() == 16) {
                tmp.toULongLong(&ok, 16);
                if (ok) {
                    candata = tmp;
                    //qDebug() << "asc candata" << candata;
                    validcnt++;
                }
            }
        }
    }
    
    return (2 == validcnt);
}

bool CanOperateDialog::isTimeoutValid(const QByteArray &data, int &val) 
{
    QTextCodec *tc = QTextCodec::codecForName("GBK");
    QString line = tc->toUnicode(data).replace("\n", "");
    QList<QString> list = line.split('\t');

    if (list.size() < 3) { // 时间 + CANID + CANDATA
        list = line.split(',');
    }

    QString tmp;
    bool ok = false;
    bool isvalid = false;
    static int pretimems = 0;
    for (int i = 0; i < list.size(); ++i) {
        tmp = list.at(i);
        tmp = tmp.replace(" ", "");
        if (tmp.contains(".", Qt::CaseInsensitive) 
            || tmp.contains(":", Qt::CaseInsensitive)) {
            QString timems = tmp.split('.').last();
            if (timems.length() > 3) {
                timems = tmp.split(':').last();
            }
            if (timems.length() <= 3) {
                int curtimems = timems.toInt(&ok, 10);
                if (ok) {
                    isvalid = true;
                    val = (curtimems < pretimems)? (1000 - pretimems) + curtimems : curtimems - pretimems;
                    //qDebug() << "pretimems" << pretimems << "curtimems" << curtimems << "val" << val;
                    pretimems = curtimems;
                }
            }
        }
    }

    //   43.229557 1  18DA7FFAx       Rx   d 8 21 6A CA 6A CA 6A CA 6A  // 1  OTP(22) Atom FA->7F : CF Seq.Nr.:   1         [ 6A CA 6A CA 6A CA 6A ]\n
    if (!isvalid) { // asc格式
        if (line.contains("Rx   d 8 ", Qt::CaseInsensitive) 
            && (line.contains(" 1 ", Qt::CaseInsensitive) || line.contains(" 2 ", Qt::CaseInsensitive))
            && line.contains(".", Qt::CaseInsensitive)) {
            const char *key = (line.contains(" 1 ", Qt::CaseInsensitive))? " 1 " : " 2 ";
            //qDebug() << "line" << line;
            int idx1 = line.indexOf(".");
            int idx2 = line.indexOf(key, idx1);
            if (-1 != idx2) {
                tmp = line.left(idx2).replace(" ", "");
                QString timems = tmp.split('.').last();
                int curtimems = timems.toInt(&ok, 10);
                //qDebug() << "timems" << timems << "curtimems" << curtimems;
                if (ok) {
                    isvalid = true;
                    curtimems /= 1000;
                    val = (curtimems < pretimems)? (1000 - pretimems) + curtimems : curtimems - pretimems;
                    //qDebug() << "asc pretimems" << pretimems << "curtimems" << curtimems << "val" << val;
                    pretimems = curtimems;
                }
            }
        }

    }
    
    return isvalid;
}

void CanOperateDialog::doFileSendTimeout()
{
    static int count = 0;
    static QByteArray readline;
    static int cycle = 0;
    QString filepath = ui->filepathLineEdit->text();
    int mode = ui->playTypeComboBox->currentIndex();

    if (!m_fileSend.isOpen()) { // 打开文件
        m_fileSend.setFileName(filepath);
        if (!m_fileSend.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("警告"), tr("打开失败: %1").arg(filepath));
            return;
        }

        // 计算文件行数
        int maxcount = 0;
        QTextStream in(&m_fileSend);
        while (!in.atEnd()) {
            in.readLine();
            maxcount++;
        }
        qDebug() << "filepath" << filepath << "maxcount" << maxcount;
        m_fileSend.seek(0);
        ui->filepathProgressBar->setMaximum(maxcount);
        ui->filepathProgressBar->setValue(0);
        count = cycle = 0;
        if (1 == mode) { // 时间播放
            readline = m_fileSend.readLine();
        } else {
            int timeinterval = ui->timeIntervalSpinBox->value();
            m_fileSendTimer->start(timeinterval);
        }
    }

    int chanel = 0;
    QString canid;
    QString candata;
    switch (mode) {
        case 0: // 顺序播放
        case 2: { // 循环播放
            int interval = m_fileSendTimer->interval();
            int num = ui->frameNumSpinBox->value();
            num = (num/interval > 1)? (1 * interval) : num; // 每ms最多发送4帧
            //qDebug() << "num" << num << "interval" << interval;
            for (int cnt = 0; cnt < num; ++cnt) {
                readline = m_fileSend.readLine();
                //qDebug() << "readline" << readline;
                if (readline.isEmpty()) {
                    if (0 == mode) { // 顺序播放
                        on_playButton_clicked();
                    } else { // 循环播放
                        m_fileSend.seek(0);
                        ui->filenameLabel->setText(tr("已播放 %1 次：").arg(++cycle));
                        ui->filepathProgressBar->setValue(0);
                        count = 0;
                    }
                    break;
                }
                if (count <= ui->filepathProgressBar->maximum()) {
                    ui->filepathProgressBar->setValue(++count);
                }
                if (isCanDataValid(readline, chanel, canid, candata)) {
                    QString play = (0 == mode)? "顺序播放" : "循环播放";
                    qDebug() << play << "currentTime" << QTime::currentTime()
                             << "count" << count << "chanel" << chanel 
                             << "canid" << canid << "candata" << candata;
                    slotSendCANData(chanel, canid, candata);
                }
            }
            break;
        }
        case 1: {// 时间播放
            for (int val = 0; ; readline = m_fileSend.readLine()) {
                if (readline.isEmpty()) {
                    on_playButton_clicked();
                    break;
                }

                if (!isTimeoutValid(readline, val)) {
                    ui->filepathProgressBar->setValue(++count);
                    continue;
                }

                //qDebug() << "时间播放 val" << val;
                if (val > 1) {
                    m_fileSendTimer->start(val - 1);
                    break;
                } else {
                    if (count <= ui->filepathProgressBar->maximum()) {
                        ui->filepathProgressBar->setValue(++count);
                    }
                    if (isCanDataValid(readline, chanel, canid, candata)) {
                        qDebug() << "时间播放 currentTime" << QTime::currentTime() 
                                 << "count" << count << "chanel" << chanel 
                                 << "canid" << canid << "candata" << candata;
                        slotSendCANData(chanel, canid, candata);
                    }
                }
            }
            break;
        }
        default: break;
    }
}

void CanOperateDialog::on_openFileToolButton_clicked()
{
    QString filepath = QFileDialog::getOpenFileName(this, tr("打开"), 
        ".", "所有文件 (*.*);;TXT文件 (*.txt);;CSV文件 (*.csv);;ASC文件 (*.asc)");
    ui->filepathLineEdit->setText(filepath);
}

void CanOperateDialog::on_playButton_clicked()
{
    if (tr("播放") == ui->playButton->text()) {
        if (ui->filepathLineEdit->text().isEmpty())
            return;

        ui->filepathProgressBar->setMaximum(100000);
        ui->playButton->setText(tr("停止"));
        ui->filenameLabel->setText(tr("进度："));
        ui->filepathProgressBar->setVisible(true);
        ui->filepathLineEdit->setVisible(false);
        ui->playTypeComboBox->setEnabled(false);
        ui->openFileToolButton->setEnabled(false);
        m_fileSendTimer->start(1); // 1ms
    } else {
        ui->playButton->setText(tr("播放"));
        ui->filenameLabel->setText(tr("文件名："));
        ui->filepathProgressBar->setVisible(false);
        ui->filepathLineEdit->setVisible(true);
        ui->playTypeComboBox->setEnabled(true);
        ui->openFileToolButton->setEnabled(true);
        m_fileSendTimer->stop();
        m_fileSend.close();
    }
}

void CanOperateDialog::on_playTypeComboBox_currentIndexChanged(const QString &arg1)
{
    if (tr("顺序播放") == arg1) {
        ui->frameNumSpinBox->setEnabled(true);
        ui->timeIntervalSpinBox->setEnabled(true);
    } else {
        ui->frameNumSpinBox->setEnabled(false);
        ui->timeIntervalSpinBox->setEnabled(false);
    }
}

void CanOperateDialog::on_sendDataLineEdit_textChanged(const QString &arg1)
{
    QString text = arg1;
    
    text = text.replace(" ", "");
    ui->DLCLineEdit->setText(QString::number(text.length()));
}
