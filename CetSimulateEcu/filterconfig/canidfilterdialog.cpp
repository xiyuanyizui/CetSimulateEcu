#include "canidfilterdialog.h"
#include "ui_canidfilterdialog.h"

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>

#include <QDebug>

#define HEX_LEN 88

CanIdFilterDialog::CanIdFilterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CanIdFilterDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("CANID过滤"));
    
    QStringList header;
    ui->tableWidget->setColumnCount(4);
    header << "低(HEX)" << "高(HEX)" << "备注" << " ";
    ui->tableWidget->setColumnWidth(0, HEX_LEN);
    ui->tableWidget->setColumnWidth(1, HEX_LEN);
    ui->tableWidget->setColumnWidth(2, HEX_LEN + 130);
    ui->tableWidget->setColumnWidth(3, 36);
    ui->tableWidget->setHorizontalHeaderLabels(header);

    QRegExp regHex8Exp("[0-9a-fA-FxX]{8}"); //创建了一个模式
    QRegExpValidator *hex8pattern = new QRegExpValidator(regHex8Exp, this); //创建了一个表达式
    ui->lowCanidLineEdit->setValidator(hex8pattern);
    ui->highCanidLineEdit->setValidator(hex8pattern);

    connect(ui->tableWidget, &QTableWidget::cellClicked, this, &CanIdFilterDialog::doDelTableWidget);

    on_modeComboBox_activated(0);
}

CanIdFilterDialog::~CanIdFilterDialog()
{
    delete ui;
}

void CanIdFilterDialog::doDelTableWidget(int row, int column)
{
    if (row >= 0 && 3 == column) {
        ui->tableWidget->removeRow(row);
    }
}

bool CanIdFilterDialog::isInRange(const QString &frameid)
{
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        QString lowStr = ui->tableWidget->item(row, 0)->text();
        QString highStr = ui->tableWidget->item(row, 1)->text();

        uint32_t low = lowStr.toUInt(nullptr, 16);
        uint32_t high = highStr.toUInt(nullptr, 16);
        uint32_t canid = frameid.toUInt(nullptr, 16);
        //qDebug("low:0x%08X, canid:0x%08X, high:0x%08X", low, canid, high);
        if (canid >= low && canid <= high) {
            //qDebug("--- In canid:0x%08X", canid);
            return true;
        }
    }

    return false;
}

bool CanIdFilterDialog::isFilterCanId(const QString &frameid)
{
    int index = ui->modeComboBox->currentIndex();
    if (0 == index)
        return false;

    if (1 == index) { // 不接收的IDs
        return isInRange(frameid);
    } else if (2 == index) { // 仅接收的IDs
        return !isInRange(frameid);
    } else {
        return false;
    }
}

void CanIdFilterDialog::on_clearButton_clicked()
{
    for (int i = ui->tableWidget->rowCount() - 1; i >= 0; --i) {
        ui->tableWidget->removeRow(i);
    }
}

void CanIdFilterDialog::on_importButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("导入"), "./", "FILTER文件 (*.filter)");
    if (filename.isEmpty()) {
        return ;
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("warning"), tr("cannot open file: %1").arg(filename));
        return ;
    }
    
    QByteArray readbyte = file.readAll();
    file.close();

    QString content = readbyte;
    if (content.size() > 6) {
        QStringList rowsOfData = content.split("\n");
        for (int row = 0; row < rowsOfData.size(); ++row) {
            if (rowsOfData.at(row).isEmpty()) {
                continue;
            }
            
            QStringList rowData = rowsOfData.at(row).split(",");
            ui->tableWidget->setRowCount(row + 1);
            for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
                QString cell = rowData[col];
                ui->tableWidget->setItem(row, col, new QTableWidgetItem(cell));
            }
            QTableWidgetItem *delItem = new QTableWidgetItem("[Del]");
            delItem->setTextColor(Qt::blue);
            ui->tableWidget->setItem(row, 3, delItem);
            ui->tableWidget->setCurrentCell(row, 2);
        }
    }
}

void CanIdFilterDialog::on_exportButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("导出"), "./", "FILTER文件 (*.filter)");
    if (filename.isEmpty()) {
        return ;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("warning"), tr("cannot open file: %1").arg(filename));
        return ;
    }
    
    QTextStream out(&file);
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        int columnCount = ui->tableWidget->columnCount();
        for (int col = 0; col < columnCount - 1; ++col) {
            out << ui->tableWidget->item(row, col)->text();
            if (col < (columnCount - 1)) {
                out << ",";
            }
        }
        out << "\n";
    }
    out.flush();
    file.close();
}

void CanIdFilterDialog::on_addButton_clicked()
{
    QString lowHex = ui->lowCanidLineEdit->text();
    QString highHex = ui->highCanidLineEdit->text();

    if (ui->tableWidget->rowCount() >= 100) {
        QMessageBox::warning(this, tr("warning"), tr("The number of rows exceeds 100."));
        return ;
    }

    lowHex = tr("%1").arg(lowHex.toUInt(nullptr, 16), 8, 16, QChar('0'));
    highHex = tr("%1").arg(highHex.toUInt(nullptr, 16), 8, 16, QChar('0'));
    
    QTableWidgetItem *lowItem = new QTableWidgetItem(lowHex.toUpper());
    QTableWidgetItem *highItem = new QTableWidgetItem(highHex.toUpper());
    QTableWidgetItem *remarkItem = new QTableWidgetItem("");
    QTableWidgetItem *delItem = new QTableWidgetItem("[Del]");
    delItem->setTextColor(Qt::blue);

    int endrow = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(endrow + 1);
    ui->tableWidget->setItem(endrow, 0, lowItem);
    ui->tableWidget->setItem(endrow, 1, highItem);
    ui->tableWidget->setItem(endrow, 2, remarkItem);
    ui->tableWidget->setItem(endrow, 3, delItem);
    ui->tableWidget->setCurrentCell(endrow, 2);
}

void CanIdFilterDialog::on_modeComboBox_activated(int index)
{
    if (index > 0) {
        ui->lowCanidLineEdit->setEnabled(true);
        ui->highCanidLineEdit->setEnabled(true);
        ui->addButton->setEnabled(true);
        ui->tableWidget->setEnabled(true);

        ui->clearButton->setEnabled(true);
        ui->exportButton->setEnabled(true);
        ui->importButton->setEnabled(true);
    } else {
        ui->lowCanidLineEdit->setEnabled(false);
        ui->highCanidLineEdit->setEnabled(false);
        ui->addButton->setEnabled(false);
        ui->tableWidget->setEnabled(false);

        ui->clearButton->setEnabled(false);
        ui->exportButton->setEnabled(false);
        ui->importButton->setEnabled(false);
    }
}

