#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStandardItemModel>
#include <QDateTime>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QMetaType>
#include <QPluginLoader>
#include <QTextCodec>

#include <QDebug>
#include "includes.h"
#include "version.h"

#if (IS_RELEASE_VERSION > 0)
#define VERSION_TIP "正式版"
#else
#define VERSION_TIP "测试版"
#endif

#define CETRECORD_DIR "./Record"

#define CET_DEV_NAME "Cet模拟ECU"
#define CETSIMULATEECU_VERSION  CET_DEV_NAME " V" PRODUCT_VERSION_STR " " VERSION_TIP
#define CETSIMULATEECU_SETUP    "CetSimulateEcu-Setup"

// 不可继续增加 否则极限测试时 会出现动态内存不够
#define CACHE_MAX_ROWS      (100000)
#define SEND_ERROR_CNT      (4)

#define TABLE_INDEX_WIDTH       70
#define TABLE_TIME_WIDTH        165
#define TABLE_DIRECTION_WIDTH   55
#define TABLE_FRAMEID_WIDTH     80
#define TABLE_FRAMETYPE_WIDTH   55
#define TABLE_FRAMEFORMAT_WIDTH 55
#define TABLE_DATALEN_WIDTH     50
#define TABLE_DATAHEX_WIDTH     170

static int sg_rpmstep = 0;

#if (ENABLE_DEBUG_TOFILE > 0)
void customMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &str)
{
    Q_UNUSED(type);
    
    QString appRunLog;
    //获取程序当前运行目录
    QString currentPathName = QCoreApplication::applicationDirPath();
    if (!QFile::exists(currentPathName)) {
        appRunLog = "debug.log";
    } else {
        appRunLog = currentPathName + "/" + "debug.log";
    }
    QFile outFile(appRunLog);
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << str << endl;
}
#endif  // (ENABLE_DEBUG_TOFILE > 0)

