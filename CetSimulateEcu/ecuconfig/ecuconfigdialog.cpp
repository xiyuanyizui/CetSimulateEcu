#include "ecuconfigdialog.h"
#include "ui_ecuconfigdialog.h"

#include <QSettings>
#include <QDebug>

EcuConfigDialog::EcuConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EcuConfigDialog)
{
    ui->setupUi(this);

    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.beginGroup(QLatin1String("EcuParamCfg"));
    m_ecuBrand = settings.value("ecuBrand").toString();
    m_engineVendor = settings.value("engineVendor").toString();
    settings.endGroup();

    ui->ecuBrandComboBox->addItem(tr("博世(BOSCH)"), ECUBRAND_BOSCH);
    ui->ecuBrandComboBox->addItem(tr("电装(DENSO)"), ECUBRAND_DENSO);
    ui->ecuBrandComboBox->addItem(tr("大陆(VDO)"), ECUBRAND_VDO);

    ui->engineVendorComboBox->addItem(tr("大曼(MAN)"), ECUVENDOR_MAN);
    ui->engineVendorComboBox->addItem(tr("潍柴(WEICHAI)"), ECUVENDOR_WEICHAI);
    ui->engineVendorComboBox->addItem(tr("玉柴(YUCHAI)"), ECUVENDOR_YUCHAI);
    ui->engineVendorComboBox->addItem(tr("锡柴(XICHAI)"), ECUVENDOR_XICHAI);
    ui->engineVendorComboBox->addItem(tr("云内(YUNNEI)"), ECUVENDOR_YUNNEI);
    ui->engineVendorComboBox->addItem(tr("康明斯(KANGMINGSI)"), ECUVENDOR_KANGMINGSI);

    ui->ecuBrandComboBox->setCurrentText(m_ecuBrand);
    ui->engineVendorComboBox->setCurrentText(m_engineVendor);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &EcuConfigDialog::doSlaveEcuParamConfig);
}

EcuConfigDialog::~EcuConfigDialog()
{
    delete ui;
}

void EcuConfigDialog::doSlaveEcuParamConfig()
{
    m_ecuBrand = ui->ecuBrandComboBox->currentText();
    m_engineVendor = ui->engineVendorComboBox->currentText();
    
    QSettings settings("./Settings.ini", QSettings::IniFormat);
    settings.beginGroup(QLatin1String("EcuParamCfg"));
    settings.setValue("ecuBrand", m_ecuBrand);
    settings.setValue("engineVendor", m_engineVendor);
    settings.endGroup();
    
    QString ecuBrand = ui->ecuBrandComboBox->currentData().toString();
    QString engineVendor = ui->engineVendorComboBox->currentData().toString();

    if (!ecuBrand.isEmpty() && !engineVendor.isEmpty()) {
        //qDebug() << "[EcuParamCfg]ecuBrand" << ecuBrand << "engineVendor" << engineVendor;
        emit signalEcuTypeChanged(ecuBrand, engineVendor);
    }
}

void EcuConfigDialog::showDialog()
{
    ui->ecuBrandComboBox->setCurrentText(m_ecuBrand);
    ui->engineVendorComboBox->setCurrentText(m_engineVendor);
    
    show();
}

void EcuConfigDialog::slotControlStateChanged(bool acc, bool ig, bool can)
{
    if (!acc && !ig && !can) {
        ui->ecuConfigGroupBox->setEnabled(true);
        ui->buttonBox->setEnabled(true);
    } else {
        ui->ecuConfigGroupBox->setEnabled(false);
        ui->buttonBox->setEnabled(false);
    }
}