QString MainWindow::usetime(int timestamp)
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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_baseRpm(ECU_DEFAULT_MAX_RPM/2),
    m_cetLicIface(nullptr),
    m_cetUpIface(nullptr),
    m_cetCanIface(nullptr)
{
    ui->setupUi(this);
    setWindowTitle(tr(CETSIMULATEECU_VERSION));

    qsrand((uint)QDateTime::currentSecsSinceEpoch());
    
#if (ENABLE_DEBUG_TOFILE > 0)
    //初始化qdebug的输出重定向到文件
    //qInstallMessageHandler(customMessageHandler);
#endif

    QPalette palette = ui->lockStatusLabel->palette();
    palette.setColor(QPalette::WindowText, QColor(0, 0, 255));
    ui->lockStatusLabel->setPalette(palette);
    ui->tiredStatusLabel->setPalette(palette);

    can0DataModel = new QStandardItemModel();
    setCANDataViewHeader(ui->can0DataTableView, can0DataModel);
    can1DataModel = new QStandardItemModel();
    setCANDataViewHeader(ui->can1DataTableView, can1DataModel);

    eventModel = new QStandardItemModel();
    setEventInfoViewHeader();

    ui->accSwitchButton->setChecked(false);
    ui->igSwitchButton->setChecked(false);
    ui->canStartButton->setChecked(false);
    ui->accSwitchButton->setEnabled(false);
    ui->igSwitchButton->setEnabled(false);
    ui->canStartButton->setEnabled(false);
    ui->rpmCheckBox->setEnabled(false);
    ui->speedCheckBox->setEnabled(false);
    ui->lockGroupBox->setVisible(false);
    ui->tiredGroupBox->setVisible(false);
    ui->eventGroupBox->setVisible(false);
    ui->canTabWidget->setTabText(0, tr("未知设备 通道 0"));
    ui->canTabWidget->setTabText(1, tr("未知设备 通道 1"));

    qRegisterMetaType<VCI_CAN_OBJ>("VCI_CAN_OBJ");
    qRegisterMetaType<VCI_ERR_INFO>("VCI_ERR_INFO");

    c0ifDlg = new CanIdFilterDialog(this);
    c1ifDlg = new CanIdFilterDialog(this);

    connect(ui->canStartButton, &QPushButton::clicked, ui->rpmCheckBox, &QCheckBox::setEnabled);
    connect(ui->canStartButton, &QPushButton::clicked, ui->speedCheckBox, &QCheckBox::setEnabled);

    ecDlg = new EcuConfigDialog(this);
    connect(ui->ecuConfigAction, &QAction::triggered, ecDlg, &EcuConfigDialog::showDialog);
    connect(ecDlg, &EcuConfigDialog::signalEcuTypeChanged, this, &MainWindow::doEcuTypeChanged);

    crThread = new CanRecvThread();
    connect(crThread, &CanRecvThread::signalRecvCANData, this, &MainWindow::doRecvCANDataShow);
    connect(crThread, &CanRecvThread::signalCANErrorInfo, this, &MainWindow::doRecvCANErrorShow);

    csThread = new CanSendThread();
    connect(csThread, &CanSendThread::signalSentCANData, this, &MainWindow::doSentCANDataShow);
    connect(csThread, &CanSendThread::signalErrorInfo, this, &MainWindow::doSentErrorInfoShow);

    ccDlg = new CanConfigDialog(this);
    connect(ui->canConfigAction, &QAction::triggered, ccDlg, &CanConfigDialog::slotShowDialog);
    connect(ccDlg, &CanConfigDialog::signalSendCANData, csThread, &CanSendThread::slotSendCANData);
    connect(ccDlg, &CanConfigDialog::signalCANDevice, this, &MainWindow::doCANDevice);

    fcDlg = new FuncConfigDialog(this);
    connect(ui->funcConfigAction, &QAction::triggered, fcDlg, &FuncConfigDialog::show);
    connect(ecDlg, &EcuConfigDialog::signalEcuTypeChanged, fcDlg, &FuncConfigDialog::slotEcuTypeChanged);
    connect(fcDlg, &FuncConfigDialog::signalEventInfo, this, &MainWindow::doRecvEventInfoShow);
    connect(fcDlg, &FuncConfigDialog::signalSendCANData, csThread, &CanSendThread::slotSendCANData);
    connect(fcDlg, &FuncConfigDialog::signalFuncTypeChanged, this, &MainWindow::doFuncTypeChanged);

    coDlg = new CanOperateDialog(this);
    ui->canOperateDockWidget->setWidget(coDlg);
    ui->canOperateDockWidget->setFixedHeight(200);
    connect(coDlg, &CanOperateDialog::signalSendCANData, csThread, &CanSendThread::slotSendCANData);

    connect(ui->accSwitchButton, &QPushButton::toggled, this, &MainWindow::doControlStateChanged);
    connect(ui->igSwitchButton, &QPushButton::toggled, this, &MainWindow::doControlStateChanged);
    connect(ui->canStartButton, &QPushButton::toggled, this, &MainWindow::doControlStateChanged);
    connect(this, &MainWindow::signalControlState, fcDlg, &FuncConfigDialog::slotControlStateChanged);
    connect(this, &MainWindow::signalControlState, ecDlg, &EcuConfigDialog::slotControlStateChanged);
    connect(this, &MainWindow::signalControlState, coDlg, &CanOperateDialog::slotControlStateChanged);

    secTimer = new QTimer(this);
    connect(secTimer, &QTimer::timeout, this, &MainWindow::doSecTimeout);

    msecTimer = new QTimer(this);
    connect(msecTimer, &QTimer::timeout, this, &MainWindow::doMSecTimeout);

    QDir().mkdir(CETRECORD_DIR);
    ui->baseRpmVerticalSlider->setMinimum(0);
    ui->baseRpmVerticalSlider->setMaximum(ECU_DEFAULT_MAX_RPM);
    ui->baseRpmVerticalSlider->setValue(ECU_DEFAULT_MAX_RPM/2);

    ui->baseRpmLabel->setWordWrap(true);
    ui->baseRpmLabel->setAlignment(Qt::AlignTop);
    QString text = tr("%1rpm").arg(ECU_DEFAULT_MAX_RPM/2).split("").join("\n").remove(0, 1);
    ui->baseRpmLabel->setText(text);

    initDllPlugin();
}

MainWindow::~MainWindow()
{
    delete ecDlg;
    delete ccDlg;
    delete fcDlg;
    delete c0ifDlg;
    delete c1ifDlg;
    delete coDlg;
    delete crThread;
    delete csThread;
    delete can0DataModel;
    delete can1DataModel;
    delete eventModel;
    delete secTimer;
    delete msecTimer;
    delete m_cetLicIface;
    delete m_cetUpIface;
    delete m_cetCanIface;
    delete ui;
}

QObject *MainWindow::loadDllPlugin(const QString &dllname)
{
    QObject *plugin = nullptr;
    QDir pluginsDir("./plugins");

    if (pluginsDir.isEmpty()) {
        pluginsDir.setPath("D:/CetToolLibs/plugins");
    }

    foreach (const QString &fileName, pluginsDir.entryList(QDir::Files)) {
        //qDebug() << "fileName" << fileName << "absoluteFilePath(fileName)" << pluginsDir.absoluteFilePath(fileName);
        QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
        plugin = pluginLoader.instance();
        if (plugin) {
            plugin->setParent(this);
            if (0 == dllname.compare(fileName, Qt::CaseInsensitive)) {
                break;
            }
        }
    }

    return plugin;
}

void MainWindow::initDllPlugin()
{
    QFileInfo fileInfo("./change.log");
    if (fileInfo.size() < 64) {
        QFile file("./change.log");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QString details;
        details.append("/***********************************************************************************************\n");
        details.append(tr("* 项目名称：%1\n").arg(QString(CETSIMULATEECU_SETUP).split('-').first()));
        details.append("* 项目简介：模拟ECU应用程序\n");
        details.append("* 创建日期：2022.05.11\n");
        details.append("* 创建人员：郑建宇(CetXiyuan)\n");
        details.append("* 版权说明：本程序仅为个人兴趣开发，有需要者可以免费使用，但是请不要在网络上进行恶意传播\n");
        details.append("***********************************************************************************************/\n\n");

        file.seek(0);
        file.write(details.toUtf8());
        file.close();
    }

    m_cetLicIface = qobject_cast<CetLicenseInterface *>(loadDllPlugin("CetLicensePlugin.dll"));
    if (!m_cetLicIface) {
        QMessageBox::warning(this, tr("警告"), tr("缺少 CetLicensePlugin.dll 库\t"));
    }

    m_cetUpIface = qobject_cast<CetUpdateInterface *>(loadDllPlugin("CetUpdatePlugin.dll"));
    if (!m_cetUpIface) {
        QMessageBox::warning(this, tr("警告"), tr("缺少 CetUpdatePlugin.dll 库\t"));
    }

    if (!m_cetLicIface || CetLicenseInterface::ACTIVATE_OK != m_cetLicIface->activate(PRODUCT_NAME)) {
        this->centralWidget()->setEnabled(false);
        this->setWindowTitle(windowTitle().append(tr(" [已到期]")));
    }

    if (m_cetUpIface) {
        m_cetUpIface->checkUpdate(CETSIMULATEECU_SETUP, PRODUCT_VERSION_STR, QCoreApplication::quit);
    }

    { // 自定义动态库加载区
        m_cetCanIface = qobject_cast<CetCANInterface *>(loadDllPlugin("CetCANPlugin.dll"));
        if (m_cetCanIface) {
            qDebug("CetCANPlugin:%s", m_cetCanIface->pluginVersion());
        }
    }
}

void MainWindow::setEventInfoViewHeader()
{
    int section = 0;

    eventModel->setColumnCount(4);
    eventModel->setHeaderData(section++, Qt::Horizontal, QStringLiteral("序号"));
    eventModel->setHeaderData(section++, Qt::Horizontal, QStringLiteral("时间"));
    eventModel->setHeaderData(section++, Qt::Horizontal, QStringLiteral("类型"));
    eventModel->setHeaderData(section++, Qt::Horizontal, QStringLiteral("内容"));

    ui->eventTableView ->setModel(eventModel);
    ui->eventTableView->setShowGrid(false);
    ui->eventTableView->setSortingEnabled(true);
    ui->eventTableView->verticalHeader()->hide();
    ui->eventTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    ui->eventTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->eventTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    /* 设置各列的宽度 */
    section = 0;
    ui->eventTableView->setColumnWidth(section++, TABLE_INDEX_WIDTH);
    ui->eventTableView->setColumnWidth(section++, TABLE_TIME_WIDTH);
    ui->eventTableView->setColumnWidth(section++, TABLE_FRAMEID_WIDTH);
    ui->eventTableView->setColumnWidth(section++, TABLE_DATAHEX_WIDTH + 230);
}

void MainWindow::setCANDataViewHeader(QTableView *tableView, QStandardItemModel *model)
{
    int section = 0;

    model->setColumnCount(8);
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("序号"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("时间"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("方向"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("帧ID"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("帧类型"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("帧格式"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("长度"));
    model->setHeaderData(section++, Qt::Horizontal, QStringLiteral("数据(十六进制)"));

    tableView->setModel(model);
    //tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->verticalHeader()->hide();
    tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    /* 设置各列的宽度 */
    section = 0;
    tableView->setColumnWidth(section++, TABLE_INDEX_WIDTH);
    tableView->setColumnWidth(section++, TABLE_TIME_WIDTH);
    tableView->setColumnWidth(section++, TABLE_DIRECTION_WIDTH);
    tableView->setColumnWidth(section++, TABLE_FRAMEID_WIDTH);
    tableView->setColumnWidth(section++, TABLE_FRAMETYPE_WIDTH);
    tableView->setColumnWidth(section++, TABLE_FRAMEFORMAT_WIDTH);
    tableView->setColumnWidth(section++, TABLE_DATALEN_WIDTH);
    tableView->setColumnWidth(section++, TABLE_DATAHEX_WIDTH);
}

void MainWindow::insertCANDataView(QTableView *tableView, const QDateTime &dateTime, 
                                bool isSend, const VCI_CAN_OBJ &can)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(tableView->model());
    int endrow = model->rowCount();
    QString index = QString("%1").arg(endrow + 1, 6, 10, QLatin1Char('0'));
    QString frameid = QString("%1").arg(can.ID, 8, 16, QLatin1Char('0'));
    QString datalen = QString::number(can.DataLen, 16);
    QString datatime = dateTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    QString datahex;
    for (int i = 0; i < can.DataLen; ++i) {
        datahex += QString("%1 ").arg(can.Data[i], 2, 16, QLatin1Char('0'));
    }
    frameid = frameid.toUpper();
    datahex = datahex.toUpper();

    QStandardItem *indexItem = new QStandardItem(index);
    QStandardItem *timeItem = new QStandardItem(datatime);
    QStandardItem *directionItem = new QStandardItem((isSend? "发送" : "接收"));
    QStandardItem *frameIdItem = new QStandardItem(frameid.insert(0, "0x"));
    QStandardItem *frameTypeItem = new QStandardItem((can.ExternFlag? "扩展帧" : "标准帧"));
    QStandardItem *frameFormatItem = new QStandardItem((can.RemoteFlag? "远程帧" : "数据帧"));
    QStandardItem *dataLenItem = new QStandardItem(datalen);
    QStandardItem *dataHexItem = new QStandardItem(datahex);

    indexItem->setTextAlignment(Qt::AlignCenter);
    timeItem->setTextAlignment(Qt::AlignCenter);
    directionItem->setTextAlignment(Qt::AlignCenter);
    frameIdItem->setTextAlignment(Qt::AlignCenter);
    frameTypeItem->setTextAlignment(Qt::AlignCenter);
    frameFormatItem->setTextAlignment(Qt::AlignCenter);
    dataLenItem->setTextAlignment(Qt::AlignCenter);

    int column = 0;
    model->setRowCount(endrow + 1);
    model->setItem(endrow, column++, indexItem);
    model->setItem(endrow, column++, timeItem);
    model->setItem(endrow, column++, directionItem);
    model->setItem(endrow, column++, frameIdItem);
    model->setItem(endrow, column++, frameTypeItem);
    model->setItem(endrow, column++, frameFormatItem);
    model->setItem(endrow, column++, dataLenItem);
    model->setItem(endrow, column++, dataHexItem);
    tableView->setCurrentIndex(model->index(endrow, column-1));

    if (model->rowCount() > CACHE_MAX_ROWS) {
        model->removeRows(0, model->rowCount());
    }
}

void MainWindow::doEcuTypeChanged(const QString &ecuBrand, const QString &engineVendor)
{
    QString ecuType = ecuBrand + "-" + engineVendor;
    ui->accSwitchButton->setEnabled(!ecuType.isEmpty());
    if (!ecuType.isEmpty()) {
        ui->ecuTypeLabel->setText(tr("ECU类型：%1").arg(ecuType));
    }

    ui->lockGroupBox->setVisible(false);
    ui->tiredGroupBox->setVisible(false);

    // 触发确定CAN配置
    ccDlg->slotOkButtonPressed();
}

void MainWindow::doSecTimeout()
{
    int minRpm = 0;
    static int rpm = 0;

    if (ccDlg->isBrake(minRpm)) {
        rpm = (rpm > minRpm)? rpm - 300 : minRpm;
        sg_rpmstep = 0;
    } else {
        /* 模拟转速 (+-200区间) */
        int maxrpm = fcDlg->maxEcuLockRpm();
        if (m_baseRpm > 0 && maxrpm > 0) {
            int baserpm = m_baseRpm < maxrpm ? m_baseRpm : maxrpm;
            if (baserpm > 200) {
                if (0 == sg_rpmstep) {
                    rpm = (baserpm - 200) + qrand() % 50;
                } else if (sg_rpmstep <= 7) {
                    rpm = rpm + qrand() % 50;
                } else {
                    rpm = rpm - qrand() % 50;
                }
                if(++sg_rpmstep > 14) sg_rpmstep = 0;
            } else {
                rpm = qrand() % baserpm;
                sg_rpmstep = 0;
            }
        } else {
            rpm = 0;
            sg_rpmstep = 0;
        }
        rpm = (rpm > maxrpm)? maxrpm : rpm;
    }
    
    ui->rpmDial->setValue(rpm);
    ui->rpmCheckBox->setText(tr("实时转速：%1 rpm").arg(rpm));
    if (ui->rpmCheckBox->isChecked()) {
       ccDlg->sendRpmCANData(rpm);
    }

    double speed = rpm * 60 * 3.14 * 0.724 / (1000 * 3.683 * 0.672);
    if (ui->speedCheckBox->isChecked()) {
        ccDlg->sendSpeedCANData(speed);
    }
    ui->speedLcdNumber->setDigitCount(5);
    ui->speedLcdNumber->display(speed);
    ui->speedCheckBox->setText(tr("实时车速：%1 km/h").arg(speed, 4, 'g', 5));

    if (ui->tiredGroupBox->isVisible()) {
        int runSeconds, restSeconds;
        fcDlg->tiredDriveSecEntry(ui->speedCheckBox->isChecked(), speed);
        fcDlg->tiredDriveParam(&runSeconds, &restSeconds);
        ui->runTimeLabel->setText(tr("运行：%1").arg(usetime(runSeconds)));
        ui->restTimeLabel->setText(tr("休息：%1").arg(usetime(restSeconds)));
    }
}

void MainWindow::doMSecTimeout()
{
    // 自定义动态库的数据处理
    if (m_cetCanIface) {
        int num = m_cetCanIface->getSendNum();
        //qDebug() << "num" << num;
        num = (num > 3)? 3 : num;
        for (int i = 0; i < num; ++i) {
            int chanel = 0;
            uint32_t canid = 0;
            uint8_t candata[8] = {0};
            m_cetCanIface->getSendData(&chanel, &canid, candata);
            if (0 != canid) {
                QString scanid = QString::number(canid, 16);
                QByteArray bdata((char *)candata, sizeof(candata));
                ccDlg->slotSendCANData(chanel, scanid, QString(bdata.toHex()));
            }
        }
    }
}

void MainWindow::doCANDevice(const QString &type, int devtype, int devid)
{
    ui->canTabWidget->setTabText(0, tr("%1 设备:%2 通道 0").arg(type).arg(devid));
    ui->canTabWidget->setTabText(1, tr("%1 设备:%2 通道 1").arg(type).arg(devid));

    crThread->setCANDevice(devtype, devid);
    csThread->setCANDevice(devtype, devid);
}

void MainWindow::doFuncTypeChanged(FuncConfigDialog::FuncType type, const QString &info, bool checked)
{
    switch (type) {
        case FuncConfigDialog::FUNC_LOCK: {
            ui->vehicleVendorLabel->setText(info);
            ui->lockGroupBox->setVisible(checked);
            break;
        }
        case FuncConfigDialog::FUNC_TIRED: {
            ui->tiredGroupBox->setVisible(checked);
            break;
        }
        default: break;
    }
}

void MainWindow::doRecvEventInfoShow(FuncConfigDialog::FuncType type, const QString &info)
{
    int endrow = eventModel->rowCount();
    QString index = QString("%1").arg(endrow + 1, 6, 10, QLatin1Char('0'));
    QString datatime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString event;
    switch (type) {
        case FuncConfigDialog::FUNC_LOCK:
            event = "销贷锁车";
            if (info.contains("绑定") || info.contains("锁车") 
                || info.contains("解绑")) {
                ui->lockStatusLabel->setText(tr("ECU状态：%1").arg(info));
            }
            break;
        case FuncConfigDialog::FUNC_TIRED:
            event = "疲劳驾驶";
            if (info.contains("已疲劳")) {
                ui->tiredStatusLabel->setText(tr("状态：已疲劳"));
            } else if (info.contains("即将疲劳")) {
                ui->tiredStatusLabel->setText(tr("状态：即将疲劳"));
            } else {
                ui->tiredStatusLabel->setText(tr("状态：未疲劳"));
            }
            break;
        default: break;
    }

    QStandardItem *indexItem = new QStandardItem(index);
    QStandardItem *timeItem = new QStandardItem(datatime);
    QStandardItem *eventItem = new QStandardItem(event);
    QStandardItem *infoItem = new QStandardItem(info);

    indexItem->setTextAlignment(Qt::AlignCenter);
    timeItem->setTextAlignment(Qt::AlignCenter);
    eventItem->setTextAlignment(Qt::AlignCenter);
    infoItem->setTextAlignment(Qt::AlignCenter);

    int column = 0;
    
    eventModel->setRowCount(endrow + 1);
    eventModel->setItem(endrow, column++, indexItem);
    eventModel->setItem(endrow, column++, timeItem);
    eventModel->setItem(endrow, column++, eventItem);
    eventModel->setItem(endrow, column++, infoItem);
    ui->eventTableView->setCurrentIndex(eventModel->index(endrow, column-1));

    if (eventModel->rowCount() > CACHE_MAX_ROWS) {
        eventModel->removeRows(0, eventModel->rowCount());
    }
}

void MainWindow::doRecvCANDataShow(int chanel, const VCI_CAN_OBJ &can)
{
    QDateTime dateTime = QDateTime::currentDateTime();
    //qDebug() << "doRecvCANDataShow: chanel" << chanel << "dateTime" << dateTime;
    switch (chanel) {
        case CAN_CH0:
            if (!c0ifDlg->isFilterCanId(QString::number(can.ID, 16))) {
                if (tr("暂停") == ui->can0DataPauseButton->text()) {
                   insertCANDataView(ui->can0DataTableView, dateTime, false, can);
                }

                // 锁车逻辑CAN入口
                if (ui->lockGroupBox->isVisible()) {
                    fcDlg->vehicleCanDataEntry(can);
                }

                // CAN的动态库数据入口
                if (m_cetCanIface) {
                    m_cetCanIface->recv(CAN_CH0, can.ID, can.Data);
                }
            }
            break;
        case CAN_CH1:
            if (!c1ifDlg->isFilterCanId(QString::number(can.ID, 16))) {
                if (tr("暂停") == ui->can1DataPauseButton->text()) {
                    insertCANDataView(ui->can1DataTableView, dateTime, false, can);
                }

                if (m_cetCanIface) {
                    m_cetCanIface->recv(CAN_CH1, can.ID, can.Data);
                }
            }
            break;
        default: break;
    }
}

void MainWindow::doRecvCANErrorShow(int chanel, const VCI_ERR_INFO &err)
{
    static int errcnt = 0;
    if (++errcnt < SEND_ERROR_CNT) {
        if (errcnt >= (SEND_ERROR_CNT/2) || ERR_CAN_BUSERR == err.ErrCode
            || ERR_CAN_BUSOFF == err.ErrCode) {
            if (tr("ACC关闭") == ui->accSwitchButton->text()) {
                ui->accSwitchButton->clicked(false);
            }
        }

        QString errstr;
        switch (err.ErrCode) {
            case ERR_CAN_OVERFLOW:
                errstr.append("CAN控制器内部FIFO溢出");
                break;
            case ERR_CAN_ERRALARM:
                errstr.append("CAN控制器错误报警");
                break;
            case ERR_CAN_PASSIVE:
                errstr.append("CAN控制器消极错误");
                break;
            case ERR_CAN_LOSE:
                errstr.append("CAN控制器仲裁丢失");
                break;
            case ERR_CAN_BUSERR:
                errstr.append("CAN控制器总线错误");
                break;
            case ERR_CAN_BUSOFF:
                errstr.append("总线关闭错误");
                break;
            default: 
                errstr.append("未知错误");
                break;
        }

        QMessageBox::critical(this, tr("CAN%1 接收错误").arg(chanel), 
            tr("错误码：%1 [ %2 ]\t\nPassive：%3 %4 %5，ArLost：%6").arg(err.ErrCode)
            .arg(errstr).arg(err.Passive_ErrData[0]).arg(err.Passive_ErrData[1])
            .arg(err.Passive_ErrData[2]).arg(err.ArLost_ErrData), QMessageBox::Ok);

        errcnt = 0;
    }
}

void MainWindow::doSentCANDataShow(int chanel, const VCI_CAN_OBJ &can)
{
    QDateTime dateTime = QDateTime::currentDateTime();
    //qDebug() << "doSentCANDataShow: chanel" << chanel << "dateTime" << dateTime;
    switch (chanel) {
        case CAN_CH0:
            if (!c0ifDlg->isFilterCanId(QString::number(can.ID, 16))) {
                if (tr("暂停") == ui->can0DataPauseButton->text()) {
                   insertCANDataView(ui->can0DataTableView, dateTime, true, can);
                }
            }
            break;
        case CAN_CH1:
            if (!c1ifDlg->isFilterCanId(QString::number(can.ID, 16))) {
                if (tr("暂停") == ui->can1DataPauseButton->text()) {
                    insertCANDataView(ui->can1DataTableView, dateTime, true, can);
                }
            }
            break;
        default: break;
    }
}

void MainWindow::doSentErrorInfoShow(const QString &info)
{
    static int errcnt = 0;
    //qDebug() << "doSentErrorInfoShow: currentTime" << QTime::currentTime();
    if (++errcnt <= 1) {
        ui->rpmCheckBox->setChecked(false);
        ui->speedCheckBox->setChecked(false);
        if (tr("ACC关闭") == ui->accSwitchButton->text()) {
            ui->accSwitchButton->clicked(false);
        }

        QMessageBox::critical(this, tr("错误"), info, QMessageBox::Ok);
        errcnt = 0;
    }
}

void MainWindow::doControlStateChanged()
{
    bool acc = ui->accSwitchButton->isChecked();
    bool ig  = ui->igSwitchButton->isChecked();
    bool can = ui->canStartButton->isChecked();

    if (m_cetCanIface) {
        m_cetCanIface->setControl(acc, ig, can);
    }
    //qDebug() << "MainWindow:acc" << acc << "ig" << ig << "can" << can;
    emit signalControlState(acc, ig, can);
}

void MainWindow::on_accSwitchButton_clicked()
{
    if (tr("ACC打开") == ui->accSwitchButton->text()) {
        ui->canStartButton->clicked(true);
        ui->canStartButton->setEnabled(true);

        ui->igSwitchButton->setEnabled(true);
        ui->accSwitchButton->setChecked(true);
        ui->accSwitchButton->setText(tr("ACC关闭"));

        sg_rpmstep = 0;
        secTimer->start(1000);
        msecTimer->start(10);
    } else {
        ui->speedLcdNumber->display(0);
        ui->rpmDial->setValue(0);
        ui->rpmCheckBox->setText(tr("实时转速：%1 rpm").arg(0));
        ui->speedCheckBox->setText(tr("实时车速：%1 km/h").arg(0));

        ui->canStartButton->setText(tr("CAN停止"));
        ui->canStartButton->clicked(false);
        ui->canStartButton->setEnabled(false);
        
        ui->igSwitchButton->setEnabled(false);
        ui->accSwitchButton->setChecked(false);
        ui->accSwitchButton->setText(tr("ACC打开"));

        secTimer->stop();
        msecTimer->stop();
    }
}

void MainWindow::on_igSwitchButton_clicked()
{

}

void MainWindow::on_canStartButton_clicked()
{
    if (tr("CAN启动") == ui->canStartButton->text()) {
        int ret = ccDlg->openCANDevice();
        if (ret < 0) {
            ui->canStartButton->setChecked(false);
            switch (ret) {
                case -1:
                    QMessageBox::critical(this, tr("错误"), tr("CAN设备打开失败 !"), QMessageBox::Ok);
                    break;
                case -2:
                    QMessageBox::critical(this, tr("错误"), tr("CAN初始化失败 !"), QMessageBox::Ok);
                    break;
                case -3:
                    QMessageBox::critical(this, tr("错误"), tr("CAN启动失败 !"), QMessageBox::Ok);
                    break;
                default: break;
            }
            return;
        }

        ui->canStartButton->setChecked(true);
        ui->canStartButton->setText(tr("CAN停止"));

        crThread->start(QThread::HighPriority);
        csThread->start(QThread::LowPriority);
    } else {
        ccDlg->closeCANDevice();

        ui->canStartButton->setChecked(false);
        ui->canStartButton->setText(tr("CAN启动"));
        
        crThread->quit();
        csThread->quit();
    }
}

void MainWindow::on_baseRpmVerticalSlider_sliderMoved(int position)
{
    sg_rpmstep = 0;
    m_baseRpm = position;
    QString text = tr("%1rpm").arg(position, 4, 10, QLatin1Char('0')).split("").join("\n").remove(0, 1);
    ui->baseRpmLabel->setText(text);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_F2: {
            QString opendir = tr("%1/%2").arg(QDir::currentPath(), CETRECORD_DIR);
            QDesktopServices::openUrl(QUrl::fromLocalFile(opendir));
            break;
        }
        default: break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::on_logToFileCheckBox_clicked(bool checked)
{
    if (checked) {
        //初始化qdebug的输出重定向到文件
        qInstallMessageHandler(customMessageHandler);
    } else {
        qInstallMessageHandler(0);
    }

    qDebug() << "on_logToFileCheckBox_clicked" << checked;
}

void MainWindow::on_eventVisibleButton_clicked()
{
    if (tr("事件显示") == ui->eventVisibleButton->text()) {
        ui->eventVisibleButton->setText(tr("事件隐藏"));
        ui->eventGroupBox->setVisible(true);
    } else {
        ui->eventVisibleButton->setText(tr("事件显示"));
        ui->eventGroupBox->setVisible(false);
    }
}

void MainWindow::on_eventInfoClearButton_clicked()
{
    eventModel->removeRows(0, eventModel->rowCount());
}

void MainWindow::on_eventInfoPauseButton_clicked()
{
    if (tr("暂停") == ui->eventInfoPauseButton->text()) {
        ui->eventInfoPauseButton->setText(tr("继续"));
        disconnect(fcDlg, &FuncConfigDialog::signalEventInfo, this, &MainWindow::doRecvEventInfoShow);
    } else {
        ui->eventInfoPauseButton->setText(tr("暂停"));
        connect(fcDlg, &FuncConfigDialog::signalEventInfo, this, &MainWindow::doRecvEventInfoShow);
    }
}

void MainWindow::on_eventInfoSaveButton_clicked()
{
    QString strtime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss");
    QString filePath = QFileDialog::getSaveFileName(this, tr("事件信息-另存为"), 
        tr("%1/EVENTINFO-%2.txt").arg(CETRECORD_DIR, strtime), 
        "文本文件 (*.txt);;所有文件 (*.*)");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("错误"), tr("%1 文件保存失败 !").arg(filePath), QMessageBox::Ok);
            return ;
        }

        int count = 0;
        QTextStream out(&file);
        for (int i = 0; i < eventModel->columnCount(); ++i) { // 表头打印
            QStandardItem *item = eventModel->horizontalHeaderItem(i);
            if (!item) {
                continue;
            }
            out << item->text() << "\t";
            if (0 == count) {
                out << "\t\t\t";
            } else if (1 == count) {
                out << "\t";
            } else if (2 == count) {
                out << "\t\t\t";
            }
            count++;
        }
        out << "\n";
        
        for (int i = 0 ; i < eventModel->rowCount(); ++i) { // 表内容打印
            for (int j = 0; j < eventModel->columnCount(); ++j) {
                QStandardItem *item = eventModel->item(i, j);
                if (!item) {
                    continue;
                }
                out << item->text() << "\t";
            }
            out << "\n";
            out.flush();
        }
        file.close();
    }
}

void MainWindow::slaveCANDataToFile(int chanel, QStandardItemModel *model)
{
    QString strtime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss");
    QString filePath = QFileDialog::getSaveFileName(this, tr("CAN数据-另存为"), 
        tr("%1/CAN%2-DATA-%3.txt").arg(CETRECORD_DIR).arg(chanel).arg(strtime), 
        "文本文件 (*.txt);;所有文件 (*.*)");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("错误"), tr("%1 文件保存失败 !").arg(filePath), QMessageBox::Ok);
            return ;
        }

        int count = 0;
        QTextStream out(&file);
        for (int i = 0; i < model->columnCount(); ++i) { // 表头打印
            QStandardItem *item = model->horizontalHeaderItem(i);
            if (!item) {
                continue;
            }
            out << item->text() << "\t";
            if (0 == count) {
                out << "\t\t\t";
            } else if (1 == count) {
                out << "\t";
            } else if (2 == count) {
                out << "\t";
            }
            count++;
        }
        out << "\n";
        
        for (int i = 0 ; i < model->rowCount(); ++i) { // 表内容打印
            for (int j = 0; j < model->columnCount(); ++j) {
                QStandardItem *item = model->item(i, j);
                if (!item) {
                    continue;
                }
                out << item->text() << "\t";
            }
            out << "\n";
            out.flush();
        }
        file.close();
    }
}

void MainWindow::on_can0DataClearButton_clicked()
{
    can0DataModel->removeRows(0, can0DataModel->rowCount());
}

void MainWindow::on_can0DataPauseButton_clicked()
{
    if (tr("暂停") == ui->can0DataPauseButton->text()) {
        ui->can0DataPauseButton->setText(tr("继续"));
    } else {
        ui->can0DataPauseButton->setText(tr("暂停"));
    }
}

void MainWindow::on_can0DataSaveButton_clicked()
{
    slaveCANDataToFile(CAN_CH0, can0DataModel);
}

void MainWindow::on_can1DataClearButton_clicked()
{
    can1DataModel->removeRows(0, can1DataModel->rowCount());
}

void MainWindow::on_can1DataPauseButton_clicked()
{
    if (tr("暂停") == ui->can1DataPauseButton->text()) {
        ui->can1DataPauseButton->setText(tr("继续"));
    } else {
        ui->can1DataPauseButton->setText(tr("暂停"));
    }
}

void MainWindow::on_can1DataSaveButton_clicked()
{
    slaveCANDataToFile(CAN_CH1, can1DataModel);
}

void MainWindow::on_canOperateAction_triggered()
{
    ui->canOperateDockWidget->show();
}

void MainWindow::on_can0DataFilterButton_clicked()
{
    c0ifDlg->show();
}

void MainWindow::on_can1DataFilterButton_clicked()
{
    c1ifDlg->show();
}
